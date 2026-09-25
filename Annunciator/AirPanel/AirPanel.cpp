#include "AirPanel.h"
#include "Layout.h"

namespace AirPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Four groups, top to bottom, as on the aircraft. The real column is portrait; it fits the
   landscape screen because no group is more than four lamps wide, but six rows of lamps
   and a row of captions leave no height for door-sized 35px lenses: 31px still holds two
   11px lines with room round them.

   The ZONE TEMP lenses are spaced as the real ones are, over their selectors at about 20%,
   48% and 77% of the panel's width. The light strip and the pressurisation strip use the
   door panel's four-column grid; the strip's third position is empty on the real panel too.
   The bleed stacks are centred at about 31% and 69% as measured, with their lenses nearly
   touching as the real ones do, and a little wider: WING-BODY is 67px of ink.
---------------------------------------------------------------------------------- */
static constexpr int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;

// Six rows of lamps and a row of captions leave no height for a full door lens on either
// screen, so this panel uses the tight one.
static constexpr int16_t LENS_W = LY::LENS_W, LENS_H = LY::LENS_H_TIGHT;
static constexpr int16_t COL_GAP = LY::COL_GAP;
static constexpr int16_t GRID_X = (PANEL_W - (4 * LENS_W + 3 * COL_GAP)) / 2; // 10 / 27
static constexpr int16_t gridX(int c) { return (int16_t)(GRID_X + c * (LENS_W + COL_GAP)); }

#if ANNUN_PANEL_RES == 480
static constexpr int16_t ZONE_Y = 8;
// The three ZONE TEMP lenses sit over their selectors at about 24%, 50% and 77% of the
// panel's width, as measured on the aircraft; these keep those proportions.
static constexpr int16_t ZONE_X0 = 63, ZONE_PITCH = 128;
static constexpr int16_t STRIP_Y = 72;
static constexpr int16_t STACK_W = 107, STACK_H = 38, STACK_PITCH = 41;
static constexpr int16_t STACK_Y = 124, STACK_L = 93;
static constexpr int16_t TRIP_GAP = 40; // room for TRIP RESET between the bleed stacks
static constexpr int16_t PRESS_Y = 263;
#else
static constexpr int16_t ZONE_Y = 6;
static constexpr int16_t ZONE_X0 = 40, ZONE_PITCH = 85;
static constexpr int16_t STRIP_Y = 54;
static constexpr int16_t STACK_W = 75, STACK_H = 28, STACK_PITCH = 30;
static constexpr int16_t STACK_Y = 93, STACK_L = 62;
static constexpr int16_t TRIP_GAP = 30;
static constexpr int16_t PRESS_Y = 197;
#endif

static constexpr int16_t ZONE_CAP = ZONE_Y + LENS_H + LY::HEADER_GAP; // caption row, cap centre
static constexpr int16_t zoneX(int i) { return (int16_t)(ZONE_X0 + i * ZONE_PITCH); }

static constexpr int16_t STACK_R = PANEL_W - STACK_L - STACK_W; // 183 / 280
static constexpr int16_t stackY(int r) { return (int16_t)(STACK_Y + r * STACK_PITCH); }
static constexpr int16_t STACK_BOTTOM = stackY(2) + STACK_H; // 181 / 244
static constexpr int16_t MID_X = PANEL_W / 2;

static_assert(zoneX(2) + LENS_W <= PANEL_W - LY::MARGIN, "ZONE TEMP row runs off the panel");
static_assert(STRIP_Y > ZONE_CAP + 6, "light strip crowds the zone captions");
static_assert(STACK_Y >= STRIP_Y + LENS_H + 8, "bleed stacks crowd the light strip");
static_assert(STACK_L + STACK_W + TRIP_GAP <= STACK_R, "no room for TRIP RESET between the stacks");
static_assert(PRESS_Y >= STACK_BOTTOM + LY::HEADER_GAP * 2, "no room for RESET under the stacks");
// WING-BODY is the widest legend on the panel and is what sets the bleed stack's width.
static_assert(LY::fits(FONT_SMALL, "WING-BODY", STACK_W, 3), "bleed stack too narrow for WING-BODY");
static_assert(LY::fits(FONT_SMALL, "FULL OPEN", LENS_W, 3), "air lens too narrow for FULL OPEN");
static_assert(PRESS_Y + LENS_H <= PANEL_H - 4, "pressurisation strip runs off the panel");

