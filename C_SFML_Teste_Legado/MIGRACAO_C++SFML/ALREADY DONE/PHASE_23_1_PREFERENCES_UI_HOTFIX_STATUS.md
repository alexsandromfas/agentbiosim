# Microfase 23.1 — Correcao de Preferencias, Usabilidade e Paridade Funcional

Status: **CONCLUIDA** (2026-05-30)

A Fase 23 entregou a UI de parametros mas a validacao manual revelou problemas serios de usabilidade e paridade com a UI Python que ja existia:

- menu Preferencias com um unico item "Abrir Preferencias" abrindo um painel tabbed gigante;
- Numba ainda visivel como opcao da UI C++;
- slider de velocidade enterrado em Preferencias (no Python ficava na toolbar);
- substrato dentro de Preferencias generico (no Python era painel proprio);
- mistura portugues/ingles + acentos quebrados;
- Ajuda no overlay legado em vez de janela propria;
- labels com nome interno de parametro (`agents_inertia`, `smooth_locomotion_enabled`);
- color picker sem popup;
- combo de redes neurais ciclando por clique;
- scroll inexistente;
- linha selecionada persistente.

Esta microfase corrige todos esses itens, **consultando apenas a camada de usabilidade do Python** (`sim/ui.py`), e mantem integralmente a Microfase 22.1.

## Estado verificado antes do trabalho

```
git status (limpo no escopo)
git log --oneline -3:
d957612 cpp: add preferences UI driven by ParameterRegistry (Phase 23)
6fc6c75 cpp: hotfix Phase 22 UI visual, menus, canvas and resize (22.1)
03fd76b cpp: add UI base with menus, toolbar and canvas tools (Phase 22)
```

## Arquivos Python consultados

Apenas os arquivos focados em UI/usabilidade, como exigido pelo prompt:

- `sim/ui.py` (consultado): leitura de `_build_menu_bar` (linhas 1419-1502) para mapear o fluxo de Arquivo/Exibir/Preferencias/Agente/Ajuda; leitura de `_build_toolbar` (linhas 510-549) para confirmar que o slider de velocidade fica na toolbar (NAO em Preferencias) com label "Velocidade" e range 0.1-50x; identificacao das funcoes `open_simulation_options_window`, `open_vision_system_window`, `open_neural_network_window`, `open_environment_appearance_window`, `open_new_changes_window`, `open_autosave_window`.

Nenhum outro arquivo Python foi consultado nesta microfase. `sim/game.py`, `sim/controllers.py`, `sim/render.py` foram ignorados — a UI Python ja explicou tudo que precisava.

## Como o Python foi usado como referencia de usabilidade

| Pergunta da microfase | Conclusao tirada do Python |
|---|---|
| O que o menu Preferencias deve listar? | Multiplas opcoes: Renderizar (toggle), Opcoes de simulacao, Autosave, Aparencia, Sistema de Visao, Redes neurais, Novas mudancas (Performance). Cada uma abre uma janela separada. |
| Slider de velocidade fica onde? | Na toolbar, ao lado dos botoes Play/Pause/Stop. Range 0.1-50x. Label "Velocidade". |
| Ajuda abre overlay ou janela? | Janela dedicada (`show_help_window` no Python). |
| Substrato fica em Preferencias? | NAO. O Python tem fluxo proprio para substrato. Aqui, como o painel completo e Fase 24, expomos um placeholder claro. |
| Mistura de idioma? | Python mantem PT-BR consistente nos menus visiveis ao usuario. Vamos seguir. |
| Color picker? | Python tem QColorDialog (popup). Vamos implementar popup com preset swatches + steppers RGB. |
| Combo de rede neural? | Python usa QComboBox (dropdown). Vamos implementar popup vertical com a lista. |

## Escopo executado

### A. Fluxo do menu Preferencias reconstruido
- Dropdown agora tem 11 itens (mais separadores): Renderizar (toggle), Opcoes de simulacao, Autosave, Aparencia do ambiente, Sistema de Visao, Redes neurais, Fisica, Performance, Substrato (Fase 24).
- Cada item dispara `CmdOpenPreferencesWindow{tab}` (ou `CmdOpenSubstratePlaceholder{}` para o placeholder de Fase 24) — sem mais "Abrir Preferencias" placeholder.

### B. Janelas independentes
- `PreferencesState` reescrito: `std::array<bool, 7> windowOpen` permite abrir multiplas janelas simultaneamente (Fisica + Aparencia + Visao lado a lado, por exemplo).
- `std::array<int, 7> windowScroll` da scroll independente por janela.
- `UiPreferencesPanel::draw` itera todas as janelas abertas e desenha cada uma com `windowRectForTab(tab, viewport)` cascateado (28px de offset por janela).
- Cada janela tem header com titulo + botao X (canto direito), area de parametros com scroll, e footer Aplicar/Reverter/Defaults.

