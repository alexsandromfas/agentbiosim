import math

from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _engine(time_scale: float) -> Engine:
    params = Params()
    for key, value in {
        "time_scale": time_scale,
        "physics_steps_per_second": 120,
        "max_physics_steps_per_frame": 200,
        "bacteria_count": 1,
        "bacteria_min_limit": 0,
        "bacteria_max_limit": 80,
        "bacteria_split_energy": 10_000.0,
        "bacteria_death_energy": 0.0,
        "predators_enabled": False,
        "food_target": 0,
        "random_seed": 9876,
        "population_min_rescue_enabled": False,
    }.items():
        params.set(key, value, validate=False)
    engine = Engine(World(300, 220), Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def _run_to(engine: Engine, target_sim_time: float, real_dt: float):
    while engine.total_simulation_time < target_sim_time - 1e-12:
        engine.step(real_dt)


def _state(engine: Engine):
    return [
        (
            round(agent.x, 9),
            round(agent.y, 9),
            round(agent.vx, 9),
            round(agent.vy, 9),
            round(agent.energy, 9),
        )
        for agent in engine.all_agents
    ]


def test_time_scale_changes_wall_grouping_not_physics():
    slow = _engine(time_scale=1.0)
    _run_to(slow, 1.0, 1.0 / 60.0)

    fast = _engine(time_scale=5.0)
    _run_to(fast, 1.0, 1.0 / 60.0)

    assert math.isclose(slow.total_simulation_time, fast.total_simulation_time, abs_tol=1e-12)
    assert _state(slow) == _state(fast)
    assert len(slow.entities["foods"]) == len(fast.entities["foods"]) == 0
