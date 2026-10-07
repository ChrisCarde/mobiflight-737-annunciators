#include "Splash.h"
#include "Board.h"
#include "Gfx.h"
#include "Layout.h"
#include "LogoSet.h"
#include "PanelGfx.h"
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
constexpr int16_t TITLE_TO_INFO = 37;  // second title line to the version line
constexpr int16_t INFO_PITCH    = 23;  // version line to date line
constexpr int16_t LABEL_GAP     = 11;  // FIRMWARE / BUILT to their values
constexpr int16_t CREDIT_PITCH  = 15;
constexpr int16_t CREDIT_SEP    = 16;  // between two credits that share a line
constexpr int16_t RULE_GAP      = 12;  // the rule to the text either side of it
constexpr int16_t NOTE_PITCH    = 29;  // NO PANEL CONFIGURED to the line under it
#else
constexpr int16_t LOGO_BOX      = 120;
constexpr int16_t LOGO_GAP      = 16;
constexpr int16_t TITLE_PITCH   = 19;
constexpr int16_t TITLE_TO_INFO = 28;
constexpr int16_t INFO_PITCH    = 17;
constexpr int16_t LABEL_GAP     = 8;
constexpr int16_t CREDIT_PITCH  = 11;
constexpr int16_t CREDIT_SEP    = 12;
constexpr int16_t RULE_GAP      = 9;
constexpr int16_t NOTE_PITCH    = 22;
#endif
constexpr int16_t TITLE_TRACK = 1; // letter-spaced a pixel, like a panel header

constexpr char TITLE_1[]       = "737 ANNUNCIATOR";
constexpr char TITLE_2[]       = "PANELS";
constexpr char LABEL_VERSION[] = "FIRMWARE";
constexpr char LABEL_DATE[]    = "BUILT";
constexpr char NO_PANEL[]      = "NO PANEL CONFIGURED";
constexpr char NO_PANEL_HINT[] = "ADD ONE IN MOBIFLIGHT CONNECTOR";

/* The credits. The display library differs by board, and Gfx.h is the one place a library
   is named, so that line comes from there. The silhouette's licence asks for its author and
   licence to be credited wherever it is shown -- this is where it is shown. */
struct Credit {
    const char *first;
    const char *second; // drawn after the first on the same line, or nullptr
};
constexpr Credit CREDITS[] = {
    {"BY CHRIS CARDE", nullptr},
    {"GITHUB.COM/CHRISCARDE/MOBIFLIGHT-737-ANNUNCIATORS", nullptr},
    {"CORE FIRMWARE: MOBIFLIGHT AND ELRAL (ESP32)", nullptr},
    {Gfx::LIBRARY_CREDIT, "FONT: DM SANS (OFL)"},
    {"737 SILHOUETTE: PETER JAMES LOWDEN (CC BY-SA 4.0)", nullptr},
};
constexpr int16_t CREDIT_COUNT = (int16_t)(sizeof(CREDITS) / sizeof(CREDITS[0]));

constexpr int16_t widthOf(PanelFont f, const char *s, int16_t track = 0)
{
    return s ? spacedWidthOf(fontOf(f), s, track) : 0;
}
constexpr int16_t creditWidth(const Credit &c)
{
    return (int16_t)(widthOf(FONT_TINY, c.first) +
                     (c.second ? CREDIT_SEP + widthOf(FONT_TINY, c.second) : 0));
}

constexpr int16_t CAP_LARGE = fontOf(FONT_LARGE).capHeight;
constexpr int16_t CAP_SMALL = fontOf(FONT_SMALL).capHeight;
constexpr int16_t CAP_TINY  = fontOf(FONT_TINY).capHeight;

