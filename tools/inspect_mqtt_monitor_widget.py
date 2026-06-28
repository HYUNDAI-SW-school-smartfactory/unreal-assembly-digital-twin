import json
import os

import unreal


ASSET_PATH = "/Game/Widgets/WBP_MQTT_Monitor"
OUT_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "artifacts", "wbp_mqtt_monitor_widget.json")
)


def describe_widget(widget):
    record = {
        "name": str(widget.get_name()),
        "class": str(widget.get_class().get_path_name()),
    }
    for attr in ("text", "color_and_opacity", "font", "visibility"):
        try:
            record[attr] = str(widget.get_editor_property(attr))
        except Exception:
            pass
    return record


asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
generated_class = unreal.EditorAssetLibrary.load_blueprint_class(ASSET_PATH)

records = {
    "asset": ASSET_PATH,
    "generated_class": str(generated_class.get_path_name() if generated_class else None),
    "widgets": [],
    "notes": [],
}

try:
    cdo = unreal.get_default_object(generated_class)
    widget_tree = cdo.get_editor_property("widget_tree")
    all_widgets = []
    widget_tree.get_all_widgets(all_widgets)
    records["widgets"] = [describe_widget(w) for w in all_widgets]
except Exception as exc:
    records["notes"].append(f"widget_tree failed: {exc}")

os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
with open(OUT_PATH, "w", encoding="utf-8") as f:
    json.dump(records, f, indent=2, ensure_ascii=False)

print(f"Wrote {OUT_PATH}")
