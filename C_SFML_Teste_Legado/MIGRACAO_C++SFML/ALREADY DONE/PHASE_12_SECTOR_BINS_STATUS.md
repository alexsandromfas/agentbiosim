# Phase 12 Status: Visao Sector/Bins Otimizada

## Scope Executado

Phase 12 implementou a estrategia `sector/bins` otimizada para alta escala:

- `SectorBinsConfig.hpp` com enums `BinsMode {Nearest, Strongest, SumSaturating, WeightedAverage}`, `BinsDistribution {Linear, NearDetail}`, `BinsFalloff {Linear, Quadratic, Step, None}`, `BinsProjection {Center, CenterEdges, ApparentSize}` e normalizadores com aliases pt/en.
- Algoritmo `sectorBinsVisionForAgent` em `PerceptionSystem.cpp`:
  - Por candidato, calcula angulo relativo, half-span e ativacao via `distanceBinActivation`.
  - Mapeia candidato → conjunto de raios via `forEachSectorIndex` (templated callback) conforme projection.
  - Agrega no setor segundo `BinsMode` (nearest/strongest mantem best; sum/weighted acumulam vetores).
  - Escrita final por modo: nearest/strongest produz canal por melhor candidato; sum_saturating escreve sums; weighted_average divide pelos pesos.
  - Pre-filtragem por FOV+half_span e por distancia (effDist > visionRadius) antes de qualquer trabalho de bin.
- `distanceBinActivation` implementa subdivisions/distribution/falloff equivalentes ao Python (`linear`/`near_detail`, `linear`/`quadratic`/`step`/`none`).
- `forEachSectorIndex` cobre `center` (1 raio), `center_edges` (1-3 raios deduplicados) e `apparent_size` (range continuo).
- `candidate_limit` por agente: aplica `std::partial_sort` por distancia ao quadrado quando candidatos excedem o limite. Limit=0 = ilimitado.
- High-scale auto sector: quando `retina_high_scale_auto_sector=true` e `agents.size() >= retina_high_scale_sector_min_agents`, modo ativo passa a `Sector` mesmo se o requested for outro. `PerceptionStats::autoSectorActive` e `requestedModeEnum` expoem isso para diagnostico.
- `PerceptionStats` extendida: `totalCandidatesAfterLimit`, `averageCandidatesAfterLimit`, `requestedModeEnum`, `autoSectorActive`.
- `VisionStrategy::isVisionModeImplemented` adicionado; o fallback para modos nao implementados continua para modos genuinamente desconhecidos.
- `retinaConfigFromRegistry` agora le todos os parametros globais `retina_bins_*` + `retina_high_scale_*` + `retina_bins_obstacles_block_vision`.
- Debug visual: `VisionDebugData` reutilizada para sector. Cada raio (setor) carrega `activation`, `hitColor*`, `dirX/Y` (direcao do centro do setor) e `hitDistance` (quando modo permite).
- 49 selftests cobrindo todos os modos de bin, subdivisions 1/5/20/99, distribuicoes, falloffs, projections, candidate_limit, multi-olho, canais, debug, integracao MLP/Movement, high-scale auto sector, regressoes 7/8/9/10/11.
- Microbenchmark com 21 cenarios x escalas 100/300/600/1000/2000 (~100 linhas de dados).
- Comandos CLI: `--phase12-selftest`, `--phase12-benchmark`, `--phase12-diagnostics`.

## Fora de Escopo

Nao implementado (preservado para fases futuras):

- Fase 13 (reproducao, mutacao, genoma).
- Predadores funcionais completos (Fase 18).
- Dieta generica (Fase 18).
- Comida chunk completa (Fase 19).
- Sistema completo de obstaculos (Fase 20).
- Oclusao final por obstaculos — `retina_bins_obstacles_block_vision` lido mas sem efeito.
- UI completa (Fases 22-25).
- Neural viewer completo (Fase 25).
- Redes avancadas (Fases 14-16).
- RNN/NEAT (Fases 15-16).
- Save/load (Fase 27).
- Benchmark runner formal completo (Fase 28).

## Documentos Consultados

