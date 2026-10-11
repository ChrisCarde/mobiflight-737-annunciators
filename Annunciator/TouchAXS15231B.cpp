/* The C3248W535's touch: the AXS15231B again.
 *
 * The same chip that drives the display is also the capacitive touch controller, on a
 * separate I2C bus. It is factory-calibrated and reports pixel coordinates directly, so
 * there is nothing for the four-point calibration the resistive board needs -- but
 * MSG_TOUCH_CAL still has to do something sane, because the profiles are shared between
 * the boards and one of them sends it. It says so on screen and returns.
 *
 * Reading it is a fixed command followed by an 8-byte reply, which is what every driver
 * for this part does (ESPHome's axs15231 component, and the Arduino ports of it). The
 * coordinates come back in the panel's native portrait orientation, so they are turned
 * here to match the landscape the renderer draws in.
 */

#include "Touch.h"
#include "Board.h"
#include "Layout.h"
#include "PanelGfx.h"
#include "Theme.h"

#include <Wire.h>

namespace Touch
{

/* The read command, as the vendor's own driver for this board issues it (Espressif's
   esp_lcd_axs15231b, and ESPHome's axs15231): 11 bytes, the last three padding, asking for
   an 8-byte reply -- one point. Then a moment for the controller to have its answer ready
   before it is clocked out. */
static const uint8_t  READ_CMD[11]   = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
static const uint8_t  REPLY_LEN      = 8;
static const uint32_t REPLY_DELAY_US = 100;

// Native panel orientation, before rotation: 320 across, 480 down.
static const int16_t NATIVE_W = 320, NATIVE_H = 480;

static bool     s_ready      = false;
static uint8_t  s_rotation   = Board::DEFAULT_ROTATION;
static bool     s_down       = false;
static int16_t  s_x = 0, s_y = 0;
static uint8_t  s_hits = 0, s_misses = 0; // touch reports towards a press, lift reports towards a release
static uint32_t s_lastTouch = 0;          // when the last good touch report came

// See poll().
static const uint8_t  PRESS_READS      = 2;
static const uint8_t  RELEASE_READS    = 2;
static const uint32_t PRESS_WINDOW_MS  = 100; // a press's reports must come this close together
static const uint32_t INVALID_GRACE_MS = 100; // junk ends a held press after this long
static uint32_t s_noticeUntil = 0; // the "no calibration needed" notice

/* Native portrait to the landscape the panel is drawn in. Rotation 1 and 3 are the two
   ways up; which is which depends on how the board is mounted, and is the one thing here
   that has to be confirmed against the hardware -- a press landing in the mirrored corner
   means these two lines want swapping. */
static void toScreen(int16_t nx, int16_t ny, int16_t &x, int16_t &y)
{
    if ((s_rotation & 3) == 3) {
        x = (int16_t)(NATIVE_H - 1 - ny);
        y = nx;
    } else {
        x = ny;
        y = (int16_t)(NATIVE_W - 1 - nx);
    }
    x = constrain(x, (int16_t)0, (int16_t)(LY::PANEL_W - 1));
    y = constrain(y, (int16_t)0, (int16_t)(LY::PANEL_H - 1));
}

/* What one read says. Believe only a well-formed report: measured on this board, after a
   touch has lifted the controller goes on answering with filler for a minute or more --
   30 30 30 30 30 30 30 30, or FF FF FF FF FF F4 30 30 -- which, taken at face value, is
   48 points at (48, 48): a finger held on the screen that nobody is touching. A real
   report reads 00 01 00 8C 01 0D 01 02:
     [0]  gesture, 0 in every touch report
     [1]  points: 0 or 1 (an 8-byte read carries one)
     [2]  event in bits 7:6 -- 0 press, 1 lift, 2 contact, 3 nothing -- reserved bits 5:4,
          X bits 11:8
     [3]  X bits 7:0
     [4]  touch ID in bits 7:4 (15 is none), Y bits 11:8
     [5]  Y bits 7:0
   The same reading is in the vendor's driver and in the most careful community one for this
   board (jantielens/esp32-macropad), whose tests carry the same filler. */
enum class Report : uint8_t { TOUCH, LIFT, INVALID };

static Report readReport(int16_t &x, int16_t &y)
{
    if (!s_ready) return Report::INVALID;

    Wire.beginTransmission(Board::TOUCH_ADDR);
    Wire.write(READ_CMD, sizeof(READ_CMD));
    if (Wire.endTransmission() != 0) return Report::INVALID;
    delayMicroseconds(REPLY_DELAY_US);

    uint8_t reply[REPLY_LEN] = {0};
    if (Wire.requestFrom((int)Board::TOUCH_ADDR, (int)REPLY_LEN) != REPLY_LEN) return Report::INVALID;
    for (uint8_t i = 0; i < REPLY_LEN; ++i) reply[i] = (uint8_t)Wire.read();

    if (reply[0] != 0) return Report::INVALID;            // and with it any one-byte filler
    if (reply[1] == 0) return Report::LIFT;
    if (reply[1] != 1) return Report::INVALID;
    if (reply[2] & 0x30) return Report::INVALID;          // reserved bits
    if ((reply[4] >> 4) == 0x0F) return Report::INVALID;  // no touch ID
    const uint8_t event = (uint8_t)(reply[2] >> 6);
    if (event == 1 || event == 3) return Report::LIFT;
    const int16_t nx = (int16_t)(((reply[2] & 0x0F) << 8) | reply[3]);
    const int16_t ny = (int16_t)(((reply[4] & 0x0F) << 8) | reply[5]);
    if (nx >= NATIVE_W || ny >= NATIVE_H) return Report::INVALID;

    toScreen(nx, ny, x, y);
    return Report::TOUCH;
}

void begin()
{
    Wire.begin(Board::TOUCH_SDA, Board::TOUCH_SCL, 400000);
    pinMode(Board::TOUCH_INT, INPUT); // polled, but leaving it floating invites chatter
    s_ready = true;
}

void setRotation(uint8_t rotation)
{
    s_rotation = rotation;
}

/* Edges from reports, so that no stray report can invent a touch or split one:
     - a press takes PRESS_READS touch reports, no further apart than PRESS_WINDOW_MS;
     - a release takes RELEASE_READS lift reports;
     - filler while a press is held ends it once no good report has come for
       INVALID_GRACE_MS. On this board a lift usually arrives as filler rather than as a lift
       report, so this is the ordinary way a press ends; the grace is what keeps one bad read
       in the middle of a held press from turning it into two presses.
   At the 20 ms poll a press and a release each take about 40 ms; filler while no finger is
   down changes nothing. */
Event poll(int16_t &x, int16_t &y)
{
    const uint32_t now = millis();
    int16_t        px = 0, py = 0;
    const Report   report = readReport(px, py);

    if (report == Report::TOUCH) {
        if (now - s_lastTouch > PRESS_WINDOW_MS) s_hits = 0; // a stale report doesn't count
        s_lastTouch = now;
        s_misses    = 0;
        s_x         = px;
        s_y         = py;
        x           = px;
        y           = py;
        if (!s_down && ++s_hits >= PRESS_READS) {
            s_down = true;
            s_hits = 0;
            return DOWN;
        }
        return NONE;
    }

    if (report == Report::LIFT) s_hits = 0;
    if (!s_down) return NONE;
    x = s_x; // the finger's last place, until it is believed gone
    y = s_y;
    const bool released = report == Report::LIFT ? ++s_misses >= RELEASE_READS
                                                 : now - s_lastTouch >= INVALID_GRACE_MS;
    if (!released) return NONE;
    s_down   = false;
    s_misses = 0;
    return UP;
}

/* There is nothing to calibrate. The message matters: a profile shared with the resistive
   board sends MSG_TOUCH_CAL, and a panel that simply ignored it would look broken. */
void startCalibration()
{
    s_noticeUntil = millis() + 1500;
    PanelGfx::clearPanel();
    const char *lines[2] = {"CAPACITIVE TOUCH", "NO CALIBRATION NEEDED"};
    for (uint8_t i = 0; i < 2; ++i) {
        const int16_t w = PanelGfx::spacedTextWidth(lines[i], 2);
        PanelGfx::drawSpacedText((int16_t)((LY::PANEL_W - w) / 2),
                                 (int16_t)(LY::PANEL_H / 2 - 12 + i * 24),
                                 lines[i], Theme::HeaderFg, 2);
    }
}

bool calibrating()
{
    return s_noticeUntil != 0;
}

// Returns true on the pass that finishes, so the caller redraws its panel.
bool stepCalibration()
{
    if (s_noticeUntil && (int32_t)(millis() - s_noticeUntil) >= 0) {
        s_noticeUntil = 0;
        return true;
    }
    return false;
}

} // namespace Touch
