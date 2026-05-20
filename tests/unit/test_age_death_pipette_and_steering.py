import json
from types import SimpleNamespace

from sim.actuators import EnergyModel, Locomotion
from sim.controllers import Params
from sim.engine import Engine
from sim.entities import create_random_bacteria
from sim.world import Camera, World


def test_age_death_and_corpse_food_are_agent_level_rules():
    params = Params()
    params.set("population_min_rescue_enabled", False)
    world = World(200, 200)
    engine = Engine(world, Camera(), params, headless=True)

    agent = create_random_bacteria([], params, world.width, world.height, at=(80, 80))
    agent.energy = 100.0
    agent.age = 11.0
    agent.energy_model = EnergyModel(
        death_energy=0.0,
        split_energy=9999.0,
        age_death_enabled=True,
        death_age=10.0,
        corpse_to_food=True,
    )

    survivors, predators = engine.death_system.apply([agent], [], params, agent_labels={})
    foods = engine._create_food_from_dead_agents(engine.death_system.last_deaths, params)

    assert survivors == []
    assert predators == []
    assert engine.death_system.last_deaths == [agent]
    assert len(foods) == 1
    assert foods[0].x == agent.x
    assert foods[0].y == agent.y


def test_pipette_serializes_brain_and_energy_genome_for_spawn():
    params = Params()
    world = World(240, 240)
    engine = Engine(world, Camera(), params, headless=True)
    agent = create_random_bacteria([], params, world.width, world.height, at=(80, 80))
    agent.energy_model.age_death_enabled = True
    agent.energy_model.death_age = 123.0
    agent.energy_model.corpse_to_food = True
    agent.color = (11, 22, 33)
    engine.entities["bacteria"].append(agent)
    engine.all_agents.append(agent)

    data = engine.sample_agent_as_prototype(agent.x, agent.y)
    spawned = engine._spawn_agent_from_prototype(data, 150, 150, preserve_prototype_color=True)

    assert data is not None
    assert json.loads(data["brain_sizes"]) == agent.brain.sizes
    assert data["energy_age_death_enabled"] == "True"
    assert float(data["energy_death_age"]) == 123.0
    assert data["energy_corpse_to_food"] == "True"
    assert spawned is not None
    assert spawned.brain.sizes == agent.brain.sizes
    assert spawned.energy_model.age_death_enabled is True
    assert spawned.energy_model.death_age == 123.0
    assert spawned.energy_model.corpse_to_food is True
    assert spawned.color == (11, 22, 33)


def test_locomotion_steering_accepts_left_and_right_turns():
    params = Params()
    world = World(500, 500)
    left = SimpleNamespace(x=250.0, y=250.0, r=5.0, angle=0.0, vx=0.0, vy=0.0)
    right = SimpleNamespace(x=250.0, y=250.0, r=5.0, angle=0.0, vx=0.0, vy=0.0)
    locomotion = Locomotion(max_speed=10.0, max_turn=1.0)

    locomotion.step(left, [0.0, -10.0], 0.5, world, params)
    locomotion.step(right, [0.0, 10.0], 0.5, world, params)

    assert left.angle < 0.0
    assert right.angle > 0.0
