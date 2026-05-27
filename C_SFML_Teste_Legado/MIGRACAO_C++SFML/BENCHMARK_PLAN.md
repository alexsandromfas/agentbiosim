# Plano de Benchmarks para Migracao Python -> C++/SFML

## A. Objetivo dos Benchmarks

- Comparar a versao Python atual com a futura versao C++.
- Medir desempenho antes e depois de cada otimizacao.
- Identificar gargalos reais.
- Evitar otimizar no escuro.
- Validar se a migracao para C++ trouxe ganho mensuravel.
- Validar se mudancas arquiteturais realmente melhoraram a simulacao.
- Separar custo de simulacao, percepcao, rede neural, fisica, interacao, render e UI.

## B. Cenarios de Benchmark

### Escala Progressiva

- 100 agentes / 100 comidas.
- 300 agentes / 150 comidas.
- 600 agentes / 300 comidas.
- 1000 agentes / 500 comidas.
- 2000 agentes / 1000 comidas.
- 5000 agentes / 2500 comidas, quando C++ estiver maduro.
- 10000 agentes / 5000 comidas, meta futura, nao criterio inicial.

### Predacao

- Sem predadores.
- 30 predadores / 300 presas.
- 100 predadores / 1000 presas.
- Predadores com mesma visao da presa.
- Predadores com visao mais ampla.

### Renderizacao

- Headless.
- Renderizacao simples.
- Renderizacao detalhada.
- Renderizacao com overlays desligados.
- Renderizacao com spatial hash visivel.
- Renderizacao com visao do agente selecionado.
- Renderizacao com grafico ativo.
- Renderizacao com painel neural ativo.

### Visao

- Visao desligada.
- `single`.
- `fullbody/raycast`.
- `sector/bins`.
- 1 olho e 1 canal D.
- 1 olho e canais D/R/G/B.
- 2 olhos e 1 canal D.
- 2 olhos e canais D/R/G/B.
- Obstaculos bloqueando visao.
- Obstaculos visiveis mas sem bloqueio.

### Comida

- Comida instantanea.
- Comida chunk/pedacos.
- Chunk movel desligado.
- Chunk movel ligado.
- Colisao de comida desligada.
- Colisao de comida ligada.
- Reposicao `spawn_cluster`.
- Reposicao `grow_existing`.
- Reposicao `grow_particles`.

### Redes Neurais

- MLP pequena: entrada atual, 1 camada, 8 neuronios.
- MLP media: 2 camadas, 16/16.
- MLP grande: 4 camadas, 20/20/20/20.
- Gated MLP.
- Shortcut MLP.
- Modulated MLP.
- Simple RNN.
- NEAT comum.
- NEAT simplificada.
- NEAT recorrente.

Cada tipo neural deve ter benchmark proprio na fase em que for implementado:

- Fase 9: MLP inicial e base de executor.
- Fase 14: Gated MLP, Shortcut MLP e Modulated MLP.
- Fase 15: Simple RNN.
- Fase 16: NEAT comum, NEAT simplificada e NEAT recorrente.

Regra: benchmarks de MLP devem ser repetidos depois de adicionar NEAT para confirmar que o caminho MLP nao ficou mais caro quando NEAT esta desligado.

### Labels/Grupos

- Sem labels/grupos alem do default.
- 2 grupos.
- 5 grupos.
- 20 grupos.
- Graficos por grupo desligados/ligados.

## C. Metricas Obrigatorias

Medir:

- Wall time total.
- Passos simulados por segundo real.
- Tempo simulado por segundo real.
- FPS com renderizacao.
- Physics steps por segundo.
- Tempo da visao/percepcao.
- Tempo do forward neural.
- Tempo da locomocao.
- Tempo de energia/metabolismo.
- Tempo de interacao/comida/predacao.
- Tempo do spatial hash.
- Tempo de reproducao.
- Tempo de morte/remocao.
- Tempo de colisao.
- Tempo de renderizacao.
- Tempo de UI/graficos.
- Uso de CPU.
- Uso de RAM.
- Numero de agentes vivos.
- Numero de comidas.
- Numero de predadores.
- Numero de especies/labels.
- Numero de eventos de alimentacao.
- Numero de eventos de predacao.
- Numero de mortes.
- Numero de reproducoes.
- `effective_time_scale`.
- Backlog de simulacao.
- Numero de rebuilds do spatial hash.
- Tempo medio por agente para percepcao.
- Tempo medio por agente para brain forward.
- Alocacoes por step, quando possivel no C++.

