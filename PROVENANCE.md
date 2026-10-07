# Provenance

Files copied from [SnowGauge](https://github.com/0kam/SnowGauge).
Fork point: commit `36e21f8` (FW `fw-2026-09-07b`).

When a bug is fixed on either side, check this table and port the fix by hand.
Add a row whenever a file is copied; update "Changes" when it diverges.

| ShutterClock path | SnowGauge source path | SnowGauge commit | Changes |
|---|---|---|---|
| `pcb/lib/XIAO-nRF52840-DIP.kicad_mod` | `pcb/lib/XIAO-nRF52840-DIP.kicad_mod` | `36e21f8` (file unchanged since `b0c9c5e`) | none — same XIAO footprint (14 through-hole pads, 15.24 mm rows) |
| `pcb/generate_board.py` (structure) | `pcb/generate_board.py` | `36e21f8` | rewritten for the ShutterClock netlist; same pcbnew approach (nets → footprints → tracks → outline → GND pour → silk) |
