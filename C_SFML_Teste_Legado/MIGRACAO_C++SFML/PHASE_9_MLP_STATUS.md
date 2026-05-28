# Phase 9 Status: MLP Inicial com Arquitetura Neural Extensivel

## Scope

Phase 9 implemented only the initial neural foundation and the baseline MLP.

Implemented:
- `BrainType` with all planned neural types represented.
- `BrainConfig` with MLP fields and future neural parameter groups.
- `BrainState` and `BrainHandle` as minimal future-facing structures.
- `BrainFactory` with MLP creation and controlled fallback for non-implemented future types.
- `BrainExecutor` as the first execution boundary.
- `MLPBrain` with dense layers, weights, biases, single forward, mutation, clone/copy, resizeInput and activation trace.
- `NeuralSystem` as a minimal bridge from temporary neural inputs to `MovementSystem` controls.
- Console validation command `--phase9-selftest`.
- Console diagnostic/microbenchmark command `--phase9-diagnostics`.

Not implemented:
- Real sensors.
- Vision/perception.
- Gated MLP.
- Shortcut MLP.
- Modulated MLP.
- Simple RNN.
- NEAT common.
- NEAT simplified.
- NEAT recurrent.
- Reproduction.
- Functional predators.
- Save/load.
- Complete UI.
- Neural viewer.
- Formal benchmark infrastructure.

## Python Reference Files Consulted

- `sim/brain.py`
- `sim/actuators.py`
- `sim/entities.py`
- `sim/controllers.py`
- `sim/engine.py`
- `sim/random_utils.py`
- `sim/fast_kernels.py`
- `sim/neural_viewer.py`

## Additional Python Files Consulted

None.

## Documentation Consulted

- `CODEX_MIGRATION_GUIDE.md`
- `MIGRATION_PHASES.md`
- `PLANNING_COVERAGE_AUDIT.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`
- `MIGRATION_RISKS.md`
- `BENCHMARK_PLAN.md`
- `PHASE_8_LOCOMOTION_STATUS.md`

## Inventory Items Covered

From `FEATURE_INVENTORY.md`:
- baseline MLP brain;
- neural mutation foundation;
- activation/debug data foundation;
- temporary integration from brain output to locomotion;
- future neural type planning.

From `PARAMETER_INVENTORY.md`:
- `neural_network_type`;
- `bacteria_hidden_layers`;
- `bacteria_neurons_layer_1`;
- `bacteria_neurons_layer_2`;
- `bacteria_neurons_layer_3`;
- `bacteria_neurons_layer_4`;
- `bacteria_neurons_layer_5`;
- `bacteria_mutation_rate`;
- `bacteria_mutation_strength`;
- `bacteria_structural_jitter`;
- `bacteria_movement_mode`;
- neural gated/shortcut/RNN future parameter groups;
- brain cache / native brain forward future parameter groups;
- predator neural layer/mutation defaults recognized for future species support.

UI inventory impact:
- no UI was implemented in this phase;
- neural network preference panels and neural viewer remain future phases;
- the architecture now exposes the concepts needed by those UI phases.

## Concepts Confirmed Against Python

- Python `NeuralNet` stores dense layer weights and biases.
- Python initializes weights with normal noise scaled by `init_std / sqrt(fan_in)`.
- Python random biases use half the weight standard deviation.
- Python hidden layers use `tanh`.
- Python output layer is linear.
- Python `activations()` returns post-activation hidden layers and raw output layer.
- Python `mutate()` clamps mutation rate to `[0, 1]` and applies Gaussian noise.
- Python `resize_input()` preserves old first-layer weights and initializes new inputs with small noise.
- Python `locomotion_output_size()` returns `2` for `forward` and `3` for `omni`.
- Python recognizes `mlp`, `gated_mlp`, `shortcut_mlp`, `modulated_mlp`, `simple_rnn`, `neat_common`, `neat_simplified` and `neat_recurrent`.

## Divergences and Decisions

### Temporary Input

Real sensors are not part of Phase 9. The temporary input vector has size `4`:

1. normalized energy: `energy / bacteria_energy_cap`, clamped to `[0, 2]`;
2. normalized age: `age / 3600`, clamped to `[0, 1]`;
3. normalized X position inside world bounds;
4. normalized Y position inside world bounds.

