# Fase 21 — Colisoes e Fisica Opcional

Status: **CONCLUIDA** (2026-05-30)

A Fase 21 introduz `CollisionSystem` headless cobrindo colisao agente-agente (separacao + elasticidade + transferencia de velocidade), viscosidade global, Brownian motion, comida chunk movel (drag, push, colisao food-food, adesao por cluster). Toda subfase e opt-in via flag; quando tudo esta desligado o `apply` faz early-return e custa essencialmente zero.

## Escopo executado

- `CollisionSystem` (`src/systems/CollisionSystem.{hpp,cpp}`) headless, sem SFML/UI.
- `CollisionConfig` carrega os 17 parametros de fisica do `ParameterRegistry`.
- `FoodStore` ganhou colunas `vx_`/`vy_` (default 0) + `velocityAt`/`setVelocityAt`/`setPositionAt`.
- App integra `CollisionSystem` no loop apos `MovementSystem` e antes do `EnergySystem`.
- `Phase21Diagnostics` com 150 selftests + microbenchmark de 16 cenarios.
- CLI: `--phase21-selftest`, `--phase21-benchmark`, `--phase21-diagnostics`.
- Mensagem inicial do App atualizada para Phase 21.

## Fora de escopo (confirmado)

- Fase 22 (UI base, Dear ImGui) NAO iniciada.
- Save/load final NAO implementado.
- Editor genetico, painel de parametros, benchmark runner formal NAO implementados.
- Threading/SIMD/solver iterativo NAO implementados.
- Rotacao fisica/torque NAO implementados.
- Nenhum arquivo Python alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `TECHNICAL_DEBT_REGISTER.md`, `PHASE_20_OBSTACLES_OCCLUSION_STATUS.md`.

## Arquivos Python consultados

- `sim/systems.py` (referencia conceitual para CollisionSystem).
- `sim/entities.py` (chunk velocity/mass).

## Arquivos C++ criados

- `src/systems/CollisionSystem.hpp`
- `src/systems/CollisionSystem.cpp`
- `src/systems/Phase21Diagnostics.hpp`
- `src/systems/Phase21Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`

## Arquivos C++ modificados

- `src/simulation/FoodStore.hpp/.cpp` — colunas `vx_`/`vy_`, getters/setters.
- `src/app/App.hpp` — campos `collisionSystem_` + `lastCollisionStats_`.
- `src/app/App.cpp` — chamada de `CollisionSystem` apos `MovementSystem`; init message Phase 21.
- `src/main.cpp` — flags `--phase21-*`.
- `CMakeLists.txt` — fontes novos.

## Correcoes de continuidade

Nenhuma divida nova. Phase 7-20 selftests permanecem PASS.

## Melhorias alem do escopo minimo

- **Decay exponencial** (`exp(-drag * dt)`) para viscosidade e drag de chunk em vez de subtracao linear. **Justificativa:** mais estavel para `dt` variavel e produz comportamento determinista quando combinado com smooth locomotion.
- **Pares deduplicados via set** (chave `min(id) << 32 | max(id)`) em agent-agent e food-food. **Justificativa:** SpatialHash retorna pares dos dois lados; sem dedup colisoes seriam processadas duas vezes (ineficiente + inconsistente).
- **`pushOutOfObstacles`** helper geometrico chamado apos qualquer movimento causado pelo CollisionSystem, garantindo invariante "agentes nunca dentro de obstaculo". **Justificativa:** mantem a garantia da Phase 20 intacta sob colisao.

## Parametros usados

```
agent_collision_enabled, agent_collision_elasticity_enabled,
agent_collision_restitution, agent_collision_velocity_transfer,
agent_collision_separation, agent_collision_max_impulse,
global_viscosity_enabled, global_viscosity_drag,
brownian_motion_enabled, brownian_motion_strength,
movable_chunk_food_enabled, chunk_food_collision_enabled,
chunk_food_adhesion_enabled, chunk_food_adhesion_strength,
chunk_food_mass_scale, chunk_food_drag, chunk_food_push_strength,
use_spatial, random_seed
```

## Parametros pendentes

Nenhum. Todos os 17 da especificacao estao consumidos.

## Como funciona `CollisionSystem`

- Owns `std::mt19937_64` seeded once via `random_seed` (lazy init).
- `fromRegistry` clamps restitution/transfer/separation para [0,1].
- `apply` faz early-return se nenhuma flag esta ativa.
- 8 subfases sequenciais (viscosity, brownian, chunk integration, agent-agent, agent-chunk, chunk-chunk, adhesion, world+obstacle clamp).
- Cada subfase tem seu proprio gate de flag — overhead linear apenas no que esta ligado.

## Ordem final do loop

