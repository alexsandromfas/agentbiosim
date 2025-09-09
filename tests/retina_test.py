import math
from sim.sensors import RetinaSensor, SceneQuery, batch_retina_sense
from sim.controllers import Params

# fake minimal entity classes
class FakeObj:
    def __init__(self,x,y,r,tc=1):
        self.x=x; self.y=y; self.r=r; self.type_code=tc

class FakeAgent:
    def __init__(self,x,y,r,angle,is_pred=False):
        self.x=x; self.y=y; self.r=r; self.angle=angle; self.is_predator=is_pred
        self.sensor = RetinaSensor(retina_count=9, vision_radius=200.0, fov_degrees=180.0)

# Setup scene
params = Params()
# Ensure bacteria can see predators in this test
params.set('bacteria_retina_see_predators', True)
# No skipping to always compute
params.set('retina_skip', 0)
# place agent at (100,100), facing right (0 rad)
agent = FakeAgent(100.0,100.0,10.0,0.0)
# place objects in front: predator, food, another bacteria
pred = FakeObj(200.0,100.0,50.0,tc=2)
food = FakeObj(170.0,120.0,6.0,tc=0)
other_bact = FakeAgent(230.0, 100.0, 10.0, 0.0, is_pred=False)
entities={'foods':[food], 'bacteria':[other_bact], 'predators':[pred]}
scene = SceneQuery(None, entities, params)

# Enable seeing all types
params.set('bacteria_retina_see_food', True)
params.set('bacteria_retina_see_bacteria', True)
params.set('bacteria_retina_see_predators', True)

# Test single vs fullbody
for mode in ('single','fullbody'):
    params.set('retina_vision_mode', mode)
    res = batch_retina_sense([agent], scene, params)
    print(mode, res)
