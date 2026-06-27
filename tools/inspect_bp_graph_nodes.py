import json
import unreal


TARGETS = [
    "/Game/Blueprints/AGV/BP_AGV",
    "/Game/Blueprints/AGV/BP_BatteryLift",
    "/Game/Blueprints/BP_TireAssemblyCell",
    "/Game/Blueprints/BP_TireAssemblyCell_L",
]


def node_info(node):
    info = {
        "class": node.get_class().get_path_name(),
        "name": node.get_name(),
        "title": "",
        "members": [],
        "properties": {},
    }
    try:
        info["title"] = str(node.get_node_title(unreal.NodeTitleType.FULL_TITLE))
    except Exception as exc:
        info["title_error"] = str(exc)
    for member in dir(node):
        lower = member.lower()
        if "destroy" in lower or "break" in lower or "pin" in lower or "function" in lower or "event" in lower or "node" in lower:
            info["members"].append(member)
    for prop in ["event_reference", "function_reference", "custom_function_name", "event_signature_name", "delegate_property_name"]:
        try:
            info["properties"][prop] = str(node.get_editor_property(prop))
        except Exception:
            pass
    return info


def graph_info(graph):
    info = {
        "name": graph.get_name(),
        "class": graph.get_class().get_path_name(),
        "nodes": [],
    }
    try:
        nodes = graph.get_editor_property("nodes")
    except Exception:
        nodes = []
    for node in nodes:
        info["nodes"].append(node_info(node))
    return info


def main():
    report = {}
    for path in TARGETS:
        bp = unreal.EditorAssetLibrary.load_asset(path)
        if not bp:
            report[path] = {"error": "missing"}
            continue
        graphs = []
        try:
            event_graph = unreal.BlueprintEditorLibrary.find_event_graph(bp)
            if event_graph:
                graphs.append(graph_info(event_graph))
        except Exception as exc:
            graphs.append({"error": "find_event_graph failed: {}".format(exc)})
        try:
            for graph_name in ["ReceiveBeginPlay", "EventGraph", "ConstructionScript", "UserConstructionScript"]:
                graph = unreal.BlueprintEditorLibrary.find_graph(bp, graph_name)
                if graph and graph.get_name() not in [g.get("name") for g in graphs]:
                    graphs.append(graph_info(graph))
        except Exception as exc:
            graphs.append({"error": "find_graph failed: {}".format(exc)})
        report[path] = {"graphs": graphs}

    out = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + "../artifacts/bp_graph_nodes.json"
    )
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[inspect_bp_graph_nodes] wrote {}".format(out))


main()
