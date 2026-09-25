#include "Gfx.h"
#include "Touch.h"
#include "commandmessenger.h"

#include <cstdint>

HostCmdMessenger cmdMessenger;

// The preview drives time explicitly, so a blink phase or a settle delay renders the same
// way every run. main.cpp sets it before each frame.
static uint32_t s_millis = 0;
uint32_t        millis() { return s_millis; }
void            hostSetMillis(uint32_t ms) { s_millis = ms; }

/* Touch does nothing here: there is no finger, and the calibration UI belongs to the
   resistive board. The interface is Touch.h's, unchanged -- the same shape a capacitive
   driver will implement for the 480x320 board. */
namespace Touch
{
// A synthetic finger, so a preview can show a button held down. main.cpp places it; the
// press then travels the real path -- TouchZones resolves the zone and asks the panel for
// its feedback -- rather than the renderer being called with a pressed flag directly.
static int16_t s_x = 0, s_y = 0;
static bool    s_down = false, s_reported = false;

void  begin() {}
void  setRotation(uint8_t) {}
Event poll(int16_t &x, int16_t &y)
{
    if (s_down && !s_reported) {
        s_reported = true;
        x = s_x;
        y = s_y;
        return DOWN;
    }
    return NONE;
}
void  startCalibration() {}
bool  calibrating() { return false; }
bool  stepCalibration() { return true; }
} // namespace Touch

void hostSetTouch(int16_t x, int16_t y)
{
    Touch::s_x        = x;
    Touch::s_y        = y;
    Touch::s_down     = true;
    Touch::s_reported = false;
}

/* The host "backend": the screen is just the canvas main.cpp writes out, so bringing it up
   is sizing it, rotation is a no-op (it is allocated landscape already), and there is
   nothing to present to. */
namespace Gfx
{
void deviceBegin(Device &device) { device.init(); }
void deviceRotate(Device &, uint8_t) {}
void present(Device &) {}
} // namespace Gfx
