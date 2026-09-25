#pragma once

#include <stdint.h>

#include "Layout.h"

/* **********************************************************************************
    Geometry and glyphs of the IRS display unit screen -- data only, so the host preview
    in tools/preview draws exactly what the firmware draws. See IsduPanel.h for the
    picture.

    This is the most position-sensitive layout in the project: seven-segment cells, the
    dots between them, two knobs with their legends at the real detent angles, and a
    keypad. The knob angles are facts about a 737 and never scale; everything else is
    driven from the sizes in the per-screen block below.
********************************************************************************** */
namespace IsduLayout
{
    constexpr int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;

    /* ---- Display strip ----------------------------------------------------------
       One dark window holding both displays: 6 cells on the left (latitude, track,
       heading, wind direction) and 7 on the right (longitude, speeds, status). The cells
       are separate dark tubes as on the real unit, about 1.9:1; the digits inside keep
       its 2:1. 13 cells plus the gap between the displays fill 300px at a 21px pitch. */
#if ANNUN_PANEL_RES == 480
    constexpr int16_t HEADER_Y = 15; // "IRS DISPLAY", cap centre
    constexpr int16_t WIN_X = 12, WIN_Y = 30, WIN_W = 456, WIN_H = 92;
    constexpr int16_t CELL_W = 28, CELL_H = 60, CELL_PITCH = 31, CELL_Y = 46;
    constexpr int16_t LEFT_X = 24, RIGHT_X = 237; // first cell of each display
#else
    constexpr int16_t HEADER_Y = 11;
    constexpr int16_t WIN_X = 8, WIN_Y = 22, WIN_W = 304, WIN_H = 68;
    constexpr int16_t CELL_W = 19, CELL_H = 44, CELL_PITCH = 21, CELL_Y = 34;
    constexpr int16_t LEFT_X = 16, RIGHT_X = 158;
#endif
    constexpr uint8_t LEFT_CELLS = 6, RIGHT_CELLS = 7;

    constexpr int16_t cellX(uint8_t i) // 0..12: left cells, then right
    {
        return (int16_t)(i < LEFT_CELLS ? LEFT_X + i * CELL_PITCH : RIGHT_X + (i - LEFT_CELLS) * CELL_PITCH);
    }

    /* The glyph box inside a cell, the segment width, and how far a segment's end stops
       short of the box. SEG_INSET is what the segment rectangles below are written in
       terms of, so the digit keeps its shape at either size instead of being re-tuned by
       hand; the 1px overlaps at the a/b, b/g, g/c and c/d joints are what make a digit
       read as continuous rather than as seven separate bars. */
#if ANNUN_PANEL_RES == 480
    constexpr int16_t GLYPH_X = 4, GLYPH_Y = 7, GLYPH_W = 19, GLYPH_H = 46, SEG_T = 4;
    constexpr int8_t  SEG_INSET = 3;
#else
    constexpr int16_t GLYPH_X = 3, GLYPH_Y = 5, GLYPH_W = 13, GLYPH_H = 34, SEG_T = 3;
    constexpr int8_t  SEG_INSET = 2;
#endif

    /* Segments: bits 0-6 are the usual a-g; 7-9 the diagonals of the letter cells -- the
       left display's first cell (N, S) has one from top left to bottom right, the right
       display's first (E, W) a V from the bottom corners to the middle. */
    enum : uint16_t {
        SEG_A = 1 << 0, SEG_B = 1 << 1, SEG_C = 1 << 2, SEG_D = 1 << 3,
        SEG_E = 1 << 4, SEG_F = 1 << 5, SEG_G = 1 << 6,
        SEG_TL_BR = 1 << 7, SEG_BL_MID = 1 << 8, SEG_MID_BR = 1 << 9,
        SEG_DIGIT = 0x7F,
    };

    // Straight segments as rectangles in the glyph box: x, y, w, h.
    struct Rect { int8_t x, y, w, h; };
    constexpr int8_t HALF = GLYPH_H / 2; // 17
    constexpr int8_t SI = SEG_INSET, SW = (int8_t)(GLYPH_W - 2 * SEG_INSET), SH = (int8_t)(HALF - SEG_INSET - 1);
    constexpr Rect SEG_RECT[7] = {
        {SI, 0, SW, SEG_T},                                          // a
        {(int8_t)(GLYPH_W - SEG_T), SI, SEG_T, SH},                  // b
        {(int8_t)(GLYPH_W - SEG_T), (int8_t)(HALF + 1), SEG_T, SH},  // c
        {SI, (int8_t)(GLYPH_H - SEG_T), SW, SEG_T},                  // d
        {0, (int8_t)(HALF + 1), SEG_T, SH},                          // e
        {0, SI, SEG_T, SH},                                          // f
        {SI, (int8_t)(HALF - 1), SW, SEG_T},                         // g
    };
    // Diagonals as lines in the glyph box: x0, y0, x1, y1.
    struct Line { int8_t x0, y0, x1, y1; };
    constexpr int8_t LY0 = (int8_t)(SEG_INSET + 1), LY1 = (int8_t)(GLYPH_H - 2 * SEG_INSET);
    constexpr int8_t LMID = (int8_t)(HALF + SEG_INSET);
    constexpr Line SEG_LINE[3] = {
        {SEG_T, LY0, (int8_t)(GLYPH_W - SEG_T - 1), LY1},                       // top left to bottom right
        {SEG_T, LY1, (int8_t)(GLYPH_W / 2), LMID},                              // bottom left to middle
        {(int8_t)(GLYPH_W / 2), LMID, (int8_t)(GLYPH_W - SEG_T - 1), LY1},      // middle to bottom right
    };

