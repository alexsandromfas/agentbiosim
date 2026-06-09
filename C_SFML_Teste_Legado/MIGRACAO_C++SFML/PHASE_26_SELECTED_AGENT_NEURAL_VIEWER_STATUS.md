# Fase 26 — Agente Selecionado e Visualizador Neural (STATUS: concluida)

Data: 2026-06-02. Build Debug + Release limpos; selftests 7 a 26 verdes; `--phase26-selftest`
PASS (73 checks); `--phase26-diagnostics` confirma custo ~zero do viewer quando oculto.

Cobre de `UI_INVENTORY.md`: painel retratil do agente (abas Genoma / Rede Neural), visualizador
neural por tipo, overlay de visao do agente selecionado. De `FEATURE_INVENTORY.md`: introspeccao do
agente, ActivationTrace, visualizacao de MLP/Gated/Shortcut/Modulated/RNN/NEAT. Referencia de
usabilidade: `sim/neural_viewer.py` e o painel de agente de `sim/ui.py` (usabilidade, nao arquitetura).

## Escopo entregue

1. **Painel do agente selecionado** (Dear ImGui, retratil) — abre ao selecionar um agente, some ao
   limpar a selecao. Menu `Agente > Painel do agente` (checkable) e `Visualizador neural`.
   - **Aba Genoma** (read-only): especie/label, energia, idade, raio, posicao, cor, tipo de cerebro,
     geracao, tamanho do corpo, energias (inicial / reproducao / maxima), taxa e forca de mutacao,
     dieta (come comida / organismos). Edicao continua no Editor Genetico (nota no proprio painel).
   - **Aba Rede Neural**: visualizador do cerebro + toggles "Preencher painel" (layout fixed vs fill)
     e "Mostrar visao".
2. **Visualizador neural por tipo** — desenho uniforme por colunas/linhas a partir de um
   `neural::NeuralView`:
   - **MLP / Gated / Shortcut / Modulated**: camadas em colunas, nodes coloridos pela ativacao do
     ultimo forward (verde +, vermelho -), arestas coloridas pelo peso (sinal -> cor, magnitude ->
     espessura/alpha). Entrada (coluna 0) usa o vetor de entrada real; demais colunas usam o
     ActivationTrace.
   - **RNN simples**: idem + faixa do **estado recorrente** (uma barra assinada por unidade) e leitura
     de `decay`/`clip`.
   - **NEAT (comum / simplificada / recorrente)**: grafo com nodes posicionados pela `layer` do no
     (entradas a esquerda, saidas a direita), arestas a partir das conexoes (desabilitadas omitidas,
     recorrentes em ambar), contadores `habilitadas/total` e `recorrentes`.
   - **Layout fixed vs preencher painel** (paridade Python): fixed usa espacamento constante e rola;
     fill estica para a area disponivel.
3. **Overlay de visao do agente selecionado** — reusa as rays de debug das Fases 11/12
   (`VisionDebugData`, ja renderizadas por `Renderer::drawVisionDebug`). Ligavel pelo checkbox; com o
   toggle desligado o engine nao pede dados de debug (custo zero).
4. **Selecao** — o painel usa o primeiro id selecionado ainda vivo; trocar/limpar selecao atualiza ou
   esconde; remover o agente selecionado limpa com seguranca (swap-remove + alvo invalida sozinho).

## Como o viewer le dados SEM mutar o engine (arquitetura)

- **Engine headless intacto.** Nenhum tipo de `ui/` entrou em `sim/`, `systems/`, `neural/`,
  `perception/`. O viewer (em `ui/ImGuiUi.cpp`) le pela API read-only do runner.
- **Trace-on-demand.** `NeuralSystem` ganhou um *trace target* (id de agente; 0 = nenhum). No forward
  de cada passo, somente o agente alvo recebe um `ActivationTrace` e tem um `NeuralView` reconstruido;
  todos os outros usam o forward sem trace (sem custo). O forward do alvo continua sendo o forward
  real (avanca o estado recorrente de RNN/NEAT) — apenas grava as ativacoes. Um contador
  (`traceCount()`) cresce so quando ha alvo.
