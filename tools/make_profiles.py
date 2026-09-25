#!/usr/bin/env python3
"""Generate MobiFlight panel configs (.mfproj) for the PMDG 737-800, to merge into a project.

One file per panel. Each holds one config tab: outputs that read PMDG data and send it to the
panel's custom device, and -- on touch panels -- inputs that send the pressed button to the
aircraft. Merge it into the project you fly with (the B107's): in Connector 11.2+, the + after
the last tab > From existing project. File > Open would REPLACE that project instead.

    python3 tools/make_profiles.py                                  # every panel, this board
    python3 tools/make_profiles.py --serial SN-XXXXXXXXXXXX
    python3 tools/make_profiles.py --board door=SN-AAAA:Doors --board mcs_both=SN-BBBB:Glareshield

A board holds one panel. With --board, each named panel is bound to its own board (and, after
the colon, that board's name as the Connector shows it), and Annunciator-Combined-*.mfproj
holds all of them, one tab each -- merge that one file when several panels are plugged in.

The Connector finds the board by serial (and name) and the panel by the custom device's name,
so they must match the board. The device names are the device definitions' labels -- what
the Connector names a device when you add it, and what tools/mfsim.py setup uses -- so they
match unless you renamed the device (then pass --name door="..."). A name is at most 16
characters, none of : . ; , # / |.

Outputs read the PMDG SDK through FSUIPC7 offsets, because those are typed and documented;
two ISDU lights and the IRS ILS/GLS lamps have no SDK field and read the aircraft's L-vars
through MobiFlight's WASM module instead (the B107 does the same for most of its lights).
Touch inputs: the doors send PMDG SDK events through FSUIPC7 (they have no click spot);
master caution and the ISDU keys send the aircraft's click-spot codes through the WASM module
-- the route every B107 input already uses on this PC (--inputs fsuipc sends PMDG events
instead). The ISDU knobs run HubHop's own knob code.

Needs on the sim PC: MobiFlight Connector 11.2 or later; FSUIPC7 7.5.6 or later; MobiFlight's
WASM module (the Connector installs it); and in the -800's 737_Options.ini:
    [SDK]
    EnableDataBroadcast=1

Offsets are from FSUIPC's "Offset Mapping for PMDG 737.pdf" (ships inside the FSUIPC7
installer), event numbers from the PMDG SDK header (PMDG_NG3_SDK.h). --offsets-text and
--sdk-header cross-check every one against those files; the touch buttons are always checked
against the firmware's zone tables, both ways. The file format is what MobiFlight Connector
writes (_version 0.9; 11.2 migrates it on load).
"""

import argparse
import hashlib
import json
import re
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "profiles"
DEVICES = ROOT / "Annunciator" / "Community" / "devices"
COMBINED = "Annunciator-Combined-PMDG737-800.mfproj"

# The board's name as the Connector shows it -- the firmware's MOBIFLIGHT_NAME until the
# board is renamed in the Connector. With several boards, rename each and pass the names with
# --board: auto-binding prefers a board with the right NAME over one with the right serial.
#
# A profile records only a controller's name and serial, never which board it is, so panels
# on a 3.2in board and a 3.5in board mix freely in one project -- the panels themselves are
# identical on both. The 3.5in board's firmware reports "Annunciator 35" instead, so name
# it explicitly: --board isdu=SN-...:"Annunciator 35".
BOARD_NAME = "Annunciator"
DEFAULT_SERIAL = "SN-B0CBD803AF24"

INT, STRING = 0, 2  # FSUIPCOffsetType: Integer=0, Float=1, String=2 (serialised as numbers)
LVAR = "lvar"       # not FSUIPC: an L-var read through MobiFlight's WASM module (SimConnect)

# The Connector's own rule for a device or board name.
NAME_MAX = 16
NAME_RESERVED = ":.;,#/|"

# (message id, label, PMDG SDK field or L-var expression, FSUIPC offset, size, offset type)
DOOR = [
    (0, "GRD PWR", "ELEC_annunGRD_POWER_AVAILABLE", 0x64AE, 1, INT),
    (1, "AIR STAIR", "DOOR_annunAIRSTAIR", 0x657A, 1, INT),
    (2, "EQUIP", "DOOR_annunEQUIP", 0x657E, 1, INT),
    (3, "FWD ENTRY", "DOOR_annunFWD_ENTRY", 0x6578, 1, INT),
    (4, "LEFT FWD OVERWING", "DOOR_annunLEFT_FWD_OVERWING", 0x657B, 1, INT),
    (5, "LEFT AFT OVERWING", "DOOR_annunLEFT_AFT_OVERWING", 0x657F, 1, INT),
    (6, "AFT ENTRY", "DOOR_annunAFT_ENTRY", 0x6582, 1, INT),
    (7, "FWD SERVICE", "DOOR_annunFWD_SERVICE", 0x6579, 1, INT),
    (8, "RIGHT FWD OVERWING", "DOOR_annunRIGHT_FWD_OVERWING", 0x657C, 1, INT),
    (9, "RIGHT AFT OVERWING", "DOOR_annunRIGHT_AFT_OVERWING", 0x6580, 1, INT),
    (10, "AFT SERVICE", "DOOR_annunAFT_SERVICE", 0x6583, 1, INT),
    (11, "FWD CARGO", "DOOR_annunFWD_CARGO", 0x657D, 1, INT),
    (12, "AFT CARGO", "DOOR_annunAFT_CARGO", 0x6581, 1, INT),
]

