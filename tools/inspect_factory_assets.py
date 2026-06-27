import json
import os

import unreal


ASSET_PATHS = [
    "/Game/Meshes/Robot/SK_RoboArm04",
    "/Game/Meshes/RobotLarge/SK_TruckWelding_Anim",
    "/Game/Blueprints/AGV/BP_AGV",
    "/Game/Blueprints/AGV/BP_BatteryLift",
    "/Game/Blueprints/BP_TireAssemblyCell",
    "/Game/Blueprints/BP_TireAssemblyCell_L",
    "/Game/Blueprints/BP_HangerVehicle",
    "/Game/Blueprints/BP_Car",
    "/Game/Genesis_Model_New/Blueprints/BP_Car",
    "/Game/Genesis_Model_New/Blueprints/BP_LineVehicle",
]

OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "factory_asset_types.json",
    )
)

records = []
for asset_path in ASSET_PATHS:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    records.append(
        {
            "path": asset_path,
            "exists": asset is not None,
            "class": asset.get_class().get_path_name() if asset else "",
            "object_path": asset.get_path_name() if asset else "",
        }
    )

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(records, output_file, ensure_ascii=False, indent=2)

unreal.log("Factory asset types written to {}".format(OUTPUT_PATH))
