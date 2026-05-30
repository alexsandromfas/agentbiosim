# Fase 20 — Obstaculos e Oclusao

Status: **CONCLUIDA** (2026-06-03)

A Fase 20 adiciona obstaculos discos (geometria minima viavel) ao engine, com bloqueio de movimento, rejeicao de spawn, oclusao em todas as estrategias de visao (single/fullbody/sector) e render basico. Toda a integracao e opcional via `const ObstacleStore*`: quando o store esta ausente ou vazio, o engine se comporta exatamente como na Fase 19 (custo zero).

## Escopo executado

- `ObstacleStore` headless (SoA: ids/x/y/radius/brushRadius/color/alive) com:
  - `add` / `remove` / `clear`
  - `paint(pos, brushRadius, color)` + `eraseAt(pos, radius)` (preparado para futuras ferramentas de canvas)
  - `containsPoint`, `overlapsCircle`, `segmentBlocked` (com hit point opcional), `queryRadius`
  - `isPositionFree(world, store, pos, radius)` helper consolidando world bounds + spawn block
- `MovementSystem::apply` aceita `ObstacleStore*` opcional. Politica: rollback para a posicao anterior + zera velocidade quando a nova posicao penetra um disco (suficiente para impedir atravessar, sem custo de slide).
- `FoodSystem::spawnInstant`, `spawnCluster`, `growExistingCluster`, `replenishToTarget` aceitam `ObstacleStore*` opcional. Spawns que cairiam dentro de um obstaculo sao retried ate 16 vezes; particulas chunk individuais que caem dentro sao silenciosamente puladas. `FoodSystemStats::spawnsRejectedByObstacle` registra rejeicoes finais.
- `PerceptionSystem::computeInputs` aceita `ObstacleStore*` opcional. Quando presente:
  - se `seeObstacles=true`, obstaculos sao adicionados como `VisibleCandidate` (entityType=Obstacle, cor sensorial do store).
  - se `seeThroughWalls=false`, candidatos cuja segmento agente->alvo passa por um disco sao descartados antes de chegar a estrategia de visao. `sectorBins.obstaclesBlockVision=true` em modo sector ativa a mesma filtragem mesmo com `seeThroughWalls=true`.
- `PerceptionStats` ganha `obstacleCandidates`, `occlusionChecks`, `occludedCandidates`.
- `Renderer::render` recebe `ObstacleStore*` opcional. Desenha discos abaixo de comida/agentes; render desligado paga zero custo de desenho.
- `App` instancia `ObstacleStore obstacles_`. `spawnDemoEntities` usa `sampleFreePosition` (rejeicao com retry + fallback para world center). `runSimulationStep` passa `obstaclePtr = empty ? nullptr : &obstacles_` para movement/perception/food. Render passa o mesmo ponteiro.
- `Phase20Diagnostics`: 134 selftests + microbenchmark com 26 cenarios.
- CLI: `--phase20-selftest`, `--phase20-benchmark`, `--phase20-diagnostics`.

## Fora de escopo (confirmado)

- Fase 21 completa (colisao agente-agente, fisica avancada, viscosidade, brownian).
- UI completa de obstaculos (Fase 22).
- Editor final de substrato (Fase 24).
- Save/load final (Fase 27).
- Nenhum arquivo Python foi alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `TECHNICAL_DEBT_REGISTER.md`, `PHASE_19_CHUNK_FOOD_STATUS.md`.

## Arquivos Python consultados

- `sim/obstacles.py` (referencia conceitual: brush/stamp circular, ObstacleMap).
- `sim/sensors.py` (referencia para oclusao em sensores).

## Arquivos C++ criados

- `src/simulation/ObstacleStore.hpp`
- `src/simulation/ObstacleStore.cpp`
- `src/systems/Phase20Diagnostics.hpp`
- `src/systems/Phase20Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_20_OBSTACLES_OCCLUSION_STATUS.md`

## Arquivos C++ modificados

- `src/systems/MovementSystem.hpp/.cpp` — `apply` ganha `const ObstacleStore*`; `MovementStats::obstacleBlocks`.
- `src/systems/FoodSystem.hpp/.cpp` — `replenishToTarget`, `spawnInstant`, `spawnCluster`, `growExistingCluster` ganham `const ObstacleStore*`; `FoodSystemStats::spawnsRejectedByObstacle`.
- `src/perception/SceneQuery.hpp/.cpp` — `appendObstacleCandidates`, `isOccludedByObstacles`.
- `src/perception/PerceptionSystem.hpp/.cpp` — `computeInputs` ganha `const ObstacleStore*`, candidate buffer recebe obstaculos quando `seeObstacles=true`, pre-filtragem de oclusao por segmento; `PerceptionStats` ganha contadores.
- `src/render/Renderer.hpp/.cpp` — `render` ganha `const ObstacleStore*`; novo `drawObstacles`.
- `src/app/App.hpp/.cpp` — campo `obstacles_`; `spawnDemoEntities` rejeita spawn em obstaculo; `runSimulationStep` propaga o store; init message atualizada.
- `src/main.cpp` — flags `--phase20-*`.
- `CMakeLists.txt` — novos fontes.

