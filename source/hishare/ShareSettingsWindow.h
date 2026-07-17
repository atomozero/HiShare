#ifndef ShareSettingsWindow_h
#define ShareSettingsWindow_h

#include <Window.h>
#include <Messenger.h>
#include <Message.h>

class BListView;
class BCardLayout;

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

private:
	enum { MSG_CATEGORY = 'stCa', MSG_CLOSE = 'stCl' };

	BView * _MakeNetworkCard(const BMessage & s);
	BView * _MakeTransfersCard(const BMessage & s);
	BView * _MakeInterfaceCard(const BMessage & s);
	BView * _MakeChatCard(const BMessage & s);

	BMessenger  _target;
	BListView * _categories;
	BCardLayout * _cards;
};

};  // namespace beshare

#endif
