import pytest

from sim.actuators import EnergyModel, Locomotion
from sim.brain import NeuralNet
from sim.controllers import Params
from sim.entities import Bacteria, Food, _create_bacteria_brain
from sim.sensors import RetinaSensor, SceneQuery, active_retina_channels, retina_input_size


def test_retina_input_size_follows_enabled_channels():
    params = Params()
    params.set("bacteria_retina_count", 18, validate=False)

    assert active_retina_channels(params, "bacteria") == ("d",)
    assert retina_input_size(params, "bacteria") == 18

    params.set("bacteria_retina_input_mode", "color_plus_distance", validate=False)
    params.set("bacteria_retina_channel_r", True, validate=False)
    params.set("bacteria_retina_channel_g", True, validate=False)
    params.set("bacteria_retina_channel_b", True, validate=False)

    assert active_retina_channels(params, "bacteria") == ("r", "g", "b", "d")
    assert retina_input_size(params, "bacteria") == 72
    assert _create_bacteria_brain(params).sizes[0] == 72


def test_color_plus_distance_retina_reports_pure_color_and_dedicated_distance():
    params = Params()
    for key, value in {
        "bacteria_retina_count": 1,
        "bacteria_vision_radius": 20.0,
        "bacteria_retina_fov_degrees": 20.0,
        "bacteria_retina_see_food": True,
        "bacteria_retina_see_bacteria": False,
        "bacteria_retina_see_predators": False,
        "bacteria_retina_input_mode": "color_plus_distance",
        "bacteria_retina_channel_r": True,
        "bacteria_retina_channel_g": True,
        "bacteria_retina_channel_b": True,
        "bacteria_retina_channel_d": True,
    }.items():
        params.set(key, value, validate=False)

    sensor = RetinaSensor(retina_count=1, vision_radius=20.0, fov_degrees=20.0, channels=("r", "g", "b", "d"))
    agent = Bacteria(
        50.0,
        50.0,
        1.0,
        NeuralNet([4, 2]),
        sensor,
        Locomotion(),
        EnergyModel(),
        angle=0.0,
    )
    food = Food(60.0, 50.0, 2.0)
    food.color = (255, 0, 0)
    scene = SceneQuery(None, {"foods": [food], "bacteria": [agent], "predators": []}, params)

    values = sensor.sense(agent, scene, params)

    assert len(values) == 4
    assert values[0] == pytest.approx(1.0)
    assert values[1] == pytest.approx(0.0)
    assert values[2] == pytest.approx(0.0)
    assert 0.0 < values[3] < 1.0


def test_color_weighted_distance_can_use_only_selected_color_channels():
    params = Params()
    params.set("bacteria_retina_count", 18, validate=False)
    params.set("bacteria_retina_input_mode", "color_distance", validate=False)
    params.set("bacteria_retina_channel_r", True, validate=False)
    params.set("bacteria_retina_channel_g", True, validate=False)
    params.set("bacteria_retina_channel_b", False, validate=False)

    assert active_retina_channels(params, "bacteria") == ("rd", "gd")
    assert retina_input_size(params, "bacteria") == 36
    assert _create_bacteria_brain(params).sizes[0] == 36


def test_offspring_inherits_parent_retina_channel_signature():
    params = Params()
    sensor = RetinaSensor(
        retina_count=2,
        vision_radius=20.0,
        fov_degrees=90.0,
        channels=("rd", "gd"),
        eye_count=2,
        eye_angle_degrees=80.0,
        eye_separation_degrees=100.0,
    )
    agent = Bacteria(
        50.0,
        50.0,
        2.0,
        NeuralNet([4, 2]),
        sensor,
        Locomotion(),
        EnergyModel(death_energy=0.0, split_energy=10.0),
        angle=0.0,
    )
    agent.energy = 20.0

    child = agent.reproduce(params)

    assert child.sensor.channels == ("rd", "gd")
    assert child.sensor.retina_count == 2
    assert child.sensor.vision_radius == pytest.approx(20.0)
    assert child.sensor.eye_count == 2
    assert child.sensor.eye_angle_degrees == pytest.approx(80.0)
    assert child.sensor.eye_separation_degrees == pytest.approx(100.0)


def test_retina_input_size_multiplies_by_eye_count():
    params = Params()
    params.set("bacteria_retina_count", 6, validate=False)
    params.set("bacteria_eye_count", 2, validate=False)
    params.set("bacteria_retina_input_mode", "color_distance", validate=False)
    params.set("bacteria_retina_channel_r", True, validate=False)
    params.set("bacteria_retina_channel_g", False, validate=False)
    params.set("bacteria_retina_channel_b", False, validate=False)

    assert active_retina_channels(params, "bacteria") == ("rd",)
    assert retina_input_size(params, "bacteria") == 12
