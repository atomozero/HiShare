#include <interface/Font.h>
#include <interface/Region.h>
#include "RemoteFileItem.h"
#include "RemoteUserItem.h"
#include "ShareWindow.h"
#include "ShareStrings.h"
#include "Colors.h"
#include "ColumnListView.h"

#ifdef __HAIKU__
 #include <interface/IconUtils.h>
#endif

namespace beshare {

RemoteFileItem ::
RemoteFileItem(RemoteUserItem * owner, const char * fileName, const MessageRef & attrs)
  : CLVListItem(0, false, false, 18.0f), _owner(owner), _fileName(fileName),
   _attributes(attrs), _iconp(NULL)
{
#ifdef DEBUG
   printf("\nRemoteFileItem %s, attr:\n", fileName);
   attrs()->PrintToStream();
#endif
}

RemoteFileItem ::
~RemoteFileItem()
{
   delete _iconp;
}

void RemoteFileItem::
DrawItemColumn(BView * clv, BRect itemRect, int32 colIdx, bool complete)
{
   bool selected = IsSelected();
   // Zebra parity from the item's on-screen row (top / row-height) — robust for all
   // lists (FullListIndexOf can return -1 for these paged result items).
   int32 rowIndex = (itemRect.Height() > 0.0f) ? (int32)((itemRect.top / itemRect.Height()) + 0.5f) : 0;
   rgb_color color = (selected) ? ((ColumnListView*)clv)->ItemSelectColor() : ((ColumnListView *)clv)->RowColor(rowIndex);
   clv->SetLowColor(color);
   // Always paint the row background so the zebra-stripe tint shows even on
   // partial (non-"complete") column redraws.
   (void) complete;
   clv->SetHighColor(color);
   clv->FillRect(itemRect);

   if (colIdx > 0)
   {
      BRegion Region;
      Region.Include(itemRect);
      clv->ConstrainClippingRegion(&Region);
      clv->SetHighColor(((ColumnListView *)clv)->TextColor());
      const char* text = _owner->GetOwner()->GetFileCellText(this, colIdx);
      if (text) clv->DrawString(text, BPoint(itemRect.left+2.0,itemRect.top+_textOffset));
      clv->ConstrainClippingRegion(NULL);
   }
   else if (colIdx == 0)
   {
      const BBitmap * bmp = _owner->GetOwner()->GetBitmap(this, colIdx);
      if (bmp)
      {
         clv->SetDrawingMode(B_OP_OVER);
         clv->DrawBitmap(bmp, BPoint(itemRect.left + ((itemRect.Width()-bmp->Bounds().Width())/2.0f), itemRect.top+((itemRect.Height()-bmp->Bounds().Height())/2.0f)));
      }
   }
}


void RemoteFileItem::
Update(BView *owner, const BFont *font)
{
   CLVListItem::Update(owner, font);

   font_height fontAttrs;
   font->GetHeight(&fontAttrs);
   _textOffset = ceil(fontAttrs.ascent) + (Height()-(ceil(fontAttrs.ascent) + ceil(fontAttrs.descent)))/2.0;
}


int
RemoteFileItem ::
Compare(const RemoteFileItem * item2, int32 key) const
{
   return _owner->GetOwner()->Compare(this, item2, key);
}

const char *
RemoteFileItem ::
GetPath() const
{
   const char * ret;
   return (_attributes.GetItemPointer()->FindString("beshare:Path", &ret) == B_NO_ERROR) ? ret : "";
}

const BBitmap *
RemoteFileItem ::
GetIcon()
{
   if (_iconp) return _iconp;
   const void *icondata;
   uint32 iconsize;
#ifdef __HAIKU__
   if (GetAttributes().FindData("besharez:Vector Icon", 'VICN', &icondata, &iconsize) == B_NO_ERROR)
   {
      _iconp = new BBitmap(BRect(0,0,15,15), B_RGBA32);
      if (_iconp) BIconUtils::GetVectorIcon((const uint8 *)icondata, iconsize, _iconp);
   }
   else
#endif
   if (GetAttributes().FindData("besharez:Mini Icon", 'MICN', &icondata, &iconsize) == B_NO_ERROR)
   {
      if (iconsize != 256) return NULL;
      _iconp = new BBitmap(BRect(0,0,15,15), B_COLOR_8_BIT);
      if (_iconp) memcpy(_iconp->Bits(), icondata, 256);
   }
   return _iconp;
}

const char *
RemoteFileItem ::
GetInfo() const
{
   const char * ret;
   return (_attributes.GetItemPointer()->FindString("beshare:Info", &ret) == B_NO_ERROR) ? ret : "";
}

};  // end namespace beshare
