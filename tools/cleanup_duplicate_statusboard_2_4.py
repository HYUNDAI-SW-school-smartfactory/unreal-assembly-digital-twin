import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
LABEL = "GenesisFactoryStatusBoard_2_4"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "cleanup_duplicate_statusboard_2_4.json")


def box_center_x(box):
    return (float(box.min.x) + float(box.max.x)) * 0.5


def box_center_y(box):
    return (float(box.min.y) + float(box.max.y)) * 0.5


def is_good_board_desc(desc):
    bounds = desc.bounds
    return box_center_x(bounds) > 10000.0 and box_center_y(bounds) > 6000.0 and float(bounds.max.z) > 900.0


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

records = []
delete_packages = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    if str(desc.label) != LABEL:
        continue

    keep = is_good_board_desc(desc)
    package = str(desc.actor_package)
    record = {
        "label": str(desc.label),
        "name": str(desc.name),
        "actor_package": package,
        "bounds": str(desc.bounds),
        "keep": keep,
    }
    records.append(record)
    if not keep:
        delete_packages.append(package)

deleted = []
failed = []
for package in delete_packages:
    try:
        ok = unreal.EditorAssetLibrary.delete_asset(package)
        (deleted if ok else failed).append(package)
    except Exception as exc:
        failed.append(f"{package}: {exc}")

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(
        {
            "before_count": len(records),
            "records": records,
            "deleted": deleted,
            "failed": failed,
        },
        f,
        ensure_ascii=False,
        indent=2,
    )

print(f"Wrote {OUT_PATH}")
