#include "McsPanel.h"
#include "Layout.h"

namespace McsPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Across, the real captain's unit, measured off a photo in units of the push-light's
   side: FIRE WARN 1, gap 0.2, MASTER CAUTION 1, gap 0.47, six-pack 2.2 wide by 1.07 tall.
   About 4.9 lights across, so the width of the screen sets the scale: 72px push-lights and
   a 146px six-pack, the gaps pinched a little. The push-lights' legends are 14px with 8px
   small print (PUSH TO RESET is 61px on a 68px face); six-pack captions are 11px,
   OVERHEAD, the widest, being 61px of ink in a 66px column.

   A single side is that strip, centred top to bottom -- the real unit is that shape, so the
   rest of the screen stays black. BOTH stacks two taller six-packs down the right, with the
   push-lights centred beside the pair.
---------------------------------------------------------------------------------- */
static constexpr int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;

/* The widest panel in the set, and the only one where width was the binding constraint at
   320: OVERHEAD is 61px of ink in a 66px column. The larger screen spends its extra width
   here first -- a 104px push-light and a 106px caption column -- so this is the panel that
   improves most. */
#if ANNUN_PANEL_RES == 480
static constexpr int16_t MARGIN = 8;
static constexpr int16_t SQ     = 104;
static constexpr int16_t SQ_GAP = 8;
static constexpr int16_t SP_GAP = 16;
static constexpr int16_t SIDE_SP_H = 121;
static constexpr int16_t BOTH_SP_H = 133, BOTH_SP_GAP = 16;
static constexpr int16_t SP_PAD_X = 7, SP_COL_GAP = 6;
static constexpr int16_t CAP_H = 27;
static constexpr int16_t SIDE_ROW_GAP = 6, BOTH_ROW_GAP = 13;
#else
static constexpr int16_t MARGIN = 6;
static constexpr int16_t SQ     = 72; // FIRE WARN and MASTER CAUTION
static constexpr int16_t SQ_GAP = 6;  // between the two push-lights
static constexpr int16_t SP_GAP = 12; // push-light to six-pack
static constexpr int16_t SIDE_SP_H = 76;
static constexpr int16_t BOTH_SP_H = 100, BOTH_SP_GAP = 12;
static constexpr int16_t SP_PAD_X = 5, SP_COL_GAP = 4;
static constexpr int16_t CAP_H = 20;
static constexpr int16_t SIDE_ROW_GAP = 4, BOTH_ROW_GAP = 10;
#endif
static constexpr int16_t SP_W   = PANEL_W - 2 * MARGIN - 2 * SQ - SQ_GAP - SP_GAP; // 146
static constexpr int16_t SQ_Y   = (PANEL_H - SQ) / 2; // 84

// The two push-lights and the six-pack, left to right, when the six-pack is on the right
// (captain, BOTH) or on the left (first officer).
static constexpr int16_t OUT_X   = MARGIN;                  // outboard push-light, six-pack right
static constexpr int16_t IN_X    = OUT_X + SQ + SQ_GAP;     // 84
static constexpr int16_t SP_R_X  = IN_X + SQ + SP_GAP;      // 168
static constexpr int16_t SP_L_X  = MARGIN;                  // six-pack left
static constexpr int16_t IN_R_X  = SP_L_X + SP_W + SP_GAP;  // 164
static constexpr int16_t OUT_R_X = IN_R_X + SQ + SQ_GAP;    // 242

// Six-pack faces: one side's is the real shape; BOTH's two are taller, filling the height.
static constexpr int16_t SIDE_SP_Y = (PANEL_H - SIDE_SP_H) / 2;                     // 82 / 99
static constexpr int16_t TOP_SP_Y  = (PANEL_H - (2 * BOTH_SP_H + BOTH_SP_GAP)) / 2; // 14 / 19
static constexpr int16_t BOT_SP_Y  = TOP_SP_Y + BOTH_SP_H + BOTH_SP_GAP;            // 126 / 168

