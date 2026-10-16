"""Orientation card: what each polarised part looks like and which way it goes on PCB v1.0."""
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyArrow, Polygon, FancyBboxPatch

OUT = sys.argv[1] if len(sys.argv) > 1 else "orientation.png"
fig, axes = plt.subplots(3, 3, figsize=(15, 14), dpi=150)
RED, BLU, GRY, BLK, MET = "#b71c1c", "#1565c0", "#9e9e9e", "#212121", "#b0bec5"

def setup(ax, title):
    ax.set_xlim(0, 10); ax.set_ylim(0, 10); ax.set_aspect("equal"); ax.axis("off")
    ax.set_title(title, fontsize=12, weight="bold", loc="left")

def note(ax, x, y, s, color=BLK, size=9, ha="left", weight="normal"):
    ax.text(x, y, s, fontsize=size, color=color, ha=ha, va="center", weight=weight)

def compass(ax, x=8.6, y=8.8):
    ax.add_patch(FancyArrow(x, y - 0.9, 0, 0.8, width=0.08, head_width=0.35, color=GRY))
    note(ax, x, y + 0.55, "board TOP edge\n(USB, silk 'USB')", GRY, 7, "center")

# (a) TO-220
ax = axes[0][0]; setup(ax, "a. 2SJ334  TO-220  (Q1, Q2, Q5)"); compass(ax)
ax.add_patch(Rectangle((1.5, 5.0), 4.0, 2.0, fc=MET, ec=BLK))            # metal tab
ax.add_patch(Circle((3.5, 6.2), 0.4, fc="white", ec=BLK))
ax.add_patch(Rectangle((1.5, 2.6), 4.0, 2.6, fc=BLK, ec=BLK))            # plastic body
ax.text(3.5, 3.9, "2SJ334", color="white", ha="center", va="center", fontsize=11, weight="bold")
for i, n in enumerate("GDS"):
    ax.add_patch(Rectangle((2.2 + i * 1.2, 0.8), 0.35, 1.8, fc=MET, ec=BLK)); note(ax, 2.37 + i * 1.2, 0.4, n, RED, 11, "center", "bold")
note(ax, 6.0, 6.0, "metal tab with hole\n= BACK  -> faces the TOP edge", BLU, 9)
note(ax, 6.0, 3.9, "black printed face\n= FRONT -> faces the BOTTOM\nedge (J1 / J2 terminals)", BLU, 9)
note(ax, 6.0, 1.6, "front view, legs down:\nleft to right  G - D - S\n(G = square pad on the board)", BLK, 9)

# (b) TO-251
ax = axes[0][1]; setup(ax, "b. 2SK4017  TO-251  (Q3, Q6, Q7)"); compass(ax)
ax.add_patch(Rectangle((2.2, 5.0), 2.6, 1.0, fc=MET, ec=BLK))
ax.add_patch(Rectangle((2.2, 2.8), 2.6, 2.2, fc=BLK, ec=BLK))
ax.text(3.5, 3.9, "2SK4017", color="white", ha="center", va="center", fontsize=9, weight="bold")
for i, n in enumerate("GDS"):
    ax.add_patch(Rectangle((2.6 + i * 0.85, 1.0), 0.3, 1.8, fc=MET, ec=BLK)); note(ax, 2.75 + i * 0.85, 0.6, n, RED, 11, "center", "bold")
note(ax, 5.6, 5.5, "small metal tab on the back\n-> faces the TOP edge", BLU, 9)
note(ax, 5.6, 3.9, "printed face -> faces the\nBOTTOM edge", BLU, 9)
note(ax, 5.6, 1.6, "same rule as TO-220:\nG - D - S left to right\nseen from the printed face", BLK, 9)

# (c) SOT-89
ax = axes[0][2]; setup(ax, "c. NJW4181U3-33B  SOT-89  (U1, SMD)"); compass(ax)
ax.add_patch(Rectangle((3.0, 3.0), 3.0, 4.0, fc=BLK, ec=BLK))
ax.text(4.5, 5.0, "4181\n33B", color="white", ha="center", va="center", fontsize=10, weight="bold", rotation=90)
for i, (yy, n) in enumerate(((6.3, "1 VOUT"), (5.0, "2 GND"), (3.7, "3 VIN"))):
    ax.add_patch(Rectangle((1.6, yy - 0.25), 1.4, 0.5, fc=MET, ec=BLK)); note(ax, 1.4, yy, n, RED, 9, "right", "bold")
