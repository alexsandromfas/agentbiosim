# Phase 6 Status: Spatial Hash

## Scope

Phase 6 implemented only the base spatial acceleration structure for the C++ migration.

Implemented:
- `SpatialHash` module.
- Radius and AABB proximity queries.
- Rebuild from `AgentStore` and `FoodStore`.
- Minimal App integration for rebuilding the grid after the Phase 4 demo spawn.
- Console self-test command.
- Console microbenchmark command.

Not implemented:
- Vision/perception.
- Neural networks.
- Feeding or food consumption.
- Energy/metabolism systems.
- Reproduction.
- Functional predators.
- Obstacles.
- Full UI.
- Full benchmark infrastructure.

## Python Reference Files Consulted

- `sim/spatial.py`
- `sim/entities.py`
- `sim/systems.py`
- `sim/sensors.py`
- `sim/controllers.py`
- `sim/engine.py`

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

- The Python project uses a spatial hash/grid as a proximity acceleration structure.
- Python rebuilds/inserts foods, bacteria, and predators into a spatial grid.
- Python circular substrate bounds are derived from center plus/minus radius.
- Python rectangular bounds use the world width/height range.
- Python cell size is computed from the largest configured food/bacteria/predator radius times two.
- Spatial queries are used by systems and sensors, but detailed behavior such as feeding, collision, and vision remains outside this phase.
- Python has optimized query paths that can return candidates into reusable buffers.

## Divergences and Decisions

### Query Filtering

Python `query_ball` primarily returns cell-range candidates. Some Python call sites perform additional filtering after the spatial query.

C++ Phase 6 `queryRadius` returns only entities whose disk intersects the query disk:

`distance(center, item.center) <= query_radius + item.radius`

Reason:
- This gives a deterministic and easy-to-test contract for future C++ systems.
- It makes the Phase 6 correction tests compare directly against brute force.
- If a later phase needs raw coarse candidates for performance-sensitive vision, an explicit coarse query can be added without changing this exact-query contract.

### ID vs Index

`SpatialItem` stores both:
- stable `EntityId`;
- current store index.

Reason:
- IDs are safer as public references.
- Store indices are useful for hot-path access into SoA stores.

Impact:
- Because stores use swap-remove, stored indices are valid only until the corresponding store changes.
- The grid must be rebuilt after entity creation/removal/reordering.
- Incremental updates are a future optimization, not part of Phase 6.

### Cell Size

No dedicated public `spatial_cell_size` parameter was found in the current C++ registry.

The App uses the Python-compatible provisional formula:

`max(food_max_r, bacteria_max_r, predator_max_r) * 2.0`

With current defaults:
- `food_max_r = 5.0`
- `bacteria_max_r = 12.0`
- `predator_max_r = 18.0`
- resulting cell size: `36.0`

The standalone validation and microbenchmark also use `36.0`.

## Files Created

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/SpatialHash.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/simulation/SpatialHash.cpp`
- `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_6_SPATIAL_HASH_STATUS.md`

## Files Modified

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/main.cpp`

## Structures Implemented

### `SpatialEntityType`

Current values:
- `Agent`
- `Food`
- `Obstacle`

`Obstacle` is reserved for future phases and is not populated yet.

### `SpatialHashConfig`

Stores:
- `cellSize`
- `minBounds`
- `maxBounds`

### `SpatialItem`

Stores:
- entity id;
- store index;
- entity type;
- type code;
- x/y;
- radius.

### `SpatialHash`

Implemented:
- `configure`
- `clear`
- `insert`
- `rebuild`
- `queryRadius`
- `queryRadiusInto`
- `queryAabb`
- `queryAabbInto`
- `stats`

### Diagnostics Helpers

Implemented:
- `runSpatialHashValidation`
- `runSpatialHashMicrobenchmark`

These are temporary Phase 6 diagnostics, not the full benchmark system.

## Parameters Used

Read from `ParameterRegistry` where possible:
- `use_spatial`
- `reuse_spatial_grid`
- `world_w`
- `world_h`
- `substrate_shape`
- `substrate_radius`
- `food_max_r`
- `bacteria_max_r`
- `predator_max_r`

## Parameters Pending

