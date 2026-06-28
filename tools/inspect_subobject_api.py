import json
import os

import unreal


OUTPUT_PATH = os.path.abspath(
    os.path.join(unreal.Paths.project_dir(), "..", "artifacts", "subobject_api.json")
)

targets = [
    "SubobjectDataSubsystem",
    "SubobjectDataBlueprintFunctionLibrary",
    "SubobjectData",
    "SubobjectDataHandle",
]

data = {}
for name in targets:
    obj = getattr(unreal, name, None)
    data[name] = [] if obj is None else dir(obj)

bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_TireConveyor")
data["bp_class"] = bp.get_class().get_path_name() if bp else None
try:
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    data["handle_count"] = len(handles)
    data["handle_type"] = type(handles[0]).__name__ if handles else None
    data["handle_dir"] = dir(handles[0]) if handles else []
except Exception as exc:
    data["gather_error"] = str(exc)

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as handle:
    json.dump(data, handle, ensure_ascii=False, indent=2)

unreal.log("[inspect_subobject_api] wrote {}".format(OUTPUT_PATH))
