# Fase 5 - Renderizacao Simples

## Escopo Executado

A Fase 5 separou a renderizacao simples em um modulo SFML proprio. O `App` continua responsavel por janela, loop, eventos, update e chamada do renderer. O novo `Renderer` le `World`, `Camera2D`, `AgentStore` e `FoodStore` de forma somente leitura e desenha fundo, limite do mundo, agentes e comida.

## Arquivos Python Consultados

- `sim/render.py`: referencia funcional para `SimpleRenderer`, desenho circular de agentes/comida, cabeca preta, overlay e separacao entre render simples/detalhado.
- `sim/game.py`: referencia funcional para uso de `render_enabled`, selecao de renderer por `simple_render`, loop visual, pan/zoom e chamada de render.
- `sim/world.py`: referencia funcional para camera e conversao mundo/tela.
- `sim/entities.py`: referencia funcional para campos desenhados de `Agent` e `Food`.
- `sim/controllers.py`: referencia funcional para defaults de renderizacao, aparencia, cor de comida e cor de bacteria.

## Arquivos Python Adicionais Consultados

Nenhum arquivo Python adicional foi consultado nesta fase.

## Conceitos Confirmados Contra o Python

- O Python possui `SimpleRenderer` com agentes e comidas desenhados como circulos simples.
- O Python desenha uma cabeca/ponto preto na frente do agente para indicar orientacao.
- O Python usa `render_enabled` para pular renderizacao quando desligado.
- O Python alterna renderer por `simple_render`, mas a fase atual implementa apenas o renderer simples.
- Cores de comida e bacteria vem dos parametros `food_color` e `bacteria_color`.
- Parametros de aparencia do fundo/substrato existem e devem ser preservados.
- O overlay completo do Python depende de fonte/UI/metricas e foi deixado fora desta fase.

## Divergencias e Decisoes Registradas

- A renderizacao detalhada com elipses orientadas nao foi implementada. Esta fase e apenas render simples.
- `simple_render` e lido e registrado em `RenderOptions`, mas a implementacao atual sempre usa o renderer simples porque o renderer detalhado pertence a uma fase futura.
- `render_resolution_scale` e lido e mantido em `RenderOptions`, mas ainda nao aplica supersampling/render target escalado.
- `background_gradient_enabled` foi implementado para o fundo por um quad vertical simples.
- `substrate_gradient_enabled` ainda nao implementa gradiente real dentro do substrato. Quando ligado, o renderer usa uma cor media entre topo e base como fallback simples.
- Overlay textual dentro da cena nao foi implementado por falta de fonte/infra de overlay nesta fase. FPS e contagens continuam no titulo da janela.
- Mundo circular foi confirmado por leitura do caminho `world.shape() == Circular` no renderer, mas nao houve alternancia visual por janela nesta fase porque ainda nao existe UI/CLI de configuracao runtime.
- Pan/zoom foram preservados no `App` por inspecao do fluxo de eventos existente; a automacao desta fase validou abertura da janela e contagens no titulo, nao interacao manual de mouse.
- Obstaculos, selecao visual, visao, labels, grafico, neural viewer e comida chunk irregular ficaram fora do escopo.

## Correcoes Feitas na Auditoria Final

- `render_enabled=false` agora faz o `Renderer` retornar antes de limpar/desenhar a tela. Isso deixa a semantica mais proxima do Python, onde o frame de render e pulado quando `render_enabled` esta desligado.

## Arquivos Criados

- `C_SFML_Teste_Legado/AgentBioSimCpp/src/render/RenderOptions.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/render/Renderer.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/render/Renderer.cpp`

## Arquivos Modificados

