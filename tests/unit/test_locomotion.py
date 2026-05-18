from types import SimpleNamespace

from sim.actuators import Locomotion
from sim.controllers import Params
from sim.world import World


def _agent():
    return SimpleNamespace(x=50.0, y=50.0, r=5.0, angle=0.0, vx=0.0, vy=0.0)


def test_locomotion_preserves_forward_only_default():
    params = Params()
    params.set("agents_inertia", 1.0)
    agent = _agent()

    Locomotion(max_speed=10.0, max_turn=0.0).step(agent, [-10.0, 0.0], 1.0, World(100, 100), params)

    assert agent.vx > 0.0
    assert agent.x > 50.0


def test_locomotion_can_reverse_when_explicitly_enabled():
    params = Params()
    params.set("agents_inertia", 1.0)
    params.set("allow_reverse_locomotion", True)
    agent = _agent()

    Locomotion(max_speed=10.0, max_turn=0.0).step(agent, [-10.0, 0.0], 1.0, World(100, 100), params)

    assert agent.vx < 0.0
    assert agent.x < 50.0


def test_locomotion_speed_output_changes_forward_speed():
    params = Params()
    params.set("agents_inertia", 1.0)
    slow = _agent()
    fast = _agent()

    locomotion = Locomotion(max_speed=10.0, max_turn=0.0)
    locomotion.step(slow, [-10.0, 0.0], 1.0, World(100, 100), params)
    locomotion.step(fast, [10.0, 0.0], 1.0, World(100, 100), params)

    assert 0.0 < slow.vx < 1.0
    assert fast.vx > 9.0
    assert fast.vx > slow.vx
