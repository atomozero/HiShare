#include <stdio.h>

#include <interface/ScrollBar.h>
#include <interface/ScrollView.h>
#include <interface/Screen.h>
#include <interface/Input.h>
#include <interface/PopUpMenu.h>
#include <interface/ColumnListView.h>
#include <interface/ColumnTypes.h>

#include <support/Beep.h>

#include "regex/StringMatcher.h"
#include "util/StringTokenizer.h"

#include "PrivateChatWindow.h"
#include "ShareStrings.h"
#include "Colors.h"

#include "SplitPane.h"

namespace beshare {

#define MIN_WIDTH  200
#define MIN_HEIGHT 125
#define MAX_WIDTH  65535
#define MAX_HEIGHT 65535

#define TEXT_ENTRY_HEIGHT 20.0f
#define USER_LIST_WIDTH   125.0f

static BRect ExtractRect(const BMessage & msg);
BRect ExtractRect(const BMessage & msg)
{
   BRect rect;
   if (msg.FindRect("frame", &rect) == B_NO_ERROR) 
   {
      BRect screenBounds;
      {
         BScreen s;
         screenBounds = s.Frame();
      }
      if (rect.left > screenBounds.Width()) rect.left = 50;
      if (rect.top > screenBounds.Height()) rect.top = 50;
      return rect;
   }
   else return BRect(30,523,670,700);
}

void 
PrivateChatWindow :: UpdateTitleBar()
{
   const String & s = GetCustomWindowTitle();
   SetTitle((s.Length() > 0) ? s() : str(STR_BESHARE_PRIVATE_CHAT));
}

PrivateChatWindow :: PrivateChatWindow(bool loggingEnabled, const BMessage & msg, uint32 idx, ShareWindow * mainWindow, const char * defaultStr) :
   ChatWindow(ExtractRect(msg),str(STR_BESHARE_PRIVATE_CHAT),B_FLOATING_WINDOW_LOOK,B_NORMAL_WINDOW_FEEL,0L),
   _index(idx),
   _mainWindow(mainWindow),
   _munged(false)
{
   const float hMargin = 2.0f;
   const float vMargin = 2.0f;

   SetSizeLimits(MIN_WIDTH, MAX_WIDTH, MIN_HEIGHT, MAX_HEIGHT);

   BMessenger toMe(this);

   BRect b(Bounds());

   BView * contentView = new BView(b, NULL, B_FOLLOW_ALL_SIDES, 0);
   AddBorderView(contentView);
   AddChild(contentView);

   BRect logLabelRect(b.Width()-(hMargin+contentView->StringWidth(str(STR_LOG))+25.0f), vMargin, b.Width()-hMargin, TEXT_ENTRY_HEIGHT);
   _logEnabled = new BCheckBox(logLabelRect, NULL, str(STR_LOG), NULL, B_FOLLOW_RIGHT | B_FOLLOW_TOP);
   if (loggingEnabled) _logEnabled->SetValue(B_CONTROL_ON);
   AddBorderView(_logEnabled);
   contentView->AddChild(_logEnabled);

   _usersEntry = new BTextControl(BRect(hMargin, vMargin, logLabelRect.left-hMargin, TEXT_ENTRY_HEIGHT), NULL, str(STR_CHAT_WITH), defaultStr, new BMessage(PRIVATE_WINDOW_USER_TEXT_CHANGED), B_FOLLOW_TOP|B_FOLLOW_LEFT_RIGHT);
   _usersEntry->SetDivider(_usersEntry->StringWidth(str(STR_CHAT_WITH))+4.0f);
   _usersEntry->SetTarget(toMe);
   _usersEntry->MakeFocus();
   AddBorderView(_usersEntry);
   contentView->AddChild(_usersEntry);

   BRect splitBounds(hMargin,_usersEntry->Frame().bottom+vMargin,b.Width()-hMargin, b.Height()-vMargin);

   _chatView = new BView(BRect(0,0,splitBounds.Width()-USER_LIST_WIDTH,splitBounds.Height()), NULL, B_FOLLOW_ALL_SIDES, 0);
   AddBorderView(_chatView);

   _usersList = new BColumnListView(BRect(0, 0, USER_LIST_WIDTH, splitBounds.Height()), NULL, B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS|B_NAVIGABLE, B_FANCY_BORDER);

   float nameColWidth = USER_LIST_WIDTH - 30.0f;
   float idColWidth = 30.0f;
   BMessage tableMsg;
   if (msg.FindMessage("table", &tableMsg) == B_NO_ERROR)
   {
      float w;
      if (msg.FindFloat("colwidth", 0, &w) == B_NO_ERROR) nameColWidth = w;
      if (msg.FindFloat("colwidth", 1, &w) == B_NO_ERROR) idColWidth = w;
   }

   _usersList->AddColumn(new BStringColumn(str(STR_NAME), nameColWidth, 20, 500, B_TRUNCATE_END), 0);
   _usersList->AddColumn(new BStringColumn(str(STR_ID), idColWidth, 20, 100, B_TRUNCATE_END, B_ALIGN_RIGHT), 1);
   _usersList->SetSortingEnabled(true);
   _usersList->SetSortColumn(_usersList->ColumnAt(0), true, true);

   _split = new SplitPane(splitBounds, _chatView, _usersList, B_FOLLOW_ALL_SIDES);
   _split->SetResizeViewOne(true, true);
   _split->SetBarPosition(BPoint(splitBounds.right-(B_V_SCROLL_BAR_WIDTH+USER_LIST_WIDTH), splitBounds.bottom-30.0f));
   BMessage splitMsg;
   if (msg.FindMessage("split", &splitMsg) == B_NO_ERROR) _split->SetState(&splitMsg);

   contentView->AddChild(_split);
   AddBorderView(_split);

   float fontSize;
   if (msg.FindFloat("fontsize", &fontSize) == B_NO_ERROR) SetFontSize(fontSize);

   const char * font;
   if (msg.FindString("font", &font) == B_NO_ERROR) SetFont(font, false);
}

PrivateChatWindow :: ~PrivateChatWindow()
{
   BMessage msg(PRIVATE_WINDOW_CLOSED);
   msg.AddPointer("which", this);

   BMessage state;
   SaveStateTo(state);
   msg.AddMessage("state", &state);
   (void) _mainWindow->PostMessage(&msg);
}

void
PrivateChatWindow ::
SaveStateTo(BMessage & msg) const
{
   msg.AddRect("frame", Frame());

   BMessage sp;
   _split->GetState(sp);
   msg.AddMessage("split", &sp);
   msg.AddInt32("index", _index);

   BMessage table;
   BColumn * col0 = _usersList->ColumnAt(0);
   BColumn * col1 = _usersList->ColumnAt(1);
   msg.AddFloat("colwidth", col0 ? col0->Width() : 100.0f);
   msg.AddFloat("colwidth", col1 ? col1->Width() : 30.0f);
   msg.AddMessage("table", &table);
   msg.AddFloat("fontsize", GetFontSize());
   msg.AddString("font", GetFont()());
}

void PrivateChatWindow :: MessageReceived(BMessage * msg)
{
   switch(msg->what)
   {
      case PRIVATE_WINDOW_USER_TEXT_CHANGED:
      {
         BMessage forward(PRIVATE_WINDOW_USER_TEXT_CHANGED);
         forward.AddString("users", _usersEntry->Text());
         forward.AddPointer("which", this);
         (void) _mainWindow->PostMessage(&forward);
         MakeChatTextFocus();
      }
      break;

      case PRIVATE_WINDOW_ADD_USER:
      { 
         const char * id;
         const char * name;
         if ((msg->FindString("name", &name) == B_NO_ERROR)&&
             (msg->FindString("id",   &id)   == B_NO_ERROR))
         {
            BRow * row = new BRow();
            row->SetField(new BStringField(name), 0);
            row->SetField(new BStringField(id), 1);
            _usersList->AddRow(row);
         }
      }
      break;

      case PRIVATE_WINDOW_REMOVE_USER:
      { 
         const char * id;
         if (msg->FindString("id", &id) != B_NO_ERROR) id = NULL;

         for (int i = _usersList->CountRows()-1; i >= 0; i--)
         {
            BRow * row = _usersList->RowAt(i);
            BStringField * idField = (BStringField *)row->GetField(1);
            if ((id == NULL)||(strcmp(id, idField->String()) == 0))
            {
               _usersList->RemoveRow(row);
               delete row;
               if (id) break;
            }
         }
      }
      break;

      case LOG_REMOTE_USER_CHAT_MESSAGE:
      {
         const char * text, * id;
         if ((msg->FindString("text", &text) == B_NO_ERROR)&&
             (msg->FindString("sid", &id) == B_NO_ERROR))
         {
            if (IsActive() == false) DoBeep(SYSTEM_SOUND_INACTIVE_CHAT_WINDOW_RECEIVED_TEXT);
            LogMessage(LOG_REMOTE_USER_CHAT_MESSAGE, text, id, &GetColor(COLOR_PRIVATE), true, NULL);
         }
      }
      break;

      default:
         ChatWindow :: MessageReceived(msg);
      break;
   }
}

status_t PrivateChatWindow :: DoTabCompletion(const char * origText, String & returnCompletedText, const char * optMatchExpression) const
{
   status_t ret = B_ERROR;
   if (_mainWindow->Lock())
   {
      ret = _mainWindow->DoTabCompletion(origText, returnCompletedText, optMatchExpression ? optMatchExpression : _usersEntry->Text());
      _mainWindow->Unlock();
   }
   return ret;
}

void PrivateChatWindow :: GetUserNameForSession(const char * sessionID, String & retUserName) const
{
   if (_mainWindow->Lock())
   {
      _mainWindow->GetUserNameForSession(sessionID, retUserName);
      _mainWindow->Unlock();
   }
}

void PrivateChatWindow :: GetLocalUserName(String & retLocalUserName) const
{
   if (_mainWindow->Lock())
   {
      _mainWindow->GetLocalUserName(retLocalUserName);
      _mainWindow->Unlock();
   }
}

void PrivateChatWindow :: GetLocalSessionID(String & retLocalSessionID) const
{
   if (_mainWindow->Lock())
   {
      _mainWindow->GetLocalSessionID(retLocalSessionID);
      _mainWindow->Unlock();
   }
}


bool PrivateChatWindow :: OkayToLog(LogMessageType, LogDestinationType destType, bool) const
{
   switch(destType)
   {
      case DESTINATION_DISPLAY:  return true;
      case DESTINATION_LOG_FILE: return (_logEnabled->Value() == B_CONTROL_ON);
      default:                   return false;
   }
}
   

bool PrivateChatWindow :: ShowTimestamps(LogDestinationType dest) const
{
   bool ret = false;
   if (_mainWindow->Lock())
   {
      ret = _mainWindow->ShowTimestamps(dest);
      _mainWindow->Unlock();
   }
   return ret;
}

bool PrivateChatWindow :: ShowUserIDs(LogDestinationType dest) const
{
   bool ret = false;
   if (_mainWindow->Lock())
   {
      ret = _mainWindow->ShowUserIDs(dest);
      _mainWindow->Unlock();
   }
   return ret;
}

void PrivateChatWindow :: SendChatText(const String & text, ChatWindow *)
{
   String altText;
   bool useAltText = false;

   if ((text.StartsWith("/"))&&(!text.StartsWith("//")))
   {
      _munged = false;
      int actionKeywordChars = 0;
           if (text.StartsWith("/me "))     actionKeywordChars = 4;
      else if (text.StartsWith("/me's "))   actionKeywordChars = 3;
      else if (text.StartsWith("/action ")) actionKeywordChars = 8;
      else if (text.StartsWith("/clear"))
      {
         ClearChatLog();
         return;
      }
      else if (text.StartsWith("/fontsize"))
      {
         SetFontSize(text);
         return;
      }
      else if (text.StartsWith("/font"))
      {
         SetFont(text, true);
         return;
      }

      if (actionKeywordChars > 0)
      {
         useAltText = true;
         altText = text.Substring(actionKeywordChars);

         String pre("/msg ");

         String target(_usersEntry->Text());
         target.Replace(' ', CLUMP_CHAR);
         pre += target;
         pre += ' ';
         
         String myName;
         GetLocalUserName(myName);
         pre += myName;
         pre += ' ';
         altText = pre.Append(altText);
      }
   }
   else 
   {
      String target(_usersEntry->Text());
      target = target.Trim();

      String temp("/msg ");
      target.Replace(' ', CLUMP_CHAR);
      temp += target;
      temp += ' ';
      useAltText = true;
      altText = temp.Append(text);
      _munged = true;
   }
   if (_mainWindow->Lock())
   {
      _mainWindow->SendChatText(useAltText ? altText : text, this);
      _mainWindow->Unlock();
   }
}

status_t
PrivateChatWindow ::
ExpandAlias(const String & str, String & retStr) const 
{
   status_t ret = B_ERROR;
   if (_mainWindow->Lock())
   {
      ret = _mainWindow->ExpandAlias(str, retStr);
      _mainWindow->Unlock();
   }
   return ret;
}

void
PrivateChatWindow ::
DispatchMessage(BMessage * msg, BHandler * handler)
{
   switch(msg->what)
   {
      case B_MOUSE_DOWN:
         if (handler == _usersList) _usersList->MakeFocus();
      break;

      case B_KEY_DOWN:
      {
         int8 key;
         int32 mods;
         int8 sc = shortcut(SHORTCUT_CLEAR_CHAT_LOG);
         if ((msg->FindInt8("byte", &key) == B_NO_ERROR)&&(msg->FindInt32("modifiers", &mods) == B_NO_ERROR)&&((key == sc)||(key == sc +('a'-'A')))&&((mods & B_CONTROL_KEY)||(mods & B_COMMAND_KEY)))
         {
            ClearChatLog();
            msg = NULL;
         }
      }
      break;
   }
   if (msg) ChatWindow::DispatchMessage(msg, handler);
}

String 
PrivateChatWindow ::
GetQualifiedSharedFileName(const String & name) const
{
   if (_mainWindow->Lock())
   {
      String ret = _mainWindow->GetQualifiedSharedFileName(name);
      _mainWindow->Unlock();
      return ret;
   }
   else return name;
}

void
PrivateChatWindow ::
ChatTextReceivedBeep(bool, bool)
{
   DoBeep(SYSTEM_SOUND_PRIVATE_MESSAGE_WINDOW);
}

void
PrivateChatWindow ::
UpdateColors()
{
   ChatWindow::UpdateColors();
   UpdateTextViewColors(_usersEntry->TextView());
}

};  // end namespace beshare
