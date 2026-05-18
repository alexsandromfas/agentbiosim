import math

from sim.controllers import Params
from sim.engine import Engine
from sim.entities import Food
from sim.intelligence import intelligence_snapshot, local_resource_density
from sim.world import Camera, World


def test_intelligence_snapshot_uses_local_density_without_global_scan_per_agent():
    params = Params()
    params.set("bacteria_count", 1, validate=False)
    params.set("predators_enabled", False, validate=False)
    params.set("food_target", 0, validate=False)
    params.set("random_seed", 123, validate=False)

    engine = Engine(World(100, 100), Camera(), params, headless=True)
    engine.start(initialize=True)
    agent = engine.entities["bacteria"][0]
    agent.x = 50.0
    agent.y = 50.0
    agent.age = 10.0
    agent.food_eaten_count = 4
    agent.food_energy_eaten_total = 100.0
    agent.sensor.vision_radius = 10.0

    near_food = Food(55.0, 50.0, 5.0)
    far_food = Food(90.0, 90.0, 5.0)
    engine.entities["foods"][:] = [near_food, far_food]
    engine._spatial_hash_dirty = True
    engine._update_spatial_hash(force=True)

    density = local_resource_density(engine, agent)
    snapshot = intelligence_snapshot(engine, agent)

    assert density > 0.0
    assert math.isclose(snapshot["intake_rate"], 10.0)
    assert snapshot["local_factor"] > 0.0
    assert snapshot["global_factor"] > 0.0
    assert snapshot["species_local_factor"] == snapshot["local_factor"]
    assert snapshot["sample_size"] == 1.0
