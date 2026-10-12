#include "Splash.h"
#include "Board.h"
#include "Gfx.h"
#include "Layout.h"
#include "LogoSet.h"
#include "PanelGfx.h"
#include "Panels.h"
#include "Theme.h"

// The build date, written into the build directory by get_version.py. A header there rather
// than __DATE__, which would only change when this file happened to be recompiled.
#if __has_include("BuildInfo.h")
#include "BuildInfo.h"
#endif
#ifndef ANNUN_BUILD_DATE
#define ANNUN_BUILD_DATE "UNKNOWN"
#endif

// The firmware version arrives from get_version.py as a bare token (-DBUILD_VERSION=0.1.0),
// so it has to be made a string, as the core's Config.cpp does with it.
#define SPLASH_STR2(x) #x
#define SPLASH_STR(x)  SPLASH_STR2(x)
#ifdef BUILD_VERSION
#define SPLASH_VERSION SPLASH_STR(BUILD_VERSION)
#else
#define SPLASH_VERSION "DEV"
#endif

/* **********************************************************************************
    Layout. The logo sits on the left with the name and the version beside it, and the
    credits run along the bottom under a rule. The seeds follow the panels' rule: x4/3 at
    480, which is the same size in the hand, since both screens are the same height.
********************************************************************************** */

