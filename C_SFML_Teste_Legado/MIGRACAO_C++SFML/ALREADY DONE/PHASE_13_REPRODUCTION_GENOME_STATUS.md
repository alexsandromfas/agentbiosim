# Phase 13 Status: Reproducao, Mutacao Base e Genoma

## Scope Executado

Phase 13 implementou a fundacao de evolucao/reproducao:

- **Divida 1 (helpers duplicados): RESOLVIDA**. 7 arquivos `.cpp` (App, Movement, Neural, Energy, Interaction, Death, BrainFactory) substituem helpers locais por `config/ParameterHelpers.hpp` via `using config::parameterX;`.
- `GenomeStore` SoA inicial com `GenomeRecord`, `GenomeHandle`, `GenomeId` em `simulation/`. Suporta `createGenome`, `cloneFrom`, `find`, `get`.
- `GenomeRecord` carrega: id, parentId, generation, bodySize, bodyShape, color, mutationRate/Strength, splitEnergy, initialEnergy, energyCap, speciesId, typeCode, brainConfig, speciesPrefix.
- `AgentStore` extendido com novos campos SoA: `reproductionCooldown_`, `genomeId_`. Getters/setters + swap-remove correto.
- `AgentSpawn` ganha `reproductionCooldown` e `genomeId`.
- `EntityTypes.hpp` adiciona `GenomeId` (uint64) e `kInvalidGenomeId`.
- `ReproductionSystem` em `systems/` com:
  - `fromRegistry(registry, speciesPrefix, brainConfig)`: le todos os parametros bacteria_* e globals legados como fallback.
  - `apply(agents, genomes, neuralSystem, world, brainSignatureConfig, config, dt)`: tick de cooldown, filtragem (energia/idade/cooldown), respeito a max_population, criacao do filho.
- `NeuralSystem` ganha 2 metodos: `inheritBrain(childId, parentId, sig, mutRate, mutStr, rng)` clona o cerebro do pai (`MLPBrain::clone`), aplica mutacao opcional, registra sob `childId`; `removeBrainFor(id)`.
- `App` integra GenomeStore + ReproductionSystem: cria genoma fundador no spawn, executa reproducao apos InteractionSystem (antes do DeathSystem); titulo exibe `births`.
- 42 selftests cobrem: energia/idade/cooldown gating, conservacao energetica, IDs unicos, mundo retangular e circular, max_population, heranca MLP, mutacao zero/positiva, no-aliasing parent/child, reproducao por seed, integracao Perception/Neural/Movement/Energy/Death/Spatial, regressoes Fases 7-12, GenomeStore.
- Microbenchmark com 10 cenarios.
- Comandos CLI: `--phase13-selftest`, `--phase13-benchmark`, `--phase13-diagnostics`.

## Fora de Escopo

Nao implementado (preservado para fases futuras):
- Fase 14 (Gated/Shortcut/Modulated MLP), Fase 15 (RNN), Fase 16 (NEAT).
- Especies completas (Fase 17).
- UI completa de especies/genomas (Fases 22-25).
- Import/export completo de genomas (Fase 27).
- Save/load (Fase 27).
- Predadores funcionais completos (Fase 18).
- Dieta generica completa (Fase 18).
- Food chunk completo (Fase 19).
- Obstaculos e oclusao (Fase 20).
- Neural viewer completo (Fase 25).
- Benchmark runner formal (Fase 28).

## Documentos Consultados

Todos os 16 obrigatorios.

## Arquivos Python Consultados

- `sim/systems.py` (focal: ReproductionSystem do Python, splits, cooldown, age, population limits).
- `sim/entities.py` (Agent reproduce(), atributos heredaveis).
- `sim/brain.py` (clone, mutate de pesos e bias).
- `sim/controllers.py` (defaults bacteria_*).
- `sim/engine.py` (ordem dos sistemas).
- `sim/random_utils.py` (estrategia de RNG/seed).
- `sim/actuators.py`, `sim/spatial.py`, `sim/intelligence.py`, `sim/ui.py` (referencia).

