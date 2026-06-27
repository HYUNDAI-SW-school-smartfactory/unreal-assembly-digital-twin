import json
import unreal


TARGET = "/Game/Blueprints/BP_TireConveyor"


def safe_str(value):
    try:
        return str(value)
    except Exception as exc:
        return "<{}>".format(exc)


def node_info(node):
    info = {
        "name": node.get_name(),
        "class": node.get_class().get_path_name(),
        "title": "",
        "props": {},
        "pins": [],
        "members": [],
    }
    try:
        info["title"] = str(node.get_node_title(unreal.NodeTitleType.FULL_TITLE))
    except Exception as exc:
        info["title_error"] = str(exc)

    for prop in [
        "function_reference",
        "member_reference",
        "event_reference",
        "custom_function_name",
        "node_comment",
    ]:
        try:
            info["props"][prop] = safe_str(node.get_editor_property(prop))
        except Exception:
            pass

    for member in dir(node):
        lower = member.lower()
        if "pin" in lower or "break" in lower or "destroy" in lower or "function" in lower or "node" in lower:
            info["members"].append(member)

    try:
        for pin in node.pins:
            info["pins"].append({
                "name": safe_str(pin.get_editor_property("pin_name")),
                "default": safe_str(pin.get_editor_property("default_value")),
                "linked_to": [safe_str(link.get_outer().get_name()) for link in pin.get_editor_property("linked_to")],
            })
    except Exception as exc:
        info["pins_error"] = str(exc)

    return info


def graph_info(graph):
    info = {
        "name": graph.get_name(),
        "class": graph.get_class().get_path_name(),
        "nodes": [],
    }
    for prop in ["nodes"]:
        try:
            nodes = graph.get_editor_property(prop)
        except Exception:
            nodes = []
        for node in nodes:
            info["nodes"].append(node_info(node))
    return info


def main():
    bp = unreal.EditorAssetLibrary.load_asset(TARGET)
    report = {
        "target": TARGET,
        "loaded": bool(bp),
        "graphs": [],
    }
    if bp:
        for graph_name in ["EventGraph", "ConstructionScript", "UserConstructionScript"]:
            graph = unreal.BlueprintEditorLibrary.find_graph(bp, graph_name)
            if graph:
                report["graphs"].append(graph_info(graph))
        try:
            event_graph = unreal.BlueprintEditorLibrary.find_event_graph(bp)
            if event_graph and event_graph.get_name() not in [g["name"] for g in report["graphs"]]:
                report["graphs"].append(graph_info(event_graph))
        except Exception as exc:
            report["event_graph_error"] = str(exc)

        try:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            report["compile"] = "ok"
        except Exception as exc:
            report["compile"] = "failed: {}".format(exc)

    out = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_dir() + "../artifacts/tire_conveyor_compile_inspect.json"
    )
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
    unreal.log("[inspect_tire_conveyor_compile_error] wrote {}".format(out))


main()
