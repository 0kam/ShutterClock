import json, sys
from collections import Counter
d = json.load(open("drc.json"))
v = [x for x in d.get("violations", []) if x["severity"] == "error"]
w = [x for x in d.get("violations", []) if x["severity"] != "error"]
u = d.get("unconnected_items", [])
print("errors", len(v), "warnings", len(w), "unconnected", len(u))
print(Counter(x["type"] for x in v))
for x in v[:50]:
    items = "; ".join(i.get("description", "")[:55] for i in x.get("items", []))
    pos = x["items"][0].get("pos", {}) if x.get("items") else {}
    print(f'- {x["type"]:22s} ({pos.get("x","?")},{pos.get("y","?")}) {x["description"][:60]} | {items}')
for x in u[:20]:
    items = "; ".join(i.get("description", "")[:50] for i in x.get("items", []))
    print("U", items)
