import unreal

MAP_PATH = "/Game/Maps/MainMaps"
TARGET_PREFIX = "Genesis_Preplaced_HangerLine"

BASE_FORWARD_STATION_X = [1620.0, 4800.0, 8020.0, 11420.0, 14640.0]
BASE_UNLOAD_POSITION_X = 15650.0
BASE_RAIL_HEIGHT = 1200.0

OFFSET_X = 150.0
OFFSET_Z = -200.0


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_editor.load_level(MAP_PATH)

    target_guids = []
    for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs() or []:
        label = str(getattr(desc, "label", ""))
        if label.lower().startswith(TARGET_PREFIX.lower()):
            target_guids.append(desc.guid)

    if target_guids:
        unreal.WorldPartitionBlueprintLibrary.load_actors(target_guids)

    desired_forward_x = [x + OFFSET_X for x in BASE_FORWARD_STATION_X]
    desired_unload_x = BASE_UNLOAD_POSITION_X + OFFSET_X
    desired_rail_height = BASE_RAIL_HEIGHT + OFFSET_Z

    changed = []
    for actor in actor_editor.get_all_level_actors():
        if not actor:
            continue

        label = actor.get_actor_label()
        if not label.lower().startswith(TARGET_PREFIX.lower()):
            continue

        actor.modify()
        center_y = float(actor.get_editor_property("CenterY"))
        actor.set_editor_property("ForwardStationX", desired_forward_x)
        actor.set_editor_property("UnloadPositionX", desired_unload_x)
        actor.set_editor_property("RailHeight", desired_rail_height)
        actor.set_actor_location(unreal.Vector(OFFSET_X, center_y, OFFSET_Z), False, True)
        changed.append(label)
        unreal.log(
            "OFFSET_HANGER_GEOMETRY {} loc={} forward_x={} unload_x={} rail_height={}".format(
                label,
                actor.get_actor_location(),
                desired_forward_x,
                desired_unload_x,
                desired_rail_height,
            )
        )

    if not changed:
        unreal.log_warning("No {} actors were updated.".format(TARGET_PREFIX))
    else:
        unreal.log("OFFSET_HANGER_GEOMETRY_COUNT {}".format(len(changed)))

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    level_editor.save_current_level()


main()