ax.add_patch(Rectangle((6.0, 4.0), 1.2, 2.0, fc=MET, ec=BLK)); note(ax, 7.4, 5.0, "wide metal tab\n(underside / right)", BLU, 9)
note(ax, 0.3, 1.6, "top view, printed side UP.  Three leads -> LEFT (toward the XIAO),\ntab -> the big pad on the right.  With the leads on the left,\nthe TOP lead is pin 1 (VOUT).  It cannot be mounted upside down;\nthe only mistake is rotating it 180 deg (tab on the small pads).", BLK, 8.5)

# (d) DIP-4
ax = axes[1][0]; setup(ax, "d. PC817  DIP-4  (U3, U4)"); compass(ax)
ax.add_patch(Rectangle((2.5, 3.0), 5.0, 4.0, fc="#eeeeee", ec=BLK))
ax.add_patch(Circle((3.2, 6.3), 0.3, fc=BLK)); ax.text(5.0, 5.0, "PC817", ha="center", va="center", fontsize=11)
for x, ys, ns in ((2.5, (6.0, 4.0), ("1", "2")), (7.5, (6.0, 4.0), ("4", "3"))):
    for yy, n in zip(ys, ns):
        ax.add_patch(Rectangle((x - 0.9 if x < 5 else x, yy - 0.2), 0.9, 0.4, fc=MET, ec=BLK))
        note(ax, x - 1.2 if x < 5 else x + 1.2, yy, n, RED, 10, "center", "bold")
note(ax, 0.3, 1.9, "dot (or notch) = pin 1 corner -> TOP-LEFT on the board\n(dot toward the top edge, on the column nearer the XIAO).\npin 1 = LED anode (from R6/R7), 2 = LED cathode (GND),\n3 = emitter (J3 sleeve), 4 = collector (R20/R21).", BLK, 8.5)

# (e) axial diodes
ax = axes[1][1]; setup(ax, "e. Diodes: band = cathode K"); compass(ax)
for yy, w, h, col, band, lab in ((7.6, 2.2, 0.8, "#ffcc80", BLK, "glass DO-35: ZD1-ZD4 (15 V / 9.1 V), D6 1N4148"),
                                 (5.4, 3.0, 1.2, BLK, "#eeeeee", "black DO-41: D1, D2 P4KE15A (TVS)"),
                                 (2.8, 4.0, 1.7, BLK, "#eeeeee", "big DO-201: D4 1N5822 (Schottky)")):
    ax.plot([0.6, 8.0], [yy, yy], color=MET, lw=3)
    ax.add_patch(Rectangle((2.0, yy - h / 2), w, h, fc=col, ec=BLK))
    ax.add_patch(Rectangle((2.0 + w - 0.35, yy - h / 2), 0.35, h, fc=band, ec=BLK))
    note(ax, 2.0 + w - 0.17, yy + h / 2 + 0.35, "K", RED, 11, "center", "bold")
    note(ax, 0.6, yy - h / 2 - 0.4, lab, BLK, 8.5)
note(ax, 0.3, 0.8, "Put the band on the end marked 'K' on the silk (= square pad).\nZD2 stands vertically: band DOWN.  D1, D2: band UP (both).", BLK, 8.5)

# (f) electrolytic
ax = axes[1][2]; setup(ax, "f. Electrolytic capacitors (C1, C3)"); compass(ax)
ax.add_patch(Rectangle((2.5, 4.0), 3.0, 4.5, fc="#1a237e", ec=BLK))
ax.add_patch(Rectangle((2.5, 4.0), 0.6, 4.5, fc="#eeeeee", ec=BLK)); ax.text(2.8, 6.2, "-", fontsize=14, ha="center", va="center", weight="bold")
ax.plot([3.3, 3.3], [4.0, 2.6], color=MET, lw=3); ax.plot([4.7, 4.7], [4.0, 1.2], color=MET, lw=3)
note(ax, 3.3, 2.2, "short = -", RED, 9, "center", "bold"); note(ax, 4.7, 0.8, "long = +", RED, 9, "center", "bold")
note(ax, 6.0, 7.5, "white stripe on the can = MINUS side", BLU, 9)
note(ax, 6.0, 5.6, "C3 4700u: long leg (+) into the LEFT\nsquare pad (silk + is correct)", BLK, 9)
note(ax, 6.0, 3.6, "C1 100u: long leg (+) into the RIGHT\nround pad.  The silk '+' next to the\nsquare pad is WRONG on v1.0", RED, 9, weight="bold")

