from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _circular_large_radius_engine() -> Engine:
    params = Params()
    for key, value in {
        "substrate_shape": "circular",
        "world_w": 1000.0,
        "world_h": 700.0,
        "substrate_radius": 1000.0,
        "bacteria_count": 0,
        "bacteria_min_limit": 0,
        "predators_enabled": False,
        "food_target": 0,
        "food_min_r": 5.0,
        "food_max_r": 5.0,
        "bacteria_body_size": 9.0,
        "bacteria_retina_count": 1,
        "bacteria_retina_fov_degrees": 1.0,
        "bacteria_vision_radius": 120.0,
        "bacteria_retina_see_food": True,
        "random_seed": 123,
    }.items():
        params.set(key, value, validate=False)

    world = World(1000.0, 700.0, shape="circular", radius=1000.0)
    engine = Engine(world, Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def test_circular_world_spatial_hash_covers_expanded_radius():
    engine = _circular_large_radius_engine()
    y = engine.world.cy
    x = engine.world.cx + 760.0

    bacterium = engine.add_bacteria_at(x, y)
    assert bacterium is not None
    bacterium.angle = 0.0

    visible_food = engine.add_food_at(x + 50.0, y)
    assert visible_food is not None

    engine._update_spatial_hash(force=True)

    assert engine.spatial_hash.min_x == engine.world.cx - engine.world.radius
    assert engine.spatial_hash.max_x == engine.world.cx + engine.world.radius
    assert visible_food in engine.spatial_hash.query_ball(
        bacterium.x, bacterium.y, bacterium.r + 60.0
    )

    retina_inputs = bacterium.sensor.sense(bacterium, engine.scene_query, engine.params)
    assert retina_inputs[0] > 0.0

    edible_food = engine.add_food_at(x, y)
    assert edible_food is not None
    engine._update_spatial_hash(force=True)

    energy_before = bacterium.energy
    engine.interaction_system.apply(
        engine.entities["bacteria"],
        engine.entities["predators"],
        engine.entities["foods"],
        engine.spatial_hash,
        engine.params,
    )

    assert edible_food not in engine.entities["foods"]
    assert bacterium.energy > energy_before
