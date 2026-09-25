#pragma once

#include <Arduino.h>

/* **********************************************************************************
    IRS display unit (ISDU) -- 320x240 landscape, laid out like the real unit on the aft
    overhead:

        ---------------- IRS DISPLAY ----------------
        [ N 4 7 ° 3 2 . 4 ' ]   [ W 1 2 2 ° 1 2 . 3 ' ]      the two segmented displays
          DSPL SEL                  1    N 2    3
        TK/GS PPOS WIND               W 4  H 5  E 6
        TEST   (knob)   HDG/STS        7   S 8    9
          SYS DSPL  L R               ENT    0   CLR

    The displays mirror the PMDG's own strings (IRS_DisplayLeft / Right): character n in
    cell n, as the PMDG's virtual cockpit fills them. The dots are one flag, as the PMDG
    has it. The knobs show where DSPL SEL and SYS DSPL are set.

    The keypad works: each key is a touch button ("ISDU 1" .. "ISDU 9", "ISDU 0",
    "ISDU ENT", "ISDU CLR") that the profiles bind to the PMDG's ISDU keyboard events, and
    ENT and CLR carry their cue lights. So do the knobs: a press on one of DSPL SEL's
    legends -- or anywhere round the knob, taking the nearest -- sends "ISDU DSPL TEST",
    "ISDU DSPL TK GS", "ISDU DSPL PPOS", "ISDU DSPL WIND" or "ISDU DSPL HDG STS", and
    SYS DSPL's "ISDU SYS DSPL L" / "R"; the profile turns the knob to that position.

    Message IDs:
      0  left display string   (IRS_DisplayLeft, up to 6 characters)
      1  right display string  (IRS_DisplayRight, up to 7)
      2  dots: 0 dark, 1 lit   (IRS_DisplayShowsDots)
      3  DSPL SEL: 0 TEST, 1 TK/GS, 2 PPOS, 3 WIND, 4 HDG/STS
      4  SYS DSPL: 0 L, 1 R
      5  ENT cue lights, 6 CLR cue lights: 0 dark, anything else lit
********************************************************************************** */
namespace IsduPanel
{
    enum : int16_t {
        MSG_LEFT     = 0,
        MSG_RIGHT    = 1,
        MSG_DOTS     = 2,
        MSG_DSPL_SEL = 3,
        MSG_SYS_DSPL = 4,
        MSG_ENT_CUE  = 5,
        MSG_CLR_CUE  = 6,
    };

    void init(uint8_t backlightPin);
    void stop();
    void clear(); // displays blank, knobs home, no dimming / lamp test / self test
    void set(int16_t messageID, char *payload);
    void update();
}
