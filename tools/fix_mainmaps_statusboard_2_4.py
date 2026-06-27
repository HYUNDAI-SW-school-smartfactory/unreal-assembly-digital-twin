import json
import os
import unreal


MAP_PATH = "/Game/Maps/MainMaps"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "fix_statusboard_2_4.json")


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def find_actor(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == label:
            return actor
    return None


def vec_tuple(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot_tuple(r):
    return [float(r.roll), float(r.pitch), float(r.yaw)]


def apply_actor_transform(actor, location, rotation, scale):
    moved = actor.set_actor_location(location, False, True)
    rotated = actor.set_actor_rotation(rotation, True)
    actor.set_actor_scale3d(scale)

    return bool(moved), bool(rotated)


def component_snapshot(actor):
    items = []
    for comp in actor.get_components_by_class(unreal.SceneComponent):
        item = {"name": comp.get_name(), "class": str(comp.get_class().get_path_name())}
        try:
            item["relative_location"] = vec_tuple(comp.get_relative_location())
            item["world_location"] = vec_tuple(comp.get_world_location())
        except Exception as exc:
            item["location_error"] = str(exc)
        try:
            item["relative_scale"] = vec_tuple(comp.get_relative_scale3d())
        except Exception as exc:
            item["scale_error"] = str(exc)
        items.append(item)
    return items


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

descs_to_load = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    label = str(desc.label)
    if label in {
        "GenesisFactoryStatusBoard_2_3",
        "GenesisFactoryStatusBoard_2_4",
        "GenesisFactoryStatusBoard_2_5",
    }:
        descs_to_load.append(desc.guid)

if descs_to_load:
    unreal.WorldPartitionBlueprintLibrary.load_actors(descs_to_load)

board23 = find_actor("GenesisFactoryStatusBoard_2_3")
board24 = find_actor("GenesisFactoryStatusBoard_2_4")
board25 = find_actor("GenesisFactoryStatusBoard_2_5")

result = {
    "loaded_desc_count": len(descs_to_load),
    "found": {
        "2_3": bool(board23),
        "2_4": bool(board24),
        "2_5": bool(board25),
    },
    "before": {},
    "after": {},
}

if board23 and board24 and board25:
    result["before"] = {
        "2_3_location": vec_tuple(board23.get_actor_location()),
        "2_3_rotation": rot_tuple(board23.get_actor_rotation()),
        "2_3_scale": vec_tuple(board23.get_actor_scale3d()),
        "2_4_location": vec_tuple(board24.get_actor_location()),
        "2_4_rotation": rot_tuple(board24.get_actor_rotation()),
        "2_4_scale": vec_tuple(board24.get_actor_scale3d()),
        "2_5_location": vec_tuple(board25.get_actor_location()),
        "2_5_rotation": rot_tuple(board25.get_actor_rotation()),
        "2_5_scale": vec_tuple(board25.get_actor_scale3d()),
    }

    target_location = (board23.get_actor_location() + board25.get_actor_location()) * 0.5
    target_rotation = board23.get_actor_rotation()
    target_scale = board23.get_actor_scale3d()

    board24.modify()
    moved, rotated = apply_actor_transform(board24, target_location, target_rotation, target_scale)

    # Reset the board's own components to their C++ defaults in case a per-instance
    # component transform caused the huge bounds.
    for comp in board24.get_components_by_class(unreal.SceneComponent):
        name = comp.get_name()
        if name == "Root":
            comp.modify()
            comp.set_relative_location(target_location, False, True)
            comp.set_relative_rotation(target_rotation, False, True)
            comp.set_relative_scale3d(target_scale)
        elif name == "BoardMesh":
            comp.modify()
            comp.set_relative_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
            comp.set_relative_rotation(unreal.Rotator(0.0, 0.0, 0.0), False, False)
            comp.set_relative_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        elif name == "MonitorWidget":
            comp.modify()
            comp.set_relative_location(unreal.Vector(-51.0, 0.0, -174.0), False, False)
            comp.set_relative_rotation(unreal.Rotator(-15.0, 180.0, 0.0), False, False)
            comp.set_relative_scale3d(unreal.Vector(0.15, 0.15, 0.16))

    result["after"] = {
        "set_actor_location_return": moved,
        "set_actor_rotation_return": rotated,
        "2_4_location": vec_tuple(board24.get_actor_location()),
        "2_4_rotation": rot_tuple(board24.get_actor_rotation()),
        "2_4_scale": vec_tuple(board24.get_actor_scale3d()),
        "2_4_components": component_snapshot(board24),
    }

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
