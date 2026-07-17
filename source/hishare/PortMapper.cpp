// PortMapper.cpp -- automatic NAT port forwarding for BeShare.
//
// Implements NAT-PMP (RFC 6886) with a UPnP-IGD (SSDP + SOAP) fallback so that
// a BeShare user sitting behind a home NAT router can be reached directly by
// downloaders out on the Internet, without manually configuring a port forward
// or resorting to "I'm Firewalled" mode.
//
// It opens a hole in the *router* only; it does not change one byte of the
// MUSCLE wire protocol BeShare speaks to the server and to its peers.
//
// Networking is done with plain BSD sockets so this file does not depend on any
// particular MUSCLE version's socket API.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Autolock.h>
#include <NetworkRoute.h>

#include "PortMapper.h"

namespace beshare {

using namespace muscle;

// Opt-in tracing: set BESHARE_PORTMAP_DEBUG=1 in the environment to see what the
// mapper is doing on stderr.  Silent otherwise.
static bool PMDebugEnabled() { static int e = -1; if (e < 0) e = (getenv("BESHARE_PORTMAP_DEBUG") != NULL) ? 1 : 0; return e != 0; }
#define PMLOG(...) do { if (PMDebugEnabled()) { fprintf(stderr, "[portmap] " __VA_ARGS__); fprintf(stderr, "\n"); } } while(0)

static const uint16 NATPMP_SERVER_PORT = 5351;
static const uint16 SSDP_PORT           = 1900;
static const char*  SSDP_MCAST_IP       = "239.255.255.250";

static const uint32 LEASE_SECONDS = 3600;   // 1 hour
static const uint32 RETRY_SECONDS = 60;     // when nothing worked, retry after a minute

// --- big-endian pack/unpack (NAT-PMP is network byte order) ---
static inline void Poke16(uint8* p, uint16 v) { p[0] = (uint8)(v >> 8); p[1] = (uint8)v; }
static inline void Poke32(uint8* p, uint32 v) { p[0] = (uint8)(v >> 24); p[1] = (uint8)(v >> 16); p[2] = (uint8)(v >> 8); p[3] = (uint8)v; }
static inline uint16 Peek16(const uint8* p) { return (uint16)((p[0] << 8) | p[1]); }
static inline uint32 Peek32(const uint8* p) { return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | (uint32)p[3]; }

static inline uint64 NowMicros() { return (uint64)system_time(); }

// Wait up to timeoutMicros for fd to become readable.
static bool WaitReadable(int fd, int64 timeoutMicros)
{
	if (fd < 0) return false;
	fd_set r; FD_ZERO(&r); FD_SET(fd, &r);
	struct timeval tv;
	tv.tv_sec  = timeoutMicros / 1000000;
	tv.tv_usec = timeoutMicros % 1000000;
	int rv = select(fd + 1, &r, NULL, NULL, &tv);
	return (rv > 0) && FD_ISSET(fd, &r);
}

static void SetNonBlocking(int fd)
{
	int fl = fcntl(fd, F_GETFL, 0);
	if (fl >= 0) fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// Format a host-order IPv4 address as "a.b.c.d".
static String IpToString(uint32 hostOrderIP)
{
	char buf[24];
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
	         (hostOrderIP >> 24) & 0xFF, (hostOrderIP >> 16) & 0xFF,
	         (hostOrderIP >> 8) & 0xFF, hostOrderIP & 0xFF);
	return String(buf);
}

// TCP-connect to a dotted-quad (or hostname) with a timeout; returns fd or -1.
static int ConnectTCP(const char* host, uint16 port, int timeoutMs)
{
	struct in_addr addr;
	uint32 netIP = 0;
	if (inet_pton(AF_INET, host, &addr) == 1) {
		netIP = addr.s_addr;
	} else {
		// Use getaddrinfo(), NOT gethostbyname(): the latter returns libc's shared static
		// hostent and is not thread-safe, so it races muscle's DNS lookups (and this probe
		// runs on its own thread) -> segfault.  getaddrinfo() allocates a per-call result.
		struct addrinfo hints; memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
		struct addrinfo* res = NULL;
		if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL) return -1;
		netIP = ((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(res);
	}

	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) return -1;
	SetNonBlocking(s);

	sockaddr_in sa; memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port   = htons(port);
	sa.sin_addr.s_addr = netIP;

	int rc = connect(s, (sockaddr*)&sa, sizeof(sa));
	if (rc == 0) return s;
	if (errno != EINPROGRESS && errno != EWOULDBLOCK) { close(s); return -1; }

	fd_set w; FD_ZERO(&w); FD_SET(s, &w);
	struct timeval tv; tv.tv_sec = timeoutMs / 1000; tv.tv_usec = (timeoutMs % 1000) * 1000;
	if (select(s + 1, NULL, &w, NULL, &tv) <= 0) { close(s); return -1; }

	int soErr = 0; socklen_t len = sizeof(soErr);
	if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soErr, &len) < 0 || soErr != 0) { close(s); return -1; }
	return s;
}

