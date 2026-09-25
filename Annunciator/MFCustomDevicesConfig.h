#pragma once

// An optional hard-coded config, baked into flash instead of being uploaded by the
// Connector. Only compiled when -DHAS_CONFIG_IN_FLASH is enabled in
// Annunciator_platformio.ini; it is off by default, and with it on the Connector can no
// longer upload a config to this board.
//
// The record format is "17.<Type>.<Pins>.<Config>.<Name>:" -- type, the backlight pin,
// the initial brightness, and a label. Swap ANNUN_DOOR for ANNUN_IRS to bake in the IRS
// panel instead. GPIO 27 is the E32R32P backlight and is fixed by the PCB -- do not put a
// different pin here; 15, for instance, is the LCD chip select.
const char CustomDeviceConfig[] PROGMEM =
    {"17.ANNUN_DOOR.27.255.Door Annunciator:"};
