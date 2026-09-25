/* The E32R32P's touch: an XPT2046 resistive panel sharing the display's SPI bus, read
   through the display library's raw calls. It needs a four-point calibration, which this
   file also draws; the capacitive board's driver is TouchAXS15231B.cpp and needs none.
   Both meet the interface in Touch.h, and TouchZones.cpp knows nothing of either. */

#include "Touch.h"
#include "Layout.h"
#include "PanelGfx.h"
#include "Theme.h"
#include "commandmessenger.h"
#include <Preferences.h>
#include <stdio.h>

namespace Touch
{

static const int16_t SCREEN_W = LY::PANEL_W, SCREEN_H = LY::PANEL_H;

/* ---- Pressure ----------------------------------------------------------------------
   getTouchRawZ() gives about 0 untouched and climbs with pressure. A press needs to pass
   Z_ON; it only counts as lifted below Z_OFF, so a finger easing off mid-press does not
   chatter. TFT_eSPI's own threshold is 350. */
static const uint16_t Z_ON  = 400;
static const uint16_t Z_OFF = 250;

// Two consecutive readings that agree make a press, and two below Z_OFF a release: 40 ms
// each way at the 20 ms poll, which also rejects the odd noise spike.
static const uint8_t  SAMPLES_ON  = 2;
static const uint8_t  SAMPLES_OFF = 2;
static const uint16_t AGREE_RAW   = 80; // ~7 px: how far two readings of one press may differ

/* ---- Calibration --------------------------------------------------------------------
   Raw readings along each screen axis, at the calibration targets' screen positions. The
   XPT2046's axes may run either way and may be swapped relative to the display, depending
   on how the panel is mounted, so all of that comes out of the calibration. */
static const int16_t CAL_X0 = 30, CAL_X1 = SCREEN_W - 1 - 30; // target columns
static const int16_t CAL_Y0 = 30, CAL_Y1 = SCREEN_H - 1 - 30; // target rows

struct Cal {
    uint32_t magic;
    uint8_t  swap;     // raw X follows the screen's y axis
    uint8_t  rotation; // display rotation it was taken in
    int16_t  ax0, ax1; // raw reading at CAL_X0 and CAL_X1
    int16_t  ay0, ay1; // raw reading at CAL_Y0 and CAL_Y1
};
static const uint32_t CAL_MAGIC = 0x54434131; // "TCA1"

// Used until a calibration has been saved: measured on an E32R32P in rotation 1. The
// XPT2046's X runs down this panel and its Y from right to left. Another board of the same
// model is usually close enough to press the right lamp; calibrate if presses land off.
static const Cal DEFAULT_CAL = {CAL_MAGIC, 1, 1, 3627, 772, 516, 3454};

static Cal     s_cal      = DEFAULT_CAL;
static uint8_t s_rotation = 1;

/* ---- Poll state ------------------------------------------------------------------- */
static bool     s_down    = false;
static uint8_t  s_onRun   = 0, s_offRun = 0;
static uint16_t s_lastRx  = 0, s_lastRy = 0;
static int32_t  s_fx      = 0, s_fy = 0; // filtered raw, x16

static void debugf(const char *fmt, int a, int b = 0, int c = 0, int d = 0, int e = 0)
{
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, a, b, c, d, e);
    cmdMessenger.sendCmd(kDebug, buf);
}

static void loadCal()
{
    Preferences p;
    if (!p.begin("annun", true)) return; // read-only; fails harmlessly before first save
    Cal c;
    if (p.getBytes("tcal", &c, sizeof(c)) == sizeof(c) && c.magic == CAL_MAGIC &&
        c.ax0 != c.ax1 && c.ay0 != c.ay1) {
        s_cal = c;
    }
    p.end();
}

static void saveCal()
{
    Preferences p;
    if (!p.begin("annun", false)) return;
    p.putBytes("tcal", &s_cal, sizeof(s_cal));
    p.end();
}

void begin()
{
    loadCal();
}

void setRotation(uint8_t rotation)
{
    s_rotation = rotation;
}

static int16_t mapAxis(int32_t raw, int16_t r0, int16_t r1, int16_t s0, int16_t s1, int16_t limit)
{
    const int32_t v = s0 + (raw - r0) * (int32_t)(s1 - s0) / (int32_t)(r1 - r0);
    return (int16_t)constrain(v, 0, limit - 1);
}