### C. Remocao do Numba na UI
- `prefsShouldHideParameter(name)` esconde `use_numba_kernels`, `use_numba_batch_retina`, `use_numba_locomotion_energy`, `use_numba_brain_forward`, `numba_brain_forward_min_batch`, `use_native_brain_forward`, `autosave_enabled` (alias).
- `prefsParametersForTab` aplica o filtro — nenhum parametro com prefixo `numba`/`use_native_brain_forward` aparece na UI.
- Aliases continuam **funcionando** internamente: `registry.find("use_numba_brain_forward")` resolve para o mesmo `ParameterDefinition` de `use_batch_forward`. A UI usa apenas o nome canonico C++.

### D. Slider de velocidade na toolbar
- Widget novo entre Fit e o status: rectangulo com `[-]`, label "Velocidade NNx", `[+]`. Ocupa 3 button-widths da toolbar.
- `[-]` emite `CmdAdjustTimeScale{0.5}`; `[+]` emite `CmdAdjustTimeScale{2.0}`; clicar no centro do label emite `CmdSetTimeScale{1.0}` (reset).
- AppController le `time_scale` atual do registry, multiplica por `factor`, clamps em [0.1, 50.0], escreve via `setValue` e re-aplica `configureFromParameters()` para o `FixedTimestep` pegar o novo valor sem reset.
- `uiState_.timeScale` espelha o valor para o display.

### E. Idioma e encoding
- Politica: portugues do Brasil em **ASCII** (sem acentos) nos labels visiveis. Justificativa: o glyph cache padrao do SFML 2.6 com `segoeui.ttf` renderiza Latin-1, mas usar ASCII evita surpresas de encoding em diferentes maquinas/locales e mantem consistencia sem reescrever o sistema de fonte.
- Padronizado: "Arquivo", "Exibir" (era "View"), "Preferencias", "Genoma", "Ajuda".
- Toolbar labels (CanvasTool): "Selecao", "Rect", "Laco", "Comida", "Agente", "Pincel", "Apagar", "Mover", "Excluir".
- Botoes: "Aplicar", "Reverter", "Defaults", "Fechar", "Novo", "Play"/"Pause", "Fit".
- Documentado em `prefsCategoryDisplay` / `prefsTabLabel`.

### F. Menu Ajuda agora abre janela propria
- Item 0 do menu Ajuda dispara `CmdOpenHelpWindow{}` (era `CmdToggleHelpPanel{}` no overlay 22.1).
- `UiPreferencesPanel::draw` renderiza a janela Help quando `state.helpWindowOpen == true`: header "Ajuda e atalhos" + lista de 24 linhas de atalhos com scroll independente (`state.helpScroll`).
- Mouse wheel sobre a janela Ajuda chama scroll; Esc fecha.

### G. Labels amigaveis
- `prefsFriendlyLabel(name)` tem 70 entradas mapeando o nome interno para um label PT-BR ASCII:
  - `agents_inertia` -> "Inercia dos agentes"
  - `smooth_locomotion_enabled` -> "Locomocao suavizada"
  - `global_viscosity_drag` -> "Intensidade da viscosidade"
  - `brownian_motion_enabled` -> "Movimento browniano"
  - `neural_network_type` -> "Tipo de rede neural"
  - `time_scale` -> "Velocidade da simulacao"
  - `random_seed` -> "Semente aleatoria (-1 = aleatoria)"
  - `auto_export_substrate` -> "Autosave ativado"
  - `use_batch_forward` -> "Forward em lote (batch)"
  - `retina_vision_mode` -> "Modo de visao da retina"
  - ... 60 outros
- O nome interno ainda aparece como subtitulo dim (10px, cinza) na linha — fica disponivel para power users sem virar label principal.

### H. Scroll funcional
- Cada janela tem `windowScroll[tab]` (int rows offset).
- `handleMouseWheel(sx, sy, viewport, delta, state, queue)` testa se o cursor esta sobre alguma janela; se sim, emite `CmdScrollPreferencesWindow{tab, step}` (1 notch = 3 rows) e retorna `true`.
- App.cpp processa `MouseWheelScrolled` antes do InputRouter: se o painel consumiu, o canvas NAO zooma (resolve o `WantCaptureMouse`-equivalente).
- Scrollbar visual: track no canto direito de cada janela, thumb proporcional ao numero de rows visiveis.
- Help window e o substrato placeholder tambem tem scroll funcional.