- No public `spatial_cell_size` parameter is currently connected.
- No public switch for exact vs coarse query mode exists.
- No UI control exists yet for spatial hash diagnostics.

These remain future decisions because Phase 6 is infrastructure-only.

## App Integration

The App now:
- creates demo agents/food as before;
- configures the grid from the current world;
- rebuilds the grid once after demo entity spawn;
- prints basic spatial stats to console;
- shows occupied/total cell count in the window title.

The App does not use the grid for simulation behavior yet.

## Correction Tests

Commands:

```powershell
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe' --spatial-selftest
& 'C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe' --spatial-diagnostics
```

Result:

```text
SpatialHash validation: PASS (9 checks)
All SpatialHash validation checks passed.
```

Tested cases:
- empty query;
- one entity inside radius;
- entity outside radius;
- multiple entities in the same cell;
- entities in neighboring cells;
- entities near bounds;
- clear;
- rebuild with mixed agents and food;
- AABB query.

## Brute Force Comparison

The deterministic validation compares `SpatialHash::queryRadiusInto` against a brute force implementation using the same exact disk-intersection rule.

Result:
- all compared results matched.

## Microbenchmark

Mode:
- Release.

Configuration:
- world size: `1000 x 700`;
- cell size: `36.0`;
- query radius: `80.0`;
- queries per scenario: `1000`;
- deterministic random seed: `20260527`.

Result:

| Entities | Queries | Rebuild ms | Query total ms | Avg query us | Avg candidates |
|---:|---:|---:|---:|---:|---:|
| 100 | 1000 | 0.0234 | 0.1825 | 0.1825 | 3.0260 |
| 1000 | 1000 | 0.2399 | 0.8721 | 0.8721 | 30.7540 |
| 10000 | 1000 | 1.5270 | 6.1049 | 6.1049 | 307.5870 |

These numbers are only a Phase 6 smoke benchmark. They should not be treated as the final benchmark suite.

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
- The SFML executable starts and remains running long enough for the Phase 5 simple render path to be considered intact.
- The process was stopped intentionally after the smoke check.

## Final Audit

Scope audit:
- All changes are inside `C_SFML_Teste_Legado/AgentBioSimCpp` and `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- No Python files are modified.
- Phase 6 only adds the spatial hash/grid infrastructure, diagnostics, and minimal App statistics.
- The implementation did not add vision, neural networks, feeding, energy/metabolism, reproduction, functional predators, full UI, or Phase 7 behavior.

Architecture audit:
- `SpatialHash` is in its own module under `src/simulation`.
- It has no UI or renderer dependency.
- It does not mutate simulation stores.
- It stores IDs and store indices, not pointers to heavy polymorphic objects.
- It currently handles agents and food and reserves `Obstacle` for future phases.
- It is suitable for future use by vision, feeding, collision, predators, and obstacles.
- It is not coupled to `Renderer`.

Correction audit:
- Debug self-test passed.
- Release diagnostics passed.
- Brute force comparison matched the documented exact-query rule.

Corrections made during final audit:
- None in C++ code after the audit rerun.
- This status file was updated with the final audit run, current microbenchmark values, and visual smoke check.

## Limitations

- Grid is rebuilt as a full rebuild, not incrementally updated.
- Store indices inside `SpatialItem` require rebuild after store mutation.
- No obstacle storage yet.
- No query filtering by entity type helper yet.
- No coarse-candidate query API yet.
- No full benchmark output files or benchmark runner yet.
- No visual grid overlay yet.
- No behavior system consumes the grid yet.

## Future Work

- Add type-filtered query helpers when vision/feeding/collision need them.
- Consider adding a coarse query variant if sector vision benefits from avoiding exact filtering inside the hash.
- Add obstacle insertion when obstacle stores are migrated.
- Add profiling counters for rebuild/query call sites once systems exist.
- Decide whether `spatial_cell_size` should become a public parameter or stay derived from radii.

## Confirmation

- No Python file was modified.
- No vision was implemented.
- No neural network was implemented.
- No feeding/food consumption was implemented.
- No energy/metabolism system was implemented.
- No reproduction was implemented.
- No functional predator behavior was implemented.
- No full UI was implemented.
- Phase 7 was not started.
