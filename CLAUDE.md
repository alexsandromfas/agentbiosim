# AgentBioSim — Guia do projeto para o Claude Code

Este arquivo e lido automaticamente pelo Claude Code em qualquer maquina. Ele carrega o
contexto essencial para continuar a migracao com seguranca, mesmo sem o historico de conversa.

## O que e este projeto

Migracao de uma simulacao evolutiva 2D de **Python (PyQt + pygame)** para **C++/SFML/CMake**.
O objetivo NAO e portar o codigo Python — e construir uma **arquitetura nova, de alto nivel,
de alta performance, nivel arquiteto senior**. O codigo Python e a **referencia de USABILIDADE**
(disposicao de botoes, fluxo de menus, abas, o que cada controle faz), **nunca** de arquitetura
de codigo.

- Projeto C++: `C_SFML_Teste_Legado/AgentBioSimCpp/`
- Plano e prompts da migracao: `C_SFML_Teste_Legado/MIGRACAO_C++SFML/`
- Codigo Python (somente referencia, NUNCA editar): `sim/`
- Idioma de trabalho com o usuario: **portugues (PT-BR)**.

## Regras invioláveis (valem para TODA fase)

1. **NUNCA alterar nenhum arquivo Python** (`sim/*.py`, etc.). Pode ler para referencia de usabilidade.
2. **Nao avancar para a proxima fase sem autorizacao explicita do usuario.** O usuario diz
   "bora para a Fase N" e voce segue o prompt `PHASE_N.md` correspondente.
3. **Engine headless e sagrado:** `src/sim/`, `src/simulation/`, `src/systems/`, `src/neural/`,
   `src/perception/` NAO podem depender de SFML, Dear ImGui nem da camada `ui/`. A UI depende do
   engine, nunca o contrario. Comandos vivem em `src/core/` (`core::Command`), nao em `ui/`.
4. **Sempre buildar Debug E Release** e **rodar os selftests** antes de considerar uma fase pronta.
5. **Escrever um doc de status** `PHASE_NN_*_STATUS.md` ao final da fase.
6. **Commit por fase** (e planejamento/docs em commit separado). So commitar/pushar quando fizer
   sentido para a fase; o usuario costuma pedir push explicitamente.
7. **Trabalho incremental:** ler uma parte, corrigir essa parte, buildar, verificar — nao tentar
   fazer tudo de uma vez. Nao "remendar"; planejar.

## Arquitetura (o que preservar — e bom, nao mexer sem motivo)

- **Stores data-oriented (SoA):** `AgentStore`, `FoodStore`, `SpeciesStore`, `GenomeStore`,
  `ObstacleStore` guardam `std::vector` paralelos (swap-remove + indice por id). Cache-friendly.
- **Engine headless:** `sim::SimulationRunner` possui todos os stores + sistemas; `step(dt)` +
  `applyCommand(core::Command)`. Determinismo por seed, dt fixo.
- **UI desacoplada por comandos:** a UI emite `core::Command` numa `core::CommandQueue`; o engine
  aplica. `App` (AppController) e fino: janela SFML + Camera2D + Renderer + Runner + InputRouter + UI.
- **UI em Dear ImGui (desde a Fase 25):** `src/ui/ImGuiUi.{hpp,cpp}` (menu, toolbar, dock com
  abas Editor Genetico / Substrato / Labels, janelas de Preferencias, ajuda/sobre) + tema em
  `src/ui/ImGuiTheme.{hpp,cpp}`. Roteamento de eventos por `io.WantCaptureMouse/Keyboard`.
- **Dear ImGui + ImGui-SFML vendados** em `third_party/imgui` (v1.90.9) e `third_party/imgui-sfml`
  (v2.6), compilados como lib estatica `imgui_sfml` (CMake). `build/` e gitignored — reconstruir.
- **Modelo de parametros:** `ParameterRegistry` (string-keyed). Rotulos amigaveis PT-BR, listas de
  enum e display PT-BR ficam em `src/config/ParameterMetadata.cpp`
  (`prefsFriendlyLabel`, `prefsFriendlyLabelBySuffix`, `prefsEnumValuesFor`, `prefsEnumDisplayLabel`,
  `prefsDecimalsFor`). Importante: o campo `domains` do registry guarda **TAGS, nao valores de
  enum** — `setValue` NAO valida string por domains (isso ja causou bug; ver Divida 9/25.1).