### I. Combo de redes neurais
- Clicar na celula de valor de `neural_network_type` agora dispara `CmdOpenPrefsPopup{"neural_combo"}`.
- O popup `neural_combo` desenha lista vertical centralizada com todos os `domains` (mlp, gated_mlp, shortcut_mlp, modulated_mlp, simple_rnn, neat, proto_neat, recurrent_neat, etc.) e a linha atual destacada.
- Clicar em uma linha do popup emite `CmdSetParameterValue{name, escolha}` e fecha o popup. Nada e aplicado ate Aplicar — a politica `RebuildBrains + RequiresReset` continua valendo.

### J. Color picker com popup
- Clicar no swatch de uma cor abre `CmdOpenPrefsPopup{"color:<param>"}`.
- Popup tem header "Selecionar cor", 8 swatches de preset (cinza claro / vermelho / verde / azul / amarelo / roxo / ciano / quase preto), e per-canal R/G/B steppers `[-]`/`[+]` (passo 16) com display do valor.
- Quadrado de preview no canto inferior-direito mostra a cor atual.
- Esc fecha; clicar fora fecha.

### K. Selecao persistente removida
- Linhas de parametro nao tem mais selectable persistente. Cor da linha:
  - `kBgRow / kBgRowAlt` em zebra alternada (padrao);
  - `kBgRowDirty` apenas quando `pendingValues.count(name) > 0`;
  - `kBgRowPending` apenas quando `applyFlags & PendingFuturePhase`.
- Hover nao deixa estado visual residual.

### L. Testes
- `Phase23_1Diagnostics`: 58 selftests cobrindo Numba removido, friendly labels, dropdown de Preferencias com multiplos itens roteando para `CmdOpenPreferencesWindow`, `CmdOpenHelpWindow` (nao `CmdToggleHelpPanel`), substrato emite `CmdOpenSubstratePlaceholder`, estado multi-window/popup, comandos do widget de velocidade.
- Selftest Phase 22.1 atualizado para reconhecer o novo roteamento (Preferencias agora roteia para `CmdOpenPreferencesWindow` OU `CmdTogglePreferencesPanel`; Ajuda agora roteia para `CmdOpenHelpWindow` OU `CmdToggleHelpPanel`). Ambos paths sao aceitos para preservar a intencao do teste original (Preferencias != Ajuda) sem reverter a melhoria.

## Decisao sobre tipo de substrato

Por usabilidade Python: o substrato nao era preferencia generica. Decisao da microfase:
- nao incluir `substrate_shape` / `world_w` / `world_h` na aba `Aparencia` (que e cosmetica);
- adicionar no menu Preferencias um item explicito "Substrato (Fase 24)" que abre um placeholder com texto "O painel completo entra na Fase 24";
- na Fase 24 esse placeholder vira o painel real de substrato/mundo, sem precisar mexer no menu.

## Fora de escopo (confirmado)

- Fase 24 NAO iniciada.
- Editor genetico completo NAO implementado.
- Painel completo de especies NAO implementado.
- Painel completo de substrato NAO implementado (apenas placeholder com texto Fase 24).
- Save/load real NAO implementado.
- Autosave/recovery real NAO implementado.
- Visualizador neural completo NAO implementado.
- Graficos/metricas completos NAO implementados.
- Benchmark runner formal NAO implementado.
- Paridade completa da UI final NAO implementada.
- Nenhum arquivo Python alterado.

## Documentos consultados

`MIGRATION_PHASES.md`, `PROPOSED_CPP_ARCHITECTURE.md`, `ARCHITECTURE_REVIEW.md`, `FEATURE_INVENTORY.md`, `UI_INVENTORY.md`, `PARAMETER_INVENTORY.md`, `CURRENT_MODULE_MAP.md`, `MIGRATION_RISKS.md`, `TECHNICAL_DEBT_REGISTER.md`, `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`, `PHASE_22_1_UI_VISUAL_CANVAS_HOTFIX_STATUS.md`, `PHASE_23_PARAMETERS_PREFERENCES_UI_STATUS.md`, `CODEX_MIGRATION_GUIDE.md`.

## Arquivos C++ criados

- `src/systems/Phase23_1Diagnostics.hpp` / `.cpp`
- `MIGRACAO_C++SFML/PHASE_23_1_PREFERENCES_UI_HOTFIX_STATUS.md`

## Arquivos C++ modificados