## D. Metodologia

- Usar seed fixa.
- Usar duracao fixa de benchmark.
- Usar numero fixo de steps para benchmark headless.
- Separar modo headless de modo renderizado.
- Fazer warm-up antes da medicao.
- Repetir cada cenario pelo menos 3 vezes.
- Registrar media, minimo, maximo e desvio padrao.
- Salvar resultados em CSV e JSON.
- Manter os mesmos parametros entre Python e C++ quando possivel.
- Registrar commit hash.
- Registrar configuracao da maquina.
- Registrar linguagem, backend e build.
- No C++, registrar Debug/Release.
- Usar Release para comparacao final de performance.
- Separar benchmark de simulacao pura e benchmark com renderizacao.
- Desligar logs detalhados durante benchmark de performance.
- Rodar benchmark sem UI antes de medir UI.

## E. Benchmarks de Paridade

Objetivo: comparar comportamento, nao apenas velocidade.

Usar:

- Mesma seed.
- Mesma quantidade inicial de agentes.
- Mesma quantidade de comida.
- Mesma configuracao de mundo.
- Mesma distribuicao inicial equivalente.
- Mesma configuracao de energia.
- Mesma configuracao de visao.
- Mesma arquitetura neural e pesos iniciais quando possivel.

Medir ao longo do tempo:

- Populacao total.
- Populacao por especie/label.
- Energia media.
- Energia minima/maxima.
- Mortes.
- Nascimentos.
- Comida consumida.
- Comida existente.
- Predadores vivos.
- Eventos de predacao.
- Posicao media e dispersao.
- Divergencias esperadas por diferencas numericas.

Tolerancias:

- Estados caoticos podem divergir depois de muitos steps.
- Os primeiros steps devem ser comparaveis em cenarios controlados.
- Testes deterministas pequenos devem bater exatamente ou dentro de tolerancia numerica.

## F. Benchmarks Especificos do Gargalo de Visao

Testes isolados:

- Custo da visao por agente.
- Custo por numero de raios/retinas: 4, 8, 18, 32, 64.
- Custo por numero de canais: D, RG, RGB, DRGB.
- Custo por numero de olhos: 1 e 2.
- Custo por numero de candidatos proximos.
- Custo do SpatialHash.
- Custo do modo `single`.
- Custo do modo `sector`.
- Custo do modo `fullbody/raycast`.
- Impacto de obstaculos bloqueando visao.
- Impacto de `retina_bins_candidate_limit`.
- Impacto de `retina_bins_distance_subdivisions`.
- Impacto de `retina_bins_mode`.
- Impacto de `retina_bins_projection`.
- Impacto de `retina_bins_distance_falloff`.
- Impacto de `retina_skip`.
- Impacto de batch perception.
- Impacto de SoA/arrays contiguos.

Saidas esperadas:

- ms total de percepcao.
- us por agente.
- us por retina.
- candidatos medios por agente.
- throughput agentes/s.

Mapeamento por fase:

- Fase 10: visao `single`, canais, olhos e input neural real.
- Fase 11: `fullbody/raycast` e debug visual de visao.
- Fase 12: `sector/bins`, subdivisoes de distancia, projection, candidate limit e high-scale auto sector.
- Fase 20: oclusao por obstaculos e custo de bloquear visao.

## G. Benchmarks de Arquitetura

Comparar:

- Objeto por agente vs `AgentStore/SoA`.
- Visao individual vs visao em lote.
- Forward neural individual vs batch.
- Renderizacao simples vs detalhada.
- Spatial hash reconstruido todo frame vs reutilizado.
- Com e sem alocacao dentro do loop.
- Com e sem obstaculos.
- Com e sem renderizacao.
- Com e sem coleta de metricas.
- Com e sem visualizacao neural do agente selecionado.
- MLP batch vs NEAT individual.
- Threading ligado/desligado, quando existir.
- Dense neural batch vs fallback individual.
- RNN com estado por agente vs MLP equivalente.
- NEAT desligado vs ligado para confirmar isolamento de custo.
- Save/load pequeno vs grande.
- UI de parametros aberta/fechada.
- Neural viewer oculto/visivel.

## H. Formato de Saida

Resultados futuros devem ser salvos em:

- `benchmarks/results/*.csv`
- `benchmarks/results/*.json`
- `benchmarks/reports/*.md`

Observacao: esses caminhos sao recomendacoes para fases futuras de implementacao. Nesta etapa de documentacao nao devem ser criados scripts, pastas de benchmark, C++ ou CMake.

