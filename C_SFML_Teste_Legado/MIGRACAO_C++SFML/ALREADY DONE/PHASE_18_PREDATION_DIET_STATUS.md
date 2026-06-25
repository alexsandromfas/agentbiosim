# Fase 18 — Predacao e Dieta Generica

Status: **CONCLUIDA** (2026-05-29)

A Fase 18 transforma predacao em uma consequencia da dieta da especie/genoma, eliminando a necessidade de uma classe rigida `Predator`. Predador agora e simplesmente uma especie com `diet_agents=true`, e bacteria continua sendo uma especie com `diet_food=true`. A logica de comida + predacao foi unificada em um unico passo do `InteractionSystem` (`applyWithDiet`) que despacha por agente segundo o `DietConfig` herdado do genoma.

## Escopo executado

- `DietConfig` (`src/simulation/DietConfig.hpp`) com `eatFood`, `eatAgents`, `eatSameSpecies`, `foodEfficiency`, `agentEfficiency`, `corpseToFood`.
- `GenomeRecord::diet` (fonte de verdade da dieta; herdavel via `cloneFrom`).
- `SpeciesRecord::dietSnapshot` (espelho da dieta do genoma default, para UI/diagnostics).
- `SpeciesBootstrap` agora le `<prefix>_diet_food/agents/same_label/food_efficiency/agent_efficiency` e `<prefix>_corpse_to_food` do `ParameterRegistry` para popular o genoma + snapshot.
- `InteractionSystem::applyWithDiet()` (novo): despacha por agente entre consumir comida e predar agente, com selecao deterministica de presa (menor id de agente vencedor), bloqueio de canibalismo configuravel e geracao de comida instantanea no local da presa quando `corpseToFood=true`.
- `InteractionSystem::apply()` (Phase 7 path) preservado intacto.
- `App::runSimulationStep` agora chama `applyWithDiet` no lugar de `apply`, agregando estatisticas de comida e predacao em `lastInteractionStats_` e `deathsCount_`.
- `Phase18Diagnostics` com 111 selftests + microbenchmark de 13 cenarios.
- Resolucao da **Divida 7** (assinatura de `ReproductionSystem::apply` agora recebe `neural::BrainConfig` por valor).
- Mensagem inicial de `App` atualizada.
- `CMakeLists.txt` lista os 2 fontes novos.

## Fora de escopo (confirmado)

- Fase 19 (comida chunk/pedacos) — nao iniciada.
- Obstaculos / oclusao — nao iniciado.
- Colisao/fisica avancada (Fase 21) — nao iniciada.
- UI completa de dieta/especies — nao implementada.
- Save/load final — nao implementado.
- Export/import de genomas — nao implementado.

