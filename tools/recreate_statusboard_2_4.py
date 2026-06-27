import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
LABEL_23 = "GenesisFactoryStatusBoard_2_3"
LABEL_24 = "GenesisFactoryStatusBoard_2_4"
LABEL_25 = "GenesisFactoryStatusBoard_2_5"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "recreate_statusboard_2_4.json")


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


def find_actors(label):
    return [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if actor_label(actor) == label]


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot(r):
    return [float(r.roll), float(r.pitch), float(r.yaw)]


def load_labels(labels):
    guids = []
    for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
        if str(desc.label) in labels:
            guids.append(desc.guid)
    if guids:
        unreal.WorldPartitionBlueprintLibrary.load_actors(guids)
    return len(guids)


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
loaded_count = load_labels({LABEL_23, LABEL_24, LABEL_25})

board23 = find_actor(LABEL_23)
old24s = find_actors(LABEL_24)
board25 = find_actor(LABEL_25)

result = {
    "loaded_desc_count": loaded_count,
    "found_before": {
        LABEL_23: bool(board23),
        LABEL_24: len(old24s),
        LABEL_25: bool(board25),
    },
}

if board23 and board25:
    target_location = (board23.get_actor_location() + board25.get_actor_location()) * 0.5
    target_rotation = board23.get_actor_rotation()
    target_scale = board23.get_actor_scale3d()
    spawn_class = board23.get_class()

    result["target"] = {
        "location": vec(target_location),
        "rotation": rot(target_rotation),
        "scale": vec(target_scale),
        "class": str(spawn_class.get_path_name()),
    }

    result["old_2_4"] = []
    for old24 in old24s:
        result["old_2_4"].append(
            {
                "name": old24.get_name(),
                "location": vec(old24.get_actor_location()),
                "rotation": rot(old24.get_actor_rotation()),
                "scale": vec(old24.get_actor_scale3d()),
            }
        )
        unreal.EditorLevelLibrary.destroy_actor(old24)

    new24 = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class=spawn_class,
        location=target_location,
        rotation=target_rotation,
    )
    result["new_2_4_spawned_before_label"] = {
        "name": new24.get_name(),
        "location": vec(new24.get_actor_location()),
        "scale": vec(new24.get_actor_scale3d()),
    }
    new24.set_actor_label(LABEL_24)
    result["new_2_4_after_label"] = {
        "name": new24.get_name(),
        "label": new24.get_actor_label(),
        "location": vec(new24.get_actor_location()),
        "scale": vec(new24.get_actor_scale3d()),
    }
    new24.set_actor_scale3d(target_scale)
    result["new_2_4_after_scale"] = {
        "name": new24.get_name(),
        "label": new24.get_actor_label(),
        "location": vec(new24.get_actor_location()),
        "scale": vec(new24.get_actor_scale3d()),
    }

    # Keep the C++ component layout clean and predictable.
    for comp in new24.get_components_by_class(unreal.SceneComponent):
        name = comp.get_name()
        comp.modify()
        if name == "Root":
            continue
        if name == "BoardMesh":
            comp.set_relative_location(unreal.Vector(0.0, 0.0, 0.0), False, True)
            comp.set_relative_rotation(unreal.Rotator(0.0, 0.0, 0.0), False, True)
            comp.set_relative_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        elif name == "MonitorWidget":
            comp.set_relative_location(unreal.Vector(-51.0, 0.0, -174.0), False, True)
            comp.set_relative_rotation(unreal.Rotator(-15.0, 180.0, 0.0), False, True)
            comp.set_relative_scale3d(unreal.Vector(0.15, 0.15, 0.16))

    result["new_2_4"] = {
        "name": new24.get_name(),
        "label": new24.get_actor_label(),
        "location": vec(new24.get_actor_location()),
        "rotation": rot(new24.get_actor_rotation()),
        "scale": vec(new24.get_actor_scale3d()),
    }

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
