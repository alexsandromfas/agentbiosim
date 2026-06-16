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
- **Fase 26 (Agente Selecionado e Visualizador Neural) concluida** (ver
  `PHASE_26_SELECTED_AGENT_NEURAL_VIEWER_STATUS.md`): painel retratil do agente (abas Genoma / Rede
  Neural), visualizador neural por tipo (MLP/Gated/Shortcut/Modulated/RNN/NEAT) via `neural::NeuralView`
  read-only, trace-on-demand no `NeuralSystem` (custo zero quando oculto), overlay de visao do
  selecionado. `--phase26-selftest` (73 checks) e `--phase26-diagnostics`.
- **Fase 27 (Metricas, Inteligencia, Logs, Profiler) concluida** (ver
  `PHASE_27_METRICS_INTELLIGENCE_PROFILER_STATUS.md`): `core::Profiler`+`ScopedTimer` por sistema
  (runner instrumenta runOneStep; App add Render/Ui), `systems::MetricsSystem` (series globais/por
  especie + smart factor de grupo), `core::Logger` (niveis/heartbeat/crash hook), janela de Metricas
  ImGui (graficos + tabela do profiler). Tudo OFF por padrao, custo ~zero. `--phase27-selftest`
  (19 checks) e `--phase27-diagnostics`.
- **Fase 28 (Save/Load) concluida** (ver `PHASE_28_SAVE_LOAD_AUTOSAVE_STATUS.md`): formato
  `.agentbiosim` (JSON versionado, modo simples — sem RNG state/bit-exact, sem legado Python, por
  decisao do usuario), `io/Json` + `io/SaveFile` + `io/FileDialog` (Win32), `neural::BrainSerializer`
  (6 tipos, friend), `sim::SimulationSnapshot` + runner `snapshot()/restore()`, menu Arquivo
  Salvar/Salvar Como/Abrir, autosave em thread de fundo (padrao 30 min) e export/import de organismo
  (`.organism`). `--phase28-selftest` (128 checks).
- **Fase 29 (Benchmark Runner) concluida** (ver `PHASE_29_BENCHMARK_RUNNER_STATUS.md`):
  `bench/Benchmark` headless (`--phase29-bench`), cenarios escala/visao/neural com repeticoes,
  relatorios CSV/JSON/MD com commit/build (CMake injeta `AGENTBIOSIM_GIT_COMMIT`), saida em
  `benchmarks/` (gitignored). Baseline: percepcao ~49% + neural ~34% do passo a 1000 agentes.
  `--phase29-selftest` (8 checks).
- **Fase 30 (Janela do Desenvolvedor) concluida** (ver `PHASE_30_DEV_PERFORMANCE_WINDOW_STATUS.md`):
  Exibir > Janela do Desenvolvedor — custo por sistema (barras de calor), sparklines, contadores do
  mundo, toggles de isolamento (restaurados ao fechar) e cenario de benchmark embutido em thread de
  fundo isolada. Runner: `setProfilerForced` + dev-toggles (`CmdSetDevSystemEnabled`).
  `--phase30-selftest` (13 checks) e `--phase30-diagnostics`.
- **Fase 31 (Paridade de UI) concluida** (ver `PHASE_31_UI_PARITY_STATUS.md`, checklist completo):
  4 bugs corrigidos — tecla H (ajuda), tecla V/menu Debug de visao (eram no-ops), Ajuda "WASD"
  (camera = setas; W removido), e "Resetar rede neural" por especie agora FUNCIONAL
  (`NeuralSystem::resetBrainsBySeed`, botao na aba Labels). `--phase31-selftest` (59 checks:
  atalhos via InputRouter headless + abas + fluxos criticos).
- **Dividas 8 e 9 resolvidas** (engine desacoplado de ui::; UI 100% ImGui). Divida 10: surface
  entregue (27/29/30); ataque na Fase 32, prova na Fase 33.
- **PENDENTE — Microfase 25.2:** botao "Aplicar" por grupo no editor genetico + mensagem de
  confirmacao "isto vai resetar a rede" ao mudar parametros neurais (sem mensagem ao mudar raio de
  visao; mudar nº de retinas afeta a entrada da rede).
- **PENDENTE — tidy-up da Divida 9:** remover fisicamente o codigo de view SFML morto
  (`UiPanel`, e os metodos de view de `UiPreferencesPanel`/`UiLeftDock`), extraindo antes as
  funcoes de MODELO (`prefsParametersForTab*`, `prefsApplyPending`, `editorParameters()`,
  `substratoParameters()`) para um arquivo proprio (ex.: `ui/PreferencesModel.*`), e remover os
  shims `ui/Command.hpp` / `ui/CanvasTool.hpp` migrando os testes para `core::`.
