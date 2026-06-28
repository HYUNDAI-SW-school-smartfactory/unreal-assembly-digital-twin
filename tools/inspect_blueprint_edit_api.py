import json
import unreal


def members(name):
    obj = getattr(unreal, name, None)
    if obj is None:
        return []
    return [item for item in dir(obj) if "blueprint" in item.lower() or "graph" in item.lower() or "node" in item.lower() or "event" in item.lower() or "function" in item.lower()]


classes = [
    "BlueprintEditorLibrary",
    "BlueprintEditorSubsystem",
    "BlueprintFactory",
    "KismetCompilerLibrary",
    "KismetEditorUtilities",
    "SubobjectDataSubsystem",
    "EditorAssetLibrary",
    "AssetToolsHelpers",
]

data = {name: members(name) for name in classes}
data["all_unreal_names_matching"] = [
    name for name in dir(unreal)
    if "Blueprint" in name or "Graph" in name or "K2" in name or "Kismet" in name
]

out = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir() + "../artifacts/blueprint_edit_api.json"
)
with open(out, "w", encoding="utf-8") as handle:
    json.dump(data, handle, ensure_ascii=False, indent=2)

unreal.log("[inspect_blueprint_edit_api] wrote {}".format(out))
