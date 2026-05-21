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
    params.set("food_absorption_seconds", 2.0, validate=False)
    params.set("food_bite_seconds", 4.0, validate=False)
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


def test_slow_absorption_food_drains_over_contact_time():
    engine = _engine_with_one_agent("slow_absorption")
    agent = engine.entities["bacteria"][0]
    agent.energy = 0.0
    food = _place_food_on_agent(engine, "slow_absorption", radius=5.0)

    engine.interaction_system.apply(
        engine.entities["bacteria"],
        engine.entities["predators"],
        engine.entities["foods"],
        engine.spatial_hash,
        engine.params,
        dt=0.5,
    )

    assert engine.entities["foods"] == [food]
    assert 0.0 < agent.energy < food.initial_energy
    assert math.isclose(food.energy, 18.75)
    assert food.r < food.base_radius


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
        dt=1.0,
    )

    assert resolved > 0
    assert distance >= agent.r + food.r - 1e-6
    assert engine.entities["foods"] == [food]
    assert 0.0 < agent.energy < food.initial_energy
    assert food.energy < food.initial_energy
    assert food.r == food.base_radius
    assert len(food.bite_holes) == 1
