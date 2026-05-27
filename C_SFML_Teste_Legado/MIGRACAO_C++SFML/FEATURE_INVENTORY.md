# Inventario de Funcionalidades

Este inventario lista funcionalidades observadas no codigo Python atual e no README. Itens marcados como "precisa de verificacao manual" foram encontrados em codigo possivelmente legado ou duplicado.

## Terminologia e Compatibilidade

- `Labels/Grupos` e o termo funcional ainda encontrado no codigo atual.
- `Especies` e o termo conceitual recomendado para a migracao C++.
- `Bacteria` e `Predator` ainda existem como classes e parametros no Python, mas devem ser preservados como aliases ou especies default na migracao.
- `Agente` e `Genoma` aparecem como conceitos proximos: agente e instancia viva; genoma e configuracao heredavel/cerebro exportavel.
- Itens com nomes antigos nao devem ser removidos sem fase de compatibilidade.

## Simulacao

- Loop com tempo fixo e acumulador de tempo.
- `time_scale` para acelerar/desacelerar tempo simulado.
- `physics_steps_per_second`, `max_physics_steps_per_frame` e backlog maximo.
- Pausa/retomada.
- Reset/stop/play por UI e atalhos.
- Modo com renderizacao e modo sem renderizacao (`render_enabled`).
- Seed fixa opcional para reprodutibilidade.
- Modo headless em testes/benchmarks.
- Limite de mortes por passo (`max_deaths_per_step`).
- Resgate de populacao minima (`population_min_rescue_enabled`).

## Mundo e Camera

- Substrato retangular.
- Substrato circular.
- Largura/altura do mundo.
- Raio circular.
- Camera com pan, zoom e fit world.
- Rastreamento de agente selecionado.
- Suavizacao de camera ao seguir agente.
- Interpolacao de renderizacao opcional.
- Cores de fundo, gradiente vertical de fundo, gradiente vertical do substrato.
- Cor e visibilidade da borda do substrato.

## Agentes

- Classes `Bacteria` e `Predator`, com indicacao de que o projeto caminha para organismo generico por dieta/label.
- Energia, idade, corpo, cor, raio/tamanho, direcao, velocidade.
- Formato elipse ou circulo.
- Um ou dois olhos.
- Nome/template de agente.
- Selecao unica, selecao por lasso, selecao retangular e selecao por grupo/label.
- Mover agente pelo canvas.
- Deletar selecionados.
- Pipeta/coleta de genoma foi implementada anteriormente; no estado atual deve ser verificada manualmente se continua ativa.

## Comida

- Comida instantanea.
- Comida em pedacos/chunk.
- Target de comida.
- Raio minimo/maximo ou tamanho de particulas.
- Reposicao por intervalo.
- Comida chunk consumida durante contato, por mordidas/pedacos.
- Modos de reposicao de chunk: `spawn_cluster`, `grow_existing`, `grow_particles`.
- Controle de raio da particula, raio do aglomerado e espacamento entre particulas.
- Trim de excesso de comida.
- Botao `Limpar Comida`.
- Cor da comida.

## Predadores

- Predadores habilitaveis.
- Populacao inicial/min/max.
- Energia, metabolismo, reproducao, morte por idade e corpse-to-food.
- Visao, retina, canais e dieta propria.
- Predacao de bacterias/organismos conforme dieta.
- Observacao arquitetural: no C++ deve ser modelado como especie/dieta, nao como classe especial rigida.

## Obstaculos

- Desenho de obstaculos com pincel.
- Largura de pincel.
- Cor do obstaculo.
- Apagar obstaculo.
- Obstaculo bloqueia movimento de agentes.
- Obstaculo impede comida/spawn sobre a area.
- Opcao de sensores verem obstaculos.
- Opcao de visao atravessar ou nao paredes/obstaculos.
- Visualizacao do spatial hash.

## Sensores e Visao

- Sensor de retina multi-raios.
- Modos de visao:
  - `single`: aproximacao rapida por centroide.
  - `fullbody`: intersecao geometrica raio/corpo.
  - `sector`: visao por bins/setores.
- Retina frontal.
- Raio de visao configuravel.
- Campo de visao/FOV em graus.
- Quantidade de retinas.
- Quantidade de olhos.
- Angulo entre olhos.
- Separacao dos olhos.
- Visualizacao da visao do agente selecionado.
- Opcao para mostrar visao de multi-selecao.
- Skip de retina (`retina_skip`).
- Candidate limit no modo sector.
- Obstaculos bloqueando visao no modo sector.

## Canais de Retina

- Canais possiveis normalizados: `r`, `g`, `b`, `rd`, `gd`, `bd`, `d`.
- Modos conceituais de entrada:
  - distancia somente;
  - cor vezes distancia;
  - cor mais distancia dedicada;
  - cor somente.
- Checkboxes por canal no editor genetico.
- Entrada neural = retinas * olhos * canais ativos.
- Subdivisoes de distancia no modo sector alteram intensidade/distancia, nao necessariamente tamanho de entrada.

## Redes Neurais

- MLP padrao.
- MLP com gates.
- MLP com atalho entrada -> saida.
- MLP modulada (gates + atalho).
- RNN simples.
- NEAT comum.
- NEAT simplificada.
- NEAT recorrente.
- Cache por arquitetura/tipo.
- Forward em lote para redes compativeis.
- Forward Numba opcional.
- Mutacao de pesos.
- Mutacoes especificas de gates/shortcut/RNN/NEAT.
- Serializacao e restauracao de cerebros.
- Ativacoes para agente selecionado.
- Visualizador neural.

## Evolucao, Reproducao e Mutacao

