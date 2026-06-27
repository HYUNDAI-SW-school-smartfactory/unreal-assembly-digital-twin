import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
MAP_BACKUP_PATH = "/Game/Maps/MainMaps_BeforeFactoryDynamicSpawn"

BLUEPRINT_BACKUPS = {
    "/Game/Blueprints/AGV/BP_AGV": "/Game/Blueprints/FactoryBackup/BP_AGV_Original",
    "/Game/Blueprints/AGV/BP_BatteryLift": "/Game/Blueprints/FactoryBackup/BP_BatteryLift_Original",
    "/Game/Blueprints/BP_TireAssemblyCell": "/Game/Blueprints/FactoryBackup/BP_TireAssemblyCell_Original",
    "/Game/Blueprints/BP_TireAssemblyCell_L": "/Game/Blueprints/FactoryBackup/BP_TireAssemblyCell_L_Original",
    "/Game/Blueprints/BP_MQTT_DisplayBoard": "/Game/Blueprints/FactoryBackup/BP_MQTT_DisplayBoard_Original",
    "/Game/Blueprints/BP_FactoryDisplayBoard": "/Game/Blueprints/FactoryBackup/BP_FactoryDisplayBoard_Original",
}

REMOVE_CLASS_MARKERS = (
    "BP_AGV_C",
    "BP_BatteryLift_C",
    "BP_TireAssemblyCell_C",
    "BP_TireAssemblyCell_L_C",
    "BP_MQTT_DisplayBoard_C",
    "BP_FactoryDisplayBoard_C",
    "Factory_Dashboard_Board_Actor",
)


def asset_exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)


def duplicate_if_needed(source, destination):
    if not asset_exists(source):
        unreal.log_warning("Backup source missing: {}".format(source))
        return
    if asset_exists(destination):
        unreal.log("Backup already exists: {}".format(destination))
        return
    if unreal.EditorAssetLibrary.duplicate_asset(source, destination):
        unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False)
        unreal.log("Backed up {} -> {}".format(source, destination))
    else:
        unreal.log_error("Failed to back up {} -> {}".format(source, destination))


if not asset_exists(MAP_PATH):
    raise RuntimeError("Map does not exist: {}".format(MAP_PATH))

duplicate_if_needed(MAP_PATH, MAP_BACKUP_PATH)
for src, dst in BLUEPRINT_BACKUPS.items():
    duplicate_if_needed(src, dst)

unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
unreal.SystemLibrary.execute_console_command(None, "wp.Editor.LoadAllActors")

deleted = []
for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
    class_path = actor.get_class().get_path_name()
    if any(marker in class_path for marker in REMOVE_CLASS_MARKERS):
        label = actor.get_actor_label()
        deleted.append("{} ({})".format(label, class_path))
        unreal.EditorLevelLibrary.destroy_actor(actor)

unreal.EditorLoadingAndSavingUtils.save_current_level()

report_path = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "mainmaps_factory_runtime_strip_report.txt",
    )
)
os.makedirs(os.path.dirname(report_path), exist_ok=True)
with open(report_path, "w", encoding="utf-8") as report:
    report.write("Map backup: {}\n".format(MAP_BACKUP_PATH))
    report.write("Blueprint backups:\n")
    for src, dst in BLUEPRINT_BACKUPS.items():
        report.write("  {} -> {}\n".format(src, dst))
    report.write("Deleted actors from {}:\n".format(MAP_PATH))
    for item in deleted:
        report.write("  {}\n".format(item))

unreal.log("Deleted {} pre-placed factory runtime actors from {}".format(len(deleted), MAP_PATH))
unreal.log("Report written to {}".format(report_path))
