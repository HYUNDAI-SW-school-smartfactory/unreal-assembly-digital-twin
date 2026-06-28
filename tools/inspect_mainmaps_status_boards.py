import json
import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUT_PATH = "C:/Users/한국전파진흥협회/unreal-assembly-digital-twin/artifacts/mainmaps_status_boards.json"


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

world = unreal.EditorLevelLibrary.get_editor_world()
try:
    descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs(world)
    board_descs = []
    for desc in descs:
        text = str(desc)
        if "StatusBoard" in text or "GenesisFactoryStatusBoard" in text:
            board_descs.append(desc)
    if board_descs:
        unreal.WorldPartitionBlueprintLibrary.load_actors(board_descs)
except Exception as exc:
    print(f"WorldPartition load failed: {exc}")

records = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    name = actor.get_actor_label()
    if "StatusBoard" not in name and "GenesisFactoryStatusBoard" not in name:
        continue

    actor_record = {
        "label": name,
        "name": actor.get_name(),
        "class": actor.get_class().get_path_name(),
        "location": tuple(round(v, 3) for v in actor.get_actor_location()),
        "rotation": tuple(round(v, 3) for v in actor.get_actor_rotation()),
        "scale": tuple(round(v, 3) for v in actor.get_actor_scale3d()),
        "components": [],
    }

    for comp in actor.get_components_by_class(unreal.ActorComponent):
        comp_record = {
            "name": comp.get_name(),
            "class": comp.get_class().get_path_name(),
        }
        if isinstance(comp, unreal.SceneComponent):
            comp_record["relative_location"] = tuple(round(v, 3) for v in comp.get_relative_location())
            comp_record["relative_rotation"] = tuple(round(v, 3) for v in comp.get_relative_rotation())
            comp_record["relative_scale"] = tuple(round(v, 3) for v in comp.get_relative_scale3d())
            comp_record["visible"] = comp.is_visible()
        if isinstance(comp, unreal.WidgetComponent):
            wc = comp.get_widget_class()
            comp_record["widget_class"] = wc.get_path_name() if wc else None
            comp_record["draw_size"] = tuple(comp.get_draw_size())
            comp_record["two_sided"] = comp.get_editor_property("two_sided")
            comp_record["space"] = str(comp.get_editor_property("space"))
        if isinstance(comp, unreal.StaticMeshComponent):
            mesh = comp.get_static_mesh()
            comp_record["static_mesh"] = mesh.get_path_name() if mesh else None
        actor_record["components"].append(comp_record)

    records.append(actor_record)

records.sort(key=lambda r: r["label"])
with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(records, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH} ({len(records)} boards)")