Arquivos Python adicionais consultados: nenhum.

## Arquivos C++ Criados

- `src/simulation/GenomeStore.hpp` / `.cpp`
- `src/systems/ReproductionSystem.hpp` / `.cpp`
- `src/systems/Phase13Diagnostics.hpp` / `.cpp`
- `MIGRACAO_C++SFML/PHASE_13_REPRODUCTION_GENOME_STATUS.md`

## Arquivos C++ Modificados

**Cleanup de helpers duplicados (resolucao Divida 1):**
- `src/app/App.cpp` — incluiu `config/ParameterHelpers.hpp`, removeu 4 helpers locais + parameterColor.
- `src/systems/EnergySystem.cpp`
- `src/systems/DeathSystem.cpp`
- `src/systems/InteractionSystem.cpp`
- `src/systems/MovementSystem.cpp`
- `src/systems/NeuralSystem.cpp`
- `src/neural/BrainFactory.cpp`

**Phase 13:**
- `CMakeLists.txt` — adiciona GenomeStore, ReproductionSystem, Phase13Diagnostics.
- `src/main.cpp` — flags `--phase13-*`.
- `src/simulation/EntityTypes.hpp` — `GenomeId`, `kInvalidGenomeId`, `AgentSpawn.genomeId/reproductionCooldown`.
- `src/simulation/AgentStore.hpp/cpp` — campos `genomeId_`, `reproductionCooldown_`; getters/setters; swap-remove.
- `src/systems/NeuralSystem.hpp/cpp` — `inheritBrain`, `removeBrainFor`.
- `src/app/App.hpp/cpp` — wire GenomeStore + ReproductionSystem; titulo com `births`; spawn cria genoma fundador.
- `MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md` — Divida 1 marcada como RESOLVIDA.

## Correcoes de Continuidade

Nenhuma correcao funcional em Fases 9-12. Apenas:
- Helpers duplicados removidos das Fases 1-9 (cleanup historico, sem mudanca semantica).
- AgentStore ganha campos novos preservando swap-remove correto. Testes Phase 7-12 passam sem alteracao.

## Melhorias Alem do Escopo

1. **Cooldown tick separado do gating**: cooldown e decremementado SEMPRE no inicio de `apply()`, mesmo se `config.enabled=false`. Garante que cooldown nao acumula quando reproducao e temporariamente desligada.
2. **Snapshot de indices antes do loop**: agentes recem-nascidos no mesmo step nao sao re-avaliados como reprodutores. Evita explosao instantanea e mantem determinismo.
3. **`ReproductionStats` com breakdown**: separa `blockedByEnergy/Age/Cooldown/Population` para diagnostico.
4. **`GenomeStore::cloneFrom` automatico**: clona record do pai com `parentId` e `generation+1` preenchidos. ReproductionSystem nao precisa montar GenomeRecord manualmente.
5. **`AgentSpawn.reproductionCooldown`**: filho recebe cooldown inicial via spawn, evitando step extra para configurar.

## Justificativas

| Melhoria | Por que |
|---|---|
| Cooldown tick separado | Garante invariante "cooldown decai sempre que dt passa" — testavel isoladamente. |
| Snapshot de indices | Sem isso, filho recem-nascido com energia inicial 100 poderia ser avaliado e potencialmente reproduzir no mesmo step (com energia certa, gerando explosao). |
| Stats detalhadas | Diagnostico de "por que ninguem reproduziu" sem instrumentacao extra. |
| cloneFrom | Mantem invariante de generation+1 num so lugar. |
| Cooldown via spawn | Operacao atomica vs split de createAgent + setCooldown. |

## Status da Divida dos Helpers

**RESOLVIDA**. Detalhes em `TECHNICAL_DEBT_REGISTER.md`. ParameterHelpers.hpp (que ja existia desde Phase 10) agora e usado por todos os 7 arquivos que tinham duplicacoes. ReproductionSystem usa o header desde sua criacao.

## Decisao sobre ParameterHelpers.hpp

