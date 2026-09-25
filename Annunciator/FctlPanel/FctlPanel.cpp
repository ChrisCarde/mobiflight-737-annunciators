#include "FctlPanel.h"
#include "Layout.h"

namespace FctlPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Door-sized 71x35 lenses. Lamps that share a bezel on the aircraft -- the LOW PRESSURE
   pair, the STANDBY HYD three, the unlabelled four -- stand 2px apart on a dark plate, so
   each cluster reads as one unit, as the real ones do.
---------------------------------------------------------------------------------- */
static constexpr int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;
static constexpr int16_t LENS_W = LY::LENS_W, LENS_H = LY::LENS_H, SEAM = LY::SEAM;
static constexpr int16_t PITCH_Y = LENS_H + SEAM; // 37 / 50

#if ANNUN_PANEL_RES == 480
static constexpr int16_t CLUSTER_GAP = 9;  // between the four-stack and STANDBY HYD
static constexpr int16_t TOP_Y       = 53; // first lamp row, under the captions
#else
static constexpr int16_t CLUSTER_GAP = 7;
static constexpr int16_t TOP_Y       = 40;
#endif

static constexpr int16_t PAIR_X = LY::MARGIN;                            // FLT CONTROL A; B beside it
static constexpr int16_t HYD_X  = PANEL_W - LY::MARGIN - LENS_W;         // 239 / 365, STANDBY HYD
static constexpr int16_t FOUR_X = HYD_X - CLUSTER_GAP - LENS_W;          // 161 / 255, the unlabelled four
static constexpr int16_t rowY(int r) { return (int16_t)(TOP_Y + r * PITCH_Y); } // 40 77 114 151 188

static constexpr int16_t PAD = LY::SEAM; // plate margin round a cluster
static constexpr uint32_t PLATE_FILL = 0x08090A;

static_assert(PAIR_X + 2 * LENS_W + SEAM + PAD < FOUR_X - PAD, "LOW PRESSURE pair runs into the four-stack");
static_assert(FOUR_X + LENS_W + PAD < HYD_X - PAD, "the two stacks' plates overlap");
static_assert(rowY(4) + LENS_H + PAD <= PANEL_H, "the four-stack runs off the panel");
static_assert(LY::fits(FONT_SMALL, "SPEED TRIM", LENS_W, 3), "fctl lens too narrow for SPEED TRIM");
static_assert(LY::fits(FONT_SMALL, "DIFF PRESS", LENS_W, 3), "fctl lens too narrow for DIFF PRESS");

#define LENS(l1, l2, x, y) {l1, l2, nullptr, x, y, LENS_W, LENS_H, LAMP_AMBER, KIND_LENS, FONT_SMALL}

static const LampPanel::Lamp LAMPS[LAMP_COUNT] = {
    LENS("LOW", "PRESSURE", PAIR_X, rowY(0)),
    LENS("LOW", "PRESSURE", (int16_t)(PAIR_X + LENS_W + SEAM), rowY(0)),
    LENS("LOW", "QUANTITY", HYD_X, rowY(0)),
    LENS("LOW", "PRESSURE", HYD_X, rowY(1)),
    LENS("STBY", "RUD ON", HYD_X, rowY(2)),
    LENS("FEEL", "DIFF PRESS", FOUR_X, rowY(1)),
    LENS("SPEED TRIM", "FAIL", FOUR_X, rowY(2)),
    LENS("MACH TRIM", "FAIL", FOUR_X, rowY(3)),
    LENS("AUTO SLAT", "FAIL", FOUR_X, rowY(4)),
};
#undef LENS

// A plate behind each cluster, PAD px round its lamps.
#define PLATE(x, y, cols, rows)                                                                  \
    {(int16_t)((x) - PAD), (int16_t)((y) - PAD), (int16_t)((cols) * LENS_W + ((cols) - 1) * SEAM + 2 * PAD), \
     (int16_t)((rows) * LENS_H + ((rows) - 1) * SEAM + 2 * PAD), PLATE_FILL, Theme::Bezel}
static const LampPanel::Plate PLATES[] = {
    PLATE(PAIR_X, rowY(0), 2, 1),
    PLATE(HYD_X, rowY(0), 1, 3),
    PLATE(FOUR_X, rowY(1), 1, 4),
};
#undef PLATE

/* Printed on the panel. FLT CONTROL's A and B are needed: the pair's legends are the same. */
static constexpr int16_t CAPTION_Y = LY::HEADER_CAP_Y + 5; // cap centre of the top captions
// A and B sit a line below FLT CONTROL: a cap height plus a gap, rather than a fixed 14px.
static constexpr int16_t CAPTION_DY = fontOf(FONT_SMALL).capHeight + 6;
/* STANDBY HYD is a much wider caption than its lens -- 98px of letter-spaced text over a
   71px lamp -- so centring it on the lens would hang it off the right edge of the panel.
   It leans inboard by this much instead; the assert below is what keeps that honest. */
#if ANNUN_PANEL_RES == 480
static constexpr int16_t HYD_LABEL_DX = 24;
#else
static constexpr int16_t HYD_LABEL_DX = 18;
#endif
static_assert(HYD_X + LENS_W / 2 - HYD_LABEL_DX + spacedWidthOf(fontOf(FONT_SMALL), "STANDBY HYD", 2) / 2
                  <= PANEL_W - LY::MARGIN / 2,
              "STANDBY HYD runs off the right of the panel");
static const LampPanel::Label LABELS[] = {
    {"FLT CONTROL", (int16_t)(PAIR_X + LENS_W + SEAM / 2), CAPTION_Y, Theme::HeaderFg, true},
    {"A", (int16_t)(PAIR_X + LENS_W / 2), (int16_t)(CAPTION_Y + CAPTION_DY), Theme::HeaderFg, true},
    {"B", (int16_t)(PAIR_X + LENS_W + SEAM + LENS_W / 2), (int16_t)(CAPTION_Y + CAPTION_DY), Theme::HeaderFg, true},
    {"STANDBY HYD", (int16_t)(HYD_X + LENS_W / 2 - HYD_LABEL_DX), CAPTION_Y, Theme::HeaderFg, true},
};

/* Self test: hydraulic pumps off, a system A loss, standby rudder on, trim failures.
   Bit n is lamp n. */
static const uint32_t DEMO[] = {
    (1u << 0) | (1u << 1),                  // cold aircraft: both LOW PRESSURE
    (1u << 0) | (1u << 5),                  // system A lost: A LOW PRESSURE, FEEL DIFF PRESS
    (1u << 4),                              // STBY RUD ON
    (1u << 6) | (1u << 7),                  // SPEED TRIM FAIL, MACH TRIM FAIL
    (1u << 2) | (1u << 3) | (1u << 8),      // LOW QUANTITY, LOW PRESSURE, AUTO SLAT FAIL
    0,
};

#define COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
const LampPanel::Def DEF = {
    nullptr,
    LAMPS, LAMP_COUNT,
    PLATES, COUNT(PLATES),
    LABELS, COUNT(LABELS),
    nullptr, 0,
    DEMO, COUNT(DEMO),
};
#undef COUNT

void init(uint8_t backlightPin) { LampPanel::init(DEF, backlightPin); }

} // namespace FctlPanel
