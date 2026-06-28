import json
import os
import re

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
OUT_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "artifacts", "fix_station_statusboard_widgets.json")
)

STATION_LABEL_RE = re.compile(r"^GenesisFactoryStatusBoard_(\d+)_(\d+)$")
LINE_LABEL_RE = re.compile(r"^GenesisFactoryStatusBoard_(\d+)_line$")

STATION_WIDGET_LOCATION = unreal.Vector(-51.0, 0.0, -174.0)
LINE_WIDGET_LOCATION = unreal.Vector(-51.0, 0.0, -174.0)
# Unreal Python Rotator constructor is roll, pitch, yaw. This matches the C++
# FRotator(Pitch=-15, Yaw=180, Roll=0) layout used by BP_MQTT_DisplayBoard.
LINE_WIDGET_ROTATION = unreal.Rotator(0.0, -15.0, 180.0)
STATION_WIDGET_ROTATION = unreal.Rotator(0.0, -15.0, 180.0)
STATION_ACTOR_SCALE = unreal.Vector(2.0, 3.0, 2.0)
STATION_WIDGET_SCALE = unreal.Vector(0.15, 0.15, 0.16)
STATUS_WIDGET_PATH = "/Script/GenesisDigitalTwin.GenesisFactoryStatusWidget"


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def rot(r):
    return [float(r.roll), float(r.pitch), float(r.yaw)]


def load_status_board_actors():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    world = unreal.EditorLevelLibrary.get_editor_world()

    try:
        try:
            descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
        except TypeError:
            descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs(world)
        guids = []
        for desc in descs:
            label = str(getattr(desc, "label", ""))
            if label.startswith("GenesisFactoryStatusBoard_"):
                guids.append(desc.guid)
        if guids:
            unreal.WorldPartitionBlueprintLibrary.load_actors(guids)
    except Exception as exc:
        unreal.log_warning(f"Could not load World Partition status board actors: {exc}")

    return unreal.EditorActorSubsystem().get_all_level_actors()


def main():
    status_widget_class = unreal.load_class(None, STATUS_WIDGET_PATH)
    changed = []
    skipped = []

    for actor in load_status_board_actors():
        label = actor.get_actor_label()
        is_station = STATION_LABEL_RE.match(label)
        is_line = LINE_LABEL_RE.match(label)
        if not is_station and not is_line:
            skipped.append(label)
            continue

        widget = None
        for component in actor.get_components_by_class(unreal.WidgetComponent):
            if component.get_name() == "MonitorWidget":
                widget = component
                break

        if not widget:
            skipped.append(label)
            continue

        actor.modify()
        widget.modify()
        if is_station:
            actor.set_actor_scale3d(STATION_ACTOR_SCALE)
        widget.set_editor_property("relative_location", STATION_WIDGET_LOCATION if is_station else LINE_WIDGET_LOCATION)
        widget.set_editor_property("relative_rotation", STATION_WIDGET_ROTATION if is_station else LINE_WIDGET_ROTATION)
        widget.set_editor_property("relative_scale3d", STATION_WIDGET_SCALE)
        widget.set_editor_property("draw_size", unreal.IntPoint(1280, 720))
        widget.set_editor_property("pivot", unreal.Vector2D(0.5, 0.5))
        widget.set_editor_property("hidden_in_game", False)
        widget.set_visibility(True, True)
        if status_widget_class:
            widget.set_editor_property("widget_class", status_widget_class)

        changed.append(
            {
                "label": label,
                "actor_location": vec(actor.get_actor_location()),
                "widget_location": vec(widget.get_editor_property("relative_location")),
                "widget_rotation": rot(widget.get_editor_property("relative_rotation")),
                "widget_scale": vec(widget.get_editor_property("relative_scale3d")),
            }
        )

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    with open(OUT_PATH, "w", encoding="utf-8") as f:
        json.dump({"changed": changed, "skipped": sorted(set(skipped))}, f, ensure_ascii=False, indent=2)

    unreal.log(f"Updated {len(changed)} station status board widget component(s).")


if __name__ == "__main__":
    main()