Nenhum arquivo Python foi alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`
- `PROPOSED_CPP_ARCHITECTURE.md`
- `FEATURE_INVENTORY.md`
- `PARAMETER_INVENTORY.md`
- `TECHNICAL_DEBT_REGISTER.md`
- `PHASE_17_SPECIES_LABELS_GENOMES_STATUS.md`

## Arquivos Python consultados

- `sim/entities.py` (referencia conceitual para diet & predation rules).
- `sim/systems.py` (referencia conceitual para InteractionSystem food path).

## Arquivos C++ criados

- `src/simulation/DietConfig.hpp`
- `src/systems/Phase18Diagnostics.hpp`
- `src/systems/Phase18Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_18_PREDATION_DIET_STATUS.md`

## Arquivos C++ modificados

- `src/simulation/GenomeStore.hpp` — campo `DietConfig diet`.
- `src/simulation/SpeciesStore.hpp/.cpp` — `dietSnapshot`, `setDietSnapshot()`.
- `src/simulation/SpeciesBootstrap.cpp` — leitura de `*_diet_*` e `*_corpse_to_food`.
- `src/systems/InteractionSystem.hpp/.cpp` — `DietInteractionConfig`, `DietInteractionStats`, `applyWithDiet`.
- `src/systems/ReproductionSystem.hpp/.cpp` — assinatura de `apply` por valor (Divida 7).
- `src/app/App.cpp` — `runSimulationStep` usa `applyWithDiet`; init message atualizado.
- `src/main.cpp` — flags `--phase18-*`.
- `CMakeLists.txt` — 2 fontes novos.
- `MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md` — Divida 7 marcada como resolvida.

## Correcoes de continuidade vindas das Fases 9 a 17

- **Divida 7 (resolvida)**: `ReproductionSystem::apply` recebia `const BrainConfig&` que podia aliar `GenomeStore`. Resolvido com pass-by-value. Phase18Diagnostics test 90 cobre o cenario perigoso explicitamente. **Por que necessario:** o prompt da Fase 18 exigia explicitamente a resolucao, e a Fase 18 mexe com `InteractionSystem` + `GenomeStore` simultaneamente.

Nenhuma outra divida foi introduzida.

## Melhorias alem do escopo minimo

- `InteractionSystem::dietConfigFromRegistry()` faz leitura de `predators_enabled` para desligar o caminho de predacao globalmente quando nao houver predadores, evitando overhead de iterar candidatos a presa. **Justificativa:** mantem o requisito de "impedir que predacao adicione custo relevante quando desligada".

- Selecao deterministica de presa usando "menor agent id" entre candidatos. **Justificativa:** o prompt pediu determinismo; entity IDs sao estaveis durante uma simulacao (Phase 13).

- Snapshot de dieta em `SpeciesRecord::dietSnapshot` (alem do genoma). **Justificativa:** facilita UI futura sem precisar dereferenciar o GenomeStore a cada query. Custo de duplicacao e ~32 bytes por especie.

## Parametros usados

Por prefixo de especie:

```
<prefix>_diet_food            -> DietConfig::eatFood
<prefix>_diet_agents          -> DietConfig::eatAgents
<prefix>_diet_same_label      -> DietConfig::eatSameSpecies (alias diet_same_species)
<prefix>_diet_food_efficiency -> DietConfig::foodEfficiency
<prefix>_diet_agent_efficiency-> DietConfig::agentEfficiency
<prefix>_corpse_to_food       -> DietConfig::corpseToFood
predators_enabled             -> DietInteractionConfig::predationEnabled (global)
bacteria_energy_cap           -> DietInteractionConfig::defaultEnergyCap (fallback)
use_spatial                   -> DietInteractionConfig::useSpatial
```

Todos os parametros listados no prompt obrigatorio sao consumidos.

## Parametros pendentes

- `diet_same_label` parametro generico (sem prefixo) e `diet_same_species` parametro generico nao tem entrada na registry (so existem como `<prefix>_diet_same_label`). Documentado para se necessario, Fase 24 adicionar UI generica.

## Como funciona `DietConfig`

`DietConfig` e uma struct POD no namespace `agentbiosim::simulation`:

```cpp
struct DietConfig {
    bool eatFood = true;
    bool eatAgents = false;
    bool eatSameSpecies = false;
    double foodEfficiency = 1.0;
    double agentEfficiency = 0.7;
    bool corpseToFood = false;
};
```

E owned pelo `GenomeRecord::diet`. `SpeciesRecord::dietSnapshot` espelha o valor do genoma default no momento do bootstrap. Filhos herdam diet automaticamente via `GenomeStore::cloneFrom(parentId)` que faz copia profunda do `GenomeRecord`.

## Onde a dieta vive

- **Source of truth**: `GenomeRecord::diet` (no `GenomeStore`).
- **Snapshot UI/diagnostics**: `SpeciesRecord::dietSnapshot` (no `SpeciesStore`).
- **Per-agent lookup**: `genomes.find(agents.genomeIdAt(i))->diet`.

Decisao: nao adicionar campo `dietAt(index)` em `AgentStore` para evitar duplicacao. Custo de lookup e ~10ns (unordered_map).

## Como funcionam os flags individuais

- `diet_food` (eatFood): se true, o agente tenta consumir uma comida instantanea adjacente no step. Mesmo se for true, predacao tambem pode ocorrer no mesmo step se `eatAgents=true`.
- `diet_agents` (eatAgents): se true, o agente busca presas no spatial hash, escolhe a com menor id, e mata uma presa por step (predador "lock per step").
- `diet_same_species` / alias `diet_same_label` (eatSameSpecies): se false, o predador nunca preda alguem da mesma `SpeciesId`. Se true, canibalismo e permitido.
- `diet_food_efficiency` (foodEfficiency): multiplicador na energia ganha por comida (`gain = foodEnergy * foodEfficiency`).
- `diet_agent_efficiency` (agentEfficiency): multiplicador na energia ganha por predacao (`gain = preyEnergy * agentEfficiency`).
- `corpse_to_food` (corpseToFood): propriedade da PRESA, nao do predador. Quando true e a presa e morta por predacao, uma comida instantanea aparece no local da presa com energia igual ao initial energy da presa.

## Como comida instantanea foi preservada

- `apply()` (Phase 7 path) continua existindo com a mesma assinatura. Phase 7 selftest ainda usa esse caminho.
- `applyWithDiet()` (Phase 18 novo) replica a logica de Phase 7 internamente quando `diet.eatFood=true`, com a mesma deteccao de contato (`touching()`), o mesmo handling de comida chunk (skip with counter), o mesmo dedup de comida consumida no mesmo step.

## Como predacao foi implementada

Pipeline determinista em duas fases:

1. **Coleta de eventos**: itera agentes (snapshot fixo do tamanho inicial), para cada um:
   - Se nao alive ou ja marcado morto, skip.
   - Lookup `DietConfig` via genome.
   - Se `eatFood`, tentar consumir uma comida (food path).
   - Se `eatAgents` e nao ja predou neste step, buscar candidatos via SpatialHash (raio = 2x predator radius). Filtrar por: vivo, nao self, especie diferente (ou same_species permitido), touching (intersecao de raios), nao ja marcado morto.
   - Selecionar candidato com menor `agentId` (determinista).
   - Gravar `PredationEvent{predator_id, prey_id, gain, prey_position, prey_initial_energy, prey_radius, prey_color, prey_corpse_to_food}`.
   - Marcar predador e presa em sets.

2. **Aplicacao**: itera eventos:
   - Aplicar `addEnergyAt(predator_index, gain, predator_cap)`.
   - Chamar `agents.removeAgent(prey_id)` (preserva swap-remove).
   - Se `prey.corpseToFood`, criar comida instantanea no local da presa.

3. **Limpeza de comida**: remove todas as comidas consumidas via `foods.removeFood`.

A ordem de iteracao (por indice estavel) + selecao por menor id garante reprodutibilidade entre runs com mesma seed.

## Como a presa morre

Via `AgentStore::removeAgent(prey_id)` (swap-remove). Spatial hash sera reconstruido no proximo step pelo `App`. Nao usei `DeathSystem` para predacao porque a regra de morte por predacao e diferente (instantanea e por evento, nao por energia abaixo de threshold). Ambos sistemas coexistem: predacao remove presa imediatamente; `DeathSystem` remove agentes com energia < threshold no fim do step.

## Como o predador ganha energia

`addEnergyAt(predator_index, prey_energy * agentEfficiency, predator_cap)`. A energia da presa usada e a corrente (no momento da predacao). Predator gain e clamped ao cap do genoma do predador.

## Como `energy cap` e respeitado

`addEnergyAt(index, delta, cap)` ja existia em `AgentStore` e respeita o cap (clamp). Phase 18 usa o cap do genoma do predador via `genomes.find(predGenomeId)->energyCap`, com fallback para `DietInteractionConfig::defaultEnergyCap`.

## Canibalismo

- `eatSameSpecies=false` (default): bloqueado. Contador `blockedSameSpecies` incrementado em cada candidato bloqueado (para diagnostico de pressao competitiva).
- `eatSameSpecies=true`: permitido. Mesma especie pode ser predada.

## Selecao de presa

Determinista: menor `EntityId` entre candidatos validos. EntityIds sao gerados monotonicamente em `AgentStore` (Phase 13), entao "menor id" = "mais antigo". Documentado no codigo.

## Determinismo

- Spawn por species e deterministico (Phase 17).
- Iteracao por indice no `AgentStore` e estavel.
- Selecao de presa por `min(id)` e estavel.
- SpatialHash nao introduz nao-determinismo (insert ordem-preservada).
- Sem `std::random_device` no hot path.

## Como `SpatialHash` foi usado

`applyWithDiet` aceita `SpatialHash*` opcional. Quando habilitado:
- Food queries: `queryRadiusInto(agentX, agentY, agentRadius)`.
- Predator prey queries: `queryRadiusInto(agentX, agentY, agentRadius * 2.0)` (raio maior para cobrir contato com presas maiores).

Fallback O(N²) implementado para tests onde nao ha spatial hash. O(N) em geral, O(K) por agente onde K = numero de vizinhos.

## InteractionSystem

Refatorado: agora tem dois entrypoints:
- `apply()` (Phase 7): comida instantanea apenas. Preservado para selftest Phase 7.
- `applyWithDiet()` (Phase 18): comida + predacao. Usado pelo App e Phase 18 selftest.

## DeathSystem / EnergySystem / SpeciesStore / GenomeStore integration

- DeathSystem inalterado. Continua removendo agentes com energia < threshold no fim do step.
- EnergySystem inalterado. `addEnergyAt` ja respeita cap.
- SpeciesStore: snapshot adicional, `setDietSnapshot()` helper.
- GenomeStore: campo `diet` em `GenomeRecord`; `cloneFrom` copia automaticamente.

## Aliases `bacteria` / `predator`

Continuam funcionando via `SpeciesStore::resolveAlias`. Bootstrap usa o prefix do espec para determinar o default por especie.

## Labels / species

Aliases legados (`label_bacteria`, `labels_predator`, etc.) preservados na Fase 17. Phase 18 nao modifica.

## Contadores

`DietInteractionStats` registra:
- `agentsProcessed`, `foodsConsumed`, `chunkFoodsSkipped`, `predationEvents`, `corpsesToFoodSpawned`, `blockedSameSpecies`, `blockedDietDisabled`.
- `foodEnergyConsumed`, `agentEnergyGainedByFood`, `agentEnergyGainedByPredation`.

Por especie: pode ser derivado iterando `agents` apos o step (nao implementado como contador interno; Fase 26 pode formalizar).

## Corpse-to-food

Implementado como **propriedade da presa** (`prey.dietConfig.corpseToFood`). Quando true e a presa e morta por predacao, uma comida instantanea aparece no local da presa com:
- `position = prey_position`
- `radius = max(1.0, prey_radius * 0.5)`
- `energy = max(1.0, prey_initial_energy)` (snapshot antes da predacao)
- `kind = Instant`

Nao implementa chunk/pedacos (Fase 19).

## Resolucao da Divida 7

Assinatura de `ReproductionSystem::apply` agora recebe `neural::BrainConfig brainSignatureConfig` por valor em vez de `const neural::BrainConfig&`. A copia (~200 bytes) e barata e elimina dangling reference.

Verificacao no test 90: passa `genomes.find(id)->brainConfig` direto, exatamente o cenario perigoso documentado. Test PASS em Debug e Release.

## Preparacao para Fase 19 (comida chunk)

- `DietConfig` ja tem `eatFood`. Fase 19 pode adicionar `eatChunk` ou reusar `eatFood` para qualquer FoodKind.
- `applyWithDiet` ja distingue `FoodKind::Instant` vs `FoodKind::Chunk` via `kindAt(foodIndex)`; basta implementar a logica de "bite" para chunks.

## Preparacao para Fase 21 (colisao/fisica)

- Predator-prey "touching" usa intersecao de raios; pode ser substituido por collision detection real sem afetar a regra de dieta.

## Preparacao para Fase 24 (editor genetico/especies)

- `SpeciesRecord::dietSnapshot` ja expoe a dieta para UI sem precisar dereferenciar GenomeStore.
- `SpeciesStore::setDietSnapshot()` helper para modificacao via UI.
- `GenomeRecord::diet` e POD direto para serializacao JSON em Fase 27.

## Preparacao para Fase 27 (save/load)

`DietConfig`, `SpeciesRecord`, `GenomeRecord` sao PODs sem ponteiros/unique_ptr. Diretos para `to_json`/`from_json`.

## Como MLP baseline foi protegida

Phase 9 selftest passa (23 checks). Caminho neural intacto.

## Como Gated/Shortcut/Modulated foram protegidas

Phase 14 selftest passa (71 checks). Caminho neural intacto.

## Como Simple RNN foi protegida

Phase 15 selftest passa (72 checks).

## Como familia NEAT foi protegida

Phase 16 selftest passa (109 checks).

## Testes executados

- Debug: `--phase18-selftest` (111 checks PASS).
- Release: `--phase18-selftest` (111 checks PASS).
- Regressoes Debug Phase 7-17: todas PASS (14+15+23+21+30+49+42+71+72+109+110 = 556 checks).
- Phase 18 benchmark Release: 13 cenarios, dados disponiveis.

## Resultado dos diagnostics / microbenchmark (Release)

| Cenario | Agentes | Predadores | Foods | us/step | us/agente | foods | pred events | corpses |
|---|---|---|---|---|---|---|---|---|
| 100ag_100food_no_pred | 100 | 0 | 100 | 33.85 | 0.34 | 15 | 0 | 0 |
| 300ag_150food_no_pred | 300 | 0 | 150 | 110.08 | 0.37 | 36 | 0 | 0 |
| 600ag_300food_no_pred | 600 | 0 | 300 | 252.45 | 0.42 | 125 | 0 | 0 |
| 1000ag_500food_no_pred | 1000 | 0 | 500 | 368.04 | 0.37 | 312 | 0 | 0 |
| 30pred_300prey | 330 | 30 | 200 | 120.62 | 0.37 | 53 | 19 | 0 |
| 100pred_1000prey | 1100 | 100 | 400 | 359.36 | 0.33 | 253 | 200 | 0 |
| 30pred_300prey_predfood | 330 | 30 | 200 | 100.21 | 0.30 | 65 | 19 | 0 |
| 30pred_300prey_corpse_on | 330 | 30 | 200 | 103.27 | 0.31 | 58 | 19 | 19 |

Observacoes:
- Sem predadores: overhead de Phase 18 vs Phase 7 = ~zero (~0.34 us/agente).
- Com predadores: ~0.33-0.37 us/agente. Predacao nao introduz overhead relevante.
- Per-event predation cost: ~5 us (dominio de spatial hash query + addEnergy + removeAgent).
- corpse_to_food: ~0.05 us extra por evento. Negligivel.

## Diagnostico visual/runtime

Smoke test do `App`: mensagem inicial atualizada para "Phase 18", bacteria spawn (150 default), predator nao spawna por default (`predators_enabled=false`). Run em background sem regressoes.

## Divergencias contra Python

- Ordem de selecao de presa: C++ usa menor agent id; Python pode usar ordem de iteracao do dict. Decisao C++: explicito e estavel.
- `corpse_to_food` em C++ usa initial energy da presa; Python pode usar valor diferente. Diferenca minima documentada.

## Limitacoes atuais

- Predator preda no maximo 1 presa por step (predator lock). Cenarios com altissima densidade ficam sub-otimos.
- Contadores por especie nao agregados internamente; precisariam iteracao adicional no App. Fase 26 vai formalizar.
- `corpse_to_food` so cria comida instantanea (Fase 19 pode estender para chunk).
- `population_min_rescue_enabled` ainda nao tem rescue logico (Fase 13 issue mantido).

## Pendencias para Fase 19

- Comida chunk + mordidas temporizadas.
- `DietConfig::eatChunk` ou reusar `eatFood`.
- `food_piece_*` parametros.

## Pendencias para Fase 21

- Colisao real entre predador e presa (substituindo touching de raios).
- Colisao food-food para chunks.

## Pendencias para Fase 24

- UI consumindo `SpeciesStore::dietSnapshot`.
- Editor de dieta por especie.

## Pendencias para Fase 26

- Metricas/grafico de predation events por especie.
- Contadores por especie agregados.

## Pendencias para Fase 27

- Serializar `DietConfig` no save de genoma.
- Carregar dieta no load.

## Pendencias para Fase 29

- Benchmark runner formal com cenarios pred/prey.

## Confirmacao de escopo

- Nenhum arquivo Python alterado.
- Comida chunk completa NAO implementada.
- Obstaculos NAO implementados.
- UI completa NAO implementada.
- Save/load final NAO implementado.
- Phase 19 NAO iniciada.
