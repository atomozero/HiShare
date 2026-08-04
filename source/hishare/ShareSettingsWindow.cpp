#include "ShareSettingsWindow.h"
#include "ShareWindow.h"      // for the ShareWindow::SHAREWINDOW_COMMAND_* enum + BESHARE_MIME_TYPE
#include "ShareStrings.h"

#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <CardLayout.h>
#include <CheckBox.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <StringView.h>
#include <TextControl.h>

#include <stdio.h>

namespace beshare {

// ---- small control-builder helpers ----------------------------------------

// A checkbox that toggles the given command on the main window.  (extra) fields, if any,
// are merged into the sent message so value-carrying commands work too.
static BCheckBox *
NewCheck(const char * label, uint32 cmd, bool on, const BMessenger & target, const BMessage * extra = NULL)
{
	BMessage * m = new BMessage(cmd);
	if (extra) m->Append(*extra);
	BCheckBox * cb = new BCheckBox(label, label, m);
	cb->SetValue(on ? B_CONTROL_ON : B_CONTROL_OFF);
	cb->SetTarget(target);
	return cb;
}

static BStringView *
NewHint(const char * text)
{
	BStringView * sv = new BStringView("hint", text);
	sv->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.62f));
	BFont f; sv->GetFont(&f); f.SetSize(f.Size() * 0.90f); sv->SetFont(&f);
	return sv;
}

static BStringView *
NewSectionLabel(const char * text)
{
	BStringView * sv = new BStringView("sec", text);
	BFont f; sv->GetFont(&f); f.SetFace(B_BOLD_FACE); f.SetSize(f.Size() * 0.86f); sv->SetFont(&f);
	sv->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.5f));
	return sv;
}

// A labelled pop-up menu whose items each send (cmd) with (fieldName)=value.
struct Opt { const char * label; int32 value; };
static BMenuField *
NewValueField(const char * label, uint32 cmd, const char * fieldName, const Opt * opts, int n,
              int32 current, const BMessenger & target)
{
	BPopUpMenu * pop = new BPopUpMenu(label);
	for (int i=0; i<n; i++) {
		BMessage * m = new BMessage(cmd);
		m->AddInt32(fieldName, opts[i].value);
		BMenuItem * it = new BMenuItem(opts[i].label, m);
		it->SetTarget(target);
		if (opts[i].value == current) it->SetMarked(true);
		pop->AddItem(it);
	}
	return new BMenuField("vf", label, pop);
}

// ---- window ---------------------------------------------------------------

