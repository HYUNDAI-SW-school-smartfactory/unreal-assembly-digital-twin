import unreal


MAP_PATH = "/Game/Maps/MainMaps"
TARGET_PREFIX = "Genesis_Preplaced_HangerLine"
OFFSET = unreal.Vector(150.0, 0.0, -100.0)


def main():
    level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    level_editor.load_level(MAP_PATH)

    actor_descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
    target_guids = []
    for desc in actor_descs or []:
        label = getattr(desc, "label", "")
        if str(label).lower().startswith(TARGET_PREFIX.lower()):
            target_guids.append(desc.guid)

    if target_guids:
        unreal.WorldPartitionBlueprintLibrary.load_actors(target_guids)

    moved = []
    for actor in actor_editor.get_all_level_actors():
        if not actor:
            continue

        label = actor.get_actor_label()
        if not label.lower().startswith(TARGET_PREFIX.lower()):
            continue

        actor.modify()
        old_location = actor.get_actor_location()
        new_location = old_location + OFFSET
        actor.set_actor_location(new_location, False, True)
        moved.append((label, old_location, new_location))

    if len(moved) != 3:
        unreal.log_warning(
            f"Expected 3 {TARGET_PREFIX} actors, but moved {len(moved)}."
        )

    for label, old_location, new_location in moved:
        unreal.log(
            f"Moved {label}: {old_location} -> {new_location}"
        )

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    level_editor.save_current_level()


main()
