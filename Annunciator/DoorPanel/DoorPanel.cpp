#include "DoorPanel.h"
#include "Layout.h"

namespace DoorPanel
{

/* ---- Geometry -------------------------------------------------------------------
   Laid out like the real 737-800 door annunciator panel. The real lenses are about 2:1;
   four columns across 300px of content fix the width at 71, so that makes them 35px tall.
   The two centre columns are four deep -- entry door, forward and aft overwing exits,
   aft door -- and the outer columns hold two lamps each, level with the overwing rows:
   AIR STAIR over EQUIP on the left, FWD over AFT CARGO on the right. (The -900ER staggers
   its outer columns half a row, because they hold two and three lamps around its mid
   exits; the -800 has no mid exits, so its panel is a plain grid.)

   The door group sits directly under the DOORS header. GRD PWR is not a door at all -- on
   the aircraft it lives on the electrical panel -- so it stands apart at the bottom left.
---------------------------------------------------------------------------------- */
static constexpr int16_t COL_W = LY::LENS_W, COL_GAP = LY::COL_GAP;
static constexpr int16_t ROW_H = LY::LENS_H, ROW_GAP = LY::ROW_GAP;
static constexpr int16_t PANEL_W = LY::PANEL_W, PANEL_H = LY::PANEL_H;
static constexpr int16_t PAD_BOTTOM = LY::PAD_BOTTOM;

static constexpr int16_t HEADER_Y = LY::HEADER_CAP_Y;
static constexpr int16_t HEADER_H = LY::HEADER_BAND;

static constexpr int16_t GRID_W    = 4 * COL_W + 3 * COL_GAP;      // 299 / 425
static constexpr int16_t CONTENT_X = (PANEL_W - GRID_W) / 2;       // 10 / 27, centred
static constexpr int16_t ROW_PITCH = ROW_H + ROW_GAP;              // 39 / 53
static constexpr int16_t GRID_H    = 4 * ROW_H + 3 * ROW_GAP;      // 152 / 206, centre columns
static constexpr int16_t GRID_Y    = HEADER_Y + HEADER_H + LY::HEADER_GAP; // straight under the header

static constexpr int16_t GRD_PWR_X = CONTENT_X;
static constexpr int16_t GRD_PWR_Y = PANEL_H - PAD_BOTTOM - ROW_H; // 197 / 262

static constexpr int16_t colX(int c) { return (int16_t)(CONTENT_X + c * (COL_W + COL_GAP)); }
static constexpr int16_t rowY(int r) { return (int16_t)(GRID_Y + r * ROW_PITCH); }

/* The geometry above was derived by hand, so pin it down: a future tweak that pushes the
   grid off the panel fails the build rather than silently clipping lamps on the hardware. */
static_assert(colX(3) + COL_W <= PANEL_W, "door grid is wider than the panel");
static_assert(GRID_Y + GRID_H + LY::PAD_BOTTOM <= GRD_PWR_Y, "GRD PWR crowds the door group");
static_assert(GRD_PWR_Y + ROW_H <= PANEL_H, "GRD PWR runs off the panel");
// The press ring is drawn just outside a lamp, in the gap between lamps.
static_assert(COL_GAP > LY::RING_INSET && ROW_GAP > LY::RING_INSET,
              "no room for the press ring between lamps");

/* The legends are what fixed the lens width in the first place, so check them rather than
   trusting the numbers above to have kept up with a font change. RIGHT FWD is the widest;
   OVERWING is the widest single word. */
static_assert(LY::fits(FONT_SMALL, "RIGHT FWD", COL_W, 3), "door lens too narrow for RIGHT FWD");
static_assert(LY::fits(FONT_SMALL, "OVERWING", COL_W, 3), "door lens too narrow for OVERWING");
static_assert(2 * fontOf(FONT_SMALL).capHeight + 4 <= ROW_H - 6, "door lens too short for two lines");
static_assert(COL_W >= LY::MIN_TOUCH_PX && ROW_H >= LY::MIN_TOUCH_PX,
              "a door lamp is too small to press");

#define DOOR(l1, l2, c, r) {l1, l2, nullptr, colX(c), rowY(r), COL_W, ROW_H, LAMP_AMBER, KIND_LENS, FONT_SMALL}

/* Column-major, top to bottom. The message IDs are these indices. */
static const LampPanel::Lamp LAMPS[LAMP_COUNT] = {
    {"GRD PWR", "", nullptr, GRD_PWR_X, GRD_PWR_Y, COL_W, ROW_H, LAMP_BLUE, KIND_LENS, FONT_SMALL},
    DOOR("AIR", "STAIR", 0, 1),
    DOOR("EQUIP", "", 0, 2),
    DOOR("FWD", "ENTRY", 1, 0),
    DOOR("LEFT FWD", "OVERWING", 1, 1),
    DOOR("LEFT AFT", "OVERWING", 1, 2),
    DOOR("AFT", "ENTRY", 1, 3),
    DOOR("FWD", "SERVICE", 2, 0),
    DOOR("RIGHT FWD", "OVERWING", 2, 1),
    DOOR("RIGHT AFT", "OVERWING", 2, 2),
    DOOR("AFT", "SERVICE", 2, 3),
    DOOR("FWD", "CARGO", 3, 1),
    DOOR("AFT", "CARGO", 3, 2),
};
#undef DOOR

/* Every door is a button; GRD PWR is not -- the PMDG has no event that connects ground
   power (that is done from the CDU or EFB). The names are what MobiFlight sees; the
   profiles from tools/make_profiles.py bind each to the PMDG door event. */
#define ZONE(c, r, name) {colX(c), rowY(r), COL_W, ROW_H, name}
static const LampPanel::Zone ZONES[] = {
    ZONE(0, 1, "AIR STAIR"),
    ZONE(0, 2, "EQUIP"),
    ZONE(1, 0, "FWD ENTRY"),
    ZONE(1, 1, "LEFT FWD OVERWING"),
    ZONE(1, 2, "LEFT AFT OVERWING"),
    ZONE(1, 3, "AFT ENTRY"),
    ZONE(2, 0, "FWD SERVICE"),
    ZONE(2, 1, "RIGHT FWD OVERWING"),
    ZONE(2, 2, "RIGHT AFT OVERWING"),
    ZONE(2, 3, "AFT SERVICE"),
    ZONE(3, 1, "FWD CARGO"),
    ZONE(3, 2, "AFT CARGO"),
};
#undef ZONE

/* The five demo states from the mockup, cycled on demand as a self test (MSG_SELF_TEST)
   so the panel can be checked against the design. Bit n is lamp n; bit 0 is GRD PWR. */
static const uint32_t DEMO[] = {
    (1u << 0),                                                    // all secure, GPU connected
    (1u << 0) | (1u << 3) | (1u << 7) | (1u << 11) | (1u << 12),  // boarding
    (1u << 0) | (1u << 11) | (1u << 12),                          // cargo loading
    (1u << 0) | (1u << 6) | (1u << 10) | (1u << 2),               // aft servicing
    (1u << 4),                                                    // in-flight caution, no GPU
};

#define COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
const LampPanel::Def DEF = {
    "DOORS",
    LAMPS, LAMP_COUNT,
    nullptr, 0,
    nullptr, 0,
    ZONES, COUNT(ZONES),
    DEMO, COUNT(DEMO),
};
#undef COUNT

void init(uint8_t backlightPin)
{
    LampPanel::init(DEF, backlightPin);
}

} // namespace DoorPanel
