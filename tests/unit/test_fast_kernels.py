import math
from types import SimpleNamespace

import numpy as np
import pytest

from sim.actuators import EnergyModel, Locomotion
from sim.controllers import Params
from sim.entities import _apply_fast_locomotion_energy
from sim.fast_kernels import has_numba, retina_fullbody_kernel, retina_single_kernel
from sim.world import World


class _Agent(SimpleNamespace):
    def speed(self):
        return math.hypot(self.vx, self.vy)


def _make_agent(x, y, angle, vx=0.0, vy=0.0, energy=120.0):
    return _Agent(
        x=float(x),
        y=float(y),
        r=8.0,
        angle=float(angle),
        vx=float(vx),
        vy=float(vy),
        m=1.0,
        energy=float(energy),
        is_predator=False,
        locomotion=Locomotion(max_speed=180.0, max_turn=math.pi * 0.75),
        energy_model=EnergyModel(
            death_energy=0.0,
            split_energy=999.0,
            v0_cost=0.5,
            vmax_cost=8.0,
            vmax_ref=180.0,
            energy_cap=300.0,
        ),
        last_brain_output=[],
        last_brain_activations=[],
    )


def test_array_locomotion_energy_matches_object_path_rectangular():
    params = Params()
    params.set("bacteria_max_speed", 180.0, validate=False)
    params.set("bacteria_metab_v0_cost", 0.7, validate=False)
    params.set("bacteria_metab_vmax_cost", 5.3, validate=False)
    params.set("bacteria_energy_cap", 300.0, validate=False)
    params.set("agents_inertia", 1.8, validate=False)
    params.set("allow_reverse_locomotion", False, validate=False)
    world = World(120.0, 90.0)
    outputs = np.array([[0.2, -0.8], [2.0, 0.5], [-1.5, 3.0]], dtype=np.float32)
    dt = 1.0 / 30.0

    reference = [
        _make_agent(20.0, 25.0, 0.1, 4.0, 1.0),
        _make_agent(112.0, 80.0, 1.0, 0.0, -3.0),
        _make_agent(10.0, 12.0, -2.3, 2.0, 2.0),
    ]
    accelerated = [
        _make_agent(20.0, 25.0, 0.1, 4.0, 1.0),
        _make_agent(112.0, 80.0, 1.0, 0.0, -3.0),
        _make_agent(10.0, 12.0, -2.3, 2.0, 2.0),
    ]

    for agent, out in zip(reference, outputs):
        agent.locomotion.step(agent, out.tolist(), dt, world, params)
        agent.energy_model.apply(agent, dt, params)

    backend = _apply_fast_locomotion_energy(accelerated, outputs, dt, world, params, force_python=True)

    assert backend == "python-array"
    for ref, got in zip(reference, accelerated):
        assert got.x == pytest_approx(ref.x)
        assert got.y == pytest_approx(ref.y)
        assert got.vx == pytest_approx(ref.vx)
        assert got.vy == pytest_approx(ref.vy)
        assert got.angle == pytest_approx(ref.angle)
        assert got.energy == pytest_approx(ref.energy)


def test_numba_retina_kernels_detect_center_food_when_available():
    if not has_numba():
        pytest.skip("Numba indisponivel neste ambiente")

    cand_x = np.array([50.0], dtype=np.float64)
    cand_y = np.array([0.0], dtype=np.float64)
    cand_r = np.array([10.0], dtype=np.float64)
    cand_type = np.array([0], dtype=np.int8)
    self_flags = np.array([False], dtype=bool)
    retina_count = 3
    half_fov = math.radians(45.0)

    for kernel in (retina_single_kernel, retina_fullbody_kernel):
        out = np.zeros(retina_count, dtype=np.float64)
        ok = kernel(
            cand_x,
            cand_y,
            cand_r,
            cand_type,
            self_flags,
            0.0,
            0.0,
            0.0,
            100.0,
            half_fov,
            retina_count,
            True,
            False,
            False,
            False,
            out,
        )
        assert ok is True
        assert out[0] == pytest.approx(0.0, abs=1e-12)
        assert out[1] == pytest.approx(0.6, rel=1e-12, abs=1e-12)
        assert out[2] == pytest.approx(0.0, abs=1e-12)


def pytest_approx(value):
    return pytest.approx(value, rel=1e-9, abs=1e-9)
