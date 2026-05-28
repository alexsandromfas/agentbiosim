# Phase 11 Status: Visao Fullbody/Raycast e Debug Visual de Visao

## Scope Executado

Phase 11 implementou a estrategia `fullbody/raycast` e a base de debug visual:

- `VisionStrategy.hpp` com enum `VisionMode {Single, Fullbody, Sector}` e helpers `normalizeVisionMode()`, `visionModeName()`, `isVisionModeImplementedInPhase11()`.
- Aliases reconhecidos: `fullbody`, `raycast`, `full_body`, `fullbody_raycast`, `ray_cast`, `geometric`.
- `VisionDebug.hpp` com `VisionRayDebug` (origem, direcao, hit/miss, distancia, ativacao, cor) e `VisionDebugData` (rays, agentId, mode, retina/eye counts, visionRadius, fovDegrees).
- `PerceptionSystem` modificado para dispatch por `VisionMode` (single ou fullbody), com fallback documentado para modos nao implementados (ex.: sector).
- Algoritmo `fullbodyVisionForAgent` em `PerceptionSystem.cpp`: interseccao raio-circulo geometrica por raio, com:
  - cache de cossenos/senos dos raios relativos por (retinaCount, halfFovRad);
  - rotacao 2D por agente (1 cos/sin de gaze por olho, depois rotacao matricial dos raios cache);
  - loop externo = candidatos, interno = raios (com pre-filtro angular para reduzir raios testados);
  - pre-filtragem por distancia minima possivel (sqrt(centerDistSq) - radius);
  - pre-rejeicao por FOV + half-span do objeto;
  - calculo do range de raios que podem interceptar via [relAngle - halfSpan, relAngle + halfSpan];
  - intersecao raio-circulo inlined com early-out em discriminante negativo.
- `PerceptionDebugRequest {agentId, VisionDebugData* out}` como parametro opcional de `computeInputs`. Debug data so e gerado quando solicitado e zerado quando nao.
- `App` ganha tecla `V` para toggle de debug, holds `VisionDebugData visionDebug_`, passa request para PerceptionSystem quando ativo.
- `Renderer::drawVisionDebug()` desenha raios em SFML como linhas (hit: cor do objeto, alpha proporcional a ativacao; miss: cinza translucido).
- Titulo da janela acrescenta `[debug]` quando o overlay esta ativo.
- 30 selftests cobrindo single e fullbody, multi-olho, canais, FOV, raio, distancia, debug on/off, integracao MLP/Movement e regressoes 7/8/9/10.
- Microbenchmark com 7 cenarios x escalas 100/300/600/1000 e retina_count 4/8/18/32/64 — total ~60 linhas de dados.
- Comandos CLI: `--phase11-selftest`, `--phase11-benchmark`, `--phase11-diagnostics`.

## Fora de Escopo

Nao implementado (preservado para fases futuras):

- Visao `sector/bins` (Fase 12).
- `retina_bins_mode/distance_subdivisions/projection`.
- High-scale auto sector.
- Oclusao final por obstaculos (Fase 20).
- Sistema completo de obstaculos.
- UI completa (Fases 22-25).
- Neural viewer completo (Fase 25).
- Reproducao (Fase 13).
- Mutacao/genoma (Fase 13/14-16).
- Predadores funcionais completos (Fase 18).
- Dieta generica (Fase 18).
- Comida chunk completa (Fase 19).
- Redes avancadas (Fases 14-16).
- RNN/NEAT (Fases 15-16).
- Save/load (Fase 27).
- Benchmark runner formal completo (Fase 28).

## Correcoes de Continuidade da Fase 10

Nenhuma correcao critica foi necessaria. A Fase 10 estava funcional, com `PerceptionSystem` → `NeuralSystem` → `MovementSystem` ligados corretamente, input sensorial real ativo por default, fallback sintetico claramente identificado, comandos `--phase10-selftest` e `--phase10-diagnostics` operacionais.

Pequenas mudancas nao-quebradoras:
- `PerceptionSystem::computeInputs` ganhou um parametro opcional `PerceptionDebugRequest` (default `{}`), 100% backward-compatible com Phase 10.
- `PerceptionStats` ganhou `visionModeEnum`, `totalRayHits`, `fallbackMode`, `fallbackReason`. Campos antigos preservados.
- `Renderer::render` ganhou parametro opcional `const VisionDebugData* visionDebug = nullptr`, default mantem comportamento Phase 10.

