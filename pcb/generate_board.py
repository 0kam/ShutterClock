#!/usr/bin/env python3
"""Generate the ShutterClock board (KiCad 10, pcbnew API). Spec v0.11 §6.

Run with KiCad's bundled python:
  /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 generate_board.py

Never hand-edit ShutterClock.kicad_pcb: edit this script, regenerate, run DRC
(kicad-cli pcb drc), re-export gerbers. Structure copied from SnowGauge
pcb/generate_board.py (see PROVENANCE.md).
"""
import os
import sys
import pcbnew
from pcbnew import VECTOR2I, FromMM

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "ShutterClock.kicad_pcb")
FPLIB = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints"
XIAO_MOD = os.path.join(HERE, "lib", "XIAO-nRF52840-DIP.kicad_mod")

PLACE_ONLY = "--place-only" in sys.argv


def mm(x, y):
    return VECTOR2I(FromMM(x), FromMM(y))


board = pcbnew.NewBoard(OUT)

# ---------------- nets ----------------
NET_NAMES = [
    "BAT_RAW", "F_OUT", "VIN", "GND", "Q1_G", "TVS_MID",
    "LDO_IN", "3V3_LDO", "3V3_SYS", "ADC_NODE",
    "CAM_EN", "Q3_G", "Q2_DRV", "Q2_G", "BUCK_IN", "BUCK_OUT", "CAM_P", "CAM_N", "A1_NODE",
    "KA_EN", "Q6_G", "Q5_DRV", "Q5_G", "KA_SW", "Q7_D", "Q7_G", "ZD_MID",
    "AF_DRV", "AF_LED_A", "AF_C", "SHUT_DRV", "SHUT_LED_A", "SHUT_C",
    "JACK_TIP", "JACK_RING", "JACK_SLEEVE", "BTN",
]
nets = {}
for n in NET_NAMES:
    ni = pcbnew.NETINFO_ITEM(board, n)
    board.Add(ni)
    nets[n] = ni


# ---------------- footprints ----------------
def load(lib, name):
    fp = pcbnew.FootprintLoad(os.path.join(FPLIB, lib + ".pretty"), name)
    assert fp is not None, f"footprint not found: {lib}/{name}"
    return fp


def load_file(path):
    fp = pcbnew.FootprintLoad(os.path.dirname(path),
                              os.path.splitext(os.path.basename(path))[0])
    assert fp is not None, f"footprint not found: {path}"
    return fp


