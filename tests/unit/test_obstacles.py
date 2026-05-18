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


def test_obstacle_brush_can_seal_rectangular_world_edge():
    engine = _engine()

    added = engine.paint_obstacle(0, -30, 0, 210, 12, (90, 90, 100), erase=False)

    assert added > 0
    assert engine.obstacles.circle_overlaps(8, 90, 8)
    assert not engine.can_place_circle(8, 90, 8)


def test_obstacle_brush_clamps_outside_stroke_to_circular_edge():
    params = Params()
    for key, value in {
        "bacteria_count": 0,
        "bacteria_min_limit": 0,
        "predators_enabled": False,
        "food_target": 0,
    }.items():
        params.set(key, value, validate=False)
    engine = Engine(World(200, 200, shape="circular", radius=80), Camera(), params, headless=True)
    engine.start(initialize=True)

    added = engine.paint_obstacle(100, 100, 220, 100, 10, (90, 90, 100), erase=False)

    assert added > 0
    assert engine.obstacles.circle_overlaps(180, 100, 4)


def test_obstacle_resolution_rechecks_after_displacement():
    engine = _engine()
    agent = engine.add_bacteria_at(100, 90)
    assert agent is not None

    engine.paint_obstacle(104, 90, 104, 90, 10, (90, 90, 100), erase=False)
    agent.x = 104
    agent.y = 90
    resolved = engine._resolve_obstacle_collisions(max_iterations=3)

    assert resolved > 0
    assert not engine.obstacles.circle_overlaps(agent.x, agent.y, agent.r)
