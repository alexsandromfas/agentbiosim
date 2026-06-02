# Fase 26 — Agente Selecionado e Visualizador Neural

Atue como arquiteto senior de software C++, designer de UI/UX (Dear ImGui) e especialista em
visualizacao de redes neurais (MLP, RNN, NEAT) e introspeccao de agentes em simulacoes evolutivas.

Esta fase conecta o **inspector do agente selecionado** e o **visualizador neural**, ja construidos
em Dear ImGui (Fase 25). Reproduz a usabilidade do `sim/neural_viewer.py` e do painel de agente do
`sim/ui.py`, com a arquitetura nova (engine headless, dados expostos por consulta read-only, sem
custo quando oculto).

## Regras invioláveis

1. Engine headless; o viewer LE dados (ActivationTrace, genoma) via API read-only do runner; nao muta
   stores diretamente.
2. Painel/viewer oculto NAO calcula dados caros (sem trace, sem layout) — custo proximo de zero.
3. Determinismo e dt fixo preservados. Nenhum Python alterado. Build Debug/Release limpos.
4. UI em Dear ImGui, usando o tema/estilo da Fase 25.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 25 concluidas e comitadas; UI ja em ImGui (Fase 25); selftests
anteriores verdes. ActivationTrace minimo ja existe desde a Fase 9 e foi estendido por RNN (15) e
NEAT (16) — confirme.

## Referencia de usabilidade (Python)

`sim/neural_viewer.py` (layout do grafo neural, modos fixed/preencher), `sim/ui.py` (painel retratil
do agente, abas Genoma e Rede Neural). Usabilidade sim; arquitetura nao.

## Escopo obrigatorio

1. **Painel do agente selecionado** (retratil), aberto ao selecionar um agente:
   - Aba **Genoma**: especie/label, parametros do genoma do agente, energia, idade, dieta, raio, cor,
     tipo de cerebro, contadores relevantes. Read-only (edicao continua sendo pelo Editor Genetico).
   - Aba **Rede Neural**: visualizador do cerebro do agente.
2. **Visualizador neural por tipo**:
   - MLP / Gated / Shortcut / Modulated: camadas, nodes, pesos (cor/intensidade), ativacoes do ultimo
     forward (ActivationTrace).
   - RNN simplificada: idem + estado recorrente.
   - NEAT (comum/simplificada/recorrente): grafo de nodes/conexoes (habilitadas/desabilitadas),
     pesos, ativacoes; layout que lide com topologia arbitraria.
   - Layout **fixed** vs **preencher painel** (paridade Python).
3. **Visao do agente selecionado**: overlay de visao/raios (reusa Fase 11/12) ligavel; sem custo
   quando oculto.
4. **Selecao**: integra com a selecao existente; ao trocar/limpar selecao, o painel atualiza/oculta.
   Selecao de agente removido limpa com seguranca.

## Fora de escopo

- Edicao do genoma pelo painel (fica no Editor Genetico).
- Metricas globais/graficos (Fase 27), save/load (Fase 28).
- "Resetar rede neural por especie" pesado — se ainda desabilitado, manter desabilitado com nota,
  ou implementar de forma segura se a infra de reinit de brain/GenomeStore permitir (documentar).

## Testes obrigatorios — `--phase26-selftest`

1. Selecionar agente expoe genoma e trace corretos (headless, sem ImGui).
2. ActivationTrace do ultimo forward bate com o forward real para input fixo.
3. Trocar tipo neural (MLP/RNN/NEAT) nao quebra o caminho do viewer (dados corretos por tipo).
4. Viewer oculto nao gera trace (contador de trace == 0 quando oculto).
5. Selecao de agente removido limpa sem crash (swap-remove seguro).

## Diagnostics — `--phase26-diagnostics`

Custo viewer on/off; custo do trace por tipo neural; 100/300/600/1000 agentes; seed, commit, build.

## Build, regressoes, documentacao

- Build Debug/Release; regressoes Fases 7 a 25 + `--phase26-selftest`.
- Crie `PHASE_26_SELECTED_AGENT_NEURAL_VIEWER_STATUS.md`: escopo; como o viewer le dados sem mutar;
  como cada tipo neural e desenhado; custo quando oculto; resultados; pendencias (Fase 27+).

## Saida final esperada

Resumo executivo; arquivos criados/modificados; como ficou o viewer por tipo; resultados de
build/selftests/diagnostics; decisao: pronto para commit ou correcao. Nao avance para a Fase 27 sem
autorizacao.
