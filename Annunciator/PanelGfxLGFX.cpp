#include "Gfx.h"

#if ANNUN_BACKEND_LGFX

#include <Arduino_GFX_Library.h>

#include "commandmessenger.h"

/* The C3248W535's backend: an AXS15231B over QSPI.
 *
 * Two libraries, each doing the thing it is good at. LovyanGFX draws the pixels, into a
 * full-screen sprite in PSRAM -- it is the only library with the smooth-font, sprite and
 * text-datum calls the renderer is written against. Arduino_GFX does nothing but carry
 * that finished frame to the panel, because its AXS15231B driver over Arduino_ESP32QSPI
 * is the QSPI path that is known to work on this display.
 *
 * Whole frames, always. Reports differ on whether a given batch of these panels honours
 * the window-address commands (0x2A/0x2B) at all, and Espressif's own driver has an open
 * bug that skips one of them in QSPI mode. Blitting the entire frame sidesteps the
 * question: at 40MHz over four data lines a 480x320 frame is about 15ms of bus time, and
 * a panel of annunciators only redraws when a lamp changes.
 *
 * THE PANEL IS NEVER ROTATED, and that is not a preference. Measured on this board: a
 * plain fillScreen at rotation 0 fills the screen, and the same fill after setRotation(1)
 * puts a handful of pixels down the left edge and nothing else -- the controller does not
 * accept the rotated address window, so every write after it lands as a stripe. The
 * colours were always right, which is what identifies this as addressing rather than data.
 *
 * So the panel keeps its native 320x480 portrait shape and the SPRITE is rotated instead.
 * LovyanGFX keeps a sprite's buffer exactly as created and applies rotation to the drawing
 * coordinates, so a 320x480 buffer at sprite rotation 1 presents a 480x320 landscape
 * surface to the renderer while staying in the layout the panel wants. The nine panel
 * layouts know nothing about any of this.
 *
 * The init sequence is named rather than defaulted. Arduino_GFX's default for this driver
 * is axs15231b_180640_init_operations -- a 180x640 LilyGO panel, not this one -- so a
 * constructor that leaves it out produces a blank or garbled screen. type1 is the sequence
 * for a 320x480 AXS15231B; the library also ships a type2, and reports differ on which
 * batch wants which, so if this panel comes up wrong that is the first thing to swap.
 */

namespace
{
Arduino_DataBus *s_bus   = nullptr;
Arduino_GFX     *s_panel = nullptr;
char             s_report[96] = "";
} // namespace

namespace Gfx
{

void deviceBegin(Device &device)
{
    s_bus = new Arduino_ESP32QSPI(Board::LCD_CS, Board::LCD_SCK,
                                  Board::LCD_D0, Board::LCD_D1, Board::LCD_D2, Board::LCD_D3);
    s_panel = new Arduino_AXS15231B(s_bus, GFX_NOT_DEFINED /* no reset pin: tied to EN */,
                                    0 /* rotation, set below */, false /* not IPS-inverted */,
                                    320, 480, 0, 0, 0, 0,
                                    axs15231b_320480_type1_init_operations,
                                    sizeof(axs15231b_320480_type1_init_operations));
    s_panel->begin();

    // The frame buffer. 480 x 320 x 16bpp is 300KB, so it has to come from PSRAM; without
    // it createSprite() fails and the panel stays dark, which is the first thing to check
    // if this board ever comes up black.
    device.setPsram(true);
    device.setColorDepth(16);
    // Created in the panel's shape (320 x 480), then rotated so the renderer sees the
    // 480x320 it draws in. The buffer stays portrait, which is the only thing the panel
    // will accept.
    const void *buffer = device.createSprite(Board::PANEL_H, Board::PANEL_W);
    device.setRotation(1);

    /* Report what actually happened. A failed allocation here is silent otherwise: every
       draw goes to a zero-sized sprite, present() blits from nothing, and the panel shows
       a near-black screen with a few stray pixels -- which looks like a display fault
       rather than an out-of-memory one. Kept rather than sent: this runs for the boot
       splash, before the serial port is open -- see deviceReport(). */
    snprintf(s_report, sizeof(s_report), "Annunciator: frame %dx%d buf=%p psram free %u",
             (int)device.width(), (int)device.height(), buffer,
             (unsigned)ESP.getFreePsram());
}

const char *deviceReport() { return s_report[0] ? s_report : nullptr; }

void deviceRotate(Device &device, uint8_t rotation)
{
    // MSG_ROTATION turns the picture over: 1 and 3 are the two ways up in landscape. It
    // turns the sprite, never the panel -- see the note at the top of this file.
    device.setRotation(rotation);
}

void present(Device &device)
{
    if (!s_panel) return;
    uint16_t *buffer = (uint16_t *)device.getBuffer();
    if (!buffer) {
        static bool moaned = false;
        if (!moaned) {
            moaned = true;
            cmdMessenger.sendCmd(kDebug, F("Annunciator: no frame buffer - nothing can be drawn"));
        }
        return;
    }
    /* Always the panel's own dimensions: the sprite reports 480x320 because it is rotated,
       but its buffer is, and has to be sent as, 320x480.

       And the big-endian blit, not the plain one. LovyanGFX keeps a 16bpp sprite in the
       byte order SPI panels want, which is already big-endian, so draw16bitRGBBitmap
       swaps bytes that need no swapping. Measured on this board: with the plain call the
       layout was perfect and every colour wrong -- amber lamps red, blue green, and the
       near-black unlit lamps bright magenta. A dark colour coming out bright is the tell
       that it is a byte swap rather than a channel order. */
    const uint32_t began = millis();
    s_panel->draw16bitBeRGBBitmap(0, 0, buffer, Board::PANEL_H, Board::PANEL_W);

    static uint16_t frames = 0;
    if ((++frames % 100) == 1) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Annunciator: frame %u sent as %dx%d in %ums",
                 (unsigned)frames, (int)Board::PANEL_H, (int)Board::PANEL_W,
                 (unsigned)(millis() - began));
        cmdMessenger.sendCmd(kDebug, msg);
    }
}

} // namespace Gfx

#endif
