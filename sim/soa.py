"""Struct-of-arrays helpers for future vectorized simulation backends."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Sequence, Any

import numpy as np


@dataclass(frozen=True)
class AgentArrays:
    x: np.ndarray
    y: np.ndarray
    vx: np.ndarray
    vy: np.ndarray
    angle: np.ndarray
    energy: np.ndarray
    radius: np.ndarray
    age: np.ndarray
    type_code: np.ndarray

    @property
    def count(self) -> int:
        return int(self.x.shape[0])


def agents_to_arrays(agents: Sequence[Any]) -> AgentArrays:
    """Copy agent scalar state into contiguous NumPy arrays.

    This does not replace the object-oriented runtime yet. It provides a tested
    bridge for benchmarks and future kernels that operate on SoA data.
    """
    count = len(agents)
    x = np.empty(count, dtype=np.float32)
    y = np.empty(count, dtype=np.float32)
    vx = np.empty(count, dtype=np.float32)
    vy = np.empty(count, dtype=np.float32)
    angle = np.empty(count, dtype=np.float32)
    energy = np.empty(count, dtype=np.float32)
    radius = np.empty(count, dtype=np.float32)
    age = np.empty(count, dtype=np.float32)
    type_code = np.empty(count, dtype=np.int8)

    for idx, agent in enumerate(agents):
        x[idx] = float(getattr(agent, "x", 0.0))
        y[idx] = float(getattr(agent, "y", 0.0))
        vx[idx] = float(getattr(agent, "vx", 0.0))
        vy[idx] = float(getattr(agent, "vy", 0.0))
        angle[idx] = float(getattr(agent, "angle", 0.0))
        energy[idx] = float(getattr(agent, "energy", 0.0))
        radius[idx] = float(getattr(agent, "r", 0.0))
        age[idx] = float(getattr(agent, "age", 0.0))
        type_code[idx] = int(getattr(agent, "type_code", -1))

    return AgentArrays(x, y, vx, vy, angle, energy, radius, age, type_code)
