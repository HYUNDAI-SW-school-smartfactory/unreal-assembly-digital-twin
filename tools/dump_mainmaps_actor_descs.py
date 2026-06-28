import json
import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUT_PATH = "C:/Users/한국전파진흥협회/unreal-assembly-digital-twin/artifacts/mainmaps_actor_descs_statusboard.json"


def safe_call(obj, method_name):
    try:
        method = getattr(obj, method_name)
        return str(method())
    except Exception as exc:
        return f"<ERR {exc}>"


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
records = []
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
except Exception as exc:
    descs = []
    records.append({"error": f"get_actor_descs failed: {exc}"})

for desc in descs:
    methods = [m for m in dir(desc) if "actor" in m.lower() or "label" in m.lower() or "name" in m.lower() or "class" in m.lower() or "path" in m.lower()]
    rec = {
        "repr": str(desc),
        "methods": methods,
    }
    for method_name in methods:
        if method_name.startswith("_"):
            continue
        value = safe_call(desc, method_name)
        rec[method_name] = value
    text = json.dumps(rec, ensure_ascii=False)
    if "StatusBoard" in text or "GenesisFactoryStatusBoard" in text or "DisplayBoard" in text or "MQTT" in text:
        records.append(rec)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(records, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH} ({len(records)} matching descs)")
