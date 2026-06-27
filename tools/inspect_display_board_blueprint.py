import json
import os

import unreal


TARGET = "/Game/Blueprints/BP_MQTT_DisplayBoard"
OUTPUT = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "..", "artifacts", "bp_mqtt_display_board_components.json")
)


def prop(obj, name):
    try:
        value = obj.get_editor_property(name)
        return str(value)
    except Exception:
        return None


def vector_to_list(value):
    try:
        return [value.x, value.y, value.z]
    except Exception:
        return None


def rotator_to_list(value):
    try:
        return [value.roll, value.pitch, value.yaw]
    except Exception:
        return None


bp = unreal.EditorAssetLibrary.load_asset(TARGET)
cls = unreal.EditorAssetLibrary.load_blueprint_class(TARGET)
if not bp or not cls:
    raise RuntimeError(f"Could not load {TARGET}")


def component_record(comp):
    return {
        "name": comp.get_name(),
        "class": comp.get_class().get_path_name(),
        "relative_location": vector_to_list(prop(comp, "relative_location")),
        "relative_rotation": rotator_to_list(prop(comp, "relative_rotation")),
        "relative_scale3d": vector_to_list(prop(comp, "relative_scale3d")),
        "static_mesh": prop(comp, "static_mesh"),
        "widget_class": prop(comp, "widget_class"),
        "draw_size": prop(comp, "draw_size"),
        "widget_space": prop(comp, "space"),
        "two_sided": prop(comp, "two_sided"),
        "pivot": prop(comp, "pivot"),
    }


records = []
cdo = unreal.get_default_object(cls)
try:
    for comp in cdo.get_components_by_class(unreal.ActorComponent):
        records.append(component_record(comp))
except Exception:
    pass

subobjects = []
try:
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        data = library.get_data(handle)
        obj = library.get_object(data)
        template = library.get_object_for_blueprint(data, bp)
        subobjects.append(
            {
                "handle": str(handle),
                "data": str(data),
                "object": str(obj),
                "template": str(template),
                "template_record": component_record(template) if template else None,
            }
        )
        if template:
            records.append(component_record(template))
except Exception as exc:
    subobjects.append({"error": str(exc), "subsystem_dir": dir(getattr(unreal, "SubobjectDataBlueprintFunctionLibrary", object))})

os.makedirs(os.path.dirname(OUTPUT), exist_ok=True)
with open(OUTPUT, "w", encoding="utf-8") as f:
    json.dump(
        {
            "target": TARGET,
            "class": cls.get_path_name(),
            "components": records,
            "subobjects": subobjects,
        },
        f,
        ensure_ascii=False,
        indent=2,
    )

unreal.log(f"Display board components written to {OUTPUT}")
