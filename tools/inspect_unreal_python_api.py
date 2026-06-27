import unreal

lines = []
for name in dir(unreal):
    if "WorldPartition" in name or "EditorActor" in name or "LevelEditor" in name:
        lines.append(name)

for cls_name in [
    "WorldPartitionEditorSubsystem",
    "EditorActorSubsystem",
    "LevelEditorSubsystem",
    "UnrealEditorSubsystem",
]:
    cls = getattr(unreal, cls_name, None)
    lines.append(f"\n{cls_name}: {cls}")
    if cls:
        subsystem = unreal.get_editor_subsystem(cls)
        lines.append(f"subsystem: {subsystem}")
        for method_name in dir(subsystem):
            if "actor" in method_name.lower() or "partition" in method_name.lower() or "load" in method_name.lower() or "level" in method_name.lower():
                lines.append(f"  {method_name}")

raise RuntimeError("\n".join(lines[:250]))
