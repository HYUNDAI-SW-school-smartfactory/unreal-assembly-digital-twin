import unreal


ASSET_PATH = "/Game/Blueprints/BP_GenesisFactoryAssemblyManager"
PARENT_CLASS_PATH = "/Script/GenesisDigitalTwin.GenesisFactoryAssemblyManager"


if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log("Factory manager Blueprint already exists: {}".format(ASSET_PATH))
else:
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        raise RuntimeError("Could not load {}".format(PARENT_CLASS_PATH))

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)

    blueprint = asset_tools.create_asset(
        "BP_GenesisFactoryAssemblyManager",
        "/Game/Blueprints",
        unreal.Blueprint,
        factory,
    )
    if blueprint is None:
        raise RuntimeError("Failed to create BP_GenesisFactoryAssemblyManager")

    unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False)
    unreal.log("Created {}".format(ASSET_PATH))
