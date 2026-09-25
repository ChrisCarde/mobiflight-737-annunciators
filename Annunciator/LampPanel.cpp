#include "LampPanel.h"
#include "Panel.h"
#include <string.h>
#include <stdlib.h>

namespace LampPanel
{

static const int16_t PANEL_W  = 320;
static const int16_t HEADER_X = 10, HEADER_Y = 9, HEADER_H = 13;
static const int16_t HEADER_R = PANEL_W - 10;

static const uint32_t DEMO_DWELL_MS = 2500;

static const Def *s_def = nullptr;

/* The zone currently held down, or -1. A touch zone and a lamp are separate things -- a
   zone is a rectangle with a MobiFlight button name, a lamp is something that lights -- and
   the two line up differently on different panels: a door lamp has a zone exactly over it,
   while a six-pack's RECALL zone covers the whole face and every caption cell inside it.
   Both are handled by asking which lamps a zone and a lamp share space with, so nothing
   needs a second table kept in step with the first. */
static int8_t s_pressedZone = -1;

static bool lampIsPressed(uint8_t lamp);  // both defined with the touch feedback below
static bool plateIsPressed(uint8_t plate);
static void drawPlate(uint8_t plate);

/* Lamps are drawn in a sprite of their own size and pushed. A panel has only a few
   distinct sizes, so there is one sprite per size -- a MASTER CAUTION square and a six-pack
   caption need different ones -- created at init. */
struct Slot {
    int16_t      w, h;
    Gfx::Canvas *spr;
    PanelFont    font; // what the sprite has loaded
};
static const uint8_t MAX_SLOTS = 4;
static Slot          s_slots[MAX_SLOTS];
static uint8_t       s_slotCount = 0;

static uint8_t  s_state[MAX_LAMPS];
static uint32_t s_dirty    = 0;
static bool     s_dim      = false;
static bool     s_lampTest = false;

static bool     s_blinkOn   = true;
static uint32_t s_blinkAt   = 0;
static bool     s_selfTest  = false;
static uint8_t  s_demoIndex = 0;
static uint32_t s_demoAt    = 0;

static Slot *slotFor(int16_t w, int16_t h)
{
    for (uint8_t i = 0; i < s_slotCount; ++i)
        if (s_slots[i].w == w && s_slots[i].h == h) return s_slots[i].spr ? &s_slots[i] : nullptr;
    return nullptr;
}

static void createSlots()
{
    for (uint8_t i = 0; i < s_def->lampCount; ++i) {
        const Lamp &L = s_def->lamps[i];
        bool        have = false;
        for (uint8_t j = 0; j < s_slotCount; ++j) have |= (s_slots[j].w == L.w && s_slots[j].h == L.h);
        if (have || s_slotCount >= MAX_SLOTS) continue;

        Slot &s = s_slots[s_slotCount++];
        s.w     = L.w;
        s.h     = L.h;
        s.font  = L.font;
        s.spr   = new Gfx::Canvas(&PanelGfx::tft());
        s.spr->setColorDepth(16);
        if (s.spr->createSprite(L.w, L.h) == nullptr) {
            delete s.spr;
            s.spr = nullptr; // out of memory: lamps of this size are skipped, not fatal
            continue;
        }
        PanelGfx::useFont(*s.spr, s.font);
    }
}

static void deleteSlots()
{
    for (uint8_t i = 0; i < s_slotCount; ++i) {
        if (!s_slots[i].spr) continue;
        s_slots[i].spr->unloadFont();
        s_slots[i].spr->deleteSprite();
        delete s_slots[i].spr;
        s_slots[i].spr = nullptr;
    }
    s_slotCount = 0;
}

static void markAllDirty()
{
    const uint8_t n = s_def ? s_def->lampCount : 0;
    s_dirty         = n >= 32 ? 0xFFFFFFFFu : ((1u << n) - 1u);
}

static void drawLamp(uint8_t i)
{
    const Lamp &L    = s_def->lamps[i];
    Slot       *slot = slotFor(L.w, L.h);
    if (!slot) return;

    const uint8_t st  = s_lampTest ? (uint8_t)LS_ON : s_state[i];
    const bool lit   = st != LS_OFF;
    const bool alt   = (st == LS_ALT || st == LS_ALT_BLINK) && L.alt != L.colour;
    const bool blink = (st == LS_BLINK || st == LS_ALT_BLINK);
    // A lamp with no second colour just lights in its own: a profile that sends 3 or 4 to
    // one gets the lit and blinking states rather than nothing at all.
    const LampMode colour = alt ? L.alt : L.colour;
    LampStyle      style;

    if (blink && !s_blinkOn) {
        if (L.kind == KIND_FILLED) {
            style = Theme::style(L.kind, colour, false, s_dim); // a face just goes dark
        } else {
            // The design's blinked-off phase: the legend at 12%, not the lamp extinguished.
            style      = Theme::style(L.kind, colour, true, s_dim);
            style.fg   = Theme::blend(style.fg, style.bg, BLINK_OFF_ALPHA);
            style.glow = false;
        }
    } else {
        style = Theme::style(L.kind, colour, lit, s_dim);
    }

    LegendLine lines[3];
    uint8_t    n = 0;
    lines[n++]   = {L.l1, L.font};
    if (L.l2 && *L.l2) lines[n++] = {L.l2, L.font};
    if (L.sub && *L.sub) lines[n++] = {L.sub, FONT_TINY};

    PanelGfx::renderLampLines(*slot->spr, style, lines, n, slot->font, lampIsPressed(i));
    PanelGfx::push(*slot->spr, L.x, L.y);
}

/* The metal face a group of lights is set into -- a six-pack, or the bezel round a cluster
   on the flight-control panel. It is the assembly that gets pressed, not the lights in it,
   so it is what shows the press. */
static void drawPlate(uint8_t i)
{
    if (!s_def || i >= s_def->plateCount) return;
    const Plate  &p = s_def->plates[i];
    const int16_t r = p.radius ? p.radius : 3;
    PanelGfx::drawPlate(p.x, p.y, p.w, p.h, r, p.fill, p.edge, plateIsPressed(i));
}

/* Does this zone press this lamp? Either the lamp sits inside the zone -- the six-pack's
   captions under its RECALL zone -- or the zone sits on the lamp, which is the ordinary
   case of a button drawn over the thing it lights. */
static bool zonePresses(const Zone &z, const Lamp &L)
{
    const int16_t zcx = (int16_t)(z.x + z.w / 2), zcy = (int16_t)(z.y + z.h / 2);
    const int16_t lcx = (int16_t)(L.x + L.w / 2), lcy = (int16_t)(L.y + L.h / 2);
    const bool lampInZone = lcx >= z.x && lcx < z.x + z.w && lcy >= z.y && lcy < z.y + z.h;
    const bool zoneOnLamp = zcx >= L.x && zcx < L.x + L.w && zcy >= L.y && zcy < L.y + L.h;
    return lampInZone || zoneOnLamp;
}

static bool lampIsPressed(uint8_t lamp)
{
    if (s_pressedZone < 0 || !s_def || s_pressedZone >= (int8_t)s_def->zoneCount) return false;
    return zonePresses(s_def->zones[s_pressedZone], s_def->lamps[lamp]);
}

/* A plate is pressed when the held zone is over it -- a RECALL zone covers the six-pack
   assembly it recalls. */
static bool plateIsPressed(uint8_t plate)
{
    if (s_pressedZone < 0 || !s_def || s_pressedZone >= (int8_t)s_def->plateCount + 127) return false;
    if (plate >= s_def->plateCount || s_pressedZone >= (int8_t)s_def->zoneCount) return false;
    const Zone  &z = s_def->zones[s_pressedZone];
    const Plate &p = s_def->plates[plate];
    const int16_t zcx = (int16_t)(z.x + z.w / 2), zcy = (int16_t)(z.y + z.h / 2);
    return zcx >= p.x && zcx < p.x + p.w && zcy >= p.y && zcy < p.y + p.h;
}

/* Shows a press by drawing what the zone covers as pushed in, rather than by ringing it.
   A whole six-pack goes in together, because on the aircraft it is one lens assembly. A
   zone covering no lamp at all -- the ISDU's knob legends, say -- returns false and gets
   the ring as before. */
static bool touchFeedback(int8_t zone, bool pressed)
{
    if (!s_def || zone < 0 || zone >= (int8_t)s_def->zoneCount) return false;

    uint8_t affected = 0;
    for (uint8_t i = 0; i < s_def->lampCount; ++i)
        if (zonePresses(s_def->zones[zone], s_def->lamps[i])) ++affected;
    if (!affected) return false;

    s_pressedZone = pressed ? zone : -1;

    // The assembly first, because the lights are drawn on top of it.
    for (uint8_t i = 0; i < s_def->plateCount; ++i) {
        const Plate  &p = s_def->plates[i];
        const Zone   &z = s_def->zones[zone];
        const int16_t zcx = (int16_t)(z.x + z.w / 2), zcy = (int16_t)(z.y + z.h / 2);
        if (zcx >= p.x && zcx < p.x + p.w && zcy >= p.y && zcy < p.y + p.h) drawPlate(i);
    }
    for (uint8_t i = 0; i < s_def->lampCount; ++i)
        if (zonePresses(s_def->zones[zone], s_def->lamps[i])) drawLamp(i);
    return true;
}

static const TouchZones::Hooks TOUCH_HOOKS = {nullptr, touchFeedback};

static void drawChrome()
{
    PanelGfx::clearPanel();

    if (s_def->header) {
        const int16_t cy = HEADER_Y + HEADER_H / 2;
        PanelGfx::drawSpacedText(HEADER_X, cy, s_def->header, Theme::HeaderFg, 2);
        const int16_t rx = HEADER_X + PanelGfx::spacedTextWidth(s_def->header, 2) + 8;
        PanelGfx::drawRule(rx, cy, HEADER_R - rx, Theme::Rule);
    }

    Gfx::Device &t = PanelGfx::tft();
    for (uint8_t i = 0; i < s_def->plateCount; ++i) drawPlate(i);

    for (uint8_t i = 0; i < s_def->labelCount; ++i) {
        const Label  &l = s_def->labels[i];
        const int16_t x = l.centred ? (int16_t)(l.x - PanelGfx::spacedTextWidth(l.text, 2) / 2) : l.x;
        PanelGfx::drawSpacedText(x, l.yMid, l.text, l.colour, 2);
    }

    TouchZones::screenCleared();
    markAllDirty();
}

static void applyDemo(uint8_t index)
{
    const uint32_t mask = s_def->demo[index];
    for (uint8_t i = 0; i < s_def->lampCount; ++i) s_state[i] = ((mask >> i) & 1u) ? LS_ON : LS_OFF;
    markAllDirty();
}

/* ---- Interface ------------------------------------------------------------------ */
void clear()
{
    memset(s_state, LS_OFF, sizeof(s_state));
    s_dim      = false;
    s_lampTest = false;
    s_selfTest = false;
    markAllDirty();
}

void init(const Def &def, uint8_t backlightPin)
{
    PanelGfx::begin(backlightPin);

    s_def = &def;
    clear(); // no self test on power-up: dark until MobiFlight runs

    deleteSlots();
    createSlots();
    s_pressedZone = -1;
    TouchZones::attach(def.zones, def.zoneCount, &TOUCH_HOOKS);

    drawChrome();
}

void stop()
{
    TouchZones::detach();
    deleteSlots();
    s_def = nullptr;
    // TFT_eSPI itself is deliberately not de-initialised; re-initialising it crashes.
}

void set(int16_t messageID, char *payload)
{
    if (!s_def) return;

    // Real traffic from MobiFlight -- a lamp or a setting -- ends a running self test.
    if (s_selfTest && messageID != MSG_SELF_TEST) {
        s_selfTest = false;
        memset(s_state, LS_OFF, sizeof(s_state));
        markAllDirty();
    }

    if (messageID >= 0 && messageID < (int16_t)s_def->lampCount) {
        const uint8_t v = payload ? (uint8_t)atoi(payload) : (uint8_t)LS_OFF;
        if (s_state[messageID] != v) {
            s_state[messageID] = v;
            s_dirty |= (1u << messageID);
        }
        return;
    }

    const int v = payload ? atoi(payload) : 0;
    switch (messageID) {
    case MSG_ROTATION:
        PanelGfx::setRotation(payload ? (uint8_t)v : 1);
        drawChrome();
        break;

    case MSG_DIM:
        s_dim = v != 0;
        markAllDirty();
        break;

    case MSG_LAMP_TEST:
        s_lampTest = v != 0;
        markAllDirty();
        break;

    case MSG_SELF_TEST:
        if (v != 0 && s_def->demoCount) {
            s_selfTest  = true;
            s_demoIndex = 0;
            s_demoAt    = millis();
            applyDemo(0);
        }
        break;

    case MSG_TOUCH_CAL:
        if (v != 0 && s_def->zoneCount) TouchZones::startCalibration();
        break;

    default:
        break;
    }
}

void update()
{
    if (!s_def) return;
    const uint32_t now = millis();

    bool redraw;
    if (TouchZones::poll(redraw)) return; // calibrating: leave the screen alone
    if (redraw) drawChrome();

    if (s_selfTest && (now - s_demoAt) >= DEMO_DWELL_MS) {
        s_demoAt    = now;
        s_demoIndex = (uint8_t)((s_demoIndex + 1) % s_def->demoCount);
        applyDemo(s_demoIndex);
    }

    if ((now - s_blinkAt) >= (BLINK_PERIOD_MS / 2)) {
        s_blinkAt = now;
        s_blinkOn = !s_blinkOn;
        for (uint8_t i = 0; i < s_def->lampCount; ++i)
            if (s_state[i] == LS_BLINK) s_dirty |= (1u << i);
    }

    // Keeps drawing while dark, so the panel reappears instantly showing current state.
    if (s_dirty) {
        for (uint8_t i = 0; i < s_def->lampCount; ++i)
            if (s_dirty & (1u << i)) drawLamp(i);
        s_dirty = 0;
    }
}

} // namespace LampPanel