- `C_SFML_Teste_Legado/AgentBioSimCpp/CMakeLists.txt`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.hpp`
- `C_SFML_Teste_Legado/AgentBioSimCpp/src/app/App.cpp`

## Estruturas Implementadas

### `RenderOptions`

Opcoes simples de render:

- `renderEnabled`
- `simpleRender`
- `renderResolutionScale`
- `backgroundColor`
- `backgroundGradientEnabled`
- `backgroundColorTop`
- `backgroundColorBottom`
- `substrateGradientEnabled`
- `substrateColorTop`
- `substrateColorBottom`
- `substrateBorderEnabled`
- `substrateBorderColor`
- `agentHeadColor`
- `chunkFoodOutlineColor`

### `RenderStats`

Contadores simples retornados pelo renderer:

- `skipped`
- `agentsDrawn`
- `foodsDrawn`

### `Renderer`

Responsabilidades:

- limpar/desenhar fundo;
- desenhar limite do mundo retangular;
- desenhar limite do mundo circular;
- desenhar comidas como circulos preenchidos;
- desenhar agentes como circulos preenchidos;
- desenhar cabeca preta do agente;
- retornar contagens desenhadas.

O renderer nao modifica o estado da simulacao.

## Parametros Usados

- `render_enabled`
- `simple_render`
- `render_resolution_scale`
- `substrate_bg_color`
- `background_gradient_enabled`
- `background_color_top`
- `background_color_bottom`
- `substrate_gradient_enabled`
- `substrate_color_top`
- `substrate_color_bottom`
- `substrate_border_enabled`
- `substrate_border_color`
- `food_color`
- `bacteria_color`

## Parametros Pendentes

Nenhum parametro exigido pela Fase 5 ficou ausente do `ParameterRegistry`.

Implementacao futura pendente:

- aplicar `render_resolution_scale` com render target intermediario;
- implementar gradiente real dentro de substrato circular/retangular;
- implementar renderer detalhado quando uma fase futura autorizar.

## Decisoes Sobre Separacao App/Renderer

- `App` manteve janela, eventos, loop, timestep e titulo.
- `Renderer` recebeu apenas referencias const/read-only para mundo, camera e stores.
- `Renderer` nao chama update, nao cria entidades e nao acessa regras biologicas.
- `App` monta `RenderOptions` a partir do `ParameterRegistry`.

## Observacoes Simples de FPS/Render

Execucao curta em Release por aproximadamente 5 segundos:

- agentes desenhados: `150/150`
- comidas desenhadas: `50/50`
- FPS aproximado no titulo: `122`
- modo: `Release`
- `render_enabled`: ativo
- `simple_render`: lido do parametro, default atual `false`, mas renderer simples e o unico implementado nesta fase
- janela iniciou e permaneceu aberta ate ser encerrada pelo teste

Auditoria final apos a correcao de `render_enabled`:

- Debug: compilou.
- Release: compilou.
- Janela Release abriu por aproximadamente 5 segundos.
- Titulo observado: `AgentBioSimCpp 0.1.0-phase1 | FPS 125 | agents 150/150 | food 50/50 | world rectangular | zoom 0.891429 | dt 0.0333333 | steps 120`.
- O build Debug/Release retornou codigo de sucesso. O MSBuild ainda emite aviso pos-build nao fatal de `pwsh.exe` ausente herdado do ambiente/build, mas o executavel e gerado.

## Limitacoes Atuais

- Nao ha visao.
- Nao ha rede neural.
- Nao ha alimentacao/comida consumida.
- Nao ha energia/metabolismo completo.
- Nao ha reproducao.
- Nao ha predadores funcionais.
- Nao ha spatial hash.
- Nao ha UI completa.
- Nao ha selecao visual.
- Nao ha labels/especies renderizadas.
- Nao ha obstaculos.
- Nao ha graficos, neural viewer ou overlay rico.
- Nao ha render detalhado por elipse.

## Confirmacao de Escopo

A Fase 5 implementou somente renderizacao simples desacoplada em SFML e manteve os stores da Fase 4 funcionando. Nenhuma regra biologica, neural, de percepcao, interacao, reproducao, predacao, spatial hash ou UI completa foi implementada.
