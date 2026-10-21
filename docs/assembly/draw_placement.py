"""Draw the ShutterClock v1.0 placement map (top view) for the assembly guide."""
import json, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle

d = json.load(open(sys.argv[1]))
OUT = sys.argv[2]
VER = sys.argv[3] if len(sys.argv) > 3 else "v1.0"
fps = {f["ref"]: f for f in d["fps"]}

STEP = {  # soldering step -> (colour, refs)
    1: ("#e53935", ["U1"]),
    2: ("#fdd835", [r for r in fps if r.startswith("R")]),
    3: ("#fb8c00", ["D1", "D2", "D4", "D6", "D7", "ZD1", "ZD2", "ZD3", "ZD4"]),
    4: ("#8d6e63", ["C2", "C4", "C6", "C7", "C8"]),
    5: ("#43a047", ["JP1", "JP2", "A1", "U3", "U4", "SW1"]),
    6: ("#8e24aa", ["Q1", "Q2", "Q3", "Q5", "Q6", "Q7"]),
    7: ("#1e88e5", ["C1", "C3", "F1", "J1", "J2", "J3", "BT1"]),
    8: ("#757575", ["U2", "U5"]),
}
STEP_NAME = {1: "U1 (SMD LDO)", 2: "Resistors", 3: "Diodes (band = K)", 4: "Ceramic caps",
             5: "Headers / sockets / DIP / switch", 6: "MOSFETs (G-D-S)", 7: "Electrolytics / fuse clips / terminals / jack",
             8: "U2 module (on pins, set 9.4 V, LED removed); U5 RTC module (v1.2)"}
step_of = {r: s for s, (_, refs) in STEP.items() for r in refs}

W, H = 100.0, (100.0 if VER >= "v1.2" else 88.0)
fig = plt.figure(figsize=(14, 14 * (H + 14) / W), dpi=170)
ax = fig.add_axes([0.02, 0.02, 0.96, 0.96])
ax.set_xlim(-2, W + 2); ax.set_ylim(H + 13, -2); ax.set_aspect("equal"); ax.axis("off")
ax.add_patch(Rectangle((0, 0), W, H, fc="#1b5e20", ec="black", lw=1.5, alpha=0.15))

def txt(x, y, s, size=7, color="black", weight="normal", ha="center", va="center", rot=0, bg=None):
    kw = dict(fontsize=size, color=color, weight=weight, ha=ha, va=va, rotation=rot, zorder=6)
    if bg:
        kw["bbox"] = dict(boxstyle="round,pad=0.12", fc=bg, ec="none", alpha=0.85)
    ax.text(x, y, s, **kw)

for ref, f in fps.items():
    if ref.startswith("H"):
        x, y = f["x"], f["y"]
        ax.add_patch(Circle((x, y), 1.6, fc="white", ec="black", lw=0.8))
        txt(x, y - 3.2, ref, 6, "#444")
        continue
    s = step_of.get(ref)
    col = STEP[s][0] if s else "#999"
    x0, y0, x1, y1 = f["cb"] or f["bb"]
    ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, fc=col, ec=col, lw=1.2, alpha=0.35, zorder=2))
    for p in f["pads"]:
        if not p["th"]:
            ax.add_patch(Rectangle((p["x"] - p["w"] / 2, p["y"] - p["h"] / 2), p["w"], p["h"], fc="#ffd54f", ec="#795548", lw=0.5, zorder=3))
            continue
        first = p["num"] in ("1", "INP") and p["shape"] in (1, 4)  # square pad = pin 1
        if first:
            ax.add_patch(Rectangle((p["x"] - p["w"] / 2, p["y"] - p["h"] / 2), p["w"], p["h"], fc="white", ec="black", lw=0.8, zorder=3))
        else:
            ax.add_patch(Circle((p["x"], p["y"]), p["w"] / 2, fc="white", ec="black", lw=0.6, zorder=3))

# --- reference / value labels ---
def label(ref, dx=0, dy=0, size=7, two_line=False, rot=0):
    f = fps[ref]; x0, y0, x1, y1 = f["cb"] or f["bb"]
    cx, cy = (x0 + x1) / 2 + dx, (y0 + y1) / 2 + dy
    val = f["val"]
    if two_line:
        txt(cx, cy - 1.3, ref, size, weight="bold", rot=rot); txt(cx, cy + 1.3, val, size - 1, rot=rot)
    else:
        txt(cx, cy, f"{ref} {val}", size, weight="bold", rot=rot)

