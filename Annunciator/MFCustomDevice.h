#pragma once

#include <Arduino.h>

/* The custom device types this firmware answers to. These strings must match
   `Info.Type` in the corresponding Community/devices/*.device.json, and both must be
   listed in `Info.CustomDeviceTypes` in the board JSON. */
#define CUSTOMDEVICE_TYPE_DOOR  "ANNUN_DOOR"
#define CUSTOMDEVICE_TYPE_IRS   "ANNUN_IRS"
#define CUSTOMDEVICE_TYPE_ELEC  "ANNUN_ELEC"
#define CUSTOMDEVICE_TYPE_MCS_L "ANNUN_MCS_L"
#define CUSTOMDEVICE_TYPE_MCS_R "ANNUN_MCS_R"
#define CUSTOMDEVICE_TYPE_MCS_BOTH "ANNUN_MCS_BOTH"
#define CUSTOMDEVICE_TYPE_MCS_BOTH_AFDS "ANNUN_MCS_BOTH_AFDS"
#define CUSTOMDEVICE_TYPE_AIR   "ANNUN_AIR"
#define CUSTOMDEVICE_TYPE_FCTL  "ANNUN_FCTL"
#define CUSTOMDEVICE_TYPE_ISDU  "ANNUN_ISDU"

class MFCustomDevice
{
public:
    MFCustomDevice();
    void attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash = false);
    void detach();
    void update();
    void set(int16_t messageID, char *setPoint);

private:
    bool    getStringFromMem(uint16_t addreeprom, char *buffer, bool configFromFlash);
    void    start();
    bool    _initialized  = false; // configured
    bool    _started      = false; // the panel is up -- not until the boot splash lets it
    bool    _identifying  = false; // lit for the splash's identify phase
    int8_t  _panel        = -1;    // index into PANELS in MFCustomDevice.cpp
    uint8_t _backlightPin = 0;
    uint8_t _brightness   = 255;
};
