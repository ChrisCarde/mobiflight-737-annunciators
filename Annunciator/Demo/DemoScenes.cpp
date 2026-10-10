#include "Demo.h"
#include "MFCustomDevice.h" // the panel type names
#include <stddef.h>
#include <string.h>

/* **********************************************************************************
    The demo's scenes, one per panel, in the order they play: a turnaround, the IRS
    alignment and its present-position entry, the power coming on, the flight-control
    check, engine start and climb, then the glareshield.

    Each is what the aircraft would actually show, to the standard of the panels' self tests
    and tools/mfsim.py's DEMOS: the same lights, in the same order, for the same reasons --
    only quicker. Message IDs are the panels' own (Annunciator/Community/devices/*.json);
    payloads are what MobiFlight sends: "0" dark, "1" lit, "2" blinking, "3"/"4" the same in
    a lamp's second colour. A PRESS names the touch zone, exactly as the panel names it.
********************************************************************************** */

namespace
{

using Demo::Step;

// The building blocks. TAP presses a button for a quarter of a second; the light it acts on
// changes just after, as the sim's answer would.
#define LIT(t, id)       {(uint16_t)(t), Demo::SET, (int16_t)(id), "1"}
#define DARK(t, id)      {(uint16_t)(t), Demo::SET, (int16_t)(id), "0"}
#define FLASH(t, id)     {(uint16_t)(t), Demo::SET, (int16_t)(id), "2"}
#define ALT_FLASH(t, id) {(uint16_t)(t), Demo::SET, (int16_t)(id), "4"}
#define SEND(t, id, s)   {(uint16_t)(t), Demo::SET, (int16_t)(id), s}
#define PRESS(t, zone)   {(uint16_t)(t), Demo::PRESS, 0, zone}
#define RELEASE(t)       {(uint16_t)(t), Demo::RELEASE, 0, nullptr}
#define TAP(t, zone)     PRESS(t, zone), RELEASE((t) + 250)

/* ---- Doors: a turnaround ----------------------------------------------------------
   Ground power plugged in; the entry, cargo and service doors open one by one round the
   airframe as passengers, bags and catering come and go, then close again for pushback.
   The overwing exits and the E&E bay stay shut, as they would. Each door is tapped first:
   on this panel touching a door is how the sim's door is opened. */
enum : int16_t {
    GRD_PWR = 0, AIR_STAIR, EQUIP, FWD_ENTRY, LEFT_FWD_OVERWING, LEFT_AFT_OVERWING, AFT_ENTRY,
    FWD_SERVICE, RIGHT_FWD_OVERWING, RIGHT_AFT_OVERWING, AFT_SERVICE, FWD_CARGO, AFT_CARGO,
};
constexpr Step DOORS[] = {
    LIT(500, GRD_PWR),
    TAP(2000, "FWD ENTRY"),   LIT(2300, FWD_ENTRY),
    TAP(3600, "AFT ENTRY"),   LIT(3900, AFT_ENTRY),
    TAP(5200, "AFT SERVICE"), LIT(5500, AFT_SERVICE),
    TAP(6800, "AFT CARGO"),   LIT(7100, AFT_CARGO),
    TAP(8400, "FWD CARGO"),   LIT(8700, FWD_CARGO),
    TAP(10000, "FWD SERVICE"), LIT(10300, FWD_SERVICE),
    TAP(14000, "FWD SERVICE"), DARK(14300, FWD_SERVICE),
    TAP(15400, "AFT SERVICE"), DARK(15700, AFT_SERVICE),
    TAP(16800, "AFT CARGO"),   DARK(17100, AFT_CARGO),
    TAP(18200, "FWD CARGO"),   DARK(18500, FWD_CARGO),
    TAP(19600, "AFT ENTRY"),   DARK(19900, AFT_ENTRY),
    TAP(21000, "FWD ENTRY"),   DARK(21300, FWD_ENTRY),
    DARK(23000, GRD_PWR),
};

/* ---- IRS: alignment ---------------------------------------------------------------
   Both mode selectors to NAV: each IRU tests its battery power first (ON DC), then aligns
   (ALIGN steady). Done aligning but given no present position, ALIGN flashes; once the
   position is entered on the ISDU (the next scene) it goes steady, then out -- NAV. */
enum : int16_t {
    IRS_GPS = 0, IRS_ILS, IRS_GLS, L_ALIGN, L_ON_DC, L_FAULT, L_DC_FAIL,
    R_ALIGN, R_ON_DC, R_FAULT, R_DC_FAIL,
};
constexpr Step IRS[] = {
    LIT(1500, L_ON_DC),  LIT(1500, R_ON_DC),
    DARK(5000, L_ON_DC), DARK(5000, R_ON_DC), LIT(5000, L_ALIGN), LIT(5000, R_ALIGN),
    FLASH(13000, L_ALIGN), FLASH(13000, R_ALIGN),
    LIT(19000, L_ALIGN), LIT(19000, R_ALIGN),
    DARK(23000, L_ALIGN),
    DARK(24500, R_ALIGN),
};

/* ---- ISDU: entering present position ----------------------------------------------
   DSPL SEL to PPOS and the position keyed in as the FCOM has it: N (the 2 key) 4 7 3 2 4,
   ENT; W (the 4 key) 1 2 2 1 2 3, ENT. The ENT key's cue light comes on when an entry is
   complete. Then back to HDG/STS, for the left IRS and the right. */
enum : int16_t { ISDU_LEFT = 0, ISDU_RIGHT, ISDU_DOTS, ISDU_SEL, ISDU_SYS, ISDU_ENT, ISDU_CLR };
enum : int16_t { SEL_TEST = 0, SEL_TKGS, SEL_PPOS, SEL_WIND, SEL_HDGSTS };
constexpr Step ISDU[] = {
    SEND(0, ISDU_SEL, "4"), SEND(0, ISDU_SYS, "0"),
    SEND(0, ISDU_LEFT, "341"), SEND(0, ISDU_RIGHT, "15"), SEND(0, ISDU_DOTS, "0"),

    TAP(2000, "ISDU DSPL PPOS"), SEND(2300, ISDU_SEL, "2"),
    SEND(2300, ISDU_LEFT, ""), SEND(2300, ISDU_RIGHT, ""), SEND(2300, ISDU_DOTS, "1"),

    TAP(3500, "ISDU 2"), SEND(3800, ISDU_LEFT, "N"),
    TAP(4300, "ISDU 4"), SEND(4600, ISDU_LEFT, "N4"),
    TAP(5100, "ISDU 7"), SEND(5400, ISDU_LEFT, "N47"),
    TAP(5900, "ISDU 3"), SEND(6200, ISDU_LEFT, "N473"),
    TAP(6700, "ISDU 2"), SEND(7000, ISDU_LEFT, "N4732"),
    TAP(7500, "ISDU 4"), SEND(7800, ISDU_LEFT, "N47324"), LIT(7800, ISDU_ENT),
    TAP(9000, "ISDU ENT"), DARK(9300, ISDU_ENT),

    TAP(10500, "ISDU 4"), SEND(10800, ISDU_RIGHT, "W"),
    TAP(11300, "ISDU 1"), SEND(11600, ISDU_RIGHT, "W1"),
    TAP(12100, "ISDU 2"), SEND(12400, ISDU_RIGHT, "W12"),
    TAP(12900, "ISDU 2"), SEND(13200, ISDU_RIGHT, "W122"),
    TAP(13700, "ISDU 1"), SEND(14000, ISDU_RIGHT, "W1221"),
    TAP(14500, "ISDU 2"), SEND(14800, ISDU_RIGHT, "W12212"),
    TAP(15300, "ISDU 3"), SEND(15600, ISDU_RIGHT, "W122123"), LIT(15600, ISDU_ENT),
    TAP(16800, "ISDU ENT"), DARK(17100, ISDU_ENT),

    TAP(21000, "ISDU DSPL HDG STS"), SEND(21300, ISDU_SEL, "4"),
    SEND(21300, ISDU_LEFT, "341"), SEND(21300, ISDU_RIGHT, "14"), SEND(21300, ISDU_DOTS, "0"),
    TAP(25000, "ISDU SYS DSPL R"), SEND(25300, ISDU_SYS, "1"),
    SEND(25300, ISDU_LEFT, "340"), SEND(25300, ISDU_RIGHT, "14"),
    TAP(29000, "ISDU DSPL PPOS"), SEND(29300, ISDU_SEL, "2"),
    SEND(29300, ISDU_LEFT, "N47324"), SEND(29300, ISDU_RIGHT, "W122123"), SEND(29300, ISDU_DOTS, "1"),
};

/* ---- Electrical metering: battery, ground power, APU ------------------------------
   On the battery alone (BAT DISCHARGE lit). Ground power plugged in shows on the AC meter
   before it is switched on, then takes the buses: the TRs charge the battery at 28 V. The
   APU's generator reads nothing until the APU is up to speed, then 115 V at 400 Hz, and
   takes the load when it is switched on.
   The lines are 12 characters: DC AMPS, blank, CPS FREQ / DC VOLTS, AC AMPS, AC VOLTS. */
enum : int16_t { BAT_DISCHARGE = 0, TR_UNIT, ELEC_LIGHT, LINE_TOP = 10, LINE_BOTTOM, DC_SEL, AC_SEL };
enum : int16_t { DC_STBY = 0, DC_BAT_BUS, DC_BAT, DC_AUX_BAT, DC_TR1, DC_TR2, DC_TR3, DC_TEST };
enum : int16_t { AC_STBY = 0, AC_GRD_PWR, AC_GEN1, AC_APU_GEN, AC_GEN2, AC_INV, AC_TEST };
constexpr Step ELEC[] = {
    SEND(0, DC_SEL, "2"), SEND(0, AC_SEL, "0"),
    SEND(0, LINE_TOP, "  10     400"), SEND(0, LINE_BOTTOM, "  24   2 115"), LIT(0, BAT_DISCHARGE),

    SEND(5000, AC_SEL, "1"), SEND(5000, LINE_BOTTOM, "  24   0 115"),
    SEND(8500, LINE_BOTTOM, "  24  45 115"), DARK(8500, BAT_DISCHARGE),
    SEND(10500, DC_SEL, "4"), SEND(10500, LINE_TOP, "  20     400"), SEND(10500, LINE_BOTTOM, "  28  45 115"),

    SEND(14000, AC_SEL, "3"), SEND(14000, LINE_TOP, "  20       0"), SEND(14000, LINE_BOTTOM, "  28   0   0"),
    SEND(17500, LINE_TOP, "  20     400"), SEND(17500, LINE_BOTTOM, "  28   0 115"),
    SEND(21000, LINE_BOTTOM, "  28  52 115"),
    SEND(24000, DC_SEL, "5"), SEND(24000, LINE_TOP, "  21     400"),
};

/* ---- Flight controls: hydraulics on, and the standby rudder check -----------------
   A cold aircraft has both FLT CONTROL LOW PRESSURE lights on. The B pump, then the A pumps,
   put them out. Then the flight-control check: each FLIGHT CONTROL switch to STBY RUD in
   turn starts the standby pump onto the rudder -- STBY RUD ON -- and back to ON. */
enum : int16_t {
    FC_A_LOW = 0, FC_B_LOW, STBY_LOW_QTY, STBY_LOW_PRESS, STBY_RUD_ON, FEEL_DIFF,
    SPEED_TRIM_FAIL, MACH_TRIM_FAIL, AUTO_SLAT_FAIL,
};
constexpr Step FCTL[] = {
    LIT(0, FC_A_LOW), LIT(0, FC_B_LOW),
    DARK(3500, FC_B_LOW),
    DARK(6000, FC_A_LOW),
    LIT(10000, STBY_RUD_ON),
    DARK(13500, STBY_RUD_ON),
    LIT(16000, STBY_RUD_ON),
    DARK(19500, STBY_RUD_ON),
};

/* ---- Air conditioning, bleed and pressurisation: start, takeoff, climb ------------
   On the ground the ram air doors are full open. Starting an engine with the APU bleed
   open and an engine bleed on gives DUAL BLEED until the APU bleed is closed. After
   takeoff, flaps up, the ram doors come off full open. In the climb the automatic pressure
   controller fails and the standby one takes over: AUTO FAIL with ALTN; selecting ALTN puts
   AUTO FAIL out and leaves ALTN. */
enum : int16_t {
    ZONE_CONT = 0, ZONE_FWD, ZONE_AFT, DUAL_BLEED, RAM_L, RAM_R, L_PACK, L_WB_OVHT, L_BLEED_TRIP,
    R_PACK, R_WB_OVHT, R_BLEED_TRIP, AUTO_FAIL, OFF_SCHED, PRESS_ALTN, PRESS_MANUAL,
};
constexpr Step AIR[] = {
    LIT(0, RAM_L), LIT(0, RAM_R),
    LIT(2500, DUAL_BLEED),
    DARK(9000, DUAL_BLEED),
    DARK(12500, RAM_L), DARK(12500, RAM_R),
    LIT(16000, AUTO_FAIL), LIT(16000, PRESS_ALTN),
    DARK(20000, AUTO_FAIL),
};

/* ---- Master caution, one side: a caution, a recall, a fire test -------------------
   A caution lights its six-pack caption and MASTER CAUTION; pressing MASTER CAUTION puts
   both out. Pressing the six-pack recalls: every caption lights while it is held, and on
   release those still at fault come back with MASTER CAUTION. The fault cleared, MASTER
   CAUTION stays until reset. Then the overheat/fire test: FIRE WARN, MASTER CAUTION and
   OVHT/DET; FIRE WARN pressed silences the bell and goes out; the rest go with the test
   switch. */
enum : int16_t { FIRE_WARN = 0, MASTER_CAUTION };
enum : int16_t { CPT_FLT_CONT = 2, CPT_ELEC, CPT_IRS, CPT_APU, CPT_FUEL, CPT_OVHT_DET };
enum : int16_t { FO_ANTI_ICE = 2, FO_ENG, FO_HYD, FO_OVERHEAD, FO_DOORS, FO_AIR_COND };

#define SIX_LIT(t, first)  LIT(t, first), LIT(t, (first) + 1), LIT(t, (first) + 2), \
                           LIT(t, (first) + 3), LIT(t, (first) + 4), LIT(t, (first) + 5)
#define SIX_DARK(t, first) DARK(t, first), DARK(t, (first) + 1), DARK(t, (first) + 2), \
                           DARK(t, (first) + 3), DARK(t, (first) + 4), DARK(t, (first) + 5)

// The captain's side: a fuel pump's low pressure gives FUEL.
constexpr Step MCS_L[] = {
    LIT(1000, CPT_FUEL), LIT(1000, MASTER_CAUTION),
    TAP(3500, "MASTER CAUTION L"), DARK(3800, MASTER_CAUTION), DARK(3800, CPT_FUEL),
    PRESS(7000, "RECALL L"), SIX_LIT(7050, CPT_FLT_CONT), LIT(7050, MASTER_CAUTION),
    RELEASE(8500), SIX_DARK(8550, CPT_FLT_CONT), LIT(8550, CPT_FUEL),
    DARK(11000, CPT_FUEL),
    TAP(12500, "MASTER CAUTION L"), DARK(12800, MASTER_CAUTION),
    LIT(15000, FIRE_WARN), LIT(15000, MASTER_CAUTION), LIT(15000, CPT_OVHT_DET),
    TAP(17000, "FIRE WARN L"), DARK(17300, FIRE_WARN),
    DARK(19500, MASTER_CAUTION), DARK(19500, CPT_OVHT_DET),
};

// The first officer's side: a door left open gives DOORS. OVHT/DET is on the captain's
// six-pack, so here the fire test shows FIRE WARN and MASTER CAUTION.
constexpr Step MCS_R[] = {
    LIT(1000, FO_DOORS), LIT(1000, MASTER_CAUTION),
    TAP(3500, "MASTER CAUTION R"), DARK(3800, MASTER_CAUTION), DARK(3800, FO_DOORS),
    PRESS(7000, "RECALL R"), SIX_LIT(7050, FO_ANTI_ICE), LIT(7050, MASTER_CAUTION),
    RELEASE(8500), SIX_DARK(8550, FO_ANTI_ICE), LIT(8550, FO_DOORS),
    DARK(11000, FO_DOORS),
    TAP(12500, "MASTER CAUTION R"), DARK(12800, MASTER_CAUTION),
    LIT(15000, FIRE_WARN), LIT(15000, MASTER_CAUTION),
    TAP(17000, "FIRE WARN R"), DARK(17300, FIRE_WARN),
    DARK(19500, MASTER_CAUTION),
};

/* ---- Master caution, both six-packs -----------------------------------------------
   The same story with both: FUEL on the captain's side and DOORS on the first officer's.
   A recall lights every caption on both six-packs. */
enum : int16_t { BOTH_FUEL = 6, BOTH_OVHT_DET = 7, BOTH_FO_FIRST = 8, BOTH_HYD = 10, BOTH_DOORS = 12 };
constexpr Step MCS_BOTH[] = {
    LIT(1000, BOTH_FUEL), LIT(1000, BOTH_DOORS), LIT(1000, MASTER_CAUTION),
    TAP(3500, "MASTER CAUTION"), DARK(3800, MASTER_CAUTION), DARK(3800, BOTH_FUEL), DARK(3800, BOTH_DOORS),
    PRESS(7000, "RECALL L"), SIX_LIT(7050, CPT_FLT_CONT), SIX_LIT(7050, BOTH_FO_FIRST), LIT(7050, MASTER_CAUTION),
    RELEASE(8500), SIX_DARK(8550, CPT_FLT_CONT), SIX_DARK(8550, BOTH_FO_FIRST),
    LIT(8550, BOTH_FUEL), LIT(8550, BOTH_DOORS),
    DARK(11000, BOTH_FUEL),
    DARK(12000, BOTH_DOORS),
    TAP(13500, "MASTER CAUTION"), DARK(13800, MASTER_CAUTION),
    LIT(16000, FIRE_WARN), LIT(16000, MASTER_CAUTION), LIT(16000, BOTH_OVHT_DET),
    TAP(18000, "FIRE WARN"), DARK(18300, FIRE_WARN),
    DARK(20500, MASTER_CAUTION), DARK(20500, BOTH_OVHT_DET),
};

/* ---- Master caution with the AFDS lights ------------------------------------------
   The autoflight warnings: the autopilot disconnected -- A/P flashes red until pressed; the
   autothrottle disengaged -- A/T flashes red until pressed; an airspeed error -- A/T
   flashes amber until the speed is back; an FMC alert message -- FMC amber until pressed.
   Then a caution and its reset. */
enum : int16_t { AFDS_AP = 14, AFDS_AT, AFDS_FMC };
constexpr Step MCS_AFDS[] = {
    FLASH(1000, AFDS_AP),
    TAP(4000, "AFDS AP"), DARK(4300, AFDS_AP),
    FLASH(6500, AFDS_AT),
    TAP(9500, "AFDS AT"), DARK(9800, AFDS_AT),
    ALT_FLASH(12500, AFDS_AT),
    DARK(16500, AFDS_AT),
    LIT(18500, AFDS_FMC),
    TAP(21500, "AFDS FMC"), DARK(21800, AFDS_FMC),
    LIT(24000, BOTH_HYD), LIT(24000, MASTER_CAUTION),
    TAP(26500, "MASTER CAUTION"), DARK(26800, MASTER_CAUTION), DARK(26800, BOTH_HYD),
};

#undef SIX_LIT
#undef SIX_DARK
#undef TAP
#undef RELEASE
#undef PRESS
#undef SEND
#undef ALT_FLASH
#undef FLASH
#undef DARK
#undef LIT

template <size_t N> constexpr uint8_t countOf(const Step (&)[N]) { return (uint8_t)N; }

const Demo::Scene SCENES[] = {
    {CUSTOMDEVICE_TYPE_DOOR, DOORS, countOf(DOORS), 26000},
    {CUSTOMDEVICE_TYPE_IRS, IRS, countOf(IRS), 28000},
    {CUSTOMDEVICE_TYPE_ISDU, ISDU, countOf(ISDU), 33000},
    {CUSTOMDEVICE_TYPE_ELEC, ELEC, countOf(ELEC), 29000},
    {CUSTOMDEVICE_TYPE_FCTL, FCTL, countOf(FCTL), 23000},
    {CUSTOMDEVICE_TYPE_AIR, AIR, countOf(AIR), 24000},
    {CUSTOMDEVICE_TYPE_MCS_L, MCS_L, countOf(MCS_L), 23000},
    {CUSTOMDEVICE_TYPE_MCS_R, MCS_R, countOf(MCS_R), 23000},
    {CUSTOMDEVICE_TYPE_MCS_BOTH, MCS_BOTH, countOf(MCS_BOTH), 24000},
    {CUSTOMDEVICE_TYPE_MCS_BOTH_AFDS, MCS_AFDS, countOf(MCS_AFDS), 30000},
};

constexpr size_t textLength(const char *s)
{
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

// A scene's steps must be in time order -- the player takes them in turn -- and all inside
// its length, with payloads that fit the player's 16-byte buffer.
template <size_t N> constexpr bool wellFormed(const Step (&s)[N], uint16_t length)
{
    for (size_t i = 0; i < N; ++i) {
        if (s[i].at >= length) return false;
        if (i && s[i].at < s[i - 1].at) return false;
        if ((s[i].action == Demo::SET || s[i].action == Demo::PRESS) && !s[i].text) return false;
        if (s[i].action == Demo::SET && textLength(s[i].text) > 15) return false;
    }
    return true;
}
static_assert(wellFormed(DOORS, 26000) && wellFormed(IRS, 28000) && wellFormed(ISDU, 33000) &&
                  wellFormed(ELEC, 29000) && wellFormed(FCTL, 23000) && wellFormed(AIR, 24000) &&
                  wellFormed(MCS_L, 23000) && wellFormed(MCS_R, 23000) &&
                  wellFormed(MCS_BOTH, 24000) && wellFormed(MCS_AFDS, 30000),
              "a demo scene's steps are out of order, past its length, or a payload is too long");

} // namespace

namespace Demo
{
uint8_t      sceneCount() { return (uint8_t)(sizeof(SCENES) / sizeof(SCENES[0])); }
const Scene &scene(uint8_t index) { return SCENES[index < sceneCount() ? index : 0]; }
int8_t       findScene(const char *type)
{
    for (uint8_t i = 0; i < sceneCount(); ++i)
        if (type && strcmp(type, SCENES[i].type) == 0) return (int8_t)i;
    return -1;
}
} // namespace Demo