namespace
{

using LY::MARGIN;
using LY::PAD_BOTTOM;
using LY::PANEL_H;
using LY::PANEL_W;

#if ANNUN_PANEL_RES == 480
constexpr int16_t LOGO_BOX      = 160; // the square tools/make_logo.py fits the logo into
constexpr int16_t LOGO_GAP      = 21;  // logo to the text beside it
constexpr int16_t TITLE_PITCH   = 25;  // the two title lines, centre to centre
constexpr int16_t TITLE_TO_NAME = 28;  // second title line to the panel's name
constexpr int16_t NAME_TO_INFO  = 30;  // the panel's name to the version line
constexpr int16_t INFO_PITCH    = 23;  // version line to date line
constexpr int16_t LABEL_GAP     = 11;  // FIRMWARE / BUILT to their values
constexpr int16_t CREDIT_PITCH  = 18;  // the credit lines under the rule, centre to centre
constexpr int16_t RULE_GAP      = 12;  // the rule to the text either side of it
constexpr int16_t NOTE_PITCH    = 29;  // NO PANEL CONFIGURED to the line under it
constexpr int16_t BUTTON_W      = 200; // the DEMO push-light
constexpr int16_t BUTTON_H      = 69;
constexpr int16_t INFO_TO_BUTTON = 18; // the BUILT line's centre to the button's top
constexpr int16_t NOTE_TO_BUTTON = 27; // the hint line's centre to the button's top
constexpr int16_t CREDITS_W     = 112; // the CREDITS push-light, beside the credit lines
constexpr int16_t CREDITS_H     = 48;
constexpr int16_t PAGE_TITLE_MID = 30; // the credits page: its title,
constexpr int16_t PAGE_FIRST_MID = 72; // its first line,
constexpr int16_t PAGE_PITCH     = 23; // the lines' pitch,
constexpr int16_t PAGE_HINT_MID  = 300; // and TAP TO CLOSE
#else
constexpr int16_t LOGO_BOX      = 120;
constexpr int16_t LOGO_GAP      = 16;
constexpr int16_t TITLE_PITCH   = 19;
constexpr int16_t TITLE_TO_NAME = 22;
constexpr int16_t NAME_TO_INFO  = 23;
constexpr int16_t INFO_PITCH    = 17;
constexpr int16_t LABEL_GAP     = 8;
constexpr int16_t CREDIT_PITCH  = 13;
constexpr int16_t RULE_GAP      = 9;
constexpr int16_t NOTE_PITCH    = 22;
constexpr int16_t BUTTON_W      = 150;
constexpr int16_t BUTTON_H      = 52;
constexpr int16_t INFO_TO_BUTTON = 14;
constexpr int16_t NOTE_TO_BUTTON = 20;
constexpr int16_t CREDITS_W     = 84;
constexpr int16_t CREDITS_H     = 36;
constexpr int16_t PAGE_TITLE_MID = 22;
constexpr int16_t PAGE_FIRST_MID = 52;
constexpr int16_t PAGE_PITCH     = 17;
constexpr int16_t PAGE_HINT_MID  = 225;
#endif
constexpr int16_t TITLE_TRACK = 1; // letter-spaced a pixel, like a panel header

constexpr char TITLE_1[]       = "737 ANNUNCIATOR";
constexpr char TITLE_2[]       = "PANELS";
constexpr char LABEL_VERSION[] = "FIRMWARE";
constexpr char LABEL_DATE[]    = "BUILT";
constexpr char NO_PANEL[]      = "NO PANEL CONFIGURED";
constexpr char NO_PANEL_HINT[] = "ADD ONE IN MOBIFLIGHT CONNECTOR";
constexpr char NOT_CONFIGURED[] = "NOT CONFIGURED"; // the panel name, when there is none
constexpr char BUTTON_LINE_1[] = "PRESS FOR";
constexpr char BUTTON_LINE_2[] = "DEMO MODE";
constexpr char CREDITS_LEGEND[] = "CREDITS";
constexpr char PAGE_TITLE[]     = "CREDITS";
constexpr char PAGE_HINT[]      = "TAP TO CLOSE";

/* The credits. On the splash, the short list in readable type, with a CREDITS button beside
   it for the whole list on a page of its own. The silhouette's licence asks for its author
   and licence to be credited wherever it is shown, so that line is on both. The display
   library differs by board, and Gfx.h is the one place a library is named, so that line
   comes from there. */
constexpr const char *CREDITS[] = {
    "BY CHRIS CARDE",
    "GITHUB.COM/CHRISCARDE/",
    "MOBIFLIGHT-737-ANNUNCIATORS",
    "SILHOUETTE: PETER JAMES LOWDEN (CC BY-SA 4.0)",
};
constexpr int16_t CREDIT_COUNT  = (int16_t)(sizeof(CREDITS) / sizeof(CREDITS[0]));
constexpr int16_t BESIDE_BUTTON = 3; // the first lines, which the CREDITS button sits beside

constexpr const char *PAGE[] = {
    "BY CHRIS CARDE",
    "GITHUB.COM/CHRISCARDE/",
    "MOBIFLIGHT-737-ANNUNCIATORS",
    "",
    "CORE FIRMWARE: MOBIFLIGHT AND ELRAL (ESP32)",
    Gfx::LIBRARY_CREDIT,
    "FONT: DM SANS (OFL)",
    "737 SILHOUETTE: PETER JAMES LOWDEN",
    "CC BY-SA 4.0",
};
constexpr int16_t PAGE_COUNT = (int16_t)(sizeof(PAGE) / sizeof(PAGE[0]));

constexpr int16_t widthOf(PanelFont f, const char *s, int16_t track = 0)
{
    return s ? spacedWidthOf(fontOf(f), s, track) : 0;
}
constexpr int16_t CAP_LARGE = fontOf(FONT_LARGE).capHeight;
constexpr int16_t CAP_SMALL = fontOf(FONT_SMALL).capHeight;

// The credits: the last line's caps rest on the bottom padding, the rest stack up from it.
// drawSpacedText() takes the row the caps are centred on.
constexpr int16_t CREDIT_LAST_MID  = (int16_t)(PANEL_H - PAD_BOTTOM - (CAP_SMALL + 1) / 2);
constexpr int16_t CREDIT_FIRST_MID = (int16_t)(CREDIT_LAST_MID - (CREDIT_COUNT - 1) * CREDIT_PITCH);
constexpr int16_t RULE_Y           = (int16_t)(CREDIT_FIRST_MID - CAP_SMALL / 2 - RULE_GAP);

// The CREDITS button, at the right, beside the first lines and clear of the last.
constexpr int16_t CREDITS_X = (int16_t)(PANEL_W - MARGIN - CREDITS_W);
constexpr int16_t CREDITS_Y = (int16_t)(CREDIT_FIRST_MID - (CAP_SMALL + 1) / 2 - 2);

// Above the rule: the logo, and the name and version beside it, each centred in the height.
constexpr int16_t UPPER_TOP = MARGIN;
constexpr int16_t UPPER_H   = (int16_t)(RULE_Y - RULE_GAP - UPPER_TOP);
constexpr int16_t LOGO_X    = (int16_t)(MARGIN + (LOGO_BOX - Logo::W) / 2);
constexpr int16_t LOGO_Y    = (int16_t)(UPPER_TOP + (UPPER_H - Logo::H) / 2);

constexpr int16_t COLUMN_X  = (int16_t)(MARGIN + LOGO_BOX + LOGO_GAP);
constexpr int16_t COLUMN_W  = (int16_t)(PANEL_W - MARGIN - COLUMN_X);
constexpr int16_t BLOCK_H   = (int16_t)(CAP_LARGE / 2 + TITLE_PITCH + TITLE_TO_NAME + NAME_TO_INFO +
                                        INFO_PITCH + INFO_TO_BUTTON + BUTTON_H);
constexpr int16_t TITLE_MID = (int16_t)(UPPER_TOP + (UPPER_H - BLOCK_H) / 2 + CAP_LARGE / 2);
constexpr int16_t NAME_MID  = (int16_t)(TITLE_MID + TITLE_PITCH + TITLE_TO_NAME);
constexpr int16_t INFO_MID  = (int16_t)(NAME_MID + NAME_TO_INFO);
constexpr int16_t LABEL_W   = widthOf(FONT_SMALL, LABEL_VERSION) > widthOf(FONT_SMALL, LABEL_DATE)
                                  ? widthOf(FONT_SMALL, LABEL_VERSION)
                                  : widthOf(FONT_SMALL, LABEL_DATE);
constexpr int16_t VALUE_X   = (int16_t)(COLUMN_X + LABEL_W + LABEL_GAP);
// Centred in the space beside the logo, under the version.
constexpr int16_t BUTTON_X  = (int16_t)(COLUMN_X + (COLUMN_W - BUTTON_W) / 2);
constexpr int16_t BUTTON_Y  = (int16_t)(INFO_MID + INFO_PITCH + INFO_TO_BUTTON);

// NO PANEL CONFIGURED: the two lines and the button under them, centred as one block.
constexpr int16_t NOTE_BLOCK_H = (int16_t)(NOTE_PITCH + NOTE_TO_BUTTON + BUTTON_H);
constexpr int16_t NOTE_MID     = (int16_t)((PANEL_H - NOTE_BLOCK_H) / 2);
constexpr int16_t NOTE_BUTTON_X = (int16_t)((PANEL_W - BUTTON_W) / 2);
constexpr int16_t NOTE_BUTTON_Y = (int16_t)(NOTE_MID + NOTE_PITCH + NOTE_TO_BUTTON);

// The widest the two values can be: a four-part version (the most the Connector accepts),
// and a date in each month -- get_version.py writes them as "7 OCT 2026".
constexpr const char *WIDEST_VALUES[] = {
    "88.88.88.88",
    "30 JAN 2088", "30 FEB 2088", "30 MAR 2088", "30 APR 2088", "30 MAY 2088", "30 JUN 2088",
    "30 JUL 2088", "30 AUG 2088", "30 SEP 2088", "30 OCT 2088", "30 NOV 2088", "30 DEC 2088",
};

constexpr bool valuesFit()
{
    for (const char *v : WIDEST_VALUES)
        if (VALUE_X + widthOf(FONT_SMALL, v) > PANEL_W - MARGIN) return false;
    return true;
}
constexpr bool creditsFit()
{
    for (int16_t i = 0; i < CREDIT_COUNT; ++i) {
        const int16_t room = i < BESIDE_BUTTON ? CREDITS_X - 8 - MARGIN : PANEL_W - 2 * MARGIN;
        if (widthOf(FONT_SMALL, CREDITS[i]) > room) return false;
    }
    return true;
}
constexpr bool pageFits()
{
    for (const char *line : PAGE)
        if (widthOf(FONT_SMALL, line) > PANEL_W - 2 * MARGIN) return false;
    return PAGE_FIRST_MID + (PAGE_COUNT - 1) * PAGE_PITCH + CAP_SMALL < PAGE_HINT_MID - CAP_SMALL;
}

static_assert(Logo::W <= LOGO_BOX && Logo::H <= LOGO_BOX,
              "the logo is bigger than its box -- regenerate it with tools/make_logo.py");
static_assert(Logo::H <= UPPER_H && BLOCK_H <= UPPER_H,
              "the logo and the name do not fit above the credits");
static_assert(widthOf(FONT_LARGE, TITLE_1, TITLE_TRACK) <= COLUMN_W &&
                  widthOf(FONT_LARGE, TITLE_2, TITLE_TRACK) <= COLUMN_W,
              "the name is wider than the column beside the logo");
static_assert(valuesFit(), "a version or a build date would run off the screen");

constexpr bool namesFit()
{
    for (const char *name : Panels::NAMES)
        if (widthOf(FONT_LARGE, name, TITLE_TRACK) > COLUMN_W) return false;
    return widthOf(FONT_LARGE, NOT_CONFIGURED, TITLE_TRACK) <= COLUMN_W;
}
static_assert(namesFit(), "a panel's name is wider than the column beside the logo");
static_assert(creditsFit(), "a credit line runs into the CREDITS button or off the screen");
static_assert(pageFits(), "the credits page does not fit the screen");
static_assert(CREDITS_W >= LY::MIN_TOUCH_PX && CREDITS_H >= LY::MIN_TOUCH_PX,
              "the CREDITS button is smaller than a fingertip");
static_assert(CREDITS_Y + CREDITS_H <= CREDIT_FIRST_MID + BESIDE_BUTTON * CREDIT_PITCH - (CAP_SMALL + 1) / 2 - 2,
              "the CREDITS button runs into the credit line under it");
static_assert(CREDITS_Y > RULE_Y + 2, "the CREDITS button runs into the rule above it");
static_assert(widthOf(FONT_SMALL, CREDITS_LEGEND) + 16 <= CREDITS_W - 4,
              "the CREDITS button's legend does not fit its cap");
// The button is something to press: at least the smallest target the panels allow, with
// its legend inside the cap (2 px of bezel each side, as renderLampLines() draws it).
static_assert(BUTTON_W >= LY::MIN_TOUCH_PX && BUTTON_H >= LY::MIN_TOUCH_PX,
              "the DEMO button is smaller than a fingertip");
static_assert(BUTTON_W <= COLUMN_W, "the DEMO button is wider than the column it sits in");
static_assert(widthOf(FONT_LARGE, BUTTON_LINE_1) + 16 <= BUTTON_W - 4 &&
                  widthOf(FONT_LARGE, BUTTON_LINE_2) + 16 <= BUTTON_W - 4,
              "the DEMO button's legend does not fit its cap");
static_assert(2 * CAP_LARGE + (CAP_LARGE + 1) / 2 <= BUTTON_H - 12,
              "the DEMO button's two lines do not fit its height");
static_assert(NOTE_BUTTON_Y + BUTTON_H <= PANEL_H - MARGIN,
              "NO PANEL CONFIGURED and its button do not fit the screen");
static_assert(widthOf(FONT_LARGE, NO_PANEL, TITLE_TRACK) <= PANEL_W - 2 * MARGIN &&
                  widthOf(FONT_SMALL, NO_PANEL_HINT) <= PANEL_W - 2 * MARGIN,
              "NO PANEL CONFIGURED does not fit across the screen");

// The screen carries one font at a time, so switch it as the lines need.
void useScreenFont(PanelFont f)
{
    Gfx::Device &screen = PanelGfx::tft();
    screen.unloadFont();
    PanelGfx::useFont(screen, f);
}

int16_t text(int16_t x, int16_t yMid, const char *s, uint32_t colour, int16_t track = 0)
{
    PanelGfx::drawSpacedText(x, yMid, s, colour, track);
    return (int16_t)(x + PanelGfx::spacedTextWidth(s, track));
}

void centred(int16_t yMid, const char *s, uint32_t colour, int16_t track = 0)
{
    text((int16_t)((PANEL_W - PanelGfx::spacedTextWidth(s, track)) / 2), yMid, s, colour, track);
}

/* A pixel at a time, straight onto the screen. It needs no buffer -- which matters on the
   E32R32P, with no PSRAM -- and draws the same through every backend. The background was
   flattened in by make_logo.py, so pixels of that colour are already on the screen. */
void drawLogo()
{
    Gfx::Device &screen = PanelGfx::tft();
    for (int16_t y = 0; y < Logo::H; ++y) {
        const uint16_t *row = &Logo::PIXELS[y * Logo::W];
        for (int16_t x = 0; x < Logo::W; ++x)
            if (row[x] != Logo::BACKGROUND) screen.drawPixel(LOGO_X + x, LOGO_Y + y, row[x]);
    }
}

/* The buttons: push-lights, drawn by the same code as MASTER CAUTION -- a cap standing proud
   of its bezel. Unlit at rest, because they are buttons, not warnings, with legends in the
   white of the panels' printing so they read at a glance. Held, one goes in and lights white
   -- unmistakable even under a fingertip. */
int16_t s_buttonX = -1, s_buttonY = -1; // DEMO, where the last screen put it; -1 for nowhere
bool    s_creditsShown = false;         // the CREDITS button is on the screen

/* The panel's name under the title. In the lamps' amber, because it says what this board is;
   NOT CONFIGURED in the grey of the labels. Its own line, so it can be redrawn alone: the
   splash goes up before the board has read its config, and the name arrives a moment later. */
const char *s_panelName   = nullptr; // what the line shows; nullptr for nothing yet
bool        s_splashShown = false;   // the splash, not another screen, is what was drawn last

void drawPanelName()
{
    PanelGfx::clearRect(COLUMN_X, (int16_t)(NAME_MID - CAP_LARGE), COLUMN_W, (int16_t)(2 * CAP_LARGE));
    if (!s_panelName) return;
    useScreenFont(FONT_LARGE);
    const bool none = s_panelName == NOT_CONFIGURED;
    text(COLUMN_X, NAME_MID, s_panelName, none ? Theme::HeaderFg : 0xFFC24D, TITLE_TRACK);
    useScreenFont(FONT_SMALL);
}

void drawPushLight(int16_t x, int16_t y, int16_t w, int16_t h, const char *line1,
                   const char *line2, PanelFont font, bool pressed)
{
    Gfx::Canvas cap(&PanelGfx::tft());
    cap.setColorDepth(16);
    if (cap.createSprite(w, h) == nullptr) return;
    PanelFont loaded = font;
    PanelGfx::useFont(cap, loaded);
    LampStyle style = Theme::style(KIND_FILLED, LAMP_AMBER, false, false);
    if (pressed) {
        style.bg   = 0xE9EDF1; // lit white face, dark legend, as a lit push-light reads
        style.fg   = 0x1A1E23;
        style.lit  = true;
        style.glow = true;
    } else {
        style.fg = Theme::blend(Theme::Placard, style.bg, 210);
    }
    const LegendLine lines[] = {{line1, font}, {line2, font}};
    PanelGfx::renderLampLines(cap, style, lines, line2 ? 2 : 1, loaded, pressed);
    PanelGfx::push(cap, x, y);
    cap.unloadFont();
    cap.deleteSprite();
}

void drawButton(int16_t x, int16_t y, bool pressed)
{
    s_buttonX = x;
    s_buttonY = y;
    drawPushLight(x, y, BUTTON_W, BUTTON_H, BUTTON_LINE_1, BUTTON_LINE_2, FONT_LARGE, pressed);
}

void drawCreditsButton(bool pressed)
{
    s_creditsShown = true;
    drawPushLight(CREDITS_X, CREDITS_Y, CREDITS_W, CREDITS_H, CREDITS_LEGEND, nullptr, FONT_SMALL, pressed);
}

bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh)
{
    return x >= rx - LY::HIT_SLOP && x < rx + rw + LY::HIT_SLOP && y >= ry - LY::HIT_SLOP &&
           y < ry + rh + LY::HIT_SLOP;
}

} // namespace

