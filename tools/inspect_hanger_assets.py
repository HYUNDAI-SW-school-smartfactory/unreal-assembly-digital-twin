import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
ASSETS = {
    "line": "/Game/Blueprints/BP_HangerMovingAlongSpline",
    "hanger": "/Game/Blueprints/BP_HangerVehicle",
    "car": "/Game/Blueprints/BP_Car",
    "dashboard": "/Game/Blueprints/BP_MQTT_DisplayBoard",
}

OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "hanger_asset_runtime_inspection.json",
    )
)


def vector(value):
    return [value.x, value.y, value.z]


def rotator(value):
    return [value.roll, value.pitch, value.yaw]


records = {}
subobject_subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

for key, asset_path in ASSETS.items():
    asset_name = asset_path.rsplit("/", 1)[-1]
    generated_class = unreal.load_class(
        None,
        "{}.{}_C".format(asset_path, asset_name),
    )
    if generated_class is None:
        raise RuntimeError("Could not load generated class for {}".format(asset_path))
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    actor = unreal.get_default_object(generated_class)

    actor_record = {
        "asset": asset_path,
        "class": actor.get_class().get_path_name(),
        "components": [],
        "subobjects": [],
    }

    for component in actor.get_components_by_class(unreal.ActorComponent):
        component_record = {
            "name": component.get_name(),
            "class": component.get_class().get_path_name(),
            "visible": None,
            "relative_location": None,
            "relative_rotation": None,
            "relative_scale": None,
        }

        if isinstance(component, unreal.SceneComponent):
            component_record["visible"] = component.is_visible()
            component_record["relative_location"] = vector(
                component.get_relative_location()
            )
            component_record["relative_rotation"] = rotator(
                component.get_relative_rotation()
            )
            component_record["relative_scale"] = vector(
                component.get_relative_scale3d()
            )

        if isinstance(component, unreal.SplineComponent):
            component_record["spline_points"] = []
            component_record["spline_length"] = component.get_spline_length()
            for index in range(component.get_number_of_spline_points()):
                component_record["spline_points"].append(
                    {
                        "index": index,
                        "location": vector(
                            component.get_location_at_spline_point(
                                index,
                                unreal.SplineCoordinateSpace.LOCAL,
                            )
                        ),
                    }
                )

        actor_record["components"].append(component_record)

    records[key] = actor_record

    for handle in subobject_subsystem.k2_gather_subobject_data_for_blueprint(
        blueprint
    ):
        data = subobject_subsystem.k2_find_subobject_data_from_handle(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        subobject_record = {
            "display_name": str(
                unreal.SubobjectDataBlueprintFunctionLibrary.get_display_name(
                    data
                )
            ),
            "variable_name": str(
                unreal.SubobjectDataBlueprintFunctionLibrary.get_variable_name(
                    data
                )
            ),
            "object": obj.get_path_name() if obj else "",
            "class": obj.get_class().get_path_name() if obj else "",
        }
        if isinstance(obj, unreal.SceneComponent):
            subobject_record["visible"] = obj.is_visible()
            subobject_record["relative_location"] = vector(
                obj.get_editor_property("relative_location")
            )
            subobject_record["relative_rotation"] = rotator(
                obj.get_editor_property("relative_rotation")
            )
            subobject_record["relative_scale"] = vector(
                obj.get_editor_property("relative_scale3d")
            )
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            subobject_record["static_mesh"] = (
                mesh.get_path_name() if mesh else ""
            )
        if isinstance(obj, unreal.SplineComponent):
            subobject_record["spline_length"] = obj.get_spline_length()
            subobject_record["spline_points"] = []
            for index in range(obj.get_number_of_spline_points()):
                subobject_record["spline_points"].append(
                    {
                        "index": index,
                        "location": vector(
                            obj.get_location_at_spline_point(
                                index,
                                unreal.SplineCoordinateSpace.LOCAL,
                            )
                        ),
                    }
                )
        actor_record["subobjects"].append(subobject_record)

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(records, output_file, ensure_ascii=False, indent=2)

unreal.log("Hanger assets inspected: {}".format(OUTPUT_PATH))
