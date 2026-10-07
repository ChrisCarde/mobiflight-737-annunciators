#!/usr/bin/env python3
"""A minimal stand-in for MobiFlight Connector, for bench-testing the annunciator.

The Connector is Windows-only. This speaks the same serial protocol, so a board can be
identified, given a config and driven from any machine with a USB cable -- which also means
the firmware's MobiFlight path can be exercised end to end without the sim PC.

    python tools/mfsim.py info                  # identify the board, check it matches the JSON
    python tools/mfsim.py setup door            # upload a config: door, irs, elec, air, fctl, isdu, mcs_l, mcs_r, mcs_both
    python tools/mfsim.py lamp 3 1              # lamp 3 on (0 off, 1 on, 2 blink)
    python tools/mfsim.py send 102 1            # any message ID -- here, night dimming
    python tools/mfsim.py demo door             # walk through a few states
    python tools/mfsim.py selftest              # cycle the design's demo states
    python tools/mfsim.py send 105 0            # bus unpowered -> panel dark (1 = lit)
    python tools/mfsim.py stop                  # what the Connector sends on Stop
    python tools/mfsim.py powersave on          # MobiFlight power saving on (dark) / off
    python tools/mfsim.py listen                # print the buttons pressed on the touch screen
    python tools/mfsim.py touchcal              # run the touch calibration, show the result

A freshly flashed board has an empty config, so it has no custom device -- run `setup` once.
The config is kept in flash across reboots, exactly as if the Connector had uploaded it.

The panel stays dark until MobiFlight sends it a value, just as it would with the real
Connector before Run, so `setup` alone shows nothing: follow it with `selftest` or `demo`.

Power saving works as with the Connector. The core firmware darkens the panel 15 minutes
after the last "power saving off" command -- lamp values do not count as activity -- so,
like the Connector at Run, every command that drives the panel sends one first, and
`listen` / `touchcal` repeat it every 5 minutes as the Connector does while running. A
board left alone after a command still goes dark 15 minutes later; any command wakes it.

Needs pyserial. PlatformIO's own interpreter already has it:
    ~/.platformio/penv/bin/python tools/mfsim.py info
"""

import argparse
import glob
import json
import re
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing -- run this with ~/.platformio/penv/bin/python, "
             "or pip install pyserial")

ROOT = Path(__file__).resolve().parent.parent

# Command IDs, from the core firmware's commandmessenger.h.
K_STATUS, K_BUTTON_CHANGE, K_GET_INFO, K_INFO = 5, 7, 9, 10
K_SET_CONFIG, K_GET_CONFIG, K_RESET_CONFIG = 11, 12, 13
K_SAVE_CONFIG, K_CONFIG_SAVED = 14, 15
K_ACTIVATE_CONFIG, K_CONFIG_ACTIVATED = 16, 17
K_SET_POWER_SAVING = 18
K_SET_CUSTOM_DEVICE = 32
K_DEBUG = 255

# CmdMessenger framing as the firmware builds it: CmdMessenger(Serial) with its defaults.
FIELD, END, ESCAPE = ",", ";", "/"

# panel -> device definition. The device's type and name come from it: the name is the
# definition's label, which is also what the Connector names a device it adds, and what the
# generated profiles (tools/make_profiles.py) address it by.
PANEL_FILES = {
    "door": "annunciator_door", "irs": "annunciator_irs", "elec": "annunciator_elec",
    "air": "annunciator_air", "fctl": "annunciator_fctl", "isdu": "annunciator_isdu",
    "mcs_l": "annunciator_mcs_l", "mcs_r": "annunciator_mcs_r", "mcs_both": "annunciator_mcs_both",
    "mcs_both_afds": "annunciator_mcs_both_afds",
}


def _panel(key):
    info = json.loads((ROOT / "Annunciator" / "Community" / "devices" /
                       (PANEL_FILES[key] + ".device.json")).read_text())["Info"]
    return info["Type"], info["Label"]


