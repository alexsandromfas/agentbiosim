from sim.controllers import Params
from sim.engine import Engine
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