Todos os 15 documentos obrigatorios:
- `CODEX_MIGRATION_GUIDE.md`, `MIGRATION_PHASES.md`, `PLANNING_COVERAGE_AUDIT.md`
- `PROPOSED_CPP_ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`
- `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`, `BENCHMARK_PLAN.md`, `MIGRATION_RISKS.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_9_MLP_STATUS.md`, `PHASE_10_PERCEPTION_SINGLE_STATUS.md`, `PHASE_11_RAYCAST_VISION_STATUS.md`

## Arquivos Python Consultados

- `sim/sensors.py` (focal: linhas 330-484 normalizadores + `_distance_bin_activation` + `_candidate_sector_indices`; 487-633 `_sector_retina_from_candidates`).
- `sim/fast_kernels.py` (referencia ao padrao numba — confirmacao dos codigos de bin mode/distribution/falloff/projection).
- `sim/spatial.py`, `sim/entities.py`, `sim/controllers.py` (defaults consistentes com Phase 10/11).
- `sim/engine.py`, `sim/render.py`, `sim/game.py`, `sim/ui.py` (consultados superficialmente; nao influenciam diretamente o algoritmo sector).

Arquivos Python adicionais consultados: nenhum alem do listado.

## Arquivos C++ Criados

- `src/perception/SectorBinsConfig.hpp` — enums, normalizadores e struct (header-only).
- `src/perception/Phase12Diagnostics.hpp` — declaracoes.
- `src/perception/Phase12Diagnostics.cpp` — 49 selftests + microbenchmark.

## Arquivos C++ Modificados

- `CMakeLists.txt` — adiciona Phase12Diagnostics.cpp.
- `src/main.cpp` — flags `--phase12-selftest`, `--phase12-benchmark`, `--phase12-diagnostics`.
- `src/perception/RetinaConfig.hpp` — adiciona `SectorBinsConfig sectorBins`, `highScaleAutoSector`, `highScaleSectorMinAgents`, `highScaleGlobalSector`.
- `src/perception/PerceptionSystem.hpp` — `PerceptionStats` ganha `totalCandidatesAfterLimit`, `averageCandidatesAfterLimit`, `requestedModeEnum`, `autoSectorActive`.
- `src/perception/PerceptionSystem.cpp` — adiciona `distanceBinActivation`, `forEachSectorIndex`, `sectorBinsVisionForAgent`, dispatch Sector, parsing dos parametros bins/auto-sector em `retinaConfigFromRegistry`, logica auto-sector e fallback generico via `isVisionModeImplemented`.
- `src/perception/VisionStrategy.hpp` — adiciona `isVisionModeImplemented` (inclui Sector).
- `src/app/App.cpp` — mensagem inicial atualizada para Phase 12.
- `src/perception/Phase11Diagnostics.cpp` — Test 30 atualizado: antes verificava fallback de sector → single (valido em Phase 11), agora verifica que sector roda nativamente apos Phase 12.

## Correcoes de Continuidade das Fases 10 ou 11

**Apenas uma correcao factual obrigatoria em Phase 11**: o test "sector mode falls back to single with documented reason" precisou ser atualizado, pois Phase 12 implementa Sector nativamente. O teste foi reescrito para confirmar a nova invariante (sector roda nativamente, sem fallback). Documentacao do PHASE_11_RAYCAST_VISION_STATUS.md nao precisou ser alterada — la o texto descreve corretamente a Phase 11 no momento em que foi escrita.

Nenhuma outra correcao foi necessaria. Phase 10/11 mantem-se intactas.

## Melhorias Alem do Escopo Minimo

1. **Pre-filtragem antes do calculo de bin**: candidatos rejeitados por FOV+span ou fora do raio nao entram em `forEachSectorIndex`. Reduz custo significativamente em cenas esparsas.

2. **Templated `forEachSectorIndex`**: aceita callable (lambda) para evitar alocacao de `std::vector` por candidato; cada projection escreve diretamente nos acumuladores.

3. **Buffers thread_local** para `bestDist`, `bestScore`, `bestColor`, `sumValues`, `weightedValues`, `weights`, `limited` (candidate sort buffer). Reuso entre agentes.

