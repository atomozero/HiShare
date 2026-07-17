// Real end-to-end probe: connect to a running BeShare's share port as a TLS *client*
// (using BeShare's bundled public cert) and see if BeShare completes the TLS handshake
// as the *server*.  A successful handshake proves the in-app accept-side TLS path
// (InitSocketUploadSession's SetSSLPrivateKey/Certificate) works in the real binary.
//   usage: tlsprobe <port>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dataio/SSLSocketDataIO.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/ByteBuffer.h"
#include "system/SetupSystem.h"
#include "BeShareSSLKeys.h"

using namespace muscle;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   uint16 port = (argc > 1) ? (uint16)atoi(argv[1]) : 7000;

   ConstByteBufferRef pub = GetByteBufferFromPool((uint32)strlen(kBeShareTLSPublicKeyPEM), (const uint8 *)kBeShareTLSPublicKeyPEM);

   ConstSocketRef conn = Connect(localhostIP, port, "localhost", "tlsprobe", false);
   if (conn() == NULL) { printf("PROBE: could not connect to 127.0.0.1:%u\n", port); return 2; }
   (void) SetSocketBlockingEnabled(conn, true);

   SSLSocketDataIO io(conn, true /*blocking*/, false /*we are the TLS client*/);
   (void) io.SetPublicKeyCertificate(pub()->GetBuffer(), pub()->GetNumBytes());

   // Writing drives SSL_connect (the handshake).  If BeShare is a TLS server, this succeeds;
   // if BeShare is plaintext, it gets our ClientHello as garbage and the handshake fails.
   const char * hello = "beshare-tls-probe\n";
   int32 w = -1;
   for (int spins = 0; spins < 200000; spins++) { w = io.Write(hello, (uint32)strlen(hello)); if (w != 0) break; }

   if (w > 0)
   {
      printf("PROBE: TLS handshake with BeShare COMPLETED (wrote %d encrypted bytes)\n", (int)w);
      printf("=== RESULT: BeShare accept-side TLS works ===\n");
      return 0;
   }
   printf("PROBE: TLS handshake FAILED (Write returned %d) -- BeShare is not serving TLS on this port\n", (int)w);
   return 1;
}
