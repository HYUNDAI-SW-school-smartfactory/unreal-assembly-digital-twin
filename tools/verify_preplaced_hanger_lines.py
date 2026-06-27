import unreal

MAP_PATH = "/Game/Maps/MainMaps"
TARGET_PREFIX = "Genesis_Preplaced_HangerLine"

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_editor.load_level(MAP_PATH)

count = 0
target_guids = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs() or []:
    label = str(getattr(desc, "label", ""))
    if not label.lower().startswith(TARGET_PREFIX.lower()):
        continue
    target_guids.append(desc.guid)
    bounds = getattr(desc, "bounds", None)
    actor_package = getattr(desc, "actor_package", "")
    unreal.log(f"VERIFY {label} package={actor_package} bounds={bounds}")
    count += 1

unreal.log(f"VERIFY_COUNT {count}")

if target_guids:
    unreal.WorldPartitionBlueprintLibrary.load_actors(target_guids)

loaded_count = 0
for actor in actor_editor.get_all_level_actors():
    if not actor:
        continue
    label = actor.get_actor_label()
    if not label.lower().startswith(TARGET_PREFIX.lower()):
        continue
    unreal.log(f"VERIFY_LOCATION {label} location={actor.get_actor_location()}")
    loaded_count += 1

unreal.log(f"VERIFY_LOCATION_COUNT {loaded_count}")
