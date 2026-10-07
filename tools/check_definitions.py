#!/usr/bin/env python3
"""Cross-check the firmware build flags against the MobiFlight Connector JSON.

MobiFlight matches a board by comparing the MOBIFLIGHT_TYPE string the firmware reports
to Info.MobiFlightType in the board JSON, and picks a custom device by strcmp'ing the
type string stored in the config against Info.Type in the device JSON. Nothing validates
those against each other, so a one-character drift silently produces a board that is
discovered but never usable. This script is that missing check.

Required-field lists mirror Boards/mfboard.schema.json and Devices/mfdevice.schema.json
as shipped with Connector 10.5+.

    python3 tools/check_definitions.py
"""

import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEV_INI = ROOT / "Annunciator" / "Annunciator_platformio.ini"
ROOT_INI = ROOT / "platformio.ini"
BOARDS = ROOT / "Annunciator" / "Community" / "boards"
DEVICES = ROOT / "Annunciator" / "Community" / "devices"
MFCD_H = ROOT / "Annunciator" / "MFCustomDevice.h"

BOARD_REQUIRED = ["Connection", "HardwareIds", "Info", "ModuleLimits", "Pins"]
BOARD_INFO_REQUIRED = [
    "CanInstallFirmware", "CanResetBoard", "FirmwareBaseName", "FirmwareExtension",
    "FriendlyName", "LatestFirmwareVersion", "MobiFlightType",
]
BOARD_CONNECTION_REQUIRED = [
    "ConnectionDelay", "DelayAfterFirmwareUpdate", "DtrEnable", "EEPROMSize",
    "ExtraConnectionRetry", "ForceResetOnFirmwareUpdate", "MessageSize",
]
BOARD_LIMITS_REQUIRED = [
    "MaxAnalogInputs", "MaxButtons", "MaxEncoders", "MaxInputShifters", "MaxLcdI2C",
    "MaxLedSegments", "MaxOutputs", "MaxServos", "MaxSteppers", "MaxInputMultiplexer",
]
DEVICE_REQUIRED = ["Info", "Config", "MessageTypes"]
DEVICE_INFO_REQUIRED = ["Label", "Type", "Author", "URL", "Version"]

# MFCustomDevice.cpp reads each '.'-terminated field into a buffer of this size.
MEMLEN_STRING_BUFFER = 40

# A device's Info.Label is the name the Connector gives it when it is added -- and the name
# the generated profiles address it by -- so it must be a name the Connector keeps: at most 16
# characters (the settings dialog truncates longer ones) and none of its reserved characters
# (',' in a stored name even cuts the board's config short).
NAME_MAX = 16
NAME_RESERVED = ":.;,#/|"

DIST = ROOT / "_dist"

# GPIOs the E32R32P board commits to on-board hardware. Offering any of these to the
# Connector lets a user wire a button or LED onto a pin that is already a display trace,
# an SD line or the UART the Connector itself talks over -- which fails confusingly at
# runtime rather than at config time. GPIO 27 (backlight) is deliberately NOT here: the
# custom device needs to be pointed at it.
# Source: LCDWIKI E32R32P specification, ESP32 pin allocation.
RESERVED_GPIO = {
    # LCDWIKI E32R32P. Source: the board's specification and its ESP32 pin allocation.
    "annunciator_e32r32p": {
        0: "BOOT button / strapping", 1: "UART0 TX (CH340C)", 2: "LCD DC",
        3: "UART0 RX (CH340C)", 4: "audio amp enable", 5: "microSD CS / strapping",
        12: "LCD MISO / MTDI strapping", 13: "LCD MOSI", 14: "LCD SCLK", 15: "LCD CS",
        16: "RGB LED green", 17: "RGB LED blue", 18: "microSD SCK", 19: "microSD MISO",
        22: "RGB LED red", 23: "microSD MOSI", 26: "audio DAC", 33: "touch CS",
        34: "battery sense", 36: "touch IRQ",
    },
    # Caturda C3248W535 / Guition JC3248W535EN. Sources: the community pin maps that agree
    # with each other (byte-me404, AudunKodehode, ESPHome's axs15231 model) and the
    # ESP32-S3-WROOM-1 N16R8 datasheet for the octal PSRAM lines, which are the ones that
    # fail most confusingly if they are ever handed out.
    "annunciator_c3248w535": {
        0: "BOOT button / strapping", 2: "I2S LRCK", 3: "touch INT / strapping",
        4: "touch SDA", 5: "battery sense", 8: "touch SCL",
        10: "microSD CS", 11: "microSD MOSI", 12: "microSD SCK", 13: "microSD MISO",
        19: "USB D-", 20: "USB D+", 21: "LCD QSPI D0",
        35: "octal PSRAM", 36: "octal PSRAM", 37: "octal PSRAM",
        38: "LCD tearing effect", 39: "LCD QSPI D3", 40: "LCD QSPI D2",
        41: "I2S DIN", 42: "I2S BCK", 43: "UART0 TX", 44: "UART0 RX",
        45: "LCD QSPI CS / strapping", 46: "strapping, input only",
        47: "LCD QSPI SCK", 48: "LCD QSPI D1",
    },
}

