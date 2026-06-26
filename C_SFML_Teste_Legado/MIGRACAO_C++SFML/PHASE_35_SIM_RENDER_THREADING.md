# Fase 35 — Desacoplar Simulação de Render/UI (pipeline em 2 threads)

> Plano **arquitetado** para o ganho identificado em `PERF_PARALELIZACAO_ANALISE.md`:
> sobrepor o **passo de simulação** com a **renderização** em threads separadas
> (paralelismo de *tarefa*/pipeline, não de dados). Executar em **estágios**, buildando
> Debug+Release e rodando os selftests + golden a cada estágio.

---

## 1. Objetivo e por que o determinismo sai DE GRAÇA

Hoje o laço de `App` é serial numa thread: `processEvents → update (N passos de sim) →
render`. Sim e render **não se sobrepõem**.

**Insight central que torna isto seguro:** toda a mudança vive em **`App`** (a casca da
janela). O **engine** (`SimulationRunner::runOneStep` e os sistemas) **não é tocado**. Os
selftests, o golden `--phase32-checksum` e os benchmarks **não passam por `App`** — eles
chamam o runner direto. Logo:

> **A prova de determinismo da Fase 32 continua válida por construção.** O worker roda
> exatamente o mesmo `runOneStep`, na mesma ordem, com os mesmos dados. Threadar `App` não
> pode mudar o digest. O golden tem de bater **byte-idêntico sem reprovar nada** (Classe A).

Ganho esperado: o menor entre (tempo de sim/frame) e (tempo de render/frame) deixa de
somar e passa a **sobrepor** — até ~2x no caso interativo comum (sim ≈ render) e nunca pior
que o serial (há knob de fallback).

---

## 2. Arquitetura alvo — modelo de BARREIRA por frame (double-buffer)

Descoberta que simplifica tudo: **a fila de comandos é tocada só pela main thread**
(InputRouter em `processEvents` e ImGuiUi em `render` PRODUZEM; `drainCommandsAndApply`
CONSOME — tudo na main). O worker só chama `runner_.step()`, que **nunca** toca a fila.
Portanto **a fila NÃO precisa de lock**. O único estado partilhado é os *stores* do runner,
e uma barreira por frame separa limpo as duas fases.

Linha do tempo de um frame (main thread):

```
[BARREIRA join]  ── espera o worker terminar os N passos do frame anterior (sim ociosa)
  │  (worker OCIOSO → a main pode ler/mutar o runner à vontade)
  ├─ drainCommandsAndApply()           // aplica comandos (reset/save/params/spawn...)
  ├─ set targets (profiler/neural/visão), maybeAutosave()
  ├─ N = timestep_.beginFrame(realDelta)
  ├─ captureRenderSnapshot()           // COPIA os stores p/ o snapshot (≈0,7ms @5000)
  ├─ ImGui::SFML::Update + imguiUi_.draw(runner_, …)   // lê runner VIVO (sim ociosa); emite cmds
  ├─ kickSimWorker(N)  ───────────────▶ worker roda N× runner_.step() EM BACKGROUND
  ├─ renderer_.render(SNAPSHOT)         // ←── SOBREPÕE o worker; desenha a CÓPIA
  ├─ ImGui::SFML::Render + window_.display()
  └─ (volta ao topo → BARREIRA join)
```

- **Fase A (sim ociosa):** main lê/muta o runner (comandos, ImGui, captura do snapshot).
- **Fase B (overlap):** worker roda os passos; main só toca o **snapshot (cópia)** + GPU.

`frame = A + max(render, simN)` em vez de `A + render + simN`. A barreira garante que as
duas fases nunca colidem nos stores.

### Snapshot por CÓPIA (zero churn no Renderer)
`AgentStore`/`FoodStore`/`ObstacleStore` são só `std::vector` + `unordered_map` →
**copiáveis** implicitamente. O snapshot guarda **cópias** dos stores + `World` +
`VisionDebugData` + escalares de overlay. O `Renderer` mantém a assinatura atual (recebe
`const AgentStore&`); passamos a referência **do snapshot**. Custo de cópia @5000 ≈ 0,7 MB
(~0,1 ms), desprezível perto de um passo (~23 ms). Sem refatorar o Renderer.

---

## 3. Análise de hazards (o que é partilhado e como a barreira resolve)

| Estado partilhado | Quem escreve | Quem lê | Resolução |
|---|---|---|---|
| Stores do runner (agents/foods/obstacles) | worker (Fase B) | main: snapshot (Fase A) + ImGui (Fase A) | **Barreira**: leitura só na Fase A (worker ocioso); na Fase B a main só lê a cópia |
| `CommandQueue` | main (produz+consome) | main | **Já é main-only**; worker não toca → sem lock |
| `Profiler::stats_` (array por seção) | worker: seções de sim (0–9,12); main: Render(10)/Ui(11) | dev window (Fase A) | **Slots DISJUNTOS** por índice → sem corrida nos `Stat` |
| `Profiler::enabled_` (bool) | `step()` no worker | `ScopedTimer` na main (Fase B, Render) | Tornar `std::atomic<bool>` (load/store relaxed) — 1 linha |
| Autosave (lê runner p/ serializar) | main em `maybeAutosave` | — | Mover p/ Fase A (worker ocioso); escrita em disco segue na thread de IO da Fase 28 |
| Save/Load/Reset/pick/targets | main (via comandos/estado) | — | Já caem na Fase A (drain/targets, worker ocioso) |

