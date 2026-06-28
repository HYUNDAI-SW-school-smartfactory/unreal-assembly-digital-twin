import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "mainmaps_factory_actor_descs.json",
    )
)

KEYWORDS = [
    "statusboard",
    "factorystatus",
    "agv",
    "batterylift",
    "tireassembly",
    "tirecell",
    "tireconveyor",
    "robottirepicker",
    "robot",
    "robo",
    "arm",
    "welding",
    "genesisfactoryassemblymanager",
]


def call(obj, name, default=""):
    attr = getattr(obj, name, None)
    if not attr:
        return default
    try:
        return str(attr() if callable(attr) else attr)
    except Exception as exc:
        return f"<ERR {exc}>"


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_editor.load_level(MAP_PATH)

records = []
descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs() or []
for desc in descs:
    text = str(desc)
    search_text = text.lower()
    if not any(keyword in search_text for keyword in KEYWORDS):
        continue

    record = {
        "desc": text,
        "actor_label": call(desc, "get_actor_label"),
        "actor_name": call(desc, "get_actor_name"),
        "actor_package": call(desc, "get_actor_package"),
        "actor_path": call(desc, "get_actor_path"),
        "actor_class": call(desc, "get_actor_class"),
        "actor_native_class": call(desc, "get_actor_native_class"),
        "guid": call(desc, "get_guid"),
        "bounds": call(desc, "get_bounds"),
        "transform": call(desc, "get_transform"),
    }
    records.append(record)

records.sort(key=lambda item: (item.get("actor_label") or item.get("actor_name") or item["desc"]).lower())

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(
        {
            "map": MAP_PATH,
            "desc_count": len(descs),
            "matched_count": len(records),
            "records": records,
        },
        output_file,
        ensure_ascii=False,
        indent=2,
    )

unreal.log(f"Factory actor descriptors written to {OUTPUT_PATH}")
