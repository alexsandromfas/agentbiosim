# Fase 22 — UI Base, Menus e Canvas

Status: **CONCLUIDA** (2026-05-30)

A Fase 22 introduz a base tecnica de UI da migracao C++/SFML: AppController fino, `SimulationRunner` que detem todos os stores/sistemas, `InputRouter` que traduz `sf::Event` em comandos, `CommandQueue` baseada em `std::variant`, `UiState`/`SelectionState`/`CanvasTool`, e `UiPanel` SFML-native com menu bar, toolbar de 10 ferramentas e overlay de ajuda. A Divida Tecnica 4 ("App acumulando responsabilidades") foi RESOLVIDA por essa refatoracao.

## Decisao tecnica sobre UI

Conforme permitido pelo prompt da Fase 22 ("se a integracao for grande/riscada, nao invente gambiarra: implemente uma UI tecnica minima em SFML ou stubs controlados, documente a decisao"), optei por **UI SFML-native** nesta fase em vez de integrar Dear ImGui + ImGui-SFML. Motivacao:

- Dear ImGui e ImGui-SFML adicionariam dependencias externas significativas ao CMake (third_party/imgui, third_party/imgui-sfml), exigiriam build deles separadamente e adicionariam risco de regressao em headless e selftests.
- O alvo desta fase e "base tecnica" e "Resolver Divida 4". A base arquitetural (Command pattern + SimulationRunner + InputRouter) e independente da camada visual da UI; trocar para Dear ImGui mais tarde so substitui a renderizacao do `UiPanel` sem afetar nada abaixo.
- A UI SFML-native cobre todos os elementos exigidos pela Fase 22: menu superior (Arquivo, View, Preferencias, Agente/Genoma, Ajuda), toolbar com 10 ferramentas + Play/Pause/Reset/Fit, overlay de ajuda e status. Toda a infraestrutura de comandos, selecao e ferramentas e testada headless via 155 selftests.

## Escopo executado

- `sim::SimulationRunner` (`src/sim/SimulationRunner.hpp/.cpp`) que possui todos os stores e sistemas; expoe `initialize/step/setPaused/togglePaused/requestStepOnce/reset/applyCommand`, acessores const/mutaveis, `pickAgentAt/agentsInRect/agentsInLasso`, `spawnAgentDefaultAt/spawnFoodAt`, `deleteAgents`.
- `ui::Command` (`src/ui/Command.hpp`) com 28 variantes cobrindo Play/Pause/Reset/Step/SetTimeScale, camera (fit, set center, pan, zoom), ferramentas (set tool, select at point/rect/lasso, clear selection, delete selected, move selected, spawn food/agent, paint/erase/clear obstaculos, clear food) e toggles (spatial hash overlay, selecao, tool, help, simple render, vision debug). `CommandQueue` com `push/drain/clear`.
- `ui::CanvasTool` (`src/ui/CanvasTool.hpp`) com 11 ferramentas: None, Select, RectangleSelect, LassoSelect, AddFood, AddAgent, PaintObstacle, EraseObstacle, Move, Delete, Pan + `canvasToolName/canvasToolLabel`.
- `ui::SelectionState` (`src/ui/SelectionState.hpp`) wrapping `std::vector<EntityId>` com `add/remove/contains/clear/size/empty/ids`.
- `ui::UiState` (`src/ui/UiState.hpp`) com `activeTool`, `selection`, `marquee`, `lasso`, flags de overlay/painel, `timeScale`, `brushRadius`, contadores `commandsProcessed/mouseEvents/keyEvents`, `lastMouseWorld/Valid`.
- `ui::InputRouter` (`src/ui/InputRouter.hpp/.cpp`) que traduz `sf::Event` em comandos sem mutar stores: mouse left por ferramenta, mouse right/middle pan, scroll zoom, teclas (Space, Esc, Delete, R, F, T, V, H, S/Q/L/G/A/M/D/B/X, WASD/setas). Suporta selecao aditiva via Shift/Ctrl.
- `ui::UiPanel` (`src/ui/UiPanel.hpp/.cpp`) SFML-native: menu bar com 5 menus, toolbar com 10 botoes + Play/Reset/Fit + status; overlay de ajuda toggle por H; `handleMouseClick` consome cliques na area do painel.
- `App` refatorado em AppController fino: possui apenas `parameters_`, `runner_`, `timestep_`, `camera_`, `renderer_`, `window_`, `font_`, `uiState_`, `inputRouter_`, `uiPanel_`, `commandQueue_`. `processEvents` roteia UI -> InputRouter; `update` faz `drainCommandsAndApply()` (camera/selecao/UI locais + `runner_.applyCommand`) e loop de timestep fixo chamando `runner_.step(dt)`; `render` chama `renderer_.render(...) + uiPanel_.draw(...)`.
- `Phase22Diagnostics` (`src/systems/Phase22Diagnostics.hpp/.cpp`) com 155 selftests + microbenchmark de 4 cenarios (headless * variacao de comandos por step).
- CLI: `--phase22-selftest`, `--phase22-benchmark`, `--phase22-diagnostics`.
- Determinismo preservado: `SimulationRunner.seed_` configuravel via `random_seed`; nenhum `std::random_device` em hot path.
- Headless preservado: todos os selftests rodam sem janela; `--phase22-selftest` nao abre janela.

