#include "TouchZones.h"
#include "PanelGfx.h"
#include "Touch.h"
#include "Theme.h"
#include "commandmessenger.h"
#include "config.h" // getBoardReady()
#include <string.h>

namespace TouchZones
{

static const int16_t  RING_INSET  = 2;    // press ring, px outside the zone
static const uint32_t RING_MIN_MS = 180;  // a quick tap still shows it
static const int16_t  HIT_SLOP    = 2;    // a press this close outside a zone still counts
static const uint32_t CAL_HOLD_MS = 6000; // hold anywhere off the zones to recalibrate

static const Zone  *s_zones     = nullptr;
static uint8_t      s_count     = 0;
static const Hooks *s_hooks     = nullptr;
static bool        s_ready     = false;
static int8_t      s_pressed   = -1; // zone held down
static int8_t      s_ringZone  = -1; // zone whose ring is showing
static uint32_t    s_ringUntil = 0;
static bool        s_holdAway  = false; // a press that landed on no zone
static uint32_t    s_holdSince = 0;

static bool        s_demo      = false; // see setDemo()
static bool        s_fingerDown = false;
static uint32_t    s_downSince = 0;
static bool        s_scripted  = false; // a scripted press is showing

static void sendButton(const char *name, uint8_t event)
{
    // A demo must never open a door or press a key in the sim.
    if (s_demo || !getBoardReady()) return;
    cmdMessenger.sendCmdStart(kButtonChange);
    cmdMessenger.sendCmdArg(name);
    cmdMessenger.sendCmdArg(event); // 0 press, 1 release: MobiFlight's btnOnPress / btnOnRelease
    cmdMessenger.sendCmdEnd();
}

static int8_t hit(int16_t x, int16_t y)
{
    if (s_hooks && s_hooks->resolve) {
        const int8_t z = s_hooks->resolve(x, y);
        if (z >= 0 && z < (int8_t)s_count) return z;
    }
    for (uint8_t i = 0; i < s_count; ++i) {
        const Zone &z = s_zones[i];
        if (x >= z.x - HIT_SLOP && x < z.x + z.w + HIT_SLOP &&
            y >= z.y - HIT_SLOP && y < z.y + z.h + HIT_SLOP)
            return (int8_t)i;
    }
    return -1;
}

static void ring(int8_t zone, bool on)
{
    if (zone < 0) return;
    s_ringZone = on ? zone : -1;
    if (s_hooks && s_hooks->feedback && s_hooks->feedback(zone, on)) return;
    const Zone &z = s_zones[zone];
    PanelGfx::drawRing(z.x, z.y, z.w, z.h, RING_INSET, on ? Theme::PressRing : Theme::PanelBg);
}

static void letGo()
{
    if (s_pressed >= 0 && s_zones) sendButton(s_zones[s_pressed].name, 1);
    s_pressed  = -1;
    s_holdAway = false;
}

static void ensureTouch()
{
    if (s_ready) return;
    Touch::begin();
    s_ready = true;
}

void attach(const Zone *zones, uint8_t count, const Hooks *hooks)
{
    letGo();
    s_zones    = zones;
    s_count    = zones ? count : 0;
    s_hooks    = hooks;
    s_ringZone = -1;
    s_scripted = false;
    if (s_count) ensureTouch();
}

void detach()
{
    letGo();
    s_scripted = false;
    s_zones = nullptr;
    s_count = 0;
    s_hooks = nullptr;
}

void screenCleared() { s_ringZone = -1; }

void startCalibration()
{
    if (s_demo) return;
    letGo();
    Touch::startCalibration();
}

bool poll(bool &redraw)
{
    redraw = false;
    if (s_demo || !s_count) return false;

    if (Touch::calibrating()) {
        if (Touch::stepCalibration()) {
            s_ringZone = -1;
            redraw     = true; // done: the panel puts itself back
        }
        return !redraw;
    }

    const uint32_t     now = millis();
    int16_t            x, y;
    const Touch::Event e = Touch::poll(x, y);

    // Presses count only while the panel is lit: dark, there is nothing to aim at, and
    // MobiFlight is not running anyway. Going dark mid-press releases the button.
    if (!PanelGfx::isLit()) {
        letGo();
        if (s_ringZone >= 0) ring(s_ringZone, false);
        return false;
    }

    if (e == Touch::DOWN) {
        s_pressed = hit(x, y);
        if (s_pressed >= 0) {
            sendButton(s_zones[s_pressed].name, 0);
            if (s_ringZone >= 0 && s_ringZone != s_pressed) ring(s_ringZone, false);
            ring(s_pressed, true);
            s_ringUntil = now + RING_MIN_MS;
        } else {
            s_holdAway  = true;
            s_holdSince = now;
        }
    } else if (e == Touch::UP) {
        letGo();
    }

    if (s_pressed < 0 && s_ringZone >= 0 && (int32_t)(now - s_ringUntil) >= 0) ring(s_ringZone, false);

    if (s_holdAway && (now - s_holdSince) >= CAL_HOLD_MS) {
        s_holdAway = false;
        Touch::startCalibration();
        return true;
    }
    return false;
}

/* ---- The demo ------------------------------------------------------------------------ */
void setDemo(bool on)
{
    letGo();
    s_demo       = on;
    s_fingerDown = false;
    s_scripted   = false;
    if (on) ensureTouch();
}

uint32_t demoPoll()
{
    if (!s_demo) return 0;
    const uint32_t     now = millis();
    int16_t            x, y;
    const Touch::Event e = Touch::poll(x, y);

    if (e == Touch::DOWN) {
        s_fingerDown = true;
        s_downSince  = now;
        if (s_scripted) releaseScripted(); // the finger wins
        s_pressed = s_count ? hit(x, y) : -1;
        if (s_pressed >= 0) {
            if (s_ringZone >= 0 && s_ringZone != s_pressed) ring(s_ringZone, false);
            ring(s_pressed, true);
            s_ringUntil = now + RING_MIN_MS;
        }
    } else if (e == Touch::UP) {
        s_fingerDown = false;
        s_pressed    = -1;
    }

    if (!s_scripted && s_pressed < 0 && s_ringZone >= 0 && (int32_t)(now - s_ringUntil) >= 0)
        ring(s_ringZone, false);
    return s_fingerDown ? now - s_downSince : 0;
}

void pressScripted(const char *zoneName)
{
    if (!s_demo || s_fingerDown || !zoneName) return;
    for (uint8_t i = 0; i < s_count; ++i) {
        if (strcmp(s_zones[i].name, zoneName) != 0) continue;
        if (s_ringZone >= 0 && s_ringZone != (int8_t)i) ring(s_ringZone, false);
        ring((int8_t)i, true);
        s_scripted = true;
        return;
    }
}

void releaseScripted()
{
    if (!s_scripted) return;
    s_scripted = false;
    if (s_ringZone >= 0) ring(s_ringZone, false);
}

} // namespace TouchZones
