#pragma once

#include <Arduino.h>

/* **********************************************************************************
    What the board shows when it powers up, before MobiFlight has sent it anything:

      0-10 s   the splash: the logo, the project name, the firmware version, the build
               date and the credits
     10-20 s   identify: the panel this board is configured as, lit and showing its lamps
               as they are -- or NO PANEL CONFIGURED on a board without one
     after     dark until MobiFlight sends a value, exactly as before there was a splash

    MobiFlight sending the panel a value ends all of this at once (end()). Merely connecting
    does not: the Connector reads the board on connect without sending anything. So a board
    that resets mid-flight never hides live lamps behind a splash -- the first value it gets
    puts the panel back.

    It has to work on a board with no panel configured, where the core never calls the
    custom device at all, and on a board with one, where setup() would bring the panel up
    and draw over it. So the splash starts before setup(), from initVariant(), runs its
    clock in a small task of its own, and a configured panel waits for it: MFCustomDevice
    defers the panel's init until phase() has left SHOWING_SPLASH.

    Who draws, and when. The main task draws the panel; the splash's task draws only when no
    panel has claim()ed the screen -- the splash itself, and NO PANEL CONFIGURED -- and turns
    the backlight off at the end only on such a board. claim() waits for the task to finish
    a drawing it has already begun, so the two never draw at once.
********************************************************************************** */

namespace Splash
{
    constexpr uint32_t PHASE_MS = 10000; // each of the two phases

    enum Phase : uint8_t {
        SHOWING_SPLASH,
        IDENTIFYING,
        DONE,
    };

    // Brings the display up, draws the splash and lights it, and starts the clock. Called
    // once, from initVariant().
    void start();

    // The two screens. Portable: the host preview renders both.
    void draw();
    void drawNoPanel();

    Phase phase();

    // A panel has been configured and takes the screen from the identify phase on. Returns
    // once the splash's task is not drawing.
    void claim();

    // That panel was removed before it ever started -- a config upload during the splash
    // replaced it with none -- so the splash ends the way it does on a board with no panel.
    void release();

    // MobiFlight is driving the panel: skip whatever is left of the splash.
    void end();
} // namespace Splash
