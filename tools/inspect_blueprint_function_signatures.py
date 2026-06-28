import inspect
import json
import unreal


targets = [
    (unreal.BlueprintEditorLibrary, "create_blueprint_asset_with_parent"),
    (unreal.BlueprintEditorLibrary, "add_function_graph"),
    (unreal.BlueprintEditorLibrary, "find_event_graph"),
    (unreal.BlueprintEditorLibrary, "find_graph"),
    (unreal.BlueprintEditorLibrary, "compile_blueprint"),
]

data = {}
for owner, name in targets:
    func = getattr(owner, name)
    entry = {
        "repr": repr(func),
        "doc": getattr(func, "__doc__", ""),
    }
    try:
        entry["signature"] = str(inspect.signature(func))
    except Exception as exc:
        entry["signature_error"] = str(exc)
    data[name] = entry

out = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir() + "../artifacts/blueprint_function_signatures.json"
)
with open(out, "w", encoding="utf-8") as handle:
    json.dump(data, handle, ensure_ascii=False, indent=2)

unreal.log("[inspect_blueprint_function_signatures] wrote {}".format(out))