Splash::Button Splash::buttonAt(int16_t x, int16_t y)
{
    if (s_buttonX >= 0 && inRect(x, y, s_buttonX, s_buttonY, BUTTON_W, BUTTON_H)) return DEMO_BUTTON;
    if (s_creditsShown && inRect(x, y, CREDITS_X, CREDITS_Y, CREDITS_W, CREDITS_H)) return CREDITS_BUTTON;
    return NO_BUTTON;
}

void Splash::showPanelName(const char *name)
{
    s_panelName = name;
    if (s_splashShown && PanelGfx::ready()) drawPanelName();
}

void Splash::pressButton(Button button, bool pressed)
{
    if (!PanelGfx::ready()) return;
    if (button == DEMO_BUTTON && s_buttonX >= 0) drawButton(s_buttonX, s_buttonY, pressed);
    if (button == CREDITS_BUTTON && s_creditsShown) drawCreditsButton(pressed);
}

void Splash::draw()
{
    if (!PanelGfx::ready()) return;
    PanelGfx::clearPanel();
    drawLogo();

    useScreenFont(FONT_LARGE);
    text(COLUMN_X, TITLE_MID, TITLE_1, Theme::Placard, TITLE_TRACK);
    text(COLUMN_X, (int16_t)(TITLE_MID + TITLE_PITCH), TITLE_2, Theme::Placard, TITLE_TRACK);
    drawPanelName();

    useScreenFont(FONT_SMALL);
    text(COLUMN_X, INFO_MID, LABEL_VERSION, Theme::HeaderFg);
    text(VALUE_X, INFO_MID, SPLASH_VERSION, Theme::Placard);
    text(COLUMN_X, (int16_t)(INFO_MID + INFO_PITCH), LABEL_DATE, Theme::HeaderFg);
    text(VALUE_X, (int16_t)(INFO_MID + INFO_PITCH), ANNUN_BUILD_DATE, Theme::Placard);
    drawButton(BUTTON_X, BUTTON_Y, false);

    PanelGfx::drawRule(MARGIN, RULE_Y, (int16_t)(PANEL_W - 2 * MARGIN), Theme::Rule);

    for (int16_t i = 0; i < CREDIT_COUNT; ++i)
        text(MARGIN, (int16_t)(CREDIT_FIRST_MID + i * CREDIT_PITCH), CREDITS[i], Theme::IruLabel);
    drawCreditsButton(false);

    useScreenFont(FONT_SMALL); // what the panels expect to find loaded
    s_splashShown = true;
}