// The credits: the last line's caps rest on the bottom padding, the rest stack up from it.
// drawSpacedText() takes the row the caps are centred on.
constexpr int16_t CREDIT_LAST_MID  = (int16_t)(PANEL_H - PAD_BOTTOM - (CAP_TINY + 1) / 2);
constexpr int16_t CREDIT_FIRST_MID = (int16_t)(CREDIT_LAST_MID - (CREDIT_COUNT - 1) * CREDIT_PITCH);
constexpr int16_t RULE_Y           = (int16_t)(CREDIT_FIRST_MID - CAP_TINY / 2 - RULE_GAP);

// Above the rule: the logo, and the name and version beside it, each centred in the height.
constexpr int16_t UPPER_TOP = MARGIN;
constexpr int16_t UPPER_H   = (int16_t)(RULE_Y - RULE_GAP - UPPER_TOP);
constexpr int16_t LOGO_X    = (int16_t)(MARGIN + (LOGO_BOX - Logo::W) / 2);
constexpr int16_t LOGO_Y    = (int16_t)(UPPER_TOP + (UPPER_H - Logo::H) / 2);

constexpr int16_t COLUMN_X  = (int16_t)(MARGIN + LOGO_BOX + LOGO_GAP);
constexpr int16_t COLUMN_W  = (int16_t)(PANEL_W - MARGIN - COLUMN_X);
constexpr int16_t BLOCK_H   = (int16_t)(CAP_LARGE / 2 + TITLE_PITCH + TITLE_TO_INFO + INFO_PITCH +
                                        (CAP_SMALL + 1) / 2);
constexpr int16_t TITLE_MID = (int16_t)(UPPER_TOP + (UPPER_H - BLOCK_H) / 2 + CAP_LARGE / 2);
constexpr int16_t INFO_MID  = (int16_t)(TITLE_MID + TITLE_PITCH + TITLE_TO_INFO);
constexpr int16_t LABEL_W   = widthOf(FONT_SMALL, LABEL_VERSION) > widthOf(FONT_SMALL, LABEL_DATE)
                                  ? widthOf(FONT_SMALL, LABEL_VERSION)
                                  : widthOf(FONT_SMALL, LABEL_DATE);
constexpr int16_t VALUE_X   = (int16_t)(COLUMN_X + LABEL_W + LABEL_GAP);

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
    for (const Credit &c : CREDITS)
        if (creditWidth(c) > PANEL_W - 2 * MARGIN) return false;
    return true;
}

static_assert(Logo::W <= LOGO_BOX && Logo::H <= LOGO_BOX,
              "the logo is bigger than its box -- regenerate it with tools/make_logo.py");
static_assert(Logo::H <= UPPER_H && BLOCK_H <= UPPER_H,
              "the logo and the name do not fit above the credits");
static_assert(widthOf(FONT_LARGE, TITLE_1, TITLE_TRACK) <= COLUMN_W &&
                  widthOf(FONT_LARGE, TITLE_2, TITLE_TRACK) <= COLUMN_W,
              "the name is wider than the column beside the logo");
static_assert(valuesFit(), "a version or a build date would run off the screen");
static_assert(creditsFit(), "a credit line is wider than the screen");
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

} // namespace

void Splash::draw()
{
    if (!PanelGfx::ready()) return;
    PanelGfx::clearPanel();
    drawLogo();

    useScreenFont(FONT_LARGE);
    text(COLUMN_X, TITLE_MID, TITLE_1, Theme::Placard, TITLE_TRACK);
    text(COLUMN_X, (int16_t)(TITLE_MID + TITLE_PITCH), TITLE_2, Theme::Placard, TITLE_TRACK);

    useScreenFont(FONT_SMALL);
    text(COLUMN_X, INFO_MID, LABEL_VERSION, Theme::HeaderFg);
    text(VALUE_X, INFO_MID, SPLASH_VERSION, Theme::Placard);
    text(COLUMN_X, (int16_t)(INFO_MID + INFO_PITCH), LABEL_DATE, Theme::HeaderFg);
    text(VALUE_X, (int16_t)(INFO_MID + INFO_PITCH), ANNUN_BUILD_DATE, Theme::Placard);

    PanelGfx::drawRule(MARGIN, RULE_Y, (int16_t)(PANEL_W - 2 * MARGIN), Theme::Rule);

    useScreenFont(FONT_TINY);
    for (int16_t i = 0; i < CREDIT_COUNT; ++i) {
        const int16_t y = (int16_t)(CREDIT_FIRST_MID + i * CREDIT_PITCH);
        const int16_t x = text(MARGIN, y, CREDITS[i].first, Theme::IruLabel);
        if (CREDITS[i].second) text((int16_t)(x + CREDIT_SEP), y, CREDITS[i].second, Theme::IruLabel);
    }

    useScreenFont(FONT_SMALL); // what the panels expect to find loaded
}

