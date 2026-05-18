from pathlib import Path

from sim.controllers import Params
from sim.engine import Engine
from sim.ui import SimulationUI
from sim.world import Camera, World


class _FakeCheck:
    def __init__(self, checked=False):
        self.checked = checked

    def setChecked(self, value):
        self.checked = bool(value)


def _ui(engine: Engine):
    ui = SimulationUI.__new__(SimulationUI)
    ui.params = engine.params
    ui.engine = engine
    ui.pygame_view = type(
        "FakeView",
        (),
        {
            "active_tool": "food",
            "brush_width": 16.0,
            "brush_color": (95, 95, 105),
            "brush_erase": False,
        },
    )()
    ui.widgets = {"paused": _FakeCheck(False)}
    ui._auto_export_timer = None
    ui._ui_params_csv = ""
    ui._legacy_ui_params_csv = ""
    ui._get_widget_value = lambda name: ui.widgets[name].checked if name == "paused" else None
    ui._set_widget_value = lambda name, value: None
    return ui


def _engine():
    params = Params()
    for key, value in {
        "bacteria_count": 2,
        "bacteria_min_limit": 0,
        "predators_enabled": True,
        "predator_count": 1,
        "predator_min_limit": 0,
        "food_target": 3,
        "random_seed": 789,
    }.items():
        params.set(key, value, validate=False)
    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start(initialize=True)
    engine.paint_obstacle(80, 80, 95, 80, 8, (90, 90, 100))
    return engine


def test_biosim_snapshot_roundtrip_preserves_core_state(tmp_path: Path):
    source = _engine()
    source.selected_agent = source.all_agents[0]
    source.loaded_agent_prototypes["demo"] = {"type": "bacteria", "brain_sizes": "[18, 2]"}
    source.current_agent_prototype = "demo"
    path = tmp_path / "state.biosim"

    _ui(source)._export_substrate(path_override=str(path), file_type="biosim")

    target = Engine(World(10, 10), Camera(), Params(), headless=True)
    _ui(target)._import_substrate(str(path))

    assert len(target.entities["bacteria"]) == len(source.entities["bacteria"])
    assert len(target.entities["predators"]) == len(source.entities["predators"])
    assert len(target.entities["foods"]) == len(source.entities["foods"])
    assert len(target.obstacles) == len(source.obstacles)
    assert target.current_agent_prototype == "demo"
    assert target.selected_agent is target.all_agents[0]
