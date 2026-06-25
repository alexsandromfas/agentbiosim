# Phase 10 Status: Sensores, Canais de Retina e Visao Single

## Scope Executado

Phase 10 implementou a fundacao de percepcao sensorial:

- `RetinaConfig` lido do `ParameterRegistry`, com prefixo de especie (`bacteria`) e validacao defensiva.
- `RetinaInputMode` enum (DistanceOnly, ColorDistance, ColorPlusDistance, ColorOnly) com normalizacao de strings/aliases Python.
- `RetinaChannel` enum (R, G, B, RD, GD, BD, D) com ordem canonica.
- Calculo de `input_size = retina_count * eye_count * num_active_channels`, equivalente a `retina_input_size()` do Python.
- `SceneQuery` (free function `queryVisibleCandidates`) com caminho via `SpatialHash` e fallback brute force, com filtragem por `see_food/see_agents/see_predators/see_all`.
- `PerceptionSystem` com algoritmo de visao `single` (mapeamento centro-objeto por raio, mais proximo por raio).
- Suporte a multiplos olhos (1 ou 2) usando `eye_angle_degrees` e `eye_separation_degrees`.
- Canais R/G/B/D preenchidos conforme modo de entrada.
- `PerceptionResult` em buffer flat (`agentCount * inputSize`) para evitar alocacao por agente.
- Integracao `PerceptionSystem` → `NeuralSystem` → `MovementSystem` no loop principal do `App`.
- MLP recriada com input_size real quando a configuracao muda (via `architectureSignature()`).
- Helpers de parametros extraidos para `config/ParameterHelpers.hpp` (resolve parcialmente Divida 1).
- Diagnostics e selftest do Phase 10 (21 testes).
- Microbenchmark com 4 cenarios x 4 escalas (D-only/RGBD, 1-eye/2-eyes, 100/300/600/1000 agentes).
- Comandos `--phase10-selftest`, `--phase10-benchmark`, `--phase10-diagnostics`.

## Fora de Escopo

Nao implementado nesta fase (preservado para fases futuras):

- Visao fullbody/raycast estrito (Fase 11).
- Visao sector/bins (Fase 12).
- Obstaculos e oclusao completa (Fase 20).
- Visual debug de raios (Fase 11/25).
- UI completa (Fases 22-25).
- Neural viewer (Fase 25).
- Reproducao (Fase 13).
- Predadores funcionais completos (Fase 18).
- Redes avancadas (Fases 14-16).
- RNN/NEAT (Fases 15-16).
- Save/load (Fase 27).
- Benchmark formal completo (Fase 28).

## Python Reference Files Consulted

Principais:
- `sim/sensors.py` - algoritmo de visao single, input_size, canais, FOV, multi-olho.
- `sim/entities.py` - atributos do Agent/Food disponiveis para sensores.
- `sim/controllers.py` - defaults de parametros de retina.
- `sim/engine.py` - ordem da simulacao (sense -> brain -> locomotion).
- `sim/spatial.py` - query_ball_filtered para spatial hash.

Adicionais consultados:
- `sim/brain.py` - input_size esperado pela MLP.
- `sim/actuators.py` - output_size do brain.
- `sim/fast_kernels.py` - confirmacao de que single mode usa mapeamento por centro (nao raycast).

## Documents Consulted

