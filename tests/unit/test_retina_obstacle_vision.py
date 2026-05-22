from types import SimpleNamespace

from sim.controllers import Params
from sim.entities import Food
from sim.obstacles import ObstacleMap
from sim.sensors import RetinaSensor, SceneQuery


def _agent():
    return SimpleNamespace(x=60.0, y=80.0, r=5.0, angle=0.0, is_predator=False)


def test_obstacle_blocks_food_when_wall_vision_is_disabled():
    params = Params()
    food = Food(150.0, 80.0, 5.0, kind="instant")
    obstacles = ObstacleMap()
    obstacles.add_brush_line(95.0, 80.0, 95.0, 80.0, 8.0, (90, 90, 90))
    scene = SceneQuery(None, {"foods": [food], "bacteria": [], "predators": []}, params, obstacles=obstacles)
    sensor = RetinaSensor(
        retina_count=1,
        vision_radius=140.0,
        fov_degrees=5.0,
        see_food=True,
        see_obstacles=False,
        see_through_walls=False,
        channels=("d",),
    )

    assert sensor.sense(_agent(), scene, params) == [0.0]


def test_obstacle_is_visible_when_enabled():
    params = Params()
    obstacles = ObstacleMap()
    obstacles.add_brush_line(95.0, 80.0, 95.0, 80.0, 8.0, (90, 90, 90))
    scene = SceneQuery(None, {"foods": [], "bacteria": [], "predators": []}, params, obstacles=obstacles)
    sensor = RetinaSensor(
        retina_count=1,
        vision_radius=140.0,
        fov_degrees=5.0,
        see_food=False,
        see_obstacles=True,
        see_through_walls=False,
        channels=("d",),
    )

    value = sensor.sense(_agent(), scene, params)[0]
    assert 0.0 < value < 1.0
