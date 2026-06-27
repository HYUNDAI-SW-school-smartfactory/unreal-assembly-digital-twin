import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "tire_cell_visual_layout.json",
    )
)

TARGET_CLASS_MARKERS = [
    "BP_TireAssemblyCell",
    "BP_TireAssemblyCell_L",
    "BP_TireConveyor",
    "BP_TireTableBuffer",
    "BP_RobotTirePicker",
    "BP_RobotTirePickerToCar",
]


def safe(callback, default=None):
    try:
        return callback()
    except Exception:
        return default


def vec(value):
    if value is None:
        return None
    return [float(value.x), float(value.y), float(value.z)]


def rot(value):
    if value is None:
        return None
    return [float(value.roll), float(value.pitch), float(value.yaw)]


def path_name(obj):
    if not obj:
        return None
    return safe(lambda: obj.get_path_name(), str(obj))


def object_name(obj):
    if not obj:
        return None
    return safe(lambda: obj.get_name(), str(obj))


def component_record(component):
    record = {
        "name": component.get_name(),
        "class": component.get_class().get_path_name(),
    }

    if isinstance(component, unreal.SceneComponent):
        record.update(
            {
                "attach_parent": object_name(safe(lambda: component.get_attach_parent(), None)),
                "relative_location": vec(safe(lambda: component.get_relative_location(), None)),
                "relative_rotation": rot(safe(lambda: component.get_relative_rotation(), None)),
                "relative_scale": vec(safe(lambda: component.get_relative_scale3d(), None)),
                "world_location": vec(safe(lambda: component.get_component_location(), None)),
                "world_rotation": rot(safe(lambda: component.get_component_rotation(), None)),
                "world_scale": vec(safe(lambda: component.get_component_scale(), None)),
            }
        )

    if isinstance(component, unreal.StaticMeshComponent):
        mesh = safe(lambda: component.static_mesh, None)
        record["static_mesh"] = path_name(mesh)
        record["materials"] = []
        for index in range(safe(lambda: component.get_num_materials(), 0) or 0):
            record["materials"].append(path_name(safe(lambda i=index: component.get_material(i), None)))

    if isinstance(component, unreal.SkeletalMeshComponent):
        mesh = safe(lambda: component.skeletal_mesh, None)
        record["skeletal_mesh"] = path_name(mesh)
        anim = safe(lambda: component.animation_data.anim_to_play, None)
        record["anim_to_play"] = path_name(anim)

    if isinstance(component, unreal.ChildActorComponent):
        child_class = safe(lambda: component.get_child_actor_class(), None)
        child_actor = safe(lambda: component.get_child_actor(), None)
        record["child_actor_class"] = path_name(child_class)
        record["child_actor_name"] = object_name(child_actor)
        record["child_actor_actual_class"] = path_name(safe(lambda: child_actor.get_class(), None))

    return record


def actor_record(actor):
    actor_class = actor.get_class()
    components = safe(lambda: actor.get_components_by_class(unreal.ActorComponent), []) or []
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    return {
        "label": safe(lambda: actor.get_actor_label(), actor.get_name()),
        "name": actor.get_name(),
        "class": actor_class.get_path_name(),
        "location": vec(location),
        "rotation": rot(rotation),
        "scale": vec(actor.get_actor_scale3d()),
        "components": [component_record(component) for component in components],
    }


def is_target_actor(actor):
    class_path = actor.get_class().get_path_name()
    label = safe(lambda: actor.get_actor_label(), actor.get_name())
    name = actor.get_name()
    haystack = "{} {} {}".format(class_path, label, name)
    return any(marker in haystack for marker in TARGET_CLASS_MARKERS)


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    records = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if is_target_actor(actor):
            records.append(actor_record(actor))

    records.sort(key=lambda item: (item["class"], item["label"]))
    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as handle:
        json.dump(
            {
                "map": MAP_PATH,
                "actor_count": len(records),
                "actors": records,
            },
            handle,
            ensure_ascii=False,
            indent=2,
        )
    unreal.log("[extract_tire_cell_visual_layout] wrote {}".format(OUTPUT_PATH))


main()