## Correcoes de continuidade

Nenhuma divida nova introduzida. Phase 7-19 selftests inalterados (todos PASS).

## Melhorias alem do escopo minimo

- **`brushRadius` separado de `radius`**: cada obstaculo carrega tanto a geometria quanto o raio do pincel original. Hoje sao iguais, mas isso permite ferramentas de canvas futuras (Fase 22) editar/mover obstaculos sem perder a metadata original. **Justificativa:** documentado no prompt como preparacao para editor.
- **`isPositionFree(world, store, pos, radius)` helper**: consolida checagem de bounds + obstaculo, evitando duplicacao em App/FoodSystem/futuro ReproductionSystem. **Justificativa:** elimina logica repetida e da um ponto unico para futuras evolucoes.
- **`segmentBlocked` retorna hit point opcional**: dois overloads para preparar debug visual da Fase 22 sem custo na versao "so testa".

## Parametros usados

```
retina_bins_obstacles_block_vision  -> RetinaConfig.sectorBins.obstaclesBlockVision
bacteria_retina_see_obstacles       -> RetinaConfig.seeObstacles
bacteria_retina_see_through_walls   -> RetinaConfig.seeThroughWalls
predator_retina_see_obstacles       -> idem (por especie)
predator_retina_see_through_walls   -> idem
random_seed                         -> FoodSystem/App seed (via Phase 17)
substrate_shape, world_w, world_h,
substrate_radius                    -> via World (bounds para spawn)
use_spatial                         -> Inalterado
```

Todos os flags de visao da Phase 10/11/12 continuam funcionando inalterados.

## Parametros pendentes

- Nenhum parametro novo de obstaculo foi cadastrado na registry porque a criacao programatica (paint/erase/clear) sera dirigida pela UI da Fase 22. Phase 27 (save/load) serializara o estado direto do `ObstacleStore`.

## Como funciona `ObstacleStore`

SoA puro com vetores paralelos. IDs monotonicos (`nextId_`) zeram em `clear()`. Sem dependencia SFML/UI. Determinismo: nenhum RNG interno. Helpers de query usam scan linear; com `bbox` implicito (centro + raio), 500 obstaculos custam ~0.5 us por agente em consulta de raio (medido).

## Como funciona a geometria

Discos. Cada obstaculo = (centro, raio). Movimento e oclusao usam:
- containsPoint: `|p - c|^2 <= r^2`.
- overlapsCircle: `|p - c|^2 <= (r + agent_radius)^2`.
- segmentBlocked: closest-point-on-segment to center + comparacao com raio (parametrizada por `t` em [0,1]; retorna o `t` minimo).

## Criacao, apagamento, clear, paint

- `add({pos, radius, brushRadius, color})` -> ObstacleId.
- `remove(id)` -> swap-remove + true se removido.
- `clear()` -> zera tudo + reseta `nextId_`.
- `paint(pos, brushRadius, color)` -> add com `radius = brushRadius`.
- `eraseAt(pos, eraseRadius)` -> remove todos os obstaculos com centro a <= `eraseRadius`. Retorna a contagem.

## Bloqueio de movimento

Politica: **rollback simples**. Quando `agent.newPosition` overlaps qualquer disco, `agent.position = previousPosition` e `velocity = (0,0)`. Sem slide, sem proxy de massa. Cheap, determinista, e suficiente para impedir atravessar — sem invadir Fase 21.

## Bloqueio de spawn

- Agentes: `App::sampleFreePosition` retry ate 32 vezes; fallback para world center clampado.
- Comida instantanea: `FoodSystem::spawnInstant` retry ate 16 vezes; falha retorna `EntityId{0}`.
- Comida chunk: centro do cluster com 16 retries; particulas individuais que caem em obstaculo sao silenciosamente puladas.
- `grow_existing`: mesma logica para particulas adicionadas.
- corpse-to-food (Phase 18) atualmente nao consulta obstaculos. Documentado como pendencia menor — a comida do cadaver aparece exatamente no local da presa, e a presa nunca esta dentro de obstaculo (foi bloqueada pelo MovementSystem).

