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

    The touch variant (ANNUN_FCTL_SW) is the same panel with the FLT CONTROL A and B switches
    drawn under their LOW PRESSURE lights, one column each, as on the aircraft. Each is a
    three-position toggle, STBY RUD at the bottom, OFF in the middle, ON at the top -- the
    order of PMDG's FCTL_FltControl_Sw -- and pressing the upper half of a toggle steps the
    switch up one position and the lower half steps it down one, like pushing a lever.
    Nothing moves on the screen until the sim says the switch has moved. Its extra message
    IDs: 9 FLT CONTROL A switch, 10 FLT CONTROL B switch, "0" STBY RUD, "1" OFF, "2" ON.
********************************************************************************** */
namespace FctlPanel
{
    static const uint8_t LAMP_COUNT = 9;

    static const uint8_t TOGGLE_COUNT = 2;

    extern const LampPanel::Def DEF;
    extern const LampPanel::Def DEF_SW; // with the switches

    void init(uint8_t backlightPin);
    void initSwitches(uint8_t backlightPin);
}
