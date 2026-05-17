from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _engine() -> Engine:
    params = Params()
    for key, value in {
        "bacteria_count": 0,
        "bacteria_min_limit": 0,
        "predators_enabled": False,
        "food_target": 0,
        "random_seed": 123,
    }.items():
        params.set(key, value, validate=False)
    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def test_obstacle_blocks_food_and_manual_bacteria_spawn():
    engine = _engine()
    engine.paint_obstacle(100, 90, 100, 90, 12, (90, 90, 100), erase=False)

    assert len(engine.obstacles) > 0
    assert engine.add_food_at(100, 90) is None
    assert engine.add_bacteria_at(100, 90) is None
    assert len(engine.entities["foods"]) == 0
    assert len(engine.entities["bacteria"]) == 0


def test_painting_obstacle_removes_existing_food_overlap():
    engine = _engine()
    food = engine.add_food_at(100, 90)

    assert food is not None
    assert len(engine.entities["foods"]) == 1

    engine.paint_obstacle(100, 90, 100, 90, 20, (90, 90, 100), erase=False)

    assert len(engine.entities["foods"]) == 0


def test_obstacle_collision_pushes_agents_out():
    engine = _engine()
    agent = engine.add_bacteria_at(100, 90)
    assert agent is not None

    engine.paint_obstacle(100, 90, 100, 90, 14, (90, 90, 100), erase=False)
    engine._resolve_obstacle_collisions()

    assert not engine.obstacles.circle_overlaps(agent.x, agent.y, agent.r)


def test_obstacle_erase_removes_barrier():
    engine = _engine()
    engine.paint_obstacle(100, 90, 120, 90, 10, (90, 90, 100), erase=False)
    assert len(engine.obstacles) > 0

    engine.paint_obstacle(100, 90, 120, 90, 20, (90, 90, 100), erase=True)

    assert len(engine.obstacles) == 0
