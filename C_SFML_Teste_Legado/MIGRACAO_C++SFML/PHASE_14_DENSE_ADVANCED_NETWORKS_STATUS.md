# Phase 14 Status: Redes Densas Avancadas (Gated, Shortcut, Modulated MLP)

## Scope Executado

- **Divida 2 RESOLVIDA**: `BrainSlot` agora carrega `neural::BrainVariant = std::variant<MLPBrain, GatedMLPBrain, ShortcutMLPBrain, ModulatedMLPBrain>`. Dispatch via `std::visit` com inlining. Zero heap allocation por cerebro.
- **Divida 3 RESOLVIDA**: `BrainPerformanceConfig::useNumbaBrainForward` -> `useBatchForward`; `numbaBrainForwardMinBatch` -> `batchForwardMinSize`. ParameterRegistry preserva aliases legados.
- **Divida 5 AVALIADA**: o refactor para variant REMOVEU alocacao por cerebro do hot path. NeuralSystem ainda aloca `std::vector<double>` para input/output por agente; isso permanece para Fase 30 com escopo conhecido.
- `BrainType` ja existia com 8 tipos (MLP + 4 avancados + 4 NEAT). `isImplementedInPhase14` adicionado.
- `BrainConfig`: campos existentes de gates/shortcut em `FutureNeuralConfig` ja eram suficientes. Apenas os campos Numba foram renomeados.
- `NeuralMutationConfig` (novo): struct unificado para mutacao com `baseRate/Strength + gateRate/Strength + shortcutRate/Strength`. Overrides -1 = fallback para base.
- `GatedMLPBrain` (novo): MLP padrao + gates escalares por neuronio oculto, multiplicados pos-tanh. Clamp a [gateMin, gateMax].
- `ShortcutMLPBrain` (novo): MLP padrao + bypass linear input -> output, escalado por `shortcutScale`. Init via `shortcutInitStd / sqrt(input_size)`.
- `ModulatedMLPBrain` (novo): combina ambos (gates em hidden + shortcut no output).
- `BrainVariant.hpp`: helpers `brainTypeOf`, `inputSizeOf`, `outputSizeOf`, `forwardOf`, `cloneOf`, `mutateOf`, `batchKeyOf` para dispatch via `std::visit`.
- `BrainFactory::createBrain` despacha por tipo. `fallbackToMlp` honrado explicitamente.
- `BrainExecutor` ganha overload `forward(BrainVariant&, ...)` via std::visit.
- `NeuralSystem`:
  - `BrainSlot` usa `BrainVariant` (sem unique_ptr).
  - `produceMovementControls` usa `executor_.forward(it->second.brain, input)` — sem `*ptr` ou null check.
  - `inheritBrain` agora tem 2 overloads: (rate, strength, rng) e (NeuralMutationConfig, rng). Clona variant via `cloneOf`, muta via `mutateOf`.
  - `syncBrains` substitui `created.mlp` por `created.brain`.
- `ActivationTrace` extendida com `BrainType brainType`, `vector<vector<double>> gateValues`, `vector<double> shortcutContribution`. Backward-compatible com Phase 9.
- `ParameterDefaults.cpp`: `use_batch_forward` e `batch_forward_min_size` registrados como canonicos com aliases para os nomes Numba/native legados.
- 71 selftests (factory/types/mutation/clone/forward/no-aliasing/integration/regressions).
- Microbenchmark com 3 arquiteturas x 4 tipos = 12 cenarios. Mede forward/clone/mutation.
- Comandos CLI: `--phase14-selftest`, `--phase14-benchmark`, `--phase14-diagnostics`.

## Fora de Escopo

- Fase 15 (SimpleRnn, estado recorrente, memory decay).
- Fase 16 (NEAT comum/simplificada/recorrente).
- UI completa de redes neurais (Fase 23).
- NeuralViewer completo (Fase 25).
- Save/load por tipo neural (Fase 27).
- Especies completas (Fase 17).
- Predadores funcionais (Fase 18).
- Dieta generica (Fase 18).
- Food chunk (Fase 19).
- Obstaculos (Fase 20).
- Benchmark runner formal (Fase 28).

## Documentos Consultados