for ref in fps:
    if ref.startswith("H"):
        continue
    f = fps[ref]
    if ref.startswith("R") and ref not in ("R3", "R19", "R17"):
        label(ref, size=6.2)
    elif ref in ("R19",):
        label(ref, size=6.5)
    elif ref == "R17":
        label(ref, size=6.2, rot=90)
    elif ref == "R3":
        label(ref, size=7.5)
    elif ref.startswith("ZD") or ref in ("D6", "D7"):
        label(ref, size=6, dy=(0 if f["rot"] == 0 else 0), rot=(90 if f["rot"] == 90 else 0))
    elif ref in ("D1", "D2"):
        label(ref, size=6.5, rot=90)
    elif ref == "D4":
        label(ref, size=7.5)
    elif ref.startswith("C") and ref not in ("C1", "C3"):
        label(ref, size=6.2)
    elif ref in ("C1", "C3"):
        label(ref, size=8, two_line=True)
    elif ref in ("Q1", "Q2", "Q5"):
        txt((f["cb"][0] + f["cb"][2]) / 2, f["cb"][1] + 1.3, f"{ref} {f['val']}", 7, weight="bold")
    elif ref in ("Q3", "Q6", "Q7"):
        txt((f["cb"][0] + f["cb"][2]) / 2, f["cb"][1] - 1.1, f"{ref} {f['val']}", 6.5, weight="bold")
    elif ref in ("U3", "U4"):
        label(ref, size=6.8, two_line=True)
    elif ref == "U1":
        txt(f["cb"][2] + 0.6, (f["cb"][1] + f["cb"][3]) / 2, "U1 NJW4181\n(SOT-89)", 6.5, weight="bold", ha="left")
    elif ref == "U5":
        txt(f["x"], f["y"], "U5 AE-RX8900 (RTC)\npins 5-8 top: SDA SCL VBAT VDD", 6.5, weight="bold")
    elif ref == "BT1":
        txt(f["x"], f["y"] - 0.3, "BT1 CR2032 vertical holder\ncentre pin = + (verify)", 7, weight="bold")
    elif ref == "U2":
        txt(f["x"], f["y"], "U2  MBC2596-01 module" + (" (v1.0 holes do not fit: use bent tinned wire)" if VER == "v1.0" else " (on 4 single pins)") + "\nset to 9.4 V before fitting, remove the red LED\nIN side left, OUT side right", 9, weight="bold")
    elif ref == "A1":
        txt(f["x"], f["y"] + 1, "A1  XIAO nRF52840\n(2 x 1x7 sockets)\nUSB connector to the\ntop edge", 8, weight="bold")
    elif ref in ("J1", "J2"):
        txt(f["x"], f["y"] + 2.2, f"{ref}\n{f['val']}", 7.5, weight="bold")
    elif ref == "J3":
        txt(f["x"] + 1, f["y"] - 5.2, "J3 3.5 mm jack\nopening to right edge", 6.8, weight="bold")
    elif ref == "F1":
        txt(f["x"], f["y"], "F1\n5 A\n5x20\nfuse\nclips", 7, weight="bold")
    elif ref == "SW1":
        txt(f["x"], f["y"] - 5.3, "SW1 TEST", 6.5, weight="bold")
    elif ref in ("JP1", "JP2"):
        txt(f["x"] + 2.6, f["y"], f"{ref}", 6.5, weight="bold", ha="left")

# --- polarity / pin annotations ---
def pad(ref, num, idx=0):
    ps = [p for p in fps[ref]["pads"] if p["num"] == num and p["th"]]
    return ps[idx]["x"], ps[idx]["y"]

for ref in ("D1", "D2", "D4", "D6", "D7", "ZD1", "ZD2", "ZD3", "ZD4"):
    if ref not in fps: continue
    t = [t for t in fps[ref]["texts"] if t["t"] == "K" and t["layer"] == "F.Silkscreen"][0]
    txt(t["x"], t["y"], "K", 7, "#b71c1c", "bold", bg="white")
for ref, num in (("Q1", "G"), ):
    pass
for ref in ("Q1", "Q2", "Q5", "Q3", "Q6", "Q7"):
    for num, name in (("1", "G"), ("2", "D"), ("3", "S")):
        x, y = pad(ref, num); txt(x, y + 1.9, name, 6, "#4a148c", "bold")
# electrolytics: + on the pad that carries the positive net (NOT the silk for C1)
x, y = pad("C3", "1"); txt(x, y - 2.0, "+", 11, "#b71c1c", "bold")
if VER == "v1.0":
    x, y = pad("C1", "2"); txt(x, y - 2.0, "+", 11, "#b71c1c", "bold", bg="white")
    x, y = pad("C1", "1"); txt(x, y - 2.0, "-", 11, "#b71c1c", "bold", bg="white")
    txt(16.5, 61.8, "C1: + is the RIGHT pad\n(silk + is wrong on v1.0)", 6.3, "#b71c1c", "bold", bg="#fff3e0")
