import math

from sim.controllers import Params
from sim.engine import Engine
from sim.entities import Food
from sim.world import Camera, World


def _engine_with_one_agent(food_mode: str) -> Engine:
    params = Params()
    params.set("bacteria_count", 1, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    params.set("food_mode", food_mode, validate=False)
    params.set("food_bite_seconds", 4.0, validate=False)
    params.set("food_piece_particle_radius", 5.0, validate=False)
    params.set("food_piece_cluster_radius", 12.0, validate=False)
    params.set("food_piece_particle_spacing", 0.0, validate=False)
    params.set("random_seed", 123, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    engine.start(initialize=True)
    return engine


def _place_food_on_agent(engine: Engine, kind: str, radius: float = 5.0) -> Food:
    agent = engine.entities["bacteria"][0]
    agent.x = 100.0
    agent.y = 80.0
    food = Food(agent.x, agent.y, radius, kind=kind)
    food.energy = radius * radius
    food.initial_energy = food.energy
    food.base_radius = radius
    engine.entities["foods"][:] = [food]
    engine._spatial_hash_dirty = True
    engine._update_spatial_hash(force=True)
    return food


def test_instant_food_keeps_existing_full_consume_behavior():
    engine = _engine_with_one_agent("instant")
    agent = engine.entities["bacteria"][0]
    agent.energy = 0.0
    _place_food_on_agent(engine, "instant", radius=5.0)

    engine.interaction_system.apply(
        engine.entities["bacteria"],
        engine.entities["predators"],
        engine.entities["foods"],
        engine.spatial_hash,
        engine.params,
        dt=0.1,
    )

    assert engine.entities["foods"] == []
    assert agent.energy == 25.0
    assert agent.food_eaten_count == 1


def test_chunk_food_is_solid_and_consumed_by_bites():
    engine = _engine_with_one_agent("chunk")
    agent = engine.entities["bacteria"][0]
    agent.energy = 0.0
    agent.angle = 0.0
    food = _place_food_on_agent(engine, "chunk", radius=6.0)
    food.x = agent.x + agent.r + food.r - 2.0
    food.y = agent.y
    engine._spatial_hash_dirty = True
    engine._update_spatial_hash(force=True)

    resolved = engine._resolve_solid_food_collisions(engine.params)
    distance = math.hypot(agent.x - food.x, agent.y - food.y)
    engine._spatial_hash_dirty = True
    engine._update_spatial_hash(force=True)
    engine.interaction_system.apply(
        engine.entities["bacteria"],
        engine.entities["predators"],
        engine.entities["foods"],
        engine.spatial_hash,
        engine.params,
        dt=0.1,
    )

    assert resolved > 0
    assert distance >= agent.r + food.r - 1e-6
    assert engine.entities["foods"] == [food]
    assert 0.0 < agent.energy < food.initial_energy
    assert food.energy < food.initial_energy
    assert food.r < food.base_radius


def test_chunk_food_controller_creates_pellet_cluster():
    engine = _engine_with_one_agent("chunk")
    engine.params.set("food_target", 8, validate=False)

    new_foods = engine.food_controller.update(
        engine.entities["foods"],
        target_count=8,
        world_w=engine.world.width,
        world_h=engine.world.height,
        params=engine.params,
        dt=0.1,
        obstacle_map=engine.obstacles,
        agents=engine.all_agents,
    )

    assert len(new_foods) > 1
    assert all(food.kind == "chunk" for food in new_foods)


def test_chunk_food_can_grow_existing_cluster_when_configured():
    engine = _engine_with_one_agent("chunk")
    anchor = Food(80.0, 80.0, 5.0, kind="chunk")
    engine.entities["foods"][:] = [anchor]
    engine.params.set("food_target", 2, validate=False)
    engine.params.set("food_piece_replenish_mode", "grow_existing", validate=False)

    new_foods = engine.food_controller.update(
        engine.entities["foods"],
        target_count=2,
        world_w=engine.world.width,
        world_h=engine.world.height,
        params=engine.params,
        dt=0.1,
        obstacle_map=engine.obstacles,
    )

    assert len(new_foods) == 1
    new_food = new_foods[0]
    assert new_food.kind == "chunk"
    assert math.hypot(new_food.x - anchor.x, new_food.y - anchor.y) <= 24.0


def test_chunk_food_can_grow_one_particle_at_a_time_when_configured():
    engine = _engine_with_one_agent("chunk")
    anchor = Food(80.0, 80.0, 5.0, kind="chunk")
    engine.entities["foods"][:] = [anchor]
    engine.params.set("food_target", 2, validate=False)
    engine.params.set("food_piece_replenish_mode", "grow_particles", validate=False)

    new_foods = engine.food_controller.update(
        engine.entities["foods"],
        target_count=2,
        world_w=engine.world.width,
        world_h=engine.world.height,
        params=engine.params,
        dt=0.1,
        obstacle_map=engine.obstacles,
    )

    assert len(new_foods) == 1
    new_food = new_foods[0]
    assert new_food.kind == "chunk"
    assert math.hypot(new_food.x - anchor.x, new_food.y - anchor.y) <= anchor.r + new_food.r + 2.0


def test_chunk_food_spawn_cluster_waits_for_consumed_energy():
    engine = _engine_with_one_agent("chunk")
    anchor = Food(80.0, 80.0, 5.0, kind="chunk")
    engine.entities["foods"][:] = [anchor]
    engine.params.set("food_target", 8, validate=False)
    engine.params.set("food_piece_replenish_mode", "spawn_cluster", validate=False)

    no_foods = engine.food_controller.update(
        engine.entities["foods"],
        target_count=8,
        world_w=engine.world.width,
        world_h=engine.world.height,
        params=engine.params,
        dt=0.1,
        obstacle_map=engine.obstacles,
        agents=engine.all_agents,
    )
    engine.food_controller.note_food_energy_consumed(1000.0)
    new_foods = engine.food_controller.update(
        engine.entities["foods"],
        target_count=8,
        world_w=engine.world.width,
        world_h=engine.world.height,
        params=engine.params,
        dt=0.1,
        obstacle_map=engine.obstacles,
        agents=engine.all_agents,
    )

    assert no_foods == []
    assert len(new_foods) > 1


def test_movable_chunk_food_particles_do_not_overlap():
    params = Params()
    params.set("food_mode", "chunk", validate=False)
    params.set("movable_chunk_food_enabled", True, validate=False)
    params.set("chunk_food_collision_enabled", True, validate=False)
    params.set("chunk_food_adhesion_enabled", False, validate=False)
    engine = Engine(World(200, 160), Camera(), params, headless=True)
    f1 = Food(90.0, 80.0, 6.0, kind="chunk")
    f2 = Food(98.0, 80.0, 6.0, kind="chunk")
    engine.entities["foods"][:] = [f1, f2]
    engine._spatial_hash_dirty = True
    engine._update_spatial_hash(force=True)

    resolved = engine._resolve_chunk_food_contacts(engine.params)

    assert resolved > 0
    assert math.hypot(f1.x - f2.x, f1.y - f2.y) >= f1.r + f2.r - 1e-6
