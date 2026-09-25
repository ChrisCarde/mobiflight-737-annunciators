#pragma once

#include <Arduino.h>

/* **********************************************************************************
    IRS annunciator panel -- 320x240 landscape, arranged like the real 737 Mode Select Unit.

    11 lamps, all the same ~2.7:1 lens: GPS / ILS / GLS across the top, then a 2x2 block
    per IRU -- ALIGN | ON DC over FAULT | DC FAIL. ALIGN lights white (the others amber)
    and supports the 1 Hz blink the aircraft uses to ask for present position.
********************************************************************************** */
namespace IrsPanel
{
    static const uint8_t LAMP_COUNT = 11;

    void init(uint8_t backlightPin);
    void stop();
    void clear(); // every lamp off, no dimming / lamp test / self test
    void set(int16_t messageID, char *payload);
    void update();
}