- **Fase 32 (Otimizacao) concluida** (ver `PHASE_32_OPTIMIZATION_SCALE_STATUS.md`): multithreading
  deterministico (percepcao + forward neural; escritas disjuntas por agente, sem RNG, contadores
  atomicos; knob `use_parallel_systems`) + Divida 5 resolvida (buffers thread_local, syncBrains sem
  set, query espacial const com QueryScratch). **~3x de speedup** (1000 ag: 444 passos/s; 2000:
  207/s; 5000: 43/s) com estado BIT-IDENTICO (golden via `--phase32-checksum`). REGRA da fase:
  qualquer otimizacao futura deve manter a ordem de FP por agente (sem SIMD em reducoes, sem
  threading com escrita cruzada) ou refazer a prova de regressao-zero.
- **Microfase 31.1**: substrato/comida mudam ao vivo (sem reset; `ApplyFlag::ReshapeWorld` +
  `applyWorldConfigLive` empurra agentes/comida pra dentro); min/max POR LABEL (max no nascimento
  via SpeciesStore na reproducao; resgate de minimo por passo em `applyPopulationRescue`).
- **Microfase 32.1**: defaults reais de populacao — bacteria min=5/max=150 (predator min=5);
  labels criadas nascem com 5/150 explicitos. Phase17 selftest atualizado para os novos defaults.
- **Microfase 32.2 (editor de genoma AO VIVO)**: "Aplicar a especie" NAO reseta mais (aplicava
  `runner_.reset()` e apagava labels criadas) e "Aplicar selecionados" NAO deleta mais — agora
  `SimulationRunner::applyEditorGenomeToSpecies/ToAgents` sobrescrevem genomas em-lugar (corpo
  atualizado, energia clampada, CEREBRO preservado); alvo do "Aplicar a especie" = label do 1º
  selecionado (rodape "Label alvo" no editor). Labels criadas tem genoma-template PROPRIO (clonado
  do 1º selecionado); atribuir a label preserva o genoma pessoal; reproducao honra o genoma do pai
  (`ReproductionConfig.honorGenome`, so no caminho fromRegistry; testes legados intactos).
  Templates default = copy-on-write. Golden 32 re-validado byte-identico (cenarios do checksum
  destravam o cap p/ manter cobertura de nascimentos). `--phase31-selftest` = 87 checks.
  LIMITES: visao/custos de energia continuam globais; arquitetura neural global (mudar
  hidden_layers/tipo recria os cerebros de todos — confirmacao = pendencia 25.2).
- **Microfase 32.3 (predacao por dieta)**: a predacao tinha gate global no flag legado
  `predators_enabled` (default false) — labels carnivoras criadas pelo usuario nunca predavam.
  `dietConfigFromRegistry` agora deixa a predacao sempre disponivel; quem decide e o `eatAgents`
  do genoma de cada agente (modelo da Fase 18). `predators_enabled` segue controlando apenas o
  spawn da especie predadora legada. Golden byte-identico (bacteria default nao preda).
  `--phase31-selftest` = 93 checks (bloco H).
- **Microfase 32.4 (piso minimo SEM respawn + predacao sem atraso)**: dois bugs reportados na
  simulacao real. DESCOBERTA: `agent_collision_enabled` tem default **true** no registry (o
  comentario "off by default" da Fase 21 estava errado), entao a colisao esta sempre ligada.
  (1) **Piso por bloqueio de morte:** o antigo `applyPopulationRescue` repunha a populacao
  criando organismos do nada ("quando morre um surge outro do nada"). Agora o piso e mantido
  BLOQUEANDO mortes: `DeathSystem::apply(...,&species_)` nao mata abaixo do `minPopulation`
  (contagem viva por especie decrementada ao matar, nunca fura o piso no lote) e a predacao
  tambem respeita o piso (`InteractionSystem::applyWithDiet(...,&species_)` protege presa cuja
  especie esta no minimo). `population_min_rescue_enabled` agora default **false** (respawn vira
  opt-in legado). (2) **Predacao sem atraso:** a ordem dos sistemas virou `Movement -> Energy ->
  rebuild -> Interaction -> Collision` (era `...Collision -> Energy -> rebuild -> Interaction`) —
  a predacao registra o toque fresco ANTES de a colisao (separation=0.9) separar os corpos.
  Golden 32 MUDOU de forma intencional (colisao on por padrao); provado por isolamento que SO a
  reordenacao altera o digest (death/predacao/rescue sao no-op nos cenarios) e `--phase32-selftest`
  (determinismo) segue PASS. `--phase31-selftest` = 99 checks (bloco I).
