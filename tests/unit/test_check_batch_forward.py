import importlib.util
from pathlib import Path

from sim.controllers import Params
from sim.engine import Engine
from sim.world import Camera, World


def _load_check_batch_forward():
    path = Path(__file__).resolve().parents[1] / "check_batch_forward.py"
    spec = importlib.util.spec_from_file_location("check_batch_forward", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def test_check_batch_forward_compares_each_group():
    module = _load_check_batch_forward()
    params = Params()
    params.set("bacteria_count", 12)
    params.set("bacteria_min_limit", 0)
    params.set("bacteria_max_limit", 20)
    params.set("predators_enabled", False)
    params.set("food_target", 8)
    params.set("random_seed", 321)

    engine = Engine(World(240, 180), Camera(), params, headless=True)
    engine.start()

    overall = module.analyze(engine, sample_limit_per_group=8)

    assert overall
    for _key, _group_count, sampled, _frac_mismatch, _mean_diff, max_diff in overall:
        assert sampled >= 2
        assert max_diff < 5e-6
