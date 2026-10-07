#include "MFCustomDevice.h"
#include "Board.h"
#include "commandmessenger.h"
#include "allocateMem.h"
#include "MFEEPROM.h"
#include "Panel.h"
#include "PanelGfx.h"
#include "LampPanel.h"
#include "DoorPanel.h"
#include "IrsPanel.h"
#include "ElecPanel.h"
#include "McsPanel.h"
#include "AirPanel.h"
#include "FctlPanel.h"
#include "IsduPanel.h"
#include "Splash.h"
#include <string.h>
#include <stdlib.h>
#include <esp_system.h>
#if defined(HAS_CONFIG_IN_FLASH)
#include "MFCustomDevicesConfig.h"
#else
const char CustomDeviceConfig[] PROGMEM = {};
#endif

extern MFEEPROM MFeeprom;

/* **********************************************************************************
    The custom device pins, type and configuration are stored in the EEPROM. While the
    config is read the addresses are handed to attach(), which copies each '.'-terminated
    string into a buffer one at a time.

    Longest string we expect here is the type, "ANNUN_MCS_BOTH" (15 with NUL). The pin field
    holds a single pin number and the config field a brightness value, so 40 bytes is
    ample.
********************************************************************************** */
#define MEMLEN_STRING_BUFFER 40

// reads a string from EEPROM or Flash at given address which is '.' terminated and saves it to the buffer
bool MFCustomDevice::getStringFromMem(uint16_t addrMem, char *buffer, bool configFromFlash)
{
    char     temp    = 0;
    uint8_t  counter = 0;
    uint16_t length  = MFeeprom.get_length();
    do {
        if (configFromFlash) {
            temp = pgm_read_byte_near(CustomDeviceConfig + addrMem++);
            if (addrMem > sizeof(CustomDeviceConfig))
                return false;
        } else {
            temp = MFeeprom.read_byte(addrMem++);
            if (addrMem > length)
                return false;
        }
        buffer[counter++] = temp;              // save character and locate next buffer position
        if (counter >= MEMLEN_STRING_BUFFER) { // nameBuffer will be exceeded
            return false;                      // abort copying to buffer
        }
    } while (temp != '.'); // reads until limiter '.' and locates the next free buffer position
    buffer[counter - 1] = 0x00; // replace '.' by NULL, terminates the string
    return true;
}

MFCustomDevice::MFCustomDevice()
{
    _initialized = false;
    _started     = false;
    _identifying = false;
}

/* **********************************************************************************
    Why the board last started, once per boot, as a debug line (the Connector logs it; so
    does tools/mfsim.py listen). A dark panel is usually by design -- MobiFlight stopped,
    power saving, the bus off -- and this tells a crash or brown-out from those.
********************************************************************************** */
static void reportPinMismatch()
{
    char msg[80];
    snprintf(msg, sizeof(msg), "Annunciator: the backlight is fixed on GPIO %u - set the custom device pin to %u",
             (unsigned)Board::BACKLIGHT_PIN, (unsigned)Board::BACKLIGHT_PIN);
    cmdMessenger.sendCmd(kDebug, msg);
}

static void reportResetReason(bool pinMismatch)
{
    static bool reported = false;
    if (reported) {
        if (pinMismatch) reportPinMismatch();
        return;
    }
    reported = true;

    const char *why;
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   why = "power on"; break;
    case ESP_RST_EXT:       why = "reset pin"; break;
    case ESP_RST_SW:        why = "software reset"; break;
    case ESP_RST_PANIC:     why = "CRASH (panic)"; break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       why = "CRASH (watchdog)"; break;
    case ESP_RST_BROWNOUT:  why = "BROWN-OUT (supply dipped)"; break;
    case ESP_RST_DEEPSLEEP: why = "deep sleep"; break;
    default:                why = "unknown"; break;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "Annunciator started: %s", why);
    // The ROM's boot text has no ';', so a message sent now would be glued onto it and
    // dropped as garbage; an empty message first closes it off.
    Serial.print(F("\r\n;"));
    cmdMessenger.sendCmd(kDebug, buf);
    // (Debug lines reach the Connector's log only at Log Level Debug -- see the handbook,
    // "Seeing the board's messages".)
    // What the display backend found when the boot splash brought the screen up, which was
    // before the serial port was open.
    if (const char *report = Gfx::deviceReport())
        cmdMessenger.sendCmd(kDebug, report);
    if (pinMismatch) reportPinMismatch();
}