# The SDK and FSUIPC's table have no ILS/GLS field, but the aircraft has L-vars for the two
# lamps (HubHop; fitted with the MMR option). "0 >" makes 0/1 of them: IRS treats 2 as blink.
IRS = [
    (0, "GPS", "IRS_annunGPS", 0x6422, 1, INT),
    (1, "ILS", "(L:switch_2461_73X, number) 0 >", None, 0, LVAR),
    (2, "GLS", "(L:switch_2462_73X, number) 0 >", None, 0, LVAR),
    (3, "L IRU ALIGN", "IRS_annunALIGN[0]", 0x6423, 1, INT),
    (4, "L IRU ON DC", "IRS_annunON_DC[0]", 0x6425, 1, INT),
    (5, "L IRU FAULT", "IRS_annunFAULT[0]", 0x6427, 1, INT),
    (6, "L IRU DC FAIL", "IRS_annunDC_FAIL[0]", 0x6429, 1, INT),
    (7, "R IRU ALIGN", "IRS_annunALIGN[1]", 0x6424, 1, INT),
    (8, "R IRU ON DC", "IRS_annunON_DC[1]", 0x6426, 1, INT),
    (9, "R IRU FAULT", "IRS_annunFAULT[1]", 0x6428, 1, INT),
    (10, "R IRU DC FAIL", "IRS_annunDC_FAIL[1]", 0x642A, 1, INT),
]

ELEC = [
    (0, "BAT DISCHARGE", "ELEC_annunBAT_DISCHARGE", 0x64A0, 1, INT),
    (1, "TR UNIT", "ELEC_annunTR_UNIT", 0x64A1, 1, INT),
    (2, "ELEC", "ELEC_annunELEC", 0x64A2, 1, INT),
    (10, "Display top line", "ELEC_MeterDisplayTop", 0x64BC, 13, STRING),
    (11, "Display bottom line", "ELEC_MeterDisplayBottom", 0x64C9, 13, STRING),
    (12, "DC meter selector", "ELEC_DCMeterSelector", 0x64A3, 1, INT),
    (13, "AC meter selector", "ELEC_ACMeterSelector", 0x64A4, 1, INT),
]

# Air conditioning, bleed and pressurisation, in the firmware's order (AirPanel.h). Which
# ZONE TEMP index is which cabin is inferred (CONT, FWD, AFT); check it in the sim.
AIR = [
    (0, "ZONE TEMP CONT CAB", "AIR_annunZoneTemp[0]", 0x6530, 1, INT),
    (1, "ZONE TEMP FWD CAB", "AIR_annunZoneTemp[1]", 0x6531, 1, INT),
    (2, "ZONE TEMP AFT CAB", "AIR_annunZoneTemp[2]", 0x6532, 1, INT),
    (3, "DUAL BLEED", "AIR_annunDualBleed", 0x6533, 1, INT),
    (4, "RAM DOOR FULL OPEN L", "AIR_annunRamDoorL", 0x6534, 1, INT),
    (5, "RAM DOOR FULL OPEN R", "AIR_annunRamDoorR", 0x6535, 1, INT),
    (6, "L PACK", "AIR_annunPackTripOff[0]", 0x653E, 1, INT),
    (7, "L WING-BODY OVERHEAT", "AIR_annunWingBodyOverheat[0]", 0x6540, 1, INT),
    (8, "L BLEED TRIP OFF", "AIR_annunBleedTripOff[0]", 0x6542, 1, INT),
    (9, "R PACK", "AIR_annunPackTripOff[1]", 0x653F, 1, INT),
    (10, "R WING-BODY OVERHEAT", "AIR_annunWingBodyOverheat[1]", 0x6541, 1, INT),
    (11, "R BLEED TRIP OFF", "AIR_annunBleedTripOff[1]", 0x6543, 1, INT),
    (12, "AUTO FAIL", "AIR_annunAUTO_FAIL", 0x6544, 1, INT),
    (13, "OFF SCHED DESCENT", "AIR_annunOFFSCHED_DESCENT", 0x6545, 1, INT),
    (14, "ALTN", "AIR_annunALTN", 0x6546, 1, INT),
    (15, "MANUAL", "AIR_annunMANUAL", 0x6547, 1, INT),
]

# Flight controls, in the firmware's order (FctlPanel.h). [0] being system A is inferred.
FCTL = [
    (0, "FLT CONTROL A LOW PRESSURE", "FCTL_annunFC_LOW_PRESSURE[0]", 0x645E, 1, INT),
    (1, "FLT CONTROL B LOW PRESSURE", "FCTL_annunFC_LOW_PRESSURE[1]", 0x645F, 1, INT),
    (2, "STANDBY HYD LOW QUANTITY", "FCTL_annunLOW_QUANTITY", 0x6461, 1, INT),
    (3, "STANDBY HYD LOW PRESSURE", "FCTL_annunLOW_PRESSURE", 0x6462, 1, INT),
    (4, "STBY RUD ON", "FCTL_annunLOW_STBY_RUD_ON", 0x6463, 1, INT),
    (5, "FEEL DIFF PRESS", "FCTL_annunFEEL_DIFF_PRESS", 0x6464, 1, INT),
    (6, "SPEED TRIM FAIL", "FCTL_annunSPEED_TRIM_FAIL", 0x6465, 1, INT),
    (7, "MACH TRIM FAIL", "FCTL_annunMACH_TRIM_FAIL", 0x6466, 1, INT),
    (8, "AUTO SLAT FAIL", "FCTL_annunAUTO_SLAT_FAIL", 0x6467, 1, INT),
]  # YAW DAMPER (FCTL_annunYAW_DAMPER, 0x6460) is left out: the B107 has that lamp.

