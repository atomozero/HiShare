#include <stdio.h>
#include <unistd.h>

#include "ShareApplication.h"
#include "system/SetupSystem.h"

using namespace beshare;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   ShareApplication beshareApp((argc>1)?argv[1]:NULL);
   beshareApp.Run();

   // Run() returns only after the app has fully quit, so every app-level cleanup
   // (settings save, router port-unmapping, network/accept thread shutdown) has
   // already run in ~ShareWindow.  We now exit immediately instead of unwinding the
   // C++ static/global destructors: muscle keeps its per-type ObjectPools (DataNode,
   // etc.) in global statics, and when connected the client's internal muscle
   // sessions (ThreadWorkerSession is a StorageReflectSession) hold a DataNode tree
   // of the server's node data whose Refs are only released during that static
   // teardown — after the pools' own static destructors run — tripping ObjectPool's
   // "still in use" assertion (an abort() on every connected quit).  _exit() skips
   // the buggy teardown; the OS reclaims the pool memory at process exit.
   fflush(NULL);
   _exit(0);
}