void Splash::drawNoPanel()
{
    if (!PanelGfx::ready()) return;
    s_splashShown  = false;
    s_creditsShown = false;
    PanelGfx::clearPanel();
    useScreenFont(FONT_LARGE);
    centred(NOTE_MID, NO_PANEL, Theme::Placard, TITLE_TRACK);
    useScreenFont(FONT_SMALL);
    centred((int16_t)(NOTE_MID + NOTE_PITCH), NO_PANEL_HINT, Theme::HeaderFg);
    drawButton(NOTE_BUTTON_X, NOTE_BUTTON_Y, false);
}

/* Every credit, on a page of its own in readable type -- what the CREDITS button opens. Any
   tap closes it (see the splash's task). */
void Splash::drawCredits()
{
    if (!PanelGfx::ready()) return;
    s_splashShown  = false;
    s_creditsShown = false;
    s_buttonX      = -1;
    PanelGfx::clearPanel();
    useScreenFont(FONT_LARGE);
    centred(PAGE_TITLE_MID, PAGE_TITLE, Theme::Placard, TITLE_TRACK);
    useScreenFont(FONT_SMALL);
    for (int16_t i = 0; i < PAGE_COUNT; ++i)
        if (*PAGE[i]) centred((int16_t)(PAGE_FIRST_MID + i * PAGE_PITCH), PAGE[i], Theme::Placard);
    centred(PAGE_HINT_MID, PAGE_HINT, Theme::HeaderFg);
}

