#pragma once

#include <Arduino.h>

/* **********************************************************************************
    What the board shows when it powers up, before MobiFlight has sent it anything:

      0-10 s   the splash: the logo, the project name, the panel the board is configured
               as, the firmware version, the build date, the credits, a DEMO button, and a
               CREDITS button that opens every credit on a page of its own
     10-20 s   identify: the panel this board is configured as, lit and showing its lamps
               as they are -- or NO PANEL CONFIGURED, with the DEMO button, on a board
               without one
     after     dark until MobiFlight sends a value, exactly as before there was a splash

    DEMO plays every panel in turn, by itself (Demo.h), until a finger is held on the screen
    for 3 s -- back to the splash -- or MobiFlight sends a value.

    MobiFlight sending the panel a value ends all of this at once (end()). Nothing else does:
    connecting sends the panel nothing, and Stop and power save are not values. So a board
    that resets mid-flight never hides live lamps behind a splash or a demo -- the first
    value puts the panel back.

    It has to work on a board with no panel configured, where the core never calls the
    custom device at all, and on a board with one, where setup() would bring the panel up
    and draw over it. So the splash starts before setup(), from initVariant(), runs in a
    task of its own -- clock, touch and demo -- and a configured panel waits for it:
    MFCustomDevice defers the panel's init while ownsScreen().

    Who draws, and when. The main task draws the panel; the splash's task draws the splash,
    the demo, and NO PANEL CONFIGURED only while no panel has claimed the screen. Each of the
    task's frames is fenced (see beginFrame() in Splash.cpp), and claim() and end() wait out
    a frame in progress, so the two never draw at once.
********************************************************************************** */

namespace Splash
{
    constexpr uint32_t PHASE_MS = 10000; // each of the two phases

    enum Phase : uint8_t {
        SHOWING_SPLASH,
        DEMO, // the DEMO button was tapped: every panel in turn, until a hold or a value
        IDENTIFYING,
        DONE,
    };

    // Brings the display up, draws the splash and lights it, and starts the clock. Called
    // once, from initVariant().
    void start();

    // The two screens. Portable: the host preview renders both.
    void draw();
    void drawNoPanel();

    // The panel name under the splash's title: a panel's, NOT CONFIGURED, or nothing yet
    // (nullptr). Draws it at once if the splash is on the screen. Portable: the host
    // preview sets one before drawing.
    void showPanelName(const char *name);

    // The credits page, which the splash's CREDITS button opens.
    void drawCredits();

    // The buttons on whichever screen was drawn last: DEMO (the splash and NO PANEL
    // CONFIGURED) and CREDITS (the splash). Which one a point is on, and drawing one pressed
    // in or standing proud.
    enum Button : uint8_t { NO_BUTTON, DEMO_BUTTON, CREDITS_BUTTON };
    Button buttonAt(int16_t x, int16_t y);
    void   pressButton(Button button, bool pressed);

    Phase phase();

    // Whether the splash or its demo has the screen. A configured panel waits while it does.
    bool ownsScreen();

    // A panel has been configured, by this name, and takes the screen from the identify
    // phase on. The splash shows the name under its title. Returns once the splash's task is
    // not drawing.
    void claim(const char *panelName);

    // That panel was removed before it ever started -- a config upload during the splash
    // replaced it with none -- so the splash ends the way it does on a board with no panel.
    void release();

    // Returns once the splash's task is not in the middle of a frame.
    void waitIdle();

    // MobiFlight is driving the panel: skip whatever is left of the splash -- including a
    // demo, which stops between frames and leaves the screen to the panel. Returns once it
    // has.
    void end();
} // namespace Splash
