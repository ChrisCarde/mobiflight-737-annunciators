# Annunciator Panel — a MobiFlight custom device for two ESP32 display boards

*The full handbook: building, installing, wiring it to the sim, and what to do when a
panel stays dark. The [README](../README.md) is the short version.*

Turns a cheap all-in-one ESP32 display board into one of the 737-800 panels the Rowsfire
B107 overhead and the WinCtrl MCP/EFIS leave out, driven from MobiFlight Connector.

Two boards are supported, and the same nine panels run on both: the **LCDWIKI E32R32P**
(3.2", 240×320 used as 320×240 landscape, resistive touch) and the **Caturda C3248W535**
(3.5", 320×480 used as 480×320, capacitive touch). Everything below applies to both unless
it says otherwise.

| Panel | Custom device type | Touch |
|-------|--------------------|-------|
| Door annunciators (+ GRD PWR) | `ANNUN_DOOR` | each door lamp opens / closes that door |
| IRS mode select unit lamps | `ANNUN_IRS` | — |
| AC & DC electrical metering | `ANNUN_ELEC` | — |
| Air conditioning / bleed / pressurisation lights | `ANNUN_AIR` | — |
| Flight controls lights | `ANNUN_FCTL` | — |
| IRS display unit (ISDU) | `ANNUN_ISDU` | the keypad and both knobs work |
| Master caution, captain's glareshield | `ANNUN_MCS_L` | FIRE WARN, MASTER CAUTION, six-pack recall |
| Master caution, first officer's glareshield | `ANNUN_MCS_R` | the same |
| Master caution, both six-packs on one screen | `ANNUN_MCS_BOTH` | the same, a recall per six-pack |
| Master caution with the AFDS P/RST lights | `ANNUN_MCS_BOTH_AFDS` | the same, plus A/P, A/T and FMC |

All of them ship in one firmware image — every board gets the same `.bin`. Which panel a
board shows is chosen **in the Connector**, by which custom device you add to it, so any
board can be any panel without reflashing to change panel. (Do flash each board with the
current build first — see *Installing*.)

<!-- markdownlint-disable MD033 -->

## How this repo is put together

This is a [MobiFlight CommunityTemplate](https://github.com/MobiFlight/CommunityTemplate)
project. The MobiFlight core firmware is **not** vendored here — `get_CoreFiles.py` clones
it into `./src` on the first build and it is compiled alongside the sources in
`Annunciator/`. The core owns the serial protocol, the `kGetInfo` handshake, EEPROM config
storage and power saving; this repo only implements the four functions in
`Annunciator/MFCustomDevice.cpp` (`attach` / `detach` / `update` / `set`) plus the panel
renderers.

> **The core comes from a fork, on purpose.** Upstream `MobiFlight/MobiFlight-FirmwareSource`
> has no ESP32 target at any released version — its environments are AVR and RP2040 only.
> ESP32 support lives on `elral/MobiFlight-FirmwareSource` branch `ESP32_support`, so that
> is what `get_CoreFiles.py` pins. It is an unmerged branch on a personal fork; that is the
> only ESP32 path that currently exists.
>
> Because a branch name is not a version, the ref to clone and the version the firmware
> *reports* are separate options: `custom_core_firmware_version = ESP32_support` and
> `custom_core_firmware_base_version = 3.1.4`. This matters more than it looks. The Connector
> parses the reported core version with .NET's `System.Version` to decide which features the
> board has — including whether a custom device may be added at all, and how outputs are
> addressed on the wire. Reporting `ESP32_support` makes it throw. The branch is 113 commits
> ahead of upstream 3.1.4 and 0 behind, so 3.1.4 is the truthful answer; `get_version.py`
> refuses to build with a non-numeric one.

> **The core is patched after every fetch.** The Connector sends a custom device's Stop
> message (`-1`) with no value at all — on every board detection, every Stop and every
> AutoRun restart — and an empty string output as an empty value. The core hands both to
> CmdMessenger's `unescape()` as a NULL pointer. An AVR shrugs that off; the ESP32 panics
> (`Guru Meditation Error … LoadProhibited`) and reboots, and whatever values the Connector
> sent while it was down are lost, because the Connector counts them as delivered.
> `get_CoreFiles.py` adds the missing NULL check after each clone or pull, and stops the
> build if the core ever changes so the patch no longer applies. Measured on the board:
> before, `32,0,-1;` rebooted it; after, it does not.

```
Annunciator/
  Annunciator_platformio.ini   one build environment per board, and the core firmware pin
  Board.h                      the only file naming a pin or a screen size: both boards
  Gfx.h                        the only file naming a graphics library
  Layout.h                     margins, lens size, gutters and touch minimums, per screen
  MFCustomDevice.{h,cpp}       MobiFlight integration point: custom device type -> panel
  Panel.h                      message IDs and lamp states shared by every panel
  Theme.{h,cpp}                the lamp colour language from the design
  PanelGfx.{h,cpp}             lamp and text rendering, and when the panel is lit
  PanelGfxTftESPI.cpp          the E32R32P's screen: ST7789 through TFT_eSPI
  PanelGfxLGFX.cpp             the C3248W535's: LovyanGFX drawing, Arduino_GFX blitting
  LampPanel.{h,cpp}            the engine for panels that are only lamps, run from a table
  Touch.h                      six functions; the two panels below implement them
  TouchXPT2046.cpp             resistive: polling, and the four-point calibration
  TouchAXS15231B.cpp           capacitive: an I2C read, and no calibration to do
  TouchZones.{h,cpp}           touch buttons -> MobiFlight button events, the press ring
  DoorPanel/                   door lamps (a LampPanel table)
  AirPanel/                    air conditioning / bleed / pressurisation (a LampPanel table)
  FctlPanel/                   flight controls (a LampPanel table)
  McsPanel/                    the three master caution layouts (LampPanel tables)
  IrsPanel/                    IRS mode select unit lamps
  ElecPanel/                   electrical metering
  IsduPanel/                   IRS display unit: segmented displays, knobs, keypad
  fonts/                       DM Sans Bold and its metrics, generated, one set per screen
  Community/                   what you drop into MobiFlight Connector (both boards)
```

## Hardware

**LCDWIKI / QDtech "3.2inch ESP32-32E Display", SKU E32R32P** — an all-in-one board, so
there is nothing to wire. Vendor page:
<https://www.lcdwiki.com/3.2inch_ESP32-32E_Display>

| | |
|---|---|
| MCU | ESP32-WROOM-32E (ESP32-D0WD-V3), classic Xtensa dual-core, 240 MHz |
| Flash / PSRAM | 4 MB / **none** |
| Display | 3.2" 240×320 IPS, ST7789P3 |
| Touch | XPT2046 resistive, shares the display SPI bus |
| USB | CH340C, enumerates as `1A86:7523` |

`E32N32P` is the same board without the touchscreen; this firmware runs on either.

**Caturda C3248W535**, sold also as Guition / JCZN **JC3248W535EN** — again all-in-one.
Vendor: <http://www.jczn1688.com/>

| | |
|---|---|
| MCU | ESP32-S3-WROOM-1 N16R8, Xtensa dual-core, 240 MHz |
| Flash / PSRAM | 16 MB / **8 MB octal** — the PSRAM is required, see below |
| Display | 3.5" 320×480 IPS, AXS15231B over **QSPI** |
| Touch | capacitive, the same AXS15231B on I²C at 0x3B, factory-calibrated |
| USB | native ESP32-S3 USB-Serial-JTAG, `303A:1001`, no bridge chip |

Three things about this board are worth knowing before it misbehaves:

- **TFT_eSPI cannot drive it.** The AXS15231B is a quad-SPI part with no DC pin, and
  TFT_eSPI has neither a QSPI transport nor a driver for it. This board draws with
  LovyanGFX into a frame buffer and blits that with Arduino_GFX — see *How the display is
  driven* below.
- **PSRAM is not optional.** The frame buffer is 480×320×16bpp, 300 KB, which cannot come
  from internal RAM. If `board_build.arduino.memory_type` is ever wrong, the screen simply
  stays black rather than failing to build.
- **Ignore the vendor manual's 120 MHz flash setting.** It is an experimental IDF mode
  whose calibration drifts with temperature; every working community config uses 80 MHz,
  and so does the env here.

Do not confuse it with the 4.3" **JC4827W543**, which looks similar and shares the QSPI pin
map but is an NV3041A with GT911 touch and 4 MB of flash. A pin map alone does not tell you
which board a document is about.

### How the display is driven

Two libraries, each doing what it is good at. LovyanGFX draws every pixel, into a
full-screen sprite in PSRAM — it is the only one with the smooth-font, sprite and
text-datum calls the renderer is written against. Arduino_GFX does nothing but carry the
finished frame to the panel, because its AXS15231B driver over `Arduino_ESP32QSPI` is the
QSPI path known to work on this display.

Whole frames, always. Reports differ on whether a given batch honours the window-address
commands at all, and Espressif's own driver has an open bug that skips one of them in QSPI
mode. Blitting the whole frame sidesteps the question: about 15 ms of bus time at 40 MHz,
and a panel of annunciators only redraws when a lamp changes. `PanelGfx::present()` sends
it, once per poll, and only when something was drawn.

Both pins in that path are fixed traces and live in `Annunciator/Board.h`: QSPI CS 45,
SCK 47, D0–D3 21/48/40/39, backlight GPIO 1, touch I²C SDA 4 / SCL 8 / INT 3.

### What this panel got wrong, and how it presents

Three things about the C3248W535 cost a day between them, and all three present as "the
display is broken" rather than as what they are. They are fixed in the firmware; this is
here so the symptoms are recognisable if they ever come back, and because two of them
contradict the advice you will find elsewhere.

**1. Arduino_GFX has to be pinned, and the init sequence named.** The common advice is to
pin 1.6.0, because 1.6.1 regressed the AXS15231B init sequence. That advice does not hold
here: **1.6.0 and earlier do not compile against Arduino-ESP32 core 3.x at all** -- their
SPI bus calls the core's old `spiFrequencyToClockDiv`. This project is on **1.6.8** and
passes `axs15231b_320480_type1_init_operations` explicitly, because from 1.6.4 the
library's default for this driver is a *different* panel's sequence (a 180x640 LilyGO) and
taking the default gives a blank screen. The library also ships a `type2` for this size;
batches differ, so if a panel ever comes up garbled that is the first thing to swap.

**2. The panel rejects a rotated address window.** *Symptom: the screen is black except for
a few coloured dots down the left edge.* Easy to read as a dead panel, a bad init sequence
or a memory fault -- it is none of those. A plain `fillScreen` at rotation 0 fills the
screen; the same call after `setRotation(1)` puts a handful of pixels down the left edge
and nothing else. Every write after the rotation lands as a stripe, because the controller
will not accept the rotated window.

So **the panel is never rotated.** It keeps its native 320x480 portrait shape, and the
*sprite* is rotated instead: LovyanGFX keeps a sprite's buffer exactly as created and
applies rotation to the drawing coordinates, so a 320x480 buffer at sprite rotation 1 hands
the renderer a 480x320 landscape surface while staying in the layout the panel wants. The
nine panel layouts know nothing about any of this. If a future board does accept a rotated
window, this is still the better arrangement -- it is one blit of a whole frame either way.

**3. LovyanGFX sprites are already big-endian.** *Symptom: the layout is perfect and every
colour is wrong.* Here amber lamps came out red, blue came out green, and the near-black
unlit lamps came out bright magenta. A dark colour coming out bright is the tell that it is
a byte swap rather than a channel order: `draw16bitRGBBitmap` swaps bytes on the way out,
and a LovyanGFX 16bpp sprite is already stored in the order SPI panels want. Use
`draw16bitBeRGBBitmap`, which does not swap. It is also faster for the same reason -- a
full frame went from 48 ms to 33 ms.

### Bringing up a new board

`tools/bringup/` is a probe that runs before the real firmware and checks one thing at a
time, so a black screen has one cause rather than five:

```sh
pio run -e bringup_c3248w535 -t upload
pio device monitor -e bringup_c3248w535
```

It reports the chip and flash, the PSRAM (8 MB, or the frame buffer cannot exist), blinks
the backlight, scans the touch bus for `0x3B`, brings the display up and fills it red,
green and blue, then prints raw touch bytes and the state of the interrupt line. It shares
`Annunciator/Board.h`, so it probes the same pins the firmware will use.

One warning from using it: **the core only starts sending input events once the Connector
has asked for the board's config** (`boardReady` is set at the end of `OnGetConfig`). A
bench script that sends values but never `kGetConfig` will see touch do nothing, and that
is not a touch fault. `mfsim.py setup` asks for the config, so touch works after it.

### Fixed pinout (E32R32P)

Every one of these is a PCB trace, not a choice. They live as `TFT_eSPI` build flags in
`Annunciator_platformio.ini`. The C3248W535's pins are in *How the display is driven*
above, and in `Annunciator/Board.h`; its library takes them as arguments rather than as
build flags.

| Function | GPIO | |
|----------|------|---|
| LCD MOSI | 13 | HSPI IOMUX pin — hence `-DUSE_HSPI_PORT` |
| LCD SCLK | 14 | |
| LCD MISO | 12 | shared with the touch controller |
| LCD CS | 15 | |
| LCD DC | 2 | |
| LCD RESET | — | tied to the ESP32 `EN` pin, so `-DTFT_RST=-1` and TFT_eSPI issues a software reset |
| Backlight | 27 | active high, PWM-dimmable |
| Touch CS / IRQ | 33 / 36 | XPT2046 |
| microSD | 5, 18, 19, 23 | VSPI |
| RGB LED | 22 / 16 / 17 | R/G/B, common anode — **lit at LOW** |
| Audio | 26 DAC, 4 amp-enable | |
| Battery sense | 34 | |

**This panel is BGR**, so `-DTFT_RGB_ORDER=TFT_BGR` is set. That was measured, not
derived: with TFT_eSPI's ST7789 default (RGB) the amber door lamps rendered blue and the
blue ground-power lamp amber. Reasoning from LCDWIKI's documented `MADCTL` value had predicted
RGB — the hardware disagreed. Inversion is right at the default; the background is dark.

### What's left for MobiFlight

Not much, and that is the honest trade for an all-in-one board. Only these are free and
broken out to the 1.25 mm JST headers:

| GPIO | Notes |
|------|-------|
| 27 | the backlight — the custom device needs this one |
| 21 | SPI header CS, full function |
| 32 | I2C header SDA, full function, ADC1 |
| 25 | I2C header SCL, full function |
| 35, 39 | **input only, and no internal pull-up** — offered as analog inputs, not buttons |

GPIO 18/19/23 are on the SPI header too, but they are the microSD bus; using them means
giving up the card slot, so they are not offered. `tools/check_definitions.py` enforces
this list — it fails the build config if the board JSON ever offers a committed pin.

Those pins take buttons, encoders, LED outputs and analog inputs. They do **not** take
stepper motors, servos or character LCDs: the annunciator builds compile that part of the
MobiFlight core out, on both boards, and the board definitions tell the Connector so. A panel
board has no use for them, and the stepper library is GPL v3 — linking it would have made
every firmware image a GPL v3 work. `custom_without` in `Annunciator_platformio.ini` says how
to put them back if you ever need to.

### Touch

Two panels, one interface. `Touch.h` declares six functions; `TouchXPT2046.cpp` implements
them for the resistive board and `TouchAXS15231B.cpp` for the capacitive one, and
`TouchZones.cpp` — which turns a point into a MobiFlight button event — knows about
neither.

The capacitive panel is factory-calibrated and reports pixel coordinates directly, so it
needs no calibration at all: it takes the four-point calibration message, says so on
screen for a second and a half, and carries on. That matters because the profiles are
shared between the boards, and one of them sends that message. The rest of this section is
about the resistive panel.

The XPT2046 shares the display's SPI bus (chip select GPIO 33), so it needs no pins of its
own. `TouchXPT2046.cpp` polls it directly — a pressure read and a position read, well under a
millisecond — because TFT_eSPI's own `getTouch()` spends 20–30 ms per call and would stall
MobiFlight's serial handling. Presses count only while the panel is lit.

**How a press reaches the sim.** A custom device's messages only run Connector → board, but
the board can send a **button event** under any name: `7,<name>,0;` on press, `7,<name>,1;`
on release, exactly what a button wired to the board would send. The Connector matches an
input config to it by the board's serial and that name alone — the name does not have to be
a device in the board's config. So each touch zone is a named button, and the profiles bind
it to a PMDG event. A thin ring shows round the zone while it is held; the lamp itself only
changes when the sim says so.

The names:

| Panel | Buttons |
|-------|---------|
| Doors | `AIR STAIR`, `EQUIP`, `FWD ENTRY`, `LEFT FWD OVERWING`, `LEFT AFT OVERWING`, `AFT ENTRY`, `FWD SERVICE`, `RIGHT FWD OVERWING`, `RIGHT AFT OVERWING`, `AFT SERVICE`, `FWD CARGO`, `AFT CARGO` |
| Master caution, captain / F/O | `FIRE WARN L`, `MASTER CAUTION L`, `RECALL L` / the same with `R` |
| Master caution, both | `FIRE WARN`, `MASTER CAUTION`, `RECALL L` (top six-pack), `RECALL R` (bottom) |
| Master caution + AFDS | the same four, plus `AFDS AP`, `AFDS AT`, `AFDS FMC` |
| IRS display unit | `ISDU 0` … `ISDU 9`, `ISDU ENT`, `ISDU CLR`; `ISDU DSPL TEST`, `ISDU DSPL TK GS`, `ISDU DSPL PPOS`, `ISDU DSPL WIND`, `ISDU DSPL HDG STS`; `ISDU SYS DSPL L`, `ISDU SYS DSPL R` |

GRD PWR is not a button: the PMDG has no event that connects ground power. The two
overwing lamps on a side send the same event (the SDK has one per side), so either toggles
both exits. On the ISDU a press anywhere round a knob picks the position nearest that
direction, and the pressed legend lights while it is held; the knob itself turns when the
sim reports it has.

**Presses count only while the panel is lit** — nothing happens while it is dark, not even
a line in the Connector's log. In practice: switch the battery on before using the door
buttons (a dark panel is also an invisible one), and press only under Run — the Connector
ignores inputs in Test Mode.

In the Connector these buttons do not appear in the input config's device list (they are
not devices on the board), so bind them with the generated profiles. *Scan for input* does
pick up a press, as it would a wired button.

**Calibration.** The firmware carries a calibration measured on this board, which is
usually close enough for another E32R32P. If presses land off target, calibrate: run
`tools/mfsim.py touchcal`, send message `106` = 1, or **hold a finger on an empty part of
the screen for 6 seconds**. Four crosses appear one at a time; press the centre of each.
The result is saved on the board (NVS) and survives power cycles and reflashing. On this
board the XPT2046's X axis runs down the screen and its Y from right to left — nothing a
default could guess.

## Build and flash

MobiFlight cannot flash an ESP32 (it only drives avrdude or a UF2 drive), so the board JSON
sets `CanInstallFirmware: false` and uploads are a PlatformIO job:

```sh
pio run                                     # both boards; first run clones the core into ./src
pio run -e annunciator_e32r32p   -t upload  # the 3.2in board
pio run -e annunciator_c3248w535 -t upload  # the 3.5in board
pio device monitor
```

Both envs are in `default_envs`, so a bare `pio run` builds both and the package ends up
with both firmwares in it.

On the **E32R32P** there is no native USB — everything goes through the CH340C, which is
also the port MobiFlight talks to. Two things that catch people out on that board:

- The USB-C socket has no CC pull-downs, so a **USB-C-to-USB-C cable will not enumerate**.
  Use USB-A to USB-C.
- The CH340C throws `Invalid head of packet` at 460800, PlatformIO's default for
  `esp32dev`. `upload_speed = 230400` is pinned in the env — measured reliable on this board.

On the **C3248W535** the ESP32-S3 provides USB itself, so there is no bridge chip, no
`Invalid head of packet`, and none of the DTR trouble described under *Installing*. It
enumerates as `303A:1001` — on a Mac that is `/dev/cu.usbmodem*` rather than the
`/dev/cu.usbserial*` a bridge gives you. If a flash ever fails to start, hold BOOT while
plugging it in.

A verified image of the factory LVGL demo is in `backup/` (4 MB, SHA-256 alongside) if you
ever want the board back as shipped:
`python -m esptool --port <port> --baud 230400 write-flash 0x0 backup/e32r32p_factory_4MB.bin`.

Every `pio run` and every upload also writes `_dist/Annunciator_<version>.zip`, rebuilt
from the current Connector definitions each time (`pio run -t annunciator_package` does
only that). One package serves both boards: it carries both board definitions, the nine
device definitions they share, and a firmware binary for each board that has been built. That binary is the **application image**, not a merged
flash image — it is there for reference, not for `esptool write_flash 0x0`. Use
`pio run -t upload`.

## Installing into MobiFlight Connector

Needs **MobiFlight Connector 11.2 or later** (11.2.0 is what the sim PC runs; the version is
in the title bar and Help → About). 10.x cannot open the profiles at all. 11.0/11.1 are
untested: the profiles also carry the older fields those versions match touch buttons on,
but upgrade rather than troubleshoot.

**0. Flash every board with the current build** — `pio run -e annunciator_e32r32p -t upload`
(see *Build and flash*). The profiles switch a panel off through message `107`; a board
flashed before `107` existed ignores it and stays lit with the battery off. On the bench,
`tools/mfsim.py send 107 1` must darken the panel.

There is no board configuration to write by hand. The board and device definitions under
`Annunciator/Community/` *are* the custom-board config, and installing them is a copy.

**1. Install the definitions.** Every build and upload writes `_dist/Annunciator_<version>.zip`,
fresh from `Annunciator/Community/` (`tools/check_definitions.py` fails if the zip no longer
matches the definitions). Unzip it over the Connector's community folder — again after any update to the
definitions — so that you end up with:

```
%LOCALAPPDATA%\MobiFlight\MobiFlight Connector\Community\Annunciator\
    boards\annunciator_e32r32p.board.json
    devices\annunciator_door.device.json        (and one per panel: irs, elec, air,
    devices\annunciator_irs.device.json          fctl, isdu, mcs_l, mcs_r, mcs_both)
    ...
    firmware\annunciator_e32r32p_<version>.bin      (not used by the Connector)
```

**Restart the Connector afterwards** — it reads definitions once at startup. Without a
panel's definition the Connector cannot add that panel, and its outputs' editor has no
message list: pressing OK there silently rebinds the output to the panel's first lamp.

**Never uninstall the Connector to update it** — run the new installer over the old one. The
uninstaller deletes `%LOCALAPPDATA%\MobiFlight` completely: these definitions, the Connector's
settings, and any project saved under it. Keep your projects somewhere else (Documents).

**2. Plug the board in.**

> Everything from here to the end of this step is about the **E32R32P** and its CH340C.
> The C3248W535 has its own USB ID (`303A:1001`), which no other definition claims, and no
> DTR-driven reset circuit: plug it in, and the Connector finds it. Skip to step 3.

Use a USB-A to USB-C cable (C-to-C will not enumerate — see above).
Windows 10/11 normally installs the CH340 driver itself; if no COM port appears, install
WCH's CH341SER driver. The Connector then lists it as **Annunciator**.

More than a dozen definitions share its USB ID (`1A86:7523`), and the Connector connects
with the first it loads — a stock Arduino's, which opens the port with DTR on; this board's
own definition (DTR off) never gets the chance. On Windows that open briefly pulses RTS
first, and on this board DTR and RTS drive the ESP32's reset and boot pins (the "one-click
download" circuit): together they make exactly the sequence a flashing tool uses to enter
the ESP32's bootloader. The board then never answers, and the Connector lists it as a
**"compatible module"** offering to upload firmware (decline — it cannot flash this board).
Pressing the board's reset button does not help while the Connector holds the port, and
closing and restarting the Connector repeats the sequence.

The fix, with no change to the board: **set the port's RTS line off before the Connector
starts.** Its open then raises DTR without pulsing reset, and the board keeps running its
firmware. Confirmed on the sim PC — the CH340 driver remembers the setting until the board is
replugged or Windows restarts, so do it every time you start the Connector. Create this batch
file on the sim PC (in Notepad — a file made there is not marked as downloaded, which is
what Smart App Control objects to), with the board's COM number from Device Manager → Ports,
and start the Connector with it instead of its own shortcut:

```bat
@echo off
rem Annunciator: RTS off on its port before the Connector opens it, so the ESP32
rem is not reset into its bootloader. Add a mode line per annunciator board.
mode COM7: dtr=off rts=off >nul
start "" "%LOCALAPPDATA%\MobiFlight\MobiFlight Connector\MFConnector.exe"
```

If Windows blocks the batch file, a shortcut does the same through the signed `cmd.exe`:
right-click the desktop → New → Shortcut, target
`cmd.exe /c mode COM7: dtr=off rts=off & start "" "%LOCALAPPDATA%\MobiFlight\MobiFlight Connector\MFConnector.exe"`.

- **Keep the board in the same USB socket.** The CH340 has no serial number, so Windows ties
  the COM number to the physical port; another socket means another number.
- **If the Connector was started without it** and lists the board as a compatible module:
  close the Connector, press the board's RESET button (or replug it), then use the batch file.
- [`windows/Start-MobiFlight-E32R32P.bat`](../windows/Start-MobiFlight-E32R32P.bat) is that
  batch file, ready to use: set `PORTS` at the top (several boards, space-separated) and start
  the Connector from it. It says which port it could not open rather than failing quietly,
  and it is on every release.
- The alternative, if you would rather not: edit the Connector's stock definition once — in
  `%LOCALAPPDATA%\MobiFlight\MobiFlight Connector\Boards\arduino_mega.board.json` change
  `"DtrEnable": true` to `false` and restart the Connector. That works for every CH340 port,
  not just this one, and a Connector update overwrites it.

**3. Choose the panel.** Each board carries exactly one custom device, and its type is the
choice. In **Extras → Settings → MobiFlight Modules** select the board, then *Add device →
Custom Devices* and pick the panel (the table at the top of this file). Add it **before** any buttons or other
devices on the board and leave its pin at **27**: the backlight is fixed on GPIO 27, and the
firmware drives 27 whatever the pin is set to — so never give 27 to a button, output or any
other device on this board (it logs a debug line if the pin is not 27). Then upload the
config to the board.

**How the Connector shows it.** The board is the controller (*Annunciator*); the whole screen
is its one custom device (*Door Annunciator*). The lamps are not devices of their own — a
MobiFlight device is something on pins, and the lamps exist only on the screen — they are
the device's message types: an output config picks *Door Annunciator* as its display, then
the lamp. The touch buttons are not devices either: the board sends each press as a button
event named after the lamp, and the input configs in the panel's profile tab (*Door
Annunciator - touch FWD ENTRY …*) bind them by that name.

**To switch a board to another panel:** Extras → Settings → MobiFlight Modules; expand the
board, select its device, **Remove device**; select the board, **Add device → Custom Devices
→** the new panel (pin 27, name as offered); **Upload config** (Stop first, Run afterwards).
Then, in the project, remove the old panel's tab (⋮ → Remove) and merge the new panel's file
(+ → From existing project). Every file in `profiles/` is generated for this board's serial,
so it binds to the board whichever panel it is.
Press **Stop before uploading a config**, and Run afterwards: the Connector resends only
values that change until the next Stop, so a board configured during Run shows a partial
picture until then.

The device gets the panel's name — `Door Annunciator`, `IRS Annunciator`, `Electrical
Meter`, `Air Conditioning`, `Flight Controls`, `IRS Display Unit`, `Master Caution L`,
`Master Caution R`, `Master Caution` — and that is exactly the name the profiles look for.
**Don't rename it** (or regenerate the profiles with `--name`, see below). A board set up
with the current `tools/mfsim.py setup` already shows its device, under the same name,
because the Connector reads the config back from the board.

**Boards set up before the device rename** hold the old name: the ISDU was `IRS Display`, the
Electrical panel `Electrical Meters`. The profiles now look for `IRS Display Unit` and
`Electrical Meter`, and nothing reaches a panel whose name does not match — it stays dark
and ignores touches. Re-run `tools/mfsim.py setup isdu` (or `elec`), or rename the device in
the module settings and upload the config, or regenerate with `--name isdu="IRS Display"`.
`tools/mfsim.py info` shows the stored name and warns on a mismatch.

The optional *Additional config* field is the startup backlight brightness, 16–255 (lower
values are raised to 16; empty means 255).

**Board names.** With one board, leave its name alone. With several, either rename **every**
board to a unique name of at most 16 characters ("Doors", "Glareshield") and regenerate the
profiles with those names, or rename **none**. When a project loads, the Connector binds each
tab to a connected board, and it prefers a board with the right *name* over one with the
right serial — so with some boards renamed and some not, a profile can land on the wrong
board. A "Great news … updated your project" message with several boards plugged in is the
sign; check the tab's board before saving.

**4. Wire lamps to the sim.** For the PMDG 737-800, merge the ready-made config for the
panel into your project — see *MobiFlight profiles* below. To wire it by hand instead: each
lamp is an output config. Set the display device to the board's custom device and choose
the lamp as the message type (the list comes from the device definition — `FWD ENTRY`,
`L IRU ALIGN (white)`, …). Then bind the aircraft's variable for that light. The firmware
treats the value as `0` off, `1` on, `2` blink, so a plain 0/1 light variable works as-is.

**One quirk to know about.** While the Connector is connected it holds the USB DTR line,
and on this board that line also drives the ESP32's boot pin. If the board *resets while
connected* — its reset button, a brown-out — it comes back up in the ESP32's bootloader
with a dark screen. Close the Connector, press the board's RESET button, and start the
Connector with the batch file from step 2.

## MobiFlight profiles for the PMDG 737-800

`profiles/` holds a ready-made config per panel, binding every lamp and readout to the PMDG
737-800 (MSFS 2024), and every touch button to the aircraft:

| File | Outputs | Touch buttons |
|------|---------|---------------|
| `Annunciator-Doors-PMDG737-800.mfproj` | 13 door/GRD PWR lamps | 12 doors |
| `Annunciator-IRS-PMDG737-800.mfproj` | 11 IRS lamps | |
| `Annunciator-Electrical-PMDG737-800.mfproj` | 3 lamps, both display lines, both selectors | |
| `Annunciator-AirCond-Bleed-Press-PMDG737-800.mfproj` | 16 lamps | |
| `Annunciator-FlightControls-PMDG737-800.mfproj` | 9 lamps | |
| `Annunciator-IRS-DisplayUnit-PMDG737-800.mfproj` | both displays, dots, both knobs, ENT/CLR cue lights | 12 keys, 7 knob positions |
| `Annunciator-MasterCaution-Captain-PMDG737-800.mfproj` | 8 lamps | 3 |
| `Annunciator-MasterCaution-FO-PMDG737-800.mfproj` | 8 lamps | 3 |
| `Annunciator-MasterCaution-Both-PMDG737-800.mfproj` | 14 lamps | 4 |

Each also has the same three **shared** outputs: **night dimming** (`102`) and **lamp test**
(`103`), both following the main panel's TEST / BRT / DIM lights switch; and **bus
unpowered** (`107`) from the DC BATT BUS, so the panel lights when the battery switch goes on.

**Merge, don't open.** Each file is one config tab, meant to go *into* the project you fly
with — the one with your B107 config. In the Connector:

1. Open your project (the B107's).
2. Click **+** after the last tab → **From existing project**, and pick the panel's file.
   It arrives as a new tab, bound to its board.
3. Save. If your project is still the old `B107_2.mcc`, the first save writes
   `B107_2.mfproj` beside it (if one is already there, it asks where to save instead: pick
   that file and confirm the overwrite, or choose a new name); open that from then on.

*File → Open* on a panel file would instead **replace** your project: the B107 stops, and the
panel's first-run checks (battery, IRS switches) then do nothing. Fine for a bench check of
the panel alone, never while flying.

**Updating a merged panel:** delete its old tab first — click the **⋮** at the right end of
that tab → Remove (right-clicking does nothing; Remove takes effect at once, without asking)
— then merge the new file and Save. The Connector never updates a tab in place — a second
copy runs alongside the first, and every touch then fires twice (a door toggle opens and
shuts again, ISDU digits are doubled). Each tab's name starts with a short code, e.g.
`[0a2eed] Doors`, that changes whenever its content does; `make_profiles.py` prints the
current one for each panel, and a newly merged tab appears at the right end. Never merge a
panel's own file and the Combined file together.

**If your aircraft auto-loads a linked config** (the icon by the aircraft name; off by
default), the link still points at the old `B107_2.mcc`: loading a flight would reload it
and drop the panels. With the -800 loaded, link the merged `.mfproj` instead (or unlink).
The log says `Auto loading config for ...` when this happens.

**What the sim PC needs:**

- **FSUIPC7 7.5.6 or later.** The outputs read the PMDG SDK through FSUIPC7 offsets, because
  those are typed and documented. (HubHop's `L:switch_NN_73X` lamps and click spots also
  work on the 2024 -800 — the B107 uses them.)
- **MobiFlight's WASM module**, which the Connector installs. The touch buttons of master
  caution and the ISDU keys use it, as do the ISDU knobs, its ENT/CLR cue lights and the
  IRS ILS/GLS lamps — the same route every B107 input takes.
- **The PMDG's data broadcast.** In `737_Options.ini` of the -800 (package
  `pmdg-aircraft-738`):
  - Steam: `%APPDATA%\Microsoft Flight Simulator 2024\WASM\MSFS2024\pmdg-aircraft-738\work\`
  - Microsoft Store / Game Pass: `%LOCALAPPDATA%\Packages\Microsoft.Limitless_8wekyb3d8bbwe\LocalState\WASM\MSFS2024\pmdg-aircraft-738\work\`

  Load the -800 once (past ready-to-fly) so the file exists, close the sim, and append
  ```ini
  [SDK]
  EnableDataBroadcast=1
  ```
  followed by an empty line (PMDG drops the last line otherwise). If the B107's FLT ALT /
  LAND ALT windows show digits in the -800, it is already on. (FSUIPC's own PDF gives the
  MSFS 2020 -700's path; the SDK header's `737NG3_Options.ini` is an old name.)

Every offset was taken from FSUIPC's own *Offset Mapping for PMDG 737* table and every event
number from the PMDG SDK header; the generator re-checks both against those files, and each
touch button against the firmware's zone table.

**How the touch buttons reach the aircraft.** Master caution, FIRE WARN, six-pack recall and
the ISDU keys press the aircraft's own click spots (`ROTOR_BRAKE` codes through the WASM
module — HubHop's codes, and the B107's route). The six-pack is hold-to-recall, like the real
one: it gets the release too. The doors send PMDG SDK events through FSUIPC7 (*FSUIPC - PMDG
- Event ID*, left-click parameter); they have no click spot. FSUIPC7 passes custom control
numbers 69632–84232 through by default, which covers them (83637–83649). The ISDU knobs run
HubHop's knob code: it reads the knob's position once and steps it to the one you touched —
tap again if a quick double tap overshoots. `--inputs fsuipc` sends master caution and the
ISDU keys as PMDG events through FSUIPC7 too, if the click spots ever misbehave.

Two traps if you edit a door binding by hand: **MobiFlight's own PMDG 737 event list is the
old P3D NGX one**, which numbers the equipment hatch and airstair one lower than the MSFS SDK
(the editor even labels the EQUIP binding `EVT_DOOR_AIRSTAIR`), and picking an event from that
list resets the parameter to 0. Don't pick from the list; if you did, restore the event
number shown in the config's name and the parameter `536870912`.

**Profiles address a board by serial (and name), and a panel by its device name**, so both
must match. The files in `profiles/` are generated for this board (`SN-B0CBD803AF24`, named
`Annunciator`) with the device names above. For another board, several boards, or renamed
devices, regenerate:

```sh
python3 tools/make_profiles.py --serial SN-XXXXXXXXXXXX                   # every panel, one board
python3 tools/make_profiles.py --board door=SN-AAAAAAAAAAAA:Doors \
                               --board mcs_both=SN-BBBBBBBBBBBB:Glareshield   # one board each
python3 tools/make_profiles.py --name door="My Doors"                     # a renamed device
```

With `--board` it also writes `Annunciator-Combined-PMDG737-800.mfproj`: all those panels,
one tab each, so one merge brings in every board. (A plain run deletes an old Combined file,
so a stale one can't linger.) A device name mismatch marks the tab's rows with a device
error and logs `ConfigErrorException_SetCustomDevice`; a wrong board serial fails silently.

**Testing from the Connector.** *Test Mode* steps through the selected tab's outputs, sending
each its test value and then `0`; the panel stays lit throughout, because bus power is sent
as "bus unpowered" (`107`), for which `0` means powered. Set Extras → Settings → General →
*Test mode speed* to Slow (1000 ms; the default is 50 ms) so each step is visible. Stop
clears the panel, so a test starts from a blank panel rather than the last flight's lamps.

A row's own *Test* button always sends `1`: a text display shows "1", and on the *Bus
unpowered* row the panel goes dark (1 = unpowered) — click Test on the row again to end it.
A row still under test (flask icon) is skipped while running, so leaving *Bus unpowered*
under test and pressing Run keeps the panel dark until Stop. With the Connector stopped, a
row test leaves the panel lit afterwards until Run, a Test Mode start, or the board has been
up 15 minutes. Presses on the touch screen only reach the sim while running (Run), never in
Test Mode.

**First evening, in order** — each step settles something before the next depends on it:

1. Every board flashed with the current build; Connector 11.2 or later; the board listed as
   **Annunciator**, with its device named exactly as in *Installing* step 3.
2. Merge the panel file into your project; Run. Load the -800 cold and dark: the panel is
   dark. Battery on: it lights. (If not, look at the *Bus unpowered* row's **Final Value**:
   it should be 0 with the battery on. Its Raw Value — the PMDG's bus-powered flag — shows
   the opposite. If Final Value stays 1, the PMDG broadcast is off, see above.)
3. Lights switch to TEST: every lamp lights; DIM: the lamps dim.
4. Doors: tap FWD ENTRY — the door opens; tap again — it closes. Tap EQUIP and AIR STAIR
   (the airstair is an option the -800 may not have). An overwing lamp toggles *both*
   exits on its side — the SDK has one event per side — so tap only one of a side's two.
5. Master caution: with a caution lit, tap MASTER CAUTION — it resets; hold a six-pack — all
   its captions light until you let go.
6. ISDU: turn an IRS to NAV; tap PPOS on the ISDU — the knob turns and the display shows
   position; tap HDG STS; tap R then L under SYS DSPL.
7. Only then trust the rest. What only the sim can settle is listed under *Known gaps*.

**Why the panel is dark — it is almost never a crash:**

- **The tab's rows show a device error**, or the log says
  `ConfigErrorException_SetCustomDevice`: the device name on the board does not match the
  profile, so nothing reaches the panel — dark, and deaf to touches. Fix the name (see
  *Installing* step 3); Stop and Run will not help.
- **The B107 is dark too:** the Connector stopped (Stop pressed, FSUIPC7 or the sim closed,
  a USB device plugged in or out — any USB change stops it — or an aircraft auto-load).
  Press Run.
- **Only the panel is dark, Connector running:** look at the **Final Value** of the tab's
  *Bus unpowered* row (not Raw Value, which is inverted). 1 means the DC BATT BUS is off
  (battery off, or no PMDG data — see step 2); that is correct. 0 but still dark: a flask
  icon on that row means a row test is still on — click Test on it again; otherwise Stop,
  wait two seconds, Run.
- **Lit with the battery off, while *Bus unpowered*'s Final Value reads 1:** the board runs
  firmware from before message `107`. Reflash it.
- **After a Stop, Test Mode, or 15 minutes with the Connector not running:** dark by design
  (MobiFlight's Stop and power saving).
- **The board restarted** (a crash or a brown-out): it is dark until values change; Stop,
  wait two seconds, Run resends them. At Log Level Debug the log names the reason —
  `Annunciator started: CRASH (panic)`, `CRASH (watchdog)`, `BROWN-OUT (supply dipped)`,
  `power on` (see *Seeing the board's messages*). If Debug logging shows nothing after a
  restart and the module is not listed as Annunciator, it is in its bootloader: close the
  Connector, press the board's RESET button, and start the Connector with the batch file
  (see *Installing* step 2).

## Message reference

Lamp payloads are `0` dark, `1` lit, `2` lit and blinking at 1 Hz.

A few lamps show two colours, because the aircraft uses colour to say how serious it is:
`3` lights the second colour and `4` blinks it. Only the autoflight lights on
`ANNUN_MCS_BOTH_AFDS` have one so far. A lamp with a single colour treats `3` and `4` as
`1` and `2`, so a binding cannot break a panel by sending them.

**Door panel** — the 737-800's, lamp IDs 0–12. The two centre columns are four deep; the
outer columns hold two lamps each, level with the overwing rows. IDs run down each column:

| IDs | Lamps, top to bottom | Where | PMDG (FSUIPC7) |
|-----|----------------------|-------|----------------|
| 1–2 | AIR STAIR, EQUIP | left column | `0x657A`, `0x657E` |
| 3–6 | FWD ENTRY, LEFT FWD OVERWING, LEFT AFT OVERWING, AFT ENTRY | second column | `0x6578`, `0x657B`, `0x657F`, `0x6582` |
| 7–10 | FWD SERVICE, RIGHT FWD OVERWING, RIGHT AFT OVERWING, AFT SERVICE | third column | `0x6579`, `0x657C`, `0x6580`, `0x6583` |
| 11–12 | FWD CARGO, AFT CARGO | right column | `0x657D`, `0x6581` |
| 0 | GRD PWR | apart, bottom left | `0x64AE` |

Lamp 0 lights **blue**: it is an advisory, not a caution, which is what the real aircraft
does. The other 12 are amber.

**IRS panel** — lamp IDs 0–10: `0` GPS, `1` ILS, `2` GLS, then `3`–`6` for L IRU
`ALIGN` / `ON DC` / `FAULT` / `DC FAIL` and `7`–`10` for the same on the R IRU. Both
`ALIGN` lamps light **white**; the rest are amber. Send `2` to `ALIGN` for the 1 Hz blink
the aircraft uses to ask for present position. On the PMDG, ILS and GLS have no SDK field;
the profile reads the aircraft's L-vars for them (`L:switch_2461_73X` / `2462`), which exist
only with the MMR option — without it they stay dark.

**Electrical metering panel** — the AC & DC metering panel from the forward overhead:

| ID | Meaning | PMDG source (FSUIPC7) |
|----|---------|-----------------------|
| 0 | BAT DISCHARGE lamp | `0x64A0` |
| 1 | TR UNIT lamp | `0x64A1` |
| 2 | ELEC lamp | `0x64A2` |
| 10 | display top line — 12 characters: DC AMPS, blank, CPS FREQ | string `0x64BC`, 13 bytes |
| 11 | display bottom line — DC VOLTS, AC AMPS, AC VOLTS | string `0x64C9`, 13 bytes |
| 12 | DC selector position `0`–`7` (optional) | `0x64A3` |
| 13 | AC selector position `0`–`6` (optional) | `0x64A4` |

The window is two lines of green 5×7 dot-matrix LEDs, three right-aligned 4-character
fields per line, drawn from the classic 5×7 LCD font — the one whose `1` has the foot seen
on the real display. The sim works out what the display shows from the selectors, so the
panel just mirrors the two text lines, character for column. Nothing on the way trims
them, so a line that arrives short has lost trailing blanks, and is padded on the right. If the selector positions are sent, the
panel names them under the lamps; leave them unbound and that row stays empty.

The real panel's **controls** are the DC selector (8 positions), the AC selector (7) and a
momentary MAINT button. They are not on screen. Wire them as ordinary MobiFlight inputs on
the board's spare pins: two resistor-ladder rotary switches on the analog pins plus MAINT
on GPIO 21 fit the board definition as it stands.

On the PMDG the meter data comes through **FSUIPC7** offsets, which MobiFlight reads
natively: needs FSUIPC 7.5.6 or later and `[SDK] EnableDataBroadcast=1` in
`737_Options.ini`. On the iFly the display text lives only in iFly's shared-memory SDK, so
the readout is PMDG-only for now.

**Air conditioning / bleed / pressurisation** — the right-hand column of the forward
overhead, top to bottom, all amber except where noted:

| IDs | Lamps | PMDG (FSUIPC7) |
|-----|-------|----------------|
| 0–2 | ZONE TEMP — CONT CAB, FWD CAB, AFT CAB | `0x6530`–`0x6532` |
| 3 | DUAL BLEED | `0x6533` |
| 4–5 | RAM DOOR FULL OPEN L / R (**blue**) | `0x6534`, `0x6535` |
| 6–8 | left PACK, WING-BODY OVERHEAT, BLEED TRIP OFF | `0x653E`, `0x6540`, `0x6542` |
| 9–11 | right PACK, WING-BODY OVERHEAT, BLEED TRIP OFF | `0x653F`, `0x6541`, `0x6543` |
| 12–15 | AUTO FAIL, OFF SCHED DESCENT, ALTN (**green**), MANUAL (**green**) | `0x6544`–`0x6547` |

The light strip keeps the real panel's empty third position, and the bleed stacks flank the
TRIP RESET button as on the aircraft. Gauges and switches are left out; the B107 already
has the FLT ALT / LAND ALT windows.

**Flight controls** — nine amber lamps: `0`/`1` FLT CONTROL A/B LOW PRESSURE (`0x645E`,
`0x645F`); STANDBY HYD `2` LOW QUANTITY, `3` LOW PRESSURE, `4` STBY RUD ON (`0x6461`–`0x6463`);
`5` FEEL DIFF PRESS, `6` SPEED TRIM FAIL, `7` MACH TRIM FAIL, `8` AUTO SLAT FAIL
(`0x6464`–`0x6467`). The panel's tenth lamp, YAW DAMPER, is left out — the B107 has it.
Seven stacked lamps do not fit 240 px, so the unlabelled four-lamp stack stands beside
STANDBY HYD's three, a row lower, rather than under it.

**Master caution** — `0` FIRE WARN (red), `1` MASTER CAUTION, then the six-pack captions row
by row, left column first:

| Layout | IDs 2–7 | IDs 8–13 |
|--------|---------|----------|
| captain (`ANNUN_MCS_L`) | FLT CONT, ELEC, IRS, APU, FUEL, OVHT/DET | |
| first officer (`ANNUN_MCS_R`) | ANTI-ICE, ENG, HYD, OVERHEAD, DOORS, AIR COND | |
| both (`ANNUN_MCS_BOTH`) | the captain's | the first officer's |

PMDG: FIRE WARN `0x65A4`/`0x65A5` and MASTER CAUTION `0x65A6`/`0x65A7` (captain/F/O — the
combined layout uses the captain's), the six-pack captions `0x65A8`–`0x65B3`. FIRE WARN and
MASTER CAUTION light their whole face with the legend dark, as the real push-lights do; the
six-pack captions glow on a shared black face. A single side is the real unit's shape — a
strip across the middle of the screen; the combined layout stacks both six-packs down the
right.

**Master caution with AFDS** (`ANNUN_MCS_BOTH_AFDS`) — the combined layout with the three
autoflight P/RST lights added below the warning caps. Lamps `0`–`13` are exactly the
combined panel's, so a binding made for `ANNUN_MCS_BOTH` lands on the same lamp:

| ID | Lamp | PMDG | Payload |
|----|------|------|---------|
| 14 | A/P P/RST | `MAIN_annunAP[0]` `0x65F1`, `MAIN_annunAP_Amber[0]` `0x65F3` | `1` red, `3` amber |
| 15 | A/T P/RST | `MAIN_annunAT[0]` `0x65F5`, `MAIN_annunAT_Amber[0]` `0x65F7` | `1` red, `3` amber |
| 16 | FMC P/RST | `MAIN_annunFMC[0]` `0x65F9` | `1` amber |

The generated profile reads each red/amber pair as one four-byte offset and picks the
colour in the transformation, red winning when both are set — the warning outranks the
caution, as on the aircraft. The three lights are also touch buttons (`AFDS AP`, `AFDS AT`,
`AFDS FMC`), but nothing is bound to them: the offsets for the lights are documented, the
event numbers for pressing them are not, and a made-up event number would look bound and do
nothing. They are listed in `UNBOUND_ZONES` in `tools/make_profiles.py`; run it with
`--sdk-header` against your own copy of the PMDG SDK to add them.

The autoflight lights sit well below the warning caps because they are separate units on
the aircraft — the caps are in the glareshield panel, the P/RST lights under the MCP.

**IRS display unit** — the two segmented displays of the aft overhead's ISDU, its knobs and
keypad:

| ID | Meaning | PMDG |
|----|---------|------|
| 0 | left display — up to 6 characters | string `0x642E`, 7 bytes |
| 1 | right display — up to 7 characters | string `0x6435`, 8 bytes |
| 2 | degree / decimal / minute dots: `0` dark, `1` lit | `0x643D` |
| 3 | DSPL SEL: `0` TEST, `1` TK/GS, `2` PPOS, `3` WIND, `4` HDG/STS | `0x6420` |
| 4 | SYS DSPL: `0` L, `1` R | `0x6421` |
| 5 / 6 | ENT / CLR cue lights: `0` dark, anything else lit | `L:switch_242_73X` / `L:switch_245_73X` |

Characters go into the cells from the left, as the PMDG fills its own virtual display (a
3-digit heading occupies the first three cells). The first cell of each display can show
N/S and E/W; the rest are 7-segment digits. The dots are one flag, as the PMDG has it.

**Shared by every panel**, so a panel is more than lamps:

| ID | Meaning |
|----|---------|
| 100 | screen rotation — `1` normal, `3` rotated 180° |
| 101 | backlight brightness 16–255, applied while lit (lower is raised to 16: dark is bus power's job) |
| 102 | night dimming — drops luminance and kills the glow |
| 103 | lamp test — lights everything |
| 104 | run the self test |
| 105 | **bus power** — `0` dark, `1` lit. For binding a bus *voltage* directly (≥ 0.5 counts as powered) |
| 106 | touch calibration — `1` runs it (touch panels; a bench step, not something to bind) |
| 107 | **bus unpowered** — `1` dark, `0` lit: 105 inverted. What the profiles bind (DC BATT BUS, `if($=0,1,0)`), because Test Mode and the end of every test send `0`, and `0` must not darken the panel (a row's own Test sends `1`: dark) |

MobiFlight's own reserved IDs are handled too: `-1` (Stop pressed / Connector shutting
down) and `-2` (power saving) both turn the panel dark. Stop also forgets the bus state, so a
test after a battery-off flight does not find the panel still dark.

## When the panel is lit

Only while **all three** hold — otherwise the backlight is off and the screen is black:

1. **MobiFlight is running** — it has sent the panel a value since the last Stop. Merely
   having the Connector open is not enough: it reads the board on connect without sending
   anything, so the panel stays dark until you press Run, and goes dark again on Stop.
2. **The bus is powered** — message `107` (or `105`). Leave both unbound and this is
   always true. Stop resets it to powered (and clears the lamps, dimming and lamp test);
   the next Run sends everything again. A config upload keeps it.
3. **MobiFlight power saving is off.** The core firmware turns power saving on 15 minutes
   after the last "power saving off" command. Lamp values do not count as activity: only
   that command resets the clock, starting from boot. The Connector sends it at Run and
   every 5 minutes while running, so a long cruise with nothing changing does not darken
   the panel; Stop asks for power saving again — which the core honours only once the board
   has been up 15 minutes, but Stop's own `-1` darkens the panel regardless. If the
   Connector is killed without a clean Stop, the timeout darkens the panel 15 minutes later.
   On the bench, `tools/mfsim.py` behaves the same way (see below).

A board with no config at all stays dark too: its backlight is driven low from
`initVariant()`, the earliest point application code runs, before `setup()`.

Lamps keep being drawn while dark, so the panel reappears instantly showing the current
state. On waking at Run it also waits for MobiFlight's opening burst of values to land — the
Connector pauses about 16 ms before each, so a 20-output panel takes a third of a second —
until the bus-power message, which is last in every profile, or 500 ms without one. So it
never shows a stale frame, and never flashes on when the bus turns out to be off.

The board says why it last started — `Annunciator started: power on`, `CRASH (panic)`,
`BROWN-OUT (supply dipped)`, … — as a debug line when it boots. The Connector logs it at Log
Level Debug only (see *Seeing the board's messages*); `tools/mfsim.py listen` shows it, with
the ESP32's own boot text, if it was already running when the board restarted.

Brightness (`101`) and bus power (`105`/`107`) are separate on purpose. Bus power is naturally a
0/1 (or a voltage), brightness a level, and keeping them apart lets each bind straight to
one variable with no MobiFlight transform juggling two.

## Self test

The panel runs nothing by itself on power-up. Message `104`, or `tools/mfsim.py selftest`,
cycles a few demo states — the original design's for the door and IRS panels, real
situations for the others (engine start on APU bleed, a fire, a master caution recall, the
FCOM's PPOS readout…) — at 2.5 s each, and the next value from MobiFlight ends it.

## Bench testing without the Connector

The Connector is Windows-only, so `tools/mfsim.py` speaks the same serial protocol from any
machine. It needs pyserial, which PlatformIO's own interpreter already has:

```sh
PY=~/.platformio/penv/bin/python
$PY tools/mfsim.py info           # identify the board, its device and names; check them against the JSON
$PY tools/mfsim.py setup door     # upload + activate a config: door, irs, elec, air, fctl,
                                  #   isdu, mcs_l, mcs_r, mcs_both
$PY tools/mfsim.py demo door      # walk through a few states over MobiFlight messages
$PY tools/mfsim.py lamp 3 1       # one lamp: id, then 0 off / 1 on / 2 blink
$PY tools/mfsim.py send 102 1     # any message ID -- here, night dimming
$PY tools/mfsim.py selftest       # cycle the design's demo states
$PY tools/mfsim.py send 107 1     # bus unpowered: panel dark (107 0 lights it again)
$PY tools/mfsim.py stop           # exactly what the Connector sends on Stop
$PY tools/mfsim.py powersave on   # MobiFlight power saving: dark now (off wakes it)
$PY tools/mfsim.py listen         # print touch buttons as the Connector would receive them
$PY tools/mfsim.py touchcal       # run the touch calibration and print what it measured
```

Like the Connector at Run, every command that drives the panel first sends "power saving
off", and `listen` and `touchcal` repeat it every 5 minutes. A panel left alone after a
command still goes dark 15 minutes later — the core's power saving, not a crash — and the
next command wakes it.

`info` applies the same checks the Connector does — the type must match `MobiFlightType`
exactly, and the core version must parse as a .NET `System.Version`. Add `-v` to any
command to see the raw traffic.

## Where the look comes from

The two panels were designed as 320×240 mockups in Claude Design
(`Door Annunciator Panel.dc.html`, `IRS Annunciator Panel.dc.html`). The colour table in
`Theme.cpp` is taken verbatim from the `lamp()` helper both mockups share, and the geometry
constants at the top of each panel `.cpp` are derived from their flex layouts.

The door panel departs from its mockup too. The mockup was drawn from a 737-900ER panel —
mid exits, and outer columns staggered half a row around them — with GRD POWER AVAILABLE
in an empty cell. The panel now follows the **737-800**: no mid exits, an AIR STAIR lamp,
~2:1 lenses on a plain grid, and `GRD PWR` — not a door, it lives on the electrical panel —
standing apart at the bottom.

### Buttons that look like buttons

The glareshield lights are not flat windows: they are things you press, and they are drawn
that way. `reliefFace()` in `PanelGfx.cpp` gives a rectangle a face lighter at the top than
the bottom, a light edge along the top and left and a dark one along the bottom and right,
as if lit from above — and turns all of that over when the shape is meant to be sunken. It
is deliberately shallow. A cap is 72px at 320 and 104px at 480 on a nearly black panel;
anything stronger reads as a cartoon rather than as moulded plastic. The edge is a pixel on
the small screen and two on the large one, so the relief looks the same size in the hand.

What is raised and what is sunken follows the aircraft, and the distinction matters:

- **FIRE WARN and MASTER CAUTION** are individual moulded caps. Each stands proud and goes
  in on its own when pressed.
- **A six-pack is not six buttons.** It is one pressable assembly with six lit windows
  behind a common faceplate. So the *plate* stands proud and shows the press, and the six
  lights are sunken into it. Pressing anywhere on it puts the whole assembly in, which is
  what recalling does on the aircraft.

A press travels the ordinary path: `TouchZones` resolves the zone and asks the panel for
feedback through the hook it already had, and `LampPanel` answers by drawing what the zone
covers as pushed in rather than by ringing it. A zone covering no lamp at all — the ISDU's
knob legends — still gets the ring. Which lamps a zone presses is worked out from the
rectangles, so a zone drawn over a lamp depresses it with no second table to keep in step.

`tools/preview` can show this without hardware: the `press` scene plants a synthetic finger
and the press then runs through the real touch path, so what you see is what the board
draws.

The IRS panel deliberately departs from its mockup. The mockup drew each IRU as a column
of four full-width bars; the panel now follows the real 737 IRS Mode Select Unit instead —
every lamp the same ~2.7:1 lens, and each IRU's four lamps in a 2×2 block (`ALIGN | ON DC`
over `FAULT | DC FAIL`), matching the reference photo and the physical knobs below it.

Two things the mockups do in CSS that a TFT cannot, and how they are approximated:

- **`box-shadow` halo** around a lit lamp → a 1px inner rim blended between the lamp's
  foreground and background.
- **`text-shadow` glow** on a lit legend → the glyph drawn in a dim copy offset 1px in four
  directions, with the bright glyph on top.

Unlit legends stay *faintly readable* rather than going fully black. That is deliberate and
matches both the mockups and the real panel.

## Sim side

The Connector and MSFS2024 run together on a Windows PC; the board plugs into that machine
over USB. Nothing in the firmware is sim-specific — it only reacts to the message IDs
above. Which sim variables drive which lamp lives in the MobiFlight profile; the ones in
`profiles/` target the PMDG 737-800.

## Fonts

Text is **DM Sans Bold**, the face the design specified, embedded as TFT_eSPI smooth
(anti-aliased) fonts at three sizes:

- **11 px** — door legends and every header. The door lamps are width-bound here, not
  height-bound: `RIGHT FWD` is 61 px of ink in a 71 px lamp, and would need 66 at 12 px.
- **14 px** — the IRS lamps and the glareshield push-lights, which have more room.
- **8 px** — small print on a lens: MASTER CAUTION's `PUSH TO RESET`, FIRE WARN's
  `BELL CUTOUT`.

The source is the official static *DM Sans 9pt Bold* from `googlefonts/dm-fonts@d0520ba`
— the commit Google Fonts pins for v4.004 — in `fonts/`, under the SIL Open Font License
(`fonts/OFL.txt`).

The generated headers in `Annunciator/fonts/` are committed, so building needs nothing
extra. To change a size or the character set, regenerate them. TFT_eSPI normally expects
Processing for this; `tools/make_vlw.py` does it with FreeType instead:

```sh
python3 -m venv .venv && .venv/bin/pip install freetype-py
.venv/bin/python tools/make_vlw.py            # regenerate
.venv/bin/python tools/make_vlw.py --check    # fail if the committed headers are stale
```

The generator writes the same layout Processing does: fed TFT_eSPI's own
Processing-generated sample font, it reproduces every byte the library parses. It also
re-parses its own output with the library's rules before writing, because the format keeps
no per-glyph offsets — one wrong bitmap length would silently shift every later glyph.

## Known gaps

- **The profiles are untested against the sim.** Formats, offsets, event numbers and names
  were checked against the Connector's source, FSUIPC's PMDG table and the SDK, but the
  first real run is on the sim PC — go through *First evening, in order* above before
  relying on them.
- **What only the sim can settle** — each with its quickest check:
  - ZONE TEMP order (`0x6530`–`0x6532` taken as CONT, FWD, AFT CAB): when exactly one lamp
    is lit in the virtual cockpit, compare. Wrong: swap the offsets in `make_profiles.py`.
  - FLT CONTROL LOW PRESSURE `[0]` = A: ground power, both electric hydraulic pumps on, FLT
    CONTROL A to OFF — only A's lamp should light.
  - The door events toggle (tap twice), and EQUIP / AIR STAIR open the right doors.
  - The six-pack recall lights while held and clears on release.
  - The ISDU knobs land on the tapped position (DSPL SEL 0 TEST … 4 HDG/STS, from HubHop),
    TEST springs back to TK/GS, and SYS DSPL flips both ways.
  - The ENT/CLR cue L-vars (`L:switch_242_73X` / `245`) light during a position entry.
  - ALIGN flashing: the SDK flag may hold steady where the virtual cockpit flashes.
  - The ISDU and electrical displays match the virtual cockpit column for column.
  - If the click spots misbehave, regenerate with `--inputs fsuipc` and compare.
- **The ISDU's colour** is a judgement: photos of real units range from cream to orange
  with the camera's white balance. It is amber leaning yellow; compare with the PMDG's
  virtual cockpit.
- **The -800 door layout** is from Fipgauges' PMDG-based -800 panel, with legend wording
  from photos of real NG panels. A photo of a real -800 panel would settle the details.
