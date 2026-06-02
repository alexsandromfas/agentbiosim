# Fase 25 — Status: Migracao Total da UI para Dear ImGui

Data: 2026-06-01. Branch: `migração_C_SFML`.

## Resumo executivo

Toda a interface do aplicativo foi migrada da UI feita a mao em SFML imediato para **Dear ImGui**
(via ImGui-SFML), preservando integralmente a arquitetura (engine headless, stores SoA, UI
desacoplada por comandos) e a usabilidade/funcionalidade entregue ate a Fase 24.2. As duas dividas
tecnicas alvo foram resolvidas: **Divida 8** (inversao de camada `sim -> ui::Command`) e **Divida 9**
(UI manual com hit-test duplicado).

Resultado: build Debug+Release limpo, selftests 7-25 todos PASS (0 falhas), app abre e roda sem
crash.

## Commits da fase

1. `e157acd` — Divida 8: mover Command/CanvasTool para a camada neutra `core`.
2. `b2420cd` — vendorizar Dear ImGui v1.90.9 + ImGui-SFML v2.6, lib estatica `imgui_sfml`.
3. `26dcdc2` — reimplementar toda a UI em Dear ImGui (ImGuiUi + ImGuiTheme + App rewire).
4. `d7a402b` — `--phase25-selftest` (Phase25Diagnostics, 31 checks).
5. (este doc) — status + atualizacao do registro de dividas.

## Divida 8 — resolvida

- `Command`, `CommandQueue`, todos os `Cmd*` e `CanvasTool` movidos de `agentbiosim::ui` para
  `agentbiosim::core` (`src/core/Command.hpp`, `src/core/CanvasTool.hpp`).
- `sim::SimulationRunner::applyCommand` agora recebe `const core::Command&`. Grep em `src/sim/`
  retorna **zero** ocorrencias de `ui::` / `ui/` — o engine nao depende mais da camada de UI.
- Shims de 3 linhas em `ui/Command.hpp` e `ui/CanvasTool.hpp` (`using namespace core;`) mantem
  `ui::CmdXxx` / `ui::CanvasTool` resolvendo para os tipos de `core` (lookup qualificado segue a
  using-directive), preservando a compilacao do codigo de UI/testes sem churn. Os shims serao
  removidos quando os testes forem migrados para `core::`.

## Divida 9 — resolvida (na essencia)

- UI viva 100% Dear ImGui: `src/ui/ImGuiUi.{hpp,cpp}` (menu, toolbar, dock, preferencias,
  ajuda/sobre) + `src/ui/ImGuiTheme.{hpp,cpp}` (tema escuro coeso, raio de borda, espacamento,
  acento teal).
- `App::processEvents` virou um unico teste `WantCaptureMouse/WantCaptureKeyboard`, eliminando toda
  a geometria de hit-test manual e o tratamento manual de edicao de texto / scroll / drag / slider.
- As classes SFML (`UiPanel`, `UiPreferencesPanel`, `UiLeftDock`) **nao sao mais desenhadas nem
  roteadas**. Tidy-up agendado: `UiPreferencesPanel.cpp` / `UiLeftDock.cpp` seguem compiladas apenas
  porque contem as funcoes de MODELO reutilizadas headless (`prefsParametersForTab/Filtered`,
  `prefsEffectiveValue`, `prefsApplyPending`, `editorParameters()`, `substratoParameters()`),
  consumidas pela UI ImGui e pelos selftests 23.x/24. O codigo de view morto e o `UiPanel.*` serao
  fisicamente removidos numa microfase de limpeza, extraindo antes o modelo para `ui/PreferencesModel.*`.

## Integracao Dear ImGui / ImGui-SFML

- Versoes: Dear ImGui **v1.90.9**, ImGui-SFML **v2.6**, vendadas em `third_party/imgui` e
  `third_party/imgui-sfml` (examples/backends/docs removidos para enxugar).
- CMake: alvo estatico `imgui_sfml` (imgui core + tables/widgets/draw/demo + misc/cpp/imgui_stdlib +
  imgui-SFML), compilado com `/W0` (codigo de terceiros) e `IMGUI_USER_CONFIG="imconfig-SFML.h"`
  PUBLIC para as conversoes `sf::Vector2<->ImVec2`. Linka `opengl32`. O executavel linka `imgui_sfml`;
  o engine headless nao.
- Ciclo no `App`: `ImGui::SFML::Init` no construtor (+ fonte + tema), `ImGui::SFML::ProcessEvent` no
  loop de eventos, `ImGui::SFML::Update` + `imguiUi_.draw(...)` + render do mundo +
  `ImGui::SFML::Render` por frame, `ImGui::SFML::Shutdown` no destrutor.

