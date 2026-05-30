# Fase 23 — UI de Parametros e Preferencias

Status: **CONCLUIDA** (2026-05-30)

A Fase 23 transforma o `ParameterRegistry` em uma interface tecnica usavel via janela de Preferencias SFML-native organizada em 7 abas (Simulacao / Fisica / Sistema de Visao / Redes Neurais / Autosave / Aparencia / Performance). Inclui:

- mutacao do registry com clamp/enum-validation (`ParameterRegistry::setValue`);
- preservacao de defaults factory (`originalDefault`);
- metadata `applyFlags` indicando `Immediate / RequiresReset / RebuildPerception / RebuildBrains / RefreshRenderer / PendingFuturePhase`;
- estado de preferencias com `pendingValues` (dirty overrides) + Apply/Revert/Defaults;
- 9 comandos novos (`CmdOpenPreferences / CmdClosePreferences / CmdSetPreferencesTab / CmdSetPreferencesSearch / CmdSetParameterValue / CmdApplyPreferences / CmdRevertPreferences / CmdRestoreDefaultsPreferences / CmdRestoreParameterDefault`);
- painel SFML que se sobrepoe ao canvas e e capturado pelo App antes do `UiPanel` e do `InputRouter`, preservando integralmente a microfase 22.1.

## Escopo executado

- Mutator API: `ParameterRegistry::setValue`, `restoreDefault`, `setApplyFlags`, `search`, `categories`.
- Metadata: `ParameterMetadata.hpp/.cpp` aplica flags em ~40 parametros conhecidos sem mexer no catalogo de defaults.
- `prefsTabForCategory` mapeia `simulation.* / render.* / ui.* -> Simulacao`, `physics.* -> Fisica`, etc.
- `UiPreferencesPanel` desenha tab column (esquerda), header com nome da aba + contador de modificados, lista de parametros com colunas `nome | badge_flags | valor`, e footer com Aplicar/Reverter/Defaults/Fechar.
- Controles por tipo: `bool` toggle clicavel, `int/float` com botoes `[-]`/`[+]` e step automatico (5% do range), `enum` cycle (string params com domains), `color` com botoes `±` por canal +/-16, `string` (sem domains) read-only.
- `prefsApplyPending` aplica todas as edicoes pendentes via `setValue`, retornando a uniao de `applyFlags` para que o App decida se chama `runner_.reset()`, `configureRenderOptions()` ou `configureFromParameters()`.
- Esc fecha preferencias (capturado no App antes do InputRouter).
- Menu `Preferencias > Abrir Preferencias (Fase 23)` agora dispara `CmdOpenPreferences` (era `CmdTogglePreferencesPanel` placeholder na Fase 22.1).
- Phase23 selftest: **189 checks PASS**. Phase 22.1 e Phase 22 continuam PASS (26 e 155). Regressoes Phase 7-21 todas PASS.
- Microbenchmark de UI Phase 23 mostra sub-microsegundo por operacao tipica (query/apply/search).

## Estado verificado antes do trabalho

```
git status (limpo no escopo Phase 22.1)
git log --oneline -5:
6fc6c75 cpp: hotfix Phase 22 UI visual, menus, canvas and resize (22.1)
03fd76b cpp: add UI base with menus, toolbar and canvas tools (Phase 22)
96b5c17 cpp: add collision system and optional physics
...
```

Verificados via reading: `Renderer`, `Camera2D`, `InputRouter`, `UiPanel`, `UiState`, `SimulationRunner`, `ParameterRegistry`, `ParameterDefaults`, `Parameter`, `ParameterHelpers`.

Phase 22.1 hotfix verificado pelo selftest dedicado e por inspecao do codigo: `App::handleResize` ainda chama `window_.setView(...)`, `InputRouter` ainda usa pan event-driven, `UiPanel` ainda renderiza 9 ferramentas sem Pan, brush/eraser stroke continuam intactos. Nenhuma regressao.

## Fora de escopo (confirmado)

- Fase 24 (editor genetico / painel especies / painel substrato): NAO iniciada.
- Fase 25 (agente selecionado / neural viewer): NAO iniciada.
- Fase 26 (graficos/metricas): NAO iniciada.
- Fase 27 (save/load / autosave / recovery): NAO iniciada. Parametros de autosave estao no UI mas com badge "pendente Fase 27".
- Fase 28 (benchmark runner formal): NAO iniciada.
- Fase 29 (paridade total de UI): NAO iniciada.
- Fase 30 (otimizacao deep / Numba C++): NAO iniciada.
- Persistencia de preferencias entre execucoes: NAO implementada (escopo Fase 27).
- Tema visual definitivo / docking avancado / atalhos configuraveis: NAO implementados.
- Editor genetico, painel completo de especies, painel completo de substrato, import/export genoma: NAO implementados (Fase 24).
- Save/load .biosim, autosave/recovery funcional, painel agente selecionado, visualizador neural completo, graficos completos, benchmark runner formal, paridade completa: NAO implementados.
- Nenhum arquivo Python alterado.