Mantido como header inline `agentbiosim::config::parameterX(...)`. Cada `.cpp` faz `#include "config/ParameterHelpers.hpp"` + `using config::parameterDouble;` (etc.) ao topo do namespace. Isso preserva chamadas existentes (ex.: `parameterDouble(parameters, "x", 0.0)`) sem refactor de cada chamada.

## Parametros Usados

- `bacteria_split_energy` (default 150)
- `bacteria_reproduction_min_age` (default 0)
- `bacteria_reproduction_cooldown` (default 0)
- `bacteria_mutation_rate` (default 0.05; vem via BrainConfig)
- `bacteria_mutation_strength` (default 0.08; vem via BrainConfig)
- `bacteria_max_limit` (default 0 = unlimited)
- `bacteria_min_limit` (lido como info; rescue completo e Fase 17)
- `bacteria_initial_energy` (default 100)
- `bacteria_energy_cap` (default 400)
- `bacteria_body_size` (default 9)
- `population_min_rescue_enabled` (lido como flag; rescue completo e Fase 17)
- `random_seed` (default -1 = epoch-derived)
- `reproduction_min_age`, `reproduction_cooldown` (globals legados como fallback)

## Parametros Pendentes

- `agent_template_name` — lido por outros sistemas; nao usado por ReproductionSystem.
- `predator_*` — Fase 18.
- `label_genome_ref` — Fase 17 (especies).

## Itens cobertos de FEATURE_INVENTORY.md

- Reproducao por energia: implementada.
- Idade minima: implementada.
- Cooldown: implementado.
- Mutacao de pesos MLP: implementada (rate + strength).
- Genoma inicial: implementado (GenomeStore + GenomeRecord).
- Limite maximo de populacao: implementado.
- Determinismo por seed: implementado.

## Itens cobertos de PARAMETER_INVENTORY.md

- Grupo "Bacterias/Organismo Base" reproducao/mutacao: usado funcionalmente.
- `population_min_rescue_enabled`: reconhecido (rescue completo e Fase 17).

## Itens de UI_INVENTORY.md Impactados

- "Editor genetico" (Fase 24): GenomeStore preparado.
- "Especies/labels" (Fase 17): preparado conceitualmente (campos speciesId, typeCode, speciesPrefix em GenomeRecord).
- "Painel agente selecionado" (Fase 25): genomeId em AgentStore para futura visualizacao.

## Decisoes

### ReproductionSystem
Sistema separado em `systems/ReproductionSystem.cpp`. Headless-friendly, sem SFML. Recebe AgentStore (mutavel), GenomeStore (mutavel), NeuralSystem (mutavel), World (const), brainSignatureConfig (const), ReproductionConfig (const), dt.

### GenomeStore inicial
SoA simples de `std::vector<GenomeRecord>` + `unordered_map<GenomeId, size_t>` indexById. Cresce via `createGenome` e `cloneFrom`. Nao remove registros nesta fase (records sao historia evolutiva, util para save/load futuro).

### GenomeId / GenomeHandle
`GenomeId = uint64_t`, contador comeca em 1. `GenomeHandle` e struct trivial com `id` + `isValid()`. Permite mudar tipo no futuro sem mudar API.

### Associacao agente-genoma
`AgentStore.genomeId_` por agente. Nao ha nada pesado: GenomeStore tem todos os dados; agente so guarda o ID.

### Heranca MLP
`NeuralSystem::inheritBrain(childId, parentId, signatureConfig, mutRate, mutStr, rng)`:
1. Busca BrainSlot do pai.
2. `MLPBrain::clone()` retorna copia profunda (vetores de pesos sao copiados — sem aliasing).
3. Aplica `mutate(rate, strength, rng)` no clone.
4. Insere no map sob childId com signature do config (evita NeuralSystem recriar com pesos aleatorios em syncBrains).

### Mutacao de pesos e biases
Delegada a `MLPBrain::mutate()` (ja existente desde Fase 9). Modifica pesos E biases. Taxa zero ou strength zero retorna false sem mudar nada.