// ---------------------------------------------------------------------------
PortMapper::PortMapper(const BMessenger& target, uint16 internalPort)
	: _target(target)
	, _internalPort(internalPort)
	, _thread(-1)
	, _keepRunning(false)
	, _localIP(0)
	, _gatewayIP(0)
	, _activeMethod(METHOD_NONE)
	, _upnpPort(0)
	, _mappedExternalPort(0)
	, _stateLock("port mapper state")
	// (_pcpNonce is filled in lazily by _PCPMap)
	, _externalPort(0)
	, _isMapped(false)
	, _probeRequested(false)
	, _reachability(-1)
{
}

PortMapper::~PortMapper()
{
	Stop();
}

status_t
PortMapper::Start()
{
	if (_thread >= 0) return B_NO_ERROR;
	_keepRunning = true;
	_thread = spawn_thread(_ThreadEntryHook, "beshare port mapper", B_LOW_PRIORITY, this);
	if (_thread < 0) { _keepRunning = false; return _thread; }
	return resume_thread(_thread);
}

void
PortMapper::Stop()
{
	if (_thread < 0) return;
	_keepRunning = false;
	status_t dummy;
	wait_for_thread(_thread, &dummy);
	_thread = -1;
}

int32
PortMapper::_ThreadEntryHook(void* self)
{
	((PortMapper*)self)->_ThreadLoop();
	return 0;
}

String PortMapper::GetExternalIP() const   { BAutolock l(_stateLock); return _externalIP; }
int    PortMapper::GetReachability() const  { BAutolock l(_stateLock); return _reachability; }
String PortMapper::GetInternetIP() const    { BAutolock l(_stateLock); return _internetIP; }
void   PortMapper::ProbeReachability()      { _probeRequested = true; }
uint16 PortMapper::GetExternalPort() const { BAutolock l(_stateLock); return _externalPort; }
bool   PortMapper::IsMapped() const        { BAutolock l(_stateLock); return _isMapped; }

void
PortMapper::_Report(int32 state, const char* method, const char* message,
                    const String& externalIP, uint16 externalPort, bool verified)
{
	{
		BAutolock l(_stateLock);
		_isMapped = (state == PORT_MAP_STATE_MAPPED);
		if (_isMapped) {
			_externalIP   = externalIP;
			_externalPort = externalPort;
		} else if (state == PORT_MAP_STATE_REMOVED || state == PORT_MAP_STATE_FAILED || state == PORT_MAP_STATE_LOST) {
			_externalPort = 0;
		}
	}

	BMessage msg(BESHARE_PORT_MAP_REPORT);
	msg.AddInt32("state", state);
	msg.AddString("method", method ? method : "");
	msg.AddString("external_ip", externalIP.Cstr());
	msg.AddInt32("external_port", externalPort);
	msg.AddInt32("internal_port", _internalPort);
	msg.AddBool("verified", verified);
	if (message) msg.AddString("message", message);
	_target.SendMessage(&msg);
}