## Melhorias Alem do Escopo Minimo

1. **Cache de raios relativos**: `PerceptionSystem::RayCache` mantem cos/sin dos angulos relativos por configuracao (retinaCount, halfFovRad). Recriado apenas quando config muda. Por agente, faz-se uma rotacao 2D (1 cos/sin de gaze) e aplica matriz aos raios cached. Reduz custo de fullbody substancialmente.
   - Justificativa: visao era o gargalo Python (60% wall) — Phase 11 precisa nascer otimizada.

2. **Loop externo = candidatos, interno = raios**: invertido em relacao ao approach naive ray-outer. Aproveita cache de candidato em registradores e permite pruning agressivo do range de raios via halfSpan angular.
   - Justificativa: para objetos pequenos/medios (caso comum), o range de raios efetivos e tipicamente 1-3 em vez de retinaCount inteiro. Reducao tipica de 6x-18x em ray-circle tests.

3. **Pre-filtragem em multiplos niveis**:
   - Nivel 1: spatial hash filtra candidatos por raio.
   - Nivel 2: skip se `centerDist - candidate.radius > visionRadius` (squared dist pra evitar sqrt no test).
   - Nivel 3: skip se `|relAngle| > halfFov + halfSpan` (FOV + body extension).
   - Nivel 4: para cada candidato sobrevivente, ray range pruning via halfSpan.

4. **Fallback documentado para modos nao implementados**: `sector` cai para `single` com `PerceptionStats::fallbackMode=true` e mensagem em `fallbackReason`. Visivel via selftest. Evita comportamento silencioso.

5. **Debug overhead minimo**: VisionDebugData so e populado para o agentId solicitado. Para 1000 agentes com debug ativo em 1 agente: overhead de ~6% no perception step (377us extra em 5994us).

6. **Tecla V para toggle de debug em runtime**: alem de comandos CLI. Usuario pode ligar/desligar visualizacao durante simulacao em execucao.

## Justificativa de Cada Melhoria

| Melhoria | Por que necessaria |
|---|---|
| Cache de raios | Sem cache, cada agente recomputa `cos/sin` de retinaCount angulos por step. Para 1000 agentes x 18 raios x 2 olhos = 36000 trig ops/step. Cache reduz para 1 cos/sin de gaze por agente/olho + tabela compartilhada. |
| Candidato-outer + ray pruning | Object angular extent < ray spacing -> 1 ray tested em vez de 18. Realista em campo aberto. |
| Pre-filtragem por distancia squared | Evita sqrt no caminho de rejeicao rapida (early-out comum). |
| Fallback documentado para sector | Selftest exige comportamento explicito; evita fallback silencioso que confundiria depuracao. |
| Debug por agentId | Custo zero para 999 agentes nao-debug; foco em 1 selecionado. |
| Tecla V | Permite verificar visualmente sem rebuildar/rerodar. |

## Documentos Consultados

- `CODEX_MIGRATION_GUIDE.md`
- `MIGRATION_PHASES.md`
- `PLANNING_COVERAGE_AUDIT.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `ARCHITECTURE_REVIEW.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `UI_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`
- `BENCHMARK_PLAN.md`
- `MIGRATION_RISKS.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_9_MLP_STATUS.md`
- `PHASE_10_PERCEPTION_SINGLE_STATUS.md`

## Arquivos Python Consultados

- `sim/sensors.py` (focal: linhas 236-274 `ray_circle_intersect`, 277-297 `_raycast_hit_from_candidates`, 855-1031 `RetinaSensor.sense`, 2195-2263 batch `fullbody`).
- `sim/render.py` (consultado superficialmente — Phase 11 nao implementa neural viewer ou inspector, apenas debug minimo).
- `sim/entities.py` (confirmacao de atributos color/r/x/y).
- `sim/spatial.py` (confirmacao do contrato de queryRadius).
- `sim/fast_kernels.py` (referencia: padrao numba batch — nao migrado, apenas inspirou layout de loops e angular pre-filter).

Arquivos Python adicionais consultados: nenhum alem do listado acima.

## Arquivos C++ Criados