- **`neural::NeuralView`** (`src/neural/NeuralView.{hpp,cpp}`) e a unica representacao read-only
  consumida pela UI e pelo selftest: bundle topologia (do cerebro) + ativacoes (do trace) num modelo
  uniforme de nodes (coluna/linha/kind/ativacao) + arestas (peso/enabled/recurrent), construido via
  `std::visit` sobre o `BrainVariant`. Acessores `weights()/biases()` read-only foram adicionados aos
  cerebros densos e a RNN (NEAT ja expunha `nodes()/connections()`). A UI nunca toca internamente os
  cerebros.
- **Sinal UI -> engine por estado, nao por mutacao.** `ImGuiUi` marca em `UiState`
  (`neuralTraceActive`, `neuralTraceAgent`) quando a aba Rede Neural esta visivel; `App::update()` le
  isso e chama `runner_.setNeuralViewerTarget(...)` / `clearNeuralViewerTarget()` antes do passo.
  Idem para a visao (`selectedVisionOverlay` -> `setVisionDebugTarget`). Latencia de 1 frame, esperada
  (o trace mostrado e sempre o do ultimo passo).

## Custo quando oculto

`--phase26-diagnostics` (Release, 30 iteracoes/medida, sem predadores):

| tipo | agentes | viewer off (ms/step) | viewer on (ms/step) | trace_count off/on |
|---|---|---|---|---|
| mlp | ~1000 | 5.52 | 5.64 | 0 / 31 |
| simple_rnn | ~1000 | 5.82 | 5.80 | 0 / 31 |
| neat | ~1000 | 6.03 | 5.97 | 0 / 31 |

A diferenca on/off fica dentro do ruido de medida — o trace e construido para **um** agente por passo.
Com o viewer oculto `traceCount == 0` (validado pelo selftest e pelos diagnostics).

## Testes — `--phase26-selftest` (PASS, 73 checks)

1. Para os 8 tipos de cerebro: tamanho de saida correto; para densos/RNN o ActivationTrace do ultimo
   forward bate com a saida; para NEAT ha `neatNodes`. `buildNeuralView` produz view valida com nodes,
   colunas e (densos) arestas, e a coluna de entrada reflete o input.
2. Viewer oculto: `neuralTraceCount() == 0` e sem view.
3. Selecionar agente expoe view valida + genoma (GenomeRecord) e trace populado.
4. Remover o agente alvo nao quebra (sem crash; view fica stale/limpa).
5. Trocar tipo neural (RNN/NEAT/recorrente) mantem o caminho do viewer correto ponta a ponta.

## Arquivos

Criados: `src/neural/NeuralView.{hpp,cpp}`, `src/systems/Phase26Diagnostics.{hpp,cpp}`,
este STATUS.
Modificados: `src/neural/{MLP,GatedMLP,ShortcutMLP,ModulatedMLP,SimpleRNN}Brain.hpp` (acessores
`weights()/biases()` read-only); `src/systems/NeuralSystem.{hpp,cpp}` (trace target + captura + view +
contador); `src/sim/SimulationRunner.{hpp,cpp}` (API do viewer + alvo de visao + preenchimento de
debugRequest); `src/ui/UiState.hpp` (campos do painel + sinais de trace/visao); `src/ui/ImGuiUi.{hpp,cpp}`
(painel inspector, canvas neural, faixa recorrente, menu Agente); `src/app/App.cpp` (alvo de
trace/visao por frame + passa visionDebug ao renderer); `src/main.cpp` (`--phase26-selftest` /
`--phase26-diagnostics`); `CMakeLists.txt` (novas fontes).

## Fora de escopo / pendencias (Fase 27+)

- Edicao de genoma pelo painel — permanece no Editor Genetico (proposital).
- "Resetar rede neural por especie" pesado — segue desabilitado (infra existe em
  `NeuralSystem::resetForSpecies`, mas o gatilho de UI fica para uma microfase, com confirmacao).
- Metricas globais / graficos de inteligencia / profiler — Fase 27.
- Realce de gates por node e contribuicao de shortcut por aresta sao expostos no `NeuralView`
  (`gates`, `shortcut`) mas ainda nao desenhados de forma dedicada; o grafo ja mostra pesos e
  ativacoes. Pode virar polimento de UI numa microfase 26.x ou na paridade da Fase 31.
- Visao do agente selecionado quando **pausado**: mostra o ultimo estado capturado (nao recaptura sem
  um passo) — comportamento aceitavel; um "passo unico para recapturar" pode ser adicionado depois.

Nao avancar para a Fase 27 sem autorizacao explicita do usuario.
