import csv
import json
import os
from types import SimpleNamespace

from sim.controllers import Params
from sim.ui import SimulationUI
from sim.world import Camera, World


def test_ui_params_are_saved_outside_sim_without_test_params(tmp_path):
    target_csv = tmp_path / "config" / "user_params.csv"

    ui = SimulationUI.__new__(SimulationUI)
    ui.params = Params()
    ui.engine = SimpleNamespace(
        camera=Camera(),
        world=World(100, 80),
        entities={"foods": [], "bacteria": [], "predators": []},
    )
    ui._ui_params_csv = str(target_csv)
    ui.widgets = {
        "test_param_1": object(),
        "substrate_shape": object(),
        "auto_export_substrate": object(),
    }
    values = {
        "test_param_1": 99,
        "substrate_shape": "circular",
        "auto_export_substrate": False,
    }
    ui._get_widget_value = lambda name: values.get(name)
    ui.params.set("background_gradient_enabled", True, validate=False)
    ui.params.set("background_color_top", (1, 2, 3), validate=False)
    ui.params.set("background_color_bottom", (4, 5, 6), validate=False)
    ui.params.set("substrate_gradient_enabled", True, validate=False)
    ui.params.set("substrate_color_top", (7, 8, 9), validate=False)
    ui.params.set("substrate_color_bottom", (10, 11, 12), validate=False)
    ui.params.set("substrate_border_enabled", False, validate=False)
    ui.params.set("substrate_border_color", (13, 14, 15), validate=False)

    SimulationUI.save_ui_params(ui)

    rows = list(csv.DictReader(open(target_csv, encoding="utf-8")))
    names = [row["name"] for row in rows]
    by_name = {row["name"]: row["value"] for row in rows}
    assert "test_param_1" not in names
    assert names.count("substrate_shape") == 1
    assert by_name["background_gradient_enabled"] == "True"
    assert json.loads(by_name["background_color_top"]) == [1, 2, 3]
    assert by_name["substrate_gradient_enabled"] == "True"
    assert json.loads(by_name["substrate_border_color"]) == [13, 14, 15]
    assert by_name["substrate_border_enabled"] == "False"
    assert os.path.normpath(str(target_csv)).endswith(os.path.join("config", "user_params.csv"))


def test_ui_params_loader_keeps_legacy_fallback_and_ignores_test_params(tmp_path):
    legacy_csv = tmp_path / "ui_params.csv"
    with open(legacy_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["name", "value"])
        writer.writeheader()
        writer.writerow({"name": "test_param_1", "value": "123"})
        writer.writerow({"name": "substrate_shape", "value": "circular"})
        writer.writerow({"name": "background_gradient_enabled", "value": "True"})
        writer.writerow({"name": "background_color_top", "value": "[21, 22, 23]"})
        writer.writerow({"name": "substrate_gradient_enabled", "value": "True"})
        writer.writerow({"name": "substrate_color_bottom", "value": "[31, 32, 33]"})
        writer.writerow({"name": "substrate_border_enabled", "value": "False"})
        writer.writerow({"name": "substrate_border_color", "value": "[41, 42, 43]"})

    ui = SimulationUI.__new__(SimulationUI)
    ui.params = Params()
    ui.engine = SimpleNamespace(
        camera=Camera(),
        world=World(100, 80),
        entities={"foods": [], "bacteria": [], "predators": []},
    )
    ui._ui_params_csv = str(tmp_path / "missing" / "user_params.csv")
    ui._legacy_ui_params_csv = str(legacy_csv)
    ui.widgets = {}
    ui._get_widget_value = lambda name: False
    ui._schedule_next_auto_export = lambda initial=False: None

    SimulationUI._load_ui_params_csv(ui)

    assert ui.params.get("substrate_shape") == "circular"
    assert ui.params.get("background_gradient_enabled") is True
    assert ui.params.get("background_color_top") == (21, 22, 23)
    assert ui.params.get("substrate_gradient_enabled") is True
    assert ui.params.get("substrate_color_bottom") == (31, 32, 33)
    assert ui.params.get("substrate_border_enabled") is False
    assert ui.params.get("substrate_border_color") == (41, 42, 43)