static void toScreen(int32_t rx, int32_t ry, int16_t &x, int16_t &y)
{
    const int32_t a = s_cal.swap ? ry : rx; // runs along the screen's x
    const int32_t b = s_cal.swap ? rx : ry;
    x = mapAxis(a, s_cal.ax0, s_cal.ax1, CAL_X0, CAL_X1, SCREEN_W);
    y = mapAxis(b, s_cal.ay0, s_cal.ay1, CAL_Y0, CAL_Y1, SCREEN_H);
    // Rotations 1 and 3 are the two landscape ways up: 180 degrees apart.
    if ((s_rotation & 3) != (s_cal.rotation & 3)) {
        x = (int16_t)(SCREEN_W - 1 - x);
        y = (int16_t)(SCREEN_H - 1 - y);
    }
}

// The debounced edge, in raw units. Shared by poll() and the calibration.
static Event pollRaw(int32_t &rx, int32_t &ry)
{
    Gfx::Device &t = PanelGfx::tft();
    const uint16_t z = t.getTouchRawZ();

    if (z >= Z_ON) {
        uint16_t x1, y1, x2, y2;
        t.getTouchRaw(&x1, &y1);
        t.getTouchRaw(&x2, &y2);
        const uint16_t sx = (uint16_t)((x1 + x2) / 2), sy = (uint16_t)((y1 + y2) / 2);
        const bool agree  = s_onRun && abs((int)sx - (int)s_lastRx) <= AGREE_RAW &&
                           abs((int)sy - (int)s_lastRy) <= AGREE_RAW;
        s_onRun  = agree ? (uint8_t)min<int>(s_onRun + 1, 255) : 1;
        s_offRun = 0;
        s_lastRx = sx;
        s_lastRy = sy;

        if (!s_down && s_onRun >= SAMPLES_ON) {
            s_down = true;
            s_fx   = (int32_t)sx << 4;
            s_fy   = (int32_t)sy << 4;
            rx     = sx;
            ry     = sy;
            return DOWN;
        }
        if (s_down && agree) {
            // Light smoothing while held; the finger barely moves on a press.
            s_fx += (((int32_t)sx << 4) - s_fx) / 4;
            s_fy += (((int32_t)sy << 4) - s_fy) / 4;
        }
    } else if (z < Z_OFF) {
        s_onRun = 0;
        if (s_down && ++s_offRun >= SAMPLES_OFF) {
            s_down   = false;
            s_offRun = 0;
            rx       = s_fx >> 4;
            ry       = s_fy >> 4;
            return UP;
        }
    }
    // Between the thresholds: hold whatever state we are in.

    rx = s_fx >> 4;
    ry = s_fy >> 4;
    return NONE;
}

Event poll(int16_t &x, int16_t &y)
{
    int32_t     rx, ry;
    const Event e = pollRaw(rx, ry);
    toScreen(rx, ry, x, y);
    if (e == DOWN) debugf("touch %d,%d (raw %d,%d)", x, y, (int)rx, (int)ry);
    return e;
}

/* ---- Calibration ------------------------------------------------------------------ */
static const uint32_t CAL_TIMEOUT_MS = 30000; // per target, then give up
static const uint32_t CAL_RESULT_MS  = 1500;  // how long the outcome stays up

static int8_t   s_step = -1; // -1 idle, 0..3 the target being pressed, 4 showing the result
static int32_t  s_sumX, s_sumY;
static uint16_t s_n;
static int16_t  s_rawX[4], s_rawY[4];
static uint32_t s_deadline;

static void target(uint8_t i, int16_t &x, int16_t &y)
{
    x = (i & 1) ? CAL_X1 : CAL_X0;
    y = (i & 2) ? CAL_Y1 : CAL_Y0;
}

static void centreText(const char *text, int16_t yMid, uint32_t colour)
{
    const int16_t w = PanelGfx::spacedTextWidth(text, 2);
    PanelGfx::drawSpacedText((int16_t)((SCREEN_W - w) / 2), yMid, text, colour, 2);
}

