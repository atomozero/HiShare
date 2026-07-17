#ifndef SHARE_UTILS_H
#define SHARE_UTILS_H

#include <time.h>
#include <support/SupportDefs.h>
#include <interface/GraphicsDefs.h>
#include "BeShareNameSpace.h"
#include "util/String.h"

namespace beshare {

// Given a byte count returns it in human-friends format into (outBuf).
// (outBuf) should be at least 32 chars long.
void GetByteSizeString(int64 v, char * outBuf);

// Given a time_t, returns a string representation of that date/time.
void GetTimeString(time_t when, char * buf);

// Saves the given color into the given BMessage with the given field name
status_t SaveColorToMessage(const char * fn, const rgb_color & col, BMessage & msg);

// Restores the given color from the given BMessage with the given field name.
status_t RestoreColorFromMessage(const char * fn, rgb_color & retCol, const BMessage & msg, uint32 which = 0);

// Returns the given string, with all the labelled URLs substituted out into just their labels
String SubstituteLabelledURLs(const String & shortName);

// Returns true iff (str) is recognized as having a hyperlink prefix
bool IsLink(const char * str);

// --- Desktop notifications (Haiku BNotification) ---------------------------
// Enable/disable BeShare's use of native desktop notifications (persisted
// preference, applied globally).
void SetNotificationsEnabled(bool enabled);
bool GetNotificationsEnabled();

// Post a native desktop notification, if notifications are enabled.  (group)
// keeps related notifications together in the notification center; (title) and
// (content) are the visible text.  Safe to call from any window/thread.
void ShowNotification(const char * group, const char * title, const char * content);

};  // end namespace beshare

#endif
