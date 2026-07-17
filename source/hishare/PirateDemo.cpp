/*  PirateDemo.cpp — the HiShare pirate intro (easter egg).
 *  Sine scroller + copper bars, with an 8-bit pirate chiptune whose waveform is
 *  synthesized in hand-written x86-64 assembly (pirate_asm.S).
 */
#include "PirateDemo.h"

#include <Bitmap.h>
#include <Font.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Screen.h>
#include <SoundPlayer.h>
#include <View.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

// The chiptune waveform generator, hand-written in assembly.
extern "C" void pirate_square_fill(int16_t * out, uint32_t frames,
                                   uint32_t * phase, uint32_t phaseInc, int16_t amp);

namespace beshare {

static const uint32_t kTickWhat = 'tick';

// ---- 8-bit tune (a Drunken-Sailor-ish sea-shanty loop) ---------------------
// Note frequencies (Hz); 0 == rest.
#define R  0
#define D4 294
#define E4 330
#define F4 349
#define G4 392
#define A4 440
#define Bb 466
#define C5 523
#define D5 587
struct Note { uint16_t hz; uint16_t ms; };
static const Note kTune[] = {
   {D4,170},{D4,170},{D4,170},{D4,170},{A4,170},{A4,340},{R,110},
   {A4,170},{G4,170},{F4,170},{E4,170},{D4,340},{R,110},
   {F4,170},{F4,170},{F4,170},{F4,170},{A4,170},{A4,340},{R,110},
   {A4,170},{G4,170},{F4,170},{E4,170},{D4,340},{R,200},
   {A4,170},{A4,170},{C5,170},{A4,170},{G4,170},{F4,170},{E4,340},{R,110},
   {D4,170},{D4,170},{F4,170},{A4,170},{D5,340},{A4,340},{R,220},
};
static const int kTuneLen = (int)(sizeof(kTune)/sizeof(kTune[0]));

struct ChipState {
   uint32_t phase;       // 32-bit phase accumulator (shared with the asm)
   int32_t  noteIndex;
   int32_t  samplesLeft; // samples remaining in the current note
   uint32_t phaseInc;    // per-sample phase increment for the current note
   int16_t  amp;         // current amplitude (with a simple per-note decay)
   int32_t  noteSamples; // total samples in the current note (for the envelope)
   float    frameRate;
   bool     muted;
};

static void StartNote(ChipState * s, int idx)
{
   s->noteIndex   = idx % kTuneLen;
   const Note & n = kTune[s->noteIndex];
   s->noteSamples = (int32_t)(n.ms * s->frameRate / 1000.0f);
   s->samplesLeft = s->noteSamples;
   s->phase       = 0;
   s->phaseInc    = (n.hz == 0) ? 0
                  : (uint32_t)(((double)n.hz / s->frameRate) * 4294967296.0);
}

// BSoundPlayer callback: fill the buffer, one note-run at a time, via the asm.
static void FillBuffer(void * cookie, void * buffer, size_t size,
                       const media_raw_audio_format &)
{
   ChipState * s = (ChipState *)cookie;
   int16_t * out = (int16_t *)buffer;
   size_t    frames = size / sizeof(int16_t);   // mono, 16-bit

   while (frames > 0)
   {
      if (s->samplesLeft <= 0) StartNote(s, s->noteIndex + 1);

      uint32_t run = (uint32_t)((frames < (size_t)s->samplesLeft) ? frames : s->samplesLeft);

      // Simple linear decay envelope so notes have a plucky chiptune attack/tail.
      int done = s->noteSamples - s->samplesLeft;
      float envF = 1.0f - (0.55f * ((float)done / (float)(s->noteSamples ? s->noteSamples : 1)));
      int16_t amp = (s->phaseInc == 0 || s->muted) ? 0 : (int16_t)(5200.0f * envF);

      pirate_square_fill(out, run, &s->phase, s->phaseInc, amp);

      out         += run;
      frames      -= run;
      s->samplesLeft -= (int32_t)run;
   }
}

// ---------------------------------------------------------------------------

static const char * kScrollText =
   "  \xE2\x98\xA0  H I S H A R E  \xE2\x98\xA0     "
   "THIS OLD CODE WAS NOT CRACKED - IT WAS BROUGHT BACK TO LIFE !!!     "
   "BESHARE 3.04, ABANDONED FOR MORE THAN 10 YEARS, SAILS AGAIN AS HISHARE 1.0     "
   "*** THE PIRATES OF HAIKU WILL CONQUER ALL SEVEN SEAS ! ***     "
   "GREETINGS TO EVERYONE KEEPING THE BEOS / HAIKU SPIRIT ALIVE  -  "
   "( the chiptune is synthesized in hand-written x86-64 assembly, of course )     "
   "\xE2\x98\xA0  HOIST THE COLOURS  -  SET SAIL  -  ARGH !  \xE2\x98\xA0        ";

static rgb_color Rainbow(float t)
{
   const float k = 6.28318f;
   rgb_color c;
   c.red   = (uint8)((sinf(t * k)          * 0.5f + 0.5f) * 255);
   c.green = (uint8)((sinf(t * k + 2.094f) * 0.5f + 0.5f) * 255);
   c.blue  = (uint8)((sinf(t * k + 4.188f) * 0.5f + 0.5f) * 255);
   c.alpha = 255;
   return c;
}

class DemoView : public BView
{
public:
   DemoView(BRect frame)
      : BView(frame, "demo", B_FOLLOW_ALL_SIDES, B_WILL_DRAW)
      , _buf(NULL), _off(NULL), _tick(0), _scrollX(0.0f), _textW(0.0f)
   {
      SetViewColor(B_TRANSPARENT_COLOR);
      _scrollFont = BFont(be_bold_font); _scrollFont.SetSize(46.0f);
      _titleFont  = BFont(be_bold_font); _titleFont.SetSize(30.0f);
      _len = (int)strlen(kScrollText);
   }

