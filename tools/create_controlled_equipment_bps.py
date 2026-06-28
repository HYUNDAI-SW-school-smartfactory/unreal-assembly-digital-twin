import json
import unreal


CONTROLLED_DIR = "/Game/Blueprints/FactoryControlled"

BLUEPRINTS = [
    {
        "source": "/Game/Blueprints/AGV/BP_AGV",
        "controlled_name": "BP_AGV_Controlled",
        "original_backup": "/Game/Blueprints/FactoryBackup/BP_AGV_Original",
    },
    {
        "source": "/Game/Blueprints/AGV/BP_BatteryLift",
        "controlled_name": "BP_BatteryLift_Controlled",
        "original_backup": "/Game/Blueprints/FactoryBackup/BP_BatteryLift_Original",
    },
    {
        "source": "/Game/Blueprints/BP_TireAssemblyCell",
        "controlled_name": "BP_TireAssemblyCell_Controlled",
        "original_backup": "/Game/Blueprints/FactoryBackup/BP_TireAssemblyCell_Original",
    },
    {
        "source": "/Game/Blueprints/BP_TireAssemblyCell_L",
        "controlled_name": "BP_TireAssemblyCell_L_Controlled",
        "original_backup": "/Game/Blueprints/FactoryBackup/BP_TireAssemblyCell_L_Original",
    },
]


def log(message):
    unreal.log("[create_controlled_equipment_bps] {}".format(message))


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def duplicate_backup_if_needed(source_path, backup_path):
    if unreal.EditorAssetLibrary.does_asset_exist(backup_path):
        return "exists"
    source = unreal.EditorAssetLibrary.load_asset(source_path)
    if not source:
        raise RuntimeError("Missing source {}".format(source_path))
    asset_name = backup_path.rsplit("/", 1)[-1]
    package_path = backup_path.rsplit("/", 1)[0]
    duplicated = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(asset_name, package_path, source)
    if not duplicated:
        raise RuntimeError("Failed to create backup {}".format(backup_path))
    unreal.EditorAssetLibrary.save_asset(backup_path)
    return "created"


def create_child_controlled_bp(source_path, controlled_name):
    controlled_path = CONTROLLED_DIR + "/" + controlled_name
    if unreal.EditorAssetLibrary.does_asset_exist(controlled_path):
        return unreal.EditorAssetLibrary.load_asset(controlled_path), "exists"

    parent_class = unreal.EditorAssetLibrary.load_blueprint_class(source_path)
    if not parent_class:
        raise RuntimeError("Could not load parent blueprint class {}".format(source_path))

    blueprint = unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(
        controlled_path,
        parent_class,
    )
    if not blueprint:
        raise RuntimeError("Failed to create {}".format(controlled_path))

    return blueprint, "created"


def try_add_empty_beginplay_override(blueprint):
    result = {
        "receive_beginplay_graph": None,
        "event_graph": None,
        "compile": None,
        "notes": [],
    }

    try:
        graph = unreal.BlueprintEditorLibrary.find_graph(blueprint, "ReceiveBeginPlay")
        if not graph:
            graph = unreal.BlueprintEditorLibrary.add_function_graph(blueprint, "ReceiveBeginPlay")
        result["receive_beginplay_graph"] = bool(graph)
    except Exception as exc:
        result["notes"].append("ReceiveBeginPlay graph failed: {}".format(exc))

    try:
        event_graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)
        result["event_graph"] = bool(event_graph)
    except Exception as exc:
        result["notes"].append("find_event_graph failed: {}".format(exc))

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        result["compile"] = "ok"
    except Exception as exc:
        result["compile"] = "failed"
        result["notes"].append("compile failed: {}".format(exc))

    try:
        unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    except Exception as exc:
        result["notes"].append("save failed: {}".format(exc))

    return result


def main():
    ensure_dir("/Game/Blueprints/FactoryBackup")
    ensure_dir(CONTROLLED_DIR)

    report = []
    for config in BLUEPRINTS:
        backup_state = duplicate_backup_if_needed(config["source"], config["original_backup"])
        blueprint, bp_state = create_child_controlled_bp(config["source"], config["controlled_name"])
        override_result = try_add_empty_beginplay_override(blueprint)
        controlled_path = CONTROLLED_DIR + "/" + config["controlled_name"]
        report.append({
            "source": config["source"],
            "backup": config["original_backup"],
            "backup_state": backup_state,
            "controlled": controlled_path,
            "controlled_state": bp_state,
            "override_result": override_result,
        })
        log("{} -> {} ({})".format(config["source"], controlled_path, bp_state))

    out = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + "../artifacts/controlled_equipment_bps.json"
    )
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    log("wrote {}".format(out))


main()
