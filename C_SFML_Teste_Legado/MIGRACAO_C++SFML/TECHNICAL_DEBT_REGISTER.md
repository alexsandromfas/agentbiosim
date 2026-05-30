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

## Divida 4 — App acumulando responsabilidades [RESOLVIDA NA FASE 22, REFINADA NA MICROFASE 22.1]

Status: **RESOLVIDA** (commit da Fase 22, 2026-05-30; refinada na microfase 22.1, 2026-05-30).

Refinamento na microfase 22.1:
- `App::handleResize` agora chama `window_.setView(sf::View(sf::FloatRect(0, 0, w, h)))` antes do refit da camera, corrigindo o bug "clique sai do lugar ao maximizar".
- `App::drainCommandsAndApply` reconhece 9 comandos novos da UI (`CmdNewSimulation`, `CmdQuitApp`, `CmdTogglePreferencesPanel`, `CmdToggleAboutPanel`, `CmdToggleGenomePanel`, `CmdResetCamera`, `CmdCloseAllMenus`, `CmdPaintObstacleStroke`, `CmdEraseObstacleStroke`).
- Continua sendo o AppController fino — agora com responsabilidades claras de antialiasing, viewport e dispatch de UI vs engine.

Resolucao:
- Extraido `sim::SimulationRunner` (`src/sim/SimulationRunner.hpp/.cpp`) que possui todos os stores (AgentStore, FoodStore, ObstacleStore, SpeciesStore, GenomeStore) e todos os sistemas (perception, neural, movement, collision, energy, spatialhash, interaction, food, reproduction, death). Expoe `initialize()`, `step(dt)`, `setPaused/togglePaused/requestStepOnce/reset`, `applyCommand(Command)`, mais acessores const/mutaveis e operacoes de spawn/delete/pick/rect/lasso necessarias para a UI. Determinismo preservado via `seed_` configurado pelo registry (`random_seed`).
- Extraido `ui::InputRouter` (`src/ui/InputRouter.hpp/.cpp`) que traduz `sf::Event` para `ui::Command` sem mutar stores diretamente.
- Extraido `ui::UiPanel` (`src/ui/UiPanel.hpp/.cpp`) que desenha menu bar, toolbar com 10 ferramentas e overlay de ajuda em SFML puro.
- `App` agora e o AppController fino: possui `parameters_`, `runner_`, `timestep_`, `camera_`, `renderer_`, `window_`, `uiState_`, `inputRouter_`, `uiPanel_`, `commandQueue_`. `processEvents` roteia UI -> InputRouter; `update` faz `drainCommandsAndApply()` (camera/selecao/UI locais + `runner_.applyCommand` para comandos de engine) e depois loop de timestep fixo chamando `runner_.step(dt)`. `render` chama `renderer_.render(...)` + `uiPanel_.draw(...)`.

Verificacao:
- Build Debug/Release: OK.
- `--phase22-selftest`: PASS (155 checks).
- Regressoes `--phase7-selftest` ate `--phase21-selftest`: PASS (todos).
- Microbenchmark `--phase22-benchmark`: avg_step_us na faixa de 700-845us para 152-163 agentes / 50 comidas / 0 obstaculos, sem regressao significativa em relacao a Fase 21.

Historico original:

Descricao:
A classe `App` orquestrava criacao da janela, eventos, camera, spawn demo, configuracao de parametros, simulation step, chamada de todos os sistemas, rebuild do spatial hash, renderizacao e titulo. Tendia a crescer com a chegada da UI.

Acao recomendada (cumprida):
- `SimulationRunner`: executa steps, gerencia sistemas e stores.
- `AppController` (papel do `App` renovado): liga janela, UI, input e renderer ao runner.
- `InputRouter`: traduz eventos SFML em comandos para o engine.

Arquivos afetados:
- `src/app/App.hpp`, `src/app/App.cpp` (refatorados, finos).
- `src/sim/SimulationRunner.hpp/.cpp` (novo).
- `src/ui/InputRouter.hpp/.cpp` (novo).
- `src/ui/UiPanel.hpp/.cpp` (novo).
- `src/ui/Command.hpp`, `src/ui/CanvasTool.hpp`, `src/ui/SelectionState.hpp`, `src/ui/UiState.hpp` (novos).

## Divida 5 — Alocacoes temporarias no NeuralSystem [PARCIALMENTE MITIGADA]

Status: parcialmente mitigada na Fase 14 (variant removeu heap alloc por cerebro). Avaliada na Fase 15 (RNN nao adiciona alocacao significativa). Reavaliada na Fase 16 (NEAT adiciona maps temporarios per-forward). Restante para Fase 30.

Avaliacao na Fase 16 (NEAT family):
- NEAT cria por forward: dois `std::unordered_map<int32_t, double>` (`values`, `incoming`), uma `std::map<double, vector<Node*>>` (hidden_by_layer), uma `std::unordered_map` para `newState` (somente recorrente). Isso e ~4 estruturas dinamicas alocadas por chamada.
- Custo medido em benchmark: NEAT minimal (~36 conexoes) em ~2.6-3.3 us/forward, equivalente a MLP simples; NEAT layered (~432 conexoes) em ~17-18 us/forward. Aceitavel para ate ~2000 agentes a 30 Hz com NEAT.
- Para NEAT o caminho otimo nao e batch (`supports_batch=false`), e sim transformar `values`/`incoming` em vetores indexados (denso) ou reusar buffers persistentes por brain entre chamadas. Avaliar em Fase 30 com 1000+ agentes NEAT reais.

