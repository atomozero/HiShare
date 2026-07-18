#include <app/MessageRunner.h>

#include "ServerConnection.h"
#include "ShareNetClient.h"

namespace beshare {

ServerConnection ::
ServerConnection(int32 connID, const BDirectory & shareDir, int32 localSharePort)
   : _connID(connID), _netClient(new ShareNetClient(shareDir, localSharePort, this)),
     _isConnecting(false), _isConnected(false), _autoReconnectRunner(NULL), _autoReconnectAttemptCount(0)
{
   // empty
}

ServerConnection ::
~ServerConnection()
{
   delete _autoReconnectRunner;
   delete _netClient;
}

void
ServerConnection ::
SetAutoReconnectRunner(BMessageRunner * runner)
{
   delete _autoReconnectRunner;
   _autoReconnectRunner = runner;
}

};  // end namespace beshare
