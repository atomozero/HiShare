// Validation of muscle's OpenSSL-3-ported SSL layer on Haiku.
//  Part A: do the bundled PEM keys load via the ported PEM_read_bio_* code?
//  Part B: does a real TLS handshake + encrypted round-trip complete?
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "dataio/SSLSocketDataIO.h"
#include "iogateway/SSLSocketAdapterGateway.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "util/ByteBuffer.h"
#include "system/SetupSystem.h"

using namespace muscle;

static ByteBufferRef SlurpPem(const char * path)
{
   FILE * f = fopen(path, "rb");
   if (!f) { printf("FAIL: cannot open %s\n", path); return ByteBufferRef(); }
   fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
   ByteBufferRef buf = GetByteBufferFromPool((uint32)len);
   if (buf()) { size_t r = fread(buf()->GetBuffer(), 1, len, f); (void)r; }
   fclose(f);
   return buf;
}

static ByteBufferRef gPriv, gPub;
static uint16 gConnectPort = 0;
static ConstSocketRef gListenSock;
static volatile bool gServerOK = false, gClientOK = false;

// Simple blocking read of exactly n bytes through SSLSocketDataIO (blocking mode).
static bool ReadN(SSLSocketDataIO & io, void * buf, uint32 n)
{
   uint32 got = 0;
   for (int spins = 0; got < n && spins < 100000; spins++)
   {
      int32 r = io.Read(((char*)buf)+got, n-got);
      if (r > 0) { got += r; spins = 0; }
      else if (r < 0) return false;   // error/closed
      // r==0 => would-block; loop again (blocking socket makes this rare)
   }
   return got == n;
}
static bool WriteN(SSLSocketDataIO & io, const void * buf, uint32 n)
{
   uint32 put = 0;
   for (int spins = 0; put < n && spins < 100000; spins++)
   {
      int32 r = io.Write(((const char*)buf)+put, n-put);
      if (r > 0) { put += r; spins = 0; }
      else if (r < 0) return false;
   }
   return put == n;
}

static void * ServerFn(void *)
{
   ConstSocketRef conn = Accept(gListenSock);
   if (conn() == NULL) { printf("SERVER: accept failed\n"); return NULL; }
   (void) SetSocketBlockingEnabled(conn, true);
   SSLSocketDataIO io(conn, true /*blocking*/, true /*accept*/);
   // TLS server needs BOTH the private key AND its matching certificate to present.
   if (io.SetPrivateKey(gPriv()->GetBuffer(), gPriv()->GetNumBytes()) != B_NO_ERROR) { printf("SERVER: SetPrivateKey failed\n"); return NULL; }
   if (io.SetPublicKeyCertificate(gPub()->GetBuffer(), gPub()->GetNumBytes()) != B_NO_ERROR) { printf("SERVER: SetPublicKeyCertificate failed\n"); return NULL; }
   char in[8]; memset(in,0,sizeof(in));
   if (ReadN(io, in, 4) && memcmp(in,"PING",4)==0) { printf("SERVER: decrypted '%.4s'\n", in); if (WriteN(io,"PONG",4)) gServerOK = true; }
   else printf("SERVER: read failed ('%.4s')\n", in);
   return NULL;
}
static void * ClientFn(void *)
{
   ConstSocketRef conn = Connect(localhostIP, gConnectPort, "localhost", "ssltest", false);
   if (conn() == NULL) { printf("CLIENT: connect failed\n"); return NULL; }
   (void) SetSocketBlockingEnabled(conn, true);
   SSLSocketDataIO io(conn, true /*blocking*/, false /*connect*/);
   if (io.SetPublicKeyCertificate(gPub()->GetBuffer(), gPub()->GetNumBytes()) != B_NO_ERROR) { printf("CLIENT: SetPublicKeyCertificate failed\n"); return NULL; }
   if (!WriteN(io,"PING",4)) { printf("CLIENT: write failed\n"); return NULL; }
   char in[8]; memset(in,0,sizeof(in));
   if (ReadN(io, in, 4) && memcmp(in,"PONG",4)==0) { printf("CLIENT: decrypted '%.4s'\n", in); gClientOK = true; }
   else printf("CLIENT: read failed ('%.4s')\n", in);
   return NULL;
}

int main()
{
   CompleteSetupSystem css;
   ByteBufferRef priv = SlurpPem("../muscle/ssl_data/beshare_private_key.pem");
   ByteBufferRef pub  = SlurpPem("../muscle/ssl_data/beshare_public_key.pem");
   if (!priv() || !pub()) return 1;
   printf("Loaded private key (%u bytes) + public cert (%u bytes)\n", priv()->GetNumBytes(), pub()->GetNumBytes());

   // --- Part A: key/cert loading through the ported OpenSSL-3 code paths ---
   ConstSocketRef s1, s2;
   if (CreateConnectedSocketPair(s1, s2) != B_NO_ERROR) { printf("FAIL: socket pair\n"); return 1; }
   bool keyA = false, certA = false;
   {
      SSLSocketDataIO srv(s1, false, true /*accept*/);
      keyA = (srv.SetPrivateKey(priv()->GetBuffer(), priv()->GetNumBytes()) == B_NO_ERROR);
      SSLSocketDataIO cli(s2, false, false /*connect*/);
      certA = (cli.SetPublicKeyCertificate(pub()->GetBuffer(), pub()->GetNumBytes()) == B_NO_ERROR);
   }
   printf("Part A  SetPrivateKey: %s   SetPublicKeyCertificate: %s\n",
          keyA ? "OK" : "FAIL", certA ? "OK" : "FAIL");

   // --- Part B: full TLS handshake + encrypted round-trip over a real TCP loopback,
   //     blocking mode, one thread per endpoint (SSL_read/SSL_write block through the handshake). ---
   gPriv = priv; gPub = pub;
   uint16 port = 0;
   gListenSock = CreateAcceptingSocket(0, 1, &port);
   if (gListenSock() == NULL) { printf("FAIL: listen socket\n"); return 1; }
   printf("Part B  listening on loopback port %u\n", port);
   gConnectPort = port;

   pthread_t st, ct;
   pthread_create(&st, NULL, ServerFn, NULL);
   pthread_create(&ct, NULL, ClientFn, NULL);
   pthread_join(st, NULL);
   pthread_join(ct, NULL);
   bool gotPing = gServerOK, gotPong = gClientOK;
   printf("Part B  server got PING: %s   client got PONG: %s\n", gotPing?"OK":"FAIL", gotPong?"OK":"FAIL");

   bool ok = keyA && certA && gotPing && gotPong;
   printf("\n=== RESULT: %s ===\n", ok ? "muscle SSL works on Haiku (keys load + TLS handshake + encrypted round-trip)" : "FAILED");

   // Release all muscle Refs before CompleteSetupSystem tears down its ObjectPools,
   // otherwise muscle asserts (ObjectPool destroyed while objects still in use).
   priv.Reset(); pub.Reset(); gPriv.Reset(); gPub.Reset(); gListenSock.Reset();
   s1.Reset(); s2.Reset();
   return ok ? 0 : 1;
}