This is deliberately not vision. It will be replaced by sensor/vision input in Phase 10/11.

### Output Size

Output size is derived from Phase 8 locomotion:

- `forward`: 2 outputs, mapped to `forward` and `turn`;
- `omni`: 3 outputs, mapped to `forward`, `strafe` and `turn`.

### Fallback for Future Brain Types

Only MLP is instantiable in Phase 9. If config requests a future type, the factory creates an MLP fallback and records that fallback. This avoids silently implementing an incomplete advanced network.

### Structural Jitter

`bacteria_structural_jitter` is preserved in `BrainConfig`, but structural mutation is not implemented in Phase 9. It remains a future reproduction/mutation topic.

### Brain Ownership

`NeuralSystem` owns one MLP brain per stable agent id using an internal map. `AgentStore` does not store MLP objects and remains data-oriented.

This is a temporary runtime ownership model until future `SpeciesStore` / genome / brain storage phases define the permanent mapping.

## Files Created

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/ActivationTrace.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainConfig.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainExecutor.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainFactory.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainFactory.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainHandle.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainState.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/BrainType.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/MLPBrain.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/MLPBrain.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/Phase9Diagnostics.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/neural/Phase9Diagnostics.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/NeuralSystem.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/NeuralSystem.cpp`
- `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_9_MLP_STATUS.md`

## Files Modified

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/main.cpp`

## Arquitetura Neural Preparada para Redes Futuras

The current MLP is the first concrete implementation inside a broader neural architecture.

- MLP: implemented by `MLPBrain`, created through `BrainFactory`, executed through `BrainExecutor` / `NeuralSystem`.
- Gated MLP: future implementation can add gate state/config and reuse dense layer execution grouping.
- Shortcut MLP: future implementation can add input-to-output shortcut weights without changing `MovementSystem`.
- Modulated MLP: future implementation can combine gates and shortcut parameters under the same `BrainType` / `BrainConfig` flow.
- Simple RNN: future implementation can store recurrent state in `BrainState::recurrentState` per brain/agent.
- NEAT common: future implementation can add a heterogeneous graph brain type and use individual fallback execution.
- NEAT simplified: future implementation can reuse graph storage with lower mutation rates and limits.
- NEAT recurrent: future implementation can extend the NEAT graph with recurrent state and memory decay.
- Dense batch execution: `BrainConfig::architectureSignature()` and `MLPBrain::batchKey()` provide grouping keys for future batch execution.
- NEAT fallback execution: graph brains can bypass dense batch and run individually without adding cost to the MLP path when disabled.
- `input_size`: currently temporary; Phase 10/11 will replace it with sensor/retina size.
- `output_size`: already follows Phase 8 movement mode.
- `ActivationTrace`: MLP exposes per-layer activations for future neural viewer.
- Serialization: not implemented; future save/load should serialize `BrainConfig`, type, weights, biases, revision and any future extras.
- Different brain types by species/genome: not implemented; future species/genome phases should build `BrainConfig` per species.

Critical rule preserved:
- MLP is not hardcoded into `Renderer`.
- MLP is not hardcoded into `MovementSystem`.
- MLP is not stored inside `AgentStore`.
- `App` only asks `NeuralSystem` for movement controls.

## Tests Executed

Commands:

```powershell
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Debug
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Release
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase9-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe' --phase9-diagnostics
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase7-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase8-selftest
```

Result:

```text
Phase9 validation: PASS (23 checks)
All Phase 9 validation checks passed. brainTypesRecognized=8 agentsTested=1 inputSize=4 outputSize=2
Phase7 validation: PASS (14 checks)
Phase8 validation: PASS (15 checks)
```

Validation coverage:
- `BrainType` recognizes MLP and all planned future types.
- Future brain types are recognized but not implemented.
- `BrainConfig` reads bacteria neural defaults.
- `BrainConfig` can read predator neural defaults for future species support.
- MLP creates expected architecture.
- Forward returns expected output size.
- Forward is deterministic with same seed/input.
- Hidden layers use `tanh`.
- Output layer is linear.
- `ActivationTrace` exposes dense layers.
- `mutation_rate=0` does not change parameters.
- `mutation_rate>0` changes parameters.
- clone/copy preserves outputs.
- `resizeInput` works.
- MLP output feeds `MovementSystem`.
- Forward mode uses 2 outputs.
- Omni mode uses 3 outputs.
- Phase 7 interaction still works.
- Phase 8 locomotion validation still passes.

