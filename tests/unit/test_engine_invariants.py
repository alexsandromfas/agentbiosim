import math

from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _small_engine() -> Engine:
    params = Params()
    for key, value in {
        "bacteria_count": 6,
        "bacteria_min_limit": 0,
        "bacteria_max_limit": 20,
        "predators_enabled": True,
        "predator_count": 2,
        "predator_min_limit": 0,
        "predator_max_limit": 5,
        "food_target": 12,
        "random_seed": 123,
    }.items():
        params.set(key, value, validate=False)

    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def test_engine_preserves_agent_lists_and_numeric_state():
    engine = _small_engine()
    for _ in range(5):
        engine.step(1.0 / 60.0)

    expected_agents = len(engine.entities["bacteria"]) + len(engine.entities["predators"])
    assert len(engine.all_agents) == expected_agents
    assert len(set(map(id, engine.all_agents))) == len(engine.all_agents)
    assert hasattr(engine, "state_lock")

    for obj in engine.all_agents + engine.entities["foods"]:
        assert math.isfinite(float(obj.x))
        assert math.isfinite(float(obj.y))
        assert math.isfinite(float(obj.r))
        if hasattr(obj, "energy"):
            assert math.isfinite(float(obj.energy))
            assert float(obj.energy) >= 0.0
