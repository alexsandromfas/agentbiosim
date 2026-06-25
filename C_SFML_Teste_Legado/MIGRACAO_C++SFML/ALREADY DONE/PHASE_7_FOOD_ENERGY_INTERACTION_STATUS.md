# Phase 7 Status: Food, Energy and Basic Interaction

## Scope

Phase 7 implemented only the initial food/energy/interaction foundation.

Implemented:
- Basic `EnergySystem`.
- Basic `InteractionSystem`.
- Basic `DeathSystem`.
- Agent energy mutation helpers in `AgentStore`.
- Minimal App integration through fixed timestep steps.
- Console validation command.
- Console microbenchmark command.

Not implemented:
- Vision/perception.
- Neural networks.
- Reproduction.
- Functional predators.
- Save/load.
- Full UI.
- Complete chunk food behavior.
- Complete locomotion/Phase 8.

## Python Reference Files Consulted

- `sim/systems.py`
- `sim/entities.py`
- `sim/actuators.py`
- `sim/controllers.py`
- `sim/spatial.py`
- `sim/engine.py`
- `sim/world.py`
- `sim/render.py`

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

- Instant food gives energy immediately when an agent touches it.
- Python uses `food.energy * diet_food_efficiency` for generic organisms.
- Bacteria default food efficiency is `1.0`.
- Food energy defaults to roughly `radius * radius`.
- Agents consume at most one food item per frame/system pass.
- Energy metabolism is linear:

`cost(v) = v0_cost + clamp(v, 0, vmax_ref) / vmax_ref * (vmax_cost - v0_cost)`

- Energy is clamped to zero after cost.
- Energy cap is reinforced after changes.
- Death by energy occurs when `energy <= death_energy`.
- Python `DeathSystem` limits deaths per step.
- Python can convert corpses to food, but that behavior is not part of Phase 7.
- Chunk food exists, but complete chunk/bite behavior is explicitly deferred.

## Divergences and Decisions

### Step Order Correction

During the final audit, the Python reference showed the effective step order as:

1. agent update / energy cost;
2. food interaction;
3. death/removal.

The initial C++ App integration was applying food interaction before energy. This was corrected during the Phase 7 audit so `App::runSimulationStep` now applies `EnergySystem`, then `InteractionSystem`, then `DeathSystem`.

Reason:
- This better preserves the Python functional reference without expanding the scope beyond Phase 7.
- The correction only changes Phase 7 system orchestration and does not implement Phase 8 locomotion.

### Food Mode

Only `FoodKind::Instant` is consumed in Phase 7.

If a food item has `FoodKind::Chunk`, `InteractionSystem` skips it and increments `chunkFoodsSkipped`.

Reason:
- The user explicitly requested only mandatory instant food for this phase.
- Chunk behavior is larger and includes contact time, collision, movable particles, replenishment modes and possibly spatial rebuild policy.

### SpatialHash Use

`InteractionSystem` uses `SpatialHash` when:
- `use_spatial` is true;
- a valid spatial hash pointer is passed;
- the grid is not empty.

If unavailable or disabled, it falls back to a brute-force scan of `FoodStore`.

Reason:
- This preserves the Python pattern where systems can run with or without spatial acceleration.

### Spatial Query Contract

Phase 6 `SpatialHash::queryRadiusInto` returns exact disk-intersection results, not raw coarse candidates.

For food interaction, the query radius is the agent radius. Because the spatial query includes each entity radius internally, this is equivalent to checking:

`distance(agent, food) <= agent_radius + food_radius`

The system still runs a final touching check as a defensive guard.

### Death and Removal

`DeathSystem` removes agents by stable `EntityId` through `AgentStore::removeAgent`.

Removal uses the existing swap-remove strategy from `AgentStore`.

Phase 7 does not implement:
- population minimum rescue;
- label/species limits;
- corpse-to-food.

Those remain future compatibility work.

### App Demonstration

Because there is still no real locomotion in this migration phase, the App places the first demo food item at the first demo agent position. This creates a deterministic contact so the Phase 7 interaction path can be observed without implementing Phase 8 movement.

This is temporary demonstration behavior and is documented as such.

## Files Created

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/EnergySystem.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/EnergySystem.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/InteractionSystem.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/InteractionSystem.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/DeathSystem.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/DeathSystem.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/Phase7Diagnostics.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/systems/Phase7Diagnostics.cpp`
- `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_7_FOOD_ENERGY_INTERACTION_STATUS.md`

## Files Modified

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/main.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/AgentStore.cpp`

## Systems Implemented

### EnergySystem

Implemented:
- linear metabolism from idle cost to max-speed cost;
- per-step energy reduction;
- energy cap reinforcement;
- age advancement.

### InteractionSystem

Implemented:
- food collision/touch detection;
- instant food consumption;
- energy gain with food efficiency;
- energy cap during gain;
- removal of consumed food;
- spatial path and brute force fallback;
- one food item consumed per agent per pass.

### DeathSystem