# IRS display unit. The ENT / CLR cue lights have no SDK field; the aircraft has them as
# L-vars next to the keyboard events' numbers (ENT +241, CLR +244).
ISDU = [
    (0, "Left display", "IRS_DisplayLeft", 0x642E, 7, STRING),
    (1, "Right display", "IRS_DisplayRight", 0x6435, 8, STRING),
    (2, "Dots", "IRS_DisplayShowsDots", 0x643D, 1, INT),
    (3, "DSPL SEL", "IRS_DisplaySelector", 0x6420, 1, INT),
    (4, "SYS DSPL", "IRS_SysDisplay_R", 0x6421, 1, INT),
    (5, "ENT cue lights", "(L:switch_242_73X, number) 0 >", None, 0, LVAR),
    (6, "CLR cue lights", "(L:switch_245_73X, number) 0 >", None, 0, LVAR),
]

# Glareshield master caution units, one per side. 0 FIRE WARN, 1 MASTER CAUTION, then the
# six-pack row by row, left column first.
MCS_L = [
    (0, "FIRE WARN", "WARN_annunFIRE_WARN[0]", 0x65A4, 1, INT),
    (1, "MASTER CAUTION", "WARN_annunMASTER_CAUTION[0]", 0x65A6, 1, INT),
    (2, "FLT CONT", "WARN_annunFLT_CONT", 0x65A8, 1, INT),
    (3, "ELEC", "WARN_annunELEC", 0x65AB, 1, INT),
    (4, "IRS", "WARN_annunIRS", 0x65A9, 1, INT),
    (5, "APU", "WARN_annunAPU", 0x65AC, 1, INT),
    (6, "FUEL", "WARN_annunFUEL", 0x65AA, 1, INT),
    (7, "OVHT/DET", "WARN_annunOVHT_DET", 0x65AD, 1, INT),
]
MCS_R = [
    (0, "FIRE WARN", "WARN_annunFIRE_WARN[1]", 0x65A5, 1, INT),
    (1, "MASTER CAUTION", "WARN_annunMASTER_CAUTION[1]", 0x65A7, 1, INT),
    (2, "ANTI-ICE", "WARN_annunANTI_ICE", 0x65AE, 1, INT),
    (3, "ENG", "WARN_annunENG", 0x65B1, 1, INT),
    (4, "HYD", "WARN_annunHYD", 0x65AF, 1, INT),
    (5, "OVERHEAD", "WARN_annunOVERHEAD", 0x65B2, 1, INT),
    (6, "DOORS", "WARN_annunDOORS", 0x65B0, 1, INT),
    (7, "AIR COND", "WARN_annunAIR_COND", 0x65B3, 1, INT),
]

# Both six-packs on one screen: one FIRE WARN and MASTER CAUTION (the sides light together;
# the captain's are bound), then the captain's six-pack and the first officer's.
MCS_BOTH = MCS_L + [(mid + 6, label, field, off, size, otype) for mid, label, field, off, size, otype in MCS_R[2:]]

# The autoflight P/RST lights, added by ANNUN_MCS_BOTH_AFDS on top of the fourteen above.
#
# Each of A/P and A/T has two annunciators in the PMDG's data, a red one and an amber one,
# and the panel shows the difference: "1" lights the lamp red, "3" amber (see LampState in
# Annunciator/Panel.h). They sit next to each other in the offset map -- MAIN_annunAP[2] at
# 0x65F1 is followed by MAIN_annunAP_Amber[2] at 0x65F3 -- so one four-byte read covers both
# and the transformation picks the colour. Byte 0 of each pair is the captain's side, which
# is the one this panel shows.
#
#   value = red[capt] + red[fo]*256 + amber[capt]*65536 + amber[fo]*16777216
#
# Red wins when both are set, which is what the aircraft does: the warning outranks the
# caution. FMC has no red, so it is a plain byte.
AFDS = [
    (14, "A/P P/RST (red, amber)", "MAIN_annunAP[0] + MAIN_annunAP_Amber[0]", 0x65F1, 4, INT,
     "if($%256>0,1,if(floor($/65536)%256>0,3,0))", 1),
    (15, "A/T P/RST (red, amber)", "MAIN_annunAT[0] + MAIN_annunAT_Amber[0]", 0x65F5, 4, INT,
     "if($%256>0,1,if(floor($/65536)%256>0,3,0))", 1),
    (16, "FMC P/RST (amber)", "MAIN_annunFMC[0]", 0x65F9, 1, INT, None, 1),
]

# Shared by every panel: (id, label, field, offset, size, type, transformation, test value).
# MAIN_LightsSelector is the TEST / BRT / DIM switch (0 / 1 / 2). The panel is dark while the
# DC BATT BUS is unpowered -- when the real annunciators are dark.
#
# Bus power is 107, "bus unpowered", not 105: Test Mode sends each output its test value as
# it is, whatever the binding says, and the end of every test sends "0" -- which must not
# darken the panel. It goes LAST: at Run the firmware keeps the panel dark until it arrives,
# so the panel lights once, already showing every lamp.
#
# Test values are what the Connector's tests show: Test Mode sends them as they are, the
# config dialog's Test runs them through the transformation (2 -> dimmed; -1 -> lamp test,
# the expression treating <= 0 as TEST; 0 -> "unpowered" = dark, which is what 107 means).
# A row's own Test button always sends "1": on the bus row that is "unpowered", dark.
SHARED = [
    (102, "Night dimming (lights switch DIM)", "MAIN_LightsSelector", 0x6601, 1, INT, "if($=2,1,0)", 2),
    (103, "Lamp test (lights switch TEST)", "MAIN_LightsSelector", 0x6601, 1, INT, "if($<=0,1,0)", -1),
    (107, "Bus unpowered (DC BATT BUS off)", "ELEC_BusPowered[2]", 0x64D6 + 2, 1, INT, "if($=0,1,0)", 0),
]