PANELS = {key: _panel(key) for key in PANEL_FILES}


def _boards():
    """Every board definition, keyed by the MobiFlightType its firmware reports.

    The backlight pin differs per board and the config record has to carry the right one,
    so it is read from the definition rather than hard-coded: whichever pin the board JSON
    offers whose name mentions the backlight.
    """
    out = {}
    for path in sorted((ROOT / "Annunciator" / "Community" / "boards").glob("*.board.json")):
        board = json.loads(path.read_text())
        info = board["Info"]
        backlight = next((p["Pin"] for p in board.get("Pins", [])
                          if "backlight" in p.get("Name", "").lower()), None)
        out[info["MobiFlightType"]] = {
            "file": path.name,
            "type": info["MobiFlightType"],
            "backlight": backlight,
            "friendly": info.get("FriendlyName", info["MobiFlightType"]),
        }
    return out


BOARDS = _boards()


def escape(value):
    out = []
    for ch in str(value):
        if ch in (FIELD, END, ESCAPE):
            out.append(ESCAPE)
        out.append(ch)
    return "".join(out)


class Board:
    def __init__(self, port, verbose=False):
        self.verbose = verbose
        self.buf = ""
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = 115200
        self.ser.timeout = 0.05
        # The CH340C's download circuit drives EN and IO0 from DTR/RTS. Measured on this
        # board: opening with both off reboots it, while DTR on / RTS off leaves it running,
        # so each command acts on the live panel instead of a freshly rebooted, dark one.
        # (The Connector on Windows also ends with DTR on, but .NET gets there by way of
        # other states, so this is not proof that its open is harmless -- see docs/handbook.md.)
        self.ser.dtr = True
        self.ser.rts = False
        self.ser.open()
        self.awake_at = 0.0
        self.boot_text = []  # text the board printed outside a message: ROM boot, panics

    def close(self):
        self.ser.close()

    def send(self, cmd, *args):
        line = FIELD.join([str(cmd)] + [escape(a) for a in args]) + END
        if self.verbose:
            print("  >> %s" % line)
        self.ser.write(line.encode("ascii", "replace"))
        self.ser.flush()

    def _messages(self):
        """Yield complete messages as [cmd, arg, ...], honouring the escape character."""
        chunk = self.ser.read(256)
        if chunk:
            self.buf += chunk.decode("ascii", "replace")
        while True:
            # Find the unescaped END first.
            i, end = 0, -1
            while i < len(self.buf):
                if self.buf[i] == ESCAPE:
                    i += 2
                    continue
                if self.buf[i] == END:
                    end = i
                    break
                i += 1
            if end < 0:
                return
            raw, self.buf = self.buf[:end], self.buf[end + 1:]
            # A message never spans a line break (the core ends each with ";\r\n"), so
            # everything up to the last unescaped line break is text the board printed
            # outside a message: ROM boot chatter after a reset (rst:0xc is a crash or
            # software reset, 0xf a brown-out), a panic dump. Keep it verbatim.
            cut, i = -1, 0
            while i < len(raw):
                if raw[i] == ESCAPE:
                    i += 2
                    continue
                if raw[i] in "\r\n":
                    cut = i
                i += 1
            chatter = [l.strip() for l in raw[:cut + 1].replace("\r", "\n").split("\n")]
            self.boot_text.extend(l for l in chatter if l)
            fields, cur, i, body = [], [], 0, raw[cut + 1:]
            while i < len(body):
                ch = body[i]
                if ch == ESCAPE and i + 1 < len(body):
                    cur.append(body[i + 1])
                    i += 2
                    continue
                if ch == FIELD:
                    fields.append("".join(cur))
                    cur = []
                else:
                    cur.append(ch)
                i += 1
            fields.append("".join(cur))
            head = fields[0].strip()
            if head.isdigit():
                yield [int(head)] + fields[1:]

    def expect(self, cmd, timeout=2.5):
        """Wait for a reply with this command ID. kDebug lines are interleaved with real
        replies on this core branch, so anything else is skipped rather than returned."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            for msg in self._messages():
                if self.verbose or msg[0] not in (cmd, K_DEBUG):
                    print("  << %s" % ",".join(str(x) for x in msg))
                if msg[0] == cmd:
                    return msg[1:]
        return None

    def keep_awake(self):
        """What the Connector sends at Run and every 5 minutes while running: power saving
        off. The core firmware counts only this as activity -- lamp values do not -- and
        darkens every custom device 15 minutes after the last one."""
        self.send(K_SET_POWER_SAVING, 0)
        self.awake_at = time.time()

    def wait_ready(self, attempts=4):
        # Normally the board is not rebooted by opening the port (see __init__), but give it
        # a moment and discard anything already buffered, including ROM chatter if it was.
        time.sleep(0.5)
        self.ser.reset_input_buffer()
        self.buf = ""
        for _ in range(attempts):
            self.send(K_GET_INFO)
            info = self.expect(K_INFO)
            if info:
                return info
        return None


def known_usb_ids():
    """The USB ids the Annunciator boards can have, read from their definitions' HardwareIds.
    Those are the same regexes the Connector matches, so a port this accepts is one the
    Connector would try too."""
    ids = set()
    for path in (ROOT / "Annunciator" / "Community" / "boards").glob("*.board.json"):
        for pattern in json.loads(path.read_text()).get("HardwareIds", []):
            m = re.search(r"VID_([0-9A-Fa-f]{4})\W+PID_([0-9A-Fa-f]{4})", pattern)
            if m:
                ids.add((int(m.group(1), 16), int(m.group(2), 16)))
    return ids


def find_port():
    """The serial port of the one Annunciator board plugged in.

    Chosen by USB vendor and product id, not by device name. Names say only what kind of
    USB serial chip is there, and other serial devices are often plugged in too -- an FTDI
    cable shows up as /dev/cu.usbserial-* exactly as a CH340 board does, and picking it
    would send the board's commands to whatever that cable is connected to."""
    from serial.tools import list_ports

    wanted = known_usb_ids()
    ports = list(list_ports.comports())
    boards = [p for p in ports if p.vid is not None and (p.vid, p.pid) in wanted]

    if len(boards) == 1:
        return boards[0].device
    if not boards:
        seen = ", ".join("%s (%04X:%04X)" % (p.device, p.vid, p.pid) if p.vid is not None
                         else p.device for p in ports) or "none"
        sys.exit("no Annunciator board found -- looked for USB ids %s; serial ports present: %s. "
                 "Pass --port if the board is there."
                 % (", ".join("%04X:%04X" % i for i in sorted(wanted)), seen))
    listed = "\n".join("  %s  %04X:%04X  %s" % (p.device, p.vid, p.pid, p.description or "")
                        for p in boards)
    sys.exit("several Annunciator boards are plugged in -- choose one with --port:\n" + listed)