/* **********************************************************************************
    Every panel this firmware can be, by the custom device type the Connector configured.
    The panels built from a lamp table share the LampPanel engine for everything but init.
********************************************************************************** */
struct PanelOps {
    const char *type;
    void (*init)(uint8_t backlightPin);
    void (*stop)();
    void (*clear)();
    void (*set)(int16_t messageID, char *payload);
    void (*update)();
};

static const PanelOps PANELS[] = {
    {CUSTOMDEVICE_TYPE_DOOR, DoorPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_IRS, IrsPanel::init, IrsPanel::stop, IrsPanel::clear, IrsPanel::set, IrsPanel::update},
    {CUSTOMDEVICE_TYPE_ELEC, ElecPanel::init, ElecPanel::stop, ElecPanel::clear, ElecPanel::set, ElecPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_L, McsPanel::initLeft, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_R, McsPanel::initRight, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_BOTH, McsPanel::initBoth, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_MCS_BOTH_AFDS, McsPanel::initBothAfds, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_AIR, AirPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_FCTL, FctlPanel::init, LampPanel::stop, LampPanel::clear, LampPanel::set, LampPanel::update},
    {CUSTOMDEVICE_TYPE_ISDU, IsduPanel::init, IsduPanel::stop, IsduPanel::clear, IsduPanel::set, IsduPanel::update},
};
static const int8_t PANEL_COUNT = (int8_t)(sizeof(PANELS) / sizeof(PANELS[0]));

/* **********************************************************************************
    The Connector stores a custom device as

        17.<Type>.<Pin|Pin|...>.<Config>.<Name>:

    so the three addresses handed in here point at the Type, Pin and Config fields.
    We use the type to pick which annunciator panel to render, the single pin as the
    display backlight, and the config string as the initial backlight brightness.
********************************************************************************** */
void MFCustomDevice::attach(uint16_t adrPin, uint16_t adrType, uint16_t adrConfig, bool configFromFlash)
{
    if (adrPin == 0) return;

    char  parameter[MEMLEN_STRING_BUFFER];
    char *params, *p = NULL;

    /* Which panel? The type string comes straight from Info.Type in the device JSON. */
    if (!getStringFromMem(adrType, parameter, configFromFlash)) return;
    _panel = -1;
    for (int8_t i = 0; i < PANEL_COUNT; ++i)
        if (strcmp(parameter, PANELS[i].type) == 0) _panel = i;

    if (_panel < 0) {
        cmdMessenger.sendCmd(kStatus, F("Custom Device is not supported by this firmware version"));
        return;
    }

    /* One pin is configured, the display backlight. The display's own pins are compile
       time settings (Board.h), not Connector settings. The backlight is a fixed trace on
       every board -- GPIO 27 on the E32R32P, GPIO 1 on the C3248W535 -- so any other pin
       is a misconfiguration: say adding a button first took it, and the custom device was
       offered the next free pin. Using that pin would leave the panel dark for good, so
       use the board's pin regardless. */
    if (!getStringFromMem(adrPin, parameter, configFromFlash)) return;
    params = strtok_r(parameter, "|", &p);
    const uint8_t configured = params ? (uint8_t)atoi(params) : 0;
    _backlightPin = Board::BACKLIGHT_PIN;
    const bool pinMismatch = configured != 0 && configured != _backlightPin; // reported below

    /* Optional free text config: initial backlight brightness, 16..255 (lower is raised to
       16, as for message 101; empty or 0 means 255). */
    uint8_t brightness = 255;
    if (getStringFromMem(adrConfig, parameter, configFromFlash)) {
        const int value = atoi(parameter);
        if (value > 0) brightness = (uint8_t)constrain(value, 16, 255);
    }

    _brightness  = brightness;
    _initialized = true;

    // The boot splash has the screen for its first ten seconds; the panel comes up when it
    // ends -- see update(). Configured later than that, it comes up now.
    Splash::claim();
    if (Splash::phase() != Splash::SHOWING_SPLASH) start();

    reportResetReason(pinMismatch);
}

/* **********************************************************************************
    Brings the panel up. Dark, as always until MobiFlight sends values -- except during the
    splash's identify phase, when it is lit to show what this board is configured as.
********************************************************************************** */
void MFCustomDevice::start()
{
    PANELS[_panel].init(_backlightPin);

    // Only the level. Set directly rather than as a message, since any message counts as
    // MobiFlight running.
    PanelGfx::setBacklight(_brightness);

    _identifying = Splash::phase() == Splash::IDENTIFYING;
    if (_identifying) {
        // Lit only once the panel is on the glass. On the 3.5in board the screen keeps
        // showing the last frame sent -- the splash -- until present(), and lamps are only
        // drawn by update().
        PANELS[_panel].update();
        PanelGfx::present();
    }
    PanelGfx::setIdentify(_identifying);
    _started = true;
}