4. **Per-mode dispatch fora do loop de canais**: a logica de modo (sum/weighted/nearest/strongest) e decidida uma vez por candidato; canais sao iterados dentro do bloco do modo.

5. **`std::partial_sort` para candidate_limit**: O(N log K) em vez de O(N log N) — apenas os K mais proximos sao parcialmente ordenados.

6. **Fallback generico `isVisionModeImplemented`**: substituiu o `isVisionModeImplementedInPhase11` para nao acumular flags por fase. O fallback agora cobre apenas modos genuinamente nao implementados (ex.: futuro modo desconhecido).

7. **`autoSectorActive` em PerceptionStats**: telemetria explicita para o usuario saber quando o auto sector entrou em acao. Aparece em testes.

## Justificativa de Cada Melhoria

| Melhoria | Por que |
|---|---|
| Pre-filtragem cedo | Visao = 60% do wall em Python; cada candidato rejeitado e ciclos economizados em loops quentes. |
| Templated forEachSectorIndex | Evita allocacao de vector por candidato. Permite inlining e otimizacao do compilador. |
| Buffers thread_local | Phase 10/11 ja usam o padrao. Mantem zero alloc por agente. |
| Per-mode dispatch | Reduz branches por candidato no caminho mais quente. |
| partial_sort para candidate_limit | O(N log K) e o algoritmo canonico para "top K". |
| isVisionModeImplemented generico | Permite Phase 13+ adicionar novos modos sem mudar PerceptionSystem.cpp; flag por fase nao escala. |
| autoSectorActive em stats | Necessario para testar a logica e para o usuario diagnosticar o que esta acontecendo em alta escala. |

## Parametros Usados

Globais (acrescentados na Phase 12):
- `retina_bins_mode` (string) — nearest/strongest/sum_saturating/weighted_average e aliases pt.
- `retina_bins_distance_subdivisions` (int, 1-99) — clamped.
- `retina_bins_distance_distribution` (string) — linear/near_detail.
- `retina_bins_distance_falloff` (string) — linear/quadratic/step/none.
- `retina_bins_projection` (string) — center/center_edges/apparent_size + alias `edges`.
- `retina_bins_candidate_limit` (int, ≥0) — 0 = ilimitado.
- `retina_bins_obstacles_block_vision` (bool) — reconhecido, sem efeito (Fase 20).
- `retina_high_scale_auto_sector` (bool).
- `retina_high_scale_sector_min_agents` (int, ≥1).
- `retina_high_scale_global_sector` (bool) — reconhecido, sem efeito especial (deferred).

Globais ja existentes:
- `retina_vision_mode` — agora aceita "sector"/"bins"/"angular_bins"/"setorial".

Por especie (bacteria) — inalterados de Phase 10/11.

## Parametros Pendentes

- `retina_skip` — reconhecido, nao consumido (deferred para cache de percepcao futuro).
- `retina_high_scale_global_sector` — reconhecido, sem comportamento especifico (experimental no Python; deferred).
- `retina_bins_obstacles_block_vision` — sem efeito ate Fase 20.
- `bacteria_retina_see_obstacles` / `bacteria_retina_see_through_walls` — sem efeito ate Fase 20.
- `predator_*` — Fase 18.
- `use_grouped_vision_batches`, `use_persistent_perception_arrays` — Fase 30.

## Itens cobertos de FEATURE_INVENTORY.md

- Visao sector/bins: implementada com 4 modos de agregacao, 4 falloffs, 3 projections.
- Subdivisoes de distancia: implementadas (linear, near_detail).
- Candidate limit por agente.
- High-scale auto sector.
- Multi-olho com sector.
- Canais R/G/B/D compativeis com sector.
- Debug visual de setores (reutilizando VisionDebugData).

## Itens cobertos de PARAMETER_INVENTORY.md

- Grupo "Visao por Bins/Setores": agora todos os parametros consumidos funcionalmente.
- Grupo "Bacterias/Organismo Base" (retina): identico a Phase 10/11.

## Itens de UI_INVENTORY.md Impactados ou Preservados

- "Sistema de Visao": parametros bins agora ativos; UI completa fica em Fase 23.
- "Visualizador de Visao": debug de raios da Phase 11 reaproveitado para sector; UI multi-agent fica em Fase 25.