def board_for(reported_type):
    """The definition matching what the board says it is."""
    return BOARDS.get(reported_type)


# The firmware's MOBIFLIGHT_NAME per board type; the profiles address a controller by name.
BOARD_NAMES = {"Annunciator E32R32P": "Annunciator", "Annunciator C3248W535": "Annunciator 35"}


def cmd_info(board, _args):
    info = board.wait_ready()
    if not info:
        sys.exit("FAIL  no kInfo reply -- is this firmware on the board, and the port free?")
    labels = ["type", "name", "serial", "version", "core version"]
    for label, value in zip(labels, info):
        print("  %-13s %s" % (label, value))

    # The board's name: a Connector config upload stores it on the board, and reflashing does
    # not clear it, so a board once used with an older firmware can still carry its name.
    expected_name = BOARD_NAMES.get(info[0] if info else None)
    if len(info) > 1 and expected_name and info[1] != expected_name:
        print("WARN  the board is named %r; the profiles expect %r unless regenerated with "
              "--board PANEL=SERIAL:%s (rename it in the Connector's module settings and "
              "upload the config once)" % (info[1], expected_name, info[1]))

    # The device it carries, read back as the Connector does. Its name must be the device
    # definition's label -- what the profiles address it by.
    board.send(K_GET_CONFIG)
    config = board.expect(K_INFO)
    labels_by_type = {t: n for t, n in PANELS.values()}
    for entry in (config[0].split(":") if config and config[0] else []):
        parts = entry.split(".")
        if len(parts) >= 5 and parts[0] == "17":
            dtype, name = parts[1], parts[4]
            want = labels_by_type.get(dtype)
            print("  %-13s %s, named %r" % ("device", dtype, name))
            if want is None:
                print("WARN  %s is not a panel this firmware knows" % dtype)
            elif name != want:
                print("WARN  the device is named %r but the profiles address %r: nothing would "
                      "reach the panel. Re-run `setup`, or rename it in the Connector and upload, "
                      "or regenerate with --name" % (name, want))
    if not config or not config[0]:
        print("  %-13s none -- run setup" % "device")
    known = board_for(info[0])
    if known:
        print("OK    type matches %s (%s)" % (known["file"], known["friendly"]))
    else:
        sys.exit("FAIL  type %r matches no board definition in Annunciator/Community/boards "
                 "(%s) -- the Connector would discover this board but never identify it"
                 % (info[0], ", ".join(sorted(BOARDS)) or "none"))

    # The Connector feeds the core version into .NET's System.Version to decide which
    # features the board has (MobiFlightModule.HasFirmwareFeature) -- including whether a
    # custom device may be added at all. System.Version throws on anything but 2-4 dotted
    # integers, so check it here the same way.
    core = info[4] if len(info) > 4 else ""
    if re.fullmatch(r"\d+(\.\d+){1,3}", core):
        print("OK    core version %s parses as a System.Version" % core)
    else:
        sys.exit("FAIL  core version %r would make the Connector throw in System.Version -- "
                 "adding the custom device would fail" % core)