# GPIOs that can be read but never driven, per chip family. They must not be offered as PWM.
INPUT_ONLY = {
    "annunciator_e32r32p": {34, 35, 36, 37, 38, 39},
    "annunciator_c3248w535": {46},
}

errors, warnings = [], []


def err(msg):
    errors.append(msg)


def warn(msg):
    warnings.append(msg)


def flag(text, name):
    m = re.search(r"-D%s=(\d+)" % name, text)
    return int(m.group(1)) if m else None


def parse_envs(ini_text):
    """Split a PlatformIO ini into its [env:NAME] sections.

    configparser cannot be used here: PlatformIO's ${...} interpolation is not its syntax.
    Each board has its own env, and a flag read from the wrong one is exactly the sort of
    drift this script exists to catch, so nothing may be read from the file as a whole.
    """
    envs, name, lines = {}, None, []
    for line in ini_text.splitlines():
        header = re.match(r"\[env:([^\]]+)\]", line.strip())
        if header or line.strip().startswith("["):
            if name:
                envs[name] = "\n".join(lines)
            name, lines = (header.group(1) if header else None), []
            continue
        if name:
            lines.append(line)
    if name:
        envs[name] = "\n".join(lines)
    return envs


def main():
    dev_ini = DEV_INI.read_text()
    root_ini = ROOT_INI.read_text()

    envs = parse_envs(dev_ini)
    if not envs:
        err("no [env:...] sections found in %s" % DEV_INI.name)
        return report()

    msg_buffer = flag(root_ini, "MESSENGERBUFFERSIZE")
    # Envs that declare a MobiFlight type are the firmware ones; anything else in the file
    # -- the hardware bring-up probe, say -- is not a board and is not checked here. An env
    # a board JSON actually names is checked below, and fails there if it has no type.
    fw_types_by_env = {}
    for name, text in envs.items():
        m = re.search(r'-DMOBIFLIGHT_TYPE="([^"]+)"', text)
        if m:
            fw_types_by_env[name] = m.group(1)

    # Type strings the firmware will strcmp against.
    fw_types = set(re.findall(r'#define\s+CUSTOMDEVICE_TYPE_\w+\s+"([^"]+)"', MFCD_H.read_text()))

    board_files = sorted(BOARDS.glob("*.board.json"))
    device_files = sorted(DEVICES.glob("*.device.json"))
    if not board_files:
        err("no *.board.json found")
    if not device_files:
        err("no *.device.json found")

    declared_types = set()
    for path in device_files:
        d = json.loads(path.read_text())
        for key in DEVICE_REQUIRED:
            if key not in d:
                err("%s: missing required '%s'" % (path.name, key))
        info = d.get("Info", {})
        for key in DEVICE_INFO_REQUIRED:
            if key not in info:
                err("%s: Info missing required '%s'" % (path.name, key))

        dtype = info.get("Type", "")
        declared_types.add(dtype)

        label = info.get("Label", "")
        if len(label) > NAME_MAX or any(c in label for c in NAME_RESERVED) or label != label.strip():
            err("%s: Info.Label %r is not a name the Connector keeps (at most %d characters, "
                "none of %s) -- a device added in the Connector would not match the profiles"
                % (path.name, label, NAME_MAX, " ".join(NAME_RESERVED)))

        # The config record is "17.<Type>.<Pins>.<Config>.<Name>:" -- a '.' inside any
        # field would terminate it early, and the reader stops at MEMLEN_STRING_BUFFER.
        if "." in dtype:
            err("%s: Info.Type %r contains '.', which terminates the config field" % (path.name, dtype))
        if len(dtype) + 1 > MEMLEN_STRING_BUFFER:
            err("%s: Info.Type %r exceeds MEMLEN_STRING_BUFFER (%d)" % (path.name, dtype, MEMLEN_STRING_BUFFER))
        if dtype not in fw_types:
            err("%s: Info.Type %r has no matching CUSTOMDEVICE_TYPE_* define in MFCustomDevice.h"
                % (path.name, dtype))

        cfg = d.get("Config", {})
        if "Pins" not in cfg:
            err("%s: Config missing required 'Pins'" % path.name)
        if not cfg.get("Pins"):
            warn("%s: Config.Pins is empty; MobiFlight expects at least one pin per custom device"
                 % path.name)
        if "isI2C" in cfg:
            warn("%s: Config.isI2C is not in the schema and is silently dropped; use Config.I2C.Enabled"
                 % path.name)

        ids = []
        for mt in d.get("MessageTypes", []):
            mid = mt.get("Id", mt.get("id"))
            if mid is None:
                err("%s: a MessageType has no Id" % path.name)
                continue
            if mid < 0:
                err("%s: MessageType Id %d is negative; -1 and -2 are reserved by MobiFlight"
                    % (path.name, mid))
            ids.append(mid)
        dupes = {i for i in ids if ids.count(i) > 1}
        if dupes:
            err("%s: duplicate MessageType Ids %s" % (path.name, sorted(dupes)))

    for path in board_files:
        b = json.loads(path.read_text())
        for key in BOARD_REQUIRED:
            if key not in b:
                err("%s: missing required '%s'" % (path.name, key))
        info = b.get("Info", {})
        for key in BOARD_INFO_REQUIRED:
            if key not in info:
                err("%s: Info missing required '%s'" % (path.name, key))
        conn = b.get("Connection", {})
        for key in BOARD_CONNECTION_REQUIRED:
            if key not in conn:
                err("%s: Connection missing required '%s'" % (path.name, key))
        limits = b.get("ModuleLimits", {})
        for key in BOARD_LIMITS_REQUIRED:
            if key not in limits:
                err("%s: ModuleLimits missing required '%s'" % (path.name, key))

        # A board is tied to the env that builds its firmware: the Connector looks for
        # <FirmwareBaseName>_<version>.bin, and get_version.py names that after the env.
        env_name = info.get("FirmwareBaseName")
        env_text = envs.get(env_name)
        if env_text is None:
            err("%s: Info.FirmwareBaseName %r matches no [env:...] in %s"
                % (path.name, env_name, DEV_INI.name))
            continue
        fw_type = fw_types_by_env.get(env_name)
        memlen_config = flag(env_text, "MEMLEN_CONFIG")
        # Compile-time display and touch pins: -D flags on the board whose library takes
        # them that way, and Board.h on the board whose library takes them as arguments.
        wired_pins = {int(v) for v in re.findall(
            r"-D(?:TFT_(?:MOSI|SCLK|MISO|CS|DC|RST|BL)|TOUCH_CS)=(-?\d+)", env_text)}
        wired_pins = {p for p in wired_pins if p >= 0}

        if info.get("MobiFlightType") != fw_type:
            err("%s: Info.MobiFlightType %r != %s's MOBIFLIGHT_TYPE %r -- the board "
                "will be discovered by VID/PID but never identified"
                % (path.name, info.get("MobiFlightType"), env_name, fw_type))

        if limits.get("MaxCustomDevices", 0) < 1:
            err("%s: ModuleLimits.MaxCustomDevices is %s; it defaults to 0 and no custom "
                "device can be added" % (path.name, limits.get("MaxCustomDevices")))

        if memlen_config and conn.get("EEPROMSize") != memlen_config:
            err("%s: Connection.EEPROMSize %s != -DMEMLEN_CONFIG %s"
                % (path.name, conn.get("EEPROMSize"), memlen_config))
        if msg_buffer and conn.get("MessageSize", 0) > msg_buffer:
            err("%s: Connection.MessageSize %s exceeds -DMESSENGERBUFFERSIZE %s"
                % (path.name, conn.get("MessageSize"), msg_buffer))

        listed = set(info.get("CustomDeviceTypes", []))
        if listed != declared_types:
            err("%s: Info.CustomDeviceTypes %s != device Info.Type values %s"
                % (path.name, sorted(listed), sorted(declared_types)))

        offered = {p["Pin"] for p in b.get("Pins", [])}
        clash = offered & wired_pins
        if clash:
            err("%s: Pins offers GPIO %s which the display wiring already uses"
                % (path.name, sorted(clash)))

        reserved = RESERVED_GPIO.get(env_name)
        if reserved is None:
            warn("%s: no reserved-GPIO table for %s; its Pins list is unchecked"
                 % (path.name, env_name))
            reserved = {}
        for gpio in sorted(offered & set(reserved)):
            err("%s: Pins offers GPIO %d, which this board uses for %s"
                % (path.name, gpio, reserved[gpio]))

        # Input-only pins cannot drive an output, so they must not be advertised as
        # PWM-capable.
        for p in b.get("Pins", []):
            if p["Pin"] in INPUT_ONLY.get(env_name, set()) and p.get("isPWM"):
                err("%s: GPIO %d is input-only but is marked isPWM"
                    % (path.name, p["Pin"]))

    check_dist(board_files + device_files)
    return report(fw_types_by_env, declared_types)