ShareSettingsWindow::ShareSettingsWindow(const BMessenger & target, const BMessage & state)
	: BWindow(BRect(0,0,620,430), str(STR_SETTINGS), B_TITLED_WINDOW,
	          B_ASYNCHRONOUS_CONTROLS | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
	, _target(target)
	, _categories(NULL)
	, _cards(NULL)
	, _reachLabel(NULL)
	, _userNameField(NULL)
	, _userStatusField(NULL)
{
	SetLayout(new BGroupLayout(B_HORIZONTAL));

	// left: category list
	_categories = new BListView("categories", B_SINGLE_SELECTION_LIST);
	_categories->AddItem(new BStringItem(str(STR_ST_PROFILE)));
	_categories->AddItem(new BStringItem(str(STR_ST_NETWORK)));
	_categories->AddItem(new BStringItem(str(STR_ST_TRANSFERS)));
	_categories->AddItem(new BStringItem(str(STR_ST_INTERFACE)));
	_categories->AddItem(new BStringItem(str(STR_ST_CHAT)));
	_categories->SetSelectionMessage(new BMessage(MSG_CATEGORY));
	_categories->SetTarget(this);
	BScrollView * catScroll = new BScrollView("catScroll", _categories, 0, false, false);
	catScroll->SetExplicitMaxSize(BSize(150, B_SIZE_UNLIMITED));
	catScroll->SetExplicitMinSize(BSize(130, 0));

	// right: card layout, one card per category
	BView * cardHost = new BView("cardHost", 0);
	_cards = new BCardLayout();
	cardHost->SetLayout(_cards);
	_cards->AddView(_MakeProfileCard(state));
	_cards->AddView(_MakeNetworkCard(state));
	_cards->AddView(_MakeTransfersCard(state));
	_cards->AddView(_MakeInterfaceCard(state));
	_cards->AddView(_MakeChatCard(state));

	BButton * close = new BButton("close", str(STR_ST_CLOSE), new BMessage(MSG_CLOSE));

	BLayoutBuilder::Group<>(this, B_HORIZONTAL, 0)
		.Add(catScroll)
		.AddGroup(B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(cardHost)
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(close)
			.End()
		.End();

	_categories->Select(0);
	CenterOnScreen();
}

ShareSettingsWindow::~ShareSettingsWindow() { }

bool ShareSettingsWindow::QuitRequested() { return true; }

void
ShareSettingsWindow::MessageReceived(BMessage * msg)
{
	switch (msg->what) {
		case MSG_CATEGORY: {
			int32 sel = _categories->CurrentSelection();
			if (sel >= 0) _cards->SetVisibleItem(sel);
		} break;

		case MSG_CLOSE:
			PostMessage(B_QUIT_REQUESTED);
		break;

		// The Profile text fields don't carry their text in the invocation message,
		// so read it here and hand it to the main window via the same commands the
		// old user-name/status pop-up menus used.
		case MSG_APPLY_NAME:
			if (_userNameField && _userNameField->Text()[0]) {
				BMessage m(ShareWindow::SHAREWINDOW_COMMAND_USER_SELECTED_USER_NAME);
				m.AddString("username", _userNameField->Text());
				_target.SendMessage(&m);
			}
		break;

		case MSG_APPLY_STATUS:
			if (_userStatusField && _userStatusField->Text()[0]) {
				BMessage m(ShareWindow::SHAREWINDOW_COMMAND_USER_SELECTED_USER_STATUS);
				m.AddString("userstatus", _userStatusField->Text());
				_target.SendMessage(&m);
			}
		break;

		case MSG_PROFILE_UPDATE: {
			const char * v;
			if (_userNameField   && (msg->FindString("username",   &v) == B_OK)) _userNameField->SetText(v);
			if (_userStatusField && (msg->FindString("userstatus", &v) == B_OK)) _userStatusField->SetText(v);
		} break;

		case MSG_REACH_UPDATE:
			if (_reachLabel) {
				char statusText[160];
				_FormatReachText(msg->GetInt32("reachable", -1), msg->GetString("internetip", ""),
				                 msg->GetInt32("extport", 0), statusText, sizeof(statusText));
				_reachLabel->SetText(statusText);
				_reachLabel->InvalidateLayout();
			}
		break;

		default:
			BWindow::MessageReceived(msg);
		break;
	}
}

void
ShareSettingsWindow::_FormatReachText(int32 reach, const char * ip, int32 extport, char * buf, size_t bufSize)
{
	if (reach == 1)      snprintf(buf, bufSize, "Reachable: %s:%ld is open to the internet.", ip, (long)extport);
	else if (reach == 0) snprintf(buf, bufSize, "Not reachable from the internet (NAT). Public IP %s.", (ip && ip[0]) ? ip : "unknown");
	else                 snprintf(buf, bufSize, "Reachability not tested yet.");
}

// Wrap a card's controls in a padded vertical group inside a titled box.
static BView *
Card(const char * title, BView * content)
{
	BBox * box = new BBox("card");
	box->SetLabel(title);
	BGroupLayout * gl = new BGroupLayout(B_VERTICAL, B_USE_DEFAULT_SPACING);
	box->SetLayout(gl);
	gl->SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
	gl->AddView(content);
	gl->AddItem(BSpaceLayoutItem::CreateGlue());
	return box;
}

BView *
ShareSettingsWindow::_MakeProfileCard(const BMessage & s)
{
	// These fields don't carry their text in the invocation message; MessageReceived
	// reads Text() on MSG_APPLY_* and forwards it to the main window.  BTextControl
	// invokes on Enter and on focus-out when the text changed, so both apply the value.
	_userNameField = new BTextControl("name", str(STR_ST_YOUR_NAME), s.GetString("username", ""),
	                                  new BMessage(MSG_APPLY_NAME));
	_userNameField->SetTarget(this);

	_userStatusField = new BTextControl("status", str(STR_ST_STATUS), s.GetString("userstatus", ""),
	                                    new BMessage(MSG_APPLY_STATUS));
	_userStatusField->SetTarget(this);

	BView * v = new BView("profile", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewSectionLabel(str(STR_ST_IDENTITY)))
		.Add(_userNameField)
		.Add(NewHint(str(STR_ST_NAME_HINT)))
		.AddStrut(6)
		.Add(_userStatusField)
		.Add(NewHint(str(STR_ST_STATUS_HINT)))
		.AddGlue()
		.SetInsets(0);
	return Card(str(STR_ST_CARD_PROFILE), v);
}

BView *
ShareSettingsWindow::_MakeNetworkCard(const BMessage & s)
{
	bool apf = s.GetBool("autoportforward"), fw = s.GetBool("firewalled"),
	     tls = s.GetBool("requiretls"), lo = s.GetBool("loginonstartup"),
	     aus = s.GetBool("autoupdateservers");
	int32 reach = s.GetInt32("reachable", -1), extport = s.GetInt32("extport", 0);
	const char * ip = s.GetString("internetip", "");
#if !BESHARE_TLS_ENABLED
	(void)tls;   // TLS checkbox hidden for 1.0; keep the read to preserve behaviour if re-enabled
#endif

	char statusText[160];
	_FormatReachText(reach, ip, extport, statusText, sizeof(statusText));

	BButton * testBtn = new BButton("test", str(STR_ST_TEST_NOW), new BMessage(ShareWindow::SHAREWINDOW_COMMAND_TEST_REACHABILITY));
	testBtn->SetTarget(_target);

	BView * v = new BView("net", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck(str(STR_ST_AUTO_PORT), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTO_PORT_FORWARD, apf, _target))
		.Add(NewHint(str(STR_ST_AUTO_PORT_HINT)))
		.AddStrut(4)
		.AddGroup(B_HORIZONTAL)
			.Add(_reachLabel = new BStringView("st", statusText))
			.AddGlue()
			.Add(testBtn)
		.End()
		.Add(NewCheck(str(STR_ST_FIREWALLED), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FIREWALLED, fw, _target))
		.AddStrut(6)
		.Add(NewSectionLabel(str(STR_ST_PRIVACY_ACCOUNT)))
#if BESHARE_TLS_ENABLED
		// Hidden for HiShare 1.0 — TLS downloads crash (see ShareConstants.h / beshare-tls-ssl).
		.Add(NewCheck(str(STR_ST_TLS), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_REQUIRE_TLS, tls, _target))
		.Add(NewHint(str(STR_ST_TLS_HINT)))
#endif
		.Add(NewCheck(str(STR_ST_LOGIN_STARTUP), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_LOGIN_ON_STARTUP, lo, _target))
		.Add(NewCheck(str(STR_ST_AUTOUPDATE_SERVERS), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTOUPDATE_SERVER_LIST, aus, _target))
		.AddGlue()
		.SetInsets(0);
	return Card(str(STR_ST_CARD_NETWORK), v);
}

BView *
ShareSettingsWindow::_MakeTransfersCard(const BMessage & s)
{
	const Opt kLimits[] = {{"1",1},{"2",2},{"3",3},{"4",4},{"5",5},{"6",6},{"8",8},{"10",10},{"16",16},{"32",32},{str(STR_ST_UNLIMITED),1000000}};
	int nL = sizeof(kLimits)/sizeof(kLimits[0]);

	BView * v = new BView("xfer", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewValueField(str(STR_ST_MAX_UP), ShareWindow::SHAREWINDOW_COMMAND_SET_UPLOAD_LIMIT, "num", kLimits, nL, s.GetInt32("uploads",3), _target))
		.Add(NewValueField(str(STR_ST_MAX_UP_USER), ShareWindow::SHAREWINDOW_COMMAND_SET_UPLOAD_PER_USER_LIMIT, "num", kLimits, nL, s.GetInt32("uploadsperuser",1), _target))
		.Add(NewValueField(str(STR_ST_MAX_DOWN), ShareWindow::SHAREWINDOW_COMMAND_SET_DOWNLOAD_LIMIT, "num", kLimits, nL, s.GetInt32("downloads",2), _target))
		.Add(NewValueField(str(STR_ST_MAX_DOWN_USER), ShareWindow::SHAREWINDOW_COMMAND_SET_DOWNLOAD_PER_USER_LIMIT, "num", kLimits, nL, s.GetInt32("downloadsperuser",1), _target))
		.AddStrut(6)
		.Add(NewCheck(str(STR_ST_SHARE_FILES), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FILE_SHARING_ENABLED, s.GetBool("sharingenabled",true), _target))
		.Add(NewCheck(str(STR_ST_SHORTEST_FIRST), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_SHORTEST_UPLOADS_FIRST, s.GetBool("shortestfirst"), _target))
		.Add(NewCheck(str(STR_ST_AUTOCLEAR), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTOCLEAR_COMPLETED_DOWNLOADS, s.GetBool("autoclear"), _target))
		.Add(NewCheck(str(STR_ST_RETAIN_PATHS), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_RETAIN_FILE_PATHS, s.GetBool("retainpaths"), _target))
		.AddGlue()
		.SetInsets(0);
	return Card(str(STR_ST_CARD_TRANSFERS), v);
}

BView *
ShareSettingsWindow::_MakeInterfaceCard(const BMessage & s)
{
	static const Opt kPages[] = {{"500",500},{"1000",1000},{"2000",2000},{"3000",3000},{"5000",5000},{"8000",8000},{"10000",10000},{"100000",100000}};

	BButton * colors = new BButton("colors", str(STR_ST_COLORS_BTN), new BMessage(ShareWindow::SHAREWINDOW_COMMAND_SHOW_COLOR_PICKER));
	colors->SetTarget(_target);

	BView * v = new BView("ui", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck(str(STR_ST_SHOW_NOTIF), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_NOTIFICATIONS, s.GetBool("notifications",true), _target))
		.Add(NewHint(str(STR_ST_NOTIF_HINT)))
		.AddStrut(4)
		.Add(NewValueField(str(STR_ST_PAGE_SIZE), ShareWindow::SHAREWINDOW_COMMAND_SET_PAGE_SIZE, "pagesize", kPages, sizeof(kPages)/sizeof(kPages[0]), s.GetInt32("pagesize",1000), _target))
		.AddStrut(4)
		.AddGroup(B_HORIZONTAL)
			.Add(NewCheck(str(STR_ST_CUSTOM_COLORS), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_CUSTOM_COLORS, s.GetBool("customcolors", true), _target))
			.AddGlue()
			.Add(colors)
		.End()
		.Add(NewHint(str(STR_ST_COLORS_HINT)))
		.AddGlue()
		.SetInsets(0);
	return Card(str(STR_ST_INTERFACE), v);
}

BView *
ShareSettingsWindow::_MakeChatCard(const BMessage & s)
{
	const Opt kAway[] = {{str(STR_ST_NEVER),0},{"5 min",5},{"10 min",10},{"15 min",15},{"30 min",30},{"60 min",60}};
	const Opt kComp[] = {{str(STR_ST_NONE),0},{str(STR_ST_LOW),1},{str(STR_ST_MEDIUM),6},{str(STR_ST_MAXIMUM),9}};

	BView * v = new BView("chat", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck(str(STR_ST_FULL_QUERIES), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FULL_USER_QUERIES, s.GetBool("fulluserqueries"), _target))
		.Add(NewCheck(str(STR_ST_LOG_CHAT), ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FILE_LOGGING, s.GetBool("logging"), _target))
		.AddStrut(4)
		.Add(NewValueField(str(STR_ST_AWAY_AFTER), ShareWindow::SHAREWINDOW_COMMAND_SET_AUTO_AWAY, "autoaway", kAway, sizeof(kAway)/sizeof(kAway[0]), s.GetInt32("autoaway",0), _target))
		.Add(NewValueField(str(STR_ST_COMPRESSION), ShareWindow::SHAREWINDOW_COMMAND_SET_COMPRESSION_LEVEL, "complevel", kComp, sizeof(kComp)/sizeof(kComp[0]), s.GetInt32("complevel",0), _target))
		.AddGlue()
		.SetInsets(0);
	return Card(str(STR_ST_CHAT), v);
}

};  // namespace beshare
