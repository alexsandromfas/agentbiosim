import os
import sys
import random
import numpy as np

# Ensure project root is on sys.path when running this script as a module
# so that `sim` package can be imported whether executed from repo root
# or from within the tests/ folder.
ROOT = os.path.dirname(os.path.dirname(__file__))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from sim.brain import NeuralNet
from sim.controllers import Params


def flatten_params(net: NeuralNet):
    vals = []
    for W in net.weights:
        arr = np.array(W).ravel()
        vals.append(arr.copy())
    for b in net.biases:
        arr = np.array(b).ravel()
        vals.append(arr.copy())
    if not vals:
        return np.array([], dtype=np.float32)
    return np.concatenate(vals)


def compare_params(a: np.ndarray, b: np.ndarray, eps=1e-9):
    assert a.shape == b.shape
    diffs = np.abs(a - b)
    changed_mask = diffs > eps
    return changed_mask, diffs


def trial(rate, strength, trials=200, seed=12345):
    random.seed(seed)
    np.random.seed(seed)
    mutated_fracs = []
    mutated_abs_means = []
    total_params = None
    for t in range(trials):
        net = NeuralNet([8, 16, 4, 2], init_std=1.0)
        base = flatten_params(net)
        total_params = base.size
        child = net.copy()
        child.mutate(rate=rate, strength=strength)
        child_flat = flatten_params(child)
        mask, diffs = compare_params(base, child_flat)
        if mask.sum() > 0:
            mutated_fracs.append(mask.sum() / total_params)
            mutated_abs_means.append(diffs[mask].mean())
        else:
            mutated_fracs.append(0.0)
            mutated_abs_means.append(0.0)
    return {
        'rate': rate,
        'strength': strength,
        'trials': trials,
        'total_params': int(total_params),
        'mean_mutated_frac': float(np.mean(mutated_fracs)),
        'std_mutated_frac': float(np.std(mutated_fracs)),
        'mean_abs_delta_mutated': float(np.mean(mutated_abs_means)),
        'std_abs_delta_mutated': float(np.std(mutated_abs_means)),
    }


def inheritance_check(rate, strength, seed=999):
    random.seed(seed)
    np.random.seed(seed)
    net = NeuralNet([6, 8, 2], init_std=1.0)
    parent_flat = flatten_params(net)
    child = net.copy()
    child_flat_before = flatten_params(child)
    # mutate child
    child.mutate(rate=rate, strength=strength)
    child_flat_after = flatten_params(child)
    # parent must be unchanged
    parent_unchanged = np.allclose(parent_flat, child_flat_before)
    parent_vs_after_mask, _ = compare_params(parent_flat, child_flat_after)
    child_changed = np.any(~np.isclose(child_flat_before, child_flat_after))
    return {
        'parent_unchanged_after_copy': bool(parent_unchanged),
        'child_changed_by_mutation': bool(child_changed),
        'fraction_params_changed_vs_parent': float(parent_vs_after_mask.sum() / parent_flat.size),
    }


def main():
    rates = [0.0, 0.05, 0.2, 0.5, 1.0]
    strength = 0.1
    print("Mutation fraction trials (mean ± std) and mean abs delta for mutated params")
    for r in rates:
        res = trial(r, strength, trials=200, seed=1000 + int(r*1000))
        print(f"rate={r}: mean_frac={res['mean_mutated_frac']:.4f} std={res['std_mutated_frac']:.4f} | mean_abs_delta_mutated={res['mean_abs_delta_mutated']:.4f}")
    print('\nInheritance check (copy -> mutate):')
    inh = inheritance_check(0.1, 0.25, seed=2021)
    print(inh)

if __name__ == '__main__':
    main()
