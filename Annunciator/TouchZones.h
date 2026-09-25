#pragma once

#include <Arduino.h>

/* **********************************************************************************
    Touch buttons on a panel: rectangles that send a MobiFlight button event when pressed
    -- "7,<name>,0;" on press and "7,<name>,1;" on release, exactly what a button wired to
    the board would send -- so the Connector binds one like any button, by name.

    Also owns the rest of what a touch panel needs: the ring drawn round a zone while it is
    held, and the touch calibration (MSG_TOUCH_CAL, or a 6-second hold off every zone).
    Pressing never changes anything on the panel by itself; lamps follow the sim.
********************************************************************************** */
namespace TouchZones
{
    struct Zone {
        int16_t     x, y, w, h;
        const char *name; // the button name MobiFlight sees
    };

    // Optional, for panels whose buttons are not simple rectangles.
    struct Hooks {
        // Which zone a press at (x, y) is for, before the rectangles are tried; -1 for none.
        int8_t (*resolve)(int16_t x, int16_t y);
        // Show or clear the pressed state of a zone yourself; return false to get the ring.
        bool (*feedback)(int8_t zone, bool pressed);
    };

    void attach(const Zone *zones, uint8_t count, const Hooks *hooks = nullptr);
    void detach(); // releases a button still held

    // Call every update. Returns true while the calibration has the screen; sets `redraw`
    // on the pass it hands the screen back, when the panel must repaint everything.
    bool poll(bool &redraw);

    void startCalibration();

    // The panel repainted the whole screen, ring and all.
    void screenCleared();
}
