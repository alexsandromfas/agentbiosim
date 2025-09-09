import os
import sys
import math
import numpy as np

# Ensure project root is on sys.path
THIS_DIR = os.path.dirname(__file__)
PROJECT_ROOT = os.path.dirname(THIS_DIR)
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from sim.sensors import RetinaSensor, SceneQuery, batch_retina_sense

class DummyObj:
    def __init__(self, x, y, r, type_code):
        self.x = float(x)
        self.y = float(y)
        self.r = float(r)
        self.type_code = int(type_code)

class DummyAgent:
    def __init__(self, x=0.0, y=0.0, r=8.0, angle=0.0, is_predator=False):
        self.x = float(x)
        self.y = float(y)
        self.r = float(r)
        self.angle = float(angle)
        self.is_predator = bool(is_predator)
        self.sensor = RetinaSensor(retina_count=9, vision_radius=120.0, fov_degrees=180.0,
                                   see_food=True, see_bacteria=True, see_predators=False)


def run_case(mode):
    agent = DummyAgent(0.0, 0.0, 8.0, 0.0, False)
    ents = {
        'foods': [DummyObj(50.0, 0.0, 5.0, 0)],
        'bacteria': [DummyObj(60.0, 20.0, 9.0, 1)],
        'predators': []
    }
    params = {
        'retina_vision_mode': mode,
        'bacteria_retina_see_food': True,
        'bacteria_retina_see_bacteria': True,
        'bacteria_retina_see_predators': False,
        'bacteria_retina_count': 9,
        'bacteria_retina_fov_degrees': 180.0,
        'bacteria_vision_radius': 120.0,
    }
    scene = SceneQuery(spatial_hash=None, entities=ents, params=params)
    res = batch_retina_sense([agent], scene, params)
    return res[0]

if __name__ == '__main__':
    single = run_case('single')
    full = run_case('fullbody')
    print('single:', np.array(single))
    print('full  :', np.array(full))
    assert len(single) == 9 and len(full) == 9
    assert all(0.0 <= v <= 1.0 for v in single)
    assert all(0.0 <= v <= 1.0 for v in full)
    print('OK')
