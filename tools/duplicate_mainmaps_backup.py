import unreal


SOURCE_MAP = "/Game/Maps/MainMaps"
BACKUP_MAP = "/Game/Maps/MainMaps_Old"


if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
    raise RuntimeError("Source map does not exist: {}".format(SOURCE_MAP))

if unreal.EditorAssetLibrary.does_asset_exist(BACKUP_MAP):
    unreal.log("Backup map already exists: {}".format(BACKUP_MAP))
else:
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(
        SOURCE_MAP,
        BACKUP_MAP,
    )
    if duplicated is None:
        raise RuntimeError(
            "Failed to duplicate {} to {}".format(SOURCE_MAP, BACKUP_MAP)
        )

    if not unreal.EditorAssetLibrary.save_asset(
        BACKUP_MAP,
        only_if_is_dirty=False,
    ):
        raise RuntimeError("Failed to save backup map: {}".format(BACKUP_MAP))

unreal.log("MainMaps backup is ready: {}".format(BACKUP_MAP))