- **Microfase 32.5 (visao POR LABEL)**: os checkboxes de visao (ver comida/organismos/tudo) nao
  apareciam no editor (nomes sem o infixo `retina_` em `editorParameters()`) e a visao era GLOBAL.
  Agora os see-flags vivem no `GenomeRecord` (`simulation::VisionConfig`, herda via cloneFrom,
  copiado do registry em `overwriteGenomeScalarsFromRegistry` — bootstrap + editor-apply per-label,
  igual a dieta da 32.2). `PerceptionSystem::computeInputs(...,const GenomeStore*)` resolve a visao
  por-agente (geometria da retina segue GLOBAL pois dimensiona a entrada da rede). "Ver tudo" =
  sem filtro de tipo (o caso mais SIMPLES; a selecao natural/RGB resolve). Golden byte-identico
  (visao default = defaults globais). `--phase31-selftest` = 103 checks (bloco J). Novo
  `--vision-bench`: filtrar por tipo e ~de graca; o custo e QUANTOS objetos entram na retina —
  "ver tudo" e o MAIS caro (~+70% na percepcao vs so-comida), nao o mais barato.
- **Fase 32.1 (visualizador de visao + see-flags no editor + type-mask)** — prompt em
  `MIGRACAO_C++SFML/PHASE_32_1_VISION_VIEWER.md` (nome "32.1" escolhido pelo usuario; nao
  confundir com a microfase 32.1 interna de defaults). (A) Os checkboxes de visao agora
  APARECEM na secao Visao do editor: o agrupamento `grp_vision` do ImGuiUi ainda usava os
  sufixos antigos (`see_food`) e nao casava os nomes corrigidos na 32.5 (`retina_see_food`).
  (B) Ao selecionar UM organismo a visao dele aparece automaticamente (sem precisar da tecla
  V; `selectedVisionOverlay` default true; 0 ou >1 selecionados = nao desenha). Desenho
  mode-aware no novo modulo `render/VisionOverlay`: cunhas/grid polar no modo setor, feixes +
  cone + pontos de hit no raycast (single/fullbody). (C) Otimizacao type-mask no
  `SpatialHash::queryRadiusInto` (golden-safe, ordem preservada): ver-so-comida ficou ~40%
  mais barato na percepcao (581->345us). Golden byte-identico; `--phase31-selftest` = 107
  checks (bloco K). LEMBRETE: o CMakeLists lista fontes EXPLICITAMENTE — arquivo .cpp novo
  exige adiciona-lo la + `cmake -S . -B build`.
- **Fase 34.1 (editor de genoma POR ESPECIE + apply granular ao vivo + Substrato em janela)**
  CONCLUIDA (ver `MIGRACAO_C++SFML/PHASE_34_1_SPECIES_EDITOR_STATUS.md`; prompt em `PHASE_34_1.md`).
  MUDANCA DE PARADIGMA: a "label" virou **especie**; o dock deixou de ter abas Editor/Substrato/
  Labels e passou a ter **uma aba por especie** (orelha tingida com a cor da especie, nome <=10,
  aba "+" ao final, retratil), cada aba sendo o editor daquela especie VINCULADO ao genoma dela.
  Editar um campo deixa a caixa LARANJA e o Enter aplica SO aquele campo ao vivo (resolve a dor de
  "mexer num campo e baguncar os outros"). Nucleo: `simulation/GenomeFields.{hpp,cpp}` (bridge unico
  campo<->genoma por chave-sufixo: `setGenomeField`/`genomeFieldValue`/`isGenomeField`) +
  `core::CmdSetSpeciesGenomeField` + `SimulationRunner::setSpeciesGenomeField` (escreve 1 campo no
  template + membros vivos, clone-on-write, atualiza raio/forma/clamp + dietSnapshot; rebuildSpatial
  so se body_size). Aba "+": com selecao = `CmdCreateSpeciesFromSelected`; sem selecao =
  `CmdCreateSpeciesDefault` (`SimulationRunner::createSpeciesDefault` -> genoma padrao + spawna 5).
  Por-especie AO VIVO = campos ja no GenomeRecord (corpo/energia/reproducao/dieta/flags-de-visao/
  mutacao); GLOBAIS (velocidade/morte/custos/geometria-de-visao/`hidden_layers`) ficam read-only
  marcados "(global)" e migram na 34.2. Modal de confirmacao no "Resetar rede neural" (apaga
  aprendizado) — gancho que a 34.2 reusa p/ geometria de visao. Substrato saiu do dock -> item no
  menu superior (entre Agente e Ajuda) abre janela (`UiState.showSubstrateWindow`); `population_min_
  rescue_enabled` virou knob IMEDIATO (dock sem botao Aplicar). Botoes "Aplicar a especie/
  selecionados" REMOVIDOS. GOLDEN `--phase32-checksum` BYTE-IDENTICO (engine inerte ate ser chamado
  por comando; UI nao toca no caminho deterministico). `--phase34-selftest` = 15 checks (PASS).
  Bateria 7-34 verde Debug+Release. `--phase31-selftest` = 112 checks. CMakeLists: +GenomeFields.cpp
  +Phase34Diagnostics.cpp.
