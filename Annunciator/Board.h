#pragma once

#include <Arduino.h>

/* **********************************************************************************
    Everything that is true of the board rather than of the panel design: screen size,
    which GPIO the backlight is on, which display library can drive it.

    This is the only header that names a pin. The ten panel layouts, the renderer and the
    MobiFlight glue all work in terms of Board::PANEL_W / PANEL_H and never learn which
    board they are running on.

    The board is chosen by a build flag in Annunciator_platformio.ini. ANNUN_BOARD_HOST is
    not a board at all -- it is the host preview in tools/preview, which renders the real
    panel code to a PNG on a desktop.
********************************************************************************** */

#if defined(ANNUN_BOARD_E32R32P)

/* LCDWIKI / QDtech "3.2inch ESP32-32E Display", SKU E32R32P.
   ESP32-WROOM-32E, no PSRAM, CH340C USB. 3.2" 240x320 IPS (ST7789P3) driven over HSPI as
   320x240 landscape, XPT2046 resistive touch sharing that SPI bus. */
#define ANNUN_BACKEND_TFT_ESPI 1
#define ANNUN_PANEL_RES        320

namespace Board
{
    constexpr int16_t PANEL_W = 320, PANEL_H = 240;
    constexpr uint8_t DEFAULT_ROTATION = 1; // 1 = landscape; 3 if the image lands 180 out

    constexpr uint8_t BACKLIGHT_PIN = 27; // active high
    // The XPT2046 shares the display SPI bus, so its chip select has to be parked high from
    // boot -- before TFT_eSPI takes the pin over -- or it can drive MISO.
    constexpr bool    HAS_SHARED_SPI_TOUCH = true;
    constexpr uint8_t TOUCH_CS_PIN         = 33;
} // namespace Board

#elif defined(ANNUN_BOARD_C3248W535)

/* Caturda C3248W535, sold also as Guition/JCZN JC3248W535EN.
   ESP32-S3-WROOM-1 N16R8 (16MB flash, 8MB OPI PSRAM), native USB-Serial-JTAG. 3.5" 320x480
   IPS with an AXS15231B over QSPI, drawn as 480x320 landscape. Capacitive touch is the same
   AXS15231B over I2C. Every pin here is a fixed PCB trace. */
#define ANNUN_BACKEND_LGFX 1
#define ANNUN_PANEL_RES    480

namespace Board
{
    constexpr int16_t PANEL_W = 480, PANEL_H = 320;
    constexpr uint8_t DEFAULT_ROTATION = 1;

    constexpr uint8_t BACKLIGHT_PIN = 1; // active high, LEDC-capable
    // Touch is on I2C here, so there is no chip select to park at boot. The pin is still
    // declared, because the code that parks one is shared and simply never runs.
    constexpr bool    HAS_SHARED_SPI_TOUCH = false;
    constexpr uint8_t TOUCH_CS_PIN         = 0;

    // Display: QSPI, no DC pin -- command and data are framed inside the QSPI transfer.
    constexpr uint8_t LCD_CS = 45, LCD_SCK = 47;
    constexpr uint8_t LCD_D0 = 21, LCD_D1 = 48, LCD_D2 = 40, LCD_D3 = 39;
    constexpr uint8_t LCD_TE = 38; // tearing-effect output; unused so far

    // Touch: the display controller again, on I2C.
    constexpr uint8_t TOUCH_SDA = 4, TOUCH_SCL = 8, TOUCH_INT = 3;
    constexpr uint8_t TOUCH_ADDR = 0x3B;
} // namespace Board

#elif defined(ANNUN_BOARD_HOST)

/* Not hardware: the host preview (tools/preview), which builds the real renderer against a
   PNG canvas so a layout can be reviewed without a board. Resolution comes from the build,
   so the same sources render both screens side by side. */
#define ANNUN_BACKEND_HOST 1
#ifndef ANNUN_PANEL_RES
#define ANNUN_PANEL_RES 320
#endif

namespace Board
{
#if ANNUN_PANEL_RES == 480
    constexpr int16_t PANEL_W = 480, PANEL_H = 320;
#else
    constexpr int16_t PANEL_W = 320, PANEL_H = 240;
#endif
    constexpr uint8_t DEFAULT_ROTATION      = 1;
    constexpr uint8_t BACKLIGHT_PIN         = 0;
    constexpr bool    HAS_SHARED_SPI_TOUCH  = false;
    constexpr uint8_t TOUCH_CS_PIN          = 0;
} // namespace Board

#else
#error "No board selected: define ANNUN_BOARD_E32R32P, ANNUN_BOARD_C3248W535 or ANNUN_BOARD_HOST"
#endif
