import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "mainmaps_layout.json",
    )
)


def safe(callback, default=None):
    try:
        return callback()
    except Exception:
        return default


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

records = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    actor_class = actor.get_class()
    components = safe(lambda: actor.get_components_by_class(unreal.ActorComponent), []) or []
    component_records = []

    for component in components:
        component_records.append(
            {
                "name": component.get_name(),
                "class": component.get_class().get_path_name(),
            }
        )

    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    tags = [str(tag) for tag in safe(lambda: actor.tags, []) or []]

    records.append(
        {
            "label": safe(lambda: actor.get_actor_label(), actor.get_name()),
            "name": actor.get_name(),
            "class": actor_class.get_path_name(),
            "location": [location.x, location.y, location.z],
            "rotation": [rotation.roll, rotation.pitch, rotation.yaw],
            "tags": tags,
            "components": component_records,
        }
    )

records.sort(key=lambda item: item["label"].lower())
os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)

with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(
        {
            "map": MAP_PATH,
            "actor_count": len(records),
            "actors": records,
        },
        output_file,
        ensure_ascii=False,
        indent=2,
    )

unreal.log("Map layout written to {}".format(OUTPUT_PATH))
