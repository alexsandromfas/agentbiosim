# Registro de Dividas Tecnicas

Este registro acompanha dividas tecnicas identificadas durante a migracao C++/SFML. Cada item deve indicar descricao, impacto, acao recomendada, momento sugerido, prioridade e se bloqueia a proxima fase.

Data de criacao: 2026-05-27 (pos-auditoria da Fase 9).

## Divida 1 — Helpers de parametros duplicados [RESOLVIDA NA FASE 13]

Status: **RESOLVIDA** (commit da Fase 13, 2026-05-28).

Descricao original:
Os helpers `parameterDouble`, `parameterInt`, `parameterBool` e `parameterString` apareciam copiados em pelo menos sete arquivos:

- `src/app/App.cpp`
- `src/systems/MovementSystem.cpp`
- `src/systems/NeuralSystem.cpp`
- `src/systems/EnergySystem.cpp`
- `src/systems/InteractionSystem.cpp`
- `src/systems/DeathSystem.cpp`
- `src/neural/BrainFactory.cpp`

Resolucao:
- `src/config/ParameterHelpers.hpp` ja existia desde a Fase 10 com as funcoes inline em `agentbiosim::config`.
- Na Fase 13, todas as 7 copias locais em namespace anonimo foram removidas. Cada arquivo agora inclui `config/ParameterHelpers.hpp` e usa `using config::parameterDouble;` (etc.) ou chamadas qualificadas `config::parameterDouble(...)`.
- `ReproductionSystem.cpp` (novo) usa o header desde o inicio — nenhuma nova duplicacao.

Verificacao:
- Build Debug/Release: OK.
- Phase 7-13 selftests: PASS.
- Grep por anonymous `parameterDouble` retorna 0 resultados nos arquivos listados.

## Divida 2 — BrainSlot dependente de MLPBrain [RESOLVIDA NA FASE 14]

Status: **RESOLVIDA** (commit da Fase 14, 2026-05-28).

Resolucao:
- `BrainSlot` agora carrega `neural::BrainVariant` (std::variant<MLPBrain, GatedMLPBrain, ShortcutMLPBrain, ModulatedMLPBrain>).
- `BrainCreationResult.mlp` (unique_ptr) substituido por `BrainCreationResult.brain` (BrainVariant).
- Dispatch via `std::visit` (compile-time, inlinavel, zero-cost).
- Sem heap alloc por cerebro — MLP baseline ficou 35% MAIS RAPIDO (1.46us vs 2.25us em Phase 9).
- Futuras RNN/NEAT adicionam novos tipos ao variant sem alterar BrainSlot/NeuralSystem publico.

Verificacao:
- Phase 9 (MLP) selftest: PASS.
- Phase 14 (advanced dense) selftest: PASS (71 checks).
- Phase 14 benchmark: advanced types dentro de 5-10% do MLP; MLP nao regrediu.

## Divida 3 — Nomes herdados de Numba/Python no C++ [RESOLVIDA NA FASE 14]

Status: **RESOLVIDA** (commit da Fase 14, 2026-05-28).

Resolucao:
- `BrainPerformanceConfig::useNumbaBrainForward` -> `useBatchForward`.
- `BrainPerformanceConfig::numbaBrainForwardMinBatch` -> `batchForwardMinSize`.
- `ParameterRegistry`: parametro canonico agora e `use_batch_forward`/`batch_forward_min_size`; aliases legados `use_numba_brain_forward`/`numba_brain_forward_min_batch`/`use_native_brain_forward` mantidos para compatibilidade de saves e codigo Python.
- `BrainFactory::configFromRegistry` agora le do nome canonico.

Verificacao:
- Phase 14 Test 60: confirma alias resolution (registry.find("use_numba_brain_forward") == registry.find("use_batch_forward")).

## Divida 4 — App acumulando responsabilidades

Descricao:
A classe `App` atualmente orquestra:

- criacao e gerenciamento da janela SFML;
- processamento de eventos de input;
- gerenciamento da camera;
- spawn de entidades demo;
- configuracao de parametros;
- execucao do simulation step;
- chamada de todos os sistemas (Neural, Movement, Energy, Interaction, Death);
- rebuild do spatial hash;
- renderizacao;
- atualizacao do titulo da janela com estatisticas.