## Convencoes de UI (Fase 25.1)

- Numericos = **caixa de texto** (sem botoes +/-). Enums = **dropdown** (PT-BR no display, valor
  canonico armazenado). Bool = checkbox. Cor = color picker.
- Layout em tabela de 2 colunas (rotulo | valor estreito ~1/3). Rotulos sempre PT-BR.
- Casas decimais por parametro via `prefsDecimalsFor` (tamanho do mundo/raios/velocidades = 0).
- Editor Genetico agrupado por secoes (Gestalt): Corpo/locomocao, Energia/reproducao, Visao,
  Dieta, Rede neural.

## Estado atual (atualizar a cada fase)

- Fases 0–24 concluidas (24 inclui microfases 24.1/24.2).
- **Fase 25 (UI -> Dear ImGui) concluida** + microfase 25.1 (correcoes de bug/UX) concluida.
- **Dividas 8 e 9 resolvidas** (engine desacoplado de ui::; UI 100% ImGui).
- **PENDENTE — Microfase 25.2:** botao "Aplicar" por grupo no editor genetico + mensagem de
  confirmacao "isto vai resetar a rede" ao mudar parametros neurais (sem mensagem ao mudar raio de
  visao; mudar nº de retinas afeta a entrada da rede).
- **PENDENTE — tidy-up da Divida 9:** remover fisicamente o codigo de view SFML morto
  (`UiPanel`, e os metodos de view de `UiPreferencesPanel`/`UiLeftDock`), extraindo antes as
  funcoes de MODELO (`prefsParametersForTab*`, `prefsApplyPending`, `editorParameters()`,
  `substratoParameters()`) para um arquivo proprio (ex.: `ui/PreferencesModel.*`), e remover os
  shims `ui/Command.hpp` / `ui/CanvasTool.hpp` migrando os testes para `core::`.
- **Proxima fase (apos autorizacao):** Fase 26 — Agente Selecionado e Visualizador Neural
  (ver `PHASE_26.md`).

## Build e testes (Windows, MSVC, SFML 2.6.2)

ATENCAO (portabilidade entre maquinas): o `CMakeLists.txt` tem um caminho FIXO de SFML desta
maquina (`.../AntSimulator-master/.../third_party/SFML-2.6.2`). Em outro PC esse caminho nao
existe e o CMake da FATAL_ERROR. Solucao: ter o **SFML 2.6.2** (build MSVC) em algum lugar e
apontar o CMake com `-DSFML_ROOT=...`:
```
cmake -S . -B build -DSFML_ROOT="C:/caminho/para/SFML-2.6.2"
```
A fonte da UI carrega de `C:/Windows/Fonts/segoeui.ttf` (cai no fallback embutido se faltar).
Dear ImGui/ImGui-SFML ja estao vendados em `third_party/` (vem com o clone).

```
cd C_SFML_Teste_Legado/AgentBioSimCpp
cmake -S . -B build   # adicione -DSFML_ROOT=... se o caminho fixo nao existir nesta maquina
cmake --build build --config Debug
cmake --build build --config Release
```

Selftests headless (rodar TODOS como regressao; devem dar PASS):
```
build/Release/AgentBioSimCpp.exe --phase7-selftest
... ate ...
build/Release/AgentBioSimCpp.exe --phase25-selftest
```
Flags especiais: `--phase22-hotfix-selftest` (22.1), `--phase23-hotfix-selftest` (23.1),
`--phase23-hotfix2-selftest` (23.2). Sem flag, o exe abre a janela (UI ImGui).

## Como continuar noutra maquina

1. `git clone` do repositorio e abrir o Claude Code na raiz (este `CLAUDE.md` carrega sozinho).
2. Ler `C_SFML_Teste_Legado/MIGRACAO_C++SFML/MIGRATION_PHASES.md` (mapa) e o `PHASE_NN.md` da fase
   desejada.
3. Confirmar baseline: build Debug+Release + selftests 7–25 PASS antes de mexer.
4. So entao implementar, seguindo as regras invioláveis acima.
