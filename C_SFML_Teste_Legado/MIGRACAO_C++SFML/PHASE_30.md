# Fase 30 — Janela do Desenvolvedor: Performance e Profiling In-App

Atue como arquiteto senior de software C++, engenheiro de performance e especialista em
observabilidade de simulacoes de alto desempenho, profiling por sistema e visualizacao de dados em
Dear ImGui.

Esta fase entrega uma **Janela do Desenvolvedor** dentro do proprio aplicativo: um painel que mostra,
em tempo real e de forma facil de ler, **quanto cada parte do sistema esta custando** (render, redes
neurais, visao/percepcao, fisica/colisao, energia, interacao, comida, reproducao, morte, spatial
hash, UI). E como ter um teste headless de performance visivel dentro da UI — para identificar
rapidamente o gargalo sem rodar benchmark externo.

Esta fase so MEDE e MOSTRA. **Nao otimiza nada** (otimizacao e a Fase 32). E a surface visual da
Divida 10 (performance prometida ainda nao comprovada ponta a ponta).

## Pre-requisitos (por isso esta fase vem aqui)

Esta fase depende e REUSA:

- **Fase 27** (Profiler por sistema): a instrumentacao com escopos por sistema ja deve existir. A
  Janela do Desenvolvedor LE esses numeros; nao reinventa a medicao.
- **Fase 29** (Benchmark Runner): o runner formal de cenarios ja deve existir. A janela pode disparar
  um cenario embutido e mostrar o resultado.
- **Fase 25** (Dear ImGui): a UI ja e ImGui; a janela usa o mesmo tema/estilo.

Se algo desses nao existir/estiver incompleto, pare e reporte antes de comecar — nao improvise um
profiler paralelo.

## Regras invioláveis

1. Engine continua headless. O profiler vive em camada neutra/engine; a JANELA (apresentacao) vive em
   `ui/`. O engine nao depende de ImGui.
2. Custo proximo de zero quando a janela esta oculta. Medicao detalhada so quando ligada (ou sempre
   barata, se o profiler ja for barato — medir e documentar).
3. Determinismo preservado. Ligar/desligar a janela ou um cenario embutido nao altera a simulacao
   "de producao" (use estado isolado/clonado para cenarios embutidos, se aplicavel).
4. Nenhum arquivo Python alterado. Build Debug/Release limpos.

## Estado esperado antes de iniciar

`git status` limpo; Fases 0 a 29 concluidas e comitadas; selftests das fases anteriores verdes,
em especial `--phase27-selftest` (profiler) e `--phase29-selftest` (benchmark runner).

## Objetivo

Uma janela ImGui de desenvolvedor que responde, de relance: "o que esta consumindo mais agora?" e
"quanto cada sistema custa por step?", com historico e com a possibilidade de rodar um cenario
controlado para comparar.

## Escopo obrigatorio

1. **Tabela de custo por sistema** (por step e por frame):
   - Uma linha por sistema instrumentado na Fase 27: perception, neural, movement, collision, energy,
     interaction, food, reproduction, death, spatialhash, render, ui, e "outros/overhead".
   - Colunas: nome amigavel PT-BR, us/step (ou ms/frame), % do tempo total, e uma barra/heat visual
     proporcional. Ordene por custo decrescente (opcional: toggle de ordenacao).
   - A soma das partes deve bater com o tempo total medido dentro de uma margem documentada.
2. **Historico / sparklines**: grafico de linha (ImGui `PlotLines`/`PlotHistogram` ou desenho
   proprio) do us/step total e dos 3-5 sistemas mais caros, sobre uma janela deslizante (ex.: ultimos
   N frames/steps). Mostrar FPS e us/step instantaneo e medio.
3. **Contadores do mundo**: numero de agentes (e por especie, se barato), comida, obstaculos; numero
   de cerebros por tipo neural (MLP/Gated/Shortcut/Modulated/RNN/NEAT); tamanho/ocupacao do spatial
   hash; memoria aproximada das stores (estimativa via tamanho dos vetores SoA).
