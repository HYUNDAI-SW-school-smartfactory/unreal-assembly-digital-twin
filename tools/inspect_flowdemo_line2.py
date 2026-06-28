import json
import os

import unreal


MAP_PATH = "/Game/Maps/L_MQTT_FlowDemo"
OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "flowdemo_line2_inspection.json",
    )
)


def vec(value):
    return [value.x, value.y, value.z]


def rot(value):
    return [value.roll, value.pitch, value.yaw]


def vec2(value):
    return [value.x, value.y]


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

actors = []
for actor in actor_subsystem.get_all_level_actors():
    components = actor.get_components_by_class(unreal.ActorComponent)
    spline_components = [
        component
        for component in components
        if isinstance(component, unreal.SplineComponent)
    ]
    spline_mesh_components = [
        component
        for component in components
        if isinstance(component, unreal.SplineMeshComponent)
    ]
    static_mesh_components = [
        component
        for component in components
        if isinstance(component, unreal.StaticMeshComponent)
    ]

    label = actor.get_actor_label()
    class_path = actor.get_class().get_path_name()
    if not (
        spline_components
        or spline_mesh_components
        or "Flow" in label
        or "Hanger" in label
        or "Display" in label
        or "MQTT" in label
    ):
        continue

    actor_record = {
        "label": label,
        "name": actor.get_name(),
        "class": class_path,
        "location": vec(actor.get_actor_location()),
        "rotation": rot(actor.get_actor_rotation()),
        "splines": [],
        "spline_meshes": [],
        "static_meshes": [],
    }

    for spline in spline_components:
        spline_record = {
            "name": spline.get_name(),
            "closed_loop": spline.is_closed_loop(),
            "length": spline.get_spline_length(),
            "points": [],
        }
        for index in range(spline.get_number_of_spline_points()):
            spline_record["points"].append(
                {
                    "index": index,
                    "location_world": vec(
                        spline.get_location_at_spline_point(
                            index,
                            unreal.SplineCoordinateSpace.WORLD,
                        )
                    ),
                    "location_local": vec(
                        spline.get_location_at_spline_point(
                            index,
                            unreal.SplineCoordinateSpace.LOCAL,
                        )
                    ),
                    "tangent_world": vec(
                        spline.get_tangent_at_spline_point(
                            index,
                            unreal.SplineCoordinateSpace.WORLD,
                        )
                    ),
                }
            )
        actor_record["splines"].append(spline_record)

    for component in spline_mesh_components:
        mesh = component.get_editor_property("static_mesh")
        materials = []
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            materials.append(material.get_path_name() if material else "")

        actor_record["spline_meshes"].append(
            {
                "name": component.get_name(),
                "mesh": mesh.get_path_name() if mesh else "",
                "materials": materials,
                "relative_location": vec(
                    component.get_editor_property("relative_location")
                ),
                "relative_rotation": rot(
                    component.get_editor_property("relative_rotation")
                ),
                "relative_scale": vec(
                    component.get_editor_property("relative_scale3d")
                ),
                "start_position": vec(component.get_start_position()),
                "start_tangent": vec(component.get_start_tangent()),
                "end_position": vec(component.get_end_position()),
                "end_tangent": vec(component.get_end_tangent()),
                "start_scale": vec2(component.get_start_scale()),
                "end_scale": vec2(component.get_end_scale()),
                "forward_axis": str(component.get_forward_axis()),
            }
        )

    for component in static_mesh_components:
        mesh = component.get_editor_property("static_mesh")
        if mesh is None:
            continue
        mesh_path = mesh.get_path_name()
        if "Rail" not in mesh_path and "Hanger" not in mesh_path and "Display" not in mesh_path:
            continue
        actor_record["static_meshes"].append(
            {
                "name": component.get_name(),
                "mesh": mesh_path,
                "relative_location": vec(
                    component.get_editor_property("relative_location")
                ),
                "relative_rotation": rot(
                    component.get_editor_property("relative_rotation")
                ),
                "relative_scale": vec(
                    component.get_editor_property("relative_scale3d")
                ),
            }
        )

    actors.append(actor_record)

actors.sort(key=lambda item: (item["location"][1], item["location"][0], item["label"]))
os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(
        {"map": MAP_PATH, "actors": actors},
        output_file,
        ensure_ascii=False,
        indent=2,
    )

unreal.log("Flow demo line inspection written to {}".format(OUTPUT_PATH))
