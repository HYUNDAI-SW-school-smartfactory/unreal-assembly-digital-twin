import json
import os

import unreal


MAP_PATH = "/Game/Maps/MainMaps"
WORKSPACE_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_PATH = os.path.join(WORKSPACE_ROOT, "artifacts", "test_statusboard_spawn_api.json")


def actor_label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def find_actor(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor_label(actor) == label:
            return actor
    return None


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)

labels = {"GenesisFactoryStatusBoard_2_3", "GenesisFactoryStatusBoard_2_5"}
guids = []
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    if str(desc.label) in labels:
        guids.append(desc.guid)
if guids:
    unreal.WorldPartitionBlueprintLibrary.load_actors(guids)

board23 = find_actor("GenesisFactoryStatusBoard_2_3")
board25 = find_actor("GenesisFactoryStatusBoard_2_5")
result = {"found": bool(board23 and board25), "attempts": []}

if board23 and board25:
    target_location = (board23.get_actor_location() + board25.get_actor_location()) * 0.5
    target_rotation = board23.get_actor_rotation()
    spawn_class = board23.get_class()
    result["target_location"] = vec(target_location)

    tests = []

    try:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class=spawn_class,
            location=target_location,
            rotation=target_rotation,
        )
        actor.set_actor_label("TEMP_StatusBoard_SpawnKeyword")
        tests.append(("EditorLevelLibraryKeyword", actor))
    except Exception as exc:
        result["attempts"].append({"api": "EditorLevelLibraryKeyword", "error": str(exc)})

    try:
        subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actor = subsystem.spawn_actor_from_class(spawn_class, target_location, target_rotation)
        actor.set_actor_label("TEMP_StatusBoard_ActorSubsystem")
        tests.append(("EditorActorSubsystem", actor))
    except Exception as exc:
        result["attempts"].append({"api": "EditorActorSubsystem", "error": str(exc)})

    for api, actor in tests:
        result["attempts"].append(
            {
                "api": api,
                "name": actor.get_name(),
                "label": actor.get_actor_label(),
                "location": vec(actor.get_actor_location()),
                "scale": vec(actor.get_actor_scale3d()),
            }
        )
        unreal.EditorLevelLibrary.destroy_actor(actor)

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False, indent=2)

print(f"Wrote {OUT_PATH}")