- `src/perception/VisionStrategy.hpp` (enum + dispatch helpers, header-only).
- `src/perception/VisionDebug.hpp` (estruturas de debug, header-only).
- `src/perception/Phase11Diagnostics.hpp` (declaracoes).
- `src/perception/Phase11Diagnostics.cpp` (30 selftests + microbenchmark).

## Arquivos C++ Modificados

- `CMakeLists.txt` — adiciona Phase11Diagnostics.cpp.
- `src/main.cpp` — flags `--phase11-selftest`, `--phase11-benchmark`, `--phase11-diagnostics`.
- `src/perception/PerceptionSystem.hpp` — adiciona `PerceptionDebugRequest`, `PerceptionStats` extendida, `RayCache` privado, parametro opcional debug em `computeInputs`.
- `src/perception/PerceptionSystem.cpp` — adiciona `fullbodyVisionForAgent`, `ensureRayCache`, dispatch por VisionMode, fallback documentado, integracao de debug data, optimizacoes (loops invertidos, ray range pruning).
- `src/app/App.hpp` — adiciona `bool visionDebugEnabled_`, `VisionDebugData visionDebug_`.
- `src/app/App.cpp` — tecla `V` toggle, request debug para primeiro agente, passa debug ao renderer, atualiza titulo.
- `src/render/Renderer.hpp` — adiciona parametro opcional `VisionDebugData*` em `render`; adiciona `drawVisionDebug`; `RenderStats` ganha `visionRaysDrawn`.
- `src/render/Renderer.cpp` — implementa `drawVisionDebug` (sf::VertexArray de linhas).

## Parametros Usados

Globais:
- `retina_vision_mode` — agora dispatchavel para `single` ou `fullbody`. Aliases reconhecidos.

Por especie (prefixo bacteria — mesmo set de Phase 10):
- `bacteria_vision_radius`, `bacteria_retina_count`, `bacteria_retina_fov_degrees`
- `bacteria_eye_count`, `bacteria_eye_angle_degrees`, `bacteria_eye_separation_degrees`
- `bacteria_retina_see_food/bacteria/predators/obstacles/all/through_walls`
- `bacteria_retina_input_mode`, `bacteria_retina_channel_r/g/b/d`

## Parametros Pendentes

- `retina_skip` — reconhecido mas nao consumido (Fase 12 podera usar para cache de percepcao entre frames).
- `retina_bins_*` — completos para Fase 12.
- `retina_high_scale_auto_sector` — Fase 12.
- `bacteria_retina_see_obstacles` — reconhecido; sem efeito ate Fase 20.
- `bacteria_retina_see_through_walls` — reconhecido; sem efeito ate Fase 20.
- `predator_*` — Fase 18.
- `use_grouped_vision_batches`, `use_persistent_perception_arrays` — Fase 30.

## Itens cobertos de FEATURE_INVENTORY.md

- Visao fullbody/raycast: implementada.
- Estrategia de visao selecionavel por parametro: `single` vs `fullbody`.
- Interseccao raio-circulo: implementada.
- Debug visual de visao (base): linhas de raios renderizadas com cor de hit/miss.
- Multi-olho: 1 e 2 olhos preservados.
- Canais R/G/B/D: equivalentes em fullbody.

## Itens cobertos de PARAMETER_INVENTORY.md

- `retina_vision_mode`: agora funcional para `single` e `fullbody`.
- Demais parametros de bacteria_*: preservados de Phase 10.

## Itens de UI_INVENTORY.md Impactados ou Preservados

- "Sistema de Visao": parametros tem efeito, mas UI completa fica em Fase 23.
- "Visualizador de Visao": versao minima implementada (linhas SFML); UI completa (overlays selecionaveis, multi-agent) fica em Fase 25.
- "Painel Agente Selecionado": preparado conceitualmente (debug por agentId); painel completo em Fase 25.

## Decisoes

### Estrategia `single`

Mantida como em Phase 10 — algoritmo centroid mapping. Selecionada quando `retina_vision_mode == "single"` ou modo nao reconhecido (default seguro). Algoritmo nao foi alterado, apenas movido para coexistir com fullbody no mesmo arquivo.

### Estrategia `fullbody/raycast`

Algoritmo geometrico via ray-circle intersection. Cada raio (de cada olho) testa interseccao contra os corpos circulares dos candidatos. O menor `t` positivo por raio define o hit. Distancia normalizada = `(visionRadius - t) / visionRadius`.

