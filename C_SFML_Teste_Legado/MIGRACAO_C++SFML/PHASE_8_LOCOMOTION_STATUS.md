# Phase 8 Status: Locomotion

## Scope

Phase 8 implemented only the initial locomotion foundation.

Implemented:
- `MovementSystem`.
- Forward locomotion mode.
- Omni / four-direction locomotion mode.
- Reverse locomotion toggle.
- Global inertia.
- Optional smooth locomotion with linear/angular acceleration and drag.
- Wall containment and bounce for rectangular and circular worlds.
- Minimal hot-data support in `AgentStore` for angular velocity and body shape.
- Temporary synthetic controls for visual/runtime testing before MLP exists.
- Console validation command.
- Console microbenchmark command.

Not implemented:
- Vision/perception.
- Neural networks / MLP.
- Reproduction.
- Functional predators.
- Save/load.
- Complete UI.
- Agent-agent collision.
- Obstacles.
- Brownian motion.
- Global viscosity.
- Render interpolation pose buffers.
- Detailed body-shape rendering.

## Python Reference Files Consulted

- `sim/actuators.py`
- `sim/entities.py`
- `sim/controllers.py`
- `sim/engine.py`
- `sim/world.py`
- `sim/systems.py`
- `sim/fast_kernels.py`

## Additional Python Files Consulted

None.

## Documentation Consulted

- `CODEX_MIGRATION_GUIDE.md`
- `MIGRATION_PHASES.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`
- `MIGRATION_RISKS.md`
- `BENCHMARK_PLAN.md`

## Concepts Confirmed Against Python

- `movement_mode = forward` uses two outputs: speed and steering.
- `movement_mode = omni` uses three outputs: forward, strafe and steering.
- Forward mode uses sigmoid for speed when reverse locomotion is disabled.
- Forward mode uses `tanh` for signed speed when reverse locomotion is enabled.
- Steering uses `tanh` and is scaled by `max_turn`.
- Omni mode uses `tanh` for forward/strafe/steer and normalizes local movement magnitude to `1.0`.
- Velocity target is scaled by `max_speed`.
- Global `agents_inertia` blends current velocity toward desired velocity when greater than `1.0`.
- Smooth locomotion uses limited angular acceleration, limited linear acceleration and optional drag.
- Rectangular wall handling clamps position and reverses the impacted velocity component by `-0.5`.
- Circular wall handling clamps radially and removes outward radial velocity with the Python `1.5` response factor.
- Python update flow applies locomotion before energy.

## Divergences and Decisions

### Temporary Controls Before MLP

The C++ version still has no neural network phase. `MovementSystem::apply` accepts explicit `MovementControl` vectors for tests and future MLP integration.

When no control vector is provided, it generates deterministic synthetic controls from agent index and age. This is only for Phase 8 runtime/visual smoke testing.

Future replacement:
- Phase 9 MLP should provide the movement outputs.
- The synthetic path should remain only as a debug/test fallback.

### Forward Movement

Forward mode was implemented to match Python:

- speed command: sigmoid if reverse is disabled;
- speed command: `tanh` if reverse is enabled;
- turn command: `tanh`;
- angle is normalized to `[-pi, pi]`;
- velocity follows the agent heading.

### Omni Movement

Omni mode was implemented in this phase because the Python behavior is explicit and low risk.

Convention:
- `forward` is local forward/backward along the agent heading;
- `strafe` is local lateral movement;
- `turn` still rotates the agent;
- local movement vector is normalized if its magnitude exceeds `1.0`.

### Smooth Locomotion / Inertia / Drag

Smooth locomotion was implemented because the formulas are contained in `sim/actuators.py` and the required parameters already exist in `ParameterRegistry`.

Supported:
- `smooth_locomotion_enabled`;
- `smooth_linear_inertia_enabled`;
- `smooth_max_linear_accel`;
- `smooth_linear_drag_enabled`;
- `smooth_linear_drag`;
- `smooth_angular_inertia_enabled`;
- `smooth_max_angular_accel`;
- `smooth_angular_drag_enabled`;
- `smooth_angular_drag`.

### Body Shape

`BodyShapeCode` was added to the entity spawn/store data to preserve the genome/body setting.

Phase 8 does not implement detailed body-shape rendering. The current renderer still draws simple circles from Phase 5.

### Render Interpolation

`render_interpolation_enabled` is read into `MovementConfig`, but previous-pose buffers were not implemented in this phase.

