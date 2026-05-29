# Fase 17 — Especies, Labels e Genomas

Status: **CONCLUIDA** (2026-05-29)

A Fase 17 consolida o conceito arquitetural de `Species` no projeto C++. Antes desta fase, `bacteria` e `predator` apareciam como strings de prefixo em parametros (`bacteria_*`, `predator_*`) e como `AgentTypeCode::LegacyBacteria/LegacyPredator` em codigo de spawn. Agora existe um `SpeciesStore` canonico que centraliza metadados, aliases e referencias para genoma default. `AgentStore` ja carregava `speciesId`/`genomeId` desde a Fase 13; esta fase liga esses campos ao novo `SpeciesStore` via `SpeciesBootstrap`.

## Escopo executado

- `SpeciesId` (`std::uint32_t`) e `kInvalidSpeciesId = 0` formalizados em `simulation/EntityTypes.hpp` e `simulation/SpeciesStore.hpp`.
- `SpeciesRecord` com nome canonico, label, prefixo de parametros, cor, initialCount/min/max/showGraph/enabled/populationMinRescueEnabled, defaultGenomeId, aliases legados, typeCode e bodyShape.
- `SpeciesStore` (headless, sem SFML/UI) com `registerSpecies`, lookup por id/nome/alias normalizado (case + trim + `-`/space -> `_`), e helpers de mutacao.
- `SpeciesBootstrap` que le um prefixo de parametros (`bacteria`, `predator`, futuros) do `ParameterRegistry` e cria simultaneamente o `SpeciesRecord` e o `GenomeRecord` default no `GenomeStore`. `BrainFactory::configFromRegistry` ja resolve o tipo neural correto a partir de `neural_network_type`.
- `bootstrapDefaultSpecies()` cria as duas especies canonicas (`bacteria` + `predator`). Predator fica registrado mesmo quando `predators_enabled=false`, com `enabled=false`, para que aliases continuem resolvendo e UI futura possa lista-lo.
- `App` agora usa `SpeciesStore` no `spawnDemoEntities()`: o spawn inicial le `initialCount`/`color`/`bodyShape` da especie; agentes recebem `speciesId` e `genomeId` da especie. Predator entra no spawn inicial somente quando `enabled && initialCount > 0`.
- `NeuralSystem::resetForSpecies(agents, speciesId, brainConfig, seed)` recria brains de todos os agentes de uma especie usando o `BrainFactory`. Estados recorrentes (Simple RNN, NEAT recorrente) sao zerados implicitamente porque uma instancia nova substitui a anterior. Operacao headless e deterministica.
- `Phase17Diagnostics` com 110 selftests e microbenchmark em 10 cenarios, expostos via `--phase17-selftest`, `--phase17-benchmark`, `--phase17-diagnostics`.
- `App` init message atualizado para Fase 17.
- `CMakeLists.txt` lista os 3 arquivos novos.

## Fora de escopo (conforme prompt)

Nao implementado:

- Fase 18 (predador/dieta generica final);
- predacao final / canibalismo / corpse-to-food;
- food chunk completa;
- obstaculos;
- UI completa de especies/genomas / editor genetico / painel de populacao;
- save/load/export/import / autosave;
- benchmark runner formal completo;
- metricas/graficos UI completos;
- refatoracao grande do `App`.

