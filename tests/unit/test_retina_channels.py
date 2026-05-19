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

    params.set("bacteria_retina_channel_r", True, validate=False)
    params.set("bacteria_retina_channel_g", True, validate=False)
    params.set("bacteria_retina_channel_b", True, validate=False)

    assert active_retina_channels(params, "bacteria") == ("r", "g", "b", "d")
    assert retina_input_size(params, "bacteria") == 72
    assert _create_bacteria_brain(params).sizes[0] == 72


def test_rgbd_retina_reports_color_weighted_by_proximity():
    params = Params()
    for key, value in {
        "bacteria_retina_count": 1,
        "bacteria_vision_radius": 20.0,
        "bacteria_retina_fov_degrees": 20.0,
        "bacteria_retina_see_food": True,
        "bacteria_retina_see_bacteria": False,
        "bacteria_retina_see_predators": False,
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
    assert values[0] == pytest.approx(values[3], rel=1e-6)
    assert values[0] > 0.0
    assert values[1] == pytest.approx(0.0)
    assert values[2] == pytest.approx(0.0)