Avaliacao na Fase 15:
- RNN adiciona apenas um vetor de estado por agente (state_size doubles), copiado no clone se reset_state_on_copy=false. Custo de reset state e ~7-20ns por agente.
- A copia interna do const forward de SimpleRNNBrain (`SimpleRNNBrain temp = *this`) e o unico ponto novo de heap allocation por chamada. NeuralSystem usa a sobrecarga nao-const (sem copia). A const overload e usada apenas por testes/UI.
- Os vetores input/output por agente em `produceMovementControls` permanecem como pendencia da Fase 30.

Descricao original:
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

## Divida 7 — ReproductionSystem::apply assume que brainSignatureConfig nao alia GenomeStore [RESOLVIDA NA FASE 18]

Status: **RESOLVIDA** (Fase 18, 2026-05-29).

Resolucao:
- Assinatura de `ReproductionSystem::apply()` mudou de `const neural::BrainConfig& brainSignatureConfig` para `neural::BrainConfig brainSignatureConfig` (passagem por valor). A copia (~200 bytes) e o argumento e materializado antes do corpo do metodo executar `cloneFrom()`, removendo a possibilidade de dangling reference.
- Phase18Diagnostics test 90 reproduz explicitamente o cenario perigoso: passa `genomes.find(id)->brainConfig` direto para `apply()` e verifica que a reproducao funciona sem crash em Debug e Release.

Verificacao:
- `--phase18-selftest`: PASS (Debug e Release).
- Regressoes Phase 7-17: PASS.

Historico original:

Descricao:
`ReproductionSystem::apply(..., const neural::BrainConfig& brainSignatureConfig, ...)` recebe a config por const-ref. Internamente chama `genomes.cloneFrom(parentGenomeId)` que pode aceitar relocacao do `std::vector<GenomeRecord>` se a capacidade for excedida. Se o chamador passar uma referencia obtida de `genomes.find(...)`, essa referencia fica DANGLING durante o resto do `apply()` e e usada para ler `brainSignatureConfig.future.*` -> UB.

Detectado durante implementacao do `Phase17Diagnostics`: testes 36 e 41 originalmente passavam `gg->brainConfig` (pointer interno do `g3.records_`) para `apply()`. Debug em MSVC com layout de memoria sem padding ficava sem ser detectado por sorte; Release com optimization expoe o crash. A correcao no teste foi simples: copiar para `const BrainConfig brainCfg = g.get(...).brainConfig;` antes do `apply()`.

Impacto:
- O caminho atual no `App` usa `NeuralSystemConfig nc.brainConfig` que e uma copia, entao `App` esta seguro.
- O caminho seria perigoso se algum codigo futuro decidisse passar `genome->brainConfig` direto para `apply()`.
- Documentacao publica de `apply()` nao explicita esse contrato.

Acao recomendada:
- Trocar a assinatura para receber `neural::BrainConfig brainSignatureConfig` por valor (copia barata, ~200 bytes). Ou
- Documentar no header que `brainSignatureConfig` nao pode aliar `genomes`. Adicionar `[[deprecated]]` se o caller for `genomes.find(...)->brainConfig`.

Momento sugerido:
Fase 18 (predador/dieta) ou Fase 27 (save/load) quando o caminho de reproducao for revisado de qualquer maneira.

Prioridade:
Media. Nao bloqueia nada hoje porque os callers atuais nunca passam ref interna a GenomeStore, mas e uma armadilha latente para autores futuros.

Bloqueia Fase 18?
Nao.

Arquivos afetados:
- `src/systems/ReproductionSystem.hpp` (assinatura).
- `src/systems/ReproductionSystem.cpp` (implementacao).

## Resumo por fase futura impactada

| Divida | Fase recomendada para resolver | Prioridade |
|---|---|---|
| 1. Helpers duplicados | **RESOLVIDA na Fase 13** | Media |
| 2. BrainSlot/MLPBrain concreto | **RESOLVIDA na Fase 14** (variant) | Alta futura |
| 3. Nomes Numba/Python | **RESOLVIDA na Fase 14** (renomeio + aliases) | Baixa/media |
| 4. App acumulando responsabilidades | **RESOLVIDA na Fase 22** (SimulationRunner + InputRouter + UiPanel; mantido limpo na Fase 22.1 e na Fase 23) | Media |
| 5. Alocacoes temporarias neural | Fase 30 ou antes se gargalo medido | Alta futura |
| 6. Version.hpp | Qualquer housekeeping | Baixa |
| 7. ReproductionSystem brainSignatureConfig alias | **RESOLVIDA na Fase 18** (pass-by-value) | Media |

## Regra

Nenhuma divida registrada aqui bloqueia a Fase 10 (sensores e visao single).

Dividas devem ser revisadas antes de iniciar a fase indicada como momento sugerido. Se uma divida for resolvida, registrar a fase e commit que a resolveu neste documento.
