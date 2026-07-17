// RemoteInfo.cpp
// -- display information on remote file
// Pete G. 2015/6/23

#include <Window.h>
#include <View.h>
#include <Font.h>
#include <TextView.h>
#include <ScrollBar.h>
#include <ScrollView.h>

#include "RemoteFileItem.h"
#include "RemoteUserItem.h"
#include "ShareWindow.h"
#include "ShareStrings.h"
#include "RemoteInfo.h"

//#define DEBUG true

#define ZDPRINTF(x)
#ifdef DEBUG
 #include <stdio.h>
 #define DPRINTF(x) printf x
#else
 #define DPRINTF(x)
#endif

namespace beshare {

struct InfoView : public BTextView {
	InfoView(BRect frame) : BTextView(frame, "Info View", frame.OffsetToCopy(0, 0),
		B_FOLLOW_ALL_SIDES, B_WILL_DRAW) {}
	void FrameResized(float width, float height);
};


void InfoView::FrameResized(float width, float height) {
	BTextView::FrameResized(width, height);
	SetTextRect(BRect(0, 0, width, height));
} 


struct InfoPanel : public BWindow {
	InfoPanel(BRect frame);
	InfoView * text; 
	BFont font, boldfont;
	uint32 fontprops;
};


InfoPanel::InfoPanel(BRect frame)
				: BWindow(frame, "BeShare - Remote File Info", B_DOCUMENT_WINDOW, 0)
{
	frame.InsetBy(B_V_SCROLL_BAR_WIDTH/2, B_H_SCROLL_BAR_HEIGHT/2);
	frame.OffsetTo(0, 0);
	text = new InfoView(frame);
	text->MakeEditable(false);
	text->SetStylable(true);
	text->GetFontAndColor(&font, &fontprops);
	boldfont = font;
	boldfont.SetSize(font.Size()*1.2);
	boldfont.SetFace(B_BOLD_FACE);
	BScrollView * scroller = new BScrollView("scroller", text,
							B_FOLLOW_ALL_SIDES,
							0, true, true);
	AddChild(scroller);
}


void 
RemoteInfo ::
ShowInfo(const BMessage & filelistMsg)
{
	// TEMP?:
	InfoPanel *panel = new InfoPanel(BRect(100, 50, 500, 400));
	panel->Show();
	
	panel->Lock();
	RemoteFileItem * item;
	for (int32 i=0;
		(filelistMsg.FindPointer("item", i, (void **)&item) == B_NO_ERROR);
		i++)
	{
		const void *infodata;
		uint32 infosize;
		if (item->GetAttributes().FindData("beshare:Info", 'CSTR',
			&infodata, &infosize) == B_NO_ERROR) {
			DPRINTF(("Info for %s:\n%s\n", item->GetFileName(), (char *)infodata);)
			panel->text->SetFontAndColor(&panel->boldfont);
			panel->text->Insert(item->GetFileName());
			panel->text->Insert(":\n");
			panel->text->SetFontAndColor(&panel->font);
			panel->text->Insert((char *)infodata);
			panel->text->Insert("\n\n");
		}
		else {
			DPRINTF(("No Info for %s:\n", item->GetFileName());)
			panel->text->SetFontAndColor(&panel->boldfont);
			panel->text->Insert("No info for ");
			panel->text->Insert(item->GetFileName());
			panel->text->Insert("...\n\n");
		}
	}
	panel->Unlock();
}

};  // end namespace beshare
