#ifndef ServerConnection_h
#define ServerConnection_h

#include <storage/Directory.h>

#include "util/String.h"
#include "BeShareNameSpace.h"

class BMessageRunner;

namespace beshare {

class ShareNetClient;

/*
 * Wraps a single connection to a MUSCLE server.  It owns the ShareNetClient
 * that talks to that server and carries the per-connection identity: a stable
 * connID (unique for the lifetime of the window) and the server's name.
 *
 * ShareWindow keeps a list of these so that it can eventually be connected to
 * several servers simultaneously; for now the list always holds exactly one,
 * so this is a pure structural extraction with no behavioural change.
 */
class ServerConnection
{
public:
   ServerConnection(int32 connID, const BDirectory & shareDir, int32 localSharePort);
   ~ServerConnection();

   /* The MUSCLE client for this server.  Owned by us. */
   ShareNetClient * Client() const {return _netClient;}

   /* Stable identifier for this connection, assigned at creation time. */
   int32 GetConnID() const {return _connID;}

   /* The server this connection is (or will be) pointed at. */
   const String & GetServerName() const {return _serverName;}
   void SetServerName(const char * name) {_serverName = name;}

   /* Connection state, updated by ShareWindow::SetConnectStatus(). */
   bool IsConnected() const  {return _isConnected;}
   bool IsConnecting() const {return _isConnecting;}
   void SetConnectState(bool isConnecting, bool isConnected) {_isConnecting = isConnecting; _isConnected = isConnected;}

   /* Auto-reconnect bookkeeping.  The runner (owned by us) periodically posts
    * SHAREWINDOW_COMMAND_AUTO_RECONNECT (tagged with our connID) to the window. */
   BMessageRunner * GetAutoReconnectRunner() const {return _autoReconnectRunner;}
   void SetAutoReconnectRunner(BMessageRunner * runner);  // deletes any previous one
   uint32 GetAutoReconnectAttemptCount() const {return _autoReconnectAttemptCount;}
   uint32 IncrementAutoReconnectAttemptCount() {return _autoReconnectAttemptCount++;}  // returns the pre-increment value
   void ResetAutoReconnectAttemptCount() {_autoReconnectAttemptCount = 0;}

private:
   int32 _connID;
   String _serverName;
   ShareNetClient * _netClient;

   bool _isConnecting;
   bool _isConnected;
   BMessageRunner * _autoReconnectRunner;
   uint32 _autoReconnectAttemptCount;
};

};  // end namespace beshare

#endif
