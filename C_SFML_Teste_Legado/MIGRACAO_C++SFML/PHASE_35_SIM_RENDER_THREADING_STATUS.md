# Fase 35 — Sim/Render em 2 threads — STATUS (CONCLUÍDA)

Plano: `PHASE_35_SIM_RENDER_THREADING.md`. Análise que motivou: `PERF_PARALELIZACAO_ANALISE.md`.

## O que foi entregue
Pipeline produtor/consumidor: a **simulação** roda numa thread separada (`app::SimWorker`),
**sobreposta** ao desenho do mundo na thread principal, com **barreira por frame** (double
buffer via `app::RenderSnapshot`). Paralelismo de *tarefa* (sim ‖ render), complementar ao
paralelismo de *dados* já existente (percepção/neural da Fase 32).

Motivação (Amdahl): percepção+neural (~83% do passo) já eram paralelas desde a Fase 32, então
a cauda serial virou ~metade do passo. O split sim/render sobrepõe o passo INTEIRO (inclusive
essa cauda) com o render — ajuda independentemente da composição do passo — **e o determinismo
sai de graça** (a sim roda sozinha, mesma ordem; o engine não foi tocado no caminho do golden).

## Arquitetura (modelo de barreira)
Por frame, na thread principal (`App::runThreadedFrame`):
1. `simWorker_.wait()` — **BARREIRA**: espera o batch anterior (worker ocioso).
2. **Fase A (worker ocioso):** `processEvents` → `drainCommandsAndApply` →
   `updateEngineTargetsFromUi` → `maybeAutosave` → `beginFrame(N)` → captura de interpolação →
   `captureRenderSnapshot()` (copia agents/foods/obstacles/world/visão) → `buildImGuiFrame()`
   (ImGui lê o runner VIVO — só pode aqui) → `emitHeartbeat`.
3. `simWorker_.request(N, dt)` — **KICK**: o worker roda N× `runner.step(dt)` em background.
4. **Fase B (worker rodando):** `presentFrame()` desenha o **snapshot** (cópia) + ImGui::Render
   + `display`. Não toca nenhum store vivo.

`frame = A + max(render, simN)` em vez de `A + render + simN`. Latência de 1 frame no render
(desenha o batch anterior) — imperceptível.

## Hazards e resolução
- **Stores do runner:** worker escreve na Fase B; main só lê na Fase A (worker ocioso) ou lê a
  CÓPIA na Fase B. A barreira garante exclusão. ✔
- **Fila de comandos:** já era **main-only** (InputRouter/ImGuiUi produzem, `drainCommandsAndApply`
  consome — tudo na main). O worker nunca toca → **sem lock**. ✔
- **`Profiler`:** slots `stats_` por-seção disjuntos (worker: sim 0–9,12; main: Render(10)/Ui(11));
  o único cruzamento é `enabled_`, que virou `std::atomic<bool>`. ✔
- **Autosave/Save/Load/Reset/pick/targets:** todos na Fase A (worker ocioso). ✔
- **Ciclo de vida:** worker declarado APÓS `runner_` (init order) e destruído ANTES (join limpo);
  `run()` chama `simWorker_.stop()` ao sair (idempotente).

## Snapshot por cópia (zero churn no Renderer)
Stores são SoA (vector + unordered_map) → copiáveis. `RenderSnapshot` guarda cópias; o
`Renderer` mantém a assinatura (`const AgentStore&`), agora a do snapshot. Custo @5000 ≈ 0,7 MB
/ ~0,1 ms vs ~23 ms/passo — desprezível.

## Knob
`sim_render_threaded` (Preferências > Performance). **Default = true** (ligado), após a prova.
Caminho serial (`runSerialFrame`) mantido idêntico ao de sempre como fallback/depuração —
desligar o knob volta ao comportamento anterior na hora.

## Provas (DoD)
- **Golden `--phase32-checksum` BYTE-IDÊNTICO** ao baseline (engine intacto; só `Profiler::enabled_`
  virou atômico, off no golden).
- **Selftests 7–34 PASS** (Debug+Release) — não passam por `App`, inalterados.
- **`--phase35-selftest` PASS (5 checks):** worker-driven == serial-driven (**digest
  bit-idêntico**), no-op de 0 passos, criar/destruir sem request não trava, `wait()` ocioso +
  `stop()` idempotente.
- **Smoke janela threaded:** app rodou 6 s em modo threaded (3 espécies / 157 genomas / 3000
  comidas) sem crash/deadlock/stderr.
- Build Debug+Release limpos (só warnings pré-existentes).

## Arquivos
- **Novos:** `src/app/RenderSnapshot.hpp`, `src/app/SimWorker.{hpp,cpp}`.
- **Alterados:** `src/app/App.{hpp,cpp}` (split `render()`→`buildImGuiFrame`+`presentFrame`;
  extração `updateEngineTargetsFromUi`/`emitHeartbeat`; `runSerialFrame`/`runThreadedFrame`;
  membro `simWorker_`+snapshot; render lê snapshot), `src/core/Profiler.hpp` (`enabled_` atômico),
  `src/config/ParameterDefaults.cpp` (knob), `src/main.cpp` (`--phase35-selftest`),
  `CMakeLists.txt` (+SimWorker.cpp).

## Pendências / futuro
- **Medir FPS serial vs threaded:** o ganho é uma propriedade da JANELA (overlap), que as
  ferramentas headless (bench mede o ENGINE, inalterado) não capturam. A/B interativo: toggle do
  knob em Preferências, observar FPS na barra de título / Janela do Desenvolvedor. O ganho é
  maior quando render ≈ sim (cenas com muitos agentes a 1x).
- **Estágio 3 (futuro):** sim *free-running* (desacoplar a TAXA da sim do FPS de render) exige
  snapshotar a superfície de leitura do ImGui (editor/métricas) — refator grande, só se o build
  do ImGui virar o gargalo serial da Fase A.