Diferencas chave vs single:
- Objeto grande/perto ativa multiplos raios (single ativa 1).
- Objeto pequeno/distante pode ativar 0 raios se angular extent < ray spacing (single sempre ativa 1 raio mais proximo do centroide).
- Hit distance e a distancia geometrica ate a borda do objeto pelo raio especifico, nao `centerDist - radius`.

### VisionStrategy

Implementado como **enum + dispatch via switch**, nao como interface virtual. Motivos:
- Performance: switch e mais previsivel que vtable em hot loop.
- Selecao e por step/config, nao por chamada.
- Adicionar Sector na Fase 12 e trivial (novo case).
- Mantem header simples (sem allocations, sem polimorfismo).

`VisionMode` enum e a "VisionStrategy" em estilo data-oriented C++.

### Ray-circle intersection

Formula classica:
```
ox = ray_origin.x - circle.x
oy = ray_origin.y - circle.y
b = dirX * ox + dirY * oy
c = ox*ox + oy*oy - r*r
disc = b*b - c
if disc < 0: no hit
sqrt_d = sqrt(disc)
t1 = -b - sqrt_d (entry)
t2 = -b + sqrt_d (exit)
return menor t >= 0
```

Inlined no loop de fullbody. Caso `t1 >= 0`: hit normal. Caso `t1 < 0 <= t2`: origem dentro do circulo, retorna `t=0` (touching).

### Debug Visual

`VisionDebugData` populado apenas quando `PerceptionDebugRequest.out != nullptr` e `agentId` bate. Custo zero para todos os outros agentes. Tecla V no App toggle a flag.

Visualizacao: `sf::VertexArray` de linhas com cor do objeto atingido (alpha = ativacao + base 64). Misses: cinza translucido.

### Geracao de Debug Data

Layout: vetor de `VisionRayDebug` na ordem `[eye0_ray0, eye0_ray1, ..., eye0_rayN, eye1_ray0, ...]`. Cada entry contem origem, direcao, ativacao, hit info, cor.

Quando desligado (`debugRequest.out == nullptr` OR `agentId != requested`): zero overhead na percepcao normal — apenas check de ponteiro nulo no loop.

### SpatialHash vs Brute Force

Sem mudancas vs Phase 10. `SceneQuery::queryVisibleCandidates` usa SpatialHash quando disponivel, brute force como fallback. Fullbody beneficia tanto quanto single — todos os filtros de candidatos sao identicos.

### Multiolhos

Preservados de Phase 10. Para fullbody:
- 1 olho: gazeOffset=0, posOffset=0.
- 2 olhos: posOffset=±eye_separation/2, gazeOffset=±eye_angle/2.

A rotacao 2D do cache de raios e feita por olho (1 cos/sin de gaze por olho).

### Canais R/G/B/D

Mesma logica do Python e Phase 10. Equivalencia: fullbody usa os mesmos `writeChannels(output, channels, activation, r, g, b)`. Os canais sao independentes do algoritmo de visao — diferenca esta apenas no `activation` por raio e na `bestColor` selecionada.

### Input Size

`input_size = retinaCount * eyeCount * channelCount`. Identico para single e fullbody dado a mesma config. Confirmado por Test 16.

Para defaults bacteria: 18 * 1 * 1 = 18 (D-only).

### Integracao com MLP

Inalterada vs Phase 10. PerceptionSystem produz buffer flat. NeuralSystem recebe ponteiro opcional. Single ou fullbody, o input shape e identico — MLP nao sabe nem precisa saber qual estrategia produziu os valores.

## Testes Executados

Comando: `--phase11-selftest`

```
Phase11 validation: PASS (30 checks)
All Phase 11 validation checks passed. checks=30
```

