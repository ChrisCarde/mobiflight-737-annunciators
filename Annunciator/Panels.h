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
        void (*init)(uint8_t backlightPin);
        void (*stop)();
        void (*clear)();
        void (*set)(int16_t messageID, char *payload);
        void (*update)();
    };

    uint8_t    count();
    const Ops &at(uint8_t index);
    int8_t     find(const char *type); // the index, or -1 for a type this firmware lacks
} // namespace Panels