## Fora de escopo (confirmado)

- Dear ImGui + ImGui-SFML NAO integrados nesta fase (decisao registrada acima).
- Save/load final (Fase 27) NAO implementado.
- Editor genetico, painel de parametros formal, benchmark runner formal NAO implementados.
- Paridade completa de UI com a versao Python NAO entregue (esta e a base tecnica).
- Fase 23 e seguintes NAO iniciadas.
- Nenhum arquivo Python alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `TECHNICAL_DEBT_REGISTER.md`, `UI_INVENTORY.md`, `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`.

## Arquivos Python consultados

- Inventario de UI conceitual (`UI_INVENTORY.md`) — sem leitura de fontes Python alem do que ja estava documentado.

## Arquivos C++ criados

- `src/sim/SimulationRunner.hpp`, `src/sim/SimulationRunner.cpp`
- `src/ui/CanvasTool.hpp`
- `src/ui/SelectionState.hpp`
- `src/ui/UiState.hpp`
- `src/ui/Command.hpp`
- `src/ui/InputRouter.hpp`, `src/ui/InputRouter.cpp`
- `src/ui/UiPanel.hpp`, `src/ui/UiPanel.cpp`
- `src/systems/Phase22Diagnostics.hpp`, `src/systems/Phase22Diagnostics.cpp`
- `MIGRACAO_C++SFML/PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`

## Arquivos C++ modificados

- `src/app/App.hpp` — refatorado para AppController fino; removidos stores/sistemas; adicionados `runner_`, `uiState_`, `inputRouter_`, `uiPanel_`, `commandQueue_`, `font_`.
- `src/app/App.cpp` — refatorado: drain de comandos com `std::visit`, ciclo de update e render finos; mensagem inicial Phase 22.
- `src/main.cpp` — flags `--phase22-selftest/benchmark/diagnostics` + dispatch.
- `CMakeLists.txt` — fontes `SimulationRunner.cpp`, `Phase22Diagnostics.cpp`, `InputRouter.cpp`, `UiPanel.cpp`.

## Resolucao da Divida Tecnica 4

A Divida 4 ("App acumulando responsabilidades") foi marcada como RESOLVIDA em `TECHNICAL_DEBT_REGISTER.md`. A acao recomendada original (`SimulationRunner` + `AppController` + `InputRouter`) foi cumprida integralmente, com a adicao bem-vinda de `CommandQueue` (std::variant) como contrato unificado entre input, UI e engine.

## Melhorias alem do escopo minimo

- **CommandQueue baseado em std::variant** em vez de polimorfismo virtual classico ou pares enum/payload. **Justificativa:** dispatch via `std::visit` e compile-time, sem heap alloc por comando, e adicionar tipos novos e somente adicionar uma alternativa ao variant.
- **InputRouter testavel headless**: o roteador opera puramente em termos de `sf::Event` -> `CommandQueue`; testes podem injetar eventos simulados sem janela.
- **UiPanel desacoplada da regra de negocio**: o painel desenha rectangles + text e empurra comandos; trocar a UI inteira por Dear ImGui depois afeta apenas `UiPanel` e nao toca `SimulationRunner`, `InputRouter` ou `App` de forma significativa.

## Como executar

```
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --phase22-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --phase22-benchmark
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --phase22-diagnostics
```

Sem flag: abre janela com renderer Phase 21 + UI Phase 22 (menu, toolbar, ajuda por H).

## Resultados de teste

- Build Debug: OK.
- Build Release: OK.
- `--phase22-selftest`: **PASS (155 checks)**.
- Regressoes Phase 7-21: **TODAS PASS** (14, 15, 23, 21, 30, 49, 42, 71, 72, 109, 110, 111, 133, 134, 150 checks respectivamente).
- `--phase22-benchmark`: avg_step_us na faixa de 700-845 us para 152-163 agentes / 50 comidas / 0 obstaculos (steps fixos), aplicando 0/2/10 comandos por step sem regressao significativa.

```
scenario,agents,foods,obstacles,steps,total_ms,avg_step_us,commands,selection,notes
headless_30steps_0cmd,152,50,0,30,25.3502,845.0067,0,0,cmds_per_step=0
headless_30steps_2cmd,152,50,0,30,23.2868,776.2267,120,0,cmds_per_step=2
headless_30steps_10cmd,152,50,0,30,23.8317,794.3900,600,0,cmds_per_step=10
headless_100steps_0cmd,163,50,0,100,70.3207,703.2070,0,0,cmds_per_step=0
```

## Proximas fases

- Fase 23: persistencia inicial (saves de mundo, parametros, agentes/genomas).
- Fase 26: editor genetico interativo (depende de Fase 22 + Fase 23).
- Avaliar se Dear ImGui sera adotado em fase posterior dedicada a UI rica.