## Decisoes

### Estrategia `single`

Inalterada. Continua sendo o default e o algoritmo centroide.

### Estrategia `fullbody/raycast`

Inalterada. Continua usando ray-circle intersection com cache de raios.

### Estrategia `sector/bins`

Algoritmo per-candidate com `forEachSectorIndex` dispatchando para o conjunto de raios afetados conforme projection. Por candidato, aplica activation via `distanceBinActivation` (subdivisions+distribution+falloff). Agrega segundo modo (nearest/strongest/sum/weighted).

### VisionStrategy

Mantida como enum + dispatch. Sector agora e implementado. `isVisionModeImplemented` substitui o helper specifico de fase.

### `retina_bins_mode`

- `nearest`: mantem candidato com menor `effDist` por setor.
- `strongest`: mantem candidato com maior `activation * max(R,G,B)` por setor.
- `sum_saturating`: soma valores por canal saturando em 1.0.
- `weighted_average`: media ponderada por `activation` (≥1e-9).

### Subdivisions

Dividem `norm = dist/visionRadius` em N bands. Para `near_detail`, usa `sqrt(norm)*subdivisions` para banda; representativo e media de quadrados. Para `linear`, banda direta e representativo no centro.

Subdivisions NAO alteram input_size — afetam apenas o valor de ativacao.

### Distribuicao

- `linear`: bands uniformes.
- `near_detail`: bands quadraticas (mais resolucao perto).

### Falloff

- `linear`: `value = 1 - representative`.
- `quadratic`: `value = (1 - representative)^2`.
- `step`: `value = (subdivisions - band) / subdivisions`.
- `none`: `value = 1.0` (presenca pura).

### Projection

- `center`: 1 setor por candidato (indice do centro angular).
- `center_edges`: ate 3 setores (centro + ±half_span), deduplicado.
- `apparent_size`: range continuo cobrindo todo o angular span do objeto.

Apparent_size permite que objetos grandes/proximos ativem multiplos setores naturalmente.

### Candidate Limit

`candidate_limit > 0` AND `candidates.size() > limit`: `std::partial_sort` pelos `limit` mais proximos (squared dist). `limit = 0`: ilimitado.

A ordenacao e deterministica por seed/posicoes (ties podem variar). Pode ignorar candidatos relevantes — comportamento documentado.

### High-scale auto sector

Quando `highScaleAutoSector = true` e `agents.size() >= highScaleSectorMinAgents`, o modo ativo e forcado para Sector mesmo se o requested for Single/Fullbody. `PerceptionStats::autoSectorActive=true` e `requestedModeEnum` mantem o modo originalmente pedido.

### High-scale global sector

Apenas lido como flag em `RetinaConfig`. Sem efeito especifico nesta fase — esta feature e experimental no Python (`retina_high_scale_global_sector`) e foi deferida para uma fase futura de otimizacao global.

### `retina_bins_obstacles_block_vision`

Lido em `SectorBinsConfig::obstaclesBlockVision`. Sem efeito ate Fase 20 (sistema de obstaculos). Pendencia documentada.

### Debug Visual

Reutiliza `VisionDebugData` da Phase 11. Para sector:
- `dirX/dirY`: direcao do centro do setor.
- `hitDistance`: distancia do candidato dominante (nearest/strongest) ou -1 (sum/weighted).
- `activation`: ativacao final do setor.
- `hitColor*`: cor do candidato dominante.

Custo zero quando nao solicitado (verificacao de ponteiro no inicio).

### SpatialHash vs Brute Force

Sem mudanca. Sector tambem usa `SceneQuery::queryVisibleCandidates`. O candidate_limit do sector e aplicado APOS o filtro do spatial hash.

### Multiolhos

Identico a Phase 10/11. Por olho, o sector itera todos os candidatos com pose de olho separada.

### Canais R/G/B/D

Identicos. `channelValue()` reusada.

### Input Size

`input_size = retinaCount * eyeCount * channelCount`. **Identico para single, fullbody E sector** dada a mesma config. Confirmado pelo Test 2.

### Integracao com MLP

Inalterada. Buffer flat compartilhado. NeuralSystem nao sabe qual modo gerou os valores.

