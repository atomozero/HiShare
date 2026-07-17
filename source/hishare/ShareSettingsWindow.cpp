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
{
	SetLayout(new BGroupLayout(B_HORIZONTAL));

	// left: category list
	_categories = new BListView("categories", B_SINGLE_SELECTION_LIST);
	_categories->AddItem(new BStringItem("Network"));
	_categories->AddItem(new BStringItem("Transfers"));
	_categories->AddItem(new BStringItem("Interface"));
	_categories->AddItem(new BStringItem("Chat"));
	_categories->SetSelectionMessage(new BMessage(MSG_CATEGORY));
	_categories->SetTarget(this);
	BScrollView * catScroll = new BScrollView("catScroll", _categories, 0, false, false);
	catScroll->SetExplicitMaxSize(BSize(150, B_SIZE_UNLIMITED));
	catScroll->SetExplicitMinSize(BSize(130, 0));

	// right: card layout, one card per category
	BView * cardHost = new BView("cardHost", 0);
	_cards = new BCardLayout();
	cardHost->SetLayout(_cards);
	_cards->AddView(_MakeNetworkCard(state));
	_cards->AddView(_MakeTransfersCard(state));
	_cards->AddView(_MakeInterfaceCard(state));
	_cards->AddView(_MakeChatCard(state));

	BButton * close = new BButton("close", "Close", new BMessage(MSG_CLOSE));

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

		default:
			BWindow::MessageReceived(msg);
		break;
	}
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
	if (reach == 1)      snprintf(statusText, sizeof(statusText), "Reachable: %s:%ld is open to the internet.", ip, (long)extport);
	else if (reach == 0) snprintf(statusText, sizeof(statusText), "Not reachable from the internet (NAT). Public IP %s.", ip[0]?ip:"unknown");
	else                 snprintf(statusText, sizeof(statusText), "Reachability not tested yet.");

	BButton * testBtn = new BButton("test", "Test now", new BMessage(ShareWindow::SHAREWINDOW_COMMAND_TEST_REACHABILITY));
	testBtn->SetTarget(_target);

	BView * v = new BView("net", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck("Open the router port automatically (UPnP / NAT-PMP / PCP)", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTO_PORT_FORWARD, apf, _target))
		.Add(NewHint("Makes you reachable from outside your home NAT without manual configuration."))
		.AddStrut(4)
		.AddGroup(B_HORIZONTAL)
			.Add(new BStringView("st", statusText))
			.AddGlue()
			.Add(testBtn)
		.End()
		.Add(NewCheck("I'm behind a firewall", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FIREWALLED, fw, _target))
		.AddStrut(6)
		.Add(NewSectionLabel("PRIVACY & ACCOUNT"))
#if BESHARE_TLS_ENABLED
		// Hidden for HiShare 1.0 — TLS downloads crash (see ShareConstants.h / beshare-tls-ssl).
		.Add(NewCheck("Encrypt file transfers (TLS)", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_REQUIRE_TLS, tls, _target))
		.Add(NewHint("Opportunistic encryption with peers that support it; plaintext with older clients."))
#endif
		.Add(NewCheck("Connect on startup", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_LOGIN_ON_STARTUP, lo, _target))
		.Add(NewCheck("Auto-update the server list", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTOUPDATE_SERVER_LIST, aus, _target))
		.AddGlue()
		.SetInsets(0);
	return Card("Network & reachability", v);
}

