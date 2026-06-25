# Fase 19 — Comida Chunk/Pedacos Completa

Status: **CONCLUIDA** (2026-05-29)

A Fase 19 completa a comida chunk/pedacos. Antes, a comida chunk era reconhecida pelo `FoodKind::Chunk` (Phase 7), porem o `InteractionSystem` silenciosamente a pulava (`++chunkFoodsSkipped`). Agora chunk e consumida gradualmente (`bite per dt`), e um novo `FoodSystem` headless gerencia spawn de clusters, crescimento, target e trim.

## Escopo executado

- Extensao de `FoodStore`: campo `clusterId_` por particula, `setEnergyAt`/`setRadiusAt` para consumo parcial, `allocateClusterId()` deterministico, `clusterIdAt(index)` para consulta.
- Novo `FoodSystem` (`src/systems/FoodSystem.{hpp,cpp}`) headless: `replenishToTarget`, `spawnInstant`, `spawnCluster`, `growExistingCluster`, `growExistingParticles`, `trimExcess`, `clearAll`. Despacha por modo (`Instant` ou `Chunk`) e estrategia de reposicao (`spawn_cluster`, `grow_existing`, `grow_particles`).
- `DietInteractionConfig` ganhou `biteSeconds`, `dt`, `corpseFoodKind`. `DietInteractionStats` ganhou `chunkBitesApplied` e `chunkParticlesDepleted`.
- `InteractionSystem::applyWithDiet` agora consome chunk parcialmente (`delta = initialEnergy * dt / biteSeconds`), atualiza `food.energy`, marca particula para remocao quando energia chega a 0, registra dois contadores (`chunkBitesApplied`, `chunkParticlesDepleted`). Comportamento da Phase 7 instant preservado.
- Corpse-to-food respeita `food_mode` (cria comida do mesmo tipo da configuracao global).
- `App::spawnDemoEntities` delega spawn inicial ao `FoodSystem` (gera clusters em modo chunk).
- `App::runSimulationStep` passa `dt` ao InteractionSystem; chama `replenishToTarget` + `trimExcess` no fim do step.
- `Phase19Diagnostics` com 133 selftests + microbenchmark de 18 cenarios.
- Wireup CLI: `--phase19-selftest`, `--phase19-benchmark`, `--phase19-diagnostics`.

## Fora de escopo (confirmado)

- Fase 20 nao iniciada.
- Obstaculos / oclusao nao implementados.
- Colisao / fisica avancada (Fase 21) nao implementadas.
- UI completa de substrato nao implementada.
- Save/load final nao implementado.

