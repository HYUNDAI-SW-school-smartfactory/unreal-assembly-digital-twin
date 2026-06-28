import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
LABEL = "GenesisFactoryStatusBoard_2_4"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "count_statusboard_2_4.json")


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

records = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    if str(desc.label) == LABEL:
        records.append(
            {
                "label": str(desc.label),
                "name": str(desc.name),
                "guid": str(desc.guid),
                "actor_package": str(desc.actor_package),
                "native_class": str(desc.native_class),
                "bounds": str(desc.bounds),
            }
        )

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump({"count": len(records), "records": records}, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