// ---------------------------------------------------------------------------
void
PortMapper::_ThreadLoop()
{
	_Report(PORT_MAP_STATE_TRYING, "", "Looking for a NAT-PMP or UPnP router...", "", 0);

	_DiscoverGatewayAndLocalIP();
	PMLOG("discovery: gateway=%s localIP=%s internalPort=%u",
	      IpToString(_gatewayIP).Cstr(), IpToString(_localIP).Cstr(), _internalPort);

	uint64 nextAttempt    = 0;
	bool   everMapped     = false;
	bool   currentlyMapped = false;

	while (_keepRunning) {
		if (NowMicros() >= nextAttempt) {
			String extIP;
			uint16 extPort = 0;
			uint32 lease   = LEASE_SECONDS;
			bool ok = false;
			const char* method = "";

			// Fast-path probes first (PCP, then legacy NAT-PMP), UPnP last.
			if (_gatewayIP != 0 && _PCPMap(LEASE_SECONDS, extIP, extPort, lease)) {
				ok = true; method = "PCP"; _activeMethod = METHOD_PCP;
				PMLOG("PCP succeeded: external port %u", extPort);
			} else if (_gatewayIP != 0 && _NatPMPMap(LEASE_SECONDS, extIP, extPort, lease)) {
				ok = true; method = "NAT-PMP"; _activeMethod = METHOD_NATPMP;
				PMLOG("NAT-PMP succeeded: external port %u", extPort);
			} else {
				PMLOG("NAT-PMP/PCP not available; trying UPnP IGD...");
				if (_UPnPMap(LEASE_SECONDS, extIP, extPort)) {
					ok = true; method = "UPnP"; _activeMethod = METHOD_UPNP; lease = LEASE_SECONDS;
					PMLOG("UPnP succeeded: external port %u (extIP=%s)", extPort, extIP.Cstr());
				} else PMLOG("UPnP mapping failed");
			}

			if (ok) {
				_mappedExternalPort = extPort;
				// Self-test: for UPnP, ask the router to read the mapping back; for
				// NAT-PMP/PCP the map response itself is the confirmation.
				bool verified = (_activeMethod == METHOD_UPNP) ? _UPnPVerify() : true;
				PMLOG("mapping self-test: %s", verified ? "confirmed on router" : "NOT confirmed");
				char buf[224];
				snprintf(buf, sizeof(buf),
				         "Router port forwarding active (%s): external port %u -> local port %u.%s",
				         method, extPort, _internalPort,
				         verified ? "  Mapping confirmed on the router."
				                  : "  WARNING: the router did not confirm the mapping, incoming transfers may still fail.");
				_Report(PORT_MAP_STATE_MAPPED, method, buf, extIP, extPort, verified);
				if (!currentlyMapped) _probeRequested = true;  // fresh map => check real reachability
				everMapped = currentlyMapped = true;
				uint32 renewIn = (lease > 60) ? (lease / 2) : 30;
				nextAttempt = NowMicros() + (uint64)renewIn * 1000000ULL;
			} else if (currentlyMapped) {
				// We had a working mapping and just failed to renew it: the router
				// probably rebooted or dropped it.  Tell the UI so it can restore
				// "I'm Firewalled".  We keep retrying in the background.
				currentlyMapped = false;
				_activeMethod = METHOD_NONE;
				_mappedExternalPort = 0;
				_Report(PORT_MAP_STATE_LOST, "",
				        "Router port forwarding was lost (router reboot?); retrying...", "", 0);
				nextAttempt = NowMicros() + (uint64)RETRY_SECONDS * 1000000ULL;
			} else {
				if (!everMapped)
					_Report(PORT_MAP_STATE_FAILED, "",
					        "No NAT-PMP/PCP/UPnP router found; incoming transfers may need manual forwarding.",
					        "", 0);
				nextAttempt = NowMicros() + (uint64)RETRY_SECONDS * 1000000ULL;
			}
		}

		// Run an external-reachability probe when requested (fresh map, or a manual
		// "Test External Reachability").  Done on this worker thread so the GUI never
		// blocks on the HTTP round-trip.
		if (_probeRequested && _keepRunning) {
			_probeRequested = false;
			_RunReachabilityProbe();
		}

		snooze(250000);  // 250 ms slices, so Stop() stays responsive
	}

	PMLOG("thread loop exiting; unmapping (method=%d, mappedPort=%u)", (int)_activeMethod, _mappedExternalPort);
	if (_activeMethod == METHOD_PCP)         _PCPUnmap();
	else if (_activeMethod == METHOD_NATPMP) _NatPMPUnmap();
	else if (_activeMethod == METHOD_UPNP)   _UPnPUnmap();
	PMLOG("unmap done");

	if (everMapped)
		_Report(PORT_MAP_STATE_REMOVED, "", "Router port forwarding removed.", "", 0);
}

// ---------------------------------------------------------------------------
bool
PortMapper::_DiscoverGatewayAndLocalIP()
{
	// Default gateway via Haiku's network-route API.
	sockaddr_in gw; memset(&gw, 0, sizeof(gw));
	if (BNetworkRoute::GetDefaultGateway(AF_INET, NULL, *(sockaddr*)&gw) == B_OK)
		_gatewayIP = ntohl(gw.sin_addr.s_addr);

	// Local IP: ask the routing layer which source address it would use to
	// reach the gateway (or any off-link address), via a connected UDP socket.
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		sockaddr_in dst; memset(&dst, 0, sizeof(dst));
		dst.sin_family = AF_INET;
		dst.sin_port   = htons(9);   // discard port; no packet is actually sent
		dst.sin_addr.s_addr = (_gatewayIP != 0) ? htonl(_gatewayIP) : inet_addr("8.8.8.8");
		if (connect(s, (sockaddr*)&dst, sizeof(dst)) == 0) {
			sockaddr_in local; socklen_t len = sizeof(local);
			if (getsockname(s, (sockaddr*)&local, &len) == 0)
				_localIP = ntohl(local.sin_addr.s_addr);
		}
		close(s);
	}
	return (_gatewayIP != 0);
}