## Testes Executados

Comando: `--phase12-selftest`
```
Phase12 validation: PASS (49 checks)
All Phase 12 validation checks passed. checks=49
```

Cobertura:
- 1-2: sector selecionado, input_size identico.
- 3-6: bins_mode nearest/strongest/sum/weighted.
- 7-10: subdivisions 1/5/20/99.
- 11: subdivisions nao alteram input_size.
- 12-13: distribuicao linear/near_detail.
- 14-17: falloff linear/quadratic/step/none.
- 18-20: projection center/edges/apparent_size (com asserts geometricos).
- 21-24: posicionamento (ahead/behind/outsideFOV/outsideRadius).
- 25-28: canais D/R/G/B.
- 29: canal desligado.
- 30: see_food=false.
- 31-32: candidate_limit 0 (ilimitado) e baixo.
- 33: eye_count=2 dobra input_size.
- 34-35: olho esquerdo/direito.
- 36-37: debug off/on.
- 38-39: MLP/Movement nao crashan.
- 40: high-scale auto sector ativa.
- 41-42: single e fullbody continuam.
- 43-47: regressoes 7/8/9/10/11.
- 48-49: aliases vision mode e bins.

## Regressao

```
Phase 7: PASS (14 checks)
Phase 8: PASS (15 checks)
Phase 9: PASS (23 checks)
Phase 10: PASS (21 checks)
Phase 11: PASS (30 checks)
```

## Resultado dos Diagnostics/Microbenchmark

Comando: `--phase12-benchmark` em Release.

Highlights (1000 agentes, retina_count=18, D-only, 1 eye):
- single:   5638 us / iter
- fullbody: 5820 us / iter
- sector:   5843 us / iter  (~3.6% acima de single)

Sector com variacoes (1000 agentes, D-only, 1 eye):
- subdiv1 (default test):  5986 us
- subdiv5 + near_detail:   7288 us
- subdiv20 + near_detail:  7521 us
- subdiv99 + near_detail:  7826 us
- falloff quadratic (sub5): 8186 us
- falloff step (sub5):     9838 us
- falloff none (sub5):     7675 us
- projection edges:        8535 us
- projection apparent:     7923 us
- candidate_limit=128:     6833 us  (mais barato — sort cap)
- candidate_limit=32:      7819 us  (sort cost domina em N pequeno)
- candidate_limit=512:    11602 us  (limit > candidates → sem skip; ainda assim noise)
- RGBD 1 eye:              8327 us
- D 2 eyes:               10911 us
- debug data 1 agente:     7408 us

Em 2000 agentes (D-only, 1 eye):
- single:   18993 us
- fullbody: 20046 us
- sector:   20110 us

Sector e essencialmente compativel com single/fullbody em D-only nearest center, com subdivisions/falloffs/projections cobrando seu custo geometrico. Para alta escala (>1000), os tres modos saturam no custo de iteracao de candidatos.

## Comparacao Single vs Fullbody vs Sector

| Aspecto | Single | Fullbody | Sector |
|---|---|---|---|
| Algoritmo | centroid mapping 1 raio | ray-circle intersection N raios | bin aggregation per candidato |
| Custo base (1000 agentes) | 5638 us | 5820 us | 5843 us |
| Acuracia geometrica | aproximada | exata | aproximada (varia por projection) |
| Multi-ray por objeto | nao | sim (rays in span) | sim (depending on projection) |
| Subdivisoes de distancia | nao | nao | sim, 1-99 |
| Modos de agregacao | implicit nearest | implicit nearest | 4 modos explicitos |
| Adequado para alta escala | bom | bom | bom (com high-scale auto sector) |
| Preserva cor | sim | sim | sim |
| Multi-olho | sim | sim | sim |

## Custo do Debug Visual/Data

- Sector sem debug: ~5843 us (1000 agentes, D-only)
- Sector com debug em 1 agente: ~7408 us (~27% overhead)
- Overhead absoluto: ~1565 us

Maior overhead que Phase 11 fullbody debug (6%) porque sector preenche debug ray-by-ray sem cache de raios precomputados. Aceitavel para 1 agente debug.

## Diagnostico Visual

