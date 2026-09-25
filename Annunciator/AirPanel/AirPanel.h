#pragma once

#include <Arduino.h>
#include "LampPanel.h"

/* **********************************************************************************
    Air conditioning, bleed air and cabin pressurisation lights -- 320x240 landscape, in the
    order they run down the right-hand column of the 737-800's forward overhead:

        ZONE TEMP  ZONE TEMP  ZONE TEMP        air temperature panel (trim-air aircraft)
        CONT CAB   FWD CAB    AFT CAB
        DUAL BLEED  RAM DOOR L  (empty)  RAM DOOR R      light strip over the bleed panel
             PACK          TRIP     PACK                  bleed air panel: a stack either
             WING-BODY OVHT         WING-BODY OVHT        side of the TRIP RESET button
             BLEED TRIP OFF  RESET  BLEED TRIP OFF
        AUTO FAIL  OFF SCHED DESCENT  ALTN  MANUAL        top of the cabin pressure panel

    Amber, except RAM DOOR FULL OPEN (blue advisory) and ALTN / MANUAL (green). Gauges and
    switches are left out; the FLT ALT / LAND ALT windows are already on the B107.

    Message IDs: 0-2 ZONE TEMP CONT / FWD / AFT CAB, 3 DUAL BLEED, 4-5 RAM DOOR L / R,
    6-8 left PACK / WING-BODY OVERHEAT / BLEED TRIP OFF, 9-11 the same on the right,
    12 AUTO FAIL, 13 OFF SCHED DESCENT, 14 ALTN, 15 MANUAL.
********************************************************************************** */
namespace AirPanel
{
    static const uint8_t LAMP_COUNT = 16;

    extern const LampPanel::Def DEF;

    void init(uint8_t backlightPin);
}
