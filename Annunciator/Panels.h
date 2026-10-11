#pragma once

#include <Arduino.h>

/* **********************************************************************************
    Every panel this firmware can be, by the custom device type the Connector configures.

    MFCustomDevice runs the one a board is configured as; the boot splash's demo runs each in
    turn. Only one runs at a time, because they share the screen and, for the panels built
    from a lamp table, one engine (LampPanel).
********************************************************************************** */
namespace Panels
{
    struct Ops {
        const char *type; // Info.Type in the panel's device definition
        const char *name; // its Info.Label, in capitals: what the boot splash calls it
        void (*init)(uint8_t backlightPin);
        void (*stop)();
        void (*clear)();
        void (*set)(int16_t messageID, char *payload);
        void (*update)();
    };

    // The names, longest first, for fit checks: the boot splash shows them in big type.
    inline constexpr const char *NAMES[] = {
        "DOOR ANNUNCIATOR", "MASTER CAUTION R", "MASTER CAUTION L", "ELECTRICAL METER",
        "AIR CONDITIONING", "FLIGHT CONTROLS", "IRS ANNUNCIATOR", "IRS DISPLAY UNIT",
        "MASTER CAUTION", "CAUTION + AFDS",
    };

    uint8_t    count();
    const Ops &at(uint8_t index);
    int8_t     find(const char *type); // the index, or -1 for a type this firmware lacks
} // namespace Panels
