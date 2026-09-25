#include "Theme.h"

namespace Theme
{

uint16_t to565(uint32_t rgb)
{
    const uint8_t r = (uint8_t)((rgb >> 16) & 0xFF);
    const uint8_t g = (uint8_t)((rgb >> 8) & 0xFF);
    const uint8_t b = (uint8_t)(rgb & 0xFF);
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint32_t blend(uint32_t a, uint32_t b, uint8_t alpha)
{
    const uint16_t ia = (uint16_t)(255 - alpha);
    const uint32_t r  = ((((a >> 16) & 0xFF) * alpha) + (((b >> 16) & 0xFF) * ia)) / 255;
    const uint32_t g  = ((((a >> 8) & 0xFF) * alpha) + (((b >> 8) & 0xFF) * ia)) / 255;
    const uint32_t bl = (((a & 0xFF) * alpha) + ((b & 0xFF) * ia)) / 255;
    return (r << 16) | (g << 8) | bl;
}

LampStyle lamp(LampMode mode, bool dim)
{
    switch (mode) {
    case LAMP_AMBER:
        return dim ? LampStyle{0x1E1607, 0xC08A2C, 0x2C2213, true, false}
                   : LampStyle{0x2C1D05, 0xFFC24D, 0x4A3410, true, true};
    case LAMP_WHITE:
        return dim ? LampStyle{0x1B1D1C, 0xB9C0B4, 0x1E2024, true, false}
                   : LampStyle{0x22261F, 0xF4F7F1, 0x1E2024, true, true};
    case LAMP_BLUE:
        return dim ? LampStyle{0x071520, 0x5FA8CC, 0x122536, true, false}
                   : LampStyle{0x0A2133, 0x8FD4F5, 0x1D4A6B, true, true};
    // Not in the mockups; built the same way -- a dark tint of the hue behind a bright
    // legend, a mid-tone edge, and a dimmed set with the halo off.
    case LAMP_GREEN:
        return dim ? LampStyle{0x071A0B, 0x55B864, 0x12301A, true, false}
                   : LampStyle{0x0B2410, 0x86F296, 0x1C4A24, true, true};
    case LAMP_RED:
        return dim ? LampStyle{0x1E0806, 0xC4463A, 0x301410, true, false}
                   : LampStyle{0x2E0A08, 0xFF6A5A, 0x5A1812, true, true};
    case LAMP_OFF:
        // The design deliberately keeps unlit legends faintly visible:
        // rgba(214,172,102,.22) composited over the unlit body colour #131412.
        return LampStyle{0x131412, blend(0xD6AC66, 0x131412, 56), 0x1E2024, false, false};
    case LAMP_BLANK:
    default:
        return LampStyle{PanelBg, PanelBg, PanelBg, false, false};
    }
}

// The glareshield push-lights light their whole face. In the photo of a lit MASTER CAUTION
// the face glows yellow-amber and the legend reads dark red-brown against it; an unlit one
// (FIRE WARN in the same photo) is a dark face with grey legends.
static LampStyle filled(LampMode colour, bool lit, bool dim)
{
    if (!lit) return LampStyle{0x151514, 0x57544F, Bezel, false, false, KIND_FILLED};
    if (colour == LAMP_RED)
        return dim ? LampStyle{0xA01E18, 0x2A0200, Bezel, true, false, KIND_FILLED}
                   : LampStyle{0xF2352B, 0x3E0300, Bezel, true, true, KIND_FILLED};
    return dim ? LampStyle{0xB5781C, 0x3E1500, Bezel, true, false, KIND_FILLED}
               : LampStyle{0xFFB52E, 0x6A1F00, Bezel, true, true, KIND_FILLED};
}

// A six-pack caption: the amber of a lit legend glowing on the unit's black face, and when
// unlit just a trace of the lettering.
static LampStyle legend(bool lit, bool dim)
{
    if (!lit) return LampStyle{SixPackFace, blend(0xD6AC66, SixPackFace, 34), SixPackFace, false, false, KIND_LEGEND};
    return dim ? LampStyle{SixPackFace, 0xC0852C, SixPackFace, true, false, KIND_LEGEND}
               : LampStyle{SixPackFace, 0xFFB43E, SixPackFace, true, true, KIND_LEGEND};
}

LampStyle style(LampKind kind, LampMode colour, bool lit, bool dim)
{
    switch (kind) {
    case KIND_FILLED: return filled(colour, lit, dim);
    case KIND_LEGEND: return legend(lit, dim);
    case KIND_LENS:
    default:          return lamp(lit ? colour : LAMP_OFF, dim);
    }
}

} // namespace Theme