// ---------------------------------------------------------------------------
// PCP  (RFC 6887) -- the modern successor to NAT-PMP.  Shares UDP port 5351.
// ---------------------------------------------------------------------------
// PCP MAP request/response are 60 bytes: a 24-byte common header followed by a
// 36-byte MAP opcode block.  We send version 2; a NAT-PMP-only router will not
// answer a version-2 packet, so we simply fall back to legacy NAT-PMP.
bool
PortMapper::_PCPMap(uint32 lifetimeSecs, String& outExternalIP,
                    uint16& outExternalPort, uint32& outLeaseSecs)
{
	if (_localIP == 0) _DiscoverGatewayAndLocalIP();
	if (_localIP == 0 || _gatewayIP == 0) return false;

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return false;
	SetNonBlocking(sock);

	// Build a fresh 12-byte nonce (uniqueness, not crypto strength) so the
	// response can be matched and the same mapping can later be deleted.
	uint64 t = (uint64)system_time();
	for (int i = 0; i < 12; i++)
		_pcpNonce[i] = (uint8)((t >> ((i % 8) * 8)) ^ (i * 37) ^ (_internalPort >> (i & 1 ? 8 : 0)));

	uint8 req[60];
	memset(req, 0, sizeof(req));
	req[0] = 2;                       // version
	req[1] = 1;                       // opcode MAP (R=0 -> request)
	Poke32(req + 4, lifetimeSecs);    // requested lifetime
	// Client IP at bytes 8..23 as an IPv4-mapped IPv6 address (::ffff:a.b.c.d)
	req[8 + 10] = 0xff; req[8 + 11] = 0xff;
	req[8 + 12] = (uint8)(_localIP >> 24); req[8 + 13] = (uint8)(_localIP >> 16);
	req[8 + 14] = (uint8)(_localIP >> 8);  req[8 + 15] = (uint8)_localIP;
	// MAP opcode data at offset 24
	memcpy(req + 24, _pcpNonce, 12);  // mapping nonce
	req[36] = 6;                      // protocol = TCP
	Poke16(req + 40, _internalPort);  // internal port
	Poke16(req + 42, _internalPort);  // suggested external port
	// suggested external IP (44..59) left as all-zeros == "no preference"

	sockaddr_in dst; memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port   = htons(NATPMP_SERVER_PORT);
	dst.sin_addr.s_addr = htonl(_gatewayIP);

	uint8 reply[80];
	int64 timeoutMicros = 250000;
	bool got = false;
	for (int attempt = 0; attempt < 3 && _keepRunning && !got; attempt++) {
		if (sendto(sock, req, sizeof(req), 0, (sockaddr*)&dst, sizeof(dst)) < 0) break;
		uint64 deadline = NowMicros() + (uint64)timeoutMicros;
		while (NowMicros() < deadline) {
			if (!WaitReadable(sock, deadline - NowMicros())) break;
			sockaddr_in from; socklen_t fl = sizeof(from);
			int n = recvfrom(sock, reply, sizeof(reply), 0, (sockaddr*)&from, &fl);
			// version 2, response bit set on MAP opcode (0x81), matching nonce
			if (n >= 60 && reply[0] == 2 && reply[1] == 0x81
			            && memcmp(reply + 24, _pcpNonce, 12) == 0) { got = true; break; }
		}
		timeoutMicros *= 2;
	}
	close(sock);
	if (!got) return false;

	uint8 resultCode = reply[3];
	if (resultCode != 0) { PMLOG("PCP result code %u (not success)", resultCode); return false; }

	outExternalPort = Peek16(reply + 42);                    // assigned external port
	uint32 assignedIP = Peek32(reply + 56);                  // last 4 bytes of the 16-byte ext IP
	if (assignedIP != 0) outExternalIP = IpToString(assignedIP);
	uint32 gotLease = Peek32(reply + 4);
	outLeaseSecs = (gotLease > 0) ? gotLease : lifetimeSecs;
	return (outExternalPort != 0);
}

void
PortMapper::_PCPUnmap()
{
	if (_gatewayIP == 0 || _localIP == 0 || _mappedExternalPort == 0) return;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return;
	SetNonBlocking(sock);

	// Same MAP request but with a requested lifetime of 0 == delete.
	uint8 req[60];
	memset(req, 0, sizeof(req));
	req[0] = 2; req[1] = 1;
	Poke32(req + 4, 0);
	req[8 + 10] = 0xff; req[8 + 11] = 0xff;
	req[8 + 12] = (uint8)(_localIP >> 24); req[8 + 13] = (uint8)(_localIP >> 16);
	req[8 + 14] = (uint8)(_localIP >> 8);  req[8 + 15] = (uint8)_localIP;
	memcpy(req + 24, _pcpNonce, 12);
	req[36] = 6;
	Poke16(req + 40, _internalPort);
	Poke16(req + 42, 0);

	sockaddr_in dst; memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port   = htons(NATPMP_SERVER_PORT);
	dst.sin_addr.s_addr = htonl(_gatewayIP);
	(void)sendto(sock, req, sizeof(req), 0, (sockaddr*)&dst, sizeof(dst));
	close(sock);
}

// ---------------------------------------------------------------------------
// NAT-PMP
// ---------------------------------------------------------------------------
// Sends one request to the gateway and waits (with retransmits) for a matching
// reply.  Returns reply length, or -1.
static int NatPMPTransact(int sock, uint32 gatewayHostIP,
                          const uint8* req, uint32 reqLen, uint8 expectedOpcode,
                          uint8* reply, uint32 replyCap, volatile bool* keepRunning)
{
	sockaddr_in dst; memset(&dst, 0, sizeof(dst));
	dst.sin_family = AF_INET;
	dst.sin_port   = htons(NATPMP_SERVER_PORT);
	dst.sin_addr.s_addr = htonl(gatewayHostIP);

	// 3 tries (250/500/1000ms): a NAT-PMP-capable router answers in
	// milliseconds, so this fails fast and hands off to UPnP quickly.
	int64 timeoutMicros = 250000;
	for (int attempt = 0; attempt < 3 && (keepRunning == NULL || *keepRunning); attempt++) {
		if (sendto(sock, req, reqLen, 0, (sockaddr*)&dst, sizeof(dst)) < 0) return -1;

		uint64 deadline = NowMicros() + (uint64)timeoutMicros;
		while (NowMicros() < deadline) {
			if (!WaitReadable(sock, deadline - NowMicros())) break;
			sockaddr_in from; socklen_t fl = sizeof(from);
			int n = recvfrom(sock, reply, replyCap, 0, (sockaddr*)&from, &fl);
			if (n >= 2 && ntohl(from.sin_addr.s_addr) == gatewayHostIP
			           && reply[0] == 0 && reply[1] == expectedOpcode)
				return n;
		}
		timeoutMicros *= 2;
	}
	return -1;
}

