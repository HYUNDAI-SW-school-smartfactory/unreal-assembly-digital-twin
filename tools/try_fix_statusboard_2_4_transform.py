import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "try_fix_statusboard_2_4_transform.json")


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot(r):
    return [float(r.roll), float(r.pitch), float(r.yaw)]


def find_actor(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == label:
            return actor
    return None


def state(actor):
    return {
        "actor_location": vec(actor.get_actor_location()),
        "actor_rotation": rot(actor.get_actor_rotation()),
        "actor_scale": vec(actor.get_actor_scale3d()),
        "actor_transform": str(actor.get_actor_transform()),
    }


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

labels = {
    "GenesisFactoryStatusBoard_2_3",
    "GenesisFactoryStatusBoard_2_4",
    "GenesisFactoryStatusBoard_2_5",
}
guids = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    if str(desc.label) in labels:
        guids.append(desc.guid)
if guids:
    unreal.WorldPartitionBlueprintLibrary.load_actors(guids)

board23 = find_actor("GenesisFactoryStatusBoard_2_3")
board24 = find_actor("GenesisFactoryStatusBoard_2_4")
board25 = find_actor("GenesisFactoryStatusBoard_2_5")

result = {"found": bool(board23 and board24 and board25), "attempts": []}

if board23 and board24 and board25:
    target_location = (board23.get_actor_location() + board25.get_actor_location()) * 0.5
    target_rotation = board23.get_actor_rotation()
    target_scale = board23.get_actor_scale3d()
    target_transform = unreal.Transform(target_location, target_rotation, target_scale)

    result["target"] = {
        "location": vec(target_location),
        "rotation": rot(target_rotation),
        "scale": vec(target_scale),
    }
    result["before"] = state(board24)

    board24.modify()
    try:
        ok = board24.set_actor_transform(target_transform, False, True)
        result["attempts"].append({"api": "set_actor_transform", "ok": bool(ok), "state": state(board24)})
    except Exception as exc:
        result["attempts"].append({"api": "set_actor_transform", "error": str(exc), "state": state(board24)})

    try:
        ok = board24.set_actor_location_and_rotation(target_location, target_rotation, False, True)
        board24.set_actor_scale3d(target_scale)
        result["attempts"].append({"api": "set_actor_location_and_rotation", "ok": bool(ok), "state": state(board24)})
    except Exception as exc:
        result["attempts"].append({"api": "set_actor_location_and_rotation", "error": str(exc), "state": state(board24)})

    try:
        root = board24.root_component
        root.modify()
        root.set_world_location(target_location, False, True)
        root.set_world_rotation(target_rotation, False, True)
        root.set_world_scale3d(target_scale)
        result["attempts"].append({"api": "root_component_world", "state": state(board24)})
    except Exception as exc:
        result["attempts"].append({"api": "root_component_world", "error": str(exc), "state": state(board24)})

    result["after"] = state(board24)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
