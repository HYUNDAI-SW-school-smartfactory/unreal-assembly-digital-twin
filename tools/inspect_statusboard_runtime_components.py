import json
import os
import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUT_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "artifacts", "inspect_statusboard_runtime_components.json")
)


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot(r):
    return [float(r.roll), float(r.pitch), float(r.yaw)]


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()

    try:
        try:
            descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        except TypeError:
            descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs(world)
        status_guids = []
        for desc in descs:
            label = str(getattr(desc, "label", ""))
            if label.startswith("GenesisFactoryStatusBoard_") or label.startswith("StatusBoard_"):
                status_guids.append(desc.guid)
        if status_guids:
            unreal.WorldPartitionBlueprintLibrary.load_actors(status_guids)
    except Exception as exc:
        unreal.log_warning(f"Could not load World Partition status board actors: {exc}")

    records = []
    actors = unreal.EditorActorSubsystem().get_all_level_actors()
    for actor in actors:
        label = actor.get_actor_label()
        if not label.startswith("GenesisFactoryStatusBoard_"):
            continue

        actor_record = {
            "label": label,
            "name": actor.get_name(),
            "class": actor.get_class().get_name(),
            "location": vec(actor.get_actor_location()),
            "rotation": rot(actor.get_actor_rotation()),
            "scale": vec(actor.get_actor_scale3d()),
            "components": [],
        }

        for comp in actor.get_components_by_class(unreal.ActorComponent):
            comp_record = {
                "name": comp.get_name(),
                "class": comp.get_class().get_name(),
            }
            if isinstance(comp, unreal.SceneComponent):
                comp_record["relative_location"] = vec(comp.get_editor_property("relative_location"))
                comp_record["relative_rotation"] = rot(comp.get_editor_property("relative_rotation"))
                comp_record["relative_scale"] = vec(comp.get_editor_property("relative_scale3d"))
                comp_record["visible"] = bool(comp.is_visible())
                comp_record["hidden_in_game"] = bool(comp.get_editor_property("hidden_in_game"))
            if isinstance(comp, unreal.WidgetComponent):
                cls = comp.get_editor_property("widget_class")
                comp_record["widget_class"] = cls.get_name() if cls else None
                draw_size = comp.get_editor_property("draw_size")
                comp_record["draw_size"] = [float(draw_size.x), float(draw_size.y)]
                comp_record["widget_space"] = str(comp.get_editor_property("space"))
                user_widget = None
                if hasattr(comp, "get_user_widget_object"):
                    user_widget = comp.get_user_widget_object()
                comp_record["user_widget"] = user_widget.get_class().get_name() if user_widget else None
            actor_record["components"].append(comp_record)

        records.append(actor_record)

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump(records, f, ensure_ascii=False, indent=2)


if __name__ == "__main__":
    main()