# What the Connector's tests show on the text displays.
TEST_TEXT = {
    ("elec", 10): "  28  49 115", ("elec", 11): "  28  49 115",
    ("isdu", 0): "N47324", ("isdu", 1): "W122123",  # the FCOM's PPOS example
}

# ---- Touch buttons -----------------------------------------------------------------------
# Each is (the name the firmware sends, the PMDG SDK event it stands for, that event's number
# above THIRD_PARTY_EVENT_ID_MIN, HOLD if the aircraft needs the release too). The names must
# match the firmware's zone tables; main() checks both ways.
#
# How a press reaches the aircraft depends on the route (--inputs):
#   wasm    the event's click spot: ROTOR_BRAKE <number><action> through MobiFlight's WASM
#           module, action 01 a left click and 04 its release -- HubHop's codes, and the
#           route every B107 input uses on this PC. The default.
#   fsuipc  the PMDG SDK event through FSUIPC7 (the Connector's "FSUIPC - PMDG - Event ID"),
#           parameter MOUSE_FLAG_LEFTSINGLE, and MOUSE_FLAG_LEFTRELEASE for a release.
# The doors always go the FSUIPC way: they are keyboard-shortcut events with no click spot.
THIRD_PARTY_EVENT_ID_MIN = 0x00011000  # 69632
MOUSE_FLAG_LEFTSINGLE = 0x20000000
MOUSE_FLAG_LEFTRELEASE = 0x00020000
CLICK, CLICK_RELEASE = 1, 4
TAP, HOLD = False, True

# Door numbers are from the MSFS SDK (PMDG_NG3_SDK.h), which added CARGO_MAIN at 14015 --
# MobiFlight's own PMDG 737 event list is the older NGX one, whose EQUIPMENT_HATCH and
# AIRSTAIR are one lower. Both overwing exits on a side are one event: either lamp toggles
# that side's exits.
DOOR_TOUCH = [
    ("AIR STAIR", "EVT_DOOR_AIRSTAIR", 14017, TAP),
    ("EQUIP", "EVT_DOOR_EQUIPMENT_HATCH", 14016, TAP),
    ("FWD ENTRY", "EVT_DOOR_FWD_L", 14005, TAP),
    ("LEFT FWD OVERWING", "EVT_DOOR_OVERWING_EXIT_L", 14009, TAP),
    ("LEFT AFT OVERWING", "EVT_DOOR_OVERWING_EXIT_L", 14009, TAP),
    ("AFT ENTRY", "EVT_DOOR_AFT_L", 14007, TAP),
    ("FWD SERVICE", "EVT_DOOR_FWD_R", 14006, TAP),
    ("RIGHT FWD OVERWING", "EVT_DOOR_OVERWING_EXIT_R", 14010, TAP),
    ("RIGHT AFT OVERWING", "EVT_DOOR_OVERWING_EXIT_R", 14010, TAP),
    ("AFT SERVICE", "EVT_DOOR_AFT_R", 14008, TAP),
    ("FWD CARGO", "EVT_DOOR_CARGO_FWD", 14013, TAP),
    ("AFT CARGO", "EVT_DOOR_CARGO_AFT", 14014, TAP),
]
# The six-pack recalls only while held, like the real one: press and release both go out.
MCS_L_TOUCH = [
    ("FIRE WARN L", "EVT_FIRE_WARN_LIGHT_LEFT", 347, TAP),
    ("MASTER CAUTION L", "EVT_MASTER_CAUTION_LIGHT_LEFT", 348, TAP),
    ("RECALL L", "EVT_SYSTEM_ANNUNCIATOR_PANEL_LEFT", 349, HOLD),
]
MCS_R_TOUCH = [
    ("FIRE WARN R", "EVT_FIRE_WARN_LIGHT_RIGHT", 439, TAP),
    ("MASTER CAUTION R", "EVT_MASTER_CAUTION_LIGHT_RIGHT", 438, TAP),
    ("RECALL R", "EVT_SYSTEM_ANNUNCIATOR_PANEL_RIGHT", 437, HOLD),
]
MCS_BOTH_TOUCH = [
    ("FIRE WARN", "EVT_FIRE_WARN_LIGHT_LEFT", 347, TAP),
    ("MASTER CAUTION", "EVT_MASTER_CAUTION_LIGHT_LEFT", 348, TAP),
    ("RECALL L", "EVT_SYSTEM_ANNUNCIATOR_PANEL_LEFT", 349, HOLD),
    ("RECALL R", "EVT_SYSTEM_ANNUNCIATOR_PANEL_RIGHT", 437, HOLD),
]
ISDU_KEYS = [("ISDU %d" % d, "EVT_ISDU_KBD_%d" % d, 231 + d, TAP) for d in range(1, 10)] + [
    ("ISDU 0", "EVT_ISDU_KBD_0", 243, TAP),
    ("ISDU ENT", "EVT_ISDU_KBD_ENT", 241, TAP),
    ("ISDU CLR", "EVT_ISDU_KBD_CLR", 244, TAP),
]

