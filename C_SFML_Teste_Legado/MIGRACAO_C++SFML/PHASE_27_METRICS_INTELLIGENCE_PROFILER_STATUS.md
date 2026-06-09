# Fase 27 — Metricas, Inteligencia, Logs, Diagnostico e Profiler (STATUS: concluida)

Data: 2026-06-09. Build Debug + Release limpos; selftests 7 a 27 verdes; `--phase27-selftest`
PASS (19 checks); `--phase27-diagnostics` confirma overhead ~zero do profiler/metricas.

Cobre de `UI_INVENTORY.md`: graficos/metricas. De `FEATURE_INVENTORY.md`: metricas por step/especie,
inteligencia de grupo, logs/heartbeat/crash. Referencia de usabilidade: `sim/profiler.py`,
`sim/intelligence.py`, `sim/diagnostics.py`, `sim/ui.py` (usabilidade, nao arquitetura).

## Escopo entregue

1. **Profiler por sistema** (`core/Profiler.{hpp,cpp}`) — o item-chave (fonte da Fase 29/30).
2. **MetricsSystem** (`systems/MetricsSystem.{hpp,cpp}`) — series temporais globais e por especie em
   ring buffers, amostragem configuravel.
3. **Inteligencia de grupo** — "smart factor" por conversao de oportunidade alimentar (paridade
   conceitual com `sim/intelligence.py`), embutido no MetricsSystem.
4. **Logger** (`core/Logger.{hpp,cpp}`) — niveis, arquivo por sessao, heartbeat, hook de crash.
5. **Janela de Metricas** (Dear ImGui) — graficos de linha + tabela do profiler por sistema.

## API do profiler (como a Fase 29 e a Fase 30 vao consumir)

`core::Profiler` + `core::ScopedTimer`. Secoes (`ProfileSection`): perception, neural, movement,
collision, energy, interaction, food, reproduction, death, spatialhash, render, ui, e **simstep**
(envolve o step inteiro). O `SimulationRunner` instrumenta o proprio `runOneStep` (cada sistema num
`ScopedTimer`); `App` adiciona `Render` e `Ui` por frame via `runner.profilerMutable()`.

Modelo de dados por secao (`Profiler::Stat`): `accumNs` (total), `calls`, `lastNs` (instantaneo),
`maxNs`. Consultas: `accumulatedUs(sec)`, `averageUs(sec)` (= us/passo p/ secoes de sim, us/frame p/
render/ui), `lastUs(sec)`, `percentOfStep(sec)`, `stepCount()`, `simSectionsAccumNs()`,
`overheadAccumNs()` (= SimStep − soma das secoes de sim), `report()`.

- **Fase 29 (benchmark runner)**: liga `profiler_enabled`, roda N steps com seed fixa, le
  `averageUs`/`percentOfStep` por secao e exporta CSV/JSON/Markdown.
- **Fase 30 (janela do desenvolvedor)**: le os mesmos campos a cada frame para sparklines/% por
  sistema e o overhead. A janela de metricas desta fase ja e um primeiro consumidor (tabela us/passo
  + % + calls).

**Desligavel e barato:** OFF por padrao. Um `ScopedTimer` com o profiler desligado nao le relogio e
nao registra (guarda `nullptr`). O selftest prova: profiler OFF ⇒ `stepCount()==0` e secoes zeradas.

## Custo (overhead) — `--phase27-diagnostics` (Release, 30 iters/medida)

| agentes | off/off (ms) | metrics on (ms) | profiler on (ms) | ambos (ms) |
|---|---|---|---|---|
| ~100  | 0.39 | 0.34 | 0.34 | 0.36 |
| ~300  | 1.24 | 1.24 | 1.22 | 1.24 |
| ~600  | 3.14 | 3.18 | 3.03 | 3.04 |
| ~1000 | 5.73 | 5.63 | 5.62 | 5.73 |

As diferencas ficam dentro do ruido de medida — metricas e profiler ligados nao alteram o tempo de
step de forma relevante. Invariante de aninhamento validada: soma(secoes de sim) ≤ SimStep (com folga
de timer); overhead = SimStep − soma.

## MetricsSystem

