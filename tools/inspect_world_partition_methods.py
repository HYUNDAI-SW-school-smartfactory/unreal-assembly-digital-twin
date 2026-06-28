import unreal

names = []
for candidate in (
    "WorldPartitionBlueprintLibrary",
    "WorldPartitionSubsystem",
    "WorldPartitionEditorSubsystem",
    "EditorActorSubsystem",
    "LevelEditorSubsystem",
):
    obj = getattr(unreal, candidate, None)
    names.append(f"{candidate}: {obj}")
    if obj:
        for name in dir(obj):
            if "load" in name.lower() or "actor" in name.lower() or "partition" in name.lower() or "cell" in name.lower():
                names.append(f"  {name}")

raise RuntimeError("\n".join(names))