    // What each cell shows unlit (a ghost of every segment it has).
    constexpr uint16_t cellSegments(uint8_t i)
    {
        return (uint16_t)(i == 0 ? SEG_DIGIT | SEG_TL_BR
                        : i == LEFT_CELLS ? SEG_DIGIT | SEG_BL_MID | SEG_MID_BR
                        : SEG_DIGIT);
    }

    // The characters the PMDG sends. Anything else shows blank.
    constexpr uint16_t glyph(char c)
    {
        return c == '0' ? SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
             : c == '1' ? SEG_B | SEG_C
             : c == '2' ? SEG_A | SEG_B | SEG_G | SEG_E | SEG_D
             : c == '3' ? SEG_A | SEG_B | SEG_G | SEG_C | SEG_D
             : c == '4' ? SEG_F | SEG_G | SEG_B | SEG_C
             : (c == '5' || c == 'S') ? SEG_A | SEG_F | SEG_G | SEG_C | SEG_D
             : c == '6' ? SEG_A | SEG_F | SEG_G | SEG_E | SEG_D | SEG_C
             : c == '7' ? SEG_A | SEG_B | SEG_C
             : c == '8' ? SEG_DIGIT
             : c == '9' ? SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
             : c == '-' ? SEG_G
             : c == 'N' ? SEG_E | SEG_F | SEG_B | SEG_C | SEG_TL_BR
             : c == 'E' ? SEG_A | SEG_F | SEG_G | SEG_E | SEG_D
             : c == 'W' ? SEG_E | SEG_F | SEG_B | SEG_C | SEG_BL_MID | SEG_MID_BR
             : c == 'H' ? SEG_F | SEG_E | SEG_B | SEG_C | SEG_G
             : 0;
    }

    /* The dots are lamps in the bezel, not characters: per display, a raised degree mark
       (after the third cell on the left, the fourth on the right), a decimal point before
       the last cell, and a raised mark after the last cell. */
    struct Dot { int16_t x, y; };
#if ANNUN_PANEL_RES == 480
    constexpr int16_t DOT_R = 3;
    constexpr int16_t DOT_HI = CELL_Y - 7, DOT_LO = CELL_Y + CELL_H + 6;
#else
    constexpr int16_t DOT_R = 2;
    constexpr int16_t DOT_HI = CELL_Y - 5, DOT_LO = CELL_Y + CELL_H + 4;
#endif
    constexpr Dot DOTS[6] = {
        {(int16_t)(LEFT_X + 3 * CELL_PITCH - 1), DOT_HI},                 // left degree
        {(int16_t)(LEFT_X + 5 * CELL_PITCH - 1), DOT_LO},                 // left decimal
        {(int16_t)(LEFT_X + 5 * CELL_PITCH + CELL_W + 2), DOT_HI},        // left minute
        {(int16_t)(RIGHT_X + 4 * CELL_PITCH - 1), DOT_HI},                // right degree
        {(int16_t)(RIGHT_X + 6 * CELL_PITCH - 1), DOT_LO},                // right decimal
        {(int16_t)(RIGHT_X + 6 * CELL_PITCH + CELL_W + 2), DOT_HI},       // right minute
    };

    /* ---- Lower face: the knobs on the left, the keypad on the right, as on the unit --- */
#if ANNUN_PANEL_RES == 480
    constexpr int16_t KNOBS_X = 12, KNOBS_W = 218, LOWER_Y = 130;
#else
    constexpr int16_t KNOBS_X = 8, KNOBS_W = 144, LOWER_Y = 96;
#endif
    constexpr int16_t LOWER_H = PANEL_H - LOWER_Y;