bool
PortMapper::_NatPMPMap(uint32 lifetimeSecs, String& outExternalIP,
                       uint16& outExternalPort, uint32& outLeaseSecs)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return false;
	SetNonBlocking(sock);

	uint8 reply[32];

	// (a) public/external address request (opcode 0 -> reply 128)
	uint8 pubReq[2] = { 0, 0 };
	int n = NatPMPTransact(sock, _gatewayIP, pubReq, sizeof(pubReq), 128, reply, sizeof(reply), &_keepRunning);
	if (n >= 12 && Peek16(reply + 2) == 0)
		outExternalIP = IpToString(Peek32(reply + 8));

	// (b) create the TCP mapping (opcode 2 -> reply 130)
	uint8 mapReq[12];
	memset(mapReq, 0, sizeof(mapReq));
	mapReq[1] = 2;
	Poke16(mapReq + 4, _internalPort);
	Poke16(mapReq + 6, _internalPort);
	Poke32(mapReq + 8, lifetimeSecs);

	n = NatPMPTransact(sock, _gatewayIP, mapReq, sizeof(mapReq), 130, reply, sizeof(reply), &_keepRunning);
	close(sock);
	if (n < 16 || Peek16(reply + 2) != 0) return false;

	if (Peek16(reply + 8) != _internalPort) return false;   // internal port echo
	outExternalPort = Peek16(reply + 10);
	uint32 gotLease = Peek32(reply + 12);
	outLeaseSecs    = (gotLease > 0) ? gotLease : lifetimeSecs;
	return true;
}

void
PortMapper::_NatPMPUnmap()
{
	if (_gatewayIP == 0 || _mappedExternalPort == 0) return;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return;
	SetNonBlocking(sock);

	uint8 req[12];
	memset(req, 0, sizeof(req));
	req[1] = 2;
	Poke16(req + 4, _internalPort);
	Poke16(req + 6, 0);
	Poke32(req + 8, 0);   // lifetime 0 == delete

	uint8 reply[32];
	(void)NatPMPTransact(sock, _gatewayIP, req, sizeof(req), 130, reply, sizeof(reply), NULL);
	close(sock);
}

// ---------------------------------------------------------------------------
// UPnP IGD helpers
// ---------------------------------------------------------------------------
static String HttpHeaderValue(const String& response, const char* headerName)
{
	String lower = response.ToLowerCase();
	String key   = String(headerName).ToLowerCase();
	int idx = lower.IndexOf(key);
	if (idx < 0) return "";
	idx += (int)key.Length();
	int eol = response.IndexOf('\n', idx);
	if (eol < 0) eol = (int)response.Length();
	return response.Substring((uint32)idx, (uint32)eol).Trim();
}

static String XmlTagValue(const String& xml, const char* tag)
{
	String open = String("<") + tag;
	int s = xml.IndexOf(open);
	if (s < 0) return "";
	s = xml.IndexOf('>', s);
	if (s < 0) return "";
	s += 1;
	String close = String("</") + tag + ">";
	int e = xml.IndexOf(close, s);
	if (e < 0) return "";
	return xml.Substring((uint32)s, (uint32)e).Trim();
}

static bool ParseHttpURL(const String& url, String& outHost, uint16& outPort, String& outPath)
{
	if (url.StartsWith("http://") == false) return false;
	String u = url.Substring(7);
	int slash = u.IndexOf('/');
	String hostport = (slash >= 0) ? u.Substring(0, (uint32)slash) : u;
	outPath = (slash >= 0) ? u.Substring((uint32)slash) : String("/");
	int colon = hostport.IndexOf(':');
	if (colon >= 0) {
		outHost = hostport.Substring(0, (uint32)colon);
		outPort = (uint16)atoi(hostport.Substring((uint32)colon + 1).Cstr());
	} else {
		outHost = hostport;
		outPort = 80;
	}
	return (outHost.Length() > 0);
}

bool
PortMapper::_HttpRequest(const char* host, uint16 port, const char* method,
                         const char* path, const String& extraHeaders,
                         const String& body, String& outBody)
{
	int sock = ConnectTCP(host, port, 6000);
	if (sock < 0) return false;

	String req;
	char line[512];
	snprintf(line, sizeof(line), "%s %s HTTP/1.1\r\n", method, path); req += line;
	snprintf(line, sizeof(line), "HOST: %s:%u\r\n", host, port);      req += line;
	req += "Connection: close\r\n";
	snprintf(line, sizeof(line), "Content-Length: %lu\r\n", (unsigned long)body.Length()); req += line;
	req += extraHeaders;
	req += "\r\n";
	req += body;

	const char* out = req.Cstr();
	uint32 remaining = req.Length();
	uint64 sendDeadline = NowMicros() + 6000000ULL;
	while (remaining > 0 && NowMicros() < sendDeadline) {
		int sent = send(sock, out, remaining, 0);
		if (sent > 0) { out += sent; remaining -= (uint32)sent; }
		else if (sent < 0 && errno != EWOULDBLOCK && errno != EAGAIN) { close(sock); return false; }
		else snooze(5000);
	}
	if (remaining > 0) { close(sock); return false; }

	String response;
	char buf[2048];
	uint64 recvDeadline = NowMicros() + 8000000ULL;
	while (NowMicros() < recvDeadline) {
		if (!WaitReadable(sock, recvDeadline - NowMicros())) break;
		int n = recv(sock, buf, sizeof(buf) - 1, 0);
		if (n > 0) { buf[n] = '\0'; response += buf; }
		else if (n == 0) break;   // peer closed
		else if (errno != EWOULDBLOCK && errno != EAGAIN) break;
		if (response.Length() > 65536) break;
	}
	close(sock);
	if (response.Length() == 0) return false;

	int hdrEnd = response.IndexOf("\r\n\r\n");
	outBody = (hdrEnd >= 0) ? response.Substring((uint32)hdrEnd + 4) : response;
	return true;
}

