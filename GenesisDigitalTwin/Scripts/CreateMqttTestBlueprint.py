import unreal

ASSET_PATH = "/Game/Blueprints/BP_MQTT_MachineTest"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_library = unreal.EditorAssetLibrary

if not asset_library.does_asset_exist(ASSET_PATH):
    factory = unreal.BlueprintFactory()
    parent_class = unreal.load_class(None, "/Script/FF_MQTT_Sync.MQTT_Machine_Test_Actor")
    if not parent_class:
        raise RuntimeError("MQTT_Machine_Test_Actor class could not be loaded.")
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset("BP_MQTT_MachineTest", "/Game/Blueprints", unreal.Blueprint, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

unreal.log("Loading MainMaps for MQTT test placement.")
unreal.log("Created BP_MQTT_MachineTest. Place it in MainMaps through the Unreal Editor viewport.")