void Splash::drawNoPanel()
{
    if (!PanelGfx::ready()) return;
    PanelGfx::clearPanel();
    const int16_t top = (int16_t)((PANEL_H - NOTE_PITCH) / 2);
    useScreenFont(FONT_LARGE);
    centred(top, NO_PANEL, Theme::Placard, TITLE_TRACK);
    useScreenFont(FONT_SMALL);
    centred((int16_t)(top + NOTE_PITCH), NO_PANEL_HINT, Theme::HeaderFg);
}

#if ANNUN_BACKEND_HOST

// The preview renders the two screens and has no clock: nothing here runs.
void          Splash::start() {}
Splash::Phase Splash::phase() { return DONE; }
void          Splash::claim() {}
void          Splash::end() {}

#else

#include <atomic>

namespace
{
// DONE until start(), so nothing waits on a splash that never began.
std::atomic<uint8_t> s_phase{Splash::DONE};
std::atomic<bool>    s_claimed{false}; // a panel has been configured
std::atomic<bool>    s_drawing{false}; // the task is drawing NO PANEL CONFIGURED

// Moves the phase on, unless end() has already finished it. Exactly one of the two wins.
bool advance(Splash::Phase from, Splash::Phase to)
{
    uint8_t expected = from;
    return s_phase.compare_exchange_strong(expected, to);
}

void splashClock(void *)
{
    vTaskDelay(pdMS_TO_TICKS(Splash::PHASE_MS));
    if (advance(Splash::SHOWING_SPLASH, Splash::IDENTIFYING)) {
        // A configured panel identifies itself: the main task brings it up now, lit. Without
        // one, say so. The flag is raised before the claim is looked at, and claim() sets the
        // claim before it looks at the flag, so a panel configured at this very moment either
        // stops this drawing or waits for it to finish -- never both drawing at once.
        s_drawing.store(true);
        if (!s_claimed.load()) {
            Splash::drawNoPanel();
            PanelGfx::present();
        }
        s_drawing.store(false);
    }
    vTaskDelay(pdMS_TO_TICKS(Splash::PHASE_MS));
    // A panel turns its own backlight off, in the main task, when it sees the phase change.
    if (advance(Splash::IDENTIFYING, Splash::DONE) && !s_claimed.load())
        digitalWrite(Board::BACKLIGHT_PIN, LOW);
    vTaskDelete(nullptr);
}
} // namespace

void Splash::start()
{
    PanelGfx::begin(0); // the display but not its backlight: that pin is driven from here
    draw();
    PanelGfx::present();
    s_phase.store(SHOWING_SPLASH);
    digitalWrite(Board::BACKLIGHT_PIN, HIGH); // initVariant() made it an output

    // A task rather than a timer, because the second phase may have to draw, and drawing
    // wants more stack than a timer callback is given.
    if (xTaskCreate(splashClock, "splash", 8192, nullptr, 1, nullptr) != pdPASS) {
        s_phase.store(DONE); // nothing to end it: better no splash than one that stays
        digitalWrite(Board::BACKLIGHT_PIN, LOW);
    }
}

Splash::Phase Splash::phase() { return (Phase)s_phase.load(); }

void Splash::claim()
{
    s_claimed.store(true);
    while (s_drawing.load()) delay(1);
}

void Splash::end() { s_phase.store(DONE); }

#endif
