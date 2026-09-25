#pragma once

#include <Arduino.h>

/* **********************************************************************************
    The E32R32P's resistive touch screen -- an XPT2046 on the display's SPI bus, chip
    select on GPIO 33 (TOUCH_CS in the build flags).

    TFT_eSPI's own getTouch() samples for 20-30 ms per call and blocks while it does, which
    would stall MobiFlight's serial handling; its calibrateTouch() blocks until all four
    corners are pressed. So this polls the controller directly -- one pressure and one
    position read, well under a millisecond -- and does the debouncing across polls, and
    calibrates without blocking.

    Screen coordinates are for the landscape panel as drawn: 0..319 across, 0..239 down.
    The calibration is kept in NVS, so it survives power cycles and reflashing.
********************************************************************************** */
namespace Touch
{
    enum Event : uint8_t {
        NONE = 0,
        DOWN, // a finger landed; the point is where
        UP,   // it lifted
    };

    void begin();
    void setRotation(uint8_t rotation); // 1 or 3, following the display

    // Call every poll (~20 ms). Returns an edge, and while a finger is down keeps `x`/`y`
    // at its filtered position.
    Event poll(int16_t &x, int16_t &y);

    // Four crosshairs, one at a time; takes over the screen until done. Driven by
    // stepCalibration(), which returns true on the pass it finishes (or gives up), when the
    // caller should redraw its panel.
    void startCalibration();
    bool calibrating();
    bool stepCalibration();
}