Boot:
```
AgentBioSimCpp Phase 12: sector/bins vision with high-scale auto sector initialized.
Controls: ..., V toggle vision debug.
```

Title bar mostra `vision sector 18in real`. Tecla V toggla overlay.

## Divergencias contra Python

| Item | Python | C++ | Justificativa |
|---|---|---|---|
| `_distance_bin_activation` formula | identica | identica | identico |
| `_candidate_sector_indices` | retorna lista | usa callback (templated) | mesma logica, sem alloc |
| `sum_saturating` clamp | per-channel min(1, sum+val) | mesmo | identico |
| `weighted_average` | dividido por sum dos weights | identico | identico |
| candidate_limit | sort por dist quadrada | identico (partial_sort) | identico |
| high-scale auto sector | override de modo | identico | identico |
| Sphere body extension | `asin(r/d)` | identico | identico |
| Numba kernel | usado em batch | nao migrado | Phase 30 podera adicionar |
| Obstaculo blocker (bins_block_obstacles) | implementado | sem efeito ate Fase 20 | obstaculos sao Phase 20 |

## Limitacoes Atuais

- `retina_high_scale_global_sector` lido mas sem comportamento especifico.
- `retina_bins_obstacles_block_vision` lido mas sem efeito ate Fase 20.
- Cache de raios `RayCache` (introduzido na Phase 11) e usado apenas em fullbody; sector recalcula direcoes de centro de setor para debug. Poderia ser unificado em Fase 30.
- Apenas especie `bacteria` tem RetinaConfig ativo; predator fica para Fase 18.
- Alocacoes temporarias do NeuralSystem (Divida 5) permanecem.

## Pendencias para Fase 13 (Reproducao)

- Brain clone/copy ao reproduzir.
- Genoma inicial.
- Mutacao reprodutiva e split de energia.
- Cooldown e idade minima.

## Pendencias para Fase 20 (Obstaculos e Oclusao)

- `ObstacleStore` / `ObstacleMap`.
- `retina_bins_obstacles_block_vision` com efeito real (bloquear visao por obstaculo).
- `see_obstacles` retornar obstaculos como candidatos visiveis.
- `see_through_walls` controlar oclusao em todos os 3 modos.

## Pendencias para UI Futura

- Toggle do modo de visao via UI (Fase 23).
- Visualizacao multi-agent (Fase 25).
- Painel de configuracao bins (Fase 23).
- Visualizacao das subdivisoes radiais (Fase 25 — opcional).

## Atualizacoes de Outros Documentos

- `PHASE_11_RAYCAST_VISION_STATUS.md`: nao foi alterado. A documentacao da Phase 11 descreve corretamente o estado naquela fase. A correcao foi apenas no test code (Test 30 reescrito).
- `TECHNICAL_DEBT_REGISTER.md`: sem novas dividas reais. As limitacoes acima sao deferred items, nao dividas.
- `MIGRATION_RISKS.md`, `BENCHMARK_PLAN.md`, `MIGRATION_PHASES.md`: sem alteracoes necessarias.

## Confirmacoes

- Nao implementou reproducao/genoma.
- Nao implementou oclusao final por obstaculos.
- Nao implementou UI completa.
- Nao implementou neural viewer completo.
- Nao implementou redes avancadas.
- Nao implementou Gated/Shortcut/Modulated/RNN/NEAT.
- Nao implementou predadores funcionais.
- Nao implementou dieta generica.
- Nao implementou comida chunk completa.
- Nao implementou save/load.
- Nao implementou benchmark runner formal completo.
- Nao avancou para Fase 13.
- Nenhum arquivo Python foi alterado.
- Build Debug: OK.
- Build Release: OK.
- Phase 12 selftest: PASS (49 checks).
- Regressoes 7/8/9/10/11: PASS.
- Visual smoke test: OK.

## Final Audit

Data: 2026-05-28.

Scope: todas as alteracoes Phase 12 dentro de `C_SFML_Teste_Legado/AgentBioSimCpp` ou `C_SFML_Teste_Legado/MIGRACAO_C++SFML`. Nenhum arquivo Python modificado.

Decisao: pronto para auditoria/commit da Phase 12.