- `src/config/ParameterMetadata.hpp/.cpp` — `prefsFriendlyLabel(name)`, `prefsShouldHideParameter(name)`.
- `src/ui/CanvasTool.hpp` — labels em PT-BR ASCII.
- `src/ui/Command.hpp` — 11 comandos novos: `CmdOpenPreferencesWindow / CmdClosePreferencesWindow / CmdScrollPreferencesWindow / CmdOpenHelpWindow / CmdCloseHelpWindow / CmdOpenSubstratePlaceholder / CmdCloseSubstratePlaceholder / CmdOpenPrefsPopup / CmdClosePrefsPopup / CmdAdjustTimeScale`.
- `src/ui/PreferencesState.hpp` — refatorado para multi-window (array windowOpen/windowScroll), popup state, help/substrate placeholder flags.
- `src/ui/UiPanel.hpp` — kMenuTitles: "View" -> "Exibir".
- `src/ui/UiPanel.cpp` — dropdown Preferencias com 11 itens despachando cada um para `CmdOpenPreferencesWindow{tab}`; Ajuda dispara `CmdOpenHelpWindow{}`; widget Velocidade na toolbar.
- `src/ui/UiPreferencesPanel.hpp/.cpp` — reescrito: multi-window, popups (neural combo, color), scroll, friendly labels, neural combo, color picker com presets, help window com scroll, substrate placeholder.
- `src/app/App.hpp/.cpp` — captura mouse antes de InputRouter para detectar `pointInsideAnyWindow`; route do MouseWheelScrolled para `handleMouseWheel`; Esc fecha popup > help > substrate > qualquer window aberta; dispatch dos 11 comandos novos; `CmdAdjustTimeScale` faz clamp e re-aplica `configureFromParameters`.
- `src/sim/SimulationRunner.cpp` — applyCommand passa pelos 11 comandos novos como no-op.
- `src/main.cpp` — flags `--phase23-hotfix-*`.
- `src/systems/Phase22_1Diagnostics.cpp` — testes 10/12 aceitam tanto o roteamento antigo quanto o novo.
- `CMakeLists.txt` — `Phase23_1Diagnostics.cpp`.

## Resultados dos testes

- Build Debug: OK.
- Build Release: OK (sem warnings novos).
- `--phase23-hotfix-selftest`: **PASS (58 checks)**.
- `--phase23-selftest`: **PASS (189 checks)**.
- `--phase22-hotfix-selftest`: **PASS (26 checks)** (atualizado para Phase 23.1).
- `--phase22-selftest`: **PASS (155 checks)**.
- Regressoes Phase 7-22: **TODAS PASS** (14, 15, 23, 21, 30, 49, 42, 71, 72, 109, 110, 111, 133, 134, 150, 155).
- `--phase23-hotfix-benchmark`:

```
scenario,windows_open,popups,parameters_visible,total_ms,avg_op_us,notes
no_windows,0,0,0,0.0000,0.0000,
simulation_only,1,0,24,0.0153,0.6375,
physics_only,1,0,28,0.0093,0.3321,
vision_only,1,0,9,0.0207,2.3000,
neural_only,1,0,54,0.0118,0.2185,
appearance_only,1,0,14,0.0075,0.5357,
performance_only,1,0,13,0.0075,0.5769,
all_windows,7,0,147,0.0526,0.3578,
```

Custo sub-microsegundo por parametro mesmo com 7 janelas abertas e 147 parametros visiveis. Filtragem por aba e quase gratuita.

- Smoke test do executavel: abre sem crash, mensagem inicial: `"AgentBioSimCpp Phase 23.1: UI com janelas independentes + slider de velocidade ..."`.

## Validacao visual/manual

Smoke test confirma que o app abre, renderiza, e fecha limpo. A validacao visual ao vivo (cliques em menu Preferencias > cada janela > scroll > color picker > combo neural > slider de velocidade) precisa ser feita por voce. A camada headless valida o roteamento completo dos comandos.

## Como a Microfase 22.1 foi protegida

- `Renderer`, `Camera2D`, `App::handleResize` nao tocados.
- `InputRouter` so teve sua relacao com `MouseWheelScrolled` desviada antes do `handleEvent` (no `App::processEvents`) quando `preferencesPanel_.handleMouseWheel` retorna `true`. Brush stroke / eraser stroke / pan / atalhos continuam intactos.
- Toolbar continua com 9 ferramentas (Pan removido). Widget Velocidade adicionado a direita.
- Selftest Phase 22.1: 26 checks PASS.

## Pendencias para fases futuras

- Color picker: input direto de hex/HSV ficou para Fase 24/26 — a versao 23.1 tem presets + steppers RGB suficientes para uso real.
- Drag de janelas: na 23.1 as janelas tem posicao fixa cascateada. Drag interativo entra na Fase 24/29.
- Slider draggable continuo: o widget Velocidade na 23.1 e step-based (`/2`, `*2`, `reset`). Slider continuo entra na Fase 24/29.
- Edicao de texto livre para int/float: continua com steppers (sem input de texto SFML-native).

## Confirmacoes finais

- **Nenhum arquivo Python foi alterado.**
- **Fase 24 NAO foi iniciada.**
- **Save/load NAO foi implementado.**
- **Editor genetico completo NAO foi implementado.**
- **Microfase 22.1 NAO regrediu.**