// Captions: two columns of three on a face. Rows spread with the face's height.
static constexpr int16_t CAP_W = (SP_W - 2 * SP_PAD_X - SP_COL_GAP) / 2; // 66 / 106
static constexpr int16_t rowGap(int16_t spH)
{
    return (int16_t)(spH == SIDE_SP_H ? SIDE_ROW_GAP : BOTH_ROW_GAP);
}
static constexpr int16_t capX(int16_t spX, int c) { return (int16_t)(spX + SP_PAD_X + c * (CAP_W + SP_COL_GAP)); }
static constexpr int16_t capY(int16_t spY, int16_t spH, int r)
{
    return (int16_t)(spY + (spH - (3 * CAP_H + 2 * rowGap(spH))) / 2 + r * (CAP_H + rowGap(spH)));
}

static_assert(OUT_R_X + SQ <= PANEL_W - MARGIN, "first officer's FIRE WARN runs off the panel");
static_assert(LY::fits(FONT_SMALL, "OVERHEAD", CAP_W, 2), "six-pack column too narrow for OVERHEAD");
static_assert(3 * CAP_H + 2 * SIDE_ROW_GAP <= SIDE_SP_H - 4,
              "captions do not fit a single side's face");
// The push-light legends: two lines of FONT_LARGE over a line of small print.
static_assert(LY::fits(FONT_LARGE, "CAUTION", SQ, 3), "push-light too narrow for CAUTION");
static_assert(LY::fits(FONT_TINY, "PUSH TO RESET", SQ, 2), "push-light too narrow for PUSH TO RESET");
static_assert(SQ >= LY::MIN_TOUCH_PX, "a push-light is too small to press");
static_assert(BOT_SP_Y + BOTH_SP_H <= PANEL_H - 3 && TOP_SP_Y >= 3, "BOTH's six-packs run off the panel");
// The press ring is drawn 2px outside a push-light or a six-pack face.
static_assert(SQ_GAP > LY::RING_INSET && SP_GAP > LY::RING_INSET &&
                  BOTH_SP_GAP > LY::RING_INSET && MARGIN > LY::RING_INSET,
              "no room for the press ring");

/* ---- The AFDS variant --------------------------------------------------------------
   The same combined panel with the autoflight annunciators added: A/P, A/T and FMC, the
   three P/RST lights that sit under the MCP on the real glareshield. They are smaller than
   the warning caps there and they are here too, in a row beneath them, which is also what
   makes the pair of blocks read as one unit rather than five equal squares.

   Adding them costs height, so the warning caps and the row below are centred together as
   a block rather than the caps being centred alone. Everything to the right -- both
   six-packs -- is untouched, and lamps 0 to 13 keep the message IDs they have on
   ANNUN_MCS_BOTH, so a binding made for that panel still lands on the right lamp here.
------------------------------------------------------------------------------------- */
/* AFDS_TO_SQ is deliberately a wide gap, not a tidy one. The warning caps and the
   autoflight lights are separate units on the aircraft -- the caps are in the glareshield
   panel, the P/RST lights sit under the MCP -- and a gap of about a quarter of a cap is
   what stops the five reading as one block of buttons -- here it is nearer half a cap,
   which is as much as the height allows while keeping both groups off the edges. */
#if ANNUN_PANEL_RES == 480
static constexpr int16_t AFDS_H = 40, AFDS_GAP = 6, AFDS_TO_SQ = 46;
#else
static constexpr int16_t AFDS_H = 30, AFDS_GAP = 5, AFDS_TO_SQ = 33;
#endif
static constexpr int16_t AFDS_SPAN  = (int16_t)(2 * SQ + SQ_GAP);            // as wide as the two caps
static constexpr int16_t AFDS_W     = (int16_t)((AFDS_SPAN - 2 * AFDS_GAP) / 3);
static constexpr int16_t AFDS_BLOCK = (int16_t)(SQ + AFDS_TO_SQ + AFDS_H);
static constexpr int16_t AFDS_SQ_Y  = (int16_t)((PANEL_H - AFDS_BLOCK) / 2);
static constexpr int16_t AFDS_Y     = (int16_t)(AFDS_SQ_Y + SQ + AFDS_TO_SQ);
static constexpr int16_t afdsX(int i) { return (int16_t)(OUT_X + i * (AFDS_W + AFDS_GAP)); }