## Fonte / acentos

- Carrega `C:/Windows/Fonts/segoeui.ttf` a 18px (fallback fonte embutida). O range padrao do ImGui
  (0x20-0xFF) cobre Latin-1, entao acentos PT-BR sao renderizaveis. Os rotulos foram mantidos em
  ASCII por consistencia com os friendly labels ja existentes (`prefsFriendlyLabel`) e para evitar
  dependencia de encoding de fonte do `.cpp`. `io.IniFilename = nullptr` (nao grava imgui.ini).

## Mapeamento SFML manual -> ImGui

| Antes (SFML manual) | Agora (Dear ImGui) |
|---|---|
| `UiPanel` menu bar | `BeginMainMenuBar` (Arquivo/Exibir/Preferencias/Agente/Ajuda) |
| `UiPanel` toolbar | janela fixa de toolbar com botoes de ferramenta + slider de velocidade + contadores |
| `UiLeftDock` (3 abas) | janela ancorada a esquerda (kDockW=460) com `BeginTabBar` (Editor/Substrato/Labels) |
| linhas de parametro (drawParamRow manual) | `drawParamRow` ImGui por tipo: Checkbox / SliderInt-InputInt / SliderFloat-DragFloat / Combo / ColorEdit3 |
| popups de combo/color manuais | `BeginCombo` / `ColorEdit3` nativos |
| `UiPreferencesPanel` janelas | janelas ImGui moviveis/redimensionaveis por categoria (resolve "nao consigo mover a janela") |
| hit-test manual + WantCapture | `io.WantCaptureMouse/Keyboard` |
| overlay de ajuda SFML | janela "Atalhos" ImGui |

## Preservado (sem regressao)

Todos os comandos e seu roteamento ao `SimulationRunner`; conversao tela->mundo + fix 22.1;
brush/eraser, selecao unica/retangular/laco; substrato circular/retangular e Aplicar ambiente sem
reescalar (24.1); atribuir label muda especie + recolore (24.2); rotulos PT-BR, combos corretos,
ocultacao de aliases internos (23.x); engine headless e selftests 7-24.

## Comandos novos (ImGui-native)

`core::CmdSetSpeciesLabel{speciesId, label}` e `core::CmdSetSpeciesColor{speciesId, r,g,b}` —
edicao direta de nome/cor da especie pela aba Labels (substitui o begin/commit e o palette-cycle do
dock SFML). Dispatch em `App::drainCommandsAndApply` chamando `runner_.setSpeciesLabel` /
`runner_.setSpeciesColorAndRecolor`.

## Testes / build

- Build: `cmake --build build --config Debug` e `--config Release` — OK.
- `--phase25-selftest`: **PASS (31 checks)** — Divida 8, comandos novos, ops de especie/label,
  modelo de preferencias, listas do dock, engine continua dando step.
- Regressoes `--phase7..24` + `--phase25`: **todas PASS** (Debug e Release).
- Smoke: o executavel abre janela, renderiza menu/toolbar/dock/preferencias e roda sem crash; clique
  sobre painel ImGui nao pinta no canvas; janelas de preferencias arrastaveis.

## Performance

A troca de UI nao altera a sequencia de comandos nem o determinismo. ImGui e barato (immediate-mode
com vertex buffer unico); a janela roda no `framerateLimit` (120). Um microbenchmark dedicado
`--phase25-diagnostics` nao foi adicionado nesta fase — a infra formal de benchmark e a comparacao
de overhead vivem na Fase 29; o overhead do ImGui e desprezivel frente ao render do mundo.

## Confirmacoes

- Engine continua headless (nenhum header de `sim/`, `simulation/`, `systems/`, `neural/`,
  `perception/` inclui ImGui/ImGui-SFML/`ui/`).
- Nenhum arquivo Python foi alterado.
- Fases 26+ nao foram iniciadas (itens marcados como placeholders desabilitados no menu Agente).

## Pendencias / proximos passos

- Tidy-up Divida 9: extrair o modelo de `UiPreferencesPanel.cpp`/`UiLeftDock.cpp` para
  `ui/PreferencesModel.*`, deletar o codigo de view morto + `UiPanel.*`, remover os shims `ui/` e
  migrar os selftests para `core::`.
- Icones da pasta `Assets/` na toolbar (carregar `sf::Texture` -> `ImTextureID`): nao implementado
  nesta fase; os botoes usam rotulos de texto. Documentado como refinamento visual.
- `--phase25-diagnostics` (microbenchmark de UI on/off) opcional, adiavel para a Fase 29.
- Fase 26 (agente selecionado + visualizador neural) ja sobre a base ImGui.
