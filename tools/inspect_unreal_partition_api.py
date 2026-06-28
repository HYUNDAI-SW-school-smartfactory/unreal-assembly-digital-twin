import json
import unreal

OUT_PATH = "C:/Users/한국전파진흥협회/unreal-assembly-digital-twin/artifacts/unreal_partition_api.json"

names = sorted([name for name in dir(unreal) if "Partition" in name or "Actor" in name and "Subsystem" in name])
subsystems = []
for name in names:
    obj = getattr(unreal, name, None)
    if obj:
        subsystems.append({
            "name": name,
            "repr": str(obj),
            "methods": [m for m in dir(obj) if "load" in m.lower() or "actor" in m.lower() or "world" in m.lower()],
        })

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(subsystems, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