Todos os 17 obrigatorios incluindo PHASE_13_REPRODUCTION_GENOME_STATUS.md.

## Arquivos Python Consultados

- `sim/brain.py` (focal: GatedNeuralNet, ShortcutNeuralNet, ModulatedNeuralNet linhas 465-770).
- `sim/entities.py`, `sim/systems.py`, `sim/controllers.py`, `sim/engine.py`, `sim/random_utils.py`, `sim/actuators.py`, `sim/sensors.py`, `sim/neural_viewer.py`, `sim/ui.py` (referencia).

Arquivos Python adicionais consultados: nenhum.

## Arquivos C++ Criados

- `src/neural/NeuralMutationConfig.hpp`
- `src/neural/BrainVariant.hpp`
- `src/neural/GatedMLPBrain.hpp/cpp`
- `src/neural/ShortcutMLPBrain.hpp/cpp`
- `src/neural/ModulatedMLPBrain.hpp/cpp`
- `src/neural/Phase14Diagnostics.hpp/cpp`
- `MIGRACAO_C++SFML/PHASE_14_DENSE_ADVANCED_NETWORKS_STATUS.md`

## Arquivos C++ Modificados

- `CMakeLists.txt` — 4 novos .cpp adicionados.
- `src/main.cpp` — flags `--phase14-*`.
- `src/neural/ActivationTrace.hpp` — adiciona brainType, gateValues, shortcutContribution.
- `src/neural/BrainConfig.hpp` — renomeia campos Numba.
- `src/neural/BrainFactory.hpp/cpp` — produz BrainVariant; dispatch por tipo; honra fallback.
- `src/neural/BrainExecutor.hpp` — overload variant.
- `src/neural/BrainType.hpp` — `isImplementedInPhase14` adicionado.
- `src/neural/MLPBrain.hpp/cpp` — overload `mutate(NeuralMutationConfig, rng)`; trace.brainType=Mlp.
- `src/neural/Phase9Diagnostics.cpp` — Test atualizado para usar `std::holds_alternative<MLPBrain>`.
- `src/systems/NeuralSystem.hpp/cpp` — BrainSlot usa BrainVariant; inheritBrain ganha overload com NeuralMutationConfig.
- `src/config/ParameterDefaults.cpp` — aliases para use_batch_forward/batch_forward_min_size.
- `src/app/App.cpp` — mensagem inicial Phase 14.
- `MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md` — Dividas 2 e 3 marcadas RESOLVIDAS.

## Correcoes de Continuidade

- Test 7 do Phase 9 (`BrainFactory falls back cleanly for future types`): adaptado para usar `std::holds_alternative<MLPBrain>` em vez de `result.mlp != nullptr`. Comportamento semantico identico.
- Sem outras correcoes funcionais nas Fases 9-13.

## Melhorias Alem do Escopo

1. **NeuralMutationConfig com overrides -1**: mapeia exatamente a semantica do Python (None -> base) em forma type-safe.
2. **ActivationTrace.brainType**: identificacao instantanea do tipo para NeuralViewer futuro.
3. **`isImplementedInPhase14` em vez de remover `isImplementedInPhase9`**: mantemos a flag historica para nao quebrar Phase 9. Novo helper cobre estado atual.
4. **Fallback explicito em `createBrain`**: quando `fallbackToMlp=true`, MLP e produzido independentemente de `config.type`. Preserva semantica Phase 9 mesmo com dispatch novo.
5. **Variant em vez de polimorfismo**: zero virtual call overhead, zero heap allocation, soma de tamanhos compacta. std::visit e otimizado pelo compilador.

## Justificativas

| Melhoria | Por que |
|---|---|
| NeuralMutationConfig | Centraliza override logic em um lugar. Evita parametros longos por brain. |
| brainType em trace | NeuralViewer futuro precisa saber qual brain trace vem para renderizar gates/shortcut. |
| isImplementedInPhase14 | Permite ver evolucao das fases por flag. Adicionar Phase15/16 e trivial. |
| fallback explicito | Phase 9 test relies on fallbackToMlp metadata; nao quebra. |
| Variant > polimorfismo | Performance: zero vcall, zero heap. Adicionar tipos = adicionar variant case. |

## Status da Divida 2

