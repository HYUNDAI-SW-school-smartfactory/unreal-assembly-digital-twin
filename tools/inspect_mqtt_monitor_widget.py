import json
import unreal


ASSET_PATH = "/Game/Widgets/WBP_MQTT_Monitor"
OUT_PATH = "C:/Users/한국전파진흥협회/unreal-assembly-digital-twin/artifacts/wbp_mqtt_monitor_widget.json"


def describe_widget(widget):
    record = {
        "name": str(widget.get_name()),
        "class": str(widget.get_class().get_path_name()),
    }
    for attr in ("text", "color_and_opacity", "font"):
        try:
            value = widget.get_editor_property(attr)
            record[attr] = str(value)
        except Exception:
            pass
    return record


asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
generated_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSET_PATH)

records = {
    "asset": ASSET_PATH,
    "generated_class": str(generated_class.get_path_name() if generated_class else None),
    "widgets": [],
    "asset_class": str(asset.get_class().get_path_name() if asset else None),
    "asset_attrs": [],
    "notes": [],
}

try:
    records["asset_attrs"] = [name for name in dir(asset) if "widget" in name.lower() or "tree" in name.lower()]
except Exception as exc:
    records["notes"].append(f"dir(asset) failed: {exc}")

try:
    cdo = unreal.get_default_object(generated_class)
    widget_tree = cdo.get_editor_property("widget_tree")
    all_widgets = []
    widget_tree.get_all_widgets(all_widgets)
    records["widgets"] = [describe_widget(w) for w in all_widgets]
except Exception as exc:
    records["notes"].append(f"widget_tree failed: {exc}")

for prop_name in ("widget_tree", "WidgetTree", "preview", "preview_widget"):
    try:
        value = asset.get_editor_property(prop_name)
        records[f"asset_property_{prop_name}"] = str(value)
        try:
            all_widgets = []
            value.get_all_widgets(all_widgets)
            records[f"asset_property_{prop_name}_widgets"] = [describe_widget(w) for w in all_widgets]
        except Exception as inner_exc:
            records["notes"].append(f"{prop_name}.get_all_widgets failed: {inner_exc}")
    except Exception as exc:
        records["notes"].append(f"asset property {prop_name} failed: {exc}")

with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(records, f, indent=2, ensure_ascii=False)

print(f"Wrote {OUT_PATH}")
