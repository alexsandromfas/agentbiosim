# Fase 29 — Benchmark Runner e Experimentos Headless Formais (STATUS: concluido)

Data: 2026-06-10. Build Debug + Release limpos; selftests 7 a 29 verdes; `--phase29-selftest`
PASS (8 checks). Baseline de performance coletado (ver abaixo).

## Escopo entregue

1. **CLI headless** — `--phase29-bench` (ou `--benchmark`) roda a suite e grava relatorios;
   `--phase29-selftest` valida. Sem janela, sem ImGui, sem render — so simulacao.
2. **Cenarios** (`bench/Benchmark.cpp`, `defaultScenarios()`): escala 100/300/600/1000 agentes (MLP,
   visao single); visao single/raycast/sector a 600; tipos neurais MLP/RNN/NEAT a 600. Cada cenario e
   parametrizavel (agentes, comida, passos, warmup, repeticoes, seed, tipo neural, modo de visao,
   predadores).
3. **Repeticoes** com media/min/max/desvio padrao do tempo por passo (warmup descartado).
4. **Profiler por sistema (Fase 27) reusado**: percentual por sistema (perception, neural, movement,
   collision, energy, interaction, food, reproduction, death, spatialhash) + overhead, somando ~100%.
5. **Relatorios CSV + JSON + Markdown** com metadados: commit (injetado pelo CMake em
   `AGENTBIOSIM_GIT_COMMIT`), build (Debug/Release via NDEBUG), versao, data. Gravados em
   `benchmarks/results/*.{csv,json}` e `benchmarks/reports/*.md` (timestamp + build no nome).

## Como a Fase 30 e a Fase 33 consomem isto

- `bench::runScenario(scenario)` / `runSuite(...)` devolvem `BenchmarkResult` (tempo por passo
  agregado + `SectionTiming` por sistema). A **Fase 30 (Janela do Desenvolvedor)** chama o mesmo
  runner para mostrar custo por sistema/sparklines e rodar um cenario embutido. A **Fase 33** roda a
  suite em Release com seed fixa para o comparativo C++ vs Python e o relatorio final.
- Os serializadores (`toCsv/toJson/toMarkdown`) sao publicos e reaproveitaveis pela UI/relatorios.

## Baseline coletado (Release, commit c699923, 3 repeticoes x 300 passos)

| cenario | agentes | neural | visao | us/passo (media) | passos/s |
|---|---:|---|---|---:|---:|
| scale_100 | 100 | mlp | single | 633 | 1580 |
| scale_300 | 300 | mlp | single | 1972 | 507 |
| scale_600 | 600 | mlp | single | 3657 | 273 |
| scale_1000 | 1000 | mlp | single | 6582 | 152 |
| vision_raycast | 600 | mlp | raycast | 3663 | 273 |
| vision_sector | 600 | mlp | sector | 3804 | 263 |
| neural_simple_rnn | 600 | simple_rnn | single | 3822 | 262 |
| neural_neat | 600 | neat | single | 4422 | 226 |

Escala ~linear com o numero de agentes (633 -> 1972 -> 3657 -> 6582 us/passo para 100/300/600/1000).

## Gargalo visivel (percentual por sistema, scale_1000)

| sistema | % do passo |
|---|---:|
| **perception (visao)** | **49.2%** |
| **neural** | **33.9%** |
| collision | 10.9% |
| interaction | 2.5% |
| spatialhash | 1.6% |
| movement | 1.1% |
| (demais + overhead) | <1% |

**A percepcao (visao) e o gargalo numero 1 (~49%), seguida da rede neural (~34%)** — exatamente o
previsto em `BENCHMARK_PLAN.md` secao F. Juntas, visao + neural sao ~83% do passo a 1000 agentes.

## Overhead de instrumentacao

O profiler e OFF por padrao; um `ScopedTimer` desligado nao le relogio (custo ~zero, provado na Fase
27). Nos benchmarks o profiler e ligado de proposito; o overhead do proprio profiler aparece como a
secao "overhead" (~0.3% a 1000 agentes), desprezivel.

## Testes — `--phase29-selftest` (PASS, 8 checks)

Tempo de passo positivo; secoes por sistema presentes; contagem de passos confere; percentuais somam
~100% (medido 100.0%); reproducibilidade (mesma seed -> mesmas contagens finais de agentes/comida);
CSV com cabecalho, Markdown com tabela, JSON bem formado (re-parseado pelo `io::Json`).

## Arquivos

Criados: `src/bench/Benchmark.{hpp,cpp}`, `src/core/BuildInfo.hpp`, este STATUS.
Modificados: `src/main.cpp` (`--phase29-bench`/`--phase29-selftest`), `CMakeLists.txt` (fonte do bench
+ injecao do git commit). Saida de relatorios em `benchmarks/` (gitignored — sao artefatos gerados).

## Pendencias / proximos passos (Fase 32)

- **Atacar o gargalo de visao (~49%)** guiado por estes numeros — e o alvo #1 da otimizacao
  data-oriented (Divida 5/10). Reduzir alocacoes do `NeuralSystem`/percepcao por passo.
- Cenarios adicionais do `BENCHMARK_PLAN.md` (predacao, comida chunk, labels/grupos, 2000/5000
  agentes) podem ser adicionados ao `defaultScenarios()` quando necessario — a infra ja suporta.
- Comparativo formal C++ vs Python fica para a Fase 33.

Nao avancar para a Fase 30 sem autorizacao explicita do usuario.
