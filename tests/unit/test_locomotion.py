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
    agent = _agent()

    Locomotion(max_speed=10.0, max_turn=0.0, allow_reverse=True).step(agent, [-10.0, 0.0], 1.0, World(100, 100), params)

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


def test_locomotion_omni_can_move_diagonal_and_rotate():
    params = Params()
    params.set("agents_inertia", 1.0)
    agent = _agent()

    Locomotion(max_speed=10.0, max_turn=1.0, movement_mode="omni").step(
        agent, [10.0, 10.0, 10.0], 1.0, World(100, 100), params
    )

    assert agent.vx != 0.0
    assert agent.vy != 0.0
    assert agent.angle > 0.0
    assert (agent.vx ** 2 + agent.vy ** 2) ** 0.5 <= 10.0 + 1e-6


def test_smooth_locomotion_limits_linear_and_angular_changes():
    params = Params()
    params.set("smooth_locomotion_enabled", True, validate=False)
    params.set("smooth_max_linear_accel", 2.0, validate=False)
    params.set("smooth_linear_drag_enabled", False, validate=False)
    params.set("smooth_max_angular_accel", 1.0, validate=False)
    params.set("smooth_angular_drag_enabled", False, validate=False)
    agent = _agent()

    Locomotion(max_speed=10.0, max_turn=4.0).step(
        agent, [10.0, 10.0], 0.1, World(100, 100), params
    )

    assert 0.0 < agent.vx <= 0.2 + 1e-9
    assert 0.0 < agent.angular_velocity <= 0.1 + 1e-9
    assert 0.0 < agent.angle <= 0.01 + 1e-9
