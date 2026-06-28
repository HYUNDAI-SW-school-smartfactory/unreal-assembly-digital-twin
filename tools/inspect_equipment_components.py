import json
import unreal


TARGETS = [
    "/Game/Blueprints/AGV/BP_AGV",
    "/Game/Blueprints/AGV/BP_BatteryLift",
    "/Game/Blueprints/BP_TireAssemblyCell",
    "/Game/Blueprints/BP_TireAssemblyCell_L",
]


INTERESTING_PROPS = [
    "auto_activate",
    "auto_destroy",
    "auto_play",
    "playing",
    "looping",
    "start_position",
    "animation_mode",
    "animation_data",
    "animation",
    "skeletal_mesh",
    "component_tags",
]


def inspect_object(obj):
    data = {
        "name": obj.get_name(),
        "class": obj.get_class().get_path_name(),
        "props": {},
    }
    for prop in INTERESTING_PROPS:
        try:
            data["props"][prop] = str(obj.get_editor_property(prop))
        except Exception:
            pass
    return data


def main():
    report = {}
    for path in TARGETS:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if not cls:
            report[path] = {"error": "class missing"}
            continue
        cdo = unreal.get_default_object(cls)
        entry = {"class": cls.get_path_name(), "cdo": inspect_object(cdo), "components": []}
        try:
            comps = cdo.get_components_by_class(unreal.ActorComponent)
        except Exception:
            comps = []
        for comp in comps:
            entry["components"].append(inspect_object(comp))
        report[path] = entry

    out = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + "../artifacts/equipment_components.json"
    )
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[inspect_equipment_components] wrote {}".format(out))


main()
