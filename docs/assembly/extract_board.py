import json, pcbnew
b = pcbnew.LoadBoard("/Users/okamoto/NIES/Camera/ShutterClock/pcb/ShutterClock.kicad_pcb")
mm = pcbnew.ToMM
out = []
for fp in b.GetFootprints():
    bb = fp.GetBoundingBox(False, False)  # no text, no invisible
    cb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox() if fp.GetCourtyard(pcbnew.F_CrtYd).OutlineCount() else None
    pads = []
    for p in fp.Pads():
        pos = p.GetPosition(); sz = p.GetSize()
        pads.append({"num": p.GetNumber(), "x": round(mm(pos.x), 2), "y": round(mm(pos.y), 2),
                     "net": p.GetNetname(), "shape": int(p.GetShape()), "w": round(mm(sz.x), 2), "h": round(mm(sz.y), 2),
                     "th": p.HasHole()})
    texts = []
    for it in fp.GraphicalItems():
        if it.GetClass() == "PCB_TEXT" or isinstance(it, pcbnew.PCB_TEXT):
            pos = it.GetPosition(); texts.append({"t": it.GetText(), "x": round(mm(pos.x), 2), "y": round(mm(pos.y), 2), "layer": it.GetLayerName()})
    out.append({"ref": fp.GetReference(), "val": fp.GetValue(), "x": round(mm(fp.GetPosition().x), 2), "y": round(mm(fp.GetPosition().y), 2),
                "rot": fp.GetOrientationDegrees(), "fpid": fp.GetFPIDAsString(),
                "bb": [round(mm(bb.GetLeft()), 2), round(mm(bb.GetTop()), 2), round(mm(bb.GetRight()), 2), round(mm(bb.GetBottom()), 2)],
                "cb": [round(mm(cb.GetLeft()), 2), round(mm(cb.GetTop()), 2), round(mm(cb.GetRight()), 2), round(mm(cb.GetBottom()), 2)] if cb else None,
                "pads": pads, "texts": texts})
silk = []
for d in b.GetDrawings():
    if isinstance(d, pcbnew.PCB_TEXT) and d.GetLayerName() == "F.SilkS":
        pos = d.GetPosition(); silk.append({"t": d.GetText(), "x": round(mm(pos.x), 2), "y": round(mm(pos.y), 2)})
json.dump({"fps": out, "silk": silk}, open("/private/tmp/claude-501/-Users-okamoto-NIES-Camera-ShutterClock/b00f817a-e89e-487a-ba3e-000a6d1051c6/scratchpad/asm/board.json", "w"), ensure_ascii=False, indent=1)
print(len(out), "footprints")