BView *
ShareSettingsWindow::_MakeTransfersCard(const BMessage & s)
{
	static const Opt kLimits[] = {{"1",1},{"2",2},{"3",3},{"4",4},{"5",5},{"6",6},{"8",8},{"10",10},{"16",16},{"32",32},{"Unlimited",1000000}};
	int nL = sizeof(kLimits)/sizeof(kLimits[0]);

	BView * v = new BView("xfer", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewValueField("Max simultaneous uploads:", ShareWindow::SHAREWINDOW_COMMAND_SET_UPLOAD_LIMIT, "num", kLimits, nL, s.GetInt32("uploads",3), _target))
		.Add(NewValueField("Max uploads per user:", ShareWindow::SHAREWINDOW_COMMAND_SET_UPLOAD_PER_USER_LIMIT, "num", kLimits, nL, s.GetInt32("uploadsperuser",1), _target))
		.Add(NewValueField("Max simultaneous downloads:", ShareWindow::SHAREWINDOW_COMMAND_SET_DOWNLOAD_LIMIT, "num", kLimits, nL, s.GetInt32("downloads",2), _target))
		.Add(NewValueField("Max downloads per user:", ShareWindow::SHAREWINDOW_COMMAND_SET_DOWNLOAD_PER_USER_LIMIT, "num", kLimits, nL, s.GetInt32("downloadsperuser",1), _target))
		.AddStrut(6)
		.Add(NewCheck("Share files with others", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FILE_SHARING_ENABLED, s.GetBool("sharingenabled",true), _target))
		.Add(NewCheck("Serve shortest uploads first", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_SHORTEST_UPLOADS_FIRST, s.GetBool("shortestfirst"), _target))
		.Add(NewCheck("Auto-clear completed downloads", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_AUTOCLEAR_COMPLETED_DOWNLOADS, s.GetBool("autoclear"), _target))
		.Add(NewCheck("Remember original file paths", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_RETAIN_FILE_PATHS, s.GetBool("retainpaths"), _target))
		.AddGlue()
		.SetInsets(0);
	return Card("Transfers & sharing", v);
}

BView *
ShareSettingsWindow::_MakeInterfaceCard(const BMessage & s)
{
	static const Opt kPages[] = {{"500",500},{"1000",1000},{"2000",2000},{"3000",3000},{"5000",5000},{"8000",8000},{"10000",10000},{"100000",100000}};

	BButton * colors = new BButton("colors", "Colours\xE2\x80\xA6", new BMessage(ShareWindow::SHAREWINDOW_COMMAND_SHOW_COLOR_PICKER));
	colors->SetTarget(_target);

	BView * v = new BView("ui", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck("Show desktop notifications", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_NOTIFICATIONS, s.GetBool("notifications",true), _target))
		.Add(NewHint("Finished downloads, private messages and name mentions."))
		.AddStrut(4)
		.Add(NewValueField("Query results per page:", ShareWindow::SHAREWINDOW_COMMAND_SET_PAGE_SIZE, "pagesize", kPages, sizeof(kPages)/sizeof(kPages[0]), s.GetInt32("pagesize",1000), _target))
		.AddStrut(4)
		.AddGroup(B_HORIZONTAL)
			.Add(NewCheck("Use custom colours", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_CUSTOM_COLORS, s.GetBool("customcolors", true), _target))
			.AddGlue()
			.Add(colors)
		.End()
		.Add(NewHint("When off, windows follow the Haiku system theme. Picking a colour turns this back on."))
		.AddGlue()
		.SetInsets(0);
	return Card("Interface", v);
}

BView *
ShareSettingsWindow::_MakeChatCard(const BMessage & s)
{
	static const Opt kAway[] = {{"Never",0},{"5 min",5},{"10 min",10},{"15 min",15},{"30 min",30},{"60 min",60}};
	static const Opt kComp[] = {{"None",0},{"Low",1},{"Medium",6},{"Maximum",9}};

	BView * v = new BView("chat", 0);
	BLayoutBuilder::Group<>(v, B_VERTICAL, B_USE_SMALL_SPACING)
		.Add(NewCheck("Show full user queries", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FULL_USER_QUERIES, s.GetBool("fulluserqueries"), _target))
		.Add(NewCheck("Log chat to a file", ShareWindow::SHAREWINDOW_COMMAND_TOGGLE_FILE_LOGGING, s.GetBool("logging"), _target))
		.AddStrut(4)
		.Add(NewValueField("Mark me away after:", ShareWindow::SHAREWINDOW_COMMAND_SET_AUTO_AWAY, "autoaway", kAway, sizeof(kAway)/sizeof(kAway[0]), s.GetInt32("autoaway",0), _target))
		.Add(NewValueField("Data compression:", ShareWindow::SHAREWINDOW_COMMAND_SET_COMPRESSION_LEVEL, "complevel", kComp, sizeof(kComp)/sizeof(kComp[0]), s.GetInt32("complevel",0), _target))
		.AddGlue()
		.SetInsets(0);
	return Card("Chat", v);
}

};  // namespace beshare
