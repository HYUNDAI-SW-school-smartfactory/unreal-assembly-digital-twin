import inspect
import unreal

lines = []
for name in ("get_actor_descs", "load_actors", "pin_actors", "unload_actors"):
    fn = getattr(unreal.WorldPartitionBlueprintLibrary, name)
    lines.append(f"{name}: {fn}")
    lines.append(f"  doc: {getattr(fn, '__doc__', '')}")
    try:
        lines.append(f"  signature: {inspect.signature(fn)}")
    except Exception as exc:
        lines.append(f"  signature error: {exc}")

raise RuntimeError("\n".join(lines))