def check_dist(sources):
    """The install zip must carry the definitions as they are now: an out-of-date zip gives
    the Connector a board without the newer panels, and they cannot be added."""
    zips = sorted(DIST.glob("*.zip")) if DIST.exists() else []
    if not zips:
        warn("no install zip in _dist yet -- run a build (pio run) to make one")
        return
    refresh = "run `pio run -t annunciator_package` to refresh it"
    for zpath in zips:
        # The build names the zip <name>_<version>.zip and stamps that version over the board
        # JSON's placeholder 0.0.1 (copy_fw_files.py); expect exactly that.
        ver = zpath.stem.rpartition("_")[2]
        if not re.fullmatch(r"\d+(\.\d+){1,3}", ver):
            err("%s: version %r is not a version the Connector can parse (2-4 dotted numbers) "
                "-- set VERSION to one before building" % (zpath.name, ver))
            continue
        with zipfile.ZipFile(zpath) as z:
            names = {Path(n).name: n for n in z.namelist() if n.endswith(".json")}
            for src in sources:
                if src.name not in names:
                    err("%s lacks %s -- %s" % (zpath.name, src.name, refresh))
                    continue
                packed = z.read(names[src.name]).decode("utf-8").replace("\r\n", "\n")
                want = src.read_text().replace("\r\n", "\n")
                if src.parent == BOARDS:
                    want = want.replace("0.0.1", ver)
                if packed != want:
                    err("%s has an out-of-date %s -- %s" % (zpath.name, src.name, refresh))
            for extra in sorted(set(names) - {s.name for s in sources}):
                err("%s carries %s, which is no longer in Annunciator/Community -- %s"
                    % (zpath.name, extra, refresh))
            # One zip serves every board, so it must carry every board's firmware; and each
            # board needs a full image beside it for a first flash. A build of only one env
            # packages without complaint, so this is where a half-built release is caught.
            packed_names = {Path(n).name for n in z.namelist()}
            for src in sources:
                if src.parent != BOARDS:
                    continue
                info = json.loads(src.read_text())["Info"]
                stem = "%s_%s" % (info["FirmwareBaseName"], ver.replace(".", "_"))
                app = "%s.%s" % (stem, info["FirmwareExtension"])
                if app not in packed_names:
                    err("%s lacks firmware %s -- build every env (pio run) with the same VERSION"
                        % (zpath.name, app))
                if not (DIST / (stem + "_full.bin")).exists():
                    err("_dist lacks %s_full.bin, the image a new board is flashed with "
                        "-- build every env (pio run) with the same VERSION" % stem)


def report(fw_types=None, declared_types=None):
    for w in warnings:
        print("WARN  %s" % w)
    for e in errors:
        print("FAIL  %s" % e)
    if errors:
        print("\n%d problem(s) found." % len(errors))
        return 1
    for env_name, fw_type in sorted((fw_types or {}).items()):
        print("OK    %-24s board type %r" % (env_name, fw_type))
    print("OK    custom devices %s" % sorted(declared_types or []))
    print("OK    firmware flags, board JSON and device JSON agree.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
