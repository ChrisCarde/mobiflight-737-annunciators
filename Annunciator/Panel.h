#pragma once

#include <Arduino.h>

/* **********************************************************************************
    Message IDs shared by every annunciator device.

    MobiFlight reserves the negative IDs:
      -1  MobiFlight stopped / shutting down  (sent WITHOUT a value argument)
      -2  power saving; payload "1" enter, "0" wake
    Everything at 0..n is a lamp and 100+ is configuration.

    The panel is dark (backlight off) until MobiFlight starts sending values, and again on
    Stop (-1), during power saving (-2), and whenever MSG_BUS_POWER says the bus is off.
    Those four are handled once in MFCustomDevice::set(), not per panel.
    This mirrors the convention used by the other MobiFlight TFT custom devices.
********************************************************************************** */
enum : int16_t {
    MSG_STOP            = -1,
    MSG_POWERSAVE       = -2,


    MSG_ROTATION        = 100,
    MSG_BRIGHTNESS      = 101,
    MSG_DIM             = 102,
    MSG_LAMP_TEST       = 103,
    MSG_SELF_TEST       = 104,
    MSG_BUS_POWER       = 105, // "1" powered, "0" not -- for a bus voltage bound directly
    MSG_TOUCH_CAL       = 106, // "1": run the four-point touch calibration (touch panels)
    MSG_BUS_UNPOWERED   = 107, // the same as 105, inverted: "1" not powered, "0" powered
};

/* Why two bus messages. The Connector's Test Mode sends each output its test value as it is
   (bypassing the modifiers), and the end of every test sends "0". Sent to 105, that "0"
   darkens the panel mid-test. The profiles therefore bind 107 -- "bus off" -- where "0" means
   "powered", and the panel stays lit through Test Mode. (A row's own Test button sends "1":
   on the bus row that shows the dark, unpowered state, as it should.) 105 stays for binding
   a bus voltage directly. */

/* Lamp payloads: "0" dark, "1" lit, "2" lit and blinking at 1 Hz.

   A few lights show two colours, because the aircraft uses the colour to say how bad it
   is: the AFDS A/P light is red for an autopilot disconnect and amber for a caution. Those
   take "3" and "4" as the same two states again in the second colour. A lamp with only one
   colour treats them as "1" and "2", so a binding cannot break a panel by sending them. */
enum LampState : uint8_t {
    LS_OFF       = 0,
    LS_ON        = 1,
    LS_BLINK     = 2,
    LS_ALT       = 3, // lit, in the lamp's alternate colour
    LS_ALT_BLINK = 4,
};

/* Blink cadence from the design: `lampblink 1s steps(1,end) infinite`, 50% duty. */
static const uint32_t BLINK_PERIOD_MS = 1000;

/* The design's blinked-off phase is opacity .12 rather than fully dark. */
static const uint8_t BLINK_OFF_ALPHA = 31; // 0.12 * 255