def cmd_setup(board, args):
    dtype, label = PANELS[args.panel]

    info = board.wait_ready()
    if not info:
        sys.exit("FAIL  board did not answer kGetInfo")

    # The pin in the config record is the backlight, and it differs per board -- GPIO 27 on
    # the E32R32P, GPIO 1 on the C3248W535. Take it from the definition for whatever board
    # actually answered, so a config uploaded here matches what the Connector would send.
    known = board_for(info[0])
    if not known or known["backlight"] is None:
        sys.exit("FAIL  no board definition with a backlight pin for type %r -- add one in "
                 "Annunciator/Community/boards" % (info[0] if info else None))
    config = "17.%s.%d.%d.%s:" % (dtype, known["backlight"], args.brightness, label)

    board.send(K_RESET_CONFIG)
    if board.expect(K_STATUS) != ["OK"]:
        sys.exit("FAIL  kResetConfig")

    # The Connector uploads in MessageSize-limited chunks and the firmware appends them;
    # reply is the cumulative length stored so far.
    sent = 0
    for i in range(0, len(config), 60):
        part = config[i:i + 60]
        board.send(K_SET_CONFIG, part)
        reply = board.expect(K_STATUS)
        sent += len(part)
        if not reply or reply[0] != str(sent):
            sys.exit("FAIL  kSetConfig: expected cumulative length %d, got %r" % (sent, reply))

    board.send(K_SAVE_CONFIG)
    if board.expect(K_CONFIG_SAVED) != ["OK"]:
        sys.exit("FAIL  kSaveConfig")

    board.send(K_ACTIVATE_CONFIG)
    if board.expect(K_CONFIG_ACTIVATED) != ["OK"]:
        sys.exit("FAIL  kActivateConfig")

    board.send(K_GET_CONFIG)
    echoed = board.expect(K_INFO)
    if not echoed or echoed[0] != config:
        sys.exit("FAIL  kGetConfig echoed %r, expected %r" % (echoed, config))

    print("OK    config stored and activated: %s" % config)
    print("      the panel stays dark until it receives a value -- try: selftest, or demo")


