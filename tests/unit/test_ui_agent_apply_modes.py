from sim.controllers import Params
from sim.engine import Engine
from sim.ui import SimulationUI
from sim.world import Camera, World


def _engine_with_agents() -> Engine:
    params = Params()
    for key, value in {
        "bacteria_count": 3,
        "bacteria_min_limit": 0,
        "bacteria_max_limit": 10,
        "predators_enabled": False,
        "food_target": 0,
        "random_seed": 321,
    }.items():
        params.set(key, value, validate=False)
    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def _ui_for_values(engine: Engine, values: dict):
    ui = SimulationUI.__new__(SimulationUI)
    ui.params = engine.params
    ui.engine = engine
    ui.widgets = {name: object() for name in values}
    ui._get_widget_value = lambda name: values[name]
    return ui


def test_apply_bacteria_template_only_does_not_touch_live_agents():
    engine = _engine_with_agents()
    old_arches = [tuple(agent.brain.sizes) for agent in engine.entities["bacteria"]]
    old_radius = engine.entities["bacteria"][0].r

    ui = _ui_for_values(engine, {
        "bacteria_hidden_layers": 1,
        "bacteria_neurons_layer_1": 7,
        "bacteria_body_size": 12.0,
    })
    ui.apply_bacteria_params("template", confirm_structural=False)

    assert engine.params.get("bacteria_hidden_layers") == 1
    assert [tuple(agent.brain.sizes) for agent in engine.entities["bacteria"]] == old_arches
    assert engine.entities["bacteria"][0].r == old_radius


def test_apply_bacteria_to_all_alive_rebuilds_structural_brains_when_allowed():
    engine = _engine_with_agents()
    engine.entities["bacteria"][0].energy = 150.0

    ui = _ui_for_values(engine, {
        "bacteria_hidden_layers": 1,
        "bacteria_neurons_layer_1": 7,
        "bacteria_retina_count": 12,
        "bacteria_body_size": 12.0,
        "bacteria_max_speed": 123.0,
        "bacteria_energy_cap": 90.0,
    })
    ui.apply_bacteria_params("all_alive", confirm_structural=False)

    for agent in engine.entities["bacteria"]:
        assert tuple(agent.brain.sizes) == (12, 7, 2)
        assert agent.sensor.retina_count == 12
        assert agent.locomotion.max_speed == 123.0
        assert agent.r == 12.0
    assert engine.entities["bacteria"][0].energy == 90.0


def test_apply_bacteria_to_selected_changes_only_selected_agent():
    engine = _engine_with_agents()
    selected = engine.entities["bacteria"][0]
    untouched = engine.entities["bacteria"][1]
    old_untouched_arch = tuple(untouched.brain.sizes)
    old_untouched_radius = untouched.r
    engine.selected_agent = selected

    ui = _ui_for_values(engine, {
        "bacteria_hidden_layers": 1,
        "bacteria_neurons_layer_1": 9,
        "bacteria_body_size": 13.0,
    })
    ui.apply_bacteria_params("selected", confirm_structural=False)

    assert tuple(selected.brain.sizes) == (18, 9, 2)
    assert selected.r == 13.0
    assert tuple(untouched.brain.sizes) == old_untouched_arch
    assert untouched.r == old_untouched_radius


def test_apply_bacteria_to_selected_group_changes_all_selected_agents():
    engine = _engine_with_agents()
    selected = engine.entities["bacteria"][:2]
    untouched = engine.entities["bacteria"][2]
    old_untouched_radius = untouched.r
    engine.selected_agent = selected[0]
    engine.selected_agents = set(selected)

    ui = _ui_for_values(engine, {
        "bacteria_body_size": 14.0,
    })
    ui.apply_bacteria_params("selected", confirm_structural=False)

    assert [agent.r for agent in selected] == [14.0, 14.0]
    assert untouched.r == old_untouched_radius


def test_apply_population_params_keeps_running_population_and_neutralizes_legacy_limits():
    engine = _engine_with_agents()
    for idx, agent in enumerate(engine.entities["bacteria"]):
        agent.energy = float(idx + 1)
    engine.selected_agent = engine.entities["bacteria"][0]
    engine.selected_agents = {engine.entities["bacteria"][0], engine.entities["bacteria"][2]}

    ui = _ui_for_values(engine, {
        "bacteria_count": 3,
        "bacteria_min_limit": 0,
        "bacteria_max_limit": 2,
        "predators_enabled": False,
        "predator_count": 0,
        "predator_min_limit": 0,
        "predator_max_limit": 10,
        "population_min_rescue_enabled": True,
    })
    ui.apply_population_params()

    assert len(engine.entities["bacteria"]) == 3
    assert len(engine.all_agents) == 3
    assert engine.params.get("bacteria_max_limit") == 0
    assert engine.params.get("predator_max_limit") == 0
    assert engine.selected_agent in engine.all_agents
    assert all(agent in engine.all_agents for agent in engine.selected_agents)


def test_group_color_apply_does_not_rebuild_mismatched_brain():
    engine = _engine_with_agents()
    selected = engine.entities["bacteria"][0]
    old_brain = selected.brain
    old_arch = tuple(selected.brain.sizes)
    engine.selected_agent = selected
    engine.selected_agents = {selected}
    engine.params.set("bacteria_color", (10, 20, 30), validate=False)

    ui = _ui_for_values(engine, {
        "bacteria_hidden_layers": 1,
        "bacteria_neurons_layer_1": 9,
    })
    ui.apply_agent_param_group("bacteria", "color", "selected")

    assert selected.color == (10, 20, 30)
    assert selected.brain is old_brain
    assert tuple(selected.brain.sizes) == old_arch