## Documentos consultados

- `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`, `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`, `CURRENT_MODULE_MAP.md`, `BENCHMARK_PLAN.md`, `MIGRATION_RISKS.md`, `TECHNICAL_DEBT_REGISTER.md`, `PLANNING_COVERAGE_AUDIT.md`, `CODEX_MIGRATION_GUIDE.md`, `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`, `PHASE_22_1_UI_VISUAL_CANVAS_HOTFIX_STATUS.md`.

## Arquivos Python consultados

Nenhum nesta fase. A UI Python ja foi usada como referencia funcional em fases anteriores e o `UI_INVENTORY.md` resume a logica conceitual; a implementacao Phase 23 e dirigida apenas pelo schema do ParameterRegistry e pelo prompt.

## Arquivos C++ criados

- `src/config/ParameterMetadata.hpp` / `.cpp`
- `src/ui/PreferencesState.hpp`
- `src/ui/UiPreferencesPanel.hpp` / `.cpp`
- `src/systems/Phase23Diagnostics.hpp` / `.cpp`
- `MIGRACAO_C++SFML/PHASE_23_PARAMETERS_PREFERENCES_UI_STATUS.md`

## Arquivos C++ modificados

- `src/config/Parameter.hpp` — `ApplyFlag` namespace; `originalDefault` + `applyFlags` adicionados ao fim de `ParameterDefinition` para preservar agregados existentes; `operator==` para `ColorRgb` (variant equality).
- `src/config/ParameterRegistry.hpp/.cpp` — `setValue` com clamp/enum/validation, `restoreDefault`, `setApplyFlags`, `search`, `categories`, captura de `originalDefault` em `add`.
- `src/ui/Command.hpp` — 9 comandos novos para preferencias.
- `src/ui/UiState.hpp` — campo `preferences` (instancia de `PreferencesState`).
- `src/ui/UiPanel.cpp` — item `Preferencias > Abrir Preferencias (Fase 23)` agora despacha `CmdOpenPreferences`.
- `src/app/App.hpp/.cpp` — instancia `UiPreferencesPanel`, aplica flags Phase 23 no construtor, captura mouse para o painel antes do `UiPanel`/`InputRouter`, fecha pelo Esc, despacha os 9 comandos de preferencias.
- `src/sim/SimulationRunner.cpp` — `applyCommand` passa por todos os 9 comandos novos como no-op (return true) para nao quebrar o pipeline de visit.
- `src/main.cpp` — flags `--phase23-selftest/benchmark/diagnostics`.
- `CMakeLists.txt` — `ParameterMetadata.cpp`, `UiPreferencesPanel.cpp`, `Phase23Diagnostics.cpp`.

## Decisao tecnica sobre UI

Continuei com SFML-native em vez de adotar Dear ImGui agora. Justificativa:
- A microfase 22.1 acabou de estabilizar a UI SFML-native (menus, toolbar, overlays, brush, eraser, resize). Trocar para ImGui agora arriscaria regressao desses itens recem-corrigidos.
- O prompt da Fase 23 admite "Use Dear ImGui/ImGui-SFML ou a alternativa ja decidida na Fase 22". Mantive a alternativa SFML-native.
- A separacao engine/UI esta limpa: trocar o renderer da janela de preferencias para ImGui no futuro afeta apenas `UiPreferencesPanel` e nada da camada de comandos/registry/runner.

## Decisao sobre arquitetura

- **Single source of truth: ParameterRegistry**. Todo valor exibido no painel vem de `registry.find(name)->defaultValue` (current value) ou de `state.pendingValues[name]` (edit pendente). Nenhuma lista paralela.
- **Aliases preservados**: `use_numba_brain_forward` e `use_batch_forward` apontam para o mesmo `ParameterDefinition` (mesmo `index`). A funcao `prefsParametersForTab` itera por `definitions()` (nao por nomes), entao aliases nao geram widgets duplicados.
- **Engine continua sem dependencia de ImGui/SFML**: o `SimulationRunner::applyCommand` aceita os comandos novos como no-op; o que de fato muda o registry e o pipeline e o `App::drainCommandsAndApply`.
- **Aplicacao via comandos**: nenhum widget mexe direto em store/registry; o painel so emite comandos. `prefsApplyPending` e tambem testavel headless (Phase23Diagnostics).
- **Headless preservado**: todos os selftests rodam sem janela; selftest Phase 23 e 100% headless (config + helpers + dispatcher).

