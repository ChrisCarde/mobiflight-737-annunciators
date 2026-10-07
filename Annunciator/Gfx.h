#pragma once

#include "Board.h"

/* **********************************************************************************
    The one place a graphics library is named.

    Gfx::Device is the screen, Gfx::Canvas an off-screen buffer that gets pushed to it.
    Everything above this line -- PanelGfx and the ten panels -- draws through those two
    types and never includes a library header, so a board with a different controller is a
    new branch here rather than a second copy of the renderer.

    The contract a backend has to meet is the subset of the TFT_eSPI / LovyanGFX drawing
    API that PanelGfx actually uses: fillScreen, fillRect, drawPixel, drawFastHLine/VLine,
    drawCircle, draw/fillRoundRect, fillSmoothCircle, drawWideLine, setTextDatum,
    setTextColor, drawString, loadFont/unloadFont, and on a canvas createSprite,
    deleteSprite, fillSprite, pushSprite, width, height. Both libraries provide all of it
    natively with matching signatures; the host preview implements it.
********************************************************************************** */

#if ANNUN_BACKEND_TFT_ESPI

#include <TFT_eSPI.h>
namespace Gfx
{
    using Device = TFT_eSPI;
    using Canvas = TFT_eSprite;

    // How the boot splash credits the display library: in capitals, as the panel fonts
    // carry no lower case.
    constexpr char LIBRARY_CREDIT[] = "DISPLAY: TFT_ESPI";
} // namespace Gfx

#elif ANNUN_BACKEND_LGFX

#include <LovyanGFX.hpp>
namespace Gfx
{
    /* Both are LovyanGFX sprites. The screen is a full-frame buffer in PSRAM rather than a
       panel object, because this display is written a whole frame at a time -- see
       present() below and PanelGfxLGFX.cpp. */
    using Device = LGFX_Sprite;
    using Canvas = LGFX_Sprite;

    constexpr char LIBRARY_CREDIT[] = "DISPLAY: LOVYANGFX + ARDUINO_GFX";
} // namespace Gfx

#elif ANNUN_BACKEND_HOST

#include "HostGfx.h"
namespace Gfx
{
    using Device = HostCanvas;
    using Canvas = HostCanvas;

    // The preview draws each board's screen, so it credits that board's libraries.
#if ANNUN_PANEL_RES == 480
    constexpr char LIBRARY_CREDIT[] = "DISPLAY: LOVYANGFX + ARDUINO_GFX";
#else
    constexpr char LIBRARY_CREDIT[] = "DISPLAY: TFT_ESPI";
#endif
} // namespace Gfx

#endif

/* The two anti-aliased shapes, which the libraries spell differently.

   TFT_eSPI has to be told the colour behind the shape, because it cannot read back from
   the panel it is drawing on. LovyanGFX and the host preview read the pixel underneath
   and blend against what is actually there, which is both better and what the glow in
   PanelGfx already relies on. Callers pass the background either way; the backends that
   do not need it ignore it. The target may be the screen or a canvas, hence the template:
   on one board those are different types. */
namespace Gfx
{
    template <class Target>
    inline void fillSmoothCircle(Target &target, int32_t x, int32_t y, int32_t r,
                                 uint16_t fg, uint16_t bg)
    {
#if ANNUN_BACKEND_LGFX
        (void)bg;
        target.fillSmoothCircle(x, y, r, fg);
#else
        target.fillSmoothCircle(x, y, r, fg, bg);
#endif
    }

    template <class Target>
    inline void drawWideLine(Target &target, float x0, float y0, float x1, float y1,
                             float width, uint16_t fg, uint16_t bg)
    {
#if ANNUN_BACKEND_LGFX
        (void)bg;
        target.drawWideLine((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, width, fg);
#else
        target.drawWideLine(x0, y0, x1, y1, width, fg, bg);
#endif
    }
} // namespace Gfx

/* What a backend provides beyond the drawing calls. Three functions, because the boards
   differ in exactly three ways: how the screen is brought up, how it is rotated, and
   whether drawing reaches the glass by itself.

   The E32R32P draws straight to its controller, so present() does nothing. The C3248W535
   cannot: its AXS15231B is written over QSPI a whole frame at a time -- some batches
   ignore the window-address commands altogether -- so its Device is a sprite in PSRAM and
   present() blits the frame. Everything above that is the same code either way.

   And one for diagnostics. The boot splash brings the screen up before setup() has opened
   the serial port, so anything a backend has to say about bringing it up -- the frame
   buffer it got, say -- would be lost if it were sent then. deviceReport() hands it over
   later instead, for MFCustomDevice to send once the port is open; nullptr if there is
   nothing to say. */
namespace Gfx
{
    void        deviceBegin(Device &device);
    void        deviceRotate(Device &device, uint8_t rotation);
    void        present(Device &device);
    const char *deviceReport();
} // namespace Gfx