- **Fase 34.2 (genoma completo — ESCALARES) CONCLUIDA** (grupo 1; ver
  `PHASE_34_2_FULL_GENOME_STATUS.md`, prompt `PHASE_34_2.md`). Migrados para o `GenomeRecord` (por
  indivíduo) os traços globais ESCALARES que NAO mudam a arquitetura da rede: `maxSpeed`/`maxTurn`/
  `allowReverse` (Movement), `moveCostV0`/`moveCostVmax` (Energy), `deathEnergy` (Death) + `energyCap`
  lido por-agente. Movement/Energy/Death::apply ganharam `const GenomeStore* genomes=nullptr` e leem
  por-agente (cfg local = config com overrides do genoma; nullptr=fallback global p/ selftests);
  runOneStep passa `&genomes_`. `overwriteGenomeScalarsFromRegistry` semeia os 6 do `prefix` (os
  `bacteria_*`/`predator_*` viram defaults de fabrica). GenomeFields ganhou os 6 (editor por-especie
  da 34.1 os edita AO VIVO; corrigido bug: editor usava `v0_cost`/`vmax_cost` inexistentes ->
  `metab_v0_cost`/`metab_vmax_cost` reais + labels). Save/load serializa os 6 (genomeToJson/From).
  **GOLDEN `--phase32-checksum` BYTE-IDENTICO** (bacteria le do genoma = valor global antigo; golden
  sem predadores, entao o unico delta por-prefixo — custo metab do predador — nao o afeta).
  `--phase34-selftest`=**26 checks** (blocos I/J/K: max_speed=0 congela, death_energy alto extingue,
  save/load preserva, determinismo c/ tracos distintos). Bench: Movement<5%/Energy~1%/Death~1%;
  Perception/Neural inalterados. Bateria 7-34 PASS Debug+Release (predador inclusive — agora usa o
  proprio custo metab; antes usava o do bacteria por bug, e nenhum teste quebrou). `death_by_age`/
  `death_age` NAO migrados (nenhum sistema os consome — seria placeholder).
- **PENDENTE — Fase 34.2 grupo 2 (decisao de risco do usuario):** geometria de visao
  (retina_count/eye_count/canais/input_mode/fov/eye_angle) + `movement_mode` — mudam o TAMANHO da
  entrada/saida da rede; tornar por-agente exige reescrever o layout flat do `PerceptionResult`
  (offsets por-agente) + o batching neural por assinatura (incluir geometria), mantendo o caso
  uniforme byte-identico. E a parte de maior risco a determinismo/FPS. Ficam GLOBAIS (marcados no
  editor) ate o usuario autorizar.
- **Proxima fase:** decidir 34.2 grupo 2 (acima) OU Fase 33 (paridade/perf C++ vs Python, fecha
  Divida 10; ver `PHASE_33.md`).

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
build/Release/AgentBioSimCpp.exe --phase32-selftest
build/Release/AgentBioSimCpp.exe --phase34-selftest   # Fase 34.1 (editor por especie)
build/Release/AgentBioSimCpp.exe --phase32-checksum    # GOLDEN: deve bater byte-a-byte
```
Diagnostics extras: `--phase26-diagnostics` (viewer neural), `--phase27-diagnostics`
(metricas/profiler), `--phase28-diagnostics` (save/load), `--phase29-bench` (suite de benchmark +
relatorios em benchmarks/), `--phase30-diagnostics` (overhead da janela do desenvolvedor).
Flags especiais: `--phase22-hotfix-selftest` (22.1), `--phase23-hotfix-selftest` (23.1),
`--phase23-hotfix2-selftest` (23.2). Sem flag, o exe abre a janela (UI ImGui).

## Como continuar noutra maquina

1. `git clone` do repositorio e abrir o Claude Code na raiz (este `CLAUDE.md` carrega sozinho).
2. Ler `C_SFML_Teste_Legado/MIGRACAO_C++SFML/MIGRATION_PHASES.md` (mapa) e o `PHASE_NN.md` da fase
   desejada.
3. Confirmar baseline: build Debug+Release + selftests 7–25 PASS antes de mexer.
4. So entao implementar, seguindo as regras invioláveis acima.
