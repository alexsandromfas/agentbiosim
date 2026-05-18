import importlib.util
from pathlib import Path


def _load_benchmark_headless():
    path = Path(__file__).resolve().parents[1] / "benchmark_headless.py"
    spec = importlib.util.spec_from_file_location("benchmark_headless", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def test_headless_benchmark_disables_reproduction_with_energy_params():
    module = _load_benchmark_headless()
    params = module.configure_params()

    assert params.get("bacteria_split_energy") == 1e9
    assert params.get("predator_split_energy") == 1e9
    assert params.get("reproduction_min_age") == 1e9
    assert params.get("reproduction_cooldown") == 1e9
    assert params.get("bacteria_split_mass") is None
    assert params.get("predator_split_mass") is None
