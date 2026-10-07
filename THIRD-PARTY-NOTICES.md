# Third-party notices

This repository's own code and definitions are under the [MIT licence](LICENSE).

The **firmware images** published with each release are built from this repository together
with the components below, compiled into a single binary. This file lists every one of them
with its licence, and reproduces the notices those licences ask to travel with a binary.

## What the firmware images contain

Code under MIT and BSD licences; the Arduino core for the ESP32, which is LGPL 2.1, and the
ESP-IDF it is built on, mostly Apache 2.0; the FreeRTOS kernel (MIT); newlib's C library,
under BSD-style licences; and two pieces of artwork, the DM Sans font (OFL) and the splash
logo (CC BY-SA 4.0). Each component is listed below with the notices its licence asks to
travel with a binary. The licence texts that have to travel in full are in
[`licenses/`](licenses/), and every release carries them: `LGPL-2.1.txt`, `Apache-2.0.txt`,
`COPYING.NEWLIB.txt` and `OFL-1.1.txt`. As the LGPL requires, the
firmware can be rebuilt against a modified Arduino core: the complete source is this
repository at the release's tag, with the MobiFlight core pinned to a commit in
`Annunciator/Annunciator_platformio.ini` and the libraries to the versions declared there.
The build steps are in the [README](README.md#building-from-source).

**Deliberately left out.** The MobiFlight core firmware can drive stepper motors, servos and
character LCDs on a board's spare pins, using AccelStepper (GPL v3), ESP32Servo (LGPL 2.1) and
LiquidCrystal_I2C (no stated licence). These panels need none of it, and AccelStepper alone
would have made every firmware image a GPL v3 work, so the annunciator builds compile all
three out. The board definitions tell the Connector the same — no steppers, servos or LCDs —
so it never offers them. Putting them back is described in
`Annunciator/Annunciator_platformio.ini`, under `custom_without`.

## Components

| Component | Version | In | Licence |
|---|---|---|---|
| [MobiFlight core firmware](https://github.com/elral/MobiFlight-FirmwareSource) (elral's `ESP32_support` branch) | `9299abc` | both | MIT |
| [Arduino-CmdMessenger](https://github.com/MobiFlight/Arduino-CmdMessenger) (MobiFlight's fork) | 4.2.2 | both | MIT |
| [ArduinoUniqueID](https://github.com/ricaun/ArduinoUniqueID) | 1.3.0 | both | MIT |
| [Arduino core for the ESP32](https://github.com/espressif/arduino-esp32) | 3.3.7 | both | LGPL-2.1-or-later, with Apache-2.0 parts |
| [ESP-IDF](https://github.com/espressif/esp-idf) (precompiled in the Arduino core, and the bootloader) | 5.5 | both | Apache-2.0, and others per component |
| [FreeRTOS kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) (ESP-IDF's SMP port) | 10.5.1 | both | MIT |
| [newlib](https://sourceware.org/newlib/) C library (libc, libm), from Espressif's Xtensa toolchain | esp-14.2.0 | both | BSD-style, per file |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | 2.5.43 | 3.2″ only | FreeBSD, BSD and MIT parts — see below |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | 1.2.29 | 3.5″ only | MIT and BSD-2-Clause |
| [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX) | 1.6.8 | 3.5″ only | not stated |
| [DM Sans](https://fonts.google.com/specimen/DM+Sans) (embedded as glyph data) | 4.004 | both | SIL Open Font License 1.1 |
| [Boeing 737-800 silhouette](https://commons.wikimedia.org/wiki/File:Boeing_737-800_silhouette.svg) (the boot splash's logo, embedded as a bitmap) | 2016 | both | CC BY-SA 4.0 |

**Not stated** means the package as distributed carries no licence file and declares none.
It is listed here for attribution; its terms are its author's to state.

Tools used only to build — PlatformIO, esptool, FreeType and Pillow — are not part of any
released file. The Xtensa toolchain is, in part: its C library (newlib's libc and libm) is
linked into the firmware, as are its libgcc and libstdc++, which are under the GCC Runtime
Library Exception and ask for no notice.

---

## MobiFlight core firmware — MIT

```text
MIT License

Copyright (c) 2021 MobiFlight

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Arduino-CmdMessenger — MIT

```text
#  MIT License
--------------
Copyright (c) 2013, 2014, 2015, 2016, 2017 
	Thijs Elenbaas, Valeriy Kucherenko,
	Dreamcat4, Neil Dudman, Thomas Ouellet Fredericks.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## ArduinoUniqueID — MIT

```text
MIT License

Copyright (c) 2019 Luiz H. Cassettari

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Arduino core for the ESP32 — LGPL 2.1 or later

Copyright (c) Espressif Systems and the Arduino-ESP32 contributors. Licensed under the GNU
Lesser General Public License, version 2.1 or later; the full text is in
[`licenses/LGPL-2.1.txt`](licenses/LGPL-2.1.txt). As the LGPL requires, the firmware can be
rebuilt against a modified copy of this library: the complete source and build are in this
repository.

Parts of the core's own hardware layer and libraries are under the Apache License 2.0
instead (`esp32-hal-uart.c` and `Preferences`, for example), and it is built on Espressif's
ESP-IDF 5.5, which is precompiled into it — as is the second-stage bootloader in every full
flash image. ESP-IDF and the components it is built from are each under their own licence,
predominantly Apache-2.0, whose full text is in
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt). Espressif lists every component and its
licence on
[ESP-IDF's copyright page](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/COPYRIGHT.html).
FreeRTOS and newlib, which these images link in, have sections of their own below.

## ESP-IDF — Apache-2.0

Copyright (c) Espressif Systems (Shanghai) Co., Ltd., and the ESP-IDF contributors. Licensed
under the Apache License, Version 2.0; the full text is in
[`licenses/Apache-2.0.txt`](licenses/Apache-2.0.txt). The firmware images contain ESP-IDF
libraries in object form, precompiled in the Arduino core, and its second-stage bootloader.

## FreeRTOS kernel — MIT

FreeRTOS Kernel V10.5.1, as modified for symmetric multiprocessing by Espressif in ESP-IDF.

```
Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## newlib — BSD-style licences

The C library linked into the firmware (libc and libm) is newlib, as built into Espressif's
Xtensa toolchain esp-14.2.0. Its files carry many authors' notices, most of them BSD-style;
the complete set is reproduced in [`licenses/COPYING.NEWLIB.txt`](licenses/COPYING.NEWLIB.txt),
copied unchanged from the toolchain's `share/licenses/newlib/COPYING.NEWLIB`.

## TFT_eSPI — FreeBSD, BSD and MIT parts

```text
The original starting point for this library was the Adafruit_ILI9341
library in January 2015.

The licence for that library is MIT.

The first evolution of the library that led to TFT_eSPI is recorded here:

https://www.instructables.com/id/Arduino-TFT-display-and-font-library/

Adafruit_ILI9341 ORIGINAL LIBRARY HEADER:

vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
  This is our library for the Adafruit  ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
  
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


Selected functions from the Adafruit_GFX library (as it was in 2015) have
been imported into the TFT_eSPI.cpp file and modified to improve
performance, add features and make them compatible with the ESP8266 and
ESP32.

The fonts from the Adafruit_GFX and Button functions were added later.
The fonts can be found with the license.txt file in the "Fonts\GFXFF"
folder.

The Adafruit_GFX functions are covered by the BSD licence.

Adafruit_GFX ORIGINAL LIBRARY LICENSE:

vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Due to the evolution of the TFT_eSPI library the original code may no longer
be recognisable, however in most cases the function names can be used as a
reference point since the aim is to retain a level of compatibility with
the popular Adafruit_GFX graphics functions.

Contributions from other authors are recorded on GitHub:
https://github.com/Bodmer/TFT_eSPI

The major addition to the original library was the addition of fast
rendering proportional fonts of different sizes as documented here:

https://www.instructables.com/id/Arduino-TFT-display-and-font-library/

The larger fonts are "Run Length Encoded (RLE)", this was done to
reduce the font memory footprint for AVR processors that have limited
FLASH, with the added benefit of a significant improvement in rendering
speed.

In 2016 the library evolved significantly to support the ESP8266 and then
the ESP32. In 2017 new Touch Screen functions were added and a new Sprite
class called TFT_eSprite to permit "flicker free" screen updates of complex
graphics.

In 2018 anti-aliased fonts were added along with a Processing font conversion
sketch.

In 2019 the library was adapted to be able to use it with any 32-bit Arduino
compatible processor. It will run on 8-bit and 16-bit processors but will be
slow due to extensive use of 32-bit variables.

Many of the example sketches are original work that contain code created
for my own projects. For all the original code the FreeBSD licence applies
and is compatible with the GNU GPL.

vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
Software License Agreement (FreeBSD License)

Copyright (c) 2026 Bodmer (https://github.com/Bodmer)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
```

## LovyanGFX — MIT and BSD-2-Clause

```text
Adafruit_ILI9341 ORIGINAL LIBRARY HEADER:
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
  This is our library for the Adafruit  ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
  
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Adafruit_GFX ORIGINAL LIBRARY LICENSE:
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

Software License Agreement (BSD License)

Copyright (c) 2012 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TFT_eSPI ORIGINAL LIBRARY LICENSE:
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
Software License Agreement (FreeBSD License)

Copyright (c) 2020 Bodmer (https://github.com/Bodmer)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

LovyanGFX ORIGINAL LIBRARY LICENSE:
vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvStartvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
Software License Agreement (FreeBSD License)

Copyright (c) 2020 lovyan03 (https://github.com/lovyan03)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^End^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
```

## GFX Library for Arduino — not stated

By Moon On Our Nation (<https://github.com/moononournation/Arduino_GFX>). Version 1.6.8 as
distributed states no licence. It derives in part from Adafruit's GFX library, which is
BSD-licensed.

## DM Sans — SIL Open Font License 1.1

Copyright 2014 The DM Sans Project Authors. The full licence is in
[`licenses/OFL-1.1.txt`](licenses/OFL-1.1.txt), a copy of [`fonts/OFL.txt`](fonts/OFL.txt),
and every release carries it. The firmware embeds glyph bitmaps generated from the font by
`tools/make_vlw.py` — a modified version of the font software, in the licence's terms — and
not the font file itself.

## Boeing 737-800 silhouette — CC BY-SA 4.0

"Boeing 737-800 silhouette" by Peter James Lowden, from Wikimedia Commons
(<https://commons.wikimedia.org/wiki/File:Boeing_737-800_silhouette.svg>), licensed under the
Creative Commons Attribution-ShareAlike 4.0 International licence
(<https://creativecommons.org/licenses/by-sa/4.0/>).

The firmware shows it on the boot splash, which credits its author and licence on screen.

**Changes made:** cropped to the aircraft, scaled to 110×120 (3.2″) and 146×160 (3.5″)
pixels, flattened onto the panel's black background and reduced to RGB565, by
`tools/make_logo.py`. That adaptation is in `Annunciator/Splash/Logo320.h` and `Logo480.h`,
and it is licensed CC BY-SA 4.0 as well.

The source image is in `art/`, with its provenance in [`art/README.md`](art/README.md). The
share-alike condition applies to the image and adaptations of it, not to the rest of the
firmware.
