#pragma once

#include <Arduino.h>
#include "LampPanel.h"

/* **********************************************************************************
    Master caution system -- the glareshield lights, 320x240 landscape, three ways:

    On the aircraft each pilot has a red FIRE WARN push-light, an amber MASTER CAUTION
    push-light and a six-pack (the system annunciator panel: six amber captions on one black
    face, arranged like the overhead systems they point to -- FUEL bottom left, and so on).

    LEFT   the captain's unit as on the aircraft, outboard to inboard
               FIRE WARN  MASTER CAUTION  FLT CONT  ELEC
                                          IRS       APU
                                          FUEL      OVHT/DET
           messages 0 FIRE WARN, 1 MASTER CAUTION, 2..7 the six-pack

    RIGHT  the first officer's, its mirror image
               ANTI-ICE  ENG        MASTER CAUTION  FIRE WARN
               HYD       OVERHEAD
               DOORS     AIR COND
           messages 0 FIRE WARN, 1 MASTER CAUTION, 2..7 the six-pack

    BOTH   one screen for both: the two sides' push-lights always light together, so one
           of each, beside both six-packs -- the captain's on top, the F/O's below
               FIRE WARN  MASTER CAUTION  FLT CONT  ELEC
                                          IRS       APU
                                          FUEL      OVHT/DET
                                          ANTI-ICE  ENG
                                          HYD       OVERHEAD
                                          DOORS     AIR COND
           messages 0 FIRE WARN, 1 MASTER CAUTION, 2..7 captain's, 8..13 F/O's six-pack

    Six-pack captions are numbered row by row, left column first.

    Every one of them is a button too, as on the aircraft: FIRE WARN silences the bell,
    MASTER CAUTION resets, and a six-pack recalls every caution. The names MobiFlight sees
    are in the Zone tables in McsPanel.cpp.
********************************************************************************** */
namespace McsPanel
{
    static const uint8_t SIDE_LAMP_COUNT = 8;
    static const uint8_t BOTH_LAMP_COUNT = 14;
    // The same again plus the three autoflight lights, which keep the message IDs above
    // the ones ANNUN_MCS_BOTH already uses: 14 A/P, 15 A/T, 16 FMC.
    static const uint8_t BOTH_AFDS_LAMP_COUNT = 17;

    extern const LampPanel::Def LEFT;
    extern const LampPanel::Def RIGHT;
    extern const LampPanel::Def BOTH;

    void initLeft(uint8_t backlightPin);
    void initRight(uint8_t backlightPin);
    void initBoth(uint8_t backlightPin);
    void initBothAfds(uint8_t backlightPin);
}
