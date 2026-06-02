# Fase 27 — Metricas, Inteligencia, Logs, Diagnostico e Profiler

Atue como arquiteto senior de software C++, engenheiro de observabilidade e especialista em
profiling por sistema, metricas de simulacao evolutiva e visualizacao de series temporais em Dear
ImGui.

Esta fase preserva a observabilidade da simulacao (metricas, inteligencia, logs) E cria o **profiler
por sistema** que sera consumido pela Fase 29 (benchmark runner) e pela Fase 30 (Janela do
Desenvolvedor). E uma fase-fundacao para a prova de performance (Divida 10).

## Regras invioláveis

1. Engine headless; metricas/profiler vivem em camada neutra/engine, sem depender de ImGui. A UI LE e
   desenha.
2. Tudo desligavel; custo proximo de zero quando desligado (medir e provar).
3. Determinismo e dt fixo preservados. Nenhum Python alterado. Build Debug/Release limpos.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 26 concluidas e comitadas; UI em ImGui; selftests anteriores verdes.

## Referencia de usabilidade (Python)

`sim/intelligence.py`, `sim/profiler.py`, `sim/diagnostics.py`, `sim/ui.py` (graficos/metricas).
Usabilidade sim; arquitetura nao.

## Escopo obrigatorio

1. **MetricsSystem**: series por step e por especie (populacao, energia media, nascimentos, mortes,
   comida, etc.), com janela deslizante e amostragem configuravel.
2. **Inteligencia** local/global/grupo (paridade conceitual com `sim/intelligence.py`).
3. **Graficos** em ImGui (linha/histograma) para as series principais; legivel, com tema da Fase 25.
4. **Logger** com niveis, baixo custo quando desligado; **heartbeat**; hooks de **crash/recovery**
   (sem implementar save/load real — isso e Fase 28).
5. **Scoped profiler por sistema** (o item mais importante para as fases seguintes):
   - Escopos de tempo para: perception, neural, movement, collision, energy, interaction, food,
     reproduction, death, spatialhash, render, ui, e overhead.
   - API que entrega us/step e % por sistema, acumulado e instantaneo, com overhead baixo e
     desligavel. Esta API e a fonte de dados da Fase 30.

## Parametros/itens novos permitidos

Se a arquitetura nova expoe metricas uteis que a UI Python nao tinha, adicione no lugar logico, com
rotulo PT-BR, sem poluir. Documente.

## Fora de escopo

- Benchmark runner formal (Fase 29).
- Janela do Desenvolvedor completa (Fase 30) — esta fase entrega os DADOS; a janela rica vem depois.
- Save/load/autosave reais (Fase 28).

## Testes obrigatorios — `--phase27-selftest`

1. Series por especie batem em cenario controlado/seed fixa.
2. Profiler mede por sistema; soma dos escopos ~= tempo total (margem documentada).
3. Profiler off vs on: overhead documentado e baixo; off nao aloca/registra.
4. Log desligado nao tem custo relevante.
5. Engine continua headless (profiler/metricas sem ImGui).

## Diagnostics — `--phase27-diagnostics`

Overhead metrics/profiler on/off; custo por sistema; 100/300/600/1000 agentes; seed, commit, build.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 26 + `--phase27-selftest`.
- Crie `PHASE_27_METRICS_INTELLIGENCE_PROFILER_STATUS.md`: escopo; API do profiler (como a Fase 29 e
  a Fase 30 vao consumi-la); overhead medido; resultados; pendencias.

## Saida final esperada

Resumo executivo; arquivos criados/modificados; descricao da API do profiler por sistema; resultados
de build/selftests/diagnostics; decisao: pronto para commit ou correcao. Nao avance para a Fase 28
sem autorizacao.