- Reproducao por energia de split.
- Energia dividida entre pai e filho.
- Mutacao de pesos por taxa e intensidade.
- NEAT com adicao/remocao/toggle de conexao, adicao de node e reset de peso.
- Idade minima de reproducao.
- Cooldown de reproducao.
- Min/max populacional por tipo legado e por label/grupo em UI.
- Resetar rede neural por label/grupo.
- Export/import de agente/genoma.

## Energia e Metabolismo

- Energia inicial.
- Energia de morte.
- Energia de split.
- Capacidade maxima de energia.
- Custo em velocidade zero (`v0_cost`).
- Custo em velocidade maxima (`vmax_cost`).
- Referencia de velocidade maxima.
- Morte por idade opcional.
- Virar comida ao morrer opcional.
- Dieta por comida, agentes e mesma label.
- Eficiencia de comida e eficiencia de comer agentes.

## Colisao e Interacao

- Colisao organismo-organismo.
- Elasticidade leve opcional.
- Transferencia de velocidade.
- Separacao e impulso maximo.
- Viscosidade global opcional.
- Ruido browniano opcional.
- Comida em pedacos movel opcional.
- Colisao comida-comida opcional.
- Adesina/coesao de comida opcional.
- Empurrao de comida por agentes.
- Interacao de comida/predacao via spatial hash.

## UI

- Menu superior com `Arquivo`, `View`, `Preferencias`, `Agente`, `Ajuda`.
- Abas principais observadas: `Editor Genetico`, `Populacao`, `Substrato`, `Labels`, `Simulacao`, `Bacterias`, `Predadores`, `Ajuda`, `Teste`.
- Observacao: algumas abas parecem legado/duplicadas no codigo e precisam de verificacao manual.
- Barra superior com ferramentas e velocidade.
- Painel do agente selecionado com abas `Genoma` e `Rede Neural`.
- Painel retratil do agente selecionado.
- Graficos com checkbox por metrica.
- Janelas de preferencias para simulacao, autosave, aparencia, sistema de visao, redes neurais e performance.

## Menus

- `Arquivo`: novo, abrir simulacao, salvar simulacao, salvar como, exportar/importar substrato JSON.
- `View`: mostrar ativacoes neurais, layout da rede neural.
- `Preferencias`: opcoes de simulacao, autosave, aparencia do ambiente, sistema de visao, redes neurais, grafico, resolucao de renderizacao, novas mudancas/performance.
- `Agente`: exportar agente selecionado, carregar agente, criar linhagem a partir do selecionado. Possivelmente legado diante do conceito de genoma/especie.
- `Ajuda`: ajuda e atalhos.

## Ferramentas de Canvas

- Selecionar.
- Selecionar por lasso.
- Selecionar por retangulo.
- Comida.
- Agente importado/prototipo.
- Desenhar obstaculo.
- Mover.
- Dead/delete.
- Pipeta/coletar genoma: precisa de verificacao manual no estado atual.
- Pan com botao direito.
- Zoom com scroll.

## Graficos e Metricas

- Historico de metricas.
- Populacao total e por grupos/labels.
- Quantidade de comida.
- Fator de inteligencia do grupo.
- Valores atuais na legenda.
- Amostragem configuravel.
- Janela temporal configuravel.
- Grafico pausa com simulacao.
- Coleta mesmo quando painel nao esta visivel, conforme parametrizacao.

## Agente Selecionado

- Painel lateral com dados de genoma e movimento.
- Visualizacao do agente.
- Dados de energia, idade, dieta, visao, arquitetura neural e outputs.
- Fator de inteligencia local.
- Visao desenhada no canvas quando selecionado.
- Camera pode seguir agente.

## Visualizador Neural

- Visualizador grafico de rede.
- Retinas no topo e saidas embaixo.
- Canais de retina agrupados.
- Valores e ativacoes.
- Pesos e conexoes.
- Layout denso/fixo ou preencher painel.
- Adaptacao simplificada para RNN/NEAT precisa ser preservada.

## Labels/Grupos/Especies

- Labels/grupos com nome, cor, quantidade, min/max, inicial, checkbox de grafico.
- Selecionar grupo.
- Atribuir/remover selecionados.
- Resetar rede neural do grupo.
- Mini paineis/cards.
- Observacao: futuro nome conceitual recomendado e "Especies", preservando compatibilidade com `Labels`.

## Exportacao/Importacao

- Exportar/importar substrato JSON.
- Exportar/importar agente.
- Salvar/abrir simulacao `.biosim`.
- Salvar como.
- Autosave.
- Exportar ativacoes neurais opcionalmente.
- JSON legivel opcional.
- Save de camera, mundo, parametros, comidas, agentes, cerebros e labels.

## Logs, Diagnostico e Profiling

- Logs de evento e excecao.
- Tracebacks no debug.
- Heartbeat diagnostico.
- Recovery save on close.
- Profiler por secoes.
- Metricas CPU/RAM.
- Benchmark headless existente em `tests`.

## Performance/Otimizacoes

- Spatial hash reutilizavel.
- Numba kernels opcionais.
- Percepcao em grupos.
- Buffers persistentes de percepcao.
- Retina Numba em lote.
- Sector vision em escala.
- Brain cache por grupo.
- Forward do cerebro em Numba.
- Locomocao/energia em Numba.
- Render simples.
- Desligar renderizacao.
- Retina skip.

## Itens Nao Identificados ou que Precisam Verificacao Manual

- Qual conjunto exato de abas aparece na UI final, pois `ui.py` contem trechos novos e antigos.
- Estado atual da pipeta apos reversoes/commits recentes.
- Se a aba `Agente` ainda deve existir ou esta em transicao para `Genoma`.
- Se `Labels` ja foi totalmente renomeado para `Especies` no comportamento visual atual.
- Quais saves antigos precisam compatibilidade oficial.
