#include "Gfx.h"

#if ANNUN_BACKEND_TFT_ESPI

/* The E32R32P's backend: an ST7789 on the display SPI bus, driven by TFT_eSPI.

   Every pin and driver option for it is a -D flag in Annunciator_platformio.ini, so there
   is nothing to configure here. Drawing reaches the panel as it happens, which is why
   present() has nothing to do. */

namespace Gfx
{

void deviceBegin(Device &device)
{
    device.init();
}

void deviceRotate(Device &device, uint8_t rotation)
{
    device.setRotation(rotation);
}

void present(Device &)
{
    // Nothing: TFT_eSPI writes straight to the controller.
}

} // namespace Gfx

#endif
