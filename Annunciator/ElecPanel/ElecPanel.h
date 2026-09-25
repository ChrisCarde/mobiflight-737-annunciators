#pragma once

#include <Arduino.h>

/* **********************************************************************************
    AC & DC electrical metering panel -- 320x240 landscape, the upper half of the real
    737NG/MAX unit on the forward overhead.

    A two-line, 12-character green 5x7 dot-matrix LED window: three right-aligned 4-character
    fields per line, DC AMPS / (blank) / CPS FREQ on top and DC VOLTS / AC AMPS / AC VOLTS
    below. Then the BAT DISCHARGE, TR UNIT and ELEC lamps.

    The sim works out what the window shows from the DC and AC meter selectors, so the panel
    just mirrors the two text lines it is sent (on the PMDG, ELEC_MeterDisplayTop/Bottom).
    The selectors and the MAINT button are physical controls on the real panel; if their
    positions are sent too, the panel names the current selections beneath the lamps.
********************************************************************************** */
namespace ElecPanel
{
    static const uint8_t LAMP_COUNT = 3; // BAT DISCHARGE, TR UNIT, ELEC

    enum : int16_t {
        MSG_LINE_TOP    = 10, // 12 characters: DC AMPS, blank, CPS FREQ
        MSG_LINE_BOTTOM = 11, // 12 characters: DC VOLTS, AC AMPS, AC VOLTS
        MSG_DC_SELECTOR = 12, // 0 STBY PWR .. 7 TEST
        MSG_AC_SELECTOR = 13, // 0 STBY PWR .. 6 TEST
    };

    void init(uint8_t backlightPin);
    void stop();
    void clear(); // lamps off, display blank, no dimming / lamp test / self test
    void set(int16_t messageID, char *payload);
    void update();
}
