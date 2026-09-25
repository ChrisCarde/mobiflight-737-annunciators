#pragma once

#include <Arduino.h>
#include "PanelGfx.h"
#include "Theme.h"
#include "TouchZones.h"

/* **********************************************************************************
    A panel that is lamps and nothing else -- doors, the glareshield master caution units,
    and the like -- described by a table and run by this one engine.

    A panel is a list of lamps, each with its legend, position, size, lit colour and kind
    (see LampKind); some static chrome (a header, face plates, printed labels); optionally
    touch zones; and a set of demo states for the self test. The lamp's index in the table
    is its MobiFlight message ID; payloads are LampState ("0" / "1" / "2" blink).

    A touch zone sends a MobiFlight button event named after it (see TouchZones.h); the
    Connector binds it like any button, by name. Pressing never changes a lamp by itself;
    the lamp follows the sim.
********************************************************************************** */
namespace LampPanel
{
    static const uint8_t MAX_LAMPS = 32; // lamp state is kept as bitmasks

    struct Lamp {
        const char *l1;  // legend, top line
        const char *l2;  // second line, or ""
        const char *sub; // small print under the legend (PUSH TO RESET), or nullptr
        int16_t     x, y, w, h;
        LampMode    colour; // when lit
        LampKind    kind;
        PanelFont   font;   // for l1 / l2; `sub` is always FONT_TINY
        LampMode    alt;    // the colour for LS_ALT; omit for a lamp with only one
    };

    // A plate drawn once behind lamps, e.g. the six-pack's shared black face.
    struct Plate {
        int16_t  x, y, w, h;
        uint32_t fill, edge;
        uint8_t  radius; // corner radius; 0 means the default 3. w/2 makes a round button
    };

    // Printed text on the panel, FONT_SMALL, letter-spaced like the headers.
    struct Label {
        const char *text;
        int16_t     x, yMid; // x is the left edge, or the centre if `centred`
        uint32_t    colour;
        bool        centred;
    };

    using Zone = TouchZones::Zone;

    struct Def {
        const char     *header; // spaced header with a rule after it, or nullptr
        const Lamp     *lamps;
        uint8_t         lampCount;
        const Plate    *plates;
        uint8_t         plateCount;
        const Label    *labels;
        uint8_t         labelCount;
        const Zone     *zones;
        uint8_t         zoneCount;
        const uint32_t *demo; // lit-lamp bitmasks cycled by the self test
        uint8_t         demoCount;
    };

    void init(const Def &def, uint8_t backlightPin);
    void stop();
    void clear(); // every lamp off, no dimming / lamp test / self test
    void set(int16_t messageID, char *payload);
    void update();
}
