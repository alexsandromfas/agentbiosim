import numpy as np

from sim.brain import (
    GatedNeuralNet,
    ModulatedNeuralNet,
    ShortcutNeuralNet,
    SimpleRNNBrain,
    create_brain,
    brain_from_data,
    brain_to_data,
    forward_many_brains,
)
from sim.controllers import Params


def test_brain_factory_creates_requested_variants():
    params = Params()
    for kind, expected in [
        ("mlp", "mlp"),
        ("gated_mlp", "gated_mlp"),
        ("shortcut_mlp", "shortcut_mlp"),
        ("modulated_mlp", "modulated_mlp"),
        ("simple_rnn", "simple_rnn"),
        ("neat_common", "neat_common"),
        ("neat_simplified", "neat_simplified"),
        ("neat_recurrent", "neat_recurrent"),
    ]:
        params.set("neural_network_type", kind, validate=False)
        brain = create_brain([4, 5, 2], params=params, init_std=0.1)
        assert brain.brain_type == expected
        assert brain.sizes == [4, 5, 2]


def test_variant_forward_many_matches_individual_forward_for_stateless_brains():
    inputs = np.array([[0.1, -0.2, 0.3], [0.4, 0.2, -0.1]], dtype=np.float32)
    for cls in (GatedNeuralNet, ShortcutNeuralNet, ModulatedNeuralNet):
        brains = [cls([3, 4, 2], init_std=0.2) for _ in range(2)]
        expected = np.array([brain.forward(inp) for brain, inp in zip(brains, inputs)], dtype=np.float32)
        # Recria os cerebros para evitar comparar depois de possivel mutacao de estado.
        brains = [brain.copy() for brain in brains]
        got = forward_many_brains(brains, inputs)
        assert got.shape == (2, 2)
        assert np.allclose(got, expected, atol=1e-5)


def test_simple_rnn_has_state_and_serializes_roundtrip():
    brain = SimpleRNNBrain([3, 4, 2], init_std=0.2)
    out1 = brain.forward([0.2, 0.1, -0.3])
    state_after = brain.state.copy()
    out2 = brain.forward([0.2, 0.1, -0.3])
    assert len(out1) == 2
    assert len(out2) == 2
    assert not np.allclose(state_after, np.zeros_like(state_after))

    data = brain_to_data(brain)
    restored = brain_from_data(data)
    assert restored.brain_type == "simple_rnn"
    assert restored.sizes == brain.sizes
    assert np.allclose(restored.recurrent_weights, brain.recurrent_weights)


def test_neat_variants_are_individual_and_serialize_roundtrip():
    inputs = np.array([[0.1, -0.2, 0.3], [0.4, 0.2, -0.1]], dtype=np.float32)
    params = Params()
    for kind in ("neat_common", "neat_simplified", "neat_recurrent"):
        params.set("neural_network_type", kind, validate=False)
        brains = [create_brain([3, 4, 2], params=params, init_std=0.2) for _ in range(2)]
        got = forward_many_brains(brains, inputs)
        assert got.shape == (2, 2)
        assert all(getattr(brain, "supports_batch", True) is False for brain in brains)

        brains[0].mutate(0.5, 0.2)
        data = brain_to_data(brains[0])
        restored = brain_from_data(data, params=params)
        assert restored.brain_type == kind
        assert restored.sizes == brains[0].sizes
        assert len(restored.nodes) == len(brains[0].nodes)
        assert len(restored.connections) == len(brains[0].connections)
        assert len(restored.forward([0.1, -0.2, 0.3])) == 2