def custom(name, pads, outline=None, silk_lines=()):
    """Build a through-hole footprint: pads = [(num, x, y, drill, size)]."""
    fp = pcbnew.FOOTPRINT(board)
    fp.SetFPID(pcbnew.LIB_ID("ShutterClock", name))
    for num, x, y, drill, size in pads:
        p = pcbnew.PAD(fp)
        p.SetNumber(num)
        p.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        p.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        p.SetDrillSize(VECTOR2I(FromMM(drill), FromMM(drill)))
        p.SetSize(VECTOR2I(FromMM(size), FromMM(size)))
        p.SetLayerSet(pcbnew.LSET.AllCuMask())
        p.SetPosition(mm(x, y))
        fp.Add(p)
    lines = list(silk_lines)
    if outline:
        x0, y0, x1, y1 = outline
        lines += [((x0, y0), (x1, y0)), ((x1, y0), (x1, y1)), ((x1, y1), (x0, y1)), ((x0, y1), (x0, y0))]
    for a, b in lines:
        s = pcbnew.PCB_SHAPE(fp, pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(mm(*a))
        s.SetEnd(mm(*b))
        s.SetLayer(pcbnew.F_SilkS)
        s.SetWidth(FromMM(0.15))
        fp.Add(s)
    return fp


def fp_jack():
    # Marushin MJ-8435 (datasheet hole layout, viewed from the insertion side):
    # 1 (sleeve) and 3 (ring) 4.4 mm apart on the rear row, 2 (tip, two pins)
    # 5.5 mm apart, 5.5 mm toward the opening. Opening points to +x here.
    return custom("MJ-8435", [
        ("1", -5.5, -2.2, 1.5, 2.4), ("3", -5.5, 2.2, 1.5, 2.4),
        ("2", 0.0, -2.75, 1.5, 2.4), ("2", 0.0, 2.75, 1.5, 2.4),
    ], outline=(-5.5, -3.0, 6.0, 3.0))


def fp_fuseclips():
    # Two FUC-03A clips (2 legs each, 6.7 mm apart across the fuse axis,
    # holes 1.7 mm) 18 mm apart for a 5.2x20 mm glass fuse along x.
    return custom("FUC-03A_x2_5x20", [
        ("1", -9.0, -3.35, 1.8, 2.8), ("1", -9.0, 3.35, 1.8, 2.8),
        ("2", 9.0, -3.35, 1.8, 2.8), ("2", 9.0, 3.35, 1.8, 2.8),
    ], outline=(-12.0, -3.6, 12.0, 3.6))


def fp_buck():
    # Akizuki MBC2596-01 (43 x 21 mm). Corner terminals IN+/IN-/OUT+/OUT-.
    # TODO: measure the real hole positions on the module before ordering;
    # placeholder = 19.0 / 8.0 mm from the centre.
    return custom("MBC2596-01", [
        ("INP", -19.0, -8.0, 1.2, 2.2), ("INN", -19.0, 8.0, 1.2, 2.2),
        ("OUTP", 19.0, -8.0, 1.2, 2.2), ("OUTN", 19.0, 8.0, 1.2, 2.2),
    ], outline=(-21.5, -10.5, 21.5, 10.5))


R = ("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal")
R1W = ("Resistor_THT", "R_Axial_DIN0411_L9.9mm_D3.6mm_P12.70mm_Horizontal")
R3W = ("Resistor_THT", "R_Axial_DIN0617_L17.0mm_D6.0mm_P20.32mm_Horizontal")
DO35 = ("Diode_THT", "D_DO-35_SOD27_P7.62mm_Horizontal")
DO41 = ("Diode_THT", "D_DO-41_SOD81_P10.16mm_Horizontal")
DO201 = ("Diode_THT", "D_DO-201AD_P15.24mm_Horizontal")
TO220 = ("Package_TO_SOT_THT", "TO-220-3_Vertical")
TO251 = ("Package_TO_SOT_THT", "TO-251-3_Vertical")
CDISC = ("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm")
HDR2 = ("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical")
HDR6 = ("Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical")
TB2 = ("TerminalBlock_Phoenix", "TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal")
HOLE = ("MountingHole", "MountingHole_3.2mm_M3")

# Board 100 x 88 mm. XIAO top-left with USB to the top edge; battery J1
# bottom-left; camera J2 bottom-right; jack J3 right edge; buck module centre.
# ref: (loader-args | "XIAO" | callable, value, anchor(x,y), rot, {pad: net})
PARTS = {
    # --- MCU: left column A0/A1 analog, right column digital outputs ---
    "A1": ("XIAO", "XIAO nRF52840", (20, 14), 90,
           {"1": "ADC_NODE", "2": "A1_NODE", "3": "AF_DRV",
            "8": "CAM_EN", "9": "SHUT_DRV", "10": "BTN", "11": "KA_EN",
            "12": "3V3_SYS", "13": "GND"}),
    "C6": (CDISC, "0.1u", (33, 12.5), 0, {"1": "3V3_SYS", "2": "GND"}),
    "JP1": (HDR2, "JP1 3V3", (38, 6), 0, {"1": "3V3_LDO", "2": "3V3_SYS"}),
    "SW1": (("Button_Switch_THT", "SW_PUSH_6mm"), "TEST", (2, 28), 0, {"1": "BTN", "2": "GND"}),
    # --- 3.3 V rail ---
    "U1": (("Package_TO_SOT_SMD", "SOT-89-3"), "NJW4181U3-33B", (44, 16), 0,
           {"1": "3V3_LDO", "2": "GND", "3": "LDO_IN"}),
    "C4": (CDISC, "2.2u", (46, 20), 0, {"1": "3V3_LDO", "2": "GND"}),
    "C2": (CDISC, "10u", (54, 18), 0, {"1": "LDO_IN", "2": "GND"}),
    "R4": (R, "100", (52, 24), 0, {"1": "VIN", "2": "LDO_IN"}),
    # --- battery divider ---
    "R1": (R, "1M", (14, 27), 0, {"1": "ADC_NODE", "2": "VIN"}),
    "R2": (R, "100k", (14, 31), 0, {"1": "ADC_NODE", "2": "GND"}),
    "C7": (CDISC, "0.1u", (4, 36), 0, {"1": "ADC_NODE", "2": "GND"}),
    # --- release ---
    "R6": (R, "1k", (54, 7), 0, {"1": "SHUT_LED_A", "2": "SHUT_DRV"}),
    "R7": (R, "1k", (54, 11), 0, {"1": "AF_LED_A", "2": "AF_DRV"}),
    "U3": (("Package_DIP", "DIP-4_W7.62mm"), "PC817 SHUTTER", (66, 6), 0,
           {"1": "SHUT_LED_A", "2": "GND", "3": "JACK_SLEEVE", "4": "SHUT_C"}),
    "U4": (("Package_DIP", "DIP-4_W7.62mm"), "PC817 AF", (66, 14), 0,
           {"1": "AF_LED_A", "2": "GND", "3": "JACK_SLEEVE", "4": "AF_C"}),
    "R20": (R, "100", (76, 8), 0, {"1": "SHUT_C", "2": "JACK_TIP"}),
    "R21": (R, "100", (76, 12), 0, {"1": "AF_C", "2": "JACK_RING"}),
    "J3": (fp_jack, "3.5mm RELEASE", (92, 14), 0,
           {"1": "JACK_SLEEVE", "2": "JACK_TIP", "3": "JACK_RING"}),
    # --- input protection (left) ---
    "J1": (TB2, "BATTERY 12V", (11, 82), 0, {"1": "BAT_RAW", "2": "GND"}),
    "F1": (fp_fuseclips, "5A", (6, 52), 90, {"1": "BAT_RAW", "2": "F_OUT"}),
    "Q1": (TO220, "2SJ334 REV", (14, 40), 0, {"1": "Q1_G", "2": "F_OUT", "3": "VIN"}),
    "R15": (R, "1M", (13, 48), 0, {"1": "Q1_G", "2": "GND"}),
    "ZD1": (DO35, "15V", (3, 66), 0, {"1": "VIN", "2": "Q1_G"}),
    "D1": (DO41, "P4KE15A", (24, 34.84), 270, {"1": "VIN", "2": "TVS_MID"}),
    "D2": (DO41, "P4KE15A", (24, 47.84), 270, {"1": "TVS_MID", "2": "GND"}),
    "C1": (("Capacitor_THT", "CP_Radial_D10.0mm_P5.00mm"), "100u/50V", (14, 55), 0,
           {"1": "GND", "2": "VIN"}),
    # --- camera switch Q2 (TO-220) driven by Q3 ---
    "Q2": (TO220, "2SJ334 CAM", (36, 34), 0, {"1": "Q2_G", "2": "BUCK_IN", "3": "VIN"}),
    "R12": (R, "100k", (36, 38), 0, {"1": "VIN", "2": "Q2_G"}),
    "ZD2": (DO35, "15V", (33, 45), 90, {"1": "VIN", "2": "Q2_G"}),
    "R16": (R, "33k", (36, 42), 0, {"1": "Q2_G", "2": "Q2_DRV"}),
    "Q3": (TO251, "2SK4017", (46, 44), 0, {"1": "Q3_G", "2": "Q2_DRV", "3": "GND"}),
    "R8": (R, "1k", (36, 47.5), 0, {"1": "CAM_EN", "2": "Q3_G"}),
    "R10": (R, "100k", (36, 51), 0, {"1": "GND", "2": "Q3_G"}),
    "JP2": (HDR2, "JP2 ALWAYS ON", (46, 55), 0, {"1": "Q2_DRV", "2": "GND"}),
    # --- buck module + output diode + bulk ---
    "U2": (fp_buck, "MBC2596-01 9.4V", (74, 38), 0,
           {"INP": "BUCK_IN", "INN": "GND", "OUTP": "BUCK_OUT", "OUTN": "GND"}),
    "D4": (DO201, "1N5822", (56, 54), 0, {"1": "CAM_P", "2": "BUCK_OUT"}),
    "C3": (("Capacitor_THT", "CP_Radial_D16.0mm_P7.50mm"), "4700u/16V", (78, 64), 0,
           {"1": "CAM_P", "2": "GND"}),
    "R17": (R, "1M", (76, 80), 90, {"1": "GND", "2": "CAM_P"}),
    "J2": (TB2, "CAMERA EP-5B", (82, 82), 0, {"1": "CAM_P", "2": "CAM_N"}),
    "R3": (R3W, "0.1R 3W", (50, 84), 0, {"1": "GND", "2": "CAM_N"}),
    "R14": (R, "10k", (52, 74), 0, {"1": "CAM_N", "2": "A1_NODE"}),
    "C8": (CDISC, "0.1u", (52, 78), 0, {"1": "A1_NODE", "2": "GND"}),
    # --- keep-alive path ---
    "Q5": (TO220, "2SJ334 KA", (36, 58), 0, {"1": "Q5_G", "2": "KA_SW", "3": "VIN"}),
    "R13": (R, "100k", (32, 62), 0, {"1": "Q5_G", "2": "VIN"}),
    "ZD3": (DO35, "15V", (42, 62), 0, {"1": "VIN", "2": "Q5_G"}),
    "R18": (R, "33k", (32, 66), 0, {"1": "Q5_G", "2": "Q5_DRV"}),
    "Q6": (TO251, "2SK4017", (37, 70), 0, {"1": "Q6_G", "2": "Q5_DRV", "3": "GND"}),
    "R9": (R, "1k", (32, 74), 0, {"1": "KA_EN", "2": "Q6_G"}),
    "R11": (R, "100k", (32, 78), 0, {"1": "GND", "2": "Q6_G"}),
    "R19": (R1W, "10R 1W", (54, 60), 0, {"1": "KA_SW", "2": "Q7_D"}),
    "Q7": (TO251, "2SK4017 KA", (54, 66), 0, {"1": "Q7_G", "2": "Q7_D", "3": "CAM_P"}),
    "R5": (R, "47k", (45.5, 69.5), 0, {"1": "KA_SW", "2": "Q7_G"}),
    "ZD4": (DO35, "9.1V", (62, 70), 0, {"1": "Q7_G", "2": "ZD_MID"}),
    "D6": (DO35, "1N4148", (62, 74), 0, {"1": "GND", "2": "ZD_MID"}),
    # --- mounting ---
    "H1": (HOLE, "M3", (3.5, 3.5), 0, {}),
    "H2": (HOLE, "M3", (96.5, 3.5), 0, {}),
    "H3": (HOLE, "M3", (3.5, 84.5), 0, {}),
    "H4": (HOLE, "M3", (96.5, 84.5), 0, {}),
}

fps = {}
for ref, (src, value, pos, rot, padnets) in PARTS.items():
    if src == "XIAO":
        fp = load_file(XIAO_MOD)
    elif callable(src):
        fp = src()
    else:
        fp = load(*src)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(mm(*pos))
    fp.SetOrientationDegrees(rot)
    board.Add(fp)
    for padnum, netname in padnets.items():
        matched = [p for p in fp.Pads() if p.GetNumber() == padnum]
        assert matched, f"{ref} pad {padnum} missing"
        for p in matched:
            p.SetNet(nets[netname])
    if ref.startswith("H"):
        for p in fp.Pads():
            p.SetLocalClearance(FromMM(0.6))
    fps[ref] = fp


def pad_xy(ref, num, idx=0):
    pads = [p for p in fps[ref].Pads() if p.GetNumber() == num]
    th = [p for p in pads if p.HasHole()] or pads
    c = th[idx].GetPosition()
    return (pcbnew.ToMM(c.x), pcbnew.ToMM(c.y))


# sanity-check XIAO geometry (same footprint as SnowGauge)
p1, p7, p8, p14 = (pad_xy("A1", n) for n in ("1", "7", "8", "14"))
assert (abs(p1[0] - 12.38) < 0.05 and abs(p1[1] - 6.38) < 0.05
        and abs(p7[1] - 21.62) < 0.05 and abs(p14[0] - 27.62) < 0.05
        and abs(p8[0] - 27.62) < 0.05), \
    f"unexpected XIAO geometry: p1={p1} p7={p7} p8={p8} p14={p14}"

if PLACE_ONLY:
    for ref in PARTS:
        for p in fps[ref].Pads():
            c = p.GetPosition()
            print(f"{ref}.{p.GetNumber()} {pcbnew.ToMM(c.x):.2f},{pcbnew.ToMM(c.y):.2f} {p.GetNetname()}")

# ---------------- tracks ----------------
POWER_W, SIG_W = 1.5, 0.6
P = pad_xy


def track(netname, pts, layer=pcbnew.F_Cu, width=SIG_W):
    for a, b in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(mm(*a))
        t.SetEnd(mm(*b))
        t.SetWidth(FromMM(width))
        t.SetLayer(layer)
        t.SetNet(nets[netname])
        board.Add(t)


def via(netname, xy):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(mm(*xy))
    v.SetDrill(FromMM(0.4))
    v.SetWidth(FromMM(0.8))
    v.SetNet(nets[netname])
    board.Add(v)


if not PLACE_ONLY:
    exec(open(os.path.join(HERE, "tracks.py")).read())

# ---------------- board outline ----------------
X0, Y0, X1, Y1 = 0, 0, 100, 88
for a, b in [((X0, Y0), (X1, Y0)), ((X1, Y0), (X1, Y1)),
             ((X1, Y1), (X0, Y1)), ((X0, Y1), (X0, Y0))]:
    seg = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
    seg.SetStart(mm(*a))
    seg.SetEnd(mm(*b))
    seg.SetLayer(pcbnew.Edge_Cuts)
    seg.SetWidth(FromMM(0.1))
    board.Add(seg)

# ---------------- GND zone on B.Cu ----------------
zone = pcbnew.ZONE(board)
zone.SetLayer(pcbnew.B_Cu)
zone.SetNet(nets["GND"])
outline = zone.Outline()
outline.NewOutline()
for x, y in [(X0 + 0.6, Y0 + 0.6), (X1 - 0.6, Y0 + 0.6), (X1 - 0.6, Y1 - 0.6), (X0 + 0.6, Y1 - 0.6)]:
    outline.Append(FromMM(x), FromMM(y))
zone.SetLocalClearance(FromMM(0.3))
zone.SetMinThickness(FromMM(0.3))
zone.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
zone.SetThermalReliefGap(FromMM(0.5))
zone.SetThermalReliefSpokeWidth(FromMM(0.8))
board.Add(zone)


# ---------------- silkscreen labels ----------------
def silk(text, x, y, size=1.2, bold=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(mm(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(VECTOR2I(FromMM(size), FromMM(size)))
    t.SetTextThickness(FromMM(size * (0.2 if bold else 0.15)))
    board.Add(t)


silk("ShutterClock v1.0", 30, 86, 1.6, True)
silk("BAT 12V +  -", 13.5, 73.5, 1.0, True)
silk("CAM +  -", 84.5, 73.5, 1.0, True)
silk("REMOVE JP1 BEFORE USB", 46, 2.5, 0.8, True)
silk("USB", 20, 1.5, 1.0)
silk("U2: SET 9.4V, REMOVE LED", 74, 50.5, 0.9)
silk("JP2=ALWAYS ON", 46, 52.5, 0.8)
silk("RELEASE", 92, 20, 0.9)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.Save(OUT)
print("saved", OUT)
conn = board.GetConnectivity()
print("unconnected items:", conn.GetUnconnectedCount(True))