Cobertura dos 30 testes:
1-3. Ray hit / no hit / behind ignored em fullbody.
4. Food ahead ativa retina central.
5. Object exactly behind ignored.
6. FOV excludes side object.
7. Vision radius excludes far object.
8. Large/near object activates MORE retinas in fullbody vs single (8 vs 1 tipicamente).
9. Medium/medium-far object activates few retinas (<=3).
10. Normalized distance coherent (close to (R - hit_dist) / R).
11. input_size com D-only = 18.
12-14. Canais R/G/B respondem a foods coloridos.
15. Canal desligado nao entra no input.
16. Single e fullbody tem mesmo input_size para mesma config.
17. 2 olhos dobram input_size.
18. Olho esquerdo detecta objeto a esquerda.
19. Olho direito detecta objeto a direita.
20. see_food=false esconde comida.
21. Debug NAO gerado quando nao solicitado.
22. Debug gerado quando solicitado, com >= 1 hit.
23. MLP recebe input fullbody sem crash.
24. MovementSystem aceita output sem crash.
25-28. Regressao 7/8/9/10.
29. normalizeVisionMode aliases.
30. Sector mode fallback documentado para single.

## Regressao

```
Phase7 validation: PASS (14 checks)
Phase8 validation: PASS (15 checks)
Phase9 validation: PASS (23 checks)
Phase10 validation: PASS (21 checks)
```

## Resultado dos Diagnostics/Microbenchmark

Comando: `--phase11-diagnostics` em Release.

Highlights (1000 agentes, retina_count=18, 1 olho, D-only):
- single:   5450 us / iter (1.50 fps a 30Hz orcamento)
- fullbody: 5994 us / iter (ratio 1.10x vs single)

Com retina_count=64:
- single:   6850 us
- fullbody: 6406 us (fullbody MAIS RAPIDO devido ao angular pre-filter)

Com retina_count=18 + RGBD:
- single:   5606 us
- fullbody: 7498 us (ratio 1.34x — gasto extra com mais cor)

Com 2 olhos (retina_count=18, D):
- single:   7754 us
- fullbody: 8279 us (ratio 1.07x)

Debug overhead (1000 agentes, retina_count=18, debug em 1 agente):
- fullbody sem debug: 5994 us
- fullbody com debug: 6371 us
- Overhead: ~377 us (6.3%)

Tabela completa: ver output bruto de `--phase11-diagnostics`. Cabecalho:
```
scenario,vision_mode,agents,retina_count,eye_count,channels,input_size,repeats,total_ms,avg_perception_us,avg_candidates,avg_hits,spatial,debug
```

## Comparacao Single vs Fullbody

| Metrica | Single | Fullbody | Observacao |
|---|---|---|---|
| Algoritmo | centroid mapping (1 raio/objeto) | ray-circle intersection (N raios/objeto) | Different geometry |
| Custo por candidato | O(1) | O(rays in span) | typical 1-3 |
| Custo total 1000 agentes (D 1-eye, 18 raios) | 5450 us | 5994 us | 10% mais caro |
| Quando fullbody e melhor | raramente para perf | retina_count alto | angular pre-filter compensa |
| Acuracia geometrica | aproximada | exata | fullbody = padrao geometrico |
| Hits por agente (cena tipica) | 10.4 | 7.4 | single ativa mais por mapeamento aproximado |
| Cor preservada | sim | sim | identico |
| Multi-olho | suportado | suportado | identico |

## Custo do Debug Visual/Data

- Sem debug: 0 us extra (verificacao de ponteiro nulo no inicio do loop).
- Com debug para 1 agente: ~377 us extra para 1000 agentes (6.3% no perception, sem contar render).
- Render: linhas SFML com VertexArray — barato. Sem benchmark formal de render por enquanto.

## Diagnostico Visual

Executavel boots:
```
AgentBioSimCpp Phase 11: fullbody/raycast vision and debug overlay initialized.
Controls: ..., V toggle vision debug.
```

Titulo da janela atualizado:
```
... | vision single 18in real [debug] | brains 150 mlp | ...
```

Ao pressionar V: visionDebugEnabled_ alterna; quando ativo, raios do primeiro agente sao desenhados em SFML (cor por hit, cinza translucido por miss). Quando desativado, debugData zerado e nenhum raio desenhado.

Modo selecionavel via parametro `retina_vision_mode` (default `single`). Para testar fullbody no app: editar default ou criar build com parametro custom.

## Divergencias contra Python