    // DSPL SEL: legends round the knob at the real positions, 0 degrees to the right,
    // counter-clockwise. The PMDG's selector runs 0 TEST .. 4 HDG/STS.
#if ANNUN_PANEL_RES == 480
    constexpr int16_t SEL_CX = 107, SEL_CY = 205, SEL_R = 17, SEL_LEGEND_R = 42, SEL_TITLE_Y = 142;
#else
    constexpr int16_t SEL_CX = 70, SEL_CY = 152, SEL_R = 12, SEL_LEGEND_R = 30, SEL_TITLE_Y = 106;
#endif
    constexpr uint8_t SEL_POSITIONS = 5;
    constexpr const char *SEL_NAME[SEL_POSITIONS] = {"TEST", "TK/GS", "PPOS", "WIND", "HDG/STS"};
    constexpr int16_t SEL_ANGLE[SEL_POSITIONS] = {180, 145, 110, 70, 30};

#if ANNUN_PANEL_RES == 480
    constexpr int16_t SYS_CX = 107, SYS_CY = 291, SYS_R = 14, SYS_LEGEND_R = 34, SYS_TITLE_Y = 252;
#else
    constexpr int16_t SYS_CX = 70, SYS_CY = 218, SYS_R = 10, SYS_LEGEND_R = 24, SYS_TITLE_Y = 186;
#endif
    constexpr const char *SYS_NAME[2] = {"L", "R"};
    constexpr int16_t SYS_ANGLE[2] = {125, 55};

    // Touch: a press this close to a knob's centre picks the nearest position.
#if ANNUN_PANEL_RES == 480
    constexpr int16_t SEL_REACH = 68, SYS_REACH = 41;
#else
    constexpr int16_t SEL_REACH = 50, SYS_REACH = 30;
#endif

    // Keypad: 3 x 4. The letter keys carry their letter over the digit.
#if ANNUN_PANEL_RES == 480
    constexpr int16_t KEY_X = 238, KEY_Y = 132, KEY_W = 68, KEY_H = 40, KEY_GAP_X = 7, KEY_GAP_Y = 8;
#else
    constexpr int16_t KEY_X = 158, KEY_Y = 98, KEY_W = 48, KEY_H = 30, KEY_GAP_X = 5, KEY_GAP_Y = 6;
#endif
    constexpr uint8_t KEYS = 12, KEY_ENT = 9, KEY_CLR = 11;
    constexpr int16_t keyX(uint8_t k) { return (int16_t)(KEY_X + (k % 3) * (KEY_W + KEY_GAP_X)); }
    constexpr int16_t keyY(uint8_t k) { return (int16_t)(KEY_Y + (k / 3) * (KEY_H + KEY_GAP_Y)); }
    constexpr const char *KEY_TOP[KEYS] = {"1", "N", "3", "W", "H", "E", "7", "S", "9", "ENT", "0", "CLR"};
    constexpr const char *KEY_BOT[KEYS] = {"", "2", "", "4", "5", "6", "", "8", "", "", "", ""};
    // ENT and CLR each have a pair of white cue lights under the legend.
#if ANNUN_PANEL_RES == 480
    constexpr int16_t CUE_DY = 31, CUE_DX = 7, CUE_R = 3;
#else
    constexpr int16_t CUE_DY = 23, CUE_DX = 5, CUE_R = 2;
#endif

    static_assert(cellX(12) + CELL_W + 6 <= WIN_X + WIN_W, "right display runs out of the window");
    static_assert(DOTS[5].x + DOT_R < WIN_X + WIN_W, "minute mark runs out of the window");
    static_assert(keyX(2) + KEY_W <= PANEL_W - 8, "keypad runs off the panel");
    static_assert(keyY(11) + KEY_H <= PANEL_H - 2, "keypad runs off the panel");
    static_assert(KNOBS_X + KNOBS_W < KEY_X, "knobs run into the keypad");
    static_assert(GLYPH_X + GLYPH_W <= CELL_W && GLYPH_Y + GLYPH_H <= CELL_H,
                  "the glyph box does not fit its cell");
    static_assert(cellX(LEFT_CELLS - 1) + CELL_W < RIGHT_X, "the two displays overlap");
    static_assert(CELL_Y + CELL_H <= WIN_Y + WIN_H, "cells run out of the window vertically");

    /* The knob resolver takes any press within SEL_REACH of DSPL SEL's centre, and within
       SYS_REACH of SYS DSPL's. Those two circles must not overlap, or the upper knob
       swallows presses meant for the lower one -- which is the invariant that quietly
       breaks the moment somebody retunes a radius. */
    static_assert(SEL_CY + SEL_R < SYS_CY - SYS_REACH, "the two knobs' touch areas overlap");

    /* The keypad is the tightest touch target in the project and cannot be made bigger:
       four rows of keys under a display that needs its own height. It is checked on pitch
       rather than on key size, because the zones are expanded into the gaps -- there is no
       dead space between keys, so the pitch is what a finger actually gets. That lands at
       about 7mm on both screens, under the 9mm usually wanted for capacitive touch; these
       are fixed targets with a press ring for feedback, which is the forgiving case. */
    static_assert(KEY_H + KEY_GAP_Y >= LY::MIN_TOUCH_PX, "keypad rows are too close to press");
    static_assert(KEY_W + KEY_GAP_X >= LY::MIN_TOUCH_PX, "keypad columns are too close to press");
}