Cada resultado deve registrar:

- Data/hora.
- Commit hash.
- Linguagem/versao.
- Backend usado.
- Build mode.
- Modo de visao.
- Modo de renderizacao.
- Numero de agentes.
- Numero de comidas.
- Numero de predadores.
- Numero de labels/especies.
- Seed.
- Parametros relevantes.
- Metricas medidas.
- Media/min/max/desvio.
- Observacoes.

## I. Criterios de Sucesso

Metas iniciais:

- C++ deve ser pelo menos 3x mais rapido que Python no cenario equivalente inicial.
- C++ deve escalar melhor que Python acima de 600 agentes.
- Visao sector em C++ deve escalar melhor que fullbody/raycast.
- Modo headless deve medir simulacao sem custo de render.
- Benchmarks devem ser reproduziveis por seed.
- Nenhuma otimizacao deve remover funcionalidade sem registro.
- Qualquer otimizacao deve ter medicao antes/depois.
- Regressao de performance deve ser registrada e explicada.

Metas futuras:

- 2000 agentes com simulacao interativa.
- 10000 agentes headless em cenario simplificado.
- Separar gargalo principal por percentual de wall time.

## J. Integracao Futura

Usar benchmarks:

- Antes de otimizar.
- Depois de implementar engine.
- Depois de implementar spatial hash.
- Depois de implementar visao.
- Depois de implementar neural batch.
- Depois de implementar render.
- Depois de implementar UI.
- Antes de considerar uma fase concluida.
- Antes de comparar Python vs C++.
- Antes de merge de uma fase importante.

## K. Cobertura por Fase Futura

Esta secao vincula benchmarks a fases executaveis. Um item citado aqui nao deve ficar apenas como intencao generica.

| Fase | Benchmark obrigatorio |
|---:|---|
| 9 | Forward MLP individual e por lote; MLP pequena/media/default. |
| 10 | Visao single por agente/retina/canal; input neural real. |
| 11 | Fullbody/raycast vs single; debug visual ligado/desligado. |
| 12 | Sector/bins vs raycast; subdivisoes 1/5/20/99; candidate limit. |
| 13 | Reproducao/mutacao: custo de nascimento/remocao e clone neural. |
| 14 | Gated/Shortcut/Modulated vs MLP baseline; batch por assinatura. |
| 15 | RNN vs MLP; custo de estado recorrente e memory decay. |
| 16 | NEAT comum/simplificada/recorrente; custo individual/grupos; MLP sem regressao com NEAT desligado. |
| 17 | Agrupamento por especie/label e reset de cerebro por especie. |
| 18 | Predacao com/sem predadores; dieta e energia ganha. |
| 19 | Comida instantanea vs chunk; spawn_cluster/grow_existing/grow_particles. |
| 20 | Obstaculos e oclusao; visao bloqueada vs atravessando paredes. |
| 21 | Colisao/fisica on/off; agente-agente, chunk movel, Brownian e viscosidade. |
| 22 | UI base on/off; toolbar e canvas tools. |
| 23 | Preferencias abertas/fechadas; alteracao de parametros sem stutter. |
| 24 | Editor genetico/especies/substrato aberto/fechado. |
| 25 | Neural viewer oculto/visivel; MLP/RNN/NEAT. |
| 26 | Metrics/profiler/logs on/off. |
| 27 | Save/load/export/import/autosave em cenarios pequenos e grandes. |
| 28 | Runner formal headless com CSV/JSON/MD. |
| 29 | UI completa com simulacao real. |
| 30 | Otimizacoes data-oriented antes/depois em 600/1000/2000/5000 agentes. |
| 31 | Campanha final Python vs C++ com seed fixa e relatorio completo. |

## Relatorio Padrao de Benchmark

Cada relatorio Markdown deve conter:

- Objetivo do benchmark.
- Configuracao.
- Commit.
- Build.
- Cenario.
- Tabela de resultados.
- Percentual por sistema.
- Comparacao com baseline anterior.
- Conclusao objetiva.
- Proxima acao recomendada.

## Cuidados Especificos

- Nunca comparar Python com C++ Debug.
- Nunca misturar benchmark renderizado com headless.
- Nunca mudar parametros entre linguagens sem registrar.
- Nunca concluir ganho com uma execucao unica.
- Sempre conferir se a simulacao nao esta pausada.
- Sempre registrar se a UI/grafico/neural viewer estavam ativos.