```text
1. PerceptionSystem  (com SpatialHash atual)
2. NeuralSystem
3. MovementSystem  (com obstacle block)
4. CollisionSystem  <-- novo
5. EnergySystem
6. SpatialHash rebuild
7. InteractionSystem (food/predacao)
8. FoodSystem (replenish + trim)
9. ReproductionSystem
10. DeathSystem
11. SpatialHash rebuild (se houve eventos)
```

`CollisionSystem` corre antes de `EnergySystem` para que mudancas de velocidade nao alterem o custo metabolico do step corrente.

## Colisao agente-agente

- Para cada par sobreposto: separa proporcionalmente ao overlap (com `separation` factor e `maxImpulse` clamp).
- Se elasticidade ativa: aplica impulso normal `j = -(1+r)*rvn*0.5` (mass-equivalent) clampado por `maxImpulse`.
- Se transferencia ativa: aplica `rvt * 0.5 * transfer` tangencialmente.
- Pos-correcao: `clampToWorld` + `pushOutOfObstacles` para preservar invariantes da Phase 20.

## Separacao + impulso maximo

- `separation` (0-1): fracao do overlap resolvida por step (`overlap * sep` clampado por `maxImpulse * 0.5`).
- `maxImpulse`: limite absoluto na correcao de posicao e velocidade.
- Overlap extremo: clampado pelo `maxImpulse`, agente reposicionado e re-clampado.

## Elasticidade

- `restitution` clampado em [0, 1].
- Impulso normal: `j = -(1+restitution) * rvn * 0.5`.
- Sem energia adicionada — a velocidade nao excede `maxImpulse`.

## Transferencia de velocidade

- `velocityTransfer` clampado em [0, 1].
- Compartilha 50% da componente tangencial relativa, escalada por `transfer`.
- 0 = sem transferencia; 1 = transferencia maxima clampada.

## Viscosidade global

- Drag exponencial: `v *= exp(-drag * dt)`.
- Aplicado a velocidades de agentes (e chunks moveis via flag separada).
- Nao inverte direcao; nao gera NaN.

## Brownian motion

- `std::normal_distribution(0, strength)` semeado uma vez por `seed`.
- Deslocamento: `dp = sqrt(dt) * noise` em x e y.
- Determinista por seed (test 69 verifica).
- Velocidade recebe 10% do delta para amortecer comportamento.
- Chunks recebem 50% da intensidade quando flag movel ligada.

## Comida chunk movel

- `movable_chunk_food_enabled` ativa: integracao `pos += v * dt` por step.
- Drag exponencial via `chunk_food_drag`.
- Clamp a world bounds + pushout de obstaculos.
- Velocidade default 0 (preserva Phase 19 quando flag off).

## Massa efetiva de chunk

`mass = max(epsilon, chunk_food_mass_scale * radius^2)`. Escala com area: chunks maiores resistem mais a empurroes e separacao.

## Drag de chunk

`v *= exp(-chunk_food_drag * dt)` por step. Independente do drag global.

## Empurrao de comida por agentes

- Agente com velocidade `v` empurra chunk: `impulse = min(push_strength * |v| / mass, maxImpulse)`.
- Direcao: normal de contato (agente -> chunk).
- Chunks instantaneos NAO recebem (filtro `kindAt == Chunk`).

## Colisao agente-chunk

Detectada via SpatialHash queryRadius (`r * 2.5`); resolvida no passo 5 do apply (push, sem alterar posicao do agente).

## Colisao chunk-chunk

`chunk_food_collision_enabled` ativa: separacao posicional proporcional a massa relativa `mb/(ma+mb)` vs `ma/(ma+mb)`. Sem impulso de velocidade — apenas resolve sobreposicao geometrica.

## Adesao/cohesao

- Calcula centro de massa por cluster.
- Cada particula com `clusterId != 0` puxada em direcao ao centro com `pull = min(d, strength * dt * 30 * 0.05)`.
- Cap em `d` impede colapso total para o centro.
- Determinista (sem RNG).

## Obstaculos

`pushOutOfObstacles` chamado apos cada movimento de agente. Mantem invariante da Phase 20: agentes nunca terminam um step dentro de um disco obstaculo.

## Mundo

- `clampToWorld` chamado apos cada movimento. Rectangular: clamp em x/y; Circular: clamp polar.
- Brownian, chunk integration, agent-agent collision, chunk-chunk collision, adhesion, post-clamp final — todos respeitam.

## Determinismo

- RNG semeado uma vez via `seed` (default 20260530).
- Iteracao por indice estavel.
- Pares deduplicados via set (chave determinista por `min(id) << 32 | max(id)`).
- Test 46, 65, 69, 96 verificam reprodutibilidade.

## SpatialHash

