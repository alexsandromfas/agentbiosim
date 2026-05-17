from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def test_dragged_agent_is_frozen_during_engine_step():
    params = Params()
    for key, value in {
        "bacteria_count": 1,
        "bacteria_min_limit": 0,
        "bacteria_max_limit": 5,
        "predators_enabled": False,
        "food_target": 0,
        "random_seed": 456,
    }.items():
        params.set(key, value, validate=False)

    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start(initialize=True)
    agent = engine.entities["bacteria"][0]
    agent.vx = 100.0
    agent.vy = 50.0
    x0, y0, age0 = agent.x, agent.y, agent.age

    engine.dragged_object = agent
    engine.step(1.0 / 30.0)

    assert agent.x == x0
    assert agent.y == y0
    assert agent.age == age0
    assert agent.vx == 0.0
    assert agent.vy == 0.0
