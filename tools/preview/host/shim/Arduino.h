#pragma once
/* Just enough Arduino for the panel sources to compile on a Mac.

   The host preview builds the real firmware rendering code -- PanelGfx, LampPanel and the
   nine panels -- against a PNG canvas, so a preview is the firmware's own pixels rather
   than a second implementation that drifts. Everything here is what those files touch and
   nothing more; if a new include needs something, add it here rather than #ifdef-ing the
   firmware. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>

#define PROGMEM
#define pgm_read_byte(addr)  (*(const uint8_t *)(addr))
#define pgm_read_word(addr)  (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define F(str) (str)

// Arduino-ESP32 brings in the std templates rather than macros, so the firmware can write
// max<int16_t>(a, b). Matching that is what lets these sources compile unchanged here.
using std::max;
using std::min;
#ifndef constrain
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

// The preview drives time explicitly so blink phases and the settle delay are reproducible:
// see hostClock() in main.cpp. Never wall-clock, or golden images would not be stable.
uint32_t millis();
void     hostSetMillis(uint32_t ms);

// Places a synthetic finger for the "press" scene; see HostStubs.cpp.
void hostSetTouch(int16_t x, int16_t y);

inline void analogWrite(uint8_t, int) {}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
#define OUTPUT 1
#define HIGH   1
#define LOW    0