static_assert(AFDS_Y + AFDS_H <= PANEL_H - MARGIN, "the AFDS row runs off the panel");
static_assert(afdsX(2) + AFDS_W <= OUT_X + AFDS_SPAN, "the AFDS row is wider than the caps above it");
static_assert(LY::fits(FONT_SMALL, "FMC", AFDS_W, 3), "AFDS light too narrow for its legend");
static_assert(LY::fits(FONT_TINY, "P/RST", AFDS_W, 2), "AFDS light too narrow for P/RST");
static_assert(AFDS_W >= LY::MIN_TOUCH_PX && AFDS_H + AFDS_GAP >= LY::MIN_TOUCH_PX,
              "an AFDS light is too small to press");
static_assert(AFDS_GAP > LY::RING_INSET, "no room between the AFDS lights");

#define FIRE_WARN(x)      {"FIRE", "WARN", "BELL CUTOUT", x, SQ_Y, SQ, SQ, LAMP_RED, KIND_FILLED, FONT_LARGE}
#define MASTER_CAUTION(x) {"MASTER", "CAUTION", "PUSH TO RESET", x, SQ_Y, SQ, SQ, LAMP_AMBER, KIND_FILLED, FONT_LARGE}
#define CAPTION(text, spX, spY, spH, c, r) \
    {text, "", nullptr, capX(spX, c), capY(spY, spH, r), CAP_W, CAP_H, LAMP_AMBER, KIND_LEGEND, FONT_SMALL}
#define CAPT_SIX_PACK(spX, spY, spH)                                                      \
    CAPTION("FLT CONT", spX, spY, spH, 0, 0), CAPTION("ELEC", spX, spY, spH, 1, 0),       \
    CAPTION("IRS", spX, spY, spH, 0, 1), CAPTION("APU", spX, spY, spH, 1, 1),             \
    CAPTION("FUEL", spX, spY, spH, 0, 2), CAPTION("OVHT/DET", spX, spY, spH, 1, 2)
#define FO_SIX_PACK(spX, spY, spH)                                                        \
    CAPTION("ANTI-ICE", spX, spY, spH, 0, 0), CAPTION("ENG", spX, spY, spH, 1, 0),        \
    CAPTION("HYD", spX, spY, spH, 0, 1), CAPTION("OVERHEAD", spX, spY, spH, 1, 1),        \
    CAPTION("DOORS", spX, spY, spH, 0, 2), CAPTION("AIR COND", spX, spY, spH, 1, 2)

static const LampPanel::Lamp LEFT_LAMPS[SIDE_LAMP_COUNT] = {
    FIRE_WARN(OUT_X),
    MASTER_CAUTION(IN_X),
    CAPT_SIX_PACK(SP_R_X, SIDE_SP_Y, SIDE_SP_H),
};
static const LampPanel::Lamp RIGHT_LAMPS[SIDE_LAMP_COUNT] = {
    FIRE_WARN(OUT_R_X),
    MASTER_CAUTION(IN_R_X),
    FO_SIX_PACK(SP_L_X, SIDE_SP_Y, SIDE_SP_H),
};
static const LampPanel::Lamp BOTH_LAMPS[BOTH_LAMP_COUNT] = {
    FIRE_WARN(OUT_X),
    MASTER_CAUTION(IN_X),
    CAPT_SIX_PACK(SP_R_X, TOP_SP_Y, BOTH_SP_H),
    FO_SIX_PACK(SP_R_X, BOT_SP_Y, BOTH_SP_H),
};

