import unreal

ASSET_PATH = "/Game/Blueprints/BP_FactoryDisplayBoard"

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log("BP_FactoryDisplayBoard already exists.")
else:
    parent_class = unreal.load_class(None, "/Script/FF_MQTT_Sync.Factory_Dashboard_Board_Actor")
    if not parent_class:
        raise RuntimeError("Factory_Dashboard_Board_Actor class could not be loaded.")

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_FactoryDisplayBoard", "/Game/Blueprints", unreal.Blueprint, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    unreal.log("Created BP_FactoryDisplayBoard. Place it in MainMaps through the Unreal Editor viewport.")
