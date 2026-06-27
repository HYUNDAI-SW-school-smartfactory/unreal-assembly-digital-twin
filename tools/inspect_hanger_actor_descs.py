import unreal

MAP_PATH = "/Game/Maps/MainMaps"
TARGET = "Genesis_Preplaced_HangerLine"

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
level_editor.load_level(MAP_PATH)

descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
lines = [f"desc_count={len(descs) if descs else 0}"]

for desc in descs or []:
    text = str(desc)
    hit = TARGET.lower() in text.lower()
    methods = []
    if hit:
        for name in dir(desc):
            low = name.lower()
            if "label" in low or "name" in low or "guid" in low or "actor" in low or "path" in low:
                methods.append(name)
        lines.append(f"DESC={text}")
        for name in methods:
            attr = getattr(desc, name)
            if callable(attr):
                try:
                    value = attr()
                except Exception as exc:
                    value = f"<ERR {exc}>"
            else:
                value = attr
            lines.append(f"  {name}: {value}")

raise RuntimeError("\n".join(lines[:200]))