| Item | Python | C++ | Justificativa |
|---|---|---|---|
| Ray-circle intersection | mesma formula | mesma formula | identico |
| Cache de raios | `_RETINA_RAY_CACHE` global | `RayCache` membro de PerceptionSystem | scope local; mesmo conceito |
| Loop order | batch=outer ray (numpy broadcast); per-agent=outer ray | outer=candidate, inner=ray | C++ escalar beneficia mais do pre-filter angular |
| FOV body extension | `half_span = asin(r/d)` | mesma | identico |
| Debug visualization | matplotlib/pygame overlay | SFML VertexArray | equivalente conceitual |
| `retina_skip` | suportado | NAO consumido em Phase 11 | deferred |
| Numba kernel | usado em batch | nao migrado | Phase 30 podera adicionar batch nativo |
| Object inside vision_radius mas centerDist > radius | ativacao por t da intersecao | mesma | identico |

## Limitacoes Atuais

- `retina_skip` nao consumido (deferred para Fase 12 ou Fase 30).
- Cache de raios e por-PerceptionSystem; nao global. Trade-off: simples e correto. Para batch grouping na Fase 30, podera ser elevado.
- Apenas modo `single` e `fullbody` implementados. `sector` faz fallback documentado.
- Apenas especie `bacteria` tem PerceptionConfig — predators serao Fase 18.
- Debug visual mostra apenas o primeiro agente. Selecao real de agente fica para UI da Fase 22-25.
- Obstaculos: `see_obstacles` e `see_through_walls` reconhecidos mas sem efeito (Fase 20).
- Render de debug nao distingue olhos visualmente (ex: cor diferente por olho). Pode ser melhorado em Fase 25.
- Alocacoes temporarias de `std::vector<double>` por agente no `NeuralSystem` ainda existem (Divida 5 pendente).

## Pendencias para Fase 12

- Modo `sector/bins` otimizado.
- Parametros `retina_bins_mode`, `retina_bins_distance_subdivisions`, `retina_bins_distance_distribution`, `retina_bins_distance_falloff`, `retina_bins_projection`, `retina_bins_candidate_limit`.
- High-scale auto sector (`retina_high_scale_auto_sector`, `retina_high_scale_sector_min_agents`).
- Modos de aggregation: nearest, strongest, sum_saturating, weighted_average.
- Subdivisoes radiais de distancia.
- Benchmark sector vs raycast vs single.

## Pendencias para Fase 20 (Obstaculos/Oclusao)

- `ObstacleStore` ou `ObstacleMap`.
- Bloqueio de visao por obstaculos (`see_through_walls=false`).
- Obstaculos visiveis por sensores (`see_obstacles=true`).
- Filtro `bins_block_obstacles` para Fase 12 sector.
- Integracao com SceneQuery: candidatos de obstaculo.
- Render de obstaculos.

## Pendencias para UI Futura

- Selecao de agente via clique (Fase 22).
- Painel de inspector com toggle de debug visual (Fase 25).
- Visualizacao multi-agent de visao (Fase 25).
- Toggle de modo de visao via UI (Fase 23).
- Visualizacao por olho com cores distintas (Fase 25).

## Atualizacoes de Outros Documentos

Nao foram necessarias atualizacoes a `TECHNICAL_DEBT_REGISTER.md`, `MIGRATION_RISKS.md`, `BENCHMARK_PLAN.md`, `MIGRATION_PHASES.md` ou `PHASE_10_PERCEPTION_SINGLE_STATUS.md` nesta fase. Nenhuma divida nova foi descoberta, nenhum risco novo, nenhuma lacuna no plano original, nenhuma falha factual em Phase 10.

## Confirmacoes

- Nao implementou sector/bins.
- Nao implementou oclusao final por obstaculos.
- Nao implementou UI completa.
- Nao implementou neural viewer completo.
- Nao implementou Gated/Shortcut/Modulated/RNN/NEAT.
- Nao implementou reproducao.
- Nao implementou predadores funcionais.
- Nao implementou save/load.
- Nao avancou para Fase 12.
- Nenhum arquivo Python foi alterado.
- Build Debug: OK.
- Build Release: OK.
- Phase 11 selftest: PASS (30 checks).
- Regressoes 7/8/9/10: PASS.
- Visual smoke test: OK.

## Final Audit

Data: 2026-05-28

Scope audit:
- Todas as alteracoes Phase 11 dentro de `C_SFML_Teste_Legado/AgentBioSimCpp` ou `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- Nenhum arquivo Python modificado.

Decisao: pronto para auditoria/commit da Phase 11.
