/* Bring-up probe: which byte order does the panel want for a LovyanGFX sprite?
 *
 * The layout is right and the colours are not, which puts the fault between the two
 * libraries rather than in either: LovyanGFX holds a 16bpp sprite in its own byte order,
 * and Arduino_GFX offers two ways to push such a buffer. This draws five known colour
 * bands into a sprite exactly as the firmware does -- 320x480 buffer, rotated to a 480x320
 * drawing surface -- and alternates the two blits every five seconds.
 *
 * Whichever shows RED GREEN BLUE WHITE GREY from top to bottom is the one the firmware
 * should use.
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <LovyanGFX.hpp>

#include "Board.h"

static Arduino_DataBus *bus    = nullptr;
static Arduino_GFX     *panel  = nullptr;
static LGFX_Sprite      canvas;

void setup()
{
    Serial.begin(115200);
    const uint32_t waited = millis();
    while (!Serial && millis() - waited < 3000) delay(10);
    delay(300);

    pinMode(Board::BACKLIGHT_PIN, OUTPUT);
    digitalWrite(Board::BACKLIGHT_PIN, HIGH);

    bus   = new Arduino_ESP32QSPI(Board::LCD_CS, Board::LCD_SCK,
                                  Board::LCD_D0, Board::LCD_D1, Board::LCD_D2, Board::LCD_D3);
    panel = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480, 0, 0, 0, 0,
                                  axs15231b_320480_type1_init_operations,
                                  sizeof(axs15231b_320480_type1_init_operations));
    panel->begin();
    // never rotated: this panel rejects a rotated address window

    canvas.setPsram(true);
    canvas.setColorDepth(16);
    void *buf = canvas.createSprite(Board::PANEL_H, Board::PANEL_W); // 320 x 480
    canvas.setRotation(1);                                           // drawn as 480 x 320
    Serial.printf("sprite buf %p, drawing surface %dx%d\n", buf,
                  (int)canvas.width(), (int)canvas.height());

    // Five bands across the landscape surface, top to bottom.
    const uint16_t bands[5] = {0xF800 /*red*/, 0x07E0 /*green*/, 0x001F /*blue*/,
                               0xFFFF /*white*/, 0x8410 /*mid grey*/};
    const int16_t  h        = (int16_t)(canvas.height() / 5);
    for (int i = 0; i < 5; ++i)
        canvas.fillRect(0, (int16_t)(i * h), canvas.width(), h, bands[i]);

    Serial.println("\nexpect, top to bottom: RED GREEN BLUE WHITE GREY");
    Serial.println("two methods alternate every 5s -- tell me which one is correct");
}

void loop()
{
    static bool bigEndian = false;
    uint16_t   *buf       = (uint16_t *)canvas.getBuffer();

    if (bigEndian)
        panel->draw16bitBeRGBBitmap(0, 0, buf, Board::PANEL_H, Board::PANEL_W);
    else
        panel->draw16bitRGBBitmap(0, 0, buf, Board::PANEL_H, Board::PANEL_W);

    Serial.printf("--- showing method %s\n", bigEndian ? "B  (draw16bitBeRGBBitmap)"
                                                       : "A  (draw16bitRGBBitmap)");
    bigEndian = !bigEndian;
    delay(5000);
}