Conclusão: com a barreira por frame, **o único ajuste fora de `App` é `enabled_` virar
atômico** (Classe A, não afeta numérica). Todo o resto é orquestração em `App`.

---

## 4. Estágios (cada um buildável e testável isoladamente)

### Estágio 1 — `RenderSnapshot` + render a partir do snapshot (SEM thread)
Isola a mudança arriscada ("o que o render lê") da concorrência.
- **Novo** `src/app/RenderSnapshot.hpp`: struct com `simulation::World world; AgentStore
  agents; FoodStore foods; ObstacleStore obstacles; perception::VisionDebugData vision;
  bool visionActive; bool spatialOverlay; double spatialCellSize;` + contadores de `info`.
- **`App`**: membro `RenderSnapshot snapshot_;` + `void captureRenderSnapshot();` (copia os
  stores vivos). Chamar ao FIM de `update()` (depois dos passos).
- **`App::render()`**: trocar as leituras do mundo de `runner_.agents()/foods()/obstacles()/
  visionDebug()/world()` para `snapshot_.*`. ImGui **continua** lendo `runner_` (mono-thread,
  seguro). `info.agents/foods/obstacles` do snapshot.
- **CMakeLists**: +`src/app/RenderSnapshot.hpp` se necessário (header-only pode dispensar).
- **Prova**: build Debug+Release; selftests 7–34 PASS; `--phase32-checksum` byte-idêntico
  (engine intacto); rodar a janela e conferir **paridade visual** (agentes/comida/obstáculos/
  overlay de visão/seleção/interpolação iguais).
- **Risco**: baixo. Sem concorrência. Se algo do render depender de algo não copiado, aparece
  já aqui, fácil de achar.

### Estágio 2 — `SimWorker` (thread) + barreira no laço + knob de fallback
- **Novo** `src/app/SimWorker.{hpp,cpp}`: thread persistente com `request(N)` / `wait()` via
  `std::mutex`+`std::condition_variable` (sem spawnar thread por frame). Dona de um ponteiro
  para o `SimulationRunner`; o corpo roda `for i in N: runner.step(dt)`. Estado: `idle`/
  `running`. `wait()` é a **barreira**.
- **`core::Profiler::enabled_`** → `std::atomic<bool>` (load/store relaxed).
- **`App::run()`**: reestruturar para o modelo da Seção 2 (join → Fase A → kick → render →
  display). Mover `maybeAutosave` p/ a Fase A. `drainCommandsAndApply` + targets na Fase A.
- **Knob** `sim_render_threaded` (bool, registry). `true` = pipeline 2 threads; `false` =
  caminho serial atual (mantido como fallback e p/ depurar). Default: **false até provar**,
  depois flip p/ **true**.
- **Encerramento**: `window close` → `wait()` + parar o worker limpo (sem detach).
- **Prova**: build Debug+Release; selftests 7–34 PASS (não usam `App` → inalterados);
  `--phase32-checksum` byte-idêntico; teste manual: alternar o knob, conferir paridade visual
  e medir **FPS/throughput** (Janela do Desenvolvedor) serial vs threaded em 1000/2000/5000
  agentes; estabilidade (sem race/crash) por alguns minutos + autosave + save/load + reset.

### Estágio 3 — (FUTURO, fora do escopo) sim *free-running* (desacoplar TAXA)
Sobrepor também o ImGui exige snapshotar a superfície de leitura dos painéis (editor/
métricas/agente selecionado) — refator grande. Anotado para depois; só se a medição do
Estágio 2 indicar que o build do ImGui virou o gargalo serial (Fase A).

---

## 5. Critérios de aceite (DoD)
1. `--phase32-checksum` **byte-idêntico** ao golden atual (Estágios 1 e 2).
2. Selftests 7–34 PASS em Debug e Release.
3. Paridade visual: nada some/duplica/atrasa vs o serial (overlay de visão, seleção,
   interpolação, tema, grid do spatial hash incluídos).
4. `sim_render_threaded=false` reproduz exatamente o comportamento de hoje.
5. Ganho medido de FPS/throughput no caso sim≈render (reportar números reais).
6. Sem crash/race em sessão longa + autosave + save/load + reset + pintar obstáculo.

## 6. Riscos e mitigação
- **Race nos stores** → barreira por frame (worker ocioso em toda leitura da main). Knob de
  fallback serial p/ bissecar.
- **Algo do render não copiado p/ o snapshot** → pega no Estágio 1 (sem thread).
- **Custo da cópia** → medido; se pesar @ populações enormes, evoluir p/ copiar só campos de
  desenho (otimização posterior, não bloqueia).
- **Encerramento/joins** → `wait()` no close; worker sem `detach`.
- **`enabled_` bool** → atômico.

## 7. Decisões assumidas (avisar se divergir)
- Mantém ImGui lendo o runner vivo na Fase A (não snapshota o painel) — 80/20.
- Knob começa `false`, vira `true` após a prova do Estágio 2.
- Sem mudar o engine além de `Profiler::enabled_` atômico.
