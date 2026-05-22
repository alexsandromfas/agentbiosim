from sim.controllers import Params
from sim.engine import Engine
from sim.systems import ReproductionSystem
from sim.world import Camera, World


def test_agent_labels_select_and_preserve_membership():
    params = Params()
    params.set("bacteria_count", 3, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    label_id = engine.create_agent_label(name="Grupo A", color=(10, 20, 30))
    assigned = engine.assign_label_to_agents(label_id, engine.entities["bacteria"][:2])

    assert assigned == 2
    assert len(engine.get_agents_by_label(label_id)) == 2
    assert all(agent.color == (10, 20, 30) for agent in engine.entities["bacteria"][:2])

    selected = engine.select_label(label_id)

    assert len(selected) == 2
    assert set(selected) == set(engine.selected_agents)
    assert engine.selected_agent in engine.selected_agents


def test_population_starts_with_default_label_and_assign_replaces_group():
    params = Params()
    params.set("bacteria_count", 3, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    default_id = engine.ensure_default_agent_label()
    assert default_id in engine.agent_labels
    assert all(agent.label_ids == {default_id} for agent in engine.all_agents)

    label_id = engine.create_agent_label(name="Grupo B", color=(50, 60, 70))
    engine.assign_label_to_agents(label_id, engine.all_agents[:2])

    assert all(agent.label_ids == {label_id} for agent in engine.all_agents[:2])
    assert all(agent.label_ids == {default_id} for agent in engine.all_agents[2:])


def test_delete_label_removes_membership_from_agents():
    params = Params()
    params.set("bacteria_count", 2, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    label_id = engine.create_agent_label()
    engine.assign_label_to_agents(label_id, engine.all_agents)
    engine.delete_agent_label(label_id)

    assert label_id not in engine.agent_labels
    assert all(label_id not in agent.label_ids for agent in engine.all_agents)


def test_label_max_limit_blocks_reproduction_for_that_group():
    params = Params()
    params.set("bacteria_count", 2, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    params.set("bacteria_split_energy", 10.0, validate=False)
    params.set("bacteria_max_limit", 0, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    label_id = engine.create_agent_label(name="Linhagem A")
    engine.assign_label_to_agents(label_id, engine.all_agents)
    engine.agent_labels[label_id]["max_limit"] = 2
    for agent in engine.all_agents:
        agent.energy = 20.0

    births = ReproductionSystem().apply(engine.all_agents, params, agent_labels=engine.agent_labels)

    assert births == []

    engine.agent_labels[label_id]["max_limit"] = 3
    births = ReproductionSystem().apply(engine.all_agents, params, agent_labels=engine.agent_labels)

    assert len(births) == 1
    assert label_id in births[0].label_ids


def test_label_brain_reset_reinitializes_whole_group():
    params = Params()
    params.set("bacteria_count", 2, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)

    label_id = engine.ensure_default_agent_label()
    original_brains = [agent.brain for agent in engine.get_agents_by_label(label_id)]
    original_sizes = [tuple(agent.brain.sizes) for agent in engine.get_agents_by_label(label_id)]

    assert engine.reset_label_brains(label_id) == 2
    reset_agents = engine.get_agents_by_label(label_id)
    assert [tuple(agent.brain.sizes) for agent in reset_agents] == original_sizes
    assert all(agent.brain is not old for agent, old in zip(reset_agents, original_brains))
    assert all(agent.last_brain_output == [] for agent in reset_agents)