Entrada por step (`MetricsStepInput`): births, deaths, foodsConsumed, predationEvents, energia ganha
por comida/predacao, dt, raio de visao de referencia. Saida: ring buffer global (`MetricsSample`:
populacao, comida, energia media, nascimentos, mortes, comida comida, predacao, smart factor) +
series por especie (populacao e energia media). `series(MetricField)` devolve `vector<float>` pronto
para `ImGui::PlotLines`. Amostragem por `metrics_sample_interval`, janela por `metrics_max_samples`.

**Inteligencia (smart factor de grupo):** `100·(1 − e^(−ratio))`, `ratio = intakeRate / oportunidade`,
`oportunidade = densidadeGlobalDeComida · populacao · area_de_visao`, suavizado (α=0.08). Mesma
forma saturante/suavizada do `sim/intelligence.py`, ao nivel de grupo (ver pendencias para per-agente).

## Logger / diagnostico

`core::Logger` (singleton). Niveis off/error/warn/info/debug (param `log_level`). Arquivo por sessao
em `<exeDir>/logs/runtime/agentbiosim_<stamp>.log`. `log()` faz gate por nivel logo na entrada
(custo = uma comparacao quando off). `heartbeat(intervalo)` (intervalo de `diagnostic_heartbeat_minutes`).
`installCrashHandler()` registra um `SetUnhandledExceptionFilter` (Windows) que loga
`UNHANDLED_EXCEPTION` e deixa o handler padrao agir — sem recovery real (isso e Fase 28). `windows.h`
fica isolado em `Logger.cpp`.

## Parametros novos

`profiler_enabled`, `metrics_enabled` (bool, off), `metrics_max_samples` (600), `metrics_sample_interval`
(1), `log_level` (off). Categoria `performance.observability` (aba Performance). Rotulos PT-BR/EN e
enum de `log_level` em `ParameterMetadata.cpp`. Aplicados ao vivo (App trata como knobs imediatos,
sem pending/reset).

## Testes — `--phase27-selftest` (PASS, 19 checks)

Series: 1 amostra/step, ultima amostra == estado vivo, series por especie presentes, smart factor
calculado, **populacao determinista por seed**. Profiler: SimStep/step, perception 1×/step, spatialhash
2×/step, soma(secoes) ≤ SimStep, overhead bem-definido, **OFF ⇒ nada registrado**. Log: parsing de
niveis e gate OFF honrado.

## Arquivos

Criados: `core/Profiler.{hpp,cpp}`, `core/Logger.{hpp,cpp}`, `systems/MetricsSystem.{hpp,cpp}`,
`systems/Phase27Diagnostics.{hpp,cpp}`, este STATUS.
Modificados: `sim/SimulationRunner.{hpp,cpp}` (owns Profiler+Metrics, instrumenta runOneStep, toggles,
read API); `app/App.cpp` (Render/Ui scopes, install logger+crash, log level, heartbeat); `ui/UiState.hpp`
(`showMetricsWindow`); `ui/ImGuiUi.{hpp,cpp}` (janela de metricas + menu); `config/ParameterDefaults.cpp`
+ `config/ParameterMetadata.cpp` (params + labels/enum); `src/main.cpp` + `CMakeLists.txt`.

## Fora de escopo / pendencias

- **Inteligencia local (por agente) e por especie**: hoje so o smart factor de grupo (global). A
  versao por agente do Python precisa de acumuladores de intake POR AGENTE (food/prey energy eaten),
  que o engine ainda nao rastreia. Adicionar contadores por agente no InteractionSystem e expor
  local/species smart factor pode ser uma microfase 27.x; o NeuralViewer/Inspector seria o consumidor.
- Profiler render/ui: medidos por frame; a relacao SimStep/overhead vale so para as secoes de sim.
- Relatorio CSV/JSON/Markdown do profiler: a Fase 29 (benchmark runner) formaliza isso; aqui ha
  `Profiler::report()` (texto) e a tabela na UI.
- Janela do desenvolvedor rica (sparklines, toggles de sistema, benchmark embutido): Fase 30.

Nao avancar para a Fase 28 sem autorizacao explicita do usuario.
