"""Utilities for reproducible simulation runs."""
from __future__ import annotations

import random
from typing import Any

import numpy as np


_MAX_NUMPY_SEED = 2**32 - 1


def normalize_seed(value: Any) -> int | None:
    """Convert a user/config seed value to an int, or None when disabled."""
    if value is None:
        return None
    if isinstance(value, str):
        text = value.strip().lower()
        if text in {"", "none", "null", "random", "off"}:
            return None
        value = text
    try:
        seed = int(float(value))
    except (TypeError, ValueError):
        return None
    return seed if seed >= 0 else None


def apply_global_seed(value: Any) -> int | None:
    """Seed Python's random module and numpy.random using the same seed."""
    seed = normalize_seed(value)
    if seed is None:
        return None
    random.seed(seed)
    np.random.seed(seed % _MAX_NUMPY_SEED)
    return seed


def _jsonable_tuple(value: Any) -> Any:
    if isinstance(value, tuple):
        return [_jsonable_tuple(v) for v in value]
    return value


def _tuple_from_jsonable(value: Any) -> Any:
    if isinstance(value, list):
        return tuple(_tuple_from_jsonable(v) for v in value)
    return value


def capture_rng_state() -> dict[str, Any]:
    """Capture Python and numpy RNG states in JSON-serializable form."""
    np_state = np.random.get_state()
    return {
        "python_random": _jsonable_tuple(random.getstate()),
        "numpy_random": {
            "bit_generator": np_state[0],
            "state": np_state[1].astype(np.uint32).tolist(),
            "pos": int(np_state[2]),
            "has_gauss": int(np_state[3]),
            "cached_gaussian": float(np_state[4]),
        },
    }


def restore_rng_state(state: Any) -> bool:
    """Restore Python and numpy RNG states from a snapshot dictionary."""
    if not isinstance(state, dict):
        return False
    restored = False
    py_state = state.get("python_random")
    if py_state is not None:
        random.setstate(_tuple_from_jsonable(py_state))
        restored = True

    np_state = state.get("numpy_random")
    if isinstance(np_state, dict):
        bit_generator = str(np_state.get("bit_generator", "MT19937"))
        keys = np.array(np_state.get("state", []), dtype=np.uint32)
        pos = int(np_state.get("pos", 0))
        has_gauss = int(np_state.get("has_gauss", 0))
        cached_gaussian = float(np_state.get("cached_gaussian", 0.0))
        if keys.size:
            np.random.set_state((bit_generator, keys, pos, has_gauss, cached_gaussian))
            restored = True
    return restored
