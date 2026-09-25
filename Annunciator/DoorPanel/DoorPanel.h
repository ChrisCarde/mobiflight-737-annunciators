#pragma once

#include <Arduino.h>
#include "LampPanel.h"

/* **********************************************************************************
    Door annunciator panel -- 320x240 landscape, arranged like the real 737-800 panel.

    12 amber door lamps in four columns, placed so a lamp's position on screen matches its
    position on the airframe: left-side doors in column two, right-side in column three,
    AIR STAIR and EQUIP outboard left, the cargo doors outboard right. Lamp 0, GRD PWR, is
    not a door: it stands apart at the bottom left and lights blue, because it is an
    advisory rather than a caution.

    Each door lamp is also a touch button named after its legend ("FWD ENTRY", ...), so a
    press can open or close that door in the sim -- not something the real panel does, but
    quicker than the CDU or the EFB.
********************************************************************************** */
namespace DoorPanel
{
    static const uint8_t LAMP_COUNT = 13;

    extern const LampPanel::Def DEF;

    void init(uint8_t backlightPin);
}