## Microbenchmark

Mode:
- Release.

Configuration:
- temporary input size `4`;
- output size `2`;
- 100 forward passes per agent;
- individual MLP forward only;
- no formal benchmark runner or CSV/JSON output.

| Architecture | Input | Output | Hidden layers | Agents | Forwards | Total ms | Avg forward us |
|---|---:|---:|---|---:|---:|---:|---:|
| small | 4 | 2 | 8 | 100 | 10000 | 2.4135 | 0.2414 |
| small | 4 | 2 | 8 | 300 | 30000 | 7.0728 | 0.2358 |
| small | 4 | 2 | 8 | 600 | 60000 | 14.0761 | 0.2346 |
| small | 4 | 2 | 8 | 1000 | 100000 | 23.7613 | 0.2376 |
| medium | 4 | 2 | 16/16 | 100 | 10000 | 6.4372 | 0.6437 |
| medium | 4 | 2 | 16/16 | 300 | 30000 | 23.2823 | 0.7761 |
| medium | 4 | 2 | 16/16 | 600 | 60000 | 50.0955 | 0.8349 |
| medium | 4 | 2 | 16/16 | 1000 | 100000 | 85.6168 | 0.8562 |
| default_bacteria | 4 | 2 | 20/20/20/20 | 100 | 10000 | 19.3401 | 1.9340 |
| default_bacteria | 4 | 2 | 20/20/20/20 | 300 | 30000 | 61.8990 | 2.0633 |
| default_bacteria | 4 | 2 | 20/20/20/20 | 600 | 60000 | 128.1174 | 2.1353 |
| default_bacteria | 4 | 2 | 20/20/20/20 | 1000 | 100000 | 222.6171 | 2.2262 |

Observation:
- This is a Phase 9 smoke microbenchmark, not the formal benchmark suite.

## Build Results

Debug:
- compiled.

Release:
- compiled.

Note:
- MSBuild still prints a post-build message that `pwsh.exe` is not recognized, but the build command returned exit code `0` and produced the executable.

## Limitations

- Neural input is temporary and not sensory.
- No real retina/vision input.
- No batch neural executor yet.
- No neural serialization.
- No per-species brain ownership yet.
- No reproduction/mutation pipeline integration yet.
- `structural_jitter` is preserved but not functionally applied.
- Advanced neural parameters are recognized for future design but not functionally used.
- `BrainHandle` is present as a future-facing type but not wired into `AgentStore`.

## Future Work

- Phase 10/11 should replace temporary input with sensor/vision input.
- Future reproduction phases should clone/mutate brains through the factory/executor boundary.
- Future species phases should move neural config to species/genome definitions.
- Future neural phases should implement Gated, Shortcut, Modulated, RNN and NEAT without changing `MovementSystem`.
- Future UI phases should expose neural type and parameters.
- Future save/load should serialize brain type, architecture, weights, biases and versioned extras.

## Final Audit

Date:
- 2026-05-27.

Scope audit:
- All Phase 9 changes are inside `C_SFML_Teste_Legado/AgentBioSimCpp` and `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- No Python files were modified.
- No real sensors were implemented.
- No vision was implemented.
- No Gated MLP, Shortcut MLP, Modulated MLP, RNN or NEAT was implemented.
- No reproduction was implemented.
- No functional predators were implemented.
- No save/load was implemented.
- No complete UI or neural viewer was implemented.
- Phase 10 was not started.

Corrections made during Phase 9:
- Added a controlled fallback path for future brain types.
- Added validation that predator neural defaults can be read for future species work.
- Added regression checks for Phase 7 and Phase 8 behavior.

Divergences found:
- No unregistered behavioral divergence was found.
- The deliberate temporary input vector is documented as a Phase 9-only replacement until sensors/vision exist.

## Confirmation

- No Python file was modified.
- No real sensors were implemented.
- No vision was implemented.
- No reproduction was implemented.
- No functional predator behavior was implemented.
- No save/load was implemented.
- No full UI was implemented.
- No neural viewer was implemented.
- No Gated MLP, Shortcut MLP, Modulated MLP, RNN or NEAT was implemented.
- Phase 10 was not started.