KEEP_AWAKE_S = 5 * 60  # the Connector's interval


def start(board):
    """Connect as the Connector does at Run: identify the board, then power saving off."""
    info = board.wait_ready()
    board.keep_awake()
    return info


def custom(board, message_id, payload=None):
    if payload is None:
        board.send(K_SET_CUSTOM_DEVICE, 0, message_id)  # Stop carries no value at all
    else:
        board.send(K_SET_CUSTOM_DEVICE, 0, message_id, payload)


def cmd_selftest(board, _args):
    start(board)
    custom(board, 104, 1)
    time.sleep(0.2)


def cmd_stop(board, _args):
    # The Connector's Stop: the custom device's Stop, then power saving on.
    board.wait_ready()
    custom(board, -1)
    board.send(K_SET_POWER_SAVING, 1)
    time.sleep(0.2)


def cmd_powersave(board, args):
    board.wait_ready()
    if args.state == "on":
        # What the Connector sends. The core only acts on it once the board has been up 15
        # minutes (it sets the last-activity clock to zero), so also tell the panel directly,
        # as the core would, to see the dark state now.
        board.send(K_SET_POWER_SAVING, 1)
        custom(board, -2, 1)
    else:
        board.keep_awake()
        custom(board, -2, 0)
    time.sleep(0.2)


def cmd_lamp(board, args):
    start(board)
    custom(board, args.id, args.state)
    time.sleep(0.2)


def cmd_send(board, args):
    start(board)
    custom(board, args.id, args.payload)
    time.sleep(0.2)