Isso e aceitavel para as fases iniciais, mas tende a crescer quando UI (Dear ImGui), input avancado, selecao de agentes e ferramentas de canvas forem adicionados.

Impacto:
- Dificuldade de manutenção quando App crescer.
- Mistura de orquestracao, runtime e interface.
- Pode dificultar o modo headless puro se o App estiver acoplado a logica de janela.

Acao recomendada:
Fatorar em componentes como:

- `SimulationRunner`: executa steps, gerencia sistemas e stores.
- `AppController`: liga janela, UI, input e renderer ao runner.
- `InputRouter`: traduz eventos SFML em comandos para o engine.

Momento sugerido:
Antes ou durante a Fase 22 (UI base).

Prioridade:
Media.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/app/App.hpp`
- `src/app/App.cpp`

## Divida 5 — Alocacoes temporarias no NeuralSystem

Descricao:
O metodo `NeuralSystem::produceMovementControls` aloca um `std::vector<double>` de input e recebe um `std::vector<double>` de output para cada agente, a cada step. Com 1000 agentes a 30 steps/s, sao ~60.000 alocacoes/dealocacoes por segundo de vetores pequenos.

O benchmark da Fase 9 mostra ~2.25 us/forward para a arquitetura padrao (4 -> 20x4 -> 2), o que e viavel para tempo real com 1000 agentes. Mas conforme sensores reais (Fase 10+) aumentem o `input_size` e mais agentes sejam suportados, esse padrao se tornara um gargalo mensuravel.

Impacto:
- Custo de alocacao no hot loop.
- Pode limitar escala acima de 2000 agentes.
- Sera mais relevante quando `input_size` crescer com sensores/visao.

Acao recomendada:
- Usar buffers persistentes pre-alocados por assinatura neural.
- Implementar batch forward para redes densas (MLP, Gated, Shortcut, Modulated, RNN).
- Manter fallback individual para NEAT.

Momento sugerido:
Fase 30 (otimizacao data-oriented) ou antes, se benchmarks mostrarem gargalo real na Fase 12/14.

Prioridade:
Alta futura.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/systems/NeuralSystem.cpp` (`produceMovementControls`, `temporaryInputForAgent`).
- Futuro: `BrainExecutor` quando batch for implementado.

## Divida 6 — Version.hpp desatualizado

Descricao:
O arquivo `src/core/Version.hpp` define a versao do executavel. Atualmente pode ainda indicar `v0.1.0-phase1`, apesar do projeto estar apos a Fase 9.

Impacto:
- Confusao em logs, diagnosticos e titulo do executavel.
- Impacto funcional nulo.

Acao recomendada:
Atualizar para `v0.9.0-phase9` ou equivalente. Considerar se o versionamento da migracao deve seguir um esquema formal.

Momento sugerido:
Qualquer fase curta de housekeeping, ou junto com a proxima fase que altere `App`.

Prioridade:
Baixa.

Bloqueia Fase 10?
Nao.

Arquivos afetados:
- `src/core/Version.hpp`

## Resumo por fase futura impactada

| Divida | Fase recomendada para resolver | Prioridade |
|---|---|---|
| 1. Helpers duplicados | **RESOLVIDA na Fase 13** | Media |
| 2. BrainSlot/MLPBrain concreto | **RESOLVIDA na Fase 14** (variant) | Alta futura |
| 3. Nomes Numba/Python | **RESOLVIDA na Fase 14** (renomeio + aliases) | Baixa/media |
| 4. App acumulando responsabilidades | Antes da Fase 22 | Media |
| 5. Alocacoes temporarias neural | Fase 30 ou antes se gargalo medido | Alta futura |
| 6. Version.hpp | Qualquer housekeeping | Baixa |

## Regra

Nenhuma divida registrada aqui bloqueia a Fase 10 (sensores e visao single).

Dividas devem ser revisadas antes de iniciar a fase indicada como momento sugerido. Se uma divida for resolvida, registrar a fase e commit que a resolveu neste documento.