Nenhum arquivo Python foi alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_13_REPRODUCTION_GENOME_STATUS.md`
- `PHASE_14_DENSE_ADVANCED_NETWORKS_STATUS.md`
- `PHASE_15_SIMPLE_RNN_STATUS.md`
- `PHASE_16_NEAT_FAMILY_STATUS.md`

`PHASE_9_MLP_STATUS.md` ate `PHASE_12_SECTOR_BINS_STATUS.md` foram lidos em sessoes anteriores e seu conteudo continua valido (verificado via `--phase{N}-selftest`).

## Arquivos Python consultados

- `sim/brain.py` (apenas como referencia funcional ja consultada em fases anteriores).
- `sim/entities.py` (referencia conceitual de Species/Labels).

Nenhuma alteracao em arquivo Python.

## Arquivos C++ criados

- `src/simulation/SpeciesStore.hpp`
- `src/simulation/SpeciesStore.cpp`
- `src/simulation/SpeciesBootstrap.hpp`
- `src/simulation/SpeciesBootstrap.cpp`
- `src/simulation/Phase17Diagnostics.hpp`
- `src/simulation/Phase17Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_17_SPECIES_LABELS_GENOMES_STATUS.md`

## Arquivos C++ modificados

- `src/app/App.hpp` — adicionado `SpeciesStore species_` e include.
- `src/app/App.cpp` — `spawnDemoEntities()` agora bootstrapa especies, le campos da `SpeciesRecord`, spawna predator condicionalmente; init message atualizado.
- `src/systems/NeuralSystem.hpp` — declarado `resetForSpecies`.
- `src/systems/NeuralSystem.cpp` — implementado `resetForSpecies`.
- `src/main.cpp` — flags `--phase17-*` e includes.
- `CMakeLists.txt` — 3 fontes novas.
- `MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md` — adicionada Divida 7.

## Correcoes de continuidade vindas das Fases 9 a 16

- **Divida 7** (nova): `ReproductionSystem::apply()` recebe `const BrainConfig&` que pode aliar `GenomeStore` invalidado durante `cloneFrom()`. O bug nao manifesta no caminho do App (que copia a config em `NeuralSystemConfig`), mas se manifestou no Phase17Diagnostics original. Documentada como divida com plano de correcao para Fase 18 ou 27. Os testes foram ajustados para copiar `brainConfig` antes do `apply()`. **Por que necessario:** o teste estressou o caminho com `cloneFrom()` no mesmo store de onde a ref vinha; o codigo do `App` so foi sortudo porque sempre copia. O contrato precisa ficar explicito.

- **App spawn flow**: antes da Fase 17, `spawnDemoEntities` montava o `GenomeRecord` founder inline com leituras hardcoded de `bacteria_*` e passava `spawn.speciesId = 0`. Agora o flow e `SpeciesStore → SpeciesRecord → GenomeRecord → AgentSpawn` e o spawn carrega `speciesId` valido. **Por que necessario:** o prompt exige que agentes recebam species_id valido; o codigo legado deixava 0.

Nenhuma outra divida foi introduzida ou modificada.

## Melhorias alem do escopo minimo

- `SpeciesStore` aceita `parameterPrefix` como alias automatico (alem do nome e label). Isso significa que `species.resolveAlias("bacteria")` (o prefixo) sempre funciona, sem precisar configurar manualmente. Util para mapear de parametros legados sem hardcode adicional. **Justificativa:** evita duplicacao no caller e fica documentado como parte do modelo de alias.

- `NeuralSystem::resetForSpecies` aceita seed e e deterministico. Nao era estritamente exigido como deterministico, mas mantemos a convencao de Fase 14/15. **Justificativa:** facilita testes e save/load determinismo na Fase 27.

## Decisao sobre `SpeciesStore`

- Headless puro: zero dependencia de SFML/UI/render/neural.
- Vetor `records_` + `unordered_map<SpeciesId, index>` + `unordered_map<string, SpeciesId>` para aliases.
- Alias normalizado: trim + lower + `-`/space -> `_`.
- Ids monotonicos (`nextId_` comeca em 1). 0 reservado para "sem especie".
- Sem `set/remove` de entries (estaveis durante simulation step).

## Decisao sobre `SpeciesId`

- `using SpeciesId = std::uint32_t;` (ja existia em `EntityTypes.hpp`).
- Adicionado `kInvalidSpeciesId = 0` em `SpeciesStore.hpp`.
- Em `AgentStore`, default `speciesId = 0` significa "ainda nao atribuido".

## Decisao sobre `SpeciesRecord`

Campos:

```text
id (SpeciesId)
name (canonical, e.g. "bacteria")
label (display, e.g. "Bacteria")
parameterPrefix (registry prefix)
color (ColorRgb)
initialCount, minPopulation, maxPopulation (int)
showGraph, enabled, populationMinRescueEnabled (bool)
defaultGenomeId (GenomeId)
legacyAliases (vector<string>)
typeCode (AgentTypeCode legado)
bodyShape (BodyShapeCode)
```

`typeCode` mantido para compatibilidade com codigo legado que ainda consulta `LegacyBacteria`/`LegacyPredator`. Em fases futuras pode ser removido.

## Decisao sobre aliases `bacteria` / `predator`

Resolvem para os ids canonicos das duas especies default. Sao registrados automaticamente via `name`/`parameterPrefix`. Em `bootstrapDefaultSpecies` adicionei tambem `organismo_1`, `organismo_base`, `label_bacteria`, `labels_bacteria` (e equivalentes para predator) para cobrir terminologia Python legada.

## Decisao sobre aliases `label` / `labels`

`label_bacteria`, `label_predator`, `labels_bacteria`, `labels_predator` registrados como aliases legados. Estrutura preparada para receber mais aliases via `SpeciesStore::addAlias(id, "...")` quando necessario. Termos genericos `label` ou `labels` sozinhos nao resolvem porque sao ambiguos — o caller deve usar `label_<name>` ou `species_<name>`.

## Decisao sobre `GenomeStore` completo/expandido

A Fase 13 ja deixou `GenomeRecord` com `bodySize`, `bodyShape`, `color`, `mutationRate`, `mutationStrength`, `reproductionMinAge`, `reproductionCooldown`, `splitEnergy`, `initialEnergy`, `energyCap`, `speciesId`, `typeCode`, `brainConfig`, `speciesPrefix`. **Decisao Fase 17**: nao expandir struct; alimentar todos os campos via `bootstrapSpecies()`. Visao/retina nao tem campos dedicados em `GenomeRecord` porque o `PerceptionSystem` ja le diretamente do `ParameterRegistry` via `speciesPrefix`. Manter assim evita duplicacao e simplifica saves futuros.

## Decisao sobre associacao especie -> genoma

`SpeciesRecord::defaultGenomeId` aponta para o `GenomeRecord` no `GenomeStore`. Atribuido durante `bootstrapSpecies`. Em fases futuras, espécies podem apontar para multiplos genomas (variant pool) — isso fica preparado porque `setDefaultGenome` e helper publico.

## Decisao sobre associacao agente -> especie

`AgentStore` (Fase 13) ja carrega `SpeciesId speciesId_`. Em Fase 17, o `App` e o `Phase17Diagnostics` passam `spawn.speciesId = species->id` durante `createAgent()`. `ReproductionSystem::apply()` (Fase 13) ja preserva `spawn.speciesId = agents.speciesIdAt(parentIndex)`.

## Decisao sobre associacao agente -> genoma/brain

`spawn.genomeId = species->defaultGenomeId` no spawn inicial. Filhos recebem `childGenomeHandle.id` via `GenomeStore::cloneFrom(parentGenomeId)` (preserva a linhagem mesmo se a especie tiver multiplos genomas no futuro). Brains sao criados/herdados por `NeuralSystem`.

## Decisao sobre especies default

`bootstrapDefaultSpecies` cria duas: bacteria e predator. Predator tem `enabled = predators_enabled` (default `false`). Quando desabilitado, a especie continua no store (aliases resolvem, UI futura pode mostrar) mas nao recebe spawn inicial.

## Decisao sobre populacao inicial

`bacteria.initialCount` lido de `bacteria_count` (default 150). `predator.initialCount` lido de `predator_count` (default 0). `App::spawnDemoEntities` itera o initialCount de cada especie habilitada.

## Decisao sobre min/max populacional por especie

`minPopulation`/`maxPopulation` lidos dos parametros `*_min_limit`/`*_max_limit`. Armazenados em `SpeciesRecord`. `ReproductionSystem::apply()` ainda recebe `maxPopulation` via `ReproductionConfig` (pre-existente) — para preparacao final da Fase 18 sera trivial fazer com que o caller passe `species.maxPopulation` por chamada.

## Decisao sobre `population_min_rescue_enabled`

Lido por especie em `bootstrapSpecies`. Armazenado em `SpeciesRecord::populationMinRescueEnabled`. Rescue logic em si ainda nao foi implementado (era pendente desde Fase 13); fica em pendencia explicita para quando houver mecanismo de spawn de emergencia.

## Decisao sobre cor por especie

Lido de `<prefix>_color` em `bootstrapSpecies`. Armazenado em `SpeciesRecord::color` e copiado para `GenomeRecord::color`. `App::spawnDemoEntities` usa `species->color` por agente. Renderer continua usando `agents.colorAt(i)` — sem acoplamento ao SpeciesStore.

## Decisao sobre graph flag por espécie

`SpeciesRecord::showGraph` armazenado mas ainda nao consumido por UI (UI fora de escopo). Disponivel para Fase 26 (metrics/graficos).

## Decisao sobre reset neural por especie

`NeuralSystem::resetForSpecies(agents, speciesId, brainConfig, seed)`:

1. Itera todos os agentes.
2. Para cada agente com `speciesIdAt(i) == speciesId`, busca o BrainSlot pelo agent id.
3. Recria o brain via `BrainFactory::createBrain(brainConfig, rng)` onde `rng = seed XOR agentId * GOLDEN_HASH`.
4. Substitui o brain no slot.
5. Retorna numero de brains recriados.

Determinismo: mesma seed + mesmos agent ids -> mesmos brains.

Estados recorrentes: por substituir a instancia inteira, RNN/NEAT recorrente comecam com state zero. Cobertura: tests 53-61 confirmam funcionamento para os 8 tipos.

## Decisao sobre labels legados

Mantidos como aliases case-insensitive em `SpeciesStore::aliasIndex_`. Sem perda de identidade.

## Decisao sobre `label_genome_ref`

Representado por `SpeciesRecord::defaultGenomeId`. Preparado para receber referencias plurais em Fase 27 (save/load) se necessario.

## Preparacao para Fase 18 (predador/dieta)

- `SpeciesRecord::typeCode` ja distingue `LegacyPredator` vs `LegacyBacteria`.
- `SpeciesRecord` pode acomodar futuro campo `DietConfig` sem quebrar saves porque nao serializa ainda.
- `parameterPrefix` ja le `*_diet_*` parametros (existentes desde Fase 0). `bootstrapSpecies` pode ser estendido para preencher `DietConfig` na Fase 18.
- Predator existe no store mesmo quando disabled — Fase 18 so precisa setar `enabled=true` e habilitar spawn.

## Preparacao para Fase 24 (editor UI)

- `SpeciesStore` ja expoe `records()` para listar todas as especies, e helpers `setColor/setEnabled/addAlias`.
- UI pode iterar/`setX` sem acoplar a SpeciesStore.
- Identidade de cada record e estavel.

## Preparacao para Fase 27 (save/load)

- `SpeciesRecord` e `GenomeRecord` sao POD-like (sem ponteiros, sem `std::function`, sem unique_ptr).
- `SpeciesId` e `GenomeId` sao `uint32_t`/`uint64_t`, faceis de serializar.
- `legacyAliases` e `vector<string>` — direto para JSON.
- `BrainConfig` e `NeatConfig` ja foram preparados em fases 14-16.

## Como MLP baseline foi protegida

- Fase 9 selftest passa (23 checks).
- Fase 9 benchmark roda em paralelo com Phase 17 (sem alteracao no caminho MLP).
- `BrainFactory` nao foi modificado nesta fase; so `BrainConfig` recebeu campo `NeatConfig` ja na Fase 16.

## Como Gated/Shortcut/Modulated foram protegidas

- Fase 14 selftest passa (71 checks).
- Microbenchmark Fase 17: gated/shortcut/modulated em ~0.39-0.41 us/agent, equivalente ao baseline.

## Como Simple RNN foi protegida

- Fase 15 selftest passa (72 checks).
- Microbenchmark Fase 17: RNN em ~0.38 us/agent, sem regressao.

## Como familia NEAT foi protegida

- Fase 16 selftest passa (109 checks).
- Fase 16 benchmark dentro do mesmo envelope (`neat_*` em ~2.4-17.9 us/forward, igual ao registrado na Fase 16).
- Phase17 benchmark mostra NEAT em ~1.45-1.59 us/agent (mistura de minimal/layered).

## Testes executados

- Debug: `--phase17-selftest` (110 checks PASS).
- Release: `--phase17-selftest` (110 checks PASS).
- Regressoes Debug Phase 7-16: todas PASS (14+15+23+21+30+49+42+71+72+109 = 446 checks).
- Phase 16 benchmark Release: sem regressao.
- Phase 17 benchmark Release: dados disponiveis.

## Resultado dos diagnostics / microbenchmark (Release)

10 cenarios. Destaques:

| Cenario | Especies | Agentes | Brain | avg_op_us (alias) | per_agent_us (forward) |
|---|---|---|---|---|---|
| 2sp_100ag_mlp | 2 | 100 | mlp | 0.0595 | 0.3521 |
| 5sp_300ag_mlp | 5 | 300 | mlp | 0.0560 | 0.3793 |
| 20sp_600ag_mlp | 20 | 600 | mlp | 0.0594 | 0.3564 |
| 2sp_100ag_gated | 2 | 100 | gated_mlp | 0.0570 | 0.3904 |
| 2sp_100ag_shortcut | 2 | 100 | shortcut_mlp | 0.0572 | 0.4159 |
| 2sp_100ag_modulated | 2 | 100 | modulated_mlp | 0.0558 | 0.4031 |
| 2sp_100ag_rnn | 2 | 100 | simple_rnn | 0.0547 | 0.3842 |
| 2sp_100ag_neat | 2 | 100 | neat_common | 0.0532 | 1.4544 |
| 2sp_100ag_neat_simple | 2 | 100 | neat_simplified | 0.0605 | 1.5478 |
| 2sp_100ag_neat_rec | 2 | 100 | neat_recurrent | 0.0725 | 1.5914 |

Observacoes:

- Lookup de alias por nome canonico em ~0.05-0.07 us, indistinguivel para 2 ou 20 especies. unordered_map performa bem.
- forward por agente nao regrediu vs Fases 9/14/15/16.
- reset_total_us escala linearmente com agentes recriados.

## Parametros usados

Lidos via `bootstrapSpecies`:

```
<prefix>_count
<prefix>_min_limit
<prefix>_max_limit
<prefix>_color
<prefix>_body_size
<prefix>_body_shape
<prefix>_initial_energy
<prefix>_energy_cap
<prefix>_split_energy
<prefix>_mutation_rate
<prefix>_mutation_strength
<prefix>_reproduction_min_age
<prefix>_reproduction_cooldown
<prefix>_show_graph (fallback)
population_min_rescue_enabled
predators_enabled (para predator)
neural_network_type (via BrainFactory)
random_seed (via App e via NeuralSystem)
```

## Metadados implementados

- species_id, species_name, species_label, species_color, species_min, species_max, species_initial, species_show_graph, species_genome_ref, species_enabled, species_min_rescue_enabled.
- legacy aliases: bacteria, predator, label_bacteria, label_predator, labels_bacteria, labels_predator, organismo_1, organismo_base, predador, predators.

## Metadados pendentes

- DietConfig (Fase 18).
- ColorRgb com alpha (atualmente sem alpha).
- Per-species perception/vision overrides (`PerceptionSystem` ainda le do registry por prefixo, nao por species record).
- showGraph atualmente nao tem UI consumindo (Fase 26).

## Itens cobertos de `FEATURE_INVENTORY.md`

- Species/Labels conceitual.
- Spawn por especie.
- Limites populacionais por especie.
- Aliases legados.

## Itens cobertos de `PARAMETER_INVENTORY.md`

- Todo o grupo `bacteria_*` consumido por bootstrap.
- Todo o grupo `predator_*` consumido por bootstrap (mesmo quando enabled=false).
- `predators_enabled`, `population_min_rescue_enabled`, `random_seed`, `neural_network_type`.

## Itens de `UI_INVENTORY.md` impactados ou preservados

- Painel de especies/genomas (Fase 24) — `SpeciesStore::records()` ja expoe iteracao para popular UI.
- Painel de gráfico por especie (Fase 26) — `showGraph` ja armazenado.

## Divergencias contra Python

- C++ trata `predators_enabled=false` registrando o predator com `enabled=false` mas sem spawn inicial. Python pula completamente o registro. Decisao C++: manter `SpeciesRecord` no store para aliases resolverem; equivalente funcional.
- Aliases C++ sao case-insensitive e tratam `-`/space como `_`. Python e mais rigido. Tomando essa diferenca como melhoria.

## Limitacoes atuais

- `ReproductionSystem` recebe `maxPopulation` por `ReproductionConfig` (legado), nao por `SpeciesStore`. Fase 18 vai unificar isso.
- `PerceptionSystem` ainda le retina_* por prefixo string, nao por `SpeciesRecord`. Funciona porque speciesPrefix esta no genome, mas seria mais limpo refatorar para consumir o store. Fase 18+ pode encarregar disso.
- Population min rescue ainda nao tem mecanismo de spawn de emergencia (era pendente desde Fase 13).
- Graph flag por especie nao tem UI consumindo (Fase 26).

## Pendencias para Fase 18

- Diet config preenchido em `bootstrapSpecies`.
- Predacao habilitada quando `predators_enabled=true`.
- Canibalismo / corpse-to-food.
- `ReproductionSystem` consumindo `SpeciesRecord::maxPopulation` em vez de `ReproductionConfig`.
- Possivelmente migrar `PerceptionSystem` para consumir SpeciesRecord.

## Pendencias para Fase 24

- UI de especies/genomas com `SpeciesStore::records()` + helpers `setColor/setEnabled/addAlias`.

## Pendencias para Fase 26

- Gráfico por especie usando `SpeciesRecord::showGraph` + metricas.

## Pendencias para Fase 27

- Save/load de `SpeciesStore` + `GenomeStore` + `AgentStore::speciesId_`/`genomeId_`.
- Import/export de genoma por especie.

## Pendencias para Fase 29

- Benchmark runner formal com cenarios multi-especie.

## Confirmacao de escopo

- Nenhum arquivo Python foi alterado.
- Predadores / dieta generica final NAO implementado.
- UI completa NAO implementada.
- Save/load final NAO implementado.
- Phase 18 NAO iniciada.