DEMOS = {
    # Lamp states per step, mirroring the design's own demo states.
    "door": [
        ("all secure, GPU connected", {0: 1}),
        ("boarding", {0: 1, 3: 1, 7: 1, 11: 1, 12: 1}),
        ("cargo loading", {0: 1, 11: 1, 12: 1}),
        ("in-flight caution, overwing exit blinking", {4: 2}),
    ],
    "irs": [
        ("power-up, all dark", {}),
        ("present position required", {3: 2, 7: 2}),
        ("aligning", {3: 1, 7: 1}),
        ("NAV", {0: 1}),
        ("R IRU on battery", {4: 1, 8: 1, 10: 1}),
    ],
    # Lamps 0-2, then the two 12-character display lines (10, 11) and the selector
    # positions (12 DC, 13 AC) -- the messages the PMDG's meter offsets feed.
    "elec": [
        ("battery only, DC BAT / AC STBY PWR", {0: 1, 10: "  10     400", 11: "  24   2 115", 12: 2, 13: 0}),
        ("ground power, DC TR1 / AC GRD PWR", {10: "  20     400", 11: "  28  45 115", 12: 4, 13: 1}),
        ("APU generator, DC TR2 / AC APU GEN", {10: "  21     400", 11: "  28  52 115", 12: 5, 13: 3}),
        ("TR3 failed", {1: 1, 10: "   0     400", 11: "   0  60 115", 12: 6, 13: 4}),
    ],
    # 0 FIRE WARN, 1 MASTER CAUTION, 2-7 the six-pack, row by row.
    "mcs_l": [
        ("all clear", {}),
        ("MASTER CAUTION, FUEL", {1: 1, 6: 1}),
        ("engine fire: FIRE WARN, MASTER CAUTION, OVHT/DET", {0: 1, 1: 1, 7: 1}),
        ("recall: every caption", {1: 1, 2: 1, 3: 1, 4: 1, 5: 1, 6: 1, 7: 1}),
    ],
    "mcs_r": [
        ("all clear", {}),
        ("MASTER CAUTION, DOORS", {1: 1, 6: 1}),
        ("engine fire: FIRE WARN, MASTER CAUTION, ENG", {0: 1, 1: 1, 3: 1}),
        ("recall: every caption", {1: 1, 2: 1, 3: 1, 4: 1, 5: 1, 6: 1, 7: 1}),
    ],
    # 0-2 ZONE TEMP, 3 DUAL BLEED, 4-5 RAM DOOR, 6-8 / 9-11 the bleed stacks, 12-15 pressurisation.
    "air": [
        ("engine start on APU bleed: DUAL BLEED, RAM DOORs", {3: 1, 4: 1, 5: 1}),
        ("left pack and bleed trip", {6: 1, 8: 1}),
        ("single pressure controller failure: AUTO FAIL, ALTN", {12: 1, 14: 1}),
        ("manual pressurisation", {15: 1}),
    ],
    # 0-1 FLT CONTROL LOW PRESSURE, 2-4 STANDBY HYD, 5-8 the four-stack.
    "fctl": [
        ("cold aircraft: both LOW PRESSURE", {0: 1, 1: 1}),
        ("system A lost: A LOW PRESSURE, FEEL DIFF PRESS", {0: 1, 5: 1}),
        ("standby rudder: STBY RUD ON", {4: 1}),
        ("trim failures", {6: 1, 7: 1}),
    ],
    # 0/1 the display strings, 2 dots, 3 DSPL SEL, 4 SYS DSPL, 5/6 ENT/CLR cue lights.
    "isdu": [
        ("PPOS, as in the FCOM", {0: "N47324", 1: "W122123", 2: 1, 3: 2, 4: 0, 5: 0, 6: 0}),
        ("TK/GS", {0: "285", 1: "452", 2: 0, 3: 1}),
        ("keying a longitude: ENT cue lit", {0: "", 1: "W171580", 2: 1, 3: 2, 5: 1}),
        ("HDG/STS during alignment, right IRS", {0: "341", 1: "15", 2: 0, 3: 4, 4: 1, 5: 0}),
    ],
    # 2-7 the captain's six-pack, 8-13 the first officer's.
    "mcs_both": [
        ("all clear", {}),
        ("MASTER CAUTION, FUEL, DOORS", {1: 1, 6: 1, 12: 1}),
        ("engine fire: FIRE WARN, MASTER CAUTION, OVHT/DET, ENG", {0: 1, 1: 1, 7: 1, 9: 1}),
        ("recall: every caption", {i: 1 for i in range(1, 14)}),
    ],
    # The same, then the autoflight row: a disconnect is red (1), a caution amber (3).
    "mcs_both_afds": [
        ("all clear", {}),
        ("MASTER CAUTION, FUEL, DOORS", {1: 1, 6: 1, 12: 1}),
        ("engine fire: FIRE WARN, MASTER CAUTION, OVHT/DET, ENG", {0: 1, 1: 1, 7: 1, 9: 1}),
        ("autopilot disconnect: A/P red, blinking", {14: 2}),
        ("autothrottle disconnect: A/T red", {15: 1}),
        ("cautions: A/P and A/T amber, FMC", {14: 3, 15: 3, 16: 1}),
        ("recall: every caption", {i: 1 for i in range(1, 14)}),
    ],
}


def cmd_demo(board, args):
    count = {"door": 13, "irs": 11, "elec": 3, "mcs_l": 8, "mcs_r": 8, "mcs_both": 14,
             "mcs_both_afds": 17, "air": 16, "fctl": 9, "isdu": 0}[args.panel]
    start(board)
    for label, values in DEMOS[args.panel]:
        for i in range(count):
            custom(board, i, values.get(i, 0))       # every lamp, so unlit ones reset
        for i in sorted(k for k in values if k >= count):
            custom(board, i, values[i])               # display lines, selectors
        print("  %s" % label)
        time.sleep(args.dwell)