/* The warning caps sit higher here, so they take their y rather than the shared SQ_Y. */
#define FIRE_WARN_AT(x, y)      {"FIRE", "WARN", "BELL CUTOUT", x, y, SQ, SQ, LAMP_RED, KIND_FILLED, FONT_LARGE}
#define MASTER_CAUTION_AT(x, y) {"MASTER", "CAUTION", "PUSH TO RESET", x, y, SQ, SQ, LAMP_AMBER, KIND_FILLED, FONT_LARGE}
#define AFDS(legend, i, colour, second) \
    {legend, "", "P/RST", afdsX(i), AFDS_Y, AFDS_W, AFDS_H, colour, KIND_FILLED, FONT_SMALL, second}

static const LampPanel::Lamp BOTH_AFDS_LAMPS[BOTH_AFDS_LAMP_COUNT] = {
    FIRE_WARN_AT(OUT_X, AFDS_SQ_Y),
    MASTER_CAUTION_AT(IN_X, AFDS_SQ_Y),
    CAPT_SIX_PACK(SP_R_X, TOP_SP_Y, BOTH_SP_H),
    FO_SIX_PACK(SP_R_X, BOT_SP_Y, BOTH_SP_H),
    /* 14, 15, 16: the autoflight lights. The aircraft says how serious it is with the
       colour, so A/P and A/T carry both: red for a warning -- an autopilot or autothrottle
       disconnect -- and amber for a caution, sent as LS_ALT ("3"). FMC is amber only. */
    AFDS("A/P", 0, LAMP_RED, LAMP_AMBER),
    AFDS("A/T", 1, LAMP_RED, LAMP_AMBER),
    AFDS("FMC", 2, LAMP_AMBER, LAMP_AMBER),
};

#undef FIRE_WARN
#undef MASTER_CAUTION
#undef FIRE_WARN_AT
#undef MASTER_CAUTION_AT
#undef AFDS
#undef CAPTION
#undef CAPT_SIX_PACK
#undef FO_SIX_PACK

static const LampPanel::Plate LEFT_PLATES[]  = {{SP_R_X, SIDE_SP_Y, SP_W, SIDE_SP_H, Theme::SixPackFace, Theme::Bezel}};
static const LampPanel::Plate RIGHT_PLATES[] = {{SP_L_X, SIDE_SP_Y, SP_W, SIDE_SP_H, Theme::SixPackFace, Theme::Bezel}};
static const LampPanel::Plate BOTH_PLATES[]  = {
    {SP_R_X, TOP_SP_Y, SP_W, BOTH_SP_H, Theme::SixPackFace, Theme::Bezel},
    {SP_R_X, BOT_SP_Y, SP_W, BOTH_SP_H, Theme::SixPackFace, Theme::Bezel},
};

/* Button names. Each side's are its own; BOTH's push-lights send the plain names (they
   stand for both sides) and its six-packs recall like the side they belong to. */
static const LampPanel::Zone LEFT_ZONES[] = {
    {OUT_X, SQ_Y, SQ, SQ, "FIRE WARN L"},
    {IN_X, SQ_Y, SQ, SQ, "MASTER CAUTION L"},
    {SP_R_X, SIDE_SP_Y, SP_W, SIDE_SP_H, "RECALL L"},
};
static const LampPanel::Zone RIGHT_ZONES[] = {
    {OUT_R_X, SQ_Y, SQ, SQ, "FIRE WARN R"},
    {IN_R_X, SQ_Y, SQ, SQ, "MASTER CAUTION R"},
    {SP_L_X, SIDE_SP_Y, SP_W, SIDE_SP_H, "RECALL R"},
};
static const LampPanel::Zone BOTH_AFDS_ZONES[] = {
    {OUT_X, AFDS_SQ_Y, SQ, SQ, "FIRE WARN"},
    {IN_X, AFDS_SQ_Y, SQ, SQ, "MASTER CAUTION"},
    {SP_R_X, TOP_SP_Y, SP_W, BOTH_SP_H, "RECALL L"},
    {SP_R_X, BOT_SP_Y, SP_W, BOTH_SP_H, "RECALL R"},
    {afdsX(0), AFDS_Y, AFDS_W, AFDS_H, "AFDS AP"},
    {afdsX(1), AFDS_Y, AFDS_W, AFDS_H, "AFDS AT"},
    {afdsX(2), AFDS_Y, AFDS_W, AFDS_H, "AFDS FMC"},
};