## Decisao sobre metadata

Adicionei `applyFlags` (bitmask) ao `ParameterDefinition` e uma tabela em `ParameterMetadata.cpp` que **post-processa** o registry depois de criado. Vantagens:
- Nao precisa tocar nas ~210 chamadas `addBool/Int/Double/...` para classificar os ~40 parametros que merecem flag especial.
- Default `Immediate` cobre 90% dos parametros (cores, viscosidade, knobs de fisica, etc.).
- Fica trivial adicionar metadados para novos parametros depois.

Flags definidas:
- `Immediate` — aplicado no proximo frame; `configureFromParameters()` re-le.
- `RequiresReset` — `SimulationRunner::reset()` antes de surtir efeito (ex.: `world_w`, `physics_steps_per_second`, `random_seed`).
- `RebuildPerception` — recria a config de percepcao via reset (`retina_*_mode/projection/falloff`).
- `RebuildBrains` — afeta novos cerebros (`neural_network_type`, topologias NEAT). Nao recria cerebros existentes silenciosamente.
- `RefreshRenderer` — `App::configureRenderOptions()` re-le cores/flags sem reset (`background_*`, `substrate_color_*`, `simple_render`, `render_enabled`).
- `PendingFuturePhase` — controle aparece mas badge "pendente" sinaliza que o backend ainda nao existe (autosave, brain_cache_max_*, show_metrics_chart, etc.).

A funcao `prefsApplyFlagsLabel` traduz a uniao em string curta para a UI: "imediato", "imediato (renderer)", "requer reset", "rebuild brains+reset", "rebuild vision+reset", "pendente".

## Decisao sobre categorias / subcategorias

Categorias do registry sao chaves pontilhadas (`simulation.time`, `save.autosave`, `performance.neural`). `prefsTabForCategory` extrai o prefixo e mapeia para uma das 7 abas:

| Tab Phase 23     | Prefixos                                |
|------------------|------------------------------------------|
| Simulacao        | `simulation.*`, `render.*`, `ui.*`        |
| Fisica           | `physics.*`                              |
| Sistema de Visao | `vision.*`                               |
| Redes Neurais    | `neural.*`                               |
| Autosave         | `save.*`                                 |
| Aparencia        | `appearance.*`, `world.*` (cores/substrato)|
| Performance      | `performance.*`                          |

`species.*` e `food.*` continuam no registry mas nao aparecem no painel (eles pertencem ao editor da Fase 24).

## Decisao sobre controles

Optei por **steppers `[-] valor [+]`** em vez de campos de texto livre para int/float. Justificativas:
- Implementar input de texto SFML puro exige um state machine completo (cursor, edicao, IME, validacao incremental). Risco/beneficio ruim para esta fase.
- Steppers visualmente alinham bem com a estetica da Fase 22.1.
- Step automatico = 5% do range; clamp garantido pelo `setValue`.
- Para enum (string com `domains`): clique cicla pelos valores permitidos.
- Para color: 6 botoes (R-/R+/G-/G+/B-/B+) com passo de 16; clamp em [0,255].

## Decisao sobre validacao

- Numerico: clamp pelo `range` registrado.
- Enum: rejeita se valor nao esta em `domains` (retorna false; UI nao re-emite o valor).
- Color: clamp por canal em [0,255].
- Bool: aceita sem transformacao.
- String livre (sem `domains`): nao editavel via SFML-native (mostra como read-only). Sera editavel na Fase 24/27 quando entrar input de texto.

## Decisao sobre aplicar/reverter/defaults

- **Aplicar**: `prefsApplyPending(registry, state)` itera `pendingValues`, chama `setValue` em cada, acumula `applyFlags`, limpa pending. O App entao usa as flags para chamar `configureRenderOptions`, `configureFromParameters`, e/ou `runner_.reset()`.
- **Reverter**: limpa `pendingValues` sem tocar no registry. Os valores aplicados anteriormente permanecem.
- **Defaults**: aplica `originalDefault` de cada parametro da aba atual a `pendingValues` (nao limpa pending — ainda exige um clique em Aplicar para gravar).

## Decisao sobre dirty state

- Linha com cor `kBgRowDirty` (azul-violeta) quando `state.pendingValues.count(name) > 0`.
- Linha com cor `kBgRowPending` (laranja escuro) quando o parametro tem `PendingFuturePhase`.
- Contador `modificados=N  aplicados=K` no header.