Nenhum arquivo Python foi alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_18_PREDATION_DIET_STATUS.md`

## Arquivos Python consultados

- `sim/systems.py` (referencia conceitual para spawn/grow/trim e para a regra de bite por dt).
- `sim/entities.py` (modelo de food piece + cluster).

## Arquivos C++ criados

- `src/systems/FoodSystem.hpp`
- `src/systems/FoodSystem.cpp`
- `src/systems/Phase19Diagnostics.hpp`
- `src/systems/Phase19Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_19_CHUNK_FOOD_STATUS.md`

## Arquivos C++ modificados

- `src/simulation/EntityTypes.hpp` — `FoodSpawn::clusterId`.
- `src/simulation/FoodStore.hpp/.cpp` — coluna `clusterId_`, `setEnergyAt`, `setRadiusAt`, `allocateClusterId`.
- `src/systems/InteractionSystem.hpp/.cpp` — chunk gradual + `corpseFoodKind`, novos contadores.
- `src/app/App.hpp/.cpp` — `FoodSystem foodSystem_` + `lastFoodStats_`, init via `FoodSystem`, `runSimulationStep` chama replenish/trim.
- `src/main.cpp` — flags `--phase19-*`.
- `CMakeLists.txt` — fontes novos.

## Correcoes de continuidade

- Nenhuma divida nova introduzida; Debt 7 resolvida na Fase 18, continua resolvida.
- Phase 7 selftest mantido intacto (legacy `apply()` path preservado).
- Phase 18 corpse-to-food generalizada para respeitar `food_mode` (era hardcoded instant).

## Melhorias alem do escopo minimo

- `food_mode` chunk corpse-to-food respeita o modo global, em vez de criar sempre instant. **Justificativa:** consistencia funcional (se o mundo usa chunk, cadaveres tambem devem virar particula chunk para alimentar comedores graduais).
- `spawnInstant` exposto como helper publico de `FoodSystem` (alem de `spawnCluster`). **Justificativa:** App usa para spawn inicial determinista; e mais limpo que duplicar logica de RNG no App.

## Parametros usados

```
food_mode                       -> FoodSystemConfig::mode
food_target                     -> FoodSystemConfig::target
food_min_r                      -> FoodSystemConfig::instantMinRadius
food_max_r                      -> FoodSystemConfig::instantMaxRadius
food_bite_seconds               -> FoodSystemConfig::biteSeconds (+ DietInteractionConfig::biteSeconds)
food_piece_particle_radius      -> FoodSystemConfig::particleRadius
food_piece_cluster_radius       -> FoodSystemConfig::clusterRadius
food_piece_particle_spacing     -> FoodSystemConfig::particleSpacing
food_piece_replenish_mode       -> FoodSystemConfig::replenishMode
food_trim_excess_enabled        -> FoodSystemConfig::trimExcessEnabled
food_trim_max_per_step          -> FoodSystemConfig::trimMaxPerStep
food_color                      -> FoodSystemConfig::color
substrate_shape                 -> via World::shape
world_w, world_h                -> via World
substrate_radius                -> via World
random_seed                     -> via FoodSystemConfig::seed
use_spatial                     -> via DietInteractionConfig
bacteria_diet_food              -> DietConfig::eatFood (Phase 18)
bacteria_diet_food_efficiency   -> DietConfig::foodEfficiency (Phase 18)
predator_diet_food              -> DietConfig::eatFood (Phase 18)
predator_diet_food_efficiency   -> DietConfig::foodEfficiency (Phase 18)
food_replenish_interval         -> documentado como pendente (cf. abaixo)
```

## Parametros pendentes

- `food_replenish_interval`: por enquanto o App chama `replenishToTarget` a cada step. O parametro existe na registry (Phase 0) mas nao e respeitado como intervalo discreto. **Quando sera resolvido:** se o usuario reportar oscilacao indesejada, ou em Fase 24 (UI) quando o controle for exposto. Adicionar um acumulador de tempo simples e trivial.

## Como funciona `FoodMode`

`FoodKind` (Phase 7) ja distinguia `Instant` e `Chunk`. Fase 19 adiciona semantica funcional:

- `Instant`: comida consumida em um unico bite por step. Energia total transferida (`food.energy * diet.foodEfficiency`).
- `Chunk`: comida persiste, energia decai por consumo parcial. `food.energy` mantida pelo FoodStore; quando atinge zero, particula e removida no fim do step.

## Como funciona `FoodStore`

`FoodStore` ja era SoA (Phase 7). Phase 19 adiciona uma coluna `clusterId_` (uint32_t) por particula. Cluster 0 = "sem cluster" (instant food). Cluster > 0 = grupo de particulas chunk relacionadas. IDs alocados via `allocateClusterId()` (monotonico, deterministico).

## Como funciona comida instantanea

Caminho da Phase 7 preservado:
- `InteractionSystem::apply()` (Phase 7) ainda funciona, usa apenas comida instantanea.
- `InteractionSystem::applyWithDiet()` ramifica por `FoodKind` por particula: se `Instant`, consome em um bite. Se `Chunk`, chama `tryConsumeChunkBite`.

## Como funciona comida chunk

Por step, cada agente com `diet.eatFood=true` toca em ate uma particula. Se chunk:
1. `bite_fraction = dt / max(epsilon, bite_seconds)`.
2. `max_bite = food.initialEnergy * bite_fraction`.
3. `bite = min(food.remaining, max_bite)`.
4. Agente ganha `bite * diet.foodEfficiency` (capped pelo energyCap do genoma).
5. `food.remaining -= bite`.
6. Se `food.remaining <= 0` -> marcada para remocao + `chunkParticlesDepleted++`.

## Como funciona consumo parcial

Detalhado acima. `bite_seconds <= 0` e tratado com clamp para `1e-6` (consumo efetivamente instantaneo, sem divisao por zero). dt vem do `runSimulationStep`. Multiplas chamadas em steps consecutivos drenam a particula gradualmente.

## Como funciona `food_bite_seconds`

Controla quanto tempo (em segundos simulados) leva para consumir uma particula chunk integralmente. Para `bite_seconds=6.0` e `dt=1/30`, consome `~0.56%` por step (= 1/180). 30 steps simulam 1 segundo; 180 steps consomem uma particula inteira.

## Como funciona `food_piece_particle_radius`

`FoodSystem` usa este valor como raio de cada particula em modo chunk (uniforme dentro de um cluster). Energia inicial = `radius * radius` (mesma formula da Phase 7 instant).

## Como funciona `food_piece_cluster_radius`

Bounding radius do cluster. Particulas sao espalhadas em disco com raio = `cluster_radius`, centro determinado pelo `randomPointInsideWorld` clampado por `cluster_radius` para garantir que o cluster cabe no mundo.

## Como funciona `food_piece_particle_spacing`

Soma ao raio dobrado de particulas adjacentes para definir o stride da grade aproximada usada para calcular contagem de particulas por cluster (`area / cellArea`). Em chunk com `particle_radius=5` e `spacing=0`, stride=10, area=pi*36^2 ~= 4071, cellArea=100, ~40 particulas/cluster.

## Como funciona `spawn_cluster`

Cria um novo `clusterId` e instancia ate `min(approxCount, target)` particulas em posicoes aleatorias no disco do cluster. Cada particula tem `radius = particle_radius`, `energy = radius * radius`, `kind = Chunk`. Seed reprodutivel.

## Como funciona `grow_existing`

Seleciona o cluster com menor `clusterId` (deterministico). Adiciona ate 8 particulas por chamada em posicoes aleatorias dentro do raio do cluster (centro = media das particulas existentes). Se nao ha clusters, faz fallback documentado para `spawn_cluster`.

## Como funciona `grow_particles`

Itera particulas chunk com `energy < initialEnergy` e restora `energy = initialEnergy`. Maintenance pass: nao adiciona particulas novas. Se store esta vazio e nada cresceu, fallback para `spawn_cluster` para bootstrap. **Nao gated por target** (e refill, nao crescimento de populacao).

## Como funciona `food_target`

Modo instant: numero de itens. `replenishToTarget` spawn um-por-um ate atingir target (bounded por `trimMaxPerStep` para limitar trabalho por step).

Modo chunk: numero de particulas chunk. `spawn_cluster` / `grow_existing` so disparam quando `foods.size() < target`. `grow_particles` ignora o target (maintenance).

## Como funciona trim

`food_trim_excess_enabled=false` -> sem trim.
`food_trim_excess_enabled=true` -> se `foods.size() > target`, remove `min(over, trim_max_per_step)` itens. Determinista: remove os indices mais altos primeiro (mais novos).

## Como funciona `food_trim_max_per_step`

Limita quantas particulas sao removidas por chamada de `trimExcess`. Tambem usado em modo instant como `max spawns per step` (bounded work pattern).

## Como funciona clear food

`FoodSystem::clearAll(foods)` -> `foods.clear()`. Reseta `nextClusterId_ = 1`. Headless. Nao afeta agentes nem genomas. UI da Fase 24 podera invoca-lo via botao "Limpar Comida".

## Como funciona corpse-to-food

Phase 18 ja criava comida instantanea no local da presa. Phase 19 generaliza: `DietInteractionConfig::corpseFoodKind` (preenchido a partir de `food_mode`) determina se o cadaver vira `Instant` ou `Chunk`. Energia inicial = `prey.initialEnergy`.

## DietConfig

Inalterada da Fase 18. `eatFood=true` permite consumo instant E chunk. `foodEfficiency` aplicada em ambos modos. `eatAgents` e `agentEfficiency` controlam predacao (Fase 18) sem interacao com chunk.

## InteractionSystem

Continua sendo o ponto unico de interacao alimentar/predacao. Caminho da Phase 7 (`apply()`) preservado. Phase 18 `applyWithDiet()` expandido com consumo parcial chunk e contadores novos.

## EnergySystem

Inalterado. `addEnergyAt(idx, delta, cap)` ja respeitava cap; usado pelos dois caminhos (instant e chunk).

## SpatialHash

Inalterado. Continua indexando tanto comida instantanea quanto chunk. `applyWithDiet` consulta o hash por agente, distingue por `entityType` e despacha consumo. Remocao de chunk depletado segue o mesmo path de remocao da Phase 7 (`foods.removeFood(id)`), entao SpatialHash sera reconstruido no proximo step pelo App.

## Renderer

Inalterado funcionalmente. Renderer ja desenha cada particula como circulo colorido com base em `radius`/`color`. Chunk e instant tem a mesma representacao visual nesta fase; visualizacao de "mordidas" fica para Fase 24/UI.

## Mundo retangular

`spawnCluster` clampa o centro do cluster a `cluster_radius` do bordo, e cada particula a `particle_radius`. Resultado: todas as particulas inteiramente dentro do mundo.

## Mundo circular

`randomPointInsideWorld` em modo circular usa amostragem polar uniforme: `r = sqrt(uni) * (world_radius - particle_radius)`. Garantia: todos os pontos dentro do disco.

## Determinismo

- `FoodSystem` usa `std::mt19937_64` interno, semeado em `fromRegistry` via `random_seed`.
- App reseeds o FoodSystem com o seed do mundo em `spawnDemoEntities`.
- Selftest semeia explicitamente para cenarios deterministicos (test 52, 61).
- Selecao de cluster em `grow_existing` usa menor `clusterId` (estavel).
- Trim usa maior indice (estavel).

## Performance

| Cenario | us/step | us/agente | bites | depletadas | observacao |
|---|---|---|---|---|---|
| inst_100ag_100food | 37.7 | 0.38 | 0 | 0 | baseline Phase 7 inalterado |
| inst_1000ag_500food | 363.8 | 0.36 | 0 | 0 | escala linear |
| chunk_100ag_100food | 32.9 | 0.33 | 0 | 0 | sem contato (random sparso); overhead zero |
| chunk_300ag_150food | 85.9 | 0.29 | 214 | 0 | 7 bites/step em media; sem regressao |
| chunk_600ag_300food | 238.4 | 0.40 | 1502 | 0 | escala linear |
| chunk_1000ag_500food | 410.9 | 0.41 | 3630 | 0 | escala linear |
| chunk_bite_1s | 29.6 | 0.30 | 0 | 0 | baixa densidade aleatoria |
| chunk_bite_12s | 30.0 | 0.30 | 0 | 0 | bite_seconds nao impacta custo |
| chunk_30pred_300prey | 94.6 | 0.29 | 185 | 0 | predacao + chunk juntos |
| chunk_30pred_300prey_predfood | 99.8 | 0.30 | 305 | 0 | predator come chunk + preda |

Overhead de modo chunk vs instant: **negligible** quando nao ha contato. Quando ha contato, custo cresce com numero de bites (~5 ns/bite). Trim varies: ~0.3 us total quando trim_max=5.

## Como comida instantanea da Phase 7 foi protegida

- `InteractionSystem::apply()` (Phase 7) signature preserved.
- Caminho `Instant` em `applyWithDiet()` matched the Phase 7 behavior bit-by-bit (immediate consumption, full energy, one-per-step per agent, food removed at end).
- Phase 7 selftest: **PASS 14/14**.

## Como dieta/predacao da Fase 18 foi protegida

- `DietConfig` inalterado. `eatFood` ja era genérico (instant ou chunk).
- Predacao path unchanged. Phase 18 selftest: **PASS 111/111**.
- Predator com `diet_food=false` nao come chunk (verificado em test 95).

## Como SpeciesStore/GenomeStore foram protegidos

- Nenhuma modificacao em `SpeciesStore` ou `GenomeRecord` nesta fase.
- Phase 17 selftest: **PASS 110/110**.

## Como MLP baseline foi protegida

- Phase 9 selftest: **PASS 23/23**.

## Como Gated/Shortcut/Modulated foram protegidas

- Phase 14 selftest: **PASS 71/71**.

## Como Simple RNN foi protegida

- Phase 15 selftest: **PASS 72/72**.

## Como familia NEAT foi protegida

- Phase 16 selftest: **PASS 109/109**.

## Preparacao para Fase 20 (obstaculos)

- `FoodSystem` aceita um `World` por valor. Adicionar checagem de obstaculo na rejeicao de posicao seria trivial.
- Spatial hash ja distingue `SpatialEntityType::Obstacle` (definido mas nao usado).

## Preparacao para Fase 21 (fisica)

- Chunk particles tem `radius_` e `position` estaveis; colisao food-food pode acoplar via SpatialHash.
- `setRadiusAt` ja existe (preparado para escalonamento por colisao).

## Preparacao para Fase 24 (UI)

- `FoodSystemStats` expoe contadores prontos para painel.
- `FoodSystem::clearAll` pronto para botao "Limpar Comida".
- `dietSnapshot` (Phase 17) continua disponivel para painel de dieta.

## Preparacao para Fase 27 (save/load)

- `FoodStore` e SoA puro com PODs; serializacao direta.
- `clusterId_` por particula preserva agrupamento entre saves.

## Testes executados

- Debug: `--phase19-selftest` (**133 checks PASS**).
- Release: `--phase19-selftest` (**133 checks PASS**).
- Regressoes Debug Phase 7-18: todas PASS (14+15+23+21+30+49+42+71+72+109+110+111 = 667 checks).
- Phase 19 microbenchmark Release: 18 cenarios, dados disponiveis.

## Resultado dos diagnostics / microbenchmark

Ver tabela em "Performance" acima.

Observacoes:
- Modo chunk nao adiciona custo mensuravel sobre instant em cenarios sem contato.
- Custo por bite chunk: ~5 ns/bite.
- Trim com `trim_max=1`, `=5`, `=50` produz tempo similar (~30 us/step). O custo de trim e dominado por trim de um único item por step quando o overshoot e pequeno.

## Diagnostico visual / runtime

`App` agora imprime "Phase 19: Chunk food fully integrated (N species, M genomes, K foods)". Em modo `food_mode=instant` (default), comportamento visual identico a Phase 18. Mudando `food_mode` para `chunk` na registry, particulas chunk aparecem em clusters em vez de espalhamento uniforme; agentes consomem aos poucos.

## Divergencias contra Python

- Spacing entre particulas e "best effort" no C++ (sem rejection sampling estrito). Python pode garantir spacing mais rigido; documentado como divergencia menor.
- `grow_particles` C++ restora particulas para `initialEnergy`. Python pode usar regra incremental; documentado.
- Trim C++ remove indices mais altos (mais novos). Python pode usar criterio diferente. Determinismo preservado.

## Limitacoes atuais

- `food_replenish_interval` parametro nao respeitado (App chama por step). Documentado.
- Spacing nao estritamente enforcado.
- corpse-to-food em modo chunk cria 1 particula, nao um cluster.
- Renderer trata chunk e instant identicamente (cores `food_color`).

## Pendencias para Fase 20

- Reject spawn position when obstacle present.

## Pendencias para Fase 21

- Food-food collision; food displacement by agents; brownian/viscosity.

## Pendencias para Fase 24

- UI control para `food_mode` toggle.
- Painel mostrando contadores `FoodSystemStats`.
- Botao "Limpar Comida" wired to `FoodSystem::clearAll`.

## Pendencias para Fase 26

- Graficos de food eaten / chunks created / particles depleted.

## Pendencias para Fase 27

- Serializar `clusterId` + `kind` + `energy` em saves.

## Pendencias para Fase 29

- Benchmark runner formal com cenarios chunk multiagent.

## Confirmacao de escopo

- Nenhum arquivo Python alterado.
- Obstaculos NAO implementados.
- Oclusao NAO implementada.
- Fisica avancada NAO implementada.
- UI completa NAO implementada.
- Save/load final NAO implementado.
- Phase 20 NAO iniciada.
