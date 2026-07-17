/*  PirateDemo — an old-school demoscene "intro" easter egg for HiShare.
 *  Triggered by clicking the flower icon in the header banner 10 times.
 *  Sine-wave scroller + copper bars + a synthesized 8-bit pirate chiptune,
 *  thanking all the pirates who kept this abandoned code alive.
 */
#ifndef PirateDemo_h
#define PirateDemo_h

#include <Window.h>

class BSoundPlayer;
class BMessageRunner;

namespace beshare {

class DemoView;

class PirateDemoWindow : public BWindow
{
public:
   PirateDemoWindow();
   virtual ~PirateDemoWindow();

   virtual bool QuitRequested();

private:
   DemoView *       _view;
   BSoundPlayer *   _player;
   BMessageRunner * _runner;
   void *           _chip;   // ChipState* (opaque here)
};

};  // namespace beshare

#endif