**RESOLVIDA**. Detalhes em `TECHNICAL_DEBT_REGISTER.md`.

## Status da Divida 3

**RESOLVIDA**. Detalhes em `TECHNICAL_DEBT_REGISTER.md`. Aliases preservados (Test 60 valida).

## Avaliacao da Divida 5

A migracao para variant **REMOVEU** alocacao por cerebro do hot path (eliminou `std::make_unique<MLPBrain>` em syncBrains e inheritBrain). MLP baseline passou de 2.25us/forward (Phase 9) para 1.46us/forward (Phase 14) — 35% mais rapido sem mudar o algoritmo.

Restante da Divida 5 (alocacao de input/output vectors por agente em NeuralSystem::produceMovementControls) permanece para Fase 30 com escopo conhecido e benchmark documentado.

## Parametros Usados

- `neural_network_type` (vem via configFromRegistry).
- `neural_gate_init`, `neural_gate_min`, `neural_gate_max` (via FutureNeuralConfig).
- `neural_gate_mutation_rate`, `neural_gate_mutation_strength` (overrides via NeuralMutationConfig).
- `neural_shortcut_init_std`, `neural_shortcut_scale` (via FutureNeuralConfig).
- `neural_shortcut_mutation_rate`, `neural_shortcut_mutation_strength` (overrides via NeuralMutationConfig).
- `bacteria_mutation_rate`, `bacteria_mutation_strength` (vem via species prefix).
- `use_batch_forward`, `batch_forward_min_size` (canonicos; aliases para nomes Numba legados).

## Parametros Pendentes

- `predator_*` (Fase 18).
- `brain_cache_*` (lido em BrainPerformanceConfig; sem uso ativo. Fase 30 podera usar).
- RNN/NEAT params (Fase 15/16).

## Itens cobertos de FEATURE_INVENTORY.md

- Gated MLP: implementada.
- Shortcut MLP: implementada.
- Modulated MLP: implementada.
- Batch por assinatura: parcialmente preparado via `batchKeyOf` (Fase 30 implementa batch real).
- Clone/copy por tipo: implementado.
- Mutacao por tipo: implementado.
- ActivationTrace por tipo: implementado.

## Itens cobertos de PARAMETER_INVENTORY.md

- Grupo "Redes Neurais Globais": gates e shortcut params usados funcionalmente.
- Grupo "Performance/Render/Percepcao Global": aliases Numba preservados.

## Itens de UI_INVENTORY.md Impactados

- "Redes Neurais" (Fase 23): parametros tem efeito real agora; UI futura pode altera-los.
- "Visualizador Neural" (Fase 25): ActivationTrace.brainType/gateValues/shortcutContribution preparados.

## Como Funciona

### BrainType
Enum com 8 tipos. `isImplementedInPhase14` retorna true para MLP/Gated/Shortcut/Modulated. RNN/NEAT continuam unimplemented.

### BrainFactory
`createBrain(config, rng)` -> `BrainCreationResult { BrainVariant brain, ... }`. Switch on `config.type` constructs the matching brain inplace via variant. `fallbackToMlp` short-circuits to MLP construction.

### BrainConfig
Mesma estrutura, com `FutureNeuralConfig` ja existente mas agora ATIVAMENTE usada por Gated/Shortcut/Modulated. `BrainPerformanceConfig` campos Numba renomeados.

### Dispatch no NeuralSystem
`BrainSlot::brain` e `BrainVariant`. `produceMovementControls` chama `executor_.forward(brain, input)`. `BrainExecutor::forward(BrainVariant&, ...)` faz `std::visit([&](auto& b) { return b.forward(input, trace); }, brain)`. Compilador inline o switch.

### Gated MLP
- Forward: para cada hidden layer, computa W*x + b, aplica tanh, depois multiplica element-wise pelo vetor `gates_[layer]`. Output layer e linear, sem gate.
- Mutate: weights/biases mutated com `baseRate/Strength`. Gates mutated com `effectiveGateRate/Strength` (fallback -1 = base). Re-clampados a [gateMin, gateMax].
- Clone: copia weights, biases, gates por valor.