/* **********************************************************************************
    Called when a new config gets uploaded. Keep it as it is.
********************************************************************************** */
void MFCustomDevice::detach()
{
    if (!_initialized) return;
    _initialized = false;
    // A panel that never started still holds the boot splash's claim on the screen; let it
    // go, or a board whose panel was removed during the splash would keep it lit for good.
    if (_started) PANELS[_panel].stop();
    else Splash::release();
    _started = false;
    if (_identifying) {
        _identifying = false;
        PanelGfx::setIdentify(false);
    }

    // A newly configured panel has no lamp states yet -- showing it lit with every lamp off
    // could contradict the aircraft. Stay dark until MobiFlight sends real values. The bus
    // state is kept: an upload can happen during Run, and the Connector resends only values
    // that change until the next Stop -- which is where the bus state is reset.
    PanelGfx::setRunning(false);
}

/* **********************************************************************************
    Called regularly from loop(). Drives the 1 Hz blink and the self test, polls the
    touch screen on panels that have buttons, and flushes any lamps whose state changed
    since the last pass.
********************************************************************************** */
void MFCustomDevice::update()
{
    if (!_initialized) return;
    if (!_started) {
        if (Splash::phase() == Splash::SHOWING_SPLASH) return; // the splash has the screen
        start();
    }
    if (_identifying && Splash::phase() != Splash::IDENTIFYING) {
        _identifying = false; // ten seconds up: dark until MobiFlight runs
        PanelGfx::setIdentify(false);
    }
    PanelGfx::tick();
    PANELS[_panel].update();
    PanelGfx::present(); // sends the frame on a board that buffers one
}

/* **********************************************************************************
    Called when the Connector has a new value for one of the device's message types.
    Whether the panel is lit at all is the same question for every panel, so those
    messages are handled here; everything else goes to the panel.
********************************************************************************** */
void MFCustomDevice::set(int16_t messageID, char *setPoint)
{
    if (!_initialized) return;

    // MobiFlight is driving the panel, so whatever is left of the boot splash gives way to
    // it now. Merely connecting never gets here: the Connector reads the board on connect
    // without sending the panel anything.
    if (!_started || _identifying) {
        Splash::end();
        if (!_started) start();
        _identifying = false;
        PanelGfx::setIdentify(false);
    }

    switch (messageID) {
    case MSG_STOP: // MobiFlight stopped or shutting down; sent with no value at all
        PanelGfx::setRunning(false);
        // Forget what the panel showed. Stop clears the Connector's record of what it sent,
        // so the next Run sends everything again -- but a test sends only its own row, and
        // must not find the last flight's lamps, dimming or battery-off still in force.
        PanelGfx::setBusPowered(true);
        PANELS[_panel].clear();
        return;

    case MSG_POWERSAVE: // "1" enter, "0" leave
        PanelGfx::setPowerSave(setPoint && atoi(setPoint) != 0);
        return;

    default:
        break;
    }

    // Any other message means MobiFlight is running and sending values. Merely being
    // connected is not enough: the Connector reads the board on connect without sending
    // anything, so the panel stays dark until Run.
    PanelGfx::setRunning(true);

    switch (messageID) {
    case MSG_BUS_POWER:
        // 0/1, or a bus voltage bound directly: from 0.5 up counts as powered.
        PanelGfx::setBusPowered(setPoint && atof(setPoint) >= 0.5);
        return;

    case MSG_BUS_UNPOWERED:
        // The profiles' form (see Panel.h): "1" is off, and anything else -- including the
        // "0" that Test Mode and the end of every test send -- is powered.
        PanelGfx::setBusPowered(!(setPoint && atof(setPoint) >= 0.5));
        return;

    case MSG_BRIGHTNESS:
        // Never fully off: turning the panel off is bus power's job, and a brightness
        // binding that reads 0 -- a knob at its stop, the "0" ending a test -- would otherwise
        // look exactly like a dead board.
        PanelGfx::setBacklight(setPoint ? (uint8_t)constrain(atoi(setPoint), 16, 255) : 255);
        return;

    default:
        break;
    }

    PANELS[_panel].set(messageID, setPoint);
}