# (g) XIAO + U2 module
ax = axes[2][0]; setup(ax, "g. XIAO (A1) and buck module (U2)"); compass(ax)
ax.add_patch(Rectangle((1.0, 4.5), 3.0, 5.0, fc="#263238", ec=BLK)); ax.add_patch(Rectangle((1.9, 9.3), 1.2, 0.6, fc=MET, ec=BLK))
ax.text(2.5, 6.8, "XIAO\nchip side\nUP", color="white", ha="center", va="center", fontsize=8)
note(ax, 2.5, 4.0, "USB-C -> TOP edge", BLU, 8.5, "center", "bold")
ax.add_patch(Rectangle((4.8, 0.6), 5.0, 2.7, fc="#1b5e20", ec=BLK))
for x, yy, n in ((5.2, 2.9, "IN+"), (5.2, 1.0, "IN-"), (9.4, 2.9, "OUT+"), (9.4, 1.0, "OUT-")):
    ax.add_patch(Circle((x, yy), 0.18, fc="white", ec=BLK)); ax.text(x + (0.3 if x < 7 else -0.3), yy, n, color="white", fontsize=7, ha="left" if x < 7 else "right", va="center", weight="bold")
ax.add_patch(Rectangle((6.2, 1.2), 0.8, 0.8, fc="#1565c0", ec=BLK)); ax.text(6.6, 1.6, "pot", color="white", fontsize=6, ha="center", va="center")
ax.add_patch(Circle((7.3, 2.1), 0.2, fc="red", ec=BLK)); note(ax, 7.3, 2.65, "LED: remove", RED, 7, "center", "bold")
note(ax, 4.8, 4.4, "U2: parts side UP.\nIN+ / IN- holes on the LEFT (toward Q2),\nOUT+ / OUT- on the RIGHT (toward C3).\nRead the print on the module.", BLK, 8)

# (h) terminals, jack, fuse clips
ax = axes[2][1]; setup(ax, "h. Terminals (J1, J2), jack (J3), fuse clips (F1)"); compass(ax)
ax.add_patch(Rectangle((1.0, 5.5), 3.2, 2.2, fc="#2e7d32", ec=BLK))
for x in (1.8, 3.4):
    ax.add_patch(Circle((x, 7.0), 0.35, fc="white", ec=BLK)); ax.add_patch(Rectangle((x - 0.4, 5.5), 0.8, 0.6, fc=BLK))
note(ax, 2.6, 4.7, "wire openings -> BOTTOM edge\n(off the board)\nleft = +, right = -", BLU, 8, "center")
ax.add_patch(Rectangle((5.5, 6.0), 3.2, 1.6, fc="#546e7a", ec=BLK)); ax.add_patch(Rectangle((8.7, 6.4), 0.5, 0.8, fc=BLK))
note(ax, 5.5, 5.3, "J3: plug opening\n-> RIGHT edge", BLU, 8)
for x in (2.0, 5.0):
    ax.add_patch(Polygon([(x, 1.2), (x + 0.6, 1.2), (x + 0.6, 2.8), (x + 0.3, 3.2), (x, 2.8)], fc=MET, ec=BLK))
ax.add_patch(Rectangle((2.6, 1.6), 2.4, 0.9, fc="#e0e0e0", ec=BLK))
note(ax, 1.5, 0.6, "F1: two clips facing each other; if they have a stop tab, tabs OUTWARD", BLK, 8)

# (i) summary
ax = axes[2][2]; setup(ax, "i. Rule of thumb")
note(ax, 0.3, 8.8, "Board orientation: silk 'USB' at the top,\n'ShutterClock v1.0' at the bottom.", BLK, 9, weight="bold")
note(ax, 0.3, 7.4, "- Square pad = pin 1 / K / +  (exception: C1, see f)", BLK, 9)
note(ax, 0.3, 6.5, "- All six MOSFETs: metal tab to the TOP edge, print to the BOTTOM edge", BLK, 9)
note(ax, 0.3, 5.6, "- All diodes: band on the 'K' silk / square pad", BLK, 9)
note(ax, 0.3, 4.7, "- PC817 dot at the top-left; U1 leads to the left", BLK, 9)
note(ax, 0.3, 3.8, "- Connectors open toward the nearest board edge", BLK, 9)
note(ax, 0.3, 2.9, "- No orientation: resistors, ceramic caps, pin headers / sockets, SW1", BLK, 9)
note(ax, 0.3, 1.7, "Check twice, solder once: a reversed C1 or MOSFET is the\nmost likely cause of a large current at power-up check #1.", RED, 9, weight="bold")
fig.tight_layout()
fig.savefig(OUT, dpi=150, facecolor="white"); print("saved", OUT)
