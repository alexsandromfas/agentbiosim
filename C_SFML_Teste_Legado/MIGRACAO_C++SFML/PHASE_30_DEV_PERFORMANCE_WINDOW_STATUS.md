# Fase 30 — Janela do Desenvolvedor (performance in-app) (STATUS: concluida)

Data: 2026-06-10. Build Debug + Release limpos; regressoes 7 a 30 verdes;
`--phase30-selftest` PASS (13 checks); `--phase30-diagnostics` confirma overhead ~zero.
Esta fase so MEDE e MOSTRA — **nada foi otimizado** (otimizacao e a Fase 32).

## Onde mora

Menu **Exibir > Janela do Desenvolvedor** (logo abaixo de "Metricas e profiler"). Escolha: e uma
janela de observacao, como a de Metricas, entao vive no mesmo grupo do menu Exibir — sem criar um
menu novo so para um item. Toggle com check; estado em `UiState::showDevWindow`.

## O que cada secao mostra (`src/ui/DevWindow.{hpp,cpp}`)

1. **Custo por sistema** — uma linha por secao do profiler da Fase 27 (percepcao, neural, locomocao,
   colisao, energia, interacao, comida, reproducao, morte, spatial hash) + "Outros (glue)" (overhead
   = SimStep − soma) + Render/UI (por frame, informativos). Colunas: nome PT-BR, us/passo, % do passo
   e barra de calor (verde < 15% < ambar < 30% < vermelho). Ordenacao por custo (toggle).
2. **Historico** — sparklines (janela deslizante de 240 amostras, coletadas SO com a janela aberta):
   passo total + percepcao + neural + colisao (us), FPS, e us/passo instantaneo + medio.
3. **Mundo** — agentes (total e por especie), comida, obstaculos; cerebros por tipo neural;
   ocupacao do spatial hash (celulas ocupadas/total, max bucket, media/celula); memoria aproximada
   das stores (constantes documentadas por entidade + parametros dos cerebros × 8 bytes).
4. **Isolamento de custo (diagnostico)** — checkbox por sistema; desligar pula o sistema inteiro no
   passo e mostra o delta ao vivo. Aviso visivel de que a simulacao fica incorreta enquanto
   desligado; botao "Restaurar todos"; **tudo e restaurado automaticamente ao fechar a janela**.
5. **Cenario de benchmark embutido** — campos (agentes, comida, passos, seed, rede, visao) + "Rodar
   cenario": executa `bench::runScenario` (Fase 29) numa **thread de fundo com estado 100% isolado**
   (registry + runner proprios) — a simulacao corrente nao e tocada. Resultado (us/passo
   medio/min/max, passos/s, % por sistema) aparece na propria janela.

## Como le o profiler (Fase 27) e o runner (Fase 29)

- A janela LE `runner.profiler()` (averageUs/percentOfStep/lastUs/overheadAccumNs) — nao reinventa
  medicao. Enquanto aberta, `App::update()` chama `runner.setProfilerForced(true)`: o profiler liga
  **sem tocar** a preferencia `profiler_enabled` do usuario; ao fechar, volta ao valor do usuario.
- O cenario embutido chama `bench::runScenario(...)` da Fase 29 via `std::async` — mesmos numeros e
  formato do benchmark headless.

## Mudancas no engine (neutras, custo zero)

- `SimulationRunner`: `setProfilerForced(bool)`; toggles `devSystemEnabled_[10]` (indices =
  `core::ProfileSection` Perception..SpatialHash) lidos no inicio de cada chamada de sistema em
  `runOneStep` (um if por sistema; default tudo ligado); comando `CmdSetDevSystemEnabled{section,
  enabled}` (−1 = restaurar todos) tratado em `applyCommand`. Neural desligado passa `nullptr` de
  controls ao MovementSystem (caminho ja suportado).
- `NeuralSystem`: `brainTypeCounts()` e `approxBrainBytes()` (O(cerebros); chamados so com a janela
  aberta).

## Overhead medido — `--phase30-diagnostics` (Release, 30 iters)

| agentes | profiler off (ms/passo) | profiler forcado (ms/passo) |
|---:|---:|---:|
| 100 | 0.41 | 0.35 |
| ~315 | 1.28 | 1.29 |
| ~626 | 2.96 | 3.01 |
| ~1027 | 5.64 | 5.62 |

Diferencas dentro do ruido de medida. Janela oculta: `DevWindow::draw` retorna na primeira linha,
nenhum historico e coletado e o profiler nao e forcado — custo zero.

## Selftest — `--phase30-selftest` (PASS, 13 checks)

Fonte de dados da janela == benchmark da Fase 29 (mesmo cenario/seed): mesmo sistema mais caro
("neural" no cenario de 150 agentes) e share dentro de 15pp; somas por sistema + overhead ~100% nos
dois caminhos; profiler forcado funciona headless (sem o parametro do registry); toggles: desligar e
religar ANTES do passo mantem a execucao bit-identica (checksum de posicoes/energia), sistema
desligado realmente pula o trabalho (Food off ⇒ sem reposicao; on ⇒ repoe), todos desligados nao
crasham; cenario embutido reproduzivel; contadores (cerebros por tipo, memoria, spatial stats)
legiveis headless.

## Gargalos que a janela revela (alimenta a Fase 32)

Os mesmos da baseline da Fase 29: **percepcao/visao (~49% a 1000 agentes)** e **rede neural (~34%)**.
A janela agora torna isso visivel ao vivo, com o delta dos toggles confirmando (desligar percepcao
derruba o passo em ~metade).

## Arquivos

Criados: `src/ui/DevWindow.{hpp,cpp}`, `src/systems/Phase30Diagnostics.{hpp,cpp}`, este STATUS.
Modificados: `src/sim/SimulationRunner.{hpp,cpp}` (force + toggles + comando + passthroughs),
`src/systems/NeuralSystem.{hpp,cpp}` (contadores), `src/core/Command.hpp`
(`CmdSetDevSystemEnabled`), `src/ui/UiState.hpp` (`showDevWindow`), `src/ui/ImGuiUi.{hpp,cpp}`
(membro + menu + chamada), `src/app/App.cpp` (profiler force), `src/main.cpp` (`--phase30-*`),
`CMakeLists.txt`. Divida 10 atualizada no `TECHNICAL_DEBT_REGISTER.md` (surface entregue; fecha na
Fase 33).

Nao avancar para a Fase 31 sem autorizacao (autorizada pelo usuario nesta rodada).