4. **Toggles de isolamento de custo**: ligar/desligar sistemas individuais (ex.: desligar neural,
   desligar percepcao, render simples vs detalhado) e ver o delta de us/step ao vivo. Restaurar o
   estado original ao fechar. Deixar claro que isso e para diagnostico (a simulacao pode ficar
   incorreta com um sistema desligado — avisar na UI).
5. **Cenario de benchmark embutido**: um botao "Rodar cenario" que executa, via runner da Fase 29, um
   cenario parametrizavel (N agentes, N comidas, N steps, seed) headless/aceleerado e mostra o
   resultado (us/step medio/min/max/desvio, % por sistema) numa sub-area da janela, sem sair do app.
   Ideal: rodar sem perturbar a simulacao corrente (estado isolado).
6. **Selecao de tipo neural para custo**: mostrar, se barato, o custo medio de forward por tipo de
   cerebro presente, ajudando a ver "NEAT esta caro", "RNN ok", etc. (reusa profiler/benchmark).
7. **Tema/estilo**: usar o tema da Fase 25; visual limpo, legivel, cores de calor para destacar o
   maior custo. Aplicar Gestalt (agrupar tabela, historico, contadores, controles em secoes claras).

## Parametros/itens novos permitidos (alem do Python)

A UI Python nao tinha essa janela; ela e uma melhoria da arquitetura nova. Adicione-a como item no
menu (sugestao: Exibir > Janela do Desenvolvedor, ou um menu "Dev"/"Diagnostico"), desabilitavel,
sem custo quando oculta. Coloque-a no lugar logico seguindo o esqueleto da UI ja existente. Documente
a decisao de onde ela mora no menu.

## Fora de escopo

- Otimizar qualquer coisa (Fase 32). Esta fase nao muda o hot loop.
- Relatorio comparativo final C++ vs Python (Fase 33) — embora a janela possa exibir numeros que
  alimentem aquele relatorio.
- Persistir/exportar os dados em arquivo (opcional; se fizer, manter simples e documentar).

## Determinismo e custo

- Janela oculta: overhead deve ser desprezivel (medir e provar em diagnostics).
- O proprio ato de medir tem custo: documente o overhead do profiler ligado vs desligado.
- Cenario embutido nao pode corromper a simulacao corrente.

## Testes obrigatorios — `--phase30-selftest`

1. Os numeros por sistema expostos pela janela (a fonte de dados que ela le) batem, dentro de margem,
   com os numeros do benchmark headless da Fase 29 para o mesmo cenario/seed.
2. A soma dos tempos por sistema ~= tempo total medido (margem documentada).
3. Ligar/desligar um sistema via toggle e depois restaurar deixa a simulacao identica (determinismo:
   mesma seed -> mesmo estado apos restaurar, se o toggle foi revertido antes do step).
4. Cenario embutido roda com seed fixa e da resultado reproduzivel.
5. Engine continua headless (profiler nao depende de ImGui).

## Diagnostics/microbenchmark — `--phase30-diagnostics`

Overhead da janela aberta vs fechada; overhead do profiler ligado vs desligado; custo de coletar o
historico; 100/300/600/1000 agentes. Seed, commit, build.

## Build, regressoes, documentacao

- Build Debug/Release.
- Regressoes: selftests das Fases 7 a 29 + `--phase30-selftest`.
- Crie `PHASE_30_DEV_PERFORMANCE_WINDOW_STATUS.md`: escopo executado; como a janela le o profiler da
  Fase 27 e o runner da Fase 29; o que cada secao mostra; onde ela mora no menu; overhead medido;
  resultados de selftest/diagnostics/regressoes; confirmacao headless; confirmacao de que nada foi
  otimizado nesta fase; pendencias para a Fase 32 (quais gargalos a janela revelou). Atualize a
  Divida 10 no `TECHNICAL_DEBT_REGISTER.md` registrando que a surface foi entregue (a divida fecha
  na Fase 33).

## Saida final esperada

Resumo executivo; arquivos criados/modificados; descricao de cada secao da janela; principais
gargalos observados (isso alimenta a Fase 32); resultados de build/selftests/diagnostics; decisao:
pronto para commit da Fase 30 ou precisa correcao. Se tudo certo, commit. Nao avance para a Fase 31
sem autorizacao.