### Divisao de energia
Modelo: `child_energy = parent_energy * 0.5; parent_energy = parent_energy * 0.5`. Conservacao garantida. Documentado como divergencia consciente do Python (que usa modelos varios). Conservacao testada (test 5).

### Idade minima
Comparada com `agents.ageAt(i)`. Globaal e por especie sao MAX para conservador.

### Cooldown
Por agente em `AgentStore.reproductionCooldown_`. Decai com dt no inicio de cada step. Filho recebe `config.reproductionCooldown` inicial para nao reproduzir no mesmo step.

### Posicao do filho
Ate 8 tentativas com angulo aleatorio + offset `(parentR + childR) * 1.0`. Se candidato sai do mundo, usa fallback `world.clampPosition`. Determinismo via rng_.

### Populacao maxima
`config.maxPopulation == 0` significa ilimitado. Comparacao com `agents.size()` antes de criar filho.

### population_min_rescue_enabled
Lido para `ReproductionConfig.populationMinRescue`. Sem efeito ativo nesta fase (sem rescue logic). Pendencia documentada para Fase 17 (especies completas).

### RNG/seed
`ReproductionSystem` tem `std::mt19937_64 rng_` proprio. Seeded de `config.seed` no primeiro `apply()` (via flag `rngInitialized_`). `reseed()` publico permite reset deterministico em testes.

### Integracao com AgentStore
Filho via `createAgent(spawn)` — mesma API que Phase 4. AgentStore preserva invariantes (swap-remove, indexById coerente).

### Integracao com NeuralSystem
`inheritBrain` injeta brain ANTES de proxima `syncBrains` em `produceMovementControls`. Signature batendo evita recriacao com pesos aleatorios. `removeBrainFor` disponivel para death events futuros (nao usado em hot path).

### Integracao com PerceptionSystem
Filho aparece em `agents.aliveAt(i) == true` no proximo step. PerceptionSystem itera sobre todos os vivos — filho recebe input automaticamente sem alteracao.

### Integracao com MovementSystem
Apos `inheritBrain`, NeuralSystem produz output para filho normalmente. MovementSystem aplica como qualquer outro agente.

### Integracao com SpatialHash
Reproducao acontece DURANTE o step (depois de Interaction). App ja rebuilda SpatialHash apos `lastReproductionStats_.birthsThisStep > 0`. Garante que proximo step ve o filho na hash.

## Testes Executados

```
Phase13 validation: PASS (42 checks)
```

Cobertura: split energy, conservacao, age, cooldown, IDs, world bounds (rect+circular), max_pop, MLP inheritance, mutation zero/positive, no-aliasing, seed determinism, integracoes com Perception/Movement/Energy/Death, SpatialHash, GenomeStore invariantes, regressoes Fases 7-12, child propio reproduz em steps futuros.

## Regressoes

```
Phase 7: PASS (14)
Phase 8: PASS (15)
Phase 9: PASS (23)
Phase 10: PASS (21)
Phase 11: PASS (30)
Phase 12: PASS (49)
```

## Resultado do Benchmark

| Cenario | Inicial | Final | Nascimentos | Steps | Total (ms) | us/step | us/nascimento | Mut | Hidden |
|---|---|---|---|---|---|---|---|---|---|
| baseline_no_reproduction | 100 | 100 | 0 | 30 | 0.02 | 0.7 | - | off | 8 |
| low_rate_100_small_mlp | 100 | 200 | 100 | 30 | 0.42 | 14 | 4.2 | on | 8 |
| low_rate_300_small_mlp | 300 | 600 | 300 | 30 | 1.24 | 41 | 4.1 | on | 8 |
| low_rate_600_default_mlp | 600 | 1200 | 600 | 30 | 9.65 | 322 | 16.1 | on | 20/20/20/20 |
| low_rate_1000_default | 1000 | 2000 | 1000 | 30 | 16.15 | 538 | 16.2 | on | 20/20/20/20 |
| high_rate_300_mutation_off | 300 | 1200 | 900 | 30 | 4.57 | 152 | 5.1 | off | 16/16 |
| high_rate_300_mutation_on | 300 | 1200 | 900 | 30 | 6.32 | 211 | 7.0 | on | 16/16 |
| clone_small_mlp | 300 | 1200 | 900 | 30 | 2.85 | 95 | 3.2 | off | 8 |
| clone_medium_mlp | 300 | 1200 | 900 | 30 | 3.65 | 122 | 4.1 | off | 16/16 |
| clone_default_mlp | 300 | 1200 | 900 | 30 | 7.01 | 234 | 7.8 | off | 20/20/20/20 |