- `CODEX_MIGRATION_GUIDE.md`
- `MIGRATION_PHASES.md`
- `PLANNING_COVERAGE_AUDIT.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `CURRENT_MODULE_MAP.md`
- `BENCHMARK_PLAN.md`
- `MIGRATION_RISKS.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_8_LOCOMOTION_STATUS.md`
- `PHASE_9_MLP_STATUS.md`

## Files Created

- `src/config/ParameterHelpers.hpp` - helpers compartilhados (resolve Divida 1 para codigo novo).
- `src/perception/RetinaConfig.hpp` - configuracao de retina, enums.
- `src/perception/PerceptionResult.hpp` - struct de resultado, header minimo.
- `src/perception/SceneQuery.hpp` - declaracao de candidate query.
- `src/perception/SceneQuery.cpp` - implementacao (spatial + brute force).
- `src/perception/PerceptionSystem.hpp` - declaracao do sistema.
- `src/perception/PerceptionSystem.cpp` - algoritmo single + retinaConfigFromRegistry.
- `src/perception/Phase10Diagnostics.hpp` - declaracoes de validacao/benchmark.
- `src/perception/Phase10Diagnostics.cpp` - 21 testes + microbenchmark.

## Files Modified

- `CMakeLists.txt` - 4 novas fontes (.cpp) adicionadas.
- `src/app/App.hpp` - include de PerceptionSystem, novos membros `perceptionSystem_` e `lastPerceptionStats_`.
- `src/app/App.cpp` - perception roda antes de neural no `runSimulationStep`; titulo da janela mostra `vision`, `inputSize`, `real/synthetic`.
- `src/systems/NeuralSystem.hpp` - `temporaryInputSize` renomeado para `syntheticInputSize`; `fromRegistry` recebe `inputSize`; `produceMovementControls` aceita ponteiro opcional para `PerceptionResult`; `NeuralStats` ganha `usingPerception` e `inputSize`; forward-declaration de `perception::PerceptionResult`.
- `src/systems/NeuralSystem.cpp` - include `PerceptionResult.hpp`; `temporaryInputForAgent` renomeado; `produceMovementControls` usa perception se valido, fallback sintetico caso contrario.
- `src/main.cpp` - 3 novos flags CLI: `--phase10-selftest`, `--phase10-benchmark`, `--phase10-diagnostics`.
- `src/neural/Phase9Diagnostics.cpp` - atualiza chamadas a `NeuralSystem::fromRegistry` para 3-arg, renomeia `temporaryInputSize` -> `syntheticInputSize`.

## Parametros Usados Funcionalmente

Globais:
- `retina_vision_mode` - "single" implementado; outros modos validados como string.

Por especie (prefixo bacteria):
- `bacteria_vision_radius` - 120.0
- `bacteria_retina_count` - 18
- `bacteria_retina_fov_degrees` - 180.0 (clamped 1-360)
- `bacteria_eye_count` - 1 (validado 1-2)
- `bacteria_eye_angle_degrees` - 60.0
- `bacteria_eye_separation_degrees` - 45.0
- `bacteria_retina_see_food` - true
- `bacteria_retina_see_bacteria` - false
- `bacteria_retina_see_predators` - false
- `bacteria_retina_see_obstacles` - false (registrado mas obstaculos sao Fase 20)
- `bacteria_retina_see_all` - false
- `bacteria_retina_see_through_walls` - true (sem efeito sem obstaculos)
- `bacteria_retina_input_mode` - "distance_only"
- `bacteria_retina_channel_r` - false
- `bacteria_retina_channel_g` - false
- `bacteria_retina_channel_b` - false
- `bacteria_retina_channel_d` - true

## Parametros Pendentes

Reconhecidos mas nao usados em Phase 10:
- `retina_skip` - sera usado na Fase 11/12 para perception caching.
- `retina_bins_*` - especifico para Fase 12 (sector).
- `retina_high_scale_auto_sector` - Fase 12.
- `predator_*` - Fase 18 (predators funcionais).
- Parametros `use_grouped_vision_batches`, `use_persistent_perception_arrays` - Fase 30 (otimizacao).

## Itens cobertos de FEATURE_INVENTORY.md

- Sensores/Visao: visao single.
- Canais de Retina: R, G, B, D com 4 modos de entrada.
- Multi-olho: 1 e 2 olhos.
- FOV configuravel.
- See-food/see-agents/see-predators flags.
- Spatial hash filtering para candidatos.
- Brute force fallback.

## Itens cobertos de PARAMETER_INVENTORY.md

- Grupo "Visao por Bins/Setores": parcial (apenas leitura/normalizacao para Fase 12).
- Grupo "Bacterias/Organismo Base": completo para parametros de visao single.
- Globais de retina: `retina_vision_mode` e `retina_skip` reconhecidos.

## Itens de UI_INVENTORY.md Impactados

- Painel "Sistema de Visao" (Fase 23): parametros agora tem efeito real; UI futura pode altera-los.
- Painel "Agente Selecionado / Rede Neural" (Fase 25): input_size real disponivel para visualizacao.
- "Visualizador de Visao" (Fase 11/25): debug visual de raios sera implementado em Fase 11.

## Decisoes Tomadas

### RetinaConfig

Implementado como `struct` simples lido do `ParameterRegistry`. Sem objeto pesado por agente. A configuracao e calculada por step (cheap), permitindo mudancas em tempo real via UI futura.

Justificativa: data-oriented, sem alocacao por agente, facil de testar.

### SceneQuery

Implementado como funcao livre `queryVisibleCandidates` (nao classe). Faz dois caminhos: spatial hash quando disponivel, brute force como fallback. O resultado e um `vector<VisibleCandidate>` com cor ja normalizada [0,1].

Justificativa: o usuario pediu SceneQuery; funcoes livres dao reuso futuro (Fase 11/12) sem o overhead de classe.

### Visao Single

Implementacao do algoritmo "single" usa mapeamento por centro do objeto (NAO ray-circle intersection). Cada candidato gera UM unico raio (rayIdx = round((relAngle + halfFov) / (2*halfFov) * (retinaCount-1))). Por raio, apenas o objeto mais proximo e mantido.

Justificativa: este e exatamente o comportamento do batch `single` do Python (linhas 2154-2194 de sensors.py). Mais rapido que fullbody, equivalente ao default Python.

Divergencia: o caminho NAO-batch do Python sense() (linhas 976-1031) usa ray-circle intersection, mas o batch e default em populacoes grandes. Optei pelo batch para consistencia e performance.

### Canais R/G/B/D

Implementados com `_channel_value()` equivalente ao Python:
- R/G/B: valor de cor [0,1]
- RD/GD/BD: ativacao * valor de cor (cor modulada pela distancia)
- D: ativacao = (vision_radius - effDist) / vision_radius

Cor preservada como dedicada (canal R/G/B/D separados) — nao destrui informacao de cor por simplificacao de distancia.

### Input Size

Formula: `retinaCount * eyeCount * channelCount` onde channelCount depende do `inputMode`:
- DistanceOnly: 1 canal (D)
- ColorDistance: 1-3 canais (RD/GD/BD para cores ativas, fallback D se nenhuma)
- ColorPlusDistance: 1-4 canais (R/G/B para cores ativas + D, fallback D se nenhuma cor ativa)
- ColorOnly: 1-3 canais (R/G/B para cores ativas, fallback D se nenhuma)

Equivalente conceitual a `active_retina_channels()` + `retina_input_size()` do Python.

### Input_size Anterior vs Real

- Anterior (Phase 9): 4 (sintetico - energy/age/x/y)
- Real (Phase 10 defaults): 18 (18 retinas * 1 olho * 1 canal D)
- Maximo (RGBD + 2 eyes): 144 (18 retinas * 2 olhos * 4 canais)

Estrategia de transicao: a MLP detecta mudanca de input_size via `architectureSignature()` em `NeuralSystem::syncBrains` e recria os cerebros. Pesos antigos sao descartados (esperado, pois input semantico mudou completamente).

Consequencia para paridade: comportamento aprendido em Phase 9 nao se preserva. Isso e esperado e correto — input sintetico nao tem semantica biologica equivalente.

### Integracao MLP

PerceptionSystem produz resultado em buffer flat. NeuralSystem recebe ponteiro opcional. Se `perception != nullptr && active && size matches`, usa input real; caso contrario, fallback sintetico (Phase 9). MovementSystem nao mudou — recebe `MovementControl` independente da origem.

### Fallback Sintetico

Mantido como debug/teste. Acessivel via:
- input_size=0 passado para `NeuralSystem::fromRegistry` (usa `syntheticInputSize=4`)
- ou `perception == nullptr` em `produceMovementControls`

O title bar do executavel mostra `real` ou `synthetic` para identificar qual caminho esta ativo.

### SpatialHash vs Brute Force

SpatialHash usado por default (quando `use_spatial=true`). Brute force ativo quando spatial=nullptr ou desligado. Os testes usam brute force (sem spatial) para isolar a logica de visao.

## SceneQuery Detalhes

Caminho com spatial hash:
- `queryRadiusInto(eyeX, eyeY, vision_radius + agent_radius + maxSeenRadius)`
- Filtra por entityType (Food/Agent) e typeCode (LegacyBacteria=1, LegacyPredator=2)
- Skip do proprio agente via `ignoreAgentId` (entityType=Agent + id match)
- Lookup de cor do store (FoodStore.colorAt ou AgentStore.colorAt)

Caminho brute force:
- Itera FoodStore se seeFood/seeAll
- Itera AgentStore se seeAgents/seePredators/seeAll, filtrando por typeCode
- Mesmo lookup de cor

Filtros aplicados:
- `see_food`: AgentTypeCode irrelevante, entityType==Food
- `see_agents`: entityType==Agent, typeCode==LegacyBacteria ou Organism
- `see_predators`: entityType==Agent, typeCode==LegacyPredator
- `see_all`: tudo incluido

## Runtime Visual

Title da janela atualizado:
```
... | vision single 18in real | brains 150 mlp | ...
```

Mostra:
- modo de visao (`single`)
- input_size (18)
- caminho real ou synthetic
- count de brains e tipo

Console inicial:
```
AgentBioSimCpp Phase 10: retina perception with single vision initialized.
```

## Diagnostico de Percepcao

PerceptionStats expoe:
- `agentsProcessed` - quantos agentes tiveram input computado
- `totalCandidatesQueried` - soma de candidatos consultados
- `averageCandidatesPerAgent` - candidatos medios por agente
- `visionMode` - "single"
- `inputSize` - tamanho do input neural real
- `channelCount` - quantos canais ativos
- `retinaCount` - 18
- `eyeCount` - 1
- `usedSpatialHash` - true/false

## Testes Executados

Comando: `--phase10-selftest`

```
Phase10 validation: PASS (21 checks)
All Phase 10 validation checks passed. checks=21
```

Cobertura dos 21 testes:
1. input_size D-only = 18
2. input_size RGBD = 72
3. food ahead produz input nao-zero
4. food ahead ativa regiao central da retina
5. food atras e ignorado
6. food fora do FOV e ignorado
7. food fora do raio e ignorado
8. distancia normalizada coerente
9. canal R responde a food vermelho
10. canal G responde a food verde
11. canal B responde a food azul
12. canal desligado nao entra no vetor
13. see_food=false esconde food
14. eye_count=2 duplica input_size
15. retina_count=9 gera input_size=9
16. MLP recebe input_size real sem crash
17. MovementSystem recebe output sem crash
18. Phase 7 regression
19. Phase 8 regression
20. Phase 9 regression
21. RetinaConfig from registry usa defaults corretos

## Regressao

```
Phase7 validation: PASS (14 checks)
Phase8 validation: PASS (15 checks)
Phase9 validation: PASS (23 checks)
```

## Resultado dos Diagnostics/Microbenchmark

Comando: `--phase10-benchmark`

```
scenario,agents,retina_count,eye_count,channels,input_size,repeats,total_ms,avg_perception_us,avg_candidates,spatial
D_only_1eye,100,18,1,1,18,30,4.68,156,3.89,true
D_only_1eye,300,18,1,1,18,30,21.95,732,11.31,true
D_only_1eye,600,18,1,1,18,30,63.57,2119,22.33,true
D_only_1eye,1000,18,1,1,18,30,153.95,5132,37.39,true
RGBD_1eye,100,18,1,4,72,30,3.32,111,3.89,true
RGBD_1eye,300,18,1,4,72,30,21.11,704,11.31,true
RGBD_1eye,600,18,1,4,72,30,64.54,2151,22.33,true
RGBD_1eye,1000,18,1,4,72,30,154.53,5151,37.39,true
D_only_2eyes,100,18,2,1,36,30,3.90,130,3.89,true
D_only_2eyes,300,18,2,1,36,30,25.73,858,11.31,true
D_only_2eyes,600,18,2,1,36,30,81.53,2718,22.33,true
D_only_2eyes,1000,18,2,1,36,30,200.97,6699,37.39,true
RGBD_2eyes,100,18,2,4,144,30,4.26,142,3.89,true
RGBD_2eyes,300,18,2,4,144,30,26.88,896,11.31,true
RGBD_2eyes,600,18,2,4,144,30,84.54,2818,22.33,true
RGBD_2eyes,1000,18,2,4,144,30,208.32,6944,37.39,true
```

Observacoes:
- Custo escala quase linear com agentes e dobra com eyes (esperado).
- RGBD nao adiciona custo significativo sobre D-only (~5-10%) — o gargalo e iteracao de candidatos, nao calculo de canais.
- Spatial hash sempre usado nestes cenarios.
- Para 1000 agentes: ~5 ms por iteracao de perception (D-only/1-eye), 1.56 us por agente. Viavel para 30Hz.
- Para 1000 agentes com 2 eyes + RGBD: ~7 ms por iteracao = 7 us/agente. Ainda dentro do orcamento.

## Diagnostico Visual

App executavel:
- Janela abre normalmente.
- 150 organismos visiveis com 50 foods.
- Title bar mostra "vision single 18in real" confirmando perception ativa.
- Movimentacao ocorre via brains MLP alimentadas por percepcao real.

## Divergencias contra Python

| Item | Python | C++ | Justificativa |
|---|---|---|---|
| Algoritmo single | batch usa centro-mapping; sense() usa raycast | C++ usa centro-mapping sempre | Consistencia, performance, matches batch default |
| Ordem de canais | sorted by RETINA_CHANNEL_ORDER | sorted by enum value (mesma ordem) | Equivalente |
| Color normalization | [0,255]/255 | mesma | Identico |
| FOV body extension | `half_span = asin(r/d)` quando d>r | mesma | Identico |
| Vision wrapping | `angle_wrap` | mesma formula | Identico |
| retina_skip | suportado | NAO usado em Phase 10 | Diferido para Fase 11/12 |
| Cache de raios | `_RETINA_RAY_CACHE` | sem cache ainda | Diferido para Fase 30 (otimizacao) |

## Limitacoes Atuais

- Apenas modo single (Fase 11/12 adicionarao fullbody/sector).
- Apenas especie "bacteria" tem RetinaConfig — predators serao Fase 18.
- retina_skip nao consumido (deferred).
- Sem cache de directions de raios — recomputado por agente (sera otimizado em Fase 30).
- Obstaculos nao bloqueiam visao (Fase 20).
- Visualizacao de raios no SFML nao implementada (Fase 11/25).
- Alocacoes temporarias de `std::vector<double>` por agente no `produceMovementControls` (Divida 5 ainda nao resolvida).

## Pendencias para Fase 11

- Modo fullbody/raycast estrito (ray-circle intersection per ray).
- Visualizacao de raios para agente selecionado.
- Flag `see_through_walls` com efeito real (preparado para Fase 20).
- Benchmark single vs fullbody.

## Pendencias para Fase 12

- Modo sector/bins otimizado.
- `retina_bins_distance_subdivisions`, `retina_bins_distance_distribution`, `retina_bins_distance_falloff`, `retina_bins_projection`, `retina_bins_candidate_limit`.
- High-scale auto sector (`retina_high_scale_auto_sector`).
- Modos de aggregation (nearest, strongest, sum_saturating, weighted_average).

## Confirmacoes

- Nao implementou fullbody/raycast.
- Nao implementou sector/bins.
- Nao implementou obstaculos/oclusao completa.
- Nao implementou UI completa.
- Nao implementou neural viewer.
- Nao implementou reproducao.
- Nao implementou predadores funcionais.
- Nao implementou redes avancadas.
- Nao implementou RNN/NEAT funcional.
- Nao implementou save/load.
- Nao implementou benchmark formal completo.
- Nao avancou para Fase 11.
- Nenhum arquivo Python foi alterado.

## Dividas Tecnicas

Resolvidas parcialmente:
- Divida 1 (helpers duplicados): `ParameterHelpers.hpp` criado. Codigo novo (PerceptionSystem, SceneQuery) usa o header. Codigo legado (App, MovementSystem, NeuralSystem, BrainFactory) ainda tem copias locais — sera limpo antes da Fase 13.

Nao resolvidas:
- Divida 2-6: nao bloquearam Phase 10, mantidas como pendencias futuras.

## Final Audit

Data: 2026-05-28

Scope audit:
- Todas as alteracoes Phase 10 estao em `C_SFML_Teste_Legado/AgentBioSimCpp` ou `C_SFML_Teste_Legado/MIGRACAO_C++SFML`.
- Nenhum arquivo Python foi modificado.
- Build Debug: OK.
- Build Release: OK.
- Phase 10 selftest: PASS (21 checks).
- Phase 7 regression: PASS (14 checks).
- Phase 8 regression: PASS (15 checks).
- Phase 9 regression: PASS (23 checks).
- Visual smoke test: OK.

Decisao: pronto para auditoria da Phase 10.