Implemented:
- energy-threshold death;
- max deaths per step;
- removal by `EntityId`.

## Parameters Used

Used through `ParameterRegistry` where applicable:
- `bacteria_initial_energy`
- `bacteria_death_energy`
- `bacteria_energy_cap`
- `bacteria_metab_v0_cost`
- `bacteria_metab_vmax_cost`
- `bacteria_max_speed`
- `food_mode`
- `food_target`
- `food_min_r`
- `food_max_r`
- `food_color`
- `bacteria_diet_food`
- `bacteria_diet_food_efficiency`
- `use_spatial`
- `max_deaths_per_step`
- `bacteria_corpse_to_food`

## Parameters Pending

Registered as future work:
- predator energy parameters;
- predator diet behavior;
- population minimum rescue;
- label/species min/max limits;
- corpse-to-food;
- chunk food contact time;
- chunk food replenishment modes;
- food target replenishment after consumption.

## Tests Executed

Commands:

```powershell
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --phase7-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe' --phase7-diagnostics
```

Result:

```text
Phase7 validation: PASS (14 checks)
All Phase 7 validation checks passed. foodsConsumed=3 deaths=3
```

Validation coverage:
- agent touching instant food with SpatialHash gains energy;
- consumed food is removed;
- agent far from food does not consume it;
- brute-force fallback consumes touching instant food;
- idle energy cost is applied;
- agent age advances with energy step;
- energy cap is respected;
- agent dies when `energy <= death_energy`;
- `max_deaths_per_step` is respected;
- chunk food is skipped in Phase 7.

## Microbenchmark

Mode:
- Release.

Configuration:
- one energy/interaction/death step per scenario;
- deterministic positions;
- some foods are spawned directly over agents to create food-consumption events;
- `SpatialHash` cell size: `36.0`;
- instant food only.

Result:

| Agents | Foods | SpatialHash | Step ms | Foods consumed | Deaths |
|---:|---:|:---:|---:|---:|---:|
| 100 | 100 | true | 0.0417 | 25 | 0 |
| 100 | 100 | false | 0.0766 | 25 | 0 |
| 300 | 150 | true | 0.1345 | 79 | 0 |
| 300 | 150 | false | 0.3044 | 79 | 0 |
| 600 | 300 | true | 0.3106 | 171 | 0 |
| 600 | 300 | false | 1.1322 | 171 | 0 |

These numbers are only a Phase 7 smoke benchmark. They are not the full benchmark suite.

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
- compiled.

Observation:
- MSBuild printed `'pwsh.exe' nao e reconhecido...` during the post-build step, but returned success and produced the executable. This appears to be an existing environment/post-build message, not a Phase 7 code failure.

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
- The SFML executable starts and remains running long enough for Phase 5 render and Phase 7 basic systems to be considered intact.
- The process was stopped intentionally after the smoke check.

## Limitations

- No food replenishment yet.
- No movement/locomotion yet; demo contact is deterministic.
- No predator behavior.
- No reproduction.
- No neural network or vision.
- No species/label constraints.
- No corpse-to-food.
- No full chunk food behavior.
- No UI controls for these systems yet.
- No formal benchmark output files yet.

## Final Audit Notes

Scope audit:
- Phase 7 changes are limited to `C_SFML_Teste_Legado/AgentBioSimCpp` and this status document.
- No Python file was modified.
- No vision, neural network, reproduction, functional predator behavior, save/load, complete UI or Phase 8 locomotion was implemented.

Architecture audit:
- `EnergySystem`, `InteractionSystem` and `DeathSystem` are separate from `Renderer`.
- The systems do not depend on UI.
- The systems read and mutate stores, but do not perform rendering.
- `InteractionSystem` uses `SpatialHash` when enabled and falls back to brute force when disabled.
- Food removal is done through `FoodStore`.
- Agent death/removal is done through `AgentStore`.

Parameter audit:
- Required Phase 7 parameters are read through `ParameterRegistry` where available.
- `food_target`, `food_min_r`, `food_max_r`, `food_color` and `food_mode` are currently used by demo spawning.
- `bacteria_corpse_to_food` is read by `DeathSystem` config but intentionally not applied yet.
- Chunk/piece food, predator diets, population rescue and species/label constraints remain pending.

Correction made during final audit:
- `App::runSimulationStep` was corrected to apply energy before food interaction and death, matching the Python reference flow.
- The Phase 7 microbenchmark was updated to include `EnergySystem` in the measured step.

## Future Work

- Add food replenishment and target control in a later food system phase.
- Add locomotion in Phase 8.
- Integrate species/label diet configs after SpeciesStore exists.
- Add predator food/prey interaction in predator phase.
- Add corpse-to-food after death and food creation policies are migrated.
- Add chunk food behavior as a separate measured feature.

## Confirmation

- No Python file was modified.
- No vision was implemented.
- No neural network was implemented.
- No reproduction was implemented.
- No functional predator behavior was implemented.
- No save/load was implemented.
- No full UI was implemented.
- Phase 8 was not started.
