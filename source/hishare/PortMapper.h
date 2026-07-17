#ifndef BESHARE_PORT_MAPPER_H
#define BESHARE_PORT_MAPPER_H

#include <Locker.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>

#include "util/String.h"

namespace beshare {

using namespace muscle;

// This message is posted (via the target BMessenger passed to the constructor)
// whenever the port-mapping state changes.  ShareWindow listens for it.
//
// Fields:
//   "state"        (int32)  one of the PortMapperState values below
//   "method"       (string) "NAT-PMP" or "UPnP" (human readable)
//   "external_ip"  (string) our public IP as seen from the WAN, if known
//   "external_port"(int32)  the port that was opened on the router, if any
//   "internal_port"(int32)  the local file-serving port we asked to forward
//   "message"      (string) a human-readable status line for the chat log
//   "reachable"    (int32)  ONLY on reachability reports: 1=reachable from the
//                           internet, 0=not reachable (CGNAT/double-NAT), -1=unknown
//   "internet_ip"  (string) ONLY on reachability reports: our internet-visible IP
#define BESHARE_PORT_MAP_REPORT 'pmMR'

enum PortMapperState {
	PORT_MAP_STATE_IDLE = 0,	// not doing anything
	PORT_MAP_STATE_TRYING,		// discovery/mapping in progress
	PORT_MAP_STATE_MAPPED,		// a mapping is currently active
	PORT_MAP_STATE_FAILED,		// gave up (no NAT-PMP/PCP/UPnP router found)
	PORT_MAP_STATE_REMOVED,		// mapping was deleted (e.g. on shutdown)
	PORT_MAP_STATE_LOST			// a previously-active mapping stopped renewing (router reboot, etc.)
};

// PortMapper asks the local NAT router to forward an external TCP port to the
// port BeShare listens on for incoming file-transfer connections, so that
// people who are NOT behind the same router can download from us even when we
// sit behind a home NAT gateway.  It tries NAT-PMP first (fast, RFC 6886) and
// falls back to UPnP IGD (SSDP + SOAP) if that gets no answer.
//
// All of the network work happens in a private worker thread so the GUI never
// blocks.  The lease is renewed automatically for as long as the object lives,
// and the mapping is deleted from the router in the destructor (best-effort).
//
// The implementation deliberately uses plain BSD sockets rather than MUSCLE's
// networking API, so it is independent of which MUSCLE version BeShare is built
// against.
class PortMapper {
public:
	// target      : where BESHARE_PORT_MAP_REPORT messages are sent
	// internalPort: the local TCP port to forward (BeShare's accept port)
	PortMapper(const BMessenger& target, uint16 internalPort);
	~PortMapper();

	status_t Start();   // spawn the worker thread and begin trying to map
	void     Stop();    // delete the mapping and join the thread (blocks)

	uint16 GetInternalPort() const { return _internalPort; }

	String GetExternalIP() const;
	uint16 GetExternalPort() const;
	bool   IsMapped() const;

	// Kicks off an external-reachability probe on the worker thread: it fetches
	// our internet-visible IP from a public IP-echo service and compares it with
	// the router's WAN address, to tell whether we are ACTUALLY reachable from
	// the internet (vs. silently stuck behind carrier-grade / double NAT, which a
	// confirmed router mapping does not rule out).  Result arrives asynchronously
	// via BESHARE_PORT_MAP_REPORT with a "reachable" field (see below).  Also run
	// automatically once after each fresh mapping.
	void   ProbeReachability();
	int    GetReachability() const;   // 1 reachable, 0 not (CGNAT), -1 unknown/not yet probed
	String GetInternetIP() const;

private:
	static int32 _ThreadEntryHook(void* self);
	void  _ThreadLoop();

	void  _Report(int32 state, const char* method, const char* message,
	              const String& externalIP, uint16 externalPort, bool verified = true);

	bool  _DiscoverGatewayAndLocalIP();

	// PCP (RFC 6887) and legacy NAT-PMP (RFC 6886); both talk to _gatewayIP:5351.
	// IPs are in host byte order.
	bool  _PCPMap(uint32 lifetimeSecs, String& outExternalIP,
	              uint16& outExternalPort, uint32& outLeaseSecs);
	void  _PCPUnmap();
	bool  _NatPMPMap(uint32 lifetimeSecs, String& outExternalIP,
	                 uint16& outExternalPort, uint32& outLeaseSecs);
	void  _NatPMPUnmap();

	// UPnP IGD (SSDP discovery + SOAP control).
	bool  _UPnPDiscover(String& outControlURL, String& outServiceType,
	                    String& outBaseHost, uint16& outBasePort);
	bool  _UPnPMap(uint32 lifetimeSecs, String& outExternalIP,
	               uint16& outExternalPort);
	void  _UPnPUnmap();
	// Self-test: ask the router (GetSpecificPortMappingEntry) whether the mapping
	// we just asked for is really installed, pointing back at us.  Catches routers
	// that acknowledge AddPortMapping but silently fail to create the entry.
	bool  _UPnPVerify();
	bool  _UPnPSoap(const char* action, const String& argsXml, String& outResponse);

	// Minimal blocking HTTP/1.1 client used for UPnP.
	bool  _HttpRequest(const char* host, uint16 port, const char* method,
	                   const char* path, const String& extraHeaders,
	                   const String& body, String& outBody);

	// External-reachability probe helpers.
	void  _RunReachabilityProbe();                       // fetch internet IP, decide verdict, report
	bool  _GetInternetIP(String& outIP);                 // HTTP GET a public IP-echo service
	static bool _ExtractIPv4(const String& text, String& outIP);
	static bool _IsPrivateIPv4(const String& ip);        // RFC1918 / CGNAT / link-local / loopback

	BMessenger _target;
	uint16     _internalPort;

	thread_id     _thread;
	volatile bool _keepRunning;

	uint32 _localIP;      // host byte order
	uint32 _gatewayIP;    // host byte order

	enum { METHOD_NONE = 0, METHOD_PCP, METHOD_NATPMP, METHOD_UPNP } _activeMethod;

	String _upnpControlURL;
	String _upnpServiceType;
	String _upnpHost;
	uint16 _upnpPort;
	uint16 _mappedExternalPort;
	uint8  _pcpNonce[12];   // PCP mapping nonce (RFC 6887), kept so we can delete the same mapping

	mutable BLocker _stateLock;
	String _externalIP;
	uint16 _externalPort;
	bool   _isMapped;

	volatile bool _probeRequested;   // set by ProbeReachability() / on a fresh map; consumed by the loop
	int    _reachability;            // 1 reachable, 0 not, -1 unknown (guarded by _stateLock)
	String _internetIP;              // internet-visible IP from the last probe (guarded by _stateLock)
};

};  // namespace beshare

#endif
