#pragma once

#include <Arduino.h>
#include "LampPanel.h"

/* **********************************************************************************
    Flight controls panel lights -- 320x240 landscape, from the top left of the 737-800's
    forward overhead. Nine amber lamps in three clusters, placed as on the real panel:

        FLT CONTROL                              STANDBY HYD
         A              B                        LOW QUANTITY
        LOW PRESSURE   LOW PRESSURE  FEEL DIFF PRESS  LOW PRESSURE
                                     SPEED TRIM FAIL  STBY RUD ON
                                     MACH TRIM FAIL
                                     AUTO SLAT FAIL

    The real panel is portrait, with the unlabelled four-lamp stack below STANDBY HYD's
    three; seven lamps will not stack in 240px, so here the four stand beside the three,
    starting a row lower -- still low and a little left of STANDBY HYD, as on the aircraft.
    The panel's tenth lamp, YAW DAMPER, is left out: the B107 has it.

    Message IDs: 0-1 FLT CONTROL A / B LOW PRESSURE, 2 LOW QUANTITY, 3 (standby) LOW
    PRESSURE, 4 STBY RUD ON, 5 FEEL DIFF PRESS, 6 SPEED TRIM FAIL, 7 MACH TRIM FAIL,
    8 AUTO SLAT FAIL.
********************************************************************************** */
namespace FctlPanel
{
    static const uint8_t LAMP_COUNT = 9;

    extern const LampPanel::Def DEF;

    void init(uint8_t backlightPin);
}