#if ANNUN_BACKEND_HOST

// The preview renders the screens and has no clock: nothing here runs.
void          Splash::start() {}
Splash::Phase Splash::phase() { return DONE; }
bool          Splash::ownsScreen() { return false; }
void          Splash::claim(const char *) {}
void          Splash::release() {}
void          Splash::waitIdle() {}
void          Splash::end() {}

#else

#include <atomic>
#include "Demo.h"
#include "Touch.h"
#include "TouchZones.h"
#include "commandmessenger.h"
#include <esp_heap_caps.h>
#include <string.h>

namespace
{
constexpr uint32_t FRAME_MS        = 20;   // the task's cadence: touch, the demo, the clock
constexpr uint32_t HOLD_TO_STOP_MS = 3000; // a finger held this long anywhere stops the demo
constexpr uint32_t SERIAL_QUIET_MS = 2000; // see reportHealth()
constexpr uint32_t TAP_SHOW_MS     = 150;  // a button stays lit this long after a tap
constexpr uint32_t CREDITS_MS      = 20000; // the credits page closes itself after this long

// DONE until start(), so nothing waits on a splash that never began.
std::atomic<uint8_t> s_phase{Splash::DONE};
std::atomic<bool>    s_claimed{false};  // a panel has been configured
std::atomic<bool>    s_drawing{false};  // the task is in the middle of a frame
std::atomic<bool>    s_stopDemo{false}; // end() asks the demo to stop
std::atomic<const char *> s_claimedName{nullptr}; // the configured panel's name, or none
constexpr uint32_t   NAME_WAIT_MS = 1000; // no panel by then: there is none, for now

// Moves the phase on, unless something else already has. Exactly one of the two wins.
bool advance(uint8_t from, uint8_t to)
{
    uint8_t expected = from;
    return s_phase.compare_exchange_strong(expected, to);
}

// Whether the screen is the task's: the splash and the demo always are; NO PANEL CONFIGURED
// only while no panel has claimed it.
bool taskOwns()
{
    const uint8_t phase = s_phase.load();
    return phase == Splash::SHOWING_SPLASH || phase == Splash::DEMO ||
           (phase == Splash::IDENTIFYING && !s_claimed.load());
}

/* A frame of the task's. The flag goes up before ownership is looked at, and claim() and
   end() change ownership before they look at the flag, so whatever happens at the same
   moment either stops this frame or waits for it -- the two tasks never draw at once. */
bool beginFrame()
{
    s_drawing.store(true);
    if (taskOwns()) return true;
    s_drawing.store(false);
    return false;
}
void endFrame() { s_drawing.store(false); }

/* The buttons. A press on one shows it pushed in; letting go on it is a tap, letting go
   elsewhere does nothing. After the demo has been stopped by a hold, the finger is still
   down: nothing counts until it lifts. */
Splash::Button s_pressing = Splash::NO_BUTTON;
bool           s_waitLift = false;
int16_t        s_touchX = 0, s_touchY = 0;

Splash::Button buttonTapped()
{
    int16_t            x, y;
    const Touch::Event e = Touch::poll(x, y);
    if (e == Touch::DOWN && !s_waitLift && s_pressing == Splash::NO_BUTTON) {
        s_pressing = Splash::buttonAt(x, y);
        if (s_pressing != Splash::NO_BUTTON) {
            Splash::pressButton(s_pressing, true);
            PanelGfx::present();
        }
    }
    if (s_pressing != Splash::NO_BUTTON && e != Touch::UP) {
        s_touchX = x; // follows the finger while it is down
        s_touchY = y;
    }
    if (e != Touch::UP) return Splash::NO_BUTTON;
    s_waitLift                 = false;
    const Splash::Button press = s_pressing;
    if (press == Splash::NO_BUTTON) return Splash::NO_BUTTON;
    s_pressing = Splash::NO_BUTTON;
    if (Splash::buttonAt(s_touchX, s_touchY) == press) {
        // Let go on the button: it stays lit a moment, so even a quick tap is seen to
        // register, then what it opens takes the screen.
        vTaskDelay(pdMS_TO_TICKS(TAP_SHOW_MS));
        return press;
    }
    Splash::pressButton(press, false); // slid off it: nothing
    PanelGfx::present();
    return Splash::NO_BUTTON;
}

/* The credits page, while it is open. Any tap closes it, or it closes itself; either way the
   splash comes back for a fresh 10 seconds, so there is time to press DEMO after reading. */
bool creditsClosed(uint32_t now, uint32_t openedAt)
{
    int16_t x, y;
    return Touch::poll(x, y) == Touch::UP || now - openedAt >= CREDITS_MS;
}

/* One line per demo loop, for checking a board left running for hours: the heap the panels'
   sprites come and go from, and this task's stack. CmdMessenger is not safe to share between
   tasks, so it is written in one piece, and only when the Connector has been quiet a while:
   the main task only ever writes in reply to it. */
void reportHealth(uint32_t &reported)
{
    const uint32_t loops = Demo::loops();
    if (loops == reported) return;
    reported = loops;
    if (millis() - getLastCommandMillis() < SERIAL_QUIET_MS) return;
    char line[120];
    snprintf(line, sizeof(line),
             "%d,Annunciator: demo loop %lu heap free %u largest %u stack free %u;\r\n", (int)kDebug,
             (unsigned long)loops, (unsigned)ESP.getFreeHeap(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    Serial.write((const uint8_t *)line, strlen(line));
}

void startDemo(uint32_t now)
{
    s_pressing = Splash::NO_BUTTON;
    TouchZones::setDemo(true);
    Demo::start(now);
}

void stopDemo()
{
    Demo::stop();
    TouchZones::setDemo(false);
}

void splashClock(void *)
{
    const uint32_t bootAt    = millis();
    uint32_t       phaseAt   = bootAt;
    uint32_t       reported  = 0;
    uint32_t       creditsAt = 0; // when the credits page opened; 0 while it is closed

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
        const uint32_t now   = millis();
        const uint8_t  phase = s_phase.load();
        if (phase == Splash::DONE) break;

        if (phase == Splash::DEMO) {
            if (!beginFrame()) continue;
            if (s_stopDemo.load()) {
                // MobiFlight sent a value: the configured panel takes over, from a fresh init.
                stopDemo();
                s_phase.store(Splash::DONE);
                endFrame();
                break;
            }
            if (TouchZones::demoPoll() >= HOLD_TO_STOP_MS) {
                // Held: back to the start -- the splash and its button, identify, dark.
                stopDemo();
                s_waitLift = true;
                Splash::draw();
                PanelGfx::present();
                PanelGfx::setShowing(true);
                if (advance(Splash::DEMO, Splash::SHOWING_SPLASH)) phaseAt = now;
                endFrame();
                continue;
            }
            Demo::tick(now);
            endFrame();
            reportHealth(reported);
            continue;
        }

        // The credits page, over the splash: the splash's clock waits for it.
        if (creditsAt) {
            if (!beginFrame()) continue;
            if (creditsClosed(now, creditsAt)) {
                creditsAt = 0;
                Splash::draw();
                PanelGfx::present();
                phaseAt = now;
            }
            endFrame();
            continue;
        }

        // The splash, or NO PANEL CONFIGURED: keep the panel's name current, and watch the
        // buttons.
        if (beginFrame()) {
            if (phase == Splash::SHOWING_SPLASH) {
                // The configured panel's name as soon as setup() has read the config -- or a
                // config upload changes it -- and NOT CONFIGURED once there plainly is none.
                const char *name = s_claimedName.load();
                if (!name && now - bootAt >= NAME_WAIT_MS) name = NOT_CONFIGURED;
                if (name != s_panelName) {
                    Splash::showPanelName(name);
                    PanelGfx::present();
                }
            }
            const Splash::Button tapped = buttonTapped();
            if (tapped == Splash::DEMO_BUTTON) {
                s_stopDemo.store(false);
                if (advance(phase, Splash::DEMO)) {
                    reported = 0;
                    startDemo(now);
                }
            } else if (tapped == Splash::CREDITS_BUTTON) {
                Splash::drawCredits();
                PanelGfx::present();
                creditsAt = now ? now : 1;
            }
            endFrame();
            if (tapped != Splash::NO_BUTTON) continue;
        }

        if (s_pressing != Splash::NO_BUTTON || now - phaseAt < Splash::PHASE_MS) continue;

        if (phase == Splash::SHOWING_SPLASH) {
            // A configured panel identifies itself: the main task brings it up now, lit.
            // Without one, say so.
            if (!advance(Splash::SHOWING_SPLASH, Splash::IDENTIFYING)) continue;
            phaseAt = now;
            if (beginFrame()) {
                Splash::drawNoPanel();
                PanelGfx::present();
                endFrame();
            }
        } else if (phase == Splash::IDENTIFYING) {
            if (!advance(Splash::IDENTIFYING, Splash::DONE)) continue;
            // A panel turns its own backlight off, in the main task, when it sees the change;
            // without one, nothing else ever will.
            s_drawing.store(true);
            if (!s_claimed.load()) PanelGfx::setShowing(false);
            s_drawing.store(false);
            break;
        }
    }
    vTaskDelete(nullptr);
}
} // namespace

void Splash::start()
{
    // The backlight goes through PanelGfx from the start, like a panel's. Once a demo panel
    // has run, the pin belongs to the LEDC peripheral, and a plain digitalWrite() from here
    // would no longer reach it.
    PanelGfx::begin(Board::BACKLIGHT_PIN);
    Touch::begin(); // for the DEMO button
    draw();
    PanelGfx::present();
    s_phase.store(SHOWING_SPLASH);
    PanelGfx::setShowing(true);

    // A task of its own, because the splash has to run on a board where the main loop never
    // calls the custom device, and its demo draws for minutes at a time.
    if (xTaskCreate(splashClock, "splash", 12288, nullptr, 1, nullptr) != pdPASS) {
        s_phase.store(DONE); // nothing to end it: better no splash than one that stays
        PanelGfx::setShowing(false);
    }
}

Splash::Phase Splash::phase() { return (Phase)s_phase.load(); }

bool Splash::ownsScreen()
{
    const uint8_t phase = s_phase.load();
    return phase == SHOWING_SPLASH || phase == DEMO;
}

void Splash::claim(const char *panelName)
{
    s_claimedName.store(panelName);
    s_claimed.store(true);
    waitIdle();
}

void Splash::release()
{
    s_claimed.store(false);
    s_claimedName.store(nullptr);
}

void Splash::waitIdle()
{
    while (s_drawing.load()) delay(1);
}

void Splash::end()
{
    for (;;) {
        uint8_t phase = s_phase.load();
        if (phase == DONE) break;
        if (phase == DEMO) {
            // The task stops the demo between frames, stops its panel, and moves to DONE.
            s_stopDemo.store(true);
            delay(1);
            continue;
        }
        if (s_phase.compare_exchange_strong(phase, (uint8_t)DONE)) break;
    }
    waitIdle();
}

#endif
