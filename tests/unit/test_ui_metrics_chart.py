from sim.controllers import Params
from sim.engine import Engine
from sim.ui import SimulationUI
from sim.world import Camera, World


class _FakeChart:
    def __init__(self):
        self.history = None

    def isVisible(self):
        return True

    def set_history(self, history):
        self.history = history


def _ui_with_engine(paused: bool = False):
    params = Params()
    params.set("bacteria_count", 2, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    params.set("paused", paused, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    ui = SimulationUI.__new__(SimulationUI)
    ui.params = params
    ui.engine = engine
    ui.metrics_chart = _FakeChart()
    ui._metrics_history = []
    ui._chart_known_label_ids = ()
    ui._refresh_chart_metric_checkboxes = lambda: None
    return ui, engine


def test_metrics_chart_does_not_sample_while_paused():
    ui, _engine = _ui_with_engine(paused=True)

    ui._update_metrics_chart()

    assert ui._metrics_history == []
    assert ui.metrics_chart.history is None


def test_metrics_chart_uses_single_group_intelligence_series_per_label():
    ui, engine = _ui_with_engine(paused=False)
    label_id = engine.create_agent_label(name="Grupo A", color=(10, 20, 30))
    engine.assign_label_to_agents(label_id, engine.entities["bacteria"])

    keys = [key for key, _label, _color in ui._current_chart_metric_defs()]
    ui._update_metrics_chart()

    assert f"label_{label_id}_smart" in keys
    assert f"label_{label_id}_local" not in keys
    assert f"label_{label_id}_global" not in keys
    assert "smart_local" not in ui._metrics_history[-1]
    assert "smart_global" not in ui._metrics_history[-1]
    assert f"label_{label_id}_smart" in ui._metrics_history[-1]
