from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _engine_with_positioned_agents():
    params = Params()
    params.set("bacteria_count", 3, validate=False)
    params.set("food_target", 0, validate=False)
    params.set("random_seed", 7, validate=False)
    engine = Engine(World(300, 300), Camera(), params, headless=True)
    engine.start(initialize=True)
    for agent, pos in zip(engine.entities["bacteria"], [(30, 30), (80, 80), (240, 240)]):
        agent.x, agent.y = pos
    return engine


def test_rect_selection_persists_selected_agent_set():
    engine = _engine_with_positioned_agents()

    engine._execute_command("select_agents_rect", x0=0, y0=0, x1=120, y1=120)

    assert len(engine.selected_agents) == 2
    assert engine.selected_agent in engine.selected_agents


def test_lasso_selection_uses_current_positions_then_persists():
    engine = _engine_with_positioned_agents()
    polygon = [(0, 0), (120, 0), (120, 120), (0, 120)]

    engine._execute_command("select_agents_lasso", points=polygon)
    selected = set(engine.selected_agents)
    for agent in selected:
        agent.x += 150

    assert len(selected) == 2
    assert engine.selected_agents == selected


def test_remove_selected_agents_deletes_only_selected_organisms():
    engine = _engine_with_positioned_agents()
    selected = set(engine.entities["bacteria"][:2])
    survivor = engine.entities["bacteria"][2]
    engine.set_selected_agents(selected)

    removed = engine.remove_selected_agents()

    assert removed == 2
    assert engine.entities["bacteria"] == [survivor]
    assert engine.all_agents == [survivor]
    assert engine.selected_agent is None
    assert engine.selected_agents == set()