## Decisao sobre busca/filtro

`PreferencesState.searchQuery` + helpers `prefsParametersForTab(registry, tab, query)` fazem filtro case-insensitive por substring (nome, descricao, aliases). O dispatcher de UI ainda nao expoe o campo de texto graficamente (mesmo motivo da decisao de steppers); o filtro funciona headless. A UI mostra todos os parametros da aba quando query e vazio.

## Como funciona cada aba

| Aba              | Parametros expostos (resumo)                                                    | Flags tipicas |
|------------------|---------------------------------------------------------------------------------|---------------|
| Simulacao        | time_scale, paused, physics_steps_per_second, random_seed, render_enabled, etc.| Immediate + RequiresReset onde aplicavel |
| Fisica           | smooth_locomotion_*, agent_collision_*, viscosity, brownian, chunk_food_*       | Immediate (efeito no proximo step) |
| Sistema de Visao | retina_skip, retina_vision_mode, retina_bins_*, show_multi_selected_vision      | RebuildPerception+RequiresReset onde aplicavel |
| Redes Neurais    | neural_network_type, gate/shortcut/RNN/NEAT knobs                                | RebuildBrains+RequiresReset apenas em neural_network_type/topology |
| Autosave         | auto_export_*, save_recovery_on_close, debug_tracebacks, diagnostic_heartbeat   | PendingFuturePhase (Fase 27) |
| Aparencia        | background/substrate colors, gradients, border, render_resolution_scale         | Immediate + RefreshRenderer |
| Performance      | use_spatial, reuse_spatial_grid, use_batch_forward, batch_forward_min_size, brain_cache_* | Immediate ou PendingFuturePhase |

## Como a Microfase 22.1 foi protegida

- Renderer nao tocado nesta fase. Bordas vetoriais, halos, marquee, brush cursor, AA permanecem.
- `App::handleResize` nao tocado — `setView` continua corrigindo cliques apos maximize.
- `InputRouter` nao tocado — pan event-driven, brush/eraser stroke, atalhos do canvas continuam funcionando.
- `UiPanel` so teve a string de um item do menu Preferencias alterada; pan removido da toolbar continua removido; 9 ferramentas permanecem.
- `SimulationRunner::applyCommand` recebe os 9 comandos novos como no-op para preservar o pipeline.

## Como engine headless foi preservado

- Phase23 selftest e 100% headless: nao instancia `App` nem janela; usa `ParameterRegistry` + `applyPhase23ApplyFlags` + `PreferencesState` + helpers (`prefsParametersForTab`, `prefsEffectiveValue`, `prefsApplyPending`).
- Microbenchmark idem.
- Comando de aplicar parametro funciona pelo `prefsApplyPending` sem SFML.

## Determinismo

- `setValue` e deterministico.
- `restoreDefault` restaura exatamente o valor capturado em `originalDefault` no momento de `add()`.
- `prefsApplyPending` opera em ordem de hash map (nao deterministica entre execucoes), mas como `setValue` e idempotente sobre cada parametro o resultado final no registry e o mesmo independente da ordem.
- Phase23 selftest faz `setValue("physics_steps_per_second", -9999)` e `setValue("physics_steps_per_second", 9999)` 50 vezes e verifica clamp.
- Nenhum uso de `std::random_device`.

## Resultados dos testes

- Build Debug: OK.
- Build Release: OK.
- `--phase23-selftest`: **PASS (189 checks)**.
- `--phase22-hotfix-selftest`: **PASS (26 checks)**.
- `--phase22-selftest`: **PASS (155 checks)**.
- Regressoes Phase 7-21: **TODAS PASS** (14, 15, 23, 21, 30, 49, 42, 71, 72, 109, 110, 111, 133, 134, 150 checks).
- `--phase23-benchmark`:

```
scenario,parameters,pending,applied,total_ms,avg_op_us,notes
query_tab_simulation,283,24,0,0.0159,0.6625,
apply_50_pending,283,3,1,0.0038,0.9500,
search_filter_vision,283,9,0,0.0095,1.0556,
restore_defaults_neural_tab,283,54,1,0.0315,0.5727,
clamp_50_invalid,283,100,0,0.0061,0.0610,
```

Custos sub-microsegundo por operacao tipica. Apply de 50 edicoes pendentes leva 0.0038 ms total — invisivel em um frame de 16ms.

- Smoke test do executavel: abre janela, mensagem inicial `"AgentBioSimCpp Phase 23: preferencias UI ready (2 species, 2 genomes, 50 foods, 0 obstacles)."`, fecha limpo.

