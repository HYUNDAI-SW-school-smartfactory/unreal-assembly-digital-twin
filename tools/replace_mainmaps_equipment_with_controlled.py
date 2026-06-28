import json
import unreal


MAP_PATH = "/Game/Maps/MainMaps"
BACKUP_MAP_PATH = "/Game/Maps/MainMaps_BeforeControlledEquipment"

REPLACEMENTS = [
    {
        "source_class": "/Game/Blueprints/AGV/BP_AGV",
        "controlled_class": "/Game/Blueprints/FactoryControlled/BP_AGV_Controlled",
    },
    {
        "source_class": "/Game/Blueprints/AGV/BP_BatteryLift",
        "controlled_class": "/Game/Blueprints/FactoryControlled/BP_BatteryLift_Controlled",
    },
    {
        "source_class": "/Game/Blueprints/BP_TireAssemblyCell",
        "controlled_class": "/Game/Blueprints/FactoryControlled/BP_TireAssemblyCell_Controlled",
    },
    {
        "source_class": "/Game/Blueprints/BP_TireAssemblyCell_L",
        "controlled_class": "/Game/Blueprints/FactoryControlled/BP_TireAssemblyCell_L_Controlled",
    },
]


def log(message):
    unreal.log("[replace_mainmaps_equipment_with_controlled] {}".format(message))


def load_map():
    if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
        raise RuntimeError("Failed to load {}".format(MAP_PATH))
    log("Loaded {}".format(MAP_PATH))


def backup_map_if_needed():
    if unreal.EditorAssetLibrary.does_asset_exist(BACKUP_MAP_PATH):
        log("Backup already exists: {}".format(BACKUP_MAP_PATH))
        return

    source = unreal.EditorAssetLibrary.load_asset(MAP_PATH)
    duplicated = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        "MainMaps_BeforeControlledEquipment",
        "/Game/Maps",
        source,
    )
    if not duplicated:
        raise RuntimeError("Failed to create map backup {}".format(BACKUP_MAP_PATH))
    unreal.EditorAssetLibrary.save_asset(BACKUP_MAP_PATH)
    log("Created backup {}".format(BACKUP_MAP_PATH))


def get_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def get_class_path(actor):
    return actor.get_class().get_path_name()


def copy_basic_actor_state(source, target):
    target.set_actor_label(get_label(source))
    target.set_actor_location(source.get_actor_location(), False, False)
    target.set_actor_rotation(source.get_actor_rotation(), False)
    target.set_actor_scale3d(source.get_actor_scale3d())
    try:
        target.set_folder_path(source.get_folder_path())
    except Exception:
        pass
    try:
        target.set_editor_property("tags", source.get_editor_property("tags"))
    except Exception:
        pass
    try:
        target.set_actor_hidden_in_game(source.is_hidden_ed())
    except Exception:
        pass


def replace_actor(source_actor, controlled_class):
    new_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        controlled_class,
        source_actor.get_actor_location(),
        source_actor.get_actor_rotation(),
    )
    copy_basic_actor_state(source_actor, new_actor)
    unreal.EditorLevelLibrary.destroy_actor(source_actor)
    return new_actor


def main():
    load_map()
    backup_map_if_needed()

    controlled_classes = {}
    source_classes = {}
    for item in REPLACEMENTS:
        source = unreal.EditorAssetLibrary.load_blueprint_class(item["source_class"])
        controlled = unreal.EditorAssetLibrary.load_blueprint_class(item["controlled_class"])
        if not source:
            raise RuntimeError("Missing source class {}".format(item["source_class"]))
        if not controlled:
            raise RuntimeError("Missing controlled class {}".format(item["controlled_class"]))
        source_classes[source.get_path_name()] = item
        controlled_classes[item["controlled_class"]] = controlled

    replaced = []
    skipped = []
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        class_path = get_class_path(actor)
        matched = None
        for item in REPLACEMENTS:
            source_class = unreal.EditorAssetLibrary.load_blueprint_class(item["source_class"])
            controlled_class = unreal.EditorAssetLibrary.load_blueprint_class(item["controlled_class"])
            if actor.get_class() == source_class:
                matched = item
                controlled = controlled_class
                break
            if actor.get_class() == controlled_class:
                skipped.append({"label": get_label(actor), "class": class_path, "reason": "already controlled"})
                matched = "already"
                break
        if not matched or matched == "already":
            continue

        old_label = get_label(actor)
        old_class = class_path
        new_actor = replace_actor(actor, controlled)
        replaced.append({
            "label": old_label,
            "old_class": old_class,
            "new_class": get_class_path(new_actor),
        })
        log("Replaced {}: {} -> {}".format(old_label, old_class, get_class_path(new_actor)))

    unreal.EditorLevelLibrary.save_current_level()

    report = {
        "map": MAP_PATH,
        "backup": BACKUP_MAP_PATH,
        "replaced": replaced,
        "skipped": skipped,
    }
    out = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + "../artifacts/mainmaps_controlled_equipment_replace_report.json"
    )
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    log("wrote {}".format(out))


main()
