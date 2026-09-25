#pragma once

#include <Arduino.h>

/* **********************************************************************************
    The lamp colour language, taken verbatim from the two Claude Design mockups
    (Door Annunciator Panel.dc.html and IRS Annunciator Panel.dc.html). Both files
    define an identical `lamp(mode)` helper, so it lives here once.

    Colours are kept in RGB888 so they can be blended; PanelGfx converts to RGB565
    at draw time.
********************************************************************************** */

enum LampMode : uint8_t {
    LAMP_BLANK = 0, // an empty grid cell -- painted as panel background
    LAMP_OFF,       // dark, but the legend stays faintly readable
    LAMP_AMBER,     // caution
    LAMP_WHITE,     // ALIGN
    LAMP_BLUE,      // advisory (GRD PWR, RAM DOOR FULL OPEN)
    LAMP_GREEN,     // pressurisation ALTN / MANUAL
    LAMP_RED,       // FIRE WARN
};

/* How a lamp is built, which decides how it looks lit and unlit:
     LENS    the overhead annunciator: a dark lens whose legend lights up and glows
     FILLED  the glareshield push-lights (MASTER CAUTION, FIRE WARN): the whole face lights
             and the legend reads dark against it
     LEGEND  one caption of the six-pack: a glowing legend on the unit's black face, no lens
             outline of its own */
enum LampKind : uint8_t {
    KIND_LENS = 0,
    KIND_FILLED,
    KIND_LEGEND,
};

struct LampStyle {
    uint32_t bg;
    uint32_t fg;
    uint32_t edge;
    bool     lit;
    bool     glow; // lit and not dimmed -- the design kills the halo in night mode
    LampKind kind; // KIND_LENS unless set: the mockup styles are all lenses
};

namespace Theme
{
    // True black. The mockup used #08090b, which quantises to RGB565 0x0841 -- one step
    // above black per channel. Indistinguishable on the IPS panel, so black it is.
    constexpr uint32_t PanelBg  = 0x000000; // body background
    constexpr uint32_t Rule     = 0x1C1F24; // the 1px divider lines
    constexpr uint32_t HeaderFg = 0x5D6672; // "DOORS" / "IRS"
    constexpr uint32_t IruLabel = 0x7D8896; // "L IRU" / "R IRU"

    // Electrical meter panel. The LED green is the hue of the real NG display's LEDs --
    // #679F11 averaged from the brightest pixels of a photo of one -- raised to full
    // brightness, since a camera exposes lit LEDs darker than the eye sees them.
    constexpr uint32_t LedOn      = 0x9CF01A;
    constexpr uint32_t LedOnDim   = 0x5E900F; // night dimming
    constexpr uint32_t LedWindow  = 0x050807; // the display window behind the dots
    constexpr uint32_t LedFrame   = 0x2A2E33;
    constexpr uint8_t  LedOffAlpha = 14;      // unlit dots, barely visible, as on a real matrix
    constexpr uint32_t Placard    = 0xB8BEC6; // DC AMPS, CPS FREQ, ... printed around the window

    // Glareshield. The six-pack's captions share one black face; the push-lights sit in a
    // dark bezel.
    constexpr uint32_t SixPackFace = 0x0A0B0B;
    constexpr uint32_t Bezel       = 0x2A2C30;

    // Drawn round a touch target while it is held, so a press visibly registered even
    // though the lamp itself only changes when the sim says so.
    constexpr uint32_t PressRing = 0x8A949F;

    LampStyle lamp(LampMode mode, bool dim);

    // Any kind of lamp, lit in `colour` or unlit. `lamp()` above is the LENS case.
    LampStyle style(LampKind kind, LampMode colour, bool lit, bool dim);

    // alpha is the weight of `a` over `b`, 0..255.
    uint32_t blend(uint32_t a, uint32_t b, uint8_t alpha);
    uint16_t to565(uint32_t rgb);
}