   virtual ~DemoView() { delete _buf; }

   virtual void AttachedToWindow()
   {
      BRect b = Bounds();
      _buf = new BBitmap(b, B_RGB32, true);
      _off = new BView(b, "off", B_FOLLOW_ALL_SIDES, B_WILL_DRAW);
      _buf->AddChild(_off);
      _scrollX = b.right;
      // precompute per-character x offsets in the scroll font
      if (_buf->Lock()) { _off->SetFont(&_scrollFont); _buf->Unlock(); }
      float x = 0;
      for (int i = 0; i < _len; i++) { _cum[i] = x; char c[2] = { kScrollText[i], 0 }; x += _scrollFont.StringWidth(c); }
      _textW = x;
   }

   void Advance()
   {
      _tick++;
      _scrollX -= 7.0f;
      if (_scrollX < -_textW) _scrollX = Bounds().right;
      Render();
      Invalidate();
   }

   virtual void Draw(BRect)
   {
      if (_buf) DrawBitmap(_buf, Bounds(), Bounds());
   }

   virtual void MouseDown(BPoint) { if (Window()) Window()->PostMessage(B_QUIT_REQUESTED); }
   virtual void KeyDown(const char *, int32) { if (Window()) Window()->PostMessage(B_QUIT_REQUESTED); }
   virtual void MessageReceived(BMessage * m)
   {
      if (m->what == kTickWhat) Advance(); else BView::MessageReceived(m);
   }

private:
   void Render()
   {
      if (!_buf || !_buf->Lock()) return;
      BRect b = _off->Bounds();
      const float w = b.Width(), h = b.Height();
      const float midY = h * 0.5f;
      const float ph = _tick * 0.06f;

      // background
      _off->SetHighColor(6, 6, 20);
      _off->FillRect(b);

      // copper bars, top and bottom
      _off->SetPenSize(1.0f);
      for (float y = 0; y < 46; y++)
      {
         float f = 1.0f - fabsf((y - 23) / 23.0f);
         rgb_color c = Rainbow(y * 0.02f + ph);
         c.red = (uint8)(c.red * f); c.green = (uint8)(c.green * f); c.blue = (uint8)(c.blue * f);
         _off->SetHighColor(c);
         _off->StrokeLine(BPoint(0, y), BPoint(w, y));
         _off->StrokeLine(BPoint(0, h - y), BPoint(w, h - y));
      }

      // pulsing title
      _off->SetFont(&_titleFont);
      _off->SetDrawingMode(B_OP_OVER);
      const char * title = "\xE2\x98\xA0  the Haiku pirates ride  \xE2\x98\xA0";
      float tw = _titleFont.StringWidth(title);
      _off->SetHighColor(Rainbow(ph * 0.5f));
      _off->DrawString(title, BPoint((w - tw) / 2.0f, 78));

      // sine-wave scroller
      _off->SetFont(&_scrollFont);
      font_height fh; _scrollFont.GetHeight(&fh);
      for (int i = 0; i < _len; i++)
      {
         float cx = _scrollX + _cum[i];
         if (cx > w) break;
         if (cx < -60) continue;
         float y = midY + sinf(cx * 0.012f + ph * 1.3f) * (h * 0.16f) + fh.ascent * 0.5f;
         _off->SetHighColor(Rainbow((float)i * 0.03f + ph * 0.4f));
         char c[2] = { kScrollText[i], 0 };
         _off->DrawString(c, BPoint(cx, y));
      }

      // static footer
      _off->SetFont(be_plain_font);
      _off->SetHighColor(180, 190, 210);
      const char * foot = "click or press any key to close  -  argh! argh! argh!";
      _off->DrawString(foot, BPoint((w - be_plain_font->StringWidth(foot)) / 2.0f, h - 60));

      _off->Sync();
      _buf->Unlock();
   }

