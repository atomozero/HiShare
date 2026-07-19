#include "RemoteUserItem.h"
#include "RemoteFileItem.h"
#include "ServerConnection.h"
#include "ShareStrings.h"
#include "ShareWindow.h"

namespace beshare {

enum {
   REMOTE_USER_COLUMN_HANDLE = 0,
   REMOTE_USER_COLUMN_STATUS,
   REMOTE_USER_COLUMN_ID,
   REMOTE_USER_COLUMN_FILES,
   REMOTE_USER_COLUMN_BANDWIDTH,
   REMOTE_USER_COLUMN_LOAD,
   REMOTE_USER_COLUMN_CLIENT,
   REMOTE_USER_COLUMN_SERVER,
   NUM_REMOTE_USER_COLUMNS
};

void
RemoteUserItem ::
UpdateField(int32 column, const char * text)
{
   BStringField * field = (BStringField *)GetField(column);
   if (field)
      field->SetString(text);
   else
      SetField(new BStringField(text), column);
}

RemoteUserItem ::
RemoteUserItem(ShareWindow * owner, const char * sessionID)
   : _owner(owner), _conn(NULL), _sessionID(sessionID), _handle(str(STR_ANONYMOUS)), _displayHandle(str(STR_ANONYMOUS)), _port(0), _firewalled(false), _isBot(false), _supportsPartialHash(false), _supportsSSL(false), _supportsRanges(false), _bandwidthLabel(str(STR_UNKNOWN)), _bandwidth(0), _installID(0)
{
   String text = GetUserString();
   text += str(STR_IS_NOW_CONNECTED);
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

   UpdateField(REMOTE_USER_COLUMN_HANDLE, _displayHandle());
   UpdateField(REMOTE_USER_COLUMN_STATUS, _displayStatus());
   UpdateField(REMOTE_USER_COLUMN_ID, _sessionID());
   UpdateField(REMOTE_USER_COLUMN_BANDWIDTH, "");
   UpdateField(REMOTE_USER_COLUMN_LOAD, "");
   UpdateField(REMOTE_USER_COLUMN_CLIENT, "");
   UpdateField(REMOTE_USER_COLUMN_SERVER, "");
   SetNumSharedFiles(-1);
   SetUploadStats(NO_FILE_LIMIT, NO_FILE_LIMIT);
}

RemoteUserItem ::
~RemoteUserItem()
{
   String text = GetUserString();
   text += str(STR_HAS_DISCONNECTED);
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());
   ClearFiles();
}

void
RemoteUserItem ::
SetConn(ServerConnection * conn)
{
   _conn = conn;

   char buf[32];
   sprintf(buf, "%ld:", conn ? (long) conn->GetConnID() : -1L);
   _userKey = buf;
   _userKey += _sessionID;

   UpdateField(REMOTE_USER_COLUMN_SERVER, conn ? conn->GetServerName()() : "");
}

String
RemoteUserItem ::
GetUserString() const
{
   String text(str(STR_USER_NUMBER));
   text += _sessionID;

   if (!_handle.Equals(str(STR_ANONYMOUS)))
   {
      text += str(STR_AKA);
      text += _handle();
      text += ")";
   }
   return text;
}

void
RemoteUserItem ::
SetHandle(const char * handle, const char * displayHandle)
{
   if ((strcmp(handle, _handle()))||(strcmp(displayHandle, _displayHandle())))
   {
      String text = GetUserString();
      text += str(STR_IS_NOW_KNOWN_AS);
      text += handle;
      _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

      _handle = handle;
      _displayHandle = displayHandle;

      UpdateField(REMOTE_USER_COLUMN_HANDLE, _displayHandle());
      _owner->RefreshUserItem(this);
      _owner->RefreshTransfersFor(this);

      RemoteFileItem * next;
      HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
      while(iter.GetNextValue(next) == B_NO_ERROR) _owner->RefreshFileItem(next);
   }
}

void
RemoteUserItem ::
SetStatus(const char * status, const char * displayStatus)
{
   String text = GetUserString();
   text += str(STR_IS_NOW);
   text += status;
   _owner->LogMessage(LOG_USER_EVENT_MESSAGE, text());

   _status = status;
   _displayStatus = displayStatus;
   UpdateField(REMOTE_USER_COLUMN_STATUS, _displayStatus());
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetNumSharedFiles(int32 bw)
{
   _numSharedFiles = bw;

   char temp[100] = "?";
   if (_numSharedFiles >= 0)
   {
      if ((_firewalled)&&(_owner->GetFirewalled())) sprintf(temp, "(%li)", (long int) _numSharedFiles);
                                               else sprintf(temp, "%li", (long int) _numSharedFiles);
   }
   UpdateField(REMOTE_USER_COLUMN_FILES, temp);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetUploadStats(uint32 cu, uint32 mu)
{
   _curUploads = cu;
   _maxUploads = mu;

   char temp[128];
   if (_curUploads < NO_FILE_LIMIT)
   {
      if (_maxUploads >= NO_FILE_LIMIT) sprintf(temp, "(%lu) 0%%", (long unsigned int) _curUploads);
                                   else sprintf(temp, "(%lu/%lu) %.0f%%", (long unsigned int) _curUploads, (long unsigned int) _maxUploads, GetLoadFactor()*100.0f);
   }
   else strcpy(temp, "?");

   UpdateField(REMOTE_USER_COLUMN_LOAD, temp);
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetBandwidth(const char * bandwidthLabel, uint32 bps)
{
   _bandwidthLabel = bandwidthLabel;
   _bandwidth = bps;

   RemoteFileItem * next;
   HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
   while(iter.GetNextValue(next) == B_NO_ERROR) _owner->RefreshFileItem(next);

   UpdateField(REMOTE_USER_COLUMN_BANDWIDTH, _bandwidthLabel());
   _owner->RefreshUserItem(this);
}

void
RemoteUserItem ::
SetClient(const char * client, const char * displayClient)
{
   _client = client;
   _displayClient = displayClient;
   UpdateField(REMOTE_USER_COLUMN_CLIENT, _displayClient());
}

void
RemoteUserItem ::
ClearFiles()
{
   HashtableIterator<const char *, RemoteFileItem *> iter = _files.GetIterator();
   RemoteFileItem * next;
   while(iter.GetNextValue(next) == B_NO_ERROR)
   {
      _owner->RemoveFileItem(next);
      delete next;
   }
   _files.Clear();
}

void
RemoteUserItem ::
PutFile(const char * fileName, const MessageRef & fileAttrs)
{
   RemoveFile(fileName);
   RemoteFileItem * item = new RemoteFileItem(this, fileName, fileAttrs);
   _files.Put(item->GetFileName(), item);
   _owner->AddFileItem(item);
}

void
RemoteUserItem ::
RemoveFile(const char * fileName)
{
   RemoteFileItem * item;
   if (_files.Remove(fileName, item) == B_NO_ERROR)
   {
      _owner->RemoveFileItem(item);
      delete item;
   }
}

void
RemoteUserItem ::
SetFirewalled(bool fw)
{
   if (fw != _firewalled)
   {
      _firewalled = fw;
      SetNumSharedFiles(GetNumSharedFiles());
   }
}

float
RemoteUserItem ::
GetLoadFactor() const
{
   if (_maxUploads >= NO_FILE_LIMIT) return  0.0f;
   if (_curUploads >= NO_FILE_LIMIT) return -1.0f;
   return ((float)_curUploads)/((float)_maxUploads);
}

};  // end namespace beshare