#define LENS(l1, l2, x, y, colour) {l1, l2, nullptr, x, y, LENS_W, LENS_H, colour, KIND_LENS, FONT_SMALL}
#define STACK(l1, l2, x, r)        {l1, l2, nullptr, x, stackY(r), STACK_W, STACK_H, LAMP_AMBER, KIND_LENS, FONT_SMALL}

static const LampPanel::Lamp LAMPS[LAMP_COUNT] = {
    LENS("ZONE", "TEMP", zoneX(0), ZONE_Y, LAMP_AMBER),
    LENS("ZONE", "TEMP", zoneX(1), ZONE_Y, LAMP_AMBER),
    LENS("ZONE", "TEMP", zoneX(2), ZONE_Y, LAMP_AMBER),
    LENS("DUAL", "BLEED", gridX(0), STRIP_Y, LAMP_AMBER),
    LENS("RAM DOOR", "FULL OPEN", gridX(1), STRIP_Y, LAMP_BLUE),
    LENS("RAM DOOR", "FULL OPEN", gridX(3), STRIP_Y, LAMP_BLUE),
    STACK("PACK", "", STACK_L, 0),
    STACK("WING-BODY", "OVERHEAT", STACK_L, 1),
    STACK("BLEED", "TRIP OFF", STACK_L, 2),
    STACK("PACK", "", STACK_R, 0),
    STACK("WING-BODY", "OVERHEAT", STACK_R, 1),
    STACK("BLEED", "TRIP OFF", STACK_R, 2),
    LENS("AUTO", "FAIL", gridX(0), PRESS_Y, LAMP_AMBER),
    LENS("OFF SCHED", "DESCENT", gridX(1), PRESS_Y, LAMP_AMBER),
    LENS("ALTN", "", gridX(2), PRESS_Y, LAMP_GREEN),
    LENS("MANUAL", "", gridX(3), PRESS_Y, LAMP_GREEN),
};
#undef LENS
#undef STACK

/* Printed on the panel: the zone selectors' names under their lamps, and the TRIP RESET
   button between the stacks -- TRIP over it, level with WING-BODY OVERHEAT, the button
   level with BLEED TRIP OFF, RESET under the stacks. */
static const LampPanel::Label LABELS[] = {
    {"CONT CAB", (int16_t)(zoneX(0) + LENS_W / 2), ZONE_CAP, Theme::HeaderFg, true},
    {"FWD CAB", (int16_t)(zoneX(1) + LENS_W / 2), ZONE_CAP, Theme::HeaderFg, true},
    {"AFT CAB", (int16_t)(zoneX(2) + LENS_W / 2), ZONE_CAP, Theme::HeaderFg, true},
    {"TRIP", MID_X, (int16_t)(stackY(1) + STACK_H / 2), Theme::HeaderFg, true},
    {"RESET", MID_X, (int16_t)(STACK_BOTTOM + 7), Theme::HeaderFg, true},
};

// The TRIP RESET button itself: a small round black cap, level with BLEED TRIP OFF.
static const LampPanel::Plate PLATES[] = {
    {(int16_t)(MID_X - 8), (int16_t)(stackY(2) + STACK_H / 2 - 8), 17, 17, 0x101112, Theme::Bezel, 8},
};

/* Self test: engine start on APU bleed, a pack trip, a single pressure controller failure,
   manual pressurisation, the overheat test, a zone overheat. Bit n is lamp n. */
static const uint32_t DEMO[] = {
    (1u << 3) | (1u << 4) | (1u << 5),  // APU bleed start: DUAL BLEED, RAM DOORs
    (1u << 4) | (1u << 5),              // on the ground, packs on: RAM DOORs
    (1u << 6) | (1u << 8),              // left PACK, left BLEED TRIP OFF
    (1u << 12) | (1u << 14),            // AUTO FAIL + ALTN: single controller failure
    (1u << 15),                         // MANUAL
    (1u << 7) | (1u << 10),             // OVHT TEST: both WING-BODY OVERHEAT
    (1u << 1) | (1u << 13),             // FWD CAB ZONE TEMP, OFF SCHED DESCENT
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

} // namespace AirPanel