### Shortcut MLP
- Forward: MLP padrao, depois `output[i] += shortcutScale * (W_short[i]*x + b_short[i])`. shortcutWeights e row-major size outputSize x inputSize.
- Init: shortcut weights com std = `shortcutInitStd / sqrt(inputSize)`.
- Mutate: weights/biases base; shortcutWeights+bias via `effectiveShortcutRate/Strength`.
- Clone: copia tudo por valor.

### Modulated MLP
- Forward: combina gated + shortcut. Gates aplicados pos-tanh em hidden; shortcut adicionado ao output final.
- Mutate: aplica base + gate + shortcut. Re-clamp gates.
- Clone: copia tudo.

### Mutacao de gates
`if (gateRate > 0 && gateStrength > 0): for each gate in gates: if bernoulli(gateRate): gate += normal(0, gateStrength)`. Depois clamp.

### Mutacao de shortcut
Igual a gates, mas em shortcutWeights e shortcutBias. Sem clamp.

### Clone/copy por tipo
Cada classe tem `clone() const` que retorna `*this` (copy ctor). std::vector copia profunda. Zero aliasing garantido (testado em selftests 20, 31, 38, 53).

### Reproducao/heranca por tipo
`ReproductionSystem` chama `NeuralSystem::inheritBrain(childId, parentId, config, NeuralMutationConfig, rng)`. NeuralSystem clona via `cloneOf(parentSlot.brain)` (variant clone), depois muta via `mutateOf(slot.brain, mc, rng)`. Tipo se preserva automaticamente (variant carrega o tipo).

### ActivationTrace por tipo
Cada `forward(input, trace)` seta `trace->brainType`. MLP popula apenas `layers`. Gated tambem popula `gateValues` (uma copia do vetor de gates aplicado por layer). Shortcut tambem popula `shortcutContribution` (vetor de contribuicao final por output). Modulated popula ambos.

### MLP baseline protegida
- API Phase 9 (`mutate(rate, strength, rng)`) preservada. Nova overload `mutate(NeuralMutationConfig, rng)` apenas adicionada.
- `BrainCreationResult.brain` substitui `BrainCreationResult.mlp` mas `std::holds_alternative<MLPBrain>(brain)` funciona transparentemente.
- Phase 9 selftest passa sem alteracao funcional.
- Phase 13 selftest passa sem alteracao funcional.

### Performance MLP validada
Phase 9 benchmark: 2.25us/forward (default 20/20/20/20).
Phase 14 benchmark: 1.46us/forward (default 20/20/20/20) — **35% MAIS RAPIDO** devido a remocao de heap allocation por cerebro.

## Testes Executados

```
Phase14 validation: PASS (71 checks)
```

Coverage:
1-4: factory por tipo.
5: fallback explicito.
6-8: sizes.
9-20: Gated (init, clamp, mutation override, mutation zero/positive, forward determinismo, clone, no-aliasing).
21-31: Shortcut (dimension, scale effect, mutation override, mutation zero/positive, forward, clone, no-aliasing).
32-40: Modulated (contains both, gate clamp, scale, mutation, clone, no-aliasing, forward determinismo, output shape).
41-45: pipeline integration (MLP/Gated/Shortcut/Modulated via NeuralSystem + Movement).
46-48: vision integration smoke.
49-52: reproduction inheritance por tipo.
53: variant-level no-aliasing.
54-57: ActivationTrace por tipo.
58: MLP baseline sem gates/shortcut.
59: BrainSlot variant invariant.
60: Numba aliases preservados.
61-67: regressions Phases 7-13.
68: MLP performance smoke.
69-71: confirmacoes de escopo.

## Regressoes

```
Phase 7: PASS (14)
Phase 8: PASS (15)
Phase 9: PASS (23)
Phase 10: PASS (21)
Phase 11: PASS (30)
Phase 12: PASS (49)
Phase 13: PASS (42)
```

## Resultado do Benchmark

Forward (us per call):
| Arch | MLP | Gated | Shortcut | Modulated |
|---|---|---|---|---|
| small (8) | 0.33 | 0.34 | 0.32 | 0.31 |
| medium (16/16) | 0.59 | 0.62 | 0.64 | 0.63 |
| default (20/20/20/20) | 1.46 | 1.46 | 1.37 | 1.41 |