# The ISDU knobs: (name, code on press, code on release or None, SDK events the code uses).
# HubHop's "PMDG B737 Irs Display Selector ..." presets, the pattern the B107 uses for its IRS
# mode selectors: read L:switch_229_73X (0, 10 .. 40) once, then step the knob that many
# detents with its click spot (229 07 up, 229 08 down). It does not re-read the knob, so a
# second tap before the aircraft has applied the first can overshoot; tap again to correct.
# The PMDG also takes the target position as the parameter of 69861 -- a later option once
# checked in the sim. TEST is spring-loaded on the real unit, so releasing it steps back to
# TK/GS if the knob is still at TEST. SYS DSPL flips (231 01) only if it is not there already.
def _dspl_sel(position):
    return ("%d (L:switch_229_73X,number) - 10 div s0 :1 "
            "l0 0 > if{ 22907 (>K:ROTOR_BRAKE) l0 -- s0 g1 } "
            "l0 0 < if{ 22908 (>K:ROTOR_BRAKE) l0 ++ s0 g1 }" % (position * 10))

_SEL = [("EVT_ISDU_DSPL_SEL", 229)]
_SYS = [("EVT_ISDU_SYS_DSPL", 231)]
ISDU_KNOBS = [
    ("ISDU DSPL TEST", _dspl_sel(0), "(L:switch_229_73X,number) 0 == if{ 22907 (>K:ROTOR_BRAKE) }", _SEL),
    ("ISDU DSPL TK GS", _dspl_sel(1), None, _SEL),
    ("ISDU DSPL PPOS", _dspl_sel(2), None, _SEL),
    ("ISDU DSPL WIND", _dspl_sel(3), None, _SEL),
    ("ISDU DSPL HDG STS", _dspl_sel(4), None, _SEL),
    ("ISDU SYS DSPL L", "(L:switch_231_73X, number) 0 > if{ 23101 (>K:ROTOR_BRAKE) }", None, _SYS),
    ("ISDU SYS DSPL R", "(L:switch_231_73X, number) 0 == if{ 23101 (>K:ROTOR_BRAKE) }", None, _SYS),
]

# key: (custom device type, device JSON, file title, outputs, event buttons, code buttons,
#       firmware file and zone table holding the buttons' names)
PANELS = {
    "door": ("ANNUN_DOOR", "annunciator_door.device.json", "Doors", DOOR, DOOR_TOUCH, [],
             ("DoorPanel/DoorPanel.cpp", "ZONES")),
    "irs": ("ANNUN_IRS", "annunciator_irs.device.json", "IRS", IRS, [], [], None),
    "elec": ("ANNUN_ELEC", "annunciator_elec.device.json", "Electrical", ELEC, [], [], None),
    "air": ("ANNUN_AIR", "annunciator_air.device.json", "AirCond-Bleed-Press", AIR, [], [], None),
    "fctl": ("ANNUN_FCTL", "annunciator_fctl.device.json", "FlightControls", FCTL, [], [], None),
    "isdu": ("ANNUN_ISDU", "annunciator_isdu.device.json", "IRS-DisplayUnit", ISDU, ISDU_KEYS, ISDU_KNOBS,
             ("IsduPanel/IsduPanel.cpp", "ZONE_NAME")),
    "mcs_l": ("ANNUN_MCS_L", "annunciator_mcs_l.device.json", "MasterCaution-Captain", MCS_L, MCS_L_TOUCH, [],
              ("McsPanel/McsPanel.cpp", "LEFT_ZONES")),
    "mcs_r": ("ANNUN_MCS_R", "annunciator_mcs_r.device.json", "MasterCaution-FO", MCS_R, MCS_R_TOUCH, [],
              ("McsPanel/McsPanel.cpp", "RIGHT_ZONES")),
    "mcs_both": ("ANNUN_MCS_BOTH", "annunciator_mcs_both.device.json", "MasterCaution-Both", MCS_BOTH,
                 MCS_BOTH_TOUCH, [], ("McsPanel/McsPanel.cpp", "BOTH_ZONES")),
    "mcs_both_afds": ("ANNUN_MCS_BOTH_AFDS", "annunciator_mcs_both_afds.device.json",
                      "MasterCaution-Both-AFDS", MCS_BOTH + AFDS, MCS_BOTH_TOUCH, [],
                      ("McsPanel/McsPanel.cpp", "BOTH_AFDS_ZONES")),
}

# Zones the firmware sends but nothing is bound to yet, with the reason. The three P/RST
# presses belong here: the lights are read from documented offsets, but the event numbers
# for pressing them are not in any reference I could check, and a made-up event number is
# worse than none -- it would look bound and do nothing. Run make_profiles.py with
# --sdk-header to have every event checked against your own copy of the SDK, then add them.
UNBOUND_ZONES = {
    "mcs_both_afds": ["AFDS AP", "AFDS AT", "AFDS FMC"],
}

GUID_NS = uuid.UUID("6f2c1d52-3b0e-4f5a-9c1e-4a9d7a0e2b11")


def device_name(panel):
    """A panel's default device name: its device definition's label, which is what the
    Connector names the device when it is added."""
    return json.loads((DEVICES / PANELS[panel][1]).read_text())["Info"]["Label"]


def name_problem(name):
    if not name or name != name.strip():
        return "empty, or with leading/trailing spaces"
    if len(name) > NAME_MAX:
        return "longer than %d characters" % NAME_MAX
    bad = [c for c in NAME_RESERVED if c in name]
    if bad:
        return "contains %s" % " ".join(bad)
    return None