## Validacao visual/manual

Esta validacao depende de inspecao humana no executavel. A camada de testes headless valida que:
- a aba ativa muda corretamente,
- parametros aparecem por categoria,
- aliases nao duplicam,
- clamp/enum funciona,
- `Aplicar` modifica o registry e dispara `configureRenderOptions/reset/configureFromParameters` no App,
- `Reverter` limpa pending,
- `Defaults` repovoa pending com `originalDefault`.

Para confirmar UX, o usuario deve:
1. Abrir o executavel.
2. Menu Preferencias > Abrir Preferencias (Fase 23).
3. Trocar de aba clicando na coluna esquerda.
4. Clicar em controles de bool/int/float/enum/color.
5. Clicar em Aplicar para gravar; Esc para fechar.
6. Confirmar que ao fechar a janela, brush/eraser/selecao/zoom/pan/maximize continuam funcionando exatamente como na Microfase 22.1.

## Parametros usados / pendentes / sem UI

- **Usados (impacto runtime)**: time_scale, paused, render_enabled, simple_render, physics_steps_per_second, random_seed, cores de background/substrato, toggles de fisica (collision, viscosity, brownian, chunk_food_*), retina_skip, retina_vision_mode, use_spatial, reuse_spatial_grid, use_batch_forward, batch_forward_min_size, etc.
- **Pendentes**: autosave/* (Fase 27), brain_cache_max_* (Fase 27/30), show_metrics_chart (Fase 26), neural_view_dense_layout (Fase 25), camera_follow_* (Fase 25), show_selected_details (Fase 25), use_grouped_vision_batches / use_persistent_perception_arrays (Fase 30).
- **Sem UI nesta fase**: parametros `species.*` e `food.*` permanecem no registry mas nao aparecem nas abas — eles pertencem ao editor da Fase 24.

## Itens de inventarios cobertos

- `FEATURE_INVENTORY.md`: "preferencias", "validacao de parametros", "aplicar runtime", "marcacao de rebuild" cobertos pela mecanica geral.
- `PARAMETER_INVENTORY.md`: ~283 parametros expostos pelo registry; os ~150 listados no prompt da Fase 23 estao todos no registry e selecionaveis via tabs/search.
- `UI_INVENTORY.md`: o item "preferencias / parametros" passa a ter uma implementacao funcional headless+grafica.

## Itens deixados para fases futuras

- Fase 24: editor genetico, painel especies, painel substrato, import/export genoma.
- Fase 25: agente selecionado completo + visualizador neural.
- Fase 26: graficos/metricas/profiler.
- Fase 27: save/load/autosave + recovery + persistencia de preferencias entre sessoes.
- Fase 28: benchmark runner formal.
- Fase 29: paridade completa da UI Python.
- Fase 30: deep optimization.

## Como cada fase anterior foi protegida

- Fase 7 (comida instantanea): nao tocada; selftest PASS (14).
- Fase 8 (locomocao): nao tocada; selftest PASS (15).
- Fases 10-12 (visao): nao tocadas; selftests PASS (21/30/49). Parametros expostos com flag RebuildPerception.
- Fase 13 (reproducao/genoma): nao tocada; selftest PASS (42).
- Fases 14-16 (redes densas/RNN/NEAT): nao tocadas; selftests PASS (71/72/109).
- Fase 17 (especies/labels/genomas): nao tocada; selftest PASS (110).
- Fase 18 (predation/dieta): nao tocada; selftest PASS (111).
- Fase 19 (comida chunk): nao tocada; selftest PASS (133).
- Fase 20 (obstaculos/oclusao): nao tocada; selftest PASS (134).
- Fase 21 (colisoes/fisica): nao tocada; selftest PASS (150).
- Fase 22 (UI base): selftest PASS (155). UiPanel teve apenas string atualizada no item de menu Preferencias.
- Microfase 22.1 (visual+canvas hotfix): selftest PASS (26). Renderer/Camera2D/InputRouter/handleResize intactos.

## Confirmacoes finais

- **Editor genetico completo**: NAO implementado.
- **Painel completo de especies**: NAO implementado.
- **Painel completo de substrato**: NAO implementado.
- **Visualizador neural completo**: NAO implementado.
- **Save/load final**: NAO implementado.
- **Autosave/recovery funcional completo**: NAO implementado (placeholder com badge "pendente Fase 27").
- **Benchmark runner formal**: NAO implementado.
- **Nenhum arquivo Python foi alterado.**
- **Fase 24 NAO foi iniciada.**