static const LampPanel::Zone BOTH_ZONES[] = {
    {OUT_X, SQ_Y, SQ, SQ, "FIRE WARN"},
    {IN_X, SQ_Y, SQ, SQ, "MASTER CAUTION"},
    {SP_R_X, TOP_SP_Y, SP_W, BOTH_SP_H, "RECALL L"},
    {SP_R_X, BOT_SP_Y, SP_W, BOTH_SP_H, "RECALL R"},
};

/* Self test: a caution, a fire, a recall (every caption lit), another caution, all clear.
   Bit n is lamp n. */
static const uint32_t SIX = 0x00FCu; // lamps 2..7: a side's six-pack, or BOTH's captain's
static const uint32_t LEFT_DEMO[] = {
    (1u << 1) | (1u << 2),             // MASTER CAUTION, FLT CONT
    (1u << 0) | (1u << 1) | (1u << 7), // FIRE WARN, MASTER CAUTION, OVHT/DET
    (1u << 1) | SIX,                   // recall
    (1u << 1) | (1u << 3) | (1u << 5), // MASTER CAUTION, ELEC, APU
    0,
};
static const uint32_t RIGHT_DEMO[] = {
    (1u << 1) | (1u << 4),             // MASTER CAUTION, HYD
    (1u << 0) | (1u << 1) | (1u << 3), // FIRE WARN, MASTER CAUTION, ENG
    (1u << 1) | SIX,                   // recall
    (1u << 1) | (1u << 6) | (1u << 7), // MASTER CAUTION, DOORS, AIR COND
    0,
};
/* The self test cycles the same states as the combined panel, then the autoflight row:
   a disconnect, then the pair of cautions. */
static const uint32_t BOTH_AFDS_DEMO[] = {
    (1u << 1) | (1u << 2) | (1u << 10),
    (1u << 0) | (1u << 1) | (1u << 7) | (1u << 9),
    (1u << 1) | SIX | (SIX << 6),
    (1u << 14) | (1u << 15),
    (1u << 16),
    0,
};

static const uint32_t BOTH_DEMO[] = {
    (1u << 1) | (1u << 2) | (1u << 10),            // MASTER CAUTION, FLT CONT, HYD
    (1u << 0) | (1u << 1) | (1u << 7) | (1u << 9), // FIRE WARN, MASTER CAUTION, OVHT/DET, ENG
    (1u << 1) | SIX | (SIX << 6),                  // recall
    (1u << 1) | (1u << 12),                        // MASTER CAUTION, DOORS
    0,
};

#define COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
#define DEF_OF(lamps, plates, zones, demo) \
    {nullptr, lamps, COUNT(lamps), plates, COUNT(plates), nullptr, 0, zones, COUNT(zones), demo, COUNT(demo)}
const LampPanel::Def LEFT  = DEF_OF(LEFT_LAMPS, LEFT_PLATES, LEFT_ZONES, LEFT_DEMO);
const LampPanel::Def RIGHT = DEF_OF(RIGHT_LAMPS, RIGHT_PLATES, RIGHT_ZONES, RIGHT_DEMO);
const LampPanel::Def BOTH  = DEF_OF(BOTH_LAMPS, BOTH_PLATES, BOTH_ZONES, BOTH_DEMO);
const LampPanel::Def BOTH_AFDS =
    DEF_OF(BOTH_AFDS_LAMPS, BOTH_PLATES, BOTH_AFDS_ZONES, BOTH_AFDS_DEMO);
#undef DEF_OF
#undef COUNT

void initLeft(uint8_t backlightPin) { LampPanel::init(LEFT, backlightPin); }
void initRight(uint8_t backlightPin) { LampPanel::init(RIGHT, backlightPin); }
void initBoth(uint8_t backlightPin) { LampPanel::init(BOTH, backlightPin); }
void initBothAfds(uint8_t backlightPin) { LampPanel::init(BOTH_AFDS, backlightPin); }

} // namespace McsPanel