def output(panel, dtype, dname, board, mid, label, field, offset, size, otype, expr=None, test=None):
    # Stable GUIDs keep regenerated files diffable. The Connector never updates by GUID: a
    # merged file is appended alongside whatever is there, so remove the old tab first.
    guid = str(uuid.uuid5(GUID_NS, "%s:%s:%d" % (panel, board["serial"], mid)))
    if otype == LVAR:
        # Read through MobiFlight's WASM module: VarType 2 is CODE, an RPN expression.
        source = {"SimConnectValue": {"UUID": "", "Value": field, "VarType": 2},
                  "Type": "SimConnectSource"}
    else:
        source = {
            # A string offset is always read 255 bytes long up to its NUL, whatever Size says;
            # 255 is what the Connector's own editor writes for one.
            "FSUIPC": {"Offset": offset, "Size": 255 if otype == STRING else size, "OffsetType": otype,
                       "Mask": 255, "BcdMode": False},  # the Connector's own default
            "Type": "FsuipcSource",
        }
    if otype == STRING:
        test_value = {"type": STRING, "Float64": 0.0, "String": TEST_TEXT.get((panel, mid), "")}
    else:
        test_value = {"type": INT, "Float64": float(1 if test is None else test), "String": None}
    return {
        "Source": source,
        "TestValue": test_value,
        "Device": {"CustomType": dtype, "CustomName": dname, "MessageType": mid, "Value": "",
                   "Name": dname, "Type": "CustomDevice"},
        "DeviceType": "CustomDevice",
        "DeviceName": dname,
        "GUID": guid,
        "Active": True,
        "Name": ("%s - %s  [%s 0x%04X]" % (dname, label, field, offset)) if offset is not None
                else "%s - %s  [%s]" % (dname, label, field),
        "Type": "OutputConfigItem",
        "Controller": {"Name": board["name"], "Serial": board["serial"]},
        "Preconditions": [],
        "Modifiers": {"Items": ([{"Type": "Transformation", "Expression": expr, "Active": True}]
                                if expr else [])},
    }


def event_action(event_name, n, route, release=False):
    """An action sending one PMDG event, by route. Returns (action, label for the name)."""
    if route == "wasm":
        code = "%d%02d (>K:ROTOR_BRAKE)" % (n, CLICK_RELEASE if release else CLICK)
        return {"Type": "MSFS2020CustomInputAction", "Command": code}, code
    param = MOUSE_FLAG_LEFTRELEASE if release else MOUSE_FLAG_LEFTSINGLE
    return ({"Type": "PmdgEventIdInputAction", "AircraftType": "B737",
             "EventId": THIRD_PARTY_EVENT_ID_MIN + n, "Param": str(param)},
            "%s %d" % (event_name, THIRD_PARTY_EVENT_ID_MIN + n))


def touch_input(panel, dname, board, button, press, release, label):
    guid = str(uuid.uuid5(GUID_NS, "%s:%s:touch:%s" % (panel, board["serial"], button)))
    return {
        # The firmware sends the button like one wired to the board -- "7,<name>,0;" on press,
        # "7,<name>,1;" on release -- and the Connector matches it by name. The name is not a
        # device in the board's config, which is fine: matching never looks there.
        "button": {"onPress": press, "onRelease": release,
                   "onLongRelease": None, "onHold": None,
                   "LongReleaseDelay": 350, "HoldDelay": 350, "RepeatDelay": 0},
        "Device": {"Name": button, "Type": "Button"},
        # The pre-11.2 form of Device. 11.2 migrates it away; 11.0/11.1 match on it.
        "DeviceName": button,
        "DeviceType": "Button",
        "GUID": guid,
        "Active": True,
        "Name": "%s - touch %s  [%s]" % (dname, button, label),
        "Type": "InputConfigItem",
        "Controller": {"Name": board["name"], "Serial": board["serial"]},
        "Preconditions": [],
        "Modifiers": {"Items": []},
        "ConfigRefs": [],
    }


def touch_inputs(panel, dname, board, route):
    _, _, _, _, events, codes, _ = PANELS[panel]
    if panel == "door":
        route = "fsuipc"  # no click spots for the doors
    items = []
    for button, event_name, n, hold in events:
        press, label = event_action(event_name, n, route)
        release = event_action(event_name, n, route, release=True)[0] if hold else None
        items.append(touch_input(panel, dname, board, button, press, release,
                                 label + (" + release" if hold else "")))
    for button, press_code, release_code, _ in codes:
        press = {"Type": "MSFS2020CustomInputAction", "Command": press_code}
        release = {"Type": "MSFS2020CustomInputAction", "Command": release_code} if release_code else None
        items.append(touch_input(panel, dname, board, button, press, release,
                                 "knob code" + (" + release" if release_code else "")))
    return items


def config_file(panel, board, dname, route):
    dtype, _, title, rows, _, _, _ = PANELS[panel]
    items = []
    # A lamp row is (id, label, field, offset, size, type), and may carry a transformation
    # and a test value after that when the sim's value is not simply the lamp's state -- the
    # two-colour autoflight lights are the only ones so far.
    for row in rows:
        mid, label, field, off, size, otype = row[:6]
        expr, test = (row[6], row[7]) if len(row) > 6 else (None, None)
        items.append(output(panel, dtype, dname, board, mid, label, field, off, size, otype,
                            expr, test))
    for mid, label, field, off, size, otype, expr, test in SHARED:
        items.append(output(panel, dtype, dname, board, mid, label, field, off, size, otype, expr, test))
    items += touch_inputs(panel, dname, board, route)
    # The tab's label carries a short hash of what is in it, so a tab merged from an older
    # generation can be told apart from a current one.
    # It goes first: the Connector cuts a long tab name short, and the code must stay visible.
    stamp = hashlib.sha1(json.dumps(items, sort_keys=True).encode()).hexdigest()[:6]
    label = "[%s] %s" % (stamp, title)
    return {"Label": label, "FileName": None, "ReferenceOnly": False,
            "EmbedContent": True, "ConfigItems": items}


def project(name, config_files):
    return {
        "Name": name,
        "ConfigFiles": config_files,
        "Sim": "msfs",
        "Aircraft": [],
        "Features": {"FSUIPC": True, "ProSim": False},
        "_version": "0.9",
    }