Custo de nascimento: ~3-8us para MLPs pequenas/medias, ~16us para MLP default (20/20/20/20). Mutacao adiciona ~30%. Reproducao desligada e essencialmente gratuita (0.7us/step). 

## Diagnostico Visual

App boota com "Phase 13: reproduction, genome and base mutation initialized." Title bar inclui `births` ao lado de `eaten`/`deaths`. Reproducao acontece naturalmente quando organismos acumulam energia suficiente comendo.

## Divergencias contra Python

| Item | Python | C++ | Justificativa |
|---|---|---|---|
| Split de energia | Modelos varios (50/50, fixed_offspring, etc.) | 50/50 strict | Conservacao testada; simples. |
| Cooldown decay | Por agent state | Identico (campo em AgentStore) | Identico. |
| RNG | numpy global | mt19937_64 por sistema | Determinismo equivalente. |
| Clone brain | numpy `.copy()` | `MLPBrain::clone()` (value copy) | Equivalente. |
| Mutacao | numpy noise | `std::normal_distribution` | Distribuicao identica. |
| Posicao filho | adjacent random | identica (8 tentativas + clamp) | Equivalente. |
| Genome metadata | dict | struct GenomeRecord | C++ idiom. |

## Limitacoes Atuais

- `population_min_rescue_enabled` lido mas sem efeito real (Fase 17).
- `predator_*` parametros nao usados (Fase 18).
- GenomeRecord guarda `BrainConfig` mas a verdade do brain esta no MLPBrain do NeuralSystem (Divida 2 ainda pendente para multiplos tipos neurais).
- GenomeStore nao remove registros — historia evolutiva acumula. OK para Phase 13, podera ser limpado em Phase 27 (save/load).
- Cor do filho nao muda — sem cor por linhagem ainda (futuro).
- Sem heranca de cor mutada (mutacao apenas em pesos MLP).

## Pendencias para Fase 14

- Brain types alem de MLP (Gated/Shortcut/Modulated).
- `BrainSlot` polimorfico ou variant.
- Mutacao especifica por tipo neural.

## Pendencias para Fase 17

- `SpeciesStore` completo.
- `population_min_rescue_enabled` com efeito real por especie.
- `label_genome_ref` (mapping label -> genome).
- Reset de cerebro por especie.
- Heranca de cor por linhagem.

## Pendencias para Save/Load

- Serializar GenomeStore (records).
- Serializar AgentStore.genomeId_.
- Serializar GenomeRecord + brainConfig + (futuramente) pesos do brain.

## Pendencias para UI Futura

- Editor genetico (Phase 24).
- Inspector com genome lineage (Phase 25).
- Toggle de mutacao em runtime (Phase 23).

## Confirmacoes

- Nao implementou redes avancadas.
- Nao implementou especies completas.
- Nao implementou UI completa.
- Nao implementou neural viewer completo.
- Nao implementou predadores funcionais.
- Nao implementou dieta generica.
- Nao implementou food chunk.
- Nao implementou obstaculos.
- Nao implementou save/load.
- Nao avancou para Fase 14.
- Nenhum arquivo Python foi alterado.
- Build Debug e Release: OK.
- Phase 13 selftest: PASS (42 checks).
- Regressoes 7-12: PASS.

## Final Audit

Data: 2026-05-28.

Decisao: pronto para commit da Fase 13.