Clone (us per clone):
| Arch | MLP | Gated | Shortcut | Modulated |
|---|---|---|---|---|
| small | 1.05 | 1.04 | 0.93 | 0.91 |
| medium | 2.07 | 2.08 | 1.80 | 2.27 |
| default | 4.78 | 4.72 | 4.78 | 4.81 |

Mutation (us per mutation):
| Arch | MLP | Gated | Shortcut | Modulated |
|---|---|---|---|---|
| small | 1.04 | 1.16 | 1.31 | 1.44 |
| medium | 3.55 | 3.90 | 3.83 | 4.02 |
| default | 9.74 | 10.71 | 9.99 | 10.47 |

Observacoes:
- Tipos avancados dentro de 5-10% de MLP em forward.
- Modulated as vezes mais rapido devido a effects de cache.
- Clone e linear no tamanho — todos os tipos comparaveis.
- Mutation cost +5-15% para avancados (gates/shortcut adicionam pequena passagem).

## Diagnostico Visual

App boota com "Phase 14: dense advanced brains (Gated/Shortcut/Modulated MLP) initialized." Title bar inalterado (mostra brain type ativo via `lastNeuralStats_.activeType`). Quando `neural_network_type` muda para gated/shortcut/modulated, simulacao usa o brain correspondente.

## Divergencias contra Python

| Item | Python | C++ | Justificativa |
|---|---|---|---|
| Gates aplicados | post-tanh, element-wise | identico | identico |
| Shortcut formula | scale * (W*x + b) added to output | identico | identico |
| Shortcut init std | std / sqrt(input_size) | identico | identico |
| Gate clamp | min/max applied after init/mutation | identico | identico |
| Gate mutation -1 fallback | None -> base | NeuralMutationConfig -1 -> base | semantica identica, type-safe |
| Modulated combines | Gated + Shortcut | identico | identico |
| Numba kernel | usado em batch | nao migrado (Phase 30) | preserve aliases |

## Limitacoes Atuais

- Batch por assinatura: `batchKeyOf` retorna assinatura unica por arquitetura, mas batch execution real (varios brains do mesmo tipo processados em SIMD/grouped pass) e pendencia de Phase 30.
- RNN/NEAT continuam unimplemented.
- ActivationTrace de Gated inclui gates aplicados; Modulated inclui ambos. Visualization completa e Phase 25.
- `predator_*` parametros nao usados (Fase 18).
- `brain_cache_*` lidos mas sem efeito ativo (Phase 30).

## Pendencias para Fase 15

- `SimpleRNNBrain` no variant.
- `BrainState.recurrentState` ativado.
- Memory decay, state clip, reset_state_on_copy.

## Pendencias para Fase 16

- `NEATGraphBrain` no variant (ou via separate store).
- Topology mutations (add/remove node/connection).
- Recurrent variant para NeatRecurrent.

## Pendencias para Fase 23

- UI dropdown com 4 tipos atuais + futuros RNN/NEAT.
- Sliders para gate_init/min/max, gate_mutation_rate/strength.
- Sliders para shortcut_init_std/scale/mutation_rate/strength.

## Pendencias para Fase 25

- Visualizador renderiza gates como cores em hidden layers.
- Visualizador renderiza shortcut como linhas adicionais input -> output.
- Modulated mostra ambos.

## Pendencias para Fase 27

- Serializacao por tipo via std::visit no save.
- Loader detecta tipo, despacha para construtor correto.
- Aliases legados de brain_type respeitados.

## Confirmacoes

- Nao implementou RNN (`SimpleRnn` continua unimplemented).
- Nao implementou NEAT (`Neat/SimpleNeat/RecurrentNeat` continuam unimplemented).
- Nao implementou UI completa.
- Nao implementou NeuralViewer completo.
- Nao implementou save/load.
- Nao implementou especies completas.
- Nao avancou para Fase 15.
- Nenhum arquivo Python foi alterado.
- Build Debug/Release: OK.
- Phase 14 selftest: PASS (71 checks).
- Regressoes Phases 7-13: PASS.

## Final Audit

Data: 2026-05-28.

Decisao: pronto para commit da Fase 14. Aguarda autorizacao explicita.