- `useSpatial && hash != nullptr && !hash->empty()` -> usa queryRadiusInto para agent-agent, agent-chunk, chunk-chunk.
- Fallback brute-force O(N^2) para `use_spatial=false`.
- Dedup via set por chave de par.

## EnergySystem

Nao tocado. Continua sendo chamado apos CollisionSystem; mudancas de velocidade nao alteram custo metabolico (que depende de speed apenas via `v0`/`vmax`).

## InteractionSystem

Nao tocado. CollisionSystem nao consome comida; apenas empurra ou separa. Test 94 confirma que push nao impede consumo.

## ReproductionSystem

Nao tocado. Filhos podem nascer em colisao (raro); CollisionSystem do proximo step separa.

## DeathSystem

Nao tocado. CollisionSystem nao remove entidades.

## Renderer

Nao tocado. Le posicoes apos CollisionSystem -> render reflete fisica.

## Resultado dos testes

- Debug: `--phase21-selftest` **PASS 150/150**.
- Release: `--phase21-selftest` **PASS 150/150**.
- Regressoes Phase 7-20 Debug: **PASS** (14+15+23+21+30+49+42+71+72+109+110+111+133+134 = 934 checks).

## Resultado dos diagnostics

Microbenchmark de 16 cenarios. Destaques:

| Cenario | Agentes | Foods | us/step | us/agente | Coll resolved | Pushes |
|---|---|---|---|---|---|---|
| off_100ag_100food | 100 | 100 | 30.6 | 0.31 | 0 | 0 |
| off_1000ag_500food | 1000 | 500 | 248.3 | 0.25 | 0 | 0 |
| aa_300ag | 300 | 150 | 136.2 | 0.45 | 85 | 0 |
| aa_elasticity_300ag | 300 | 150 | 129.9 | 0.43 | 85 | 0 |
| aa_transfer_300ag | 300 | 150 | 122.6 | 0.41 | 85 | 0 |
| viscosity_300ag | 300 | 150 | 80.7 | 0.27 | 0 | 0 |
| brownian_300ag | 300 | 150 | 85.7 | 0.29 | 0 | 0 (9000 brownian) |
| chunk_movable_300ag | 300 | 150 | 115.1 | 0.38 | 0 | 1081 |
| chunk_ff_300ag | 300 | 150 | 133.9 | 0.45 | 0 | 1083 + 181 ff coll |
| chunk_adhesion_300ag | 300 | 150 | 116.9 | 0.39 | 0 | 1088 + 4500 adhesions |
| aa_30obs_300ag | 300 | 150 | 128.3 | 0.43 | 103 | 0 |

Observacoes:
- **Tudo off**: ~0.25 us/agente (custo basico ja existente).
- **Agent-agent on**: +0.10-0.20 us/agente (depende de overlap density).
- **Elasticidade/transferencia**: custo similar a colisao basica (apenas computa impulso adicional).
- **Viscosidade**: ~0.04 us/agente.
- **Brownian**: ~0.06 us/agente.
- **Chunk movel**: ~0.13 us/agente.
- **Chunk-chunk colisao**: +0.07 us/agente.
- **Adesao**: ~0.13 us/agente.
- **Obstaculos + fisica**: sem regressao mensuravel.

## Divergencias contra Python

- C++ usa decay exponencial (`exp(-drag * dt)`) vs Python que pode usar subtracao linear. C++ e mais estavel para dt variavel.
- C++ adesao calcula centro de massa por cluster a cada step; Python pode usar pares locais. Documentado como aproximacao.
- C++ chunk push depende de `speed / mass`; Python pode usar regra diferente. Determinismo preservado.

## Limitacoes atuais

- Sem rotacao fisica / torque (intencionalmente fora do escopo).
- Adesao usa centro de massa simples (poderia ser pairwise springs).
- Sem solver iterativo (1 pass de separation por step).
- `chunk_food_collision_enabled` resolve apenas overlap; nao aplica impulso.

## Pendencias

- **Fase 22 (UI)**: expor toggles de fisica em painel; visualizar pares colidindo.
- **Fase 23 (preferencias)**: persistir defaults de fisica.
- **Fase 24 (editor)**: pintar regioes de viscosidade variavel?
- **Fase 26 (metricas)**: graficos de eventos de colisao.
- **Fase 27 (save/load)**: serializar `vx`/`vy` de chunks movel.
- **Fase 28 (benchmark runner)**: cenarios fisica formais.
- **Fase 30 (otimizacao)**: SIMD em loops de separation; passes paralelos.

## Confirmacoes

- Nenhum arquivo Python alterado.
- UI base NAO implementada.
- Save/load final NAO implementado.
- Benchmark runner formal NAO implementado.
- Phase 22 NAO iniciada.
