#ifndef ShareSettingsWindow_h
#define ShareSettingsWindow_h

#include <Window.h>
#include <Messenger.h>
#include <Message.h>

class BListView;
class BCardLayout;
class BStringView;
class BTextControl;

namespace beshare {

// A modern, categorised Settings window that replaces BeShare's old 24-item Settings
// menu.  It doesn't hold any settings state of its own: every control targets the main
// ShareWindow and sends the SAME SHAREWINDOW_COMMAND_* messages the old menu did, so all
// existing handlers and persisted state are reused.  Initial control values come from a
// snapshot BMessage passed by ShareWindow.
class ShareSettingsWindow : public BWindow {
public:
	// target : the main ShareWindow (receives all the setting commands)
	// state  : snapshot of current values (bools/ints) used to initialise the controls
	ShareSettingsWindow(const BMessenger & target, const BMessage & state);
	virtual ~ShareSettingsWindow();

	virtual void MessageReceived(BMessage * msg);
	virtual bool QuitRequested();

	// Sent by ShareWindow when a reachability verdict arrives, so the Network
	// card's status line updates live instead of staying a stale snapshot.
	// Fields: "reachable" (int32), "internetip" (string), "extport" (int32).
	enum { MSG_REACH_UPDATE = 'stRe' };

	// Sent by ShareWindow when the local name/status changes elsewhere (/nick,
	// auto-away, …) so the Profile card's fields stay in sync while open.
	// Fields (optional): "username" (string), "userstatus" (string).
	enum { MSG_PROFILE_UPDATE = 'stPu' };

private:
	enum { MSG_CATEGORY = 'stCa', MSG_CLOSE = 'stCl', MSG_APPLY_NAME = 'stAn', MSG_APPLY_STATUS = 'stAs' };

	BView * _MakeProfileCard(const BMessage & s);
	BView * _MakeNetworkCard(const BMessage & s);
	BView * _MakeTransfersCard(const BMessage & s);
	BView * _MakeInterfaceCard(const BMessage & s);
	BView * _MakeChatCard(const BMessage & s);

	static void _FormatReachText(int32 reach, const char * ip, int32 extport, char * buf, size_t bufSize);

	BMessenger  _target;
	BListView * _categories;
	BCardLayout * _cards;
	BStringView * _reachLabel;
	BTextControl * _userNameField;
	BTextControl * _userStatusField;
};

};  // namespace beshare

#endif