static void drawTarget(uint8_t i)
{
    Gfx::Device &t = PanelGfx::tft();
    PanelGfx::clearPanel();
    int16_t x, y;
    target(i, x, y);
    const uint16_t c = Theme::to565(0xF4F7F1);
    t.drawFastHLine(x - 12, y, 25, c);
    t.drawFastVLine(x, y - 12, 25, c);
    t.drawCircle(x, y, 7, c);
    centreText("TOUCH CALIBRATION", 104, Theme::HeaderFg);
    centreText("PRESS THE CENTRE OF THE CROSS", 124, Theme::Placard);
    char n[8];
    snprintf(n, sizeof(n), "%d / 4", i + 1);
    centreText(n, 144, Theme::HeaderFg);
}

static void finish(const char *message, uint32_t colour)
{
    PanelGfx::clearPanel();
    centreText(message, 120, colour);
    s_step     = 4;
    s_deadline = millis() + CAL_RESULT_MS;
}

static void compute()
{
    // Which raw axis runs along the screen's x: the one that changes more from the left
    // targets to the right ones.
    const int32_t xAlongX = abs(s_rawX[1] - s_rawX[0]) + abs(s_rawX[3] - s_rawX[2]);
    const int32_t yAlongX = abs(s_rawY[1] - s_rawY[0]) + abs(s_rawY[3] - s_rawY[2]);
    const bool    swap    = yAlongX > xAlongX;

    int16_t a[4], b[4];
    for (uint8_t i = 0; i < 4; ++i) {
        a[i] = swap ? s_rawY[i] : s_rawX[i];
        b[i] = swap ? s_rawX[i] : s_rawY[i];
    }
    Cal c;
    c.magic    = CAL_MAGIC;
    c.swap     = swap;
    c.rotation = s_rotation;
    c.ax0      = (int16_t)((a[0] + a[2]) / 2); // left pair
    c.ax1      = (int16_t)((a[1] + a[3]) / 2); // right pair
    c.ay0      = (int16_t)((b[0] + b[1]) / 2); // top pair
    c.ay1      = (int16_t)((b[2] + b[3]) / 2); // bottom pair

    debugf("touch cal swap=%d x=%d..%d y=%d..%d", c.swap, c.ax0, c.ax1, c.ay0, c.ay1);

    // Across ~260 px a real panel spans thousands of raw counts. Much less means presses
    // missed their targets (or one was pressed twice): keep the old calibration.
    if (abs(c.ax1 - c.ax0) < 800 || abs(c.ay1 - c.ay0) < 600) {
        finish("CALIBRATION FAILED - NOT SAVED", 0xFFC24D);
        return;
    }
    s_cal = c;
    saveCal();
    finish("TOUCH CALIBRATED", 0x86F296);
}

void startCalibration()
{
    s_step     = 0;
    s_n        = 0;
    s_sumX     = s_sumY = 0;
    s_deadline = millis() + CAL_TIMEOUT_MS;
    drawTarget(0);
}

bool calibrating() { return s_step >= 0; }

bool stepCalibration()
{
    if (s_step < 0) return false;

    if (s_step == 4) {
        if ((int32_t)(millis() - s_deadline) < 0) return false;
        s_step = -1;
        return true;
    }

    int32_t     rx, ry;
    const Event e = pollRaw(rx, ry);
    if (s_down) {
        s_sumX += s_lastRx;
        s_sumY += s_lastRy;
        ++s_n;
    }
    if (e == UP && s_n >= 2) {
        s_rawX[s_step] = (int16_t)(s_sumX / s_n);
        s_rawY[s_step] = (int16_t)(s_sumY / s_n);
        debugf("touch cal %d raw %d,%d", s_step + 1, s_rawX[s_step], s_rawY[s_step]);
        s_n        = 0;
        s_sumX     = s_sumY = 0;
        s_deadline = millis() + CAL_TIMEOUT_MS;
        if (++s_step < 4)
            drawTarget((uint8_t)s_step);
        else
            compute();
        return false;
    }
    if (e == UP) { // too brief to trust; wait for another press
        s_n    = 0;
        s_sumX = s_sumY = 0;
    }

    if ((int32_t)(millis() - s_deadline) >= 0) finish("CALIBRATION CANCELLED", Theme::HeaderFg);
    return false;
}

} // namespace Touch
