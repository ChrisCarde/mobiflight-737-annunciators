#pragma once

#include "Board.h"
#include "fonts/FontSet.h"

/* **********************************************************************************
    The design language shared by the panels: page margin, lens size, gutters, and the
    numbers that decide whether a thing can be pressed with a finger.

    Why there are two sets rather than one scale factor. 320x240 to 480x320 is x1.5 across
    but only x1.333 down, and text cannot scale continuously -- 11px goes to 15, not to
    16.5. Height is therefore the binding axis, and it is spent on lamps; the extra width
    becomes gutter and margin, never a stretched lens. A 737 panel is a metal plate with
    lenses set into it, and more bezel between them reads as more like the aircraft, not
    less.

    Both screens are the same physical height: a 3.2in 240x320 module and a 3.5in 320x480
    module are both about 49mm x 73mm of glass, so the 3.5in one has 1.33x the pixels per
    millimetre, not 1.5x. That is the whole reason the 480 numbers cluster around x1.34:
    the panel is the same size in the cockpit and simply drawn with more pixels. Where a
    lamp does grow beyond that -- the door lenses, the six-pack -- it grows because the
    layout had width to spare, and it is worth saying that those are about 7% larger in
    the hand, not 50%.

    Panel-specific numbers stay in their own panel next to the paragraph that explains
    them. Only what several panels share lives here. Both branches must define the same
    names: the panels use them, so the compiler enforces it.

    PX_PER_MM is used for touch-target checks. Measure it on the real modules before
    trusting it to the millimetre.
********************************************************************************** */

namespace LY
{

constexpr int16_t PANEL_W = Board::PANEL_W;
constexpr int16_t PANEL_H = Board::PANEL_H;

#if ANNUN_PANEL_RES == 480

constexpr int16_t MARGIN = 14; // page margin

// The door/flight-control lens. 101x47 holds RIGHT FWD -- 82px of ink at 15px -- with 19px
// to spare, where the 320 lens had 9. About 7% physically larger, twice the pixels.
constexpr int16_t LENS_W = 101, LENS_H = 47;
constexpr int16_t LENS_H_TIGHT = 42; // the air panel: six rows plus captions, no room for 47

constexpr int16_t COL_GAP = 7, ROW_GAP = 6;
constexpr int16_t SEAM    = 3; // lamps sharing a bezel on the aircraft

constexpr int16_t HEADER_CAP_Y = 12; // cap centre of a panel header
constexpr int16_t HEADER_BAND  = 18; // header band height
constexpr int16_t HEADER_GAP   = 9;  // header to the first row
constexpr int16_t PAD_BOTTOM   = 11;

constexpr float PX_PER_MM = 6.54f; // 3.5in 320x480, about 49mm x 73mm of active area

constexpr int16_t HIT_SLOP   = 3; // a press this close outside a zone still counts
constexpr int16_t RING_INSET = 3; // press ring, px outside the zone

#else

constexpr int16_t MARGIN = 10;

constexpr int16_t LENS_W = 71, LENS_H = 35;
constexpr int16_t LENS_H_TIGHT = 31;

constexpr int16_t COL_GAP = 5, ROW_GAP = 4;
constexpr int16_t SEAM    = 2;

constexpr int16_t HEADER_CAP_Y = 9;
constexpr int16_t HEADER_BAND  = 13;
constexpr int16_t HEADER_GAP   = 7;
constexpr int16_t PAD_BOTTOM   = 8;

constexpr float PX_PER_MM = 4.90f; // 3.2in 240x320, about 49mm x 65mm of active area

constexpr int16_t HIT_SLOP   = 2;
constexpr int16_t RING_INSET = 2;

#endif

// Smallest touch target worth drawing. 9mm is the usual guidance for a capacitive panel;
// 7mm is what a 3.5in screen can actually give a four-row keypad under a display, and
// these are fixed targets with a press ring for feedback, which is the forgiving case.
// The ISDU keypad sits at this limit deliberately -- see IsduLayout.h.
constexpr int16_t MIN_TOUCH_PX = (int16_t)(7.0f * PX_PER_MM);

// Does a legend fit the box it is drawn in? These turn the hand-measured widths in the
// panel comments into something the compiler checks, at whatever font size the screen
// happens to use.
constexpr int16_t ink(PanelFont f, const char *s) { return inkWidthOf(fontOf(f), s); }
constexpr bool    fits(PanelFont f, const char *s, int16_t box, int16_t pad = 2)
{
    return ink(f, s) + 2 * pad <= box;
}

// The lens is the one shape whose proportions are a recognisable feature of the real
// hardware, so it may not be stretched to fill a screen. 10% covers the rounding that
// integer pixel sizes force; anything more is a different design.
static_assert(LENS_W * 100 <= LENS_H * 203 * 110 / 100 &&
                  LENS_W * 100 >= LENS_H * 203 * 90 / 100,
              "the lens has drifted from the aircraft's 2:1");

static_assert(PANEL_W >= 320 && PANEL_H >= 240, "layout assumes at least 320x240");

} // namespace LY