else:
    x, y = pad("C1", "1"); txt(x, y - 2.0, "+", 11, "#b71c1c", "bold")
# DIP pin 1
for ref in ("U3", "U4"):
    x, y = pad(ref, "1"); ax.add_patch(Circle((x - 1.5, y - 1.5), 0.45, fc="#b71c1c", ec="none", zorder=7))
    txt(x - 2.6, y - 1.7, "1", 6, "#b71c1c", "bold")
# U1 pins
for num, name in (("1", "1 VOUT"), ("2", "2 GND"), ("3", "3 VIN")):
    p = [p for p in fps["U1"]["pads"] if p["num"] == num][0]
    txt(p["x"] - 1.0, p["y"], name, 5.2, "#b71c1c", "bold", ha="right")
# terminals, jack, jumpers, module
for ref, a, b in (("J1", "+", "-"), ("J2", "+", "-")):
    x, y = pad(ref, "1"); txt(x, y - 4.2, a, 11, "#b71c1c", "bold")
    x, y = pad(ref, "2"); txt(x, y - 4.2, b, 11, "#b71c1c", "bold")
txt(13.5, 73.5, "BAT 12V", 7, "#333", "bold"); txt(84.5, 73.5, "CAM (EP-5B)", 7, "#333", "bold")
x, y = pad("J3", "1"); txt(x, y - 2.3, "S", 6.5, "#b71c1c", "bold")
x, y = pad("J3", "3"); txt(x, y + 2.3, "R", 6.5, "#b71c1c", "bold")
x, y = pad("J3", "2", 0); txt(x + 2.4, y, "T", 6.5, "#b71c1c", "bold")
for num, name in (("1", "3V3_LDO"), ("2", "3V3_SYS")):
    x, y = pad("JP1", num); txt(x - 1.4, y, name, 5.2, "#333", ha="right")
for num, name in (("1", "Q3 D"), ("2", "GND")):
    x, y = pad("JP2", num); txt(x - 1.4, y, name, 5.2, "#333", ha="right")
for num, name, dx in (("INP", "IN+", 3.4), ("INN", "IN-", 3.4), ("OUTP", "OUT+", -3.6), ("OUTN", "OUT-", -3.6)):
    p = [p for p in fps["U2"]["pads"] if p["num"] == num][0]
    txt(p["x"] + dx, p["y"], name, 8, "#b71c1c", "bold")
# XIAO pin names at the sockets
XIAO = {"1": "A0 ADC", "2": "A1 Isense", "3": "D2 AF", "4": "D3", "5": "D4", "6": "D5", "7": "D6",
        "8": "D7 CAM_EN", "9": "D8 SHUTTER", "10": "D9 BTN", "11": "D10 KA_EN", "12": "3V3", "13": "GND", "14": "5V"}
for num, name in XIAO.items():
    x, y = pad("A1", num)
    if int(num) <= 7:
        txt(x - 1.6, y, name, 5.2, "#333", ha="right")
    else:
        txt(x + 1.6, y, name, 5.2, "#333", ha="left")
txt(20, 2.6, "USB", 7, "#333", "bold")

# --- legend ---
ly = H + 2.5
txt(0, ly, f"ShutterClock PCB {VER} - top view, soldering order (low -> high)", 10, weight="bold", ha="left")
for s, (col, refs) in STEP.items():
    yy = ly + 2.2 + (s - 1) * 1.25
    ax.add_patch(Rectangle((0.5, yy - 0.45), 2.2, 0.9, fc=col, ec="black", lw=0.3, alpha=0.7))
    txt(3.5, yy, f"{s}. {STEP_NAME[s]}", 7.5, ha="left")
txt(55, ly + 2.2, "Square pad = pin 1 / cathode (K) / positive (+).  MOSFETs: metal tab toward the TOP edge, printed face toward the bottom edge, pins G-D-S.", 7, ha="left")
txt(55, ly + 3.45, "K = cathode band.  Electrolytics: + on the pad marked + here" + (" (C1: the right pad, silk is wrong)." if VER == "v1.0" else "."), 7, ha="left")
txt(55, ly + 4.7, "PC817: dot / pin 1 at the top-left corner.  U1: three leads to the left, tab on the wide pad.", 7, ha="left")
txt(55, ly + 5.95, "JP1: fitted normally, REMOVE BEFORE USB.  JP2: fitted = camera always on.", 7, ha="left")
txt(55, ly + 7.2, "J3: T = tip (SHUTTER), R = ring (AF), S = sleeve (common).", 7, ha="left")
fig.savefig(OUT, dpi=170, facecolor="white")
print("saved", OUT)