Reason:
- Render interpolation affects renderer/snapshot behavior and should be completed when the render pipeline is ready for interpolation.

### World Limits

The movement system applies the wall response from Python instead of a silent position-only clamp:

- rectangular: clamp plus velocity component bounce;
- circular: radial clamp plus radial velocity correction.

This prevents agents escaping the substrate while preserving the current Python movement feel.

## Files Created

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/MovementSystem.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/MovementSystem.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/Phase8Diagnostics.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/Phase8Diagnostics.cpp`
- `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_8_LOCOMOTION_STATUS.md`

## Files Modified

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/main.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/EntityTypes.hpp`

## System Implemented

### MovementSystem

Implemented:
- movement config from `ParameterRegistry`;
- synthetic control fallback;
- explicit test/future control vector input;
- forward motion;
- omni motion;
- reverse locomotion;
- global inertia;
- smooth locomotion;
- rectangular wall response;
- circular wall response;
- per-step movement stats.

The system is separate from `Renderer` and does not depend on UI.

## Parameters Used

Used through `ParameterRegistry` where applicable:

- `bacteria_movement_mode`
- `bacteria_max_speed`
- `bacteria_max_turn`
- `bacteria_allow_reverse_locomotion`
- `bacteria_body_shape`
- `allow_reverse_locomotion`
- `agents_inertia`
- `smooth_locomotion_enabled`
- `smooth_linear_inertia_enabled`
- `smooth_max_linear_accel`
- `smooth_linear_drag_enabled`
- `smooth_linear_drag`
- `smooth_angular_inertia_enabled`
- `smooth_max_angular_accel`
- `smooth_angular_drag_enabled`
- `smooth_angular_drag`
- `render_interpolation_enabled`

## Parameters Pending or Partially Connected

- `render_interpolation_enabled`: read, but previous-pose/render interpolation buffers are pending.
- Predator movement parameters: already registered, but functional predators are not part of Phase 8.
- Per-species movement configs: pending `SpeciesStore`.
- `global_viscosity_enabled` and `global_viscosity_drag`: not part of Phase 8.
- `brownian_motion_enabled` and `brownian_motion_strength`: not part of Phase 8.
- Agent collision/elasticity parameters: not part of Phase 8.

## App Integration

The runtime step order is now:

1. `MovementSystem`;
2. `EnergySystem`;
3. `SpatialHash` rebuild;
4. `InteractionSystem`;
5. `DeathSystem`.

This preserves the Python concept that movement velocity affects the following energy/metabolism cost.

## Tests Executed

Commands:

```powershell
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase8-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe' --phase8-diagnostics
```

Result:

```text
Phase8 validation: PASS (15 checks)
All Phase 8 validation checks passed. agentsTested=10 modes=forward, omni
```

Validation coverage:
- agent with positive speed moves forward;
- steering changes angle;
- `max_speed` is respected;
- `max_turn` is respected;
- reverse disabled prevents signed reverse movement;
- reverse enabled allows signed reverse movement;
- rectangular world containment works;
- circular world containment works;
- omni strafe works;
- smooth acceleration limit works;
- Phase 7 food interaction still works after movement;
- EnergySystem still reacts to movement velocity.

## Microbenchmark

Mode:
- Release.

Configuration:
- one movement step per scenario;
- deterministic controls;
- rectangular world `1000 x 700`;
- `dt = 1 / 30`;
- forward and omni modes;
- smooth locomotion disabled/enabled.

Result:

| Agents | Movement mode | Smooth | Step ms | Agents processed | Max speed observed | Wall collisions |
|---:|:---|:---:|---:|---:|---:|---:|
| 100 | forward | false | 0.0070 | 100 | 240.6510 | 0 |
| 100 | forward | true | 0.0099 | 100 | 29.2593 | 0 |
| 100 | omni | false | 0.0085 | 100 | 278.4276 | 0 |
| 100 | omni | true | 0.0112 | 100 | 29.2593 | 0 |
| 300 | forward | false | 0.0191 | 300 | 240.6510 | 0 |
| 300 | forward | true | 0.0280 | 300 | 29.2593 | 0 |
| 300 | omni | false | 0.0229 | 300 | 278.4276 | 0 |
| 300 | omni | true | 0.0331 | 300 | 29.2593 | 0 |
| 600 | forward | false | 0.0392 | 600 | 240.6510 | 0 |
| 600 | forward | true | 0.0561 | 600 | 29.2593 | 0 |
| 600 | omni | false | 0.0434 | 600 | 278.4276 | 0 |
| 600 | omni | true | 0.0676 | 600 | 29.2593 | 0 |
| 1000 | forward | false | 0.0630 | 1000 | 240.6510 | 0 |
| 1000 | forward | true | 0.0987 | 1000 | 29.2593 | 0 |
| 1000 | omni | false | 0.0722 | 1000 | 278.4276 | 0 |
| 1000 | omni | true | 0.1058 | 1000 | 29.2593 | 0 |

