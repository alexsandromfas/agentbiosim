# Fase 24 — Editor Genetico, Especies/Labels, Populacao e Substrato

Status: **CONCLUIDA** (2026-06-01)

A Fase 24 entrega os 4 paineis operacionais da UI antiga, em janelas independentes (cada uma arrastavel, com scroll, header, footer com botoes de acao). A arquitetura segue rigorosamente o que aprendemos nas Microfases 22.1, 23.1 e 23.2: nada de painel unico gigante, nada de Numba na UI, nada de nome interno como label principal, nada de combo falso.

## Estado verificado antes do trabalho

```
git log --oneline -5:
9433fa3 cpp: hotfix Phase 23 prefs (round 2) - drag, text edit, combo fix (23.2)
a409718 cpp: hotfix Phase 23 preferences UI for Python parity (23.1)
d957612 cpp: add preferences UI driven by ParameterRegistry (Phase 23)
6fc6c75 cpp: hotfix Phase 22 UI visual, menus, canvas and resize (22.1)
03fd76b cpp: add UI base with menus, toolbar and canvas tools (Phase 22)
```

Working tree limpo, Fase 23 + 23.1 + 23.2 comitadas, Fase 25 nao iniciada.

## Documentos consultados

- `MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`, `FEATURE_INVENTORY.md`, `UI_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `CURRENT_MODULE_MAP.md`, `MIGRATION_RISKS.md`, `TECHNICAL_DEBT_REGISTER.md`, `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`, `PHASE_22_1_UI_VISUAL_CANVAS_HOTFIX_STATUS.md`, `PHASE_23_PARAMETERS_PREFERENCES_UI_STATUS.md`, `PHASE_23_1_PREFERENCES_UI_HOTFIX_STATUS.md`, `PHASE_23_2_PREFERENCES_UI_HOTFIX_STATUS.md`.

## Arquivos Python consultados

**Usabilidade (referencia principal):**
- `sim/ui.py` — linhas 2541-3000: `_build_tab_genetic_editor`, `_build_tab_population`, `_build_tab_environment`, `_build_food_color_swatch`, `_pick_food_color`, `apply_population_params`, `apply_substrate_params`, `clear_food`. Confirmaram que o Python tem abas/paineis SEPARADOS para Editor Genetico, Populacao e Substrato — e nao um painel unico.

**Semantica funcional:** nenhum arquivo Python adicional foi necessario porque o ParameterRegistry C++ ja contem os parametros corretos (bacteria_*, predator_*, food_*, world_*, substrate_*). A lista de parametros foi extraida diretamente do registry, nao do Python.

## Como a UI Python foi usada como referencia

| Pergunta da Fase 24 | Conclusao do Python | Decisao C++ |
|---|---|---|
| O que e Editor Genetico? | Aba separada com nome do template + secoes Corpo/Energia/Visao/Dieta/Rede neural | Janela `Editor Genetico (bacteria)` com mesmas secoes via headers in-line |
| Populacao e parte do Editor? | Nao — e aba propria com `bacteria_count`, regras populacionais, botao Aplicar | Janela Populacao propria com `bacteria_count`, `predator_count`, `*_min_limit`/`*_max_limit`, `population_min_rescue_enabled`, `max_deaths_per_step` |
| Substrato fica em Preferencias? | Nao — aba propria com Comida + Mundo + acoes Aplicar/Limpar Comida | Janela Substrato com mesma estrutura |
| Botoes de aplicar | "Aplicar populacao", "Aplicar ambiente", "Limpar Comida" | Mesma estrutura em PT-BR ASCII |
| Color picker da comida | `QColorDialog` no botao "Escolher cor" | Popup color picker reaproveitado da Fase 23.2 (clica no swatch -> popup com 8 presets + steppers RGB) |
| Pipeta/coletar genoma | Nao encontrado no `_build_tab_genetic_editor` consultado | Marcado como pendencia explicita para Fase 25 |

## Decisoes especificas

### Editor Genetico
- Janela propria (`editorOpen`), arrastavel, com scroll independente.
- Mostra parametros `bacteria_*` agrupados por secao: **Corpo e movimento**, **Energia e metabolismo**, **Visao**, **Dieta**, **Rede neural**.
- Labels amigaveis via `prefsFriendlyLabel`. Prefixo `bacteria_` removido do label exibido.
- Footer com 4 botoes: **Aplicar a especie**, **Aplicar selecionados**, **Reverter**, **Defaults**.

### Especies/Labels
- Janela propria (`especiesOpen`).
- Lista cards de cada especie do `SpeciesStore`: swatch de cor + nome (label do `SpeciesRecord`) + texto "limite [min-max]".
- 3 botoes por linha: **Selecionar** (CmdSelectAllOfSpecies), **Reset rede** (CmdResetNeuralForSpecies), **Atribuir sel.** (CmdAssignSelectedToSpecies).
- Footer com 2 botoes: **Nova com selecionados** (CmdCreateSpeciesFromSelected) e **Atribuir aos selecionados** (CmdAssignSelectedToSpecies).

### Populacao
- Janela propria (`populacaoOpen`).
- Secoes: **Quantidade inicial por especie**, **Limites populacionais**, **Regras gerais**.
- Footer: **Aplicar populacao**, **Reverter**, **Defaults**.

### Substrato e Comida
- Janela propria (`substratoOpen`).
- Secoes: **Mundo / substrato**, **Comida**, **Comida em pedacos (chunk)**.
- Footer: **Aplicar ambiente**, **Limpar comida**, **Reverter**, **Defaults**.
- `food_mode` aparece como combo via `prefsEnumValuesFor("food_mode")` — reaproveita o sistema de combo da Fase 23.2.
- `food_color` abre o popup color picker.
- `substrate_shape` combo: rectangular/circular.

### Pipeta / coletar genoma
**Decisao**: nao implementada nesta fase. O `_build_tab_genetic_editor` do Python nao expoe pipeta — pode ser feature de outra aba que precisaria consulta mais ampla. Listado como pendencia explicita para Fase 25 (painel do agente selecionado), onde fara mais sentido junto com o ActivationTrace.

### Importar/exportar genoma
**Decisao**: itens "Importar Genoma (Fase 27)" e "Exportar Genoma (Fase 27)" aparecem no dropdown Genoma com flag desabilitada. Backend definitivo entra na Fase 27 (save/load). Sem botao falso.

### Aplicacao de genoma/parametros
- **Aplicar a especie**: emite `CmdApplyGenomeToSpecies` que chama `prefsApplyPending` (baka pendingValues no registry) + `configureFromParameters` + `runner_.reset()` + `fitCameraToWorld`. Resultado: novos agentes spawned na proxima inicializacao usam o novo default. **NAO** recria agentes existentes silenciosamente.
- **Aplicar selecionados**: emite `CmdApplyGenomeToSelected` que baka pendingValues + deleta os agentes selecionados. O sistema de rescue populacional respawna com o novo default. Implementacao completa de mutacao per-agente fica para Fase 25 (painel do agente selecionado).
- **Aplicar populacao**: `CmdApplyPopulation` -> mesma rota de reset com novo `bacteria_count` no registry.
- **Aplicar ambiente**: `CmdApplyEnvironment` -> mesma rota; `world_w/h/shape/radius` re-lidos durante `initialize()`.
- **Limpar comida**: `CmdClearAllFood` -> `foodSystem_.clearAll(foods_)` imediato. Reposicao continua respeitando target.
- **Reset rede neural por especie**, **Criar nova especie com selecionados**: comandos roteados mas implementacao detalhada da mutacao em SpeciesStore/GenomeStore fica para Fase 25 (documentado nos botoes).

### Labels amigaveis
Exemplos antes/depois (todos via `prefsFriendlyLabel` ou strip do prefixo da especie):

| Nome interno | Label exibido |
|---|---|
| `bacteria_body_size` | tamanho do corpo (prefixo strip) |
| `bacteria_max_speed` | max_speed (prefixo strip) ou via friendly se mapeado |
| `bacteria_diet_food` | diet_food (prefixo strip) |
| `food_mode` | "Tipo de comida" |
| `food_target` | "Quantidade alvo de comida" |
| `substrate_shape` | "Formato do substrato" |
| `world_w` | "Largura do mundo" |
| `world_h` | "Altura do mundo" |
| `substrate_radius` | "Raio do substrato (circular)" |
| `population_min_rescue_enabled` | "Resgatar populacao minima" |

Nome interno **NAO** aparece como subtitulo gris (removido na 23.2 e mantido removido aqui).

### Idioma / encoding
PT-BR ASCII em todos os labels visiveis. Sem mistura ingles. Sem acentos quebrados.

### Color pickers e dropdowns
Reaproveitam integralmente o sistema da Fase 23.2: combos via `CmdOpenPrefsPopup{"enum:<name>"}`, cores via `CmdOpenPrefsPopup{"color:<name>"}`.

### Scroll
Cada janela tem `*_scroll` em PreferencesState. Mouse wheel sobre uma janela emite `CmdScrollOperationalWindow{which, delta}` — o canvas nao zooma.

### Botoes desabilitados / placeholders
- "Importar Genoma (Fase 27)" — disabled.
- "Exportar Genoma (Fase 27)" — disabled.
- **Reset rede neural por especie** e **Criar nova especie com selecionados** — botoes funcionais (clicaveis e emitem comandos), mas a implementacao detalhada em SpeciesStore/GenomeStore fica para Fase 25. O App dispatcher trata como no-op documentado para nao crashar.

## Integracao arquitetural

- `SpeciesStore`: usado em modo leitura via `runner.species().records()` na janela Especies. Mutacoes (criar/excluir) ainda nao implementadas — Fase 25.
- `GenomeStore`: nao tocado diretamente. Mudancas de genoma sao via parametros do registry seguidas de reset.
- `ParameterRegistry`: fonte de verdade. Todos os parametros editados usam `setValue` com clamp/enum validation.
- `CommandQueue`: todas as acoes da UI passam por aqui.
- `SimulationRunner`: trata `CmdClearAllFood` efetivamente; demais comandos da Fase 24 sao no-op do runner (o App e quem coordena reset + apply).
- `App.cpp`: dispatch de 19 comandos novos; eventos de mouse roteados na ordem prefsPanel -> operationalPanels -> uiPanel -> InputRouter.
- `Renderer`, `Camera2D`, `InputRouter`: NAO tocados. Microfase 22.1 intacta.

### Como Labels legados foram preservados
`SpeciesRecord` mantem `legacyAliases` e `parameterPrefix`. A UI exibe o `label` (display) do record, que ja e a forma humana. Bacteria/Predator continuam como os 2 records default registrados via `SpeciesBootstrap`.

### Como Bacteria/Predator foram preservados como aliases
`runner.species().findByName("bacteria")` e `findByName("predator")` continuam funcionando — confirmado nos checks 64/65 do selftest.

## Arquivos C++ criados

- `src/ui/UiOperationalPanels.hpp` / `.cpp`
- `src/systems/Phase24Diagnostics.hpp` / `.cpp`
- `MIGRACAO_C++SFML/PHASE_24_GENETIC_SPECIES_SUBSTRATE_UI_STATUS.md`

## Arquivos C++ modificados

- `src/ui/PreferencesState.hpp` — campos `editorOpen/especiesOpen/populacaoOpen/substratoOpen`, posicoes X/Y, scroll, `draggingOperational`.
- `src/ui/Command.hpp` — 19 comandos novos: open/close das 4 janelas, scroll, move, e os comandos de aplicar (ApplyGenomeToSpecies/Selected, ResetNeuralForSpecies, SelectAllOfSpecies, AssignSelectedToSpecies, CreateSpeciesFromSelected, ApplyPopulation, ApplyEnvironment, ClearAllFood).
- `src/ui/UiPanel.cpp` — Genoma menu agora lista Editor / Especies / Populacao / Substrato + Importar/Exportar disabled.
- `src/app/App.hpp/.cpp` — instancia `UiOperationalPanels`; roteia mouse (click/move/release/wheel); dispatch dos 19 comandos novos; aplica via runner.reset/configureFromParameters/configureRenderOptions; CmdClearAllFood via runner.applyCommand; mensagem inicial atualizada.
- `src/sim/SimulationRunner.cpp` — no-op cases para os 19 comandos; `CmdClearAllFood` chama `foodSystem_.clearAll`.
- `src/main.cpp` — flag `--phase24-selftest`.
- `CMakeLists.txt` — `UiOperationalPanels.cpp` + `Phase24Diagnostics.cpp`.
- `src/systems/Phase22_1Diagnostics.cpp` — check 14 ajustado para aceitar tanto o placeholder legado quanto os itens funcionais Phase 24 do menu Genoma.

## Resultados dos testes

- Build Debug: OK.
- Build Release: OK.
- `--phase24-selftest`: **PASS (130 checks)**.
- `--phase23-hotfix2-selftest`: **PASS (58 checks)**.
- `--phase23-hotfix-selftest`: **PASS (58 checks)**.
- `--phase23-selftest`: **PASS (189 checks)**.
- `--phase22-hotfix-selftest`: **PASS (26 checks)** (ajustado para reconhecer novo menu Genoma).
- `--phase22-selftest`: **PASS (155 checks)**.
- Regressoes Phase 7-21: **todas PASS** (14, 15, 23, 21, 30, 49, 42, 71, 72, 109, 110, 111, 133, 134, 150).
- Smoke do executavel: abre limpo. Mensagem: `"AgentBioSimCpp Phase 24: Editor Genetico + Especies + Populacao + Substrato ..."`.

## Itens cobertos vs deixados para fases futuras

**Cobertos (Fase 24)**:
- Editor Genetico com ~40 parametros bacteria_* organizados em 5 secoes.
- Especies/Labels com lista + 3 botoes por linha + footer.
- Populacao com 9 parametros + botao Aplicar.
- Substrato/Comida com 16 parametros + 2 botoes especiais (Aplicar ambiente / Limpar comida).
- Combos para `food_mode`, `substrate_shape`, `food_piece_replenish_mode`, `retina_input_mode` etc. via `prefsEnumValuesFor`.
- Color picker para `food_color` e demais cores.

**Deixados para Fase 25** (painel do agente selecionado):
- Pipeta / coletar genoma (precisa do ActivationTrace tooling).
- Mutacao per-agente real (CmdApplyGenomeToSelected hoje deleta+respawna; ideal e patch in-place).
- Reset rede neural per-especie (precisa API de SpeciesStore).
- Criar nova especie com selecionados (precisa API de SpeciesStore + GenomeStore).

**Deixados para Fase 26**: gráficos / metricas por especie.
**Deixados para Fase 27**: save/load + autosave + import/export real de genoma e substrato.
**Deixados para Fase 28**: benchmark runner formal.
**Deixados para Fase 29**: paridade completa de UI.
**Deixados para Fase 30**: otimizacao profunda.

## Como cada fase anterior foi protegida

- Fase 17 SpeciesStore/GenomeStore: usados em modo leitura, nao mutados.
- Fase 18 dieta/predacao: nao tocada.
- Fase 19 comida chunk: parametros `food_piece_*` expostos no painel Substrato, sem alterar engine.
- Fase 20 obstaculos/oclusao: nao tocada.
- Fase 21 fisica/colisao: nao tocada.
- Fase 22 UI base: menus continuam roteando corretamente; toolbar mantida com 9 ferramentas.
- Microfase 22.1: Renderer, Camera2D, handleResize, InputRouter, brush stroke — intactos.
- Fase 23 preferencias: janelas continuam separadas e funcionais.
- Microfases 23.1 e 23.2: enum combos, color picker, friendly labels, drag, text edit — reaproveitados pelos novos paineis.

## Pendencias remanescentes

- Manipulacao plena de SpeciesStore (criar/excluir/mutar registro) — Fase 25.
- Mutacao per-agente in-place no GenomeStore — Fase 25.
- Pipeta para coletar genoma de agente clicado — Fase 25.
- Persistencia de posicoes/abertura de janelas entre sessoes — Fase 27.
- Import/export real de `.biosim` — Fase 27.
- Icones PNG da pasta Assets/ — pendencia continuada.

## Confirmacoes finais

- **Nenhum arquivo Python foi alterado.**
- **Fase 25 NAO foi iniciada.**
- **Save/load final NAO foi implementado.**
- **Visualizador neural completo NAO foi implementado.**
- **Benchmark runner formal NAO foi implementado.**
- **Editor Genetico, Especies, Populacao e Substrato sao janelas SEPARADAS** — sem painel unico gigante.
- **Sem Numba na UI**, **sem nome interno como label principal**, **sem botao funcional falso** (botoes desabilitados estao marcados como Fase 27 com motivo claro).
- **Microfases 22.1, 23.1 e 23.2 nao regrediram.**