# ---- Checks ------------------------------------------------------------------------------
def check_offsets(pdf_text):
    """Every binding must match FSUIPC's own table: field name, offset, and for an array the
    index in range and the element size taken into account."""
    rows = {}
    for line in pdf_text.splitlines():
        # "6423 2 BYTE x 2 IRS_annunALIGN[2] ...": offset, total size, type, name[count].
        # (FSUIPC's table prints one name, FCTL_annunLOW_STBY_RUD_ON, with a stray ';'.)
        m = re.match(r"\s*([0-9A-F]{4})\s+(\d+)\s+(\w+).*?\b([A-Z]+_\w+?)(?:\[(\d+)\])?;?\s", line + " ")
        if m:
            count = int(m.group(5)) if m.group(5) else None
            rows[m.group(4)] = (int(m.group(1), 16), int(m.group(2)), count, m.group(3).upper())
    problems = []
    for panel, spec in PANELS.items():
        for mid, label, field, off, size, otype in list(spec[3]) + [s[:6] for s in SHARED]:
            if off is None:
                continue  # an L-var, not in FSUIPC's table
            base, _, idx = field.partition("[")
            index = int(idx.rstrip("]")) if idx else 0
            if base not in rows:
                problems.append("%s: %s not found in the FSUIPC table" % (panel, base))
                continue
            start, total, count, ftype = rows[base]
            if idx and count is None:
                problems.append("%s: %s is indexed but not an array in the FSUIPC table" % (panel, field))
                continue
            if count is not None and index >= count:
                problems.append("%s: %s is past the end of %s[%d]" % (panel, field, base, count))
                continue
            elem = total // count if count else total
            want = start + index * elem
            if want != off:
                problems.append("%s: %s is 0x%04X in the FSUIPC table, profile says 0x%04X"
                                % (panel, field, want, off))
            if otype == INT and (size != elem or ftype not in ("BYTE", "WORD", "DWORD")):
                problems.append("%s: %s is %d-byte %s in the FSUIPC table, profile reads %d-byte integer"
                                % (panel, field, elem, ftype, size))
    return problems


def check_events(header_text):
    """Every event number -- a PMDG event, a click spot, or one a knob's code uses -- must
    match the PMDG SDK header's definition."""
    defs = {m.group(1): int(m.group(2)) for m in
            re.finditer(r"#define\s+(EVT_\w+)\s+\(THIRD_PARTY_EVENT_ID_MIN\s*\+\s*(\d+)\)", header_text)}
    problems = []
    for panel, spec in PANELS.items():
        pairs = [(e, n) for _, e, n, _ in spec[4]] + [pair for c in spec[5] for pair in c[3]]
        for event_name, n in pairs:
            if event_name not in defs:
                problems.append("%s: %s is not in the SDK header" % (panel, event_name))
            elif defs[event_name] != n:
                problems.append("%s: %s is +%d in the SDK header, profile says +%d"
                                % (panel, event_name, defs[event_name], n))
        for button, press, release, events in spec[5]:
            for code in filter(None, (press, release)):
                spots = re.findall(r"(\d+)\s*\(>K:ROTOR_BRAKE\b", code)
                if len(spots) != code.count("ROTOR_BRAKE"):
                    problems.append("%s: %s has a click spot this check cannot read" % (panel, button))
                for spot in spots:
                    if int(spot) // 100 not in [n for _, n in events]:
                        problems.append("%s: %s uses click spot %s, not one of its events"
                                        % (panel, button, spot))
    return problems


def zone_names(source, table):
    """The string literals in one zone table of a panel's .cpp, from the table's opening
    brace to its closing '};'."""
    m = re.search(r"\b%s\s*\[[^\]]*\]\s*=\s*\{(.*?)\};" % re.escape(table), source, re.S)
    if not m:
        return None
    return re.findall(r'"([^"\\]*)"', m.group(1))


def check_touch_names():
    """Each panel's bindings against the zone table the firmware sends names from, both ways:
    a binding the firmware never sends would silently never fire, and a zone with no binding
    would be a dead button."""
    problems = []
    for panel, spec in PANELS.items():
        bound = [t[0] for t in spec[4]] + [c[0] for c in spec[5]]
        for button in bound:
            if any(c in button for c in ",;\\"):
                problems.append("%s: button %r contains a character the serial protocol reserves"
                                % (panel, button))
        for dup in sorted({b for b in bound if bound.count(b) > 1}):
            problems.append("%s: button %r is bound twice" % (panel, dup))
        if not spec[6]:
            if bound:
                problems.append("%s: has bindings but no firmware zone table" % panel)
            continue
        path, table = spec[6]
        zones = zone_names((ROOT / "Annunciator" / path).read_text(), table)
        if zones is None:
            problems.append("%s: zone table %s not found in %s" % (panel, table, path))
            continue
        for dup in sorted({z for z in zones if zones.count(z) > 1}):
            problems.append("%s: zone %r appears twice in %s" % (panel, dup, table))
        for button in sorted(set(bound) - set(zones)):
            problems.append("%s: button %r is bound but %s never sends it" % (panel, button, table))
        known_unbound = set(UNBOUND_ZONES.get(panel, []))
        for button in sorted(set(zones) - set(bound) - known_unbound):
            problems.append("%s: zone %r in %s has no binding" % (panel, button, table))
        for button in sorted(known_unbound - set(zones)):
            problems.append("%s: %r is listed in UNBOUND_ZONES but %s never sends it"
                            % (panel, button, table))
    return problems


# ---- Command line ------------------------------------------------------------------------
SERIAL_RE = re.compile(r"^SN-[0-9A-F]{12}$")