These numbers are only a Phase 8 smoke benchmark. They are not the full benchmark suite.

## Build Results

Debug:

```powershell
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Debug
```

Result:
- compiled.

Release:

```powershell
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Release
```

Result:
- compiled after closing a stale `AgentBioSimCpp.exe` process that was locking the output executable.

Observation:
- Latest audit run compiled cleanly in Debug and Release.

## Visual Smoke Check

Command:

```powershell
$exe = Resolve-Path 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe'; $p = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Hidden; Start-Sleep -Seconds 4; if ($p.HasExited) { "exited:$($p.ExitCode)" } else { Stop-Process -Id $p.Id -Force; "started_ok" }
```

Result:

```text
started_ok
```

Interpretation:
- window opens;
- Release executable remains running long enough for a smoke check;
- process was stopped intentionally after the check;
- render from Phase 5 remains intact.

## Final Audit

Date:
- 2026-05-27.

Scope audit:
- All current Phase 8 changes are inside `C_SFML_Teste_Legado/AgentBioSimCpp` and `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- No Python files are modified.
- Phase 8 adds locomotion support only: `MovementSystem`, diagnostics, minimal `AgentStore` hot-data support, App orchestration and status documentation.
- No vision, sensors, neural network, MLP, reproduction, functional predators, save/load or full UI were implemented.
- Phase 9 was not started.

Architecture audit:
- `MovementSystem` is a separate system and has no dependency on SFML, UI or `Renderer`.
- `MovementSystem` operates over `AgentStore` and `World`.
- `AgentStore` remains data-oriented/SoA, using contiguous vectors for hot fields.
- No polymorphic per-agent class was introduced.
- `App` only orchestrates movement, energy, spatial rebuild, interaction, death and rendering.
- `Renderer` remains a reader of simulation state; it does not decide movement.
- The control-vector input path is ready for future MLP output integration.
- Synthetic controls are documented as temporary runtime/test fallback before Phase 9.

Parameter audit:
- Required movement parameters are read through `ParameterRegistry` when available.
- `render_interpolation_enabled` is read but render interpolation pose buffers remain pending.
- Predator, per-species, collision, Brownian and viscosity parameters remain pending for later phases.

Final validation commands:

```powershell
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Debug
cmake --build 'C_SFML_Teste_Legado/AgentBioSimCpp/build' --config Release
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase8-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe' --phase8-diagnostics
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase7-selftest
```

Final validation results:
- Debug build passed.
- Release build passed.
- Phase 8 self-test passed: `PASS (15 checks)`.
- Phase 8 diagnostics passed and produced the microbenchmark table above.
- Phase 7 regression self-test passed: `PASS (14 checks)`.
- Release visual smoke returned `started_ok`.

Corrections made during final audit:
- Updated this status document with the latest Phase 8 benchmark values and final audit notes.
- No C++ code correction was needed during the final audit.

Divergences found during final audit:
- No new unregistered behavioral divergence was found.
- Existing documented divergences remain: synthetic controls before MLP and pending render interpolation buffers.

## Limitations

- Movement controls are synthetic until Phase 9 MLP exists.
- No neural output is connected yet.
- No per-species movement grouping beyond the default bacteria parameters.
- No functional predators.
- No obstacle collisions.
- No agent-agent collisions.
- No reproduction.
- No save/load.
- No complete UI controls.
- No render interpolation buffers.
- No detailed ellipse/circle renderer behavior.

## Future Work

- Connect MLP outputs to `MovementSystem` in Phase 9.
- Replace synthetic controls with neural outputs for real simulation behavior.
- Move movement config to `SpeciesStore` when species are implemented.
- Add render interpolation buffers when render snapshots are formalized.
- Add collision/viscosity/Brownian systems in dedicated physics phases.
- Add predator/species movement after predator/species phase.

## Confirmation

- No Python file was modified.
- No vision was implemented.
- No neural network was implemented.
- No reproduction was implemented.
- No functional predator behavior was implemented.
- No save/load was implemented.
- No full UI was implemented.
- Phase 9 was not started.
