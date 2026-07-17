#include "ServerConnection.h"
#include "ShareNetClient.h"

namespace beshare {

ServerConnection ::
ServerConnection(int32 connID, const BDirectory & shareDir, int32 localSharePort)
   : _connID(connID), _netClient(new ShareNetClient(shareDir, localSharePort, this))
{
   // empty
}

ServerConnection ::
~ServerConnection()
{
   delete _netClient;
}

};  // end namespace beshare