def split_board(value, what):
    """A board as given: 'SN-...', or pasted the way the Connector shows boards,
    'Name/ SN-...'. Returns (serial, name or None)."""
    name, sep, s = value.strip().rpartition("/")
    s = s.strip().upper()
    if not SERIAL_RE.match(s):
        sys.exit("%s: %r is not a board serial (SN- and 12 hex digits, as the Connector shows it)"
                 % (what, value))
    return s, (name.strip() or None) if sep else None


def parse_pairs(values, what):
    out = {}
    for v in values or []:
        key, sep, val = v.partition("=")
        key = key.strip()
        if not sep or key not in PANELS:
            sys.exit("--%s wants PANEL=VALUE with PANEL one of %s, got %r"
                     % (what, ", ".join(PANELS), v))
        if key in out:
            sys.exit("--%s gives %s twice" % (what, key))
        out[key] = val.strip()
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--serial", default=DEFAULT_SERIAL,
                    help="board serial for every panel without its own --board; may be pasted as "
                         "the Connector shows it, 'Name/ SN-...', to give the board's name too")
    ap.add_argument("--board", action="append", metavar="PANEL=SERIAL[:NAME]",
                    help="bind one panel to its own board, optionally with the board's name "
                         "(default %r; or paste 'Name/ SN-...' as the Connector shows it); repeat "
                         "per panel. Also writes %s" % (BOARD_NAME, COMBINED))
    ap.add_argument("--name", action="append", metavar="PANEL=NAME",
                    help="the custom device's name on the board, if not the definition's label")
    ap.add_argument("--inputs", choices=["wasm", "fsuipc"], default="wasm",
                    help="how master caution and ISDU key presses reach the aircraft: click spots "
                         "through MobiFlight's WASM module (default; the B107's route) or PMDG SDK "
                         "events through FSUIPC7. Doors always use FSUIPC7")
    ap.add_argument("--offsets-text", help="text of FSUIPC's 'Offset Mapping for PMDG 737.pdf' "
                                           "to cross-check every offset before writing")
    ap.add_argument("--sdk-header", help="PMDG_NG3_SDK.h, to cross-check every event number")
    args = ap.parse_args()

    problems = check_touch_names()
    for panel in PANELS:
        why = name_problem(device_name(panel))
        if why:
            problems.append("%s: device definition label %r is %s" % (panel, device_name(panel), why))
    if args.offsets_text:
        problems += check_offsets(Path(args.offsets_text).read_text())
    if args.sdk_header:
        problems += check_events(Path(args.sdk_header).read_text(errors="replace"))
    if problems:
        sys.exit("\n".join("FAIL  " + p for p in problems))
    print("OK    touch buttons match the firmware's zone tables, both ways")
    if args.offsets_text:
        print("OK    every offset matches FSUIPC's PMDG 737 table")
    if args.sdk_header:
        print("OK    every event number matches the PMDG SDK header")

    default_serial, default_name = split_board(args.serial, "--serial")
    default_name = default_name or BOARD_NAME
    boards = {}
    for panel, value in parse_pairs(args.board, "board").items():
        serial_part, _, explicit = value.partition(":")
        serial, pasted = split_board(serial_part, "--board " + panel)
        explicit = explicit.strip() or None
        if pasted and explicit and pasted != explicit:
            sys.exit("--board %s: the pasted board name %r and :%s disagree" % (panel, pasted, explicit))
        name = explicit or pasted or BOARD_NAME
        why = name_problem(name)
        if why:
            sys.exit("--board %s: board name %r is %s" % (panel, name, why))
        boards[panel] = {"serial": serial, "name": name}
    taken = {}
    for panel, b in boards.items():
        if b["serial"] in taken:
            sys.exit("%s and %s are both on %s -- a board holds one panel"
                     % (taken[b["serial"]], panel, b["serial"]))
        taken[b["serial"]] = panel
    if boards and default_serial in taken and len(boards) < len(PANELS):
        print("note  panels without --board are written for %s, which --board gives to %s; "
              "merge only the files for the panels you have" % (default_serial, taken[default_serial]))

    names = {k: device_name(k) for k in PANELS}
    for panel, name in parse_pairs(args.name, "name").items():
        why = name_problem(name)
        if why:
            sys.exit("--name %s: %r is %s -- the Connector would not keep it" % (panel, name, why))
        names[panel] = name

    OUT_DIR.mkdir(exist_ok=True)
    combined = []
    for panel in PANELS:
        board = boards.get(panel, {"serial": default_serial, "name": default_name})
        cfg = config_file(panel, board, names[panel], args.inputs)
        path = OUT_DIR / ("Annunciator-%s-PMDG737-800.mfproj" % PANELS[panel][2])
        path.write_text(json.dumps(project(cfg["Label"], [cfg]), indent=2) + "\n")
        n_in = len(PANELS[panel][4]) + len(PANELS[panel][5])
        print("wrote %-52s %2d outputs%s -> %s on %s (%s), tab %s"
              % (path.name, len(cfg["ConfigItems"]) - n_in,
                 (", %2d touch" % n_in) if n_in else "", names[panel], board["serial"], board["name"],
                 cfg["Label"].split("]")[0] + "]"))
        if panel in boards:
            combined.append(cfg)

    path = OUT_DIR / COMBINED
    if combined:
        path.write_text(json.dumps(project("Annunciators - PMDG 737-800", combined), indent=2) + "\n")
        print("wrote %s: %s" % (path.name, ", ".join(p for p in PANELS if p in boards)))
    elif path.exists():
        path.unlink()  # from an earlier --board run; its bindings would be stale now
        print("removed stale %s" % path.name)


if __name__ == "__main__":
    main()