def watch(board, seconds, until_debug=None):
    """Print what the board sends: touch buttons as the Connector would log them, and the
    firmware's debug lines. Returns the first debug line containing `until_debug`."""
    deadline = time.time() + seconds
    while time.time() < deadline:
        if time.time() - board.awake_at >= KEEP_AWAKE_S:
            board.keep_awake()  # as the Connector does while running
        msgs = list(board._messages())
        for line in board.boot_text:
            print("  boot    %s" % line)
        board.boot_text.clear()
        for msg in msgs:
            if msg[0] == K_BUTTON_CHANGE and len(msg) >= 3:
                print("  button  %-20s %s" % (msg[1], {"0": "PRESS", "1": "RELEASE"}.get(msg[2], msg[2])))
            elif msg[0] == K_DEBUG:
                text = ",".join(msg[1:])
                print("  debug   %s" % text)
                if until_debug and until_debug in text:
                    return text
    return None


def cmd_listen(board, args):
    # The panel only takes presses while lit, so make sure it is: awake, running, bus powered.
    start(board)
    custom(board, 105, 1)
    print("  press the lamps -- Ctrl-C to stop" if args.seconds <= 0 else
          "  press the lamps for %d s" % args.seconds)
    try:
        watch(board, args.seconds if args.seconds > 0 else 10 ** 9)
    except KeyboardInterrupt:
        pass


def cmd_touchcal(board, _args):
    start(board)
    custom(board, 105, 1)
    custom(board, 106, 1)
    print("  press the centre of each cross as it appears (4 in all)")
    result = watch(board, 130, until_debug="touch cal swap")
    if not result:
        sys.exit("FAIL  no calibration result -- did the crosses appear?")
    print("OK    %s" % result)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", help="serial port (default: first USB serial port found)")
    ap.add_argument("-v", "--verbose", action="store_true", help="show raw traffic")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info", help="identify the board")

    p = sub.add_parser("setup", help="upload and activate a config for one panel")
    p.add_argument("panel", choices=sorted(PANELS))
    p.add_argument("--brightness", type=int, default=255)

    p = sub.add_parser("lamp", help="set one lamp")
    p.add_argument("id", type=int)
    p.add_argument("state", type=int, choices=[0, 1, 2])

    p = sub.add_parser("send", help="send any message ID with a payload")
    p.add_argument("id", type=int)
    p.add_argument("payload")

    sub.add_parser("selftest", help="cycle the design's demo states on the panel")
    sub.add_parser("stop", help="what the Connector sends on Stop: -1 with no value, then power saving on")
    p = sub.add_parser("powersave", help="MobiFlight power saving: on darkens the panel, off wakes it")
    p.add_argument("state", choices=["on", "off"])

    p = sub.add_parser("listen", help="print touch-screen button events")
    p.add_argument("seconds", type=int, nargs="?", default=0, help="0 (default): until Ctrl-C")
    sub.add_parser("touchcal", help="run the touch calibration and print what it measured")

    p = sub.add_parser("demo", help="walk through a few panel states")
    p.add_argument("panel", choices=sorted(DEMOS))
    p.add_argument("--dwell", type=float, default=3.0)

    args = ap.parse_args()
    board = Board(args.port or find_port(), verbose=args.verbose)
    try:
        {"info": cmd_info, "setup": cmd_setup, "lamp": cmd_lamp, "send": cmd_send,
         "demo": cmd_demo, "selftest": cmd_selftest, "stop": cmd_stop,
         "listen": cmd_listen, "touchcal": cmd_touchcal, "powersave": cmd_powersave}[args.cmd](board, args)
    finally:
        board.close()


if __name__ == "__main__":
    main()
