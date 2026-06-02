# Fase 29 — Benchmark Runner e Experimentos Headless Formais

Atue como arquiteto senior de software C++, engenheiro de performance e especialista em benchmarking
reproduzivel, experimentos headless e relatorios de desempenho de simulacoes de alta escala.

Esta fase cria a **infraestrutura formal de benchmark**: um runner headless de cenarios, com
relatorios reproduziveis, reusando o profiler por sistema da Fase 27. E a base de dados da Fase 30
(Janela do Desenvolvedor) e da prova de performance da Fase 33 (Divida 10).

## Regras invioláveis

1. Totalmente headless: roda sem janela, sem ImGui, sem render (ou com render opcional medido a
   parte).
2. Reproduzivel: seed fixa -> resultado estavel; metadados de commit/build em todo relatorio.
3. Reusa o profiler da Fase 27 para os percentuais por sistema; nao reinventa medicao.
4. Nenhum Python alterado. Build Debug/Release limpos.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 28 concluidas e comitadas; profiler por sistema (Fase 27) disponivel;
selftests anteriores verdes.

## Referencia

`BENCHMARK_PLAN.md` (cenarios e metricas alvo), `sim/profiler.py`. Esta fase implementa o plano
formal de `BENCHMARK_PLAN.md`.

## Escopo obrigatorio

1. **CLI de benchmark headless** (flag dedicada, ex.: `--benchmark <cenario>` ou `--phase29-bench`),
   parametrizavel por N agentes, N comidas, N obstaculos, N steps, seed, tipo neural, modo de visao.
2. **Cenarios**: pelo menos os de `BENCHMARK_PLAN.md` (escala 100/300/600/1000/2000/5000+, variando
   visao single/raycast/sector, e tipos neurais MLP/RNN/NEAT).
3. **Relatorios** em CSV, JSON e Markdown: us/step medio/min/max/desvio, FPS-equivalente, % por
   sistema (do profiler), contagem de entidades, seed, commit, build, data.
4. **Repeticoes** com media/min/max/desvio padrao por cenario.
5. **Metadados** de commit/build embutidos em cada relatorio.

## Fora de escopo

- Otimizar o engine (Fase 32) — esta fase MEDE.
- Janela ImGui de performance (Fase 30) — esta fase entrega o runner que a janela usa.
- Comparativo formal C++ vs Python (Fase 33).

## Testes obrigatorios — `--phase29-selftest`

1. Suite minima roda com seed fixa e e reproduzivel (duas execucoes -> mesmos numeros dentro de
   tolerancia).
2. Headless sem janela funciona (sem dependencia de ImGui/SFML window).
3. Percentuais por sistema somam ~100% (consistentes com o profiler da Fase 27).
4. Relatorios CSV/JSON/Markdown sao gerados e bem formados.

## Diagnostics

O proprio runner e o diagnostics; garanta que o overhead de instrumentacao esta documentado.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 28 + `--phase29-selftest`.
- Crie `PHASE_29_BENCHMARK_RUNNER_STATUS.md`: escopo; cenarios implementados; formato dos relatorios;
  como a Fase 30 e a Fase 33 consomem o runner; primeiros numeros coletados (baseline de
  performance); gargalos visiveis; pendencias para a Fase 32.

## Saida final esperada

Resumo executivo; arquivos criados/modificados; cenarios e relatorios; baseline de performance
coletado; decisao: pronto para commit ou correcao. Nao avance para a Fase 30 sem autorizacao.