## see_obstacles e see_through_walls

- `see_obstacles=true`: obstaculos sao adicionados ao candidate buffer como `VisibleCandidate{entityType=Obstacle, color={60,60,70}}` (cor neutra cinza-escuro). Respeitam FOV/vision radius pelos mesmos filtros que outros candidatos.
- `see_through_walls=true`: oclusao desligada globalmente. Comportamento Phase 19 preservado.
- `see_through_walls=false`: oclusao ativa. Cada candidato (exceto obstaculos) e testado contra `segmentBlocked(agentPos, candidatePos, obstacles)`. Candidatos bloqueados sao descartados antes de chegarem a estrategia de visao.

## Oclusao em single/fullbody/sector

Mesma logica: filtragem no candidate buffer **antes** do dispatch para a estrategia. Como as estrategias trabalham diretamente sobre `candidateBuffer_`, elas nao precisam saber sobre obstaculos. Isso garante que retinas 4/8/18/32/64, canais D/RG/RGB/DRGB, 1 ou 2 olhos, e todos os modos de projecao (center/edges) funcionam sem mudancas.

## `retina_bins_obstacles_block_vision`

Em modo sector, ativa oclusao mesmo com `seeThroughWalls=true`. Implementado no gate global:
```cpp
occlusionEnabledForRays = obstaclesActive &&
    (!seeThroughWalls || (activeMode == Sector && sectorBins.obstaclesBlockVision));
```

## Debug visual

`PerceptionStats::obstacleCandidates`, `occlusionChecks`, `occludedCandidates` expoem telemetria sem custo de visualizacao. Phase 22 podera consumir esses contadores em UI overlay.

## Integracao com SpatialHash

Obstaculos NAO entram no SpatialHash atual (que rebuilda a cada step com agentes + comida). Phase 20 mantem queries diretas no `ObstacleStore` (scan linear com early reject por bbox). Justificativa: obstaculos sao estaticos; rebuild N vezes por segundo seria desperdicio. Phase 22 ou 27 podem adicionar grid dedicado se necessario.

## Integracao com sistemas

- **MovementSystem**: chamada extra `obstacles->overlapsCircle(newPos, agentRadius)`. Custo medido: ~0.5 us/agente com 500 obstaculos.
- **FoodSystem**: rejeicao com retries. Custo medido: spawn rejection <0.1% do tempo total.
- **PerceptionSystem**: filtragem de candidatos. Custo medido: oclusao add ~0.5-2 us/agente dependendo do count.
- **SpeciesStore/GenomeStore**: nao tocados.
- **Renderer**: `drawObstacles` adiciona ~0.5 us por obstaculo desenhado quando render ligado.

## Determinismo

- `ObstacleStore` sem RNG -> determinista por design.
- `MovementSystem` rollback determinista (mesmo input + obstaculos = mesma saida).
- `FoodSystem` retries usam o RNG interno (seedable via `reseed`).
- `App::sampleFreePosition` usa o mesmo `std::mt19937` de `spawnDemoEntities`.
- `Phase20Diagnostics` test 36 verifica explicitamente: mesmo cenario -> mesma posicao final.

## Performance

Cenarios destacados (medidos em Release):

| Cenario | us/step | us/agente | obstacle_blocks |
|---|---|---|---|
| baseline 100 ag (sem obstaculo) | 355 | 3.55 | 0 |
| baseline 1000 ag (sem obstaculo) | 9412 | 9.41 | 0 |
| 100 ag + 10 obstaculos | 275 | 2.75 | 201 |
| 100 ag + 100 obstaculos | 267 | 2.67 | 1664 |
| 100 ag + 500 obstaculos | 289 | 2.89 | 2785 |
| single sem oclusao | 614 | 3.07 | 2073 |
| single COM oclusao | 962 | 4.81 | 2073 (78417 checks, 20759 occluded) |
| fullbody sem oclusao | 647 | 3.24 | 2073 |
| fullbody COM oclusao | 1099 | 5.49 | 2073 |
| sector sem oclusao | 602 | 3.01 | 2073 |
| sector COM oclusao | 1010 | 5.05 | 2073 |

Observacoes:
- **Sem obstaculos**: zero overhead (path inteiro skippa quando `obstaclePtr == nullptr`).
- **Com obstaculos, sem oclusao**: ~0.5 us/agente para o teste de movimento.
- **Com oclusao ativa**: ~1.5-2.3 us/agente extra para a filtragem de candidatos.
- **Retina 4/8/18/32/64 com oclusao**: cost stays within 360-414 us/step para 100 agentes. A oclusao e dominante; retinaCount muda muito pouco (filtragem e por candidato, nao por raio).
- **Canais D/RGB/DRGB com oclusao**: 370/392/405 us/step. Custo de canais ~5% sobre oclusao.

