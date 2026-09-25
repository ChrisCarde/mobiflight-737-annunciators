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

// The read command, as the controller's own driver issues it, then eight bytes back.
static const uint8_t READ_CMD[8]  = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08};
static const uint8_t REPLY_LEN    = 8;

// Native panel orientation, before rotation: 320 across, 480 down.
static const int16_t NATIVE_W = 320, NATIVE_H = 480;

static bool     s_ready      = false;
static uint8_t  s_rotation   = Board::DEFAULT_ROTATION;
static bool     s_down       = false;
static int16_t  s_x = 0, s_y = 0;
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

static bool readPoint(int16_t &x, int16_t &y)
{
    if (!s_ready) return false;

    Wire.beginTransmission(Board::TOUCH_ADDR);
    Wire.write(READ_CMD, sizeof(READ_CMD));
    if (Wire.endTransmission() != 0) return false;

    uint8_t reply[REPLY_LEN] = {0};
    if (Wire.requestFrom((int)Board::TOUCH_ADDR, (int)REPLY_LEN) != REPLY_LEN) return false;
    for (uint8_t i = 0; i < REPLY_LEN; ++i) reply[i] = (uint8_t)Wire.read();

    // reply[1] is the number of points; the coordinates carry flag bits in their top nibble.
    if (reply[1] == 0) return false;
    const int16_t nx = (int16_t)(((reply[2] & 0x0F) << 8) | reply[3]);
    const int16_t ny = (int16_t)(((reply[4] & 0x0F) << 8) | reply[5]);
    if (nx >= NATIVE_W || ny >= NATIVE_H) return false;

    toScreen(nx, ny, x, y);
    return true;
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

Event poll(int16_t &x, int16_t &y)
{
    int16_t px = 0, py = 0;
    const bool touching = readPoint(px, py);

    // A capacitive controller debounces in hardware and reports a settled point, so unlike
    // the resistive panel there is nothing to average or threshold here.
    if (touching) {
        s_x = px;
        s_y = py;
        x   = px;
        y   = py;
        if (!s_down) {
            s_down = true;
            return DOWN;
        }
        return NONE;
    }

    if (s_down) {
        s_down = false;
        x      = s_x;
        y      = s_y;
        return UP;
    }
    return NONE;
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
