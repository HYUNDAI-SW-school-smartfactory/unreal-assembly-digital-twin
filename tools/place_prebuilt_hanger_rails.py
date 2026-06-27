import unreal


MAP_PATH = "/Game/Maps/MainMaps"
BACKUP_MAP_PATH = "/Game/Maps/MainMaps_BeforePreplacedRails"

LINE_CONFIGS = [
    {"line_id": 1, "center_y": 1620.0},
    {"line_id": 2, "center_y": 6400.0},
    {"line_id": 3, "center_y": 11200.0},
]

RAIL_HEIGHT = 1200.0
FORWARD_STATION_X = [1620.0, 4800.0, 8020.0, 11420.0, 14640.0]
UNLOAD_POSITION_X = 15650.0
EMPTY_RETURN_OFFSET = 1680.0


def log(message):
    unreal.log("[place_prebuilt_hanger_rails] {}".format(message))


def load_map():
    editor_level_library = unreal.EditorLevelLibrary
    if not editor_level_library.load_level(MAP_PATH):
        raise RuntimeError("Failed to load {}".format(MAP_PATH))
    log("Loaded {}".format(MAP_PATH))


def make_backup_if_needed():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    existing_backup = unreal.EditorAssetLibrary.does_asset_exist(BACKUP_MAP_PATH)
    if existing_backup:
        log("Backup already exists: {}".format(BACKUP_MAP_PATH))
        return

    duplicated = asset_tools.duplicate_asset(
        "MainMaps_BeforePreplacedRails",
        "/Game/Maps",
        unreal.EditorAssetLibrary.load_asset(MAP_PATH),
    )
    if not duplicated:
        raise RuntimeError("Failed to create backup {}".format(BACKUP_MAP_PATH))

    unreal.EditorAssetLibrary.save_asset(BACKUP_MAP_PATH)
    log("Created backup {}".format(BACKUP_MAP_PATH))


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def find_existing_line_actor(line_id):
    target_label = "Genesis_Preplaced_HangerLine_{:02d}".format(line_id)
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == target_label:
            return actor
    return None


def set_property(actor, property_name, value):
    actor.set_editor_property(property_name, value)


def configure_line_actor(actor, line_id, center_y):
    set_property(actor, "LineId", line_id)
    set_property(actor, "CenterY", center_y)
    set_property(actor, "RailHeight", RAIL_HEIGHT)
    set_property(actor, "ForwardStationX", FORWARD_STATION_X)
    set_property(actor, "UnloadPositionX", UNLOAD_POSITION_X)
    set_property(actor, "EmptyReturnOffset", EMPTY_RETURN_OFFSET)
    actor.set_actor_location(unreal.Vector(0.0, center_y, 0.0), False, False)
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    try:
        actor.rerun_construction_scripts()
    except Exception as exc:
        log("Could not rerun construction scripts for {}: {}".format(actor_label(actor), exc))


def spawn_or_update_line(line_class, line_id, center_y):
    existing = find_existing_line_actor(line_id)
    if existing:
        actor = existing
        log("Updating existing {}".format(actor_label(actor)))
    else:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            line_class,
            unreal.Vector(0.0, center_y, 0.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        actor.set_actor_label("Genesis_Preplaced_HangerLine_{:02d}".format(line_id))
        log("Spawned {}".format(actor_label(actor)))

    configure_line_actor(actor, line_id, center_y)


def main():
    load_map()
    make_backup_if_needed()

    line_class = unreal.load_class(None, "/Script/GenesisDigitalTwin.GenesisHangerLineActor")
    if not line_class:
        raise RuntimeError("Failed to load AGenesisHangerLineActor class")

    for config in LINE_CONFIGS:
        spawn_or_update_line(line_class, config["line_id"], config["center_y"])

    unreal.EditorLevelLibrary.save_current_level()
    log("Saved {} with prebuilt hanger rails".format(MAP_PATH))


main()