// ---------------------------------------------------------------------------
// External reachability probe

// Finds the first well-formed IPv4 dotted-quad in (text).
bool
PortMapper::_ExtractIPv4(const String& text, String& outIP)
{
	const char* s = text.Cstr();
	for (const char* p = s; *p; ++p) {
		if (*p < '0' || *p > '9') continue;
		if (p != s && (p[-1] == '.' || (p[-1] >= '0' && p[-1] <= '9'))) continue; // mid-number
		int a, b, c, d, n = 0;
		if (sscanf(p, "%d.%d.%d.%d%n", &a, &b, &c, &d, &n) == 4 &&
		    a >= 0 && a <= 255 && b >= 0 && b <= 255 &&
		    c >= 0 && c <= 255 && d >= 0 && d <= 255) {
			char after = p[n];
			if (!((after >= '0' && after <= '9') || after == '.')) {
				char buf[16];
				snprintf(buf, sizeof(buf), "%d.%d.%d.%d", a, b, c, d);
				outIP = buf;
				return true;
			}
		}
	}
	return false;
}

// True if (ip) is a non-internet-routable address (RFC1918 / CGNAT / link-local / loopback).
bool
PortMapper::_IsPrivateIPv4(const String& ip)
{
	unsigned a, b, c, d;
	if (sscanf(ip.Cstr(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
	if (a == 10)                       return true;   // 10.0.0.0/8
	if (a == 172 && b >= 16 && b <= 31) return true;  // 172.16.0.0/12
	if (a == 192 && b == 168)          return true;   // 192.168.0.0/16
	if (a == 100 && b >= 64 && b <= 127) return true; // 100.64.0.0/10 (carrier-grade NAT, RFC 6598)
	if (a == 169 && b == 254)          return true;   // 169.254.0.0/16 link-local
	if (a == 127)                      return true;   // loopback
	if (a == 0)                        return true;
	return false;
}

// Fetches our internet-visible IP from a public IP-echo service (plain HTTP, so it
// works with PortMapper's raw-socket client).  Tries several in case one is down.
bool
PortMapper::_GetInternetIP(String& outIP)
{
	static const struct { const char* host; const char* path; } kServices[] = {
		{ "api.ipify.org",         "/"   },
		{ "icanhazip.com",         "/"   },
		{ "checkip.amazonaws.com", "/"   },
		{ "ifconfig.me",           "/ip" },
	};
	for (size_t i = 0; i < sizeof(kServices) / sizeof(kServices[0]); ++i) {
		if (!_keepRunning) return false;
		String body;
		if (_HttpRequest(kServices[i].host, 80, "GET", kServices[i].path,
		                 "User-Agent: BeShare\r\n", "", body)
		    && _ExtractIPv4(body, outIP)) {
			PMLOG("internet IP %s (via %s)", outIP.Cstr(), kServices[i].host);
			return true;
		}
	}
	return false;
}

// Decides whether we are actually reachable from the internet and reports the verdict.
void
PortMapper::_RunReachabilityProbe()
{
	String internetIP;
	bool gotNet = _GetInternetIP(internetIP);

	String routerExt;
	uint16 extPort;
	bool   mapped;
	{
		BAutolock l(_stateLock);
		routerExt = _externalIP;
		extPort   = _externalPort;
		mapped    = _isMapped;
	}
	String localIP = IpToString(_localIP);

	int  reach;
	char buf[416];

	if (!gotNet) {
		reach = -1;
		snprintf(buf, sizeof(buf),
		         "Reachability check: could not contact an internet IP-echo service "
		         "(offline or outbound HTTP blocked); external reachability unknown.");
	} else if (routerExt.Length() > 0 && _IsPrivateIPv4(routerExt)) {
		reach = 0;
		snprintf(buf, sizeof(buf),
		         "Reachability check: NOT reachable from the internet. Your router's WAN address "
		         "(%s) is itself a private / carrier-grade-NAT address, so forwarding a port on it "
		         "cannot expose you. Your real public IP is %s. Ask your ISP for a public IP, or "
		         "keep 'I'm Firewalled' on.", routerExt.Cstr(), internetIP.Cstr());
	} else if (routerExt.Length() > 0 && routerExt != internetIP) {
		reach = 0;
		snprintf(buf, sizeof(buf),
		         "Reachability check: NOT reachable from the internet. Carrier-grade / double NAT "
		         "detected: your router's WAN IP (%s) is not your real public IP (%s), so the "
		         "forwarded port isn't reachable from outside. Keep 'I'm Firewalled' on.",
		         routerExt.Cstr(), internetIP.Cstr());
	} else if (routerExt.Length() > 0) {   // routerExt == internetIP
		reach = 1;
		snprintf(buf, sizeof(buf),
		         "Reachability check: reachable! Your public address %s:%u is open to the internet; "
		         "external users can download from you.",
		         internetIP.Cstr(), (unsigned)(extPort ? extPort : _internalPort));
	} else if (internetIP == localIP) {    // no mapping, but we hold a public IP directly
		reach = 1;
		snprintf(buf, sizeof(buf),
		         "Reachability check: you appear to be directly on the internet (public IP %s, no NAT). "
		         "With 'I'm Firewalled' off, external users can reach you.", internetIP.Cstr());
	} else {                               // no mapping and behind NAT
		reach = 0;
		snprintf(buf, sizeof(buf),
		         "Reachability check: you are behind NAT (local %s, public %s) with no active port "
		         "forwarding; external users likely cannot reach you unless you forward the port "
		         "manually or enable UPnP/NAT-PMP.", localIP.Cstr(), internetIP.Cstr());
	}

	{
		BAutolock l(_stateLock);
		_reachability = reach;
		_internetIP   = internetIP;
	}

	BMessage report(BESHARE_PORT_MAP_REPORT);
	report.AddInt32("state", mapped ? PORT_MAP_STATE_MAPPED : PORT_MAP_STATE_IDLE);
	report.AddInt32("reachable", reach);
	report.AddString("internet_ip", internetIP.Cstr());
	report.AddInt32("external_port", extPort);
	report.AddInt32("internal_port", _internalPort);
	report.AddString("message", buf);
	_target.SendMessage(&report);
	PMLOG("reachability verdict=%d internetIP=%s routerExt=%s", reach, internetIP.Cstr(), routerExt.Cstr());
}

bool
PortMapper::_UPnPDiscover(String& outControlURL, String& outServiceType,
                          String& outBaseHost, uint16& outBasePort)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return false;
	SetNonBlocking(sock);

	sockaddr_in mcast; memset(&mcast, 0, sizeof(mcast));
	mcast.sin_family = AF_INET;
	mcast.sin_port   = htons(SSDP_PORT);
	mcast.sin_addr.s_addr = inet_addr(SSDP_MCAST_IP);

	const char* targets[] = {
		"urn:schemas-upnp-org:service:WANIPConnection:1",
		"urn:schemas-upnp-org:service:WANPPPConnection:1",
		"urn:schemas-upnp-org:device:InternetGatewayDevice:1"
	};

	String location;
	for (uint32 t = 0; t < 3 && location.Length() == 0 && _keepRunning; t++) {
		char msearch[512];
		snprintf(msearch, sizeof(msearch),
		         "M-SEARCH * HTTP/1.1\r\n"
		         "HOST: 239.255.255.250:1900\r\n"
		         "MAN: \"ssdp:discover\"\r\n"
		         "MX: 2\r\n"
		         "ST: %s\r\n"
		         "\r\n", targets[t]);

		for (int rep = 0; rep < 2; rep++)
			sendto(sock, msearch, strlen(msearch), 0, (sockaddr*)&mcast, sizeof(mcast));

		uint64 deadline = NowMicros() + 3000000ULL;
		while (NowMicros() < deadline && location.Length() == 0) {
			if (!WaitReadable(sock, deadline - NowMicros())) break;
			char resp[2048];
			int n = recvfrom(sock, resp, sizeof(resp) - 1, 0, NULL, NULL);
			if (n <= 0) continue;
			resp[n] = '\0';
			String loc = HttpHeaderValue(String(resp), "\nlocation:");
			if (loc.StartsWith("http://")) location = loc;
		}
	}
	close(sock);

	if (location.Length() == 0) { PMLOG("SSDP: no IGD LOCATION found"); return false; }
	PMLOG("SSDP: IGD LOCATION = %s", location.Cstr());

	String host, path; uint16 port = 80;
	if (!ParseHttpURL(location, host, port, path)) return false;
	outBaseHost = host; outBasePort = port;

	String xml;
	if (!_HttpRequest(host.Cstr(), port, "GET", path.Cstr(), "", "", xml)) { PMLOG("HTTP GET description failed"); return false; }
	PMLOG("device description fetched (%lu bytes)", (unsigned long)xml.Length());

	const char* svcTypes[] = {
		"urn:schemas-upnp-org:service:WANIPConnection:1",
		"urn:schemas-upnp-org:service:WANPPPConnection:1"
	};
	for (uint32 s = 0; s < 2; s++) {
		int svcIdx = xml.IndexOf(svcTypes[s]);
		if (svcIdx < 0) continue;
		int ctlIdx = xml.IndexOf("<controlURL>", (uint32)svcIdx);
		if (ctlIdx < 0) continue;
		int open  = xml.IndexOf('>', (uint32)ctlIdx) + 1;
		int close = xml.IndexOf("</controlURL>", (uint32)open);
		if (close < 0) continue;
		outControlURL  = xml.Substring((uint32)open, (uint32)close).Trim();
		outServiceType = svcTypes[s];
		PMLOG("found service %s controlURL=%s", svcTypes[s], outControlURL.Cstr());
		return (outControlURL.Length() > 0);
	}
	PMLOG("no WAN{IP,PPP}Connection service/controlURL found");
	return false;
}

bool
PortMapper::_UPnPSoap(const char* action, const String& argsXml, String& outResponse)
{
	String host = _upnpHost, path = _upnpControlURL; uint16 port = _upnpPort;
	if (_upnpControlURL.StartsWith("http://"))
		ParseHttpURL(_upnpControlURL, host, port, path);
	else if (_upnpControlURL.StartsWith("/") == false)
		path = String("/") + _upnpControlURL;

	String bodyXml;
	bodyXml += "<?xml version=\"1.0\"?>\r\n";
	bodyXml += "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
	bodyXml += "<s:Body>";
	bodyXml += String("<u:") + action + " xmlns:u=\"" + _upnpServiceType + "\">";
	bodyXml += argsXml;
	bodyXml += String("</u:") + action + ">";
	bodyXml += "</s:Body></s:Envelope>";

	String headers;
	headers += "Content-Type: text/xml; charset=\"utf-8\"\r\n";
	headers += String("SOAPAction: \"") + _upnpServiceType + "#" + action + "\"\r\n";

	return _HttpRequest(host.Cstr(), port, "POST", path.Cstr(), headers, bodyXml, outResponse);
}

bool
PortMapper::_UPnPMap(uint32 lifetimeSecs, String& outExternalIP, uint16& outExternalPort)
{
	if (!_UPnPDiscover(_upnpControlURL, _upnpServiceType, _upnpHost, _upnpPort))
		return false;

	if (_localIP == 0) _DiscoverGatewayAndLocalIP();
	if (_localIP == 0) return false;
	String localIpStr = IpToString(_localIP);

	String resp;
	if (_UPnPSoap("GetExternalIPAddress", "", resp))
		outExternalIP = XmlTagValue(resp, "NewExternalIPAddress");

	char args[512];
	for (int attempt = 0; attempt < 2; attempt++) {
		uint32 lease = (attempt == 0) ? lifetimeSecs : 0;
		snprintf(args, sizeof(args),
		         "<NewRemoteHost></NewRemoteHost>"
		         "<NewExternalPort>%u</NewExternalPort>"
		         "<NewProtocol>TCP</NewProtocol>"
		         "<NewInternalPort>%u</NewInternalPort>"
		         "<NewInternalClient>%s</NewInternalClient>"
		         "<NewEnabled>1</NewEnabled>"
		         "<NewPortMappingDescription>BeShare</NewPortMappingDescription>"
		         "<NewLeaseDuration>%lu</NewLeaseDuration>",
		         _internalPort, _internalPort, localIpStr.Cstr(), (unsigned long)lease);

		String mapResp;
		bool sent = _UPnPSoap("AddPortMapping", String(args), mapResp);
		PMLOG("AddPortMapping(lease=%lu) sent=%d resp=%lu bytes", (unsigned long)lease, sent, (unsigned long)mapResp.Length());
		if (sent && mapResp.IndexOfIgnoreCase("AddPortMappingResponse") >= 0
		         && mapResp.IndexOf("<s:Fault>") < 0
		         && mapResp.IndexOfIgnoreCase("faultstring") < 0) {
			outExternalPort = _internalPort;
			return true;
		}
	}
	return false;
}

void
PortMapper::_UPnPUnmap()
{
	if (_upnpControlURL.Length() == 0 || _mappedExternalPort == 0) return;

	char args[256];
	snprintf(args, sizeof(args),
	         "<NewRemoteHost></NewRemoteHost>"
	         "<NewExternalPort>%u</NewExternalPort>"
	         "<NewProtocol>TCP</NewProtocol>",
	         _mappedExternalPort);

	String resp;
	(void)_UPnPSoap("DeletePortMapping", String(args), resp);
}

bool
PortMapper::_UPnPVerify()
{
	if (_upnpControlURL.Length() == 0 || _mappedExternalPort == 0) return false;

	char args[256];
	snprintf(args, sizeof(args),
	         "<NewRemoteHost></NewRemoteHost>"
	         "<NewExternalPort>%u</NewExternalPort>"
	         "<NewProtocol>TCP</NewProtocol>",
	         _mappedExternalPort);

	String resp;
	if (!_UPnPSoap("GetSpecificPortMappingEntry", String(args), resp)) return false;

	// The router should echo back our internal port and (usually) our LAN IP.
	String gotPort   = XmlTagValue(resp, "NewInternalPort");
	String gotClient = XmlTagValue(resp, "NewInternalClient");
	bool portOk   = (gotPort.Length() > 0) && ((uint16)atoi(gotPort.Cstr()) == _internalPort);
	bool clientOk = (gotClient.Length() == 0) || (_localIP == 0) || (gotClient == IpToString(_localIP));
	return portOk && clientOk;
}

};  // namespace beshare
