import json
import os

import unreal


OUTPUT_PATH = os.path.abspath(
    os.path.join(
        unreal.Paths.project_dir(),
        "..",
        "artifacts",
        "blueprint_inventory.json",
    )
)


def safe_call(callback, default=None):
    try:
        return callback()
    except Exception:
        return default


def class_name(value):
    if value is None:
        return ""
    return safe_call(lambda: value.get_class().get_name(), type(value).__name__)


def object_path(value):
    if value is None:
        return ""
    return safe_call(lambda: value.get_path_name(), str(value))


def tag_value(asset_data, tag_name):
    value = safe_call(lambda: asset_data.get_tag_value(tag_name))
    return "" if value is None else str(value)


def graph_summary(graph):
    nodes = safe_call(lambda: graph.get_editor_property("nodes"), []) or []
    node_types = {}
    for node in nodes:
        node_type = class_name(node)
        node_types[node_type] = node_types.get(node_type, 0) + 1

    return {
        "name": safe_call(lambda: graph.get_name(), ""),
        "node_count": len(nodes),
        "node_types": dict(sorted(node_types.items())),
    }


def variable_summary(variable):
    return {
        "name": str(
            safe_call(lambda: variable.get_editor_property("var_name"), "")
        ),
        "category": str(
            safe_call(lambda: variable.get_editor_property("category"), "")
        ),
        "default_value": str(
            safe_call(
                lambda: variable.get_editor_property("default_value"), ""
            )
        ),
    }


asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
asset_registry.search_all_assets(True)

blueprint_class_names = {
    "Blueprint",
    "WidgetBlueprint",
    "AnimBlueprint",
    "EditorUtilityBlueprint",
    "BlueprintInterface",
}

records = []
all_assets = asset_registry.get_all_assets()

for asset_data in all_assets:
    package_name = str(asset_data.package_name)
    if not (
        package_name.startswith("/Game/")
        or package_name.startswith("/FF_MQTT_Sync/")
    ):
        continue

    asset_class = str(asset_data.asset_class_path.asset_name)
    if asset_class not in blueprint_class_names:
        continue

    asset = safe_call(lambda: asset_data.get_asset())
    generated_class = safe_call(
        lambda: asset.get_editor_property("generated_class")
    )
    parent_class = safe_call(
        lambda: generated_class.get_super_class() if generated_class else None
    )
    parent_class_tag = tag_value(asset_data, "ParentClass")
    generated_class_tag = tag_value(asset_data, "GeneratedClass")

    event_graphs = (
        safe_call(lambda: asset.get_editor_property("ubergraph_pages"), []) or []
    )
    function_graphs = (
        safe_call(lambda: asset.get_editor_property("function_graphs"), []) or []
    )
    macro_graphs = (
        safe_call(lambda: asset.get_editor_property("macro_graphs"), []) or []
    )
    variables = (
        safe_call(lambda: asset.get_editor_property("new_variables"), []) or []
    )

    dependencies = safe_call(
        lambda: asset_registry.get_dependencies(
            asset_data.package_name,
            unreal.AssetRegistryDependencyOptions(
                include_soft_package_references=True,
                include_hard_package_references=True,
                include_searchable_names=False,
                include_soft_management_references=False,
                include_hard_management_references=False,
            ),
        ),
        [],
    )

    record = {
        "asset_name": str(asset_data.asset_name),
        "package_name": package_name,
        "asset_class": asset_class,
        "generated_class": object_path(generated_class) or generated_class_tag,
        "parent_class": object_path(parent_class) or parent_class_tag,
        "event_graphs": [graph_summary(graph) for graph in event_graphs],
        "function_graphs": [graph_summary(graph) for graph in function_graphs],
        "macro_graphs": [graph_summary(graph) for graph in macro_graphs],
        "variables": [variable_summary(variable) for variable in variables],
        "dependencies": sorted(
            str(value)
            for value in dependencies
            if str(value).startswith("/Game/")
            or str(value).startswith("/FF_MQTT_Sync/")
            or str(value).startswith("/Script/")
        ),
    }

    records.append(record)

records.sort(key=lambda item: item["package_name"].lower())

os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
with open(OUTPUT_PATH, "w", encoding="utf-8") as output_file:
    json.dump(
        {
            "project": unreal.Paths.get_project_file_path(),
            "blueprint_count": len(records),
            "blueprints": records,
        },
        output_file,
        ensure_ascii=False,
        indent=2,
    )

unreal.log("Blueprint inventory written to {}".format(OUTPUT_PATH))