   BBitmap * _buf;
   BView   * _off;
   BFont     _scrollFont, _titleFont;
   int64     _tick;
   float     _scrollX, _textW;
   int       _len;
   float     _cum[1024];
};

// ---------------------------------------------------------------------------

PirateDemoWindow::PirateDemoWindow()
   : BWindow(BRect(0, 0, 639, 399), "HiShare \xE2\x98\xA0 the Haiku pirates ride",
             B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_ASYNCHRONOUS_CONTROLS)
   , _view(NULL), _player(NULL), _chip(NULL)
{
   _view = new DemoView(Bounds());
   AddChild(_view);
   CenterOnScreen();

   // Set up the chiptune player (waveform synthesized by the asm routine).
   media_raw_audio_format fmt = media_raw_audio_format::wildcard;
   fmt.frame_rate    = 44100;
   fmt.channel_count = 1;
   fmt.format        = media_raw_audio_format::B_AUDIO_SHORT;
   fmt.byte_order    = B_MEDIA_HOST_ENDIAN;
   fmt.buffer_size   = 2048;

   ChipState * chip = new ChipState;
   memset(chip, 0, sizeof(ChipState));
   chip->frameRate = fmt.frame_rate;
   StartNote(chip, 0);
   _chip = chip;

   _player = new BSoundPlayer(&fmt, "HiShare chiptune", &FillBuffer, NULL, chip);
   if (_player->InitCheck() == B_OK) { _player->SetVolume(0.55f); _player->Start(); _player->SetHasData(true); }

   // ~30 fps animation ticks.
   _runner = new BMessageRunner(BMessenger(_view), new BMessage(kTickWhat), 33000);
}

PirateDemoWindow::~PirateDemoWindow()
{
   delete _runner;
   if (_player) { _player->Stop(); delete _player; }
   delete (ChipState *)_chip;
}

bool PirateDemoWindow::QuitRequested()
{
   if (_player) { _player->Stop(); delete _player; _player = NULL; }
   return true;
}

};  // namespace beshare