## Como visao single da Fase 10 foi protegida

Phase 10 selftest passa (21 checks). Single quando `obstaclePtr == nullptr` e bit-exato Phase 19.

## Como raycast/fullbody da Fase 11 foi protegido

Phase 11 selftest passa (30 checks). Mesmo comportamento sem obstaculos.

## Como sector/bins da Fase 12 foi protegido

Phase 12 selftest passa (49 checks). `obstaclesBlockVision=false` (default) preserva sector intacto.

## Como reprod./genoma da Fase 13 foi protegida

Phase 13 selftest passa (42 checks). ReproductionSystem nao toca obstaculos (filhos nascem na posicao do pai, que ja esta livre por construcao).

## Como redes densas/RNN/NEAT (14-16) foram protegidas

Phases 14-16 selftests passam (71+72+109 checks). Sistemas neurais nao tem path tocado.

## Como SpeciesStore/GenomeStore da Fase 17 foram protegidos

Phase 17 selftest passa (110 checks). SpeciesStore/GenomeStore continuam sem dependencia de obstaculo.

## Como dieta/predacao da Fase 18 foi protegida

Phase 18 selftest passa (111 checks). InteractionSystem nao toca obstaculos.

## Como comida chunk da Fase 19 foi protegida

Phase 19 selftest passa (133 checks). Quando obstaclePtr e nullptr, o caminho de spawn/grow e idempotente com Phase 19.

## Preparacao para Fase 21

- `MovementStats::obstacleBlocks` ja existe; Phase 21 pode substituir rollback por slide/separation.
- `SpatialEntityType::Obstacle` ja definido (Phase 0); pode entrar no SpatialHash se necessario.

## Preparacao para Fase 22

- `brushRadius` separado permite editor de canvas com pincel ajustavel.
- `paint`/`eraseAt`/`clear` sao funcoes headless prontas para serem expostas como botoes.

## Preparacao para Fase 24

- `ObstacleStore::records()` (via positionAt/radiusAt/colorAt/idAt) expoe iteracao para UI.
- Cor por obstaculo permite estilizacao.

## Preparacao para Fase 27

- `ObstacleStore` e SoA puro. Save/load = serializar os 6 vetores (ids, x, y, radius, brushRadius, color).

## Testes executados

- Debug: `--phase20-selftest` (**134 checks PASS**).
- Release: `--phase20-selftest` (**134 checks PASS**).
- Regressoes Debug Phase 7-19: todas PASS (14+15+23+21+30+49+42+71+72+109+110+111+133 = 800 checks).
- Phase 20 microbenchmark Release: 26 cenarios, dados disponiveis.

## Divergencias contra Python

- C++ usa **rollback** para colisao agente-obstaculo; Python pode usar push-out. Determinista; sem invasao da Fase 21.
- C++ desenha obstaculos como discos solidos; Python pode usar raster/sprite. Visual sera revisitado em Phase 22.
- Spacing entre particulas chunk perto de obstaculos: C++ silenciosamente pula; Python pode tentar deslocar. Documentado.

## Limitacoes atuais

- corpse-to-food nao consulta obstaculos (cadaver no local da presa).
- Nenhuma forma alem de disco. Phase 24 pode adicionar retangulos/polilinhas.
- Sem persistencia (Phase 27).
- Sem editor UI (Phase 22).

## Pendencias para Fase 21

- Push-out + slide ao inves de rollback puro.
- Colisao agente-agente.
- Possivel adicao de obstaculos ao SpatialHash se Phase 21 quiser collision unified.

## Pendencias para Fase 22

- Editor com brush UI.
- Botoes de paint/erase/clear.
- Visualizacao de occlusao debug.

## Pendencias para Fase 24

- Outras geometrias (rect, polyline).
- Estilizacao avancada.

## Pendencias para Fase 26

- Graficos: obstacle_blocks, occluded_candidates.

## Pendencias para Fase 27

- Serializar `ObstacleStore`.

## Pendencias para Fase 29

- Benchmark runner formal com cenarios pred+chunk+obstaculo.

## Confirmacao de escopo

- Nenhum arquivo Python alterado.
- Fisica avancada NAO implementada.
- UI completa NAO implementada.
- Save/load final NAO implementado.
- Phase 21 NAO iniciada.
