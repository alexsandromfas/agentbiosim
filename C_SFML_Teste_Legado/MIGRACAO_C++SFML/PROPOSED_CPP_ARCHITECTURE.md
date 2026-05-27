# Arquitetura C++ Proposta

## Visao Geral

A versao C++ deve ser uma melhoria arquitetural, nao uma traducao literal do Python. A recomendacao e um nucleo headless data-oriented, renderizacao SFML desacoplada, UI tecnica com Dear ImGui, persistencia versionada e benchmarks desde a primeira fase.

Objetivo central:

- Preservar comportamento e parametros atuais.
- Escalar de centenas para milhares de agentes com melhor uso de CPU/cache.
- Permitir experimentos headless e renderizados.
- Manter UI completa sem transformar o engine em dependencia da UI.

## Estrutura de Pastas Sugerida

Esta estrutura e proposta para fases futuras. Nao deve ser criada nesta etapa.

```text
app/
  MainApp
  AppController
  InputRouter
core/
  Config
  ParameterSchema
  RandomService
  Logger
  Profiler
  Diagnostics
  Serialization
sim/
  SimulationEngine
  WorldState
  TimeStepper
  CommandQueue
  stores/
    AgentStore
    FoodStore
    SpeciesStore
    GenomeStore
    ObstacleStore
  systems/
    SpatialSystem
    PerceptionSystem
    NeuralSystem
    MovementSystem
    EnergySystem
    InteractionSystem
    ReproductionSystem
    DeathSystem
    CollisionSystem
    MetricsSystem
  perception/
    RetinaConfig
    RaycastVision
    SectorBinVision
    VisionDebugData
  neural/
    BrainType
    DenseBrainStore
    RnnBrainStore
    NeatBrainStore
    BrainExecutor
render/
  SfmlRenderer
  Camera2D
  OverlayRenderer
  VisionDebugRenderer
ui/
  MainMenu
  Toolbar
  GenomePanel
  SpeciesPanel
  SubstratePanel
  PreferencesWindows
  AgentInspector
  NeuralViewer
  MetricsChart
benchmarks/
  BenchmarkRunner
  BenchmarkScenarios
  BenchmarkReporter
```

## Relacao Entre Modulos

- `SimulationEngine` nao depende de SFML nem Dear ImGui.
- `WorldState` contem stores e parametros aplicados.
- `SystemScheduler` executa sistemas em ordem fixa.
- `AppController` liga UI, input, renderer e engine.
- `SfmlRenderer` le snapshots somente leitura do estado.
- `UI` envia comandos e alteracoes de parametros para o engine por uma camada de comandos.
- `Serialization` conhece schemas, mas nao chama UI.
- `Benchmarks` rodam o engine headless ou renderizado sem depender da UI completa.

## Politica de Terminologia e Compatibilidade

- A arquitetura interna deve usar `Species` para o conceito biologico de grupo heredavel.
- O loader e a UI devem continuar entendendo `Labels`, porque este e o termo ainda presente no Python atual e em saves/configuracoes.
- `Bacteria` e `Predator` devem ser tratados como especies iniciais ou aliases legados, definidos por dieta, cor, parametros e genoma.
- O menu/fluxo `Agente` deve migrar conceitualmente para `Genoma`, mas apenas quando houver paridade de import/export e caminho de compatibilidade.
- Nenhuma renomeacao deve apagar informacao de save/load. Renomeacoes devem ser feitas por aliases versionados.

## Fluxo Principal da Simulacao

1. Receber comandos pendentes.
2. Aplicar alteracoes de parametros seguras.
3. Acumular tempo simulado com dt fixo.
4. Atualizar spatial hash.
5. Construir queries/buffers de percepcao.
6. Executar percepcao por grupos de assinatura.
7. Executar brain forward por grupos de assinatura neural.
8. Aplicar locomocao/movimento.
9. Aplicar energia/metabolismo.
10. Aplicar interacao com comida/predacao.
11. Resolver colisoes.
12. Aplicar reproducao.
13. Aplicar morte/remocao compactada.
14. Atualizar metricas leves.
15. Produzir snapshot de render/debug se render ligado.

## Estrategia para Engine Headless

- O engine deve compilar e rodar sem janela.
- Headless deve ser o modo base de teste.
- Renderizacao e UI devem ser consumidores opcionais do estado.
- Benchmarks devem usar seed fixa, numero fixo de steps e build Release.

## Estrategia para Sistemas

Cada sistema deve operar sobre stores e ranges de indices:

- `SpatialSystem`: rebuild/update do grid.
- `PerceptionSystem`: gera inputs sensoriais.
- `NeuralSystem`: gera outputs neurais.
- `MovementSystem`: aplica outputs em velocidade/angulo/posicao.
- `EnergySystem`: aplica custo e cap.
- `InteractionSystem`: alimento/predacao.
- `CollisionSystem`: separacao/elasticidade.
- `ReproductionSystem`: cria filhos.
- `DeathSystem`: remove mortos de forma deterministica.
- `MetricsSystem`: atualiza contadores e series temporais.

## Estrategia para Entidades

Evitar um objeto polimorfico por agente no hot loop. Usar stores:

- `AgentStore`: arrays de posicao, velocidade, angulo, raio, energia, idade, especie, flags, brain index, sensor config index.
- `FoodStore`: arrays de posicao, raio, energia, kind, chunk id, cor, velocidade opcional.
- `SpeciesStore`: nome, cor, limites, quantidade inicial, genoma default, dieta, parametros biologicos e aliases de label/tipo legado.
- `GenomeStore`: configuracoes herdaveis e parametros de mutacao.
- `ObstacleStore`: geometria raster/segmentada para colisao e oclusao.

Objetos OO podem existir como facades ou handles de editor, mas nao devem ser a representacao quente da simulacao.

## Estrategia Data-Oriented / SoA

- Arrays contiguos para dados acessados a cada step.
- Separar dados frios e quentes.
- Usar indices em vez de ponteiros quando possivel.
- Remocao por swap-remove com mapeamento de ids estaveis para UI.
- Agrupar por assinaturas:
  - `PerceptionSignature`: modo de visao, canais, retina count, olhos, FOV, filtros, oclusao.
  - `BrainSignature`: tipo de brain, input size, output size, arquitetura.
  - `MovementSignature`: modo de locomocao.
- Processar grupos em lote.

## Estrategia para Spatial Hash

- Buckets contiguos com indices de entidades.
- Cell size configuravel e benchmarkado.
- Grids separados ou flags por tipo: agentes, comida, obstaculos/chunks.
- Rebuild completo inicialmente; update incremental depois apenas se medido como necessario.
- Queries devem retornar ranges/indices, nao objetos.
- Expor overlay debug do grid sem impactar quando desligado.

## Estrategia para Visao/Percepcao

Percepcao e area critica.

Modos:

- Raycast/single equivalente ao Python.
- Fullbody/raycast estrito.
- Sector/bin vision.

Regras:

- Manter entrada neural compativel: retinas * olhos * canais.
- Subdivisoes de distancia podem alterar intensidade sem aumentar inputs, como no conceito atual.
- Canais R/G/B/D devem ser configuraveis.
- Oclusao por obstaculos opcional.
- Visualizacao da visao deve gerar dados apenas para agente selecionado ou quando debug ativado.
- Processamento por grupos de configuracao.
- Buffers persistentes para candidatos, outputs sensoriais e debug.

## Estrategia para Redes Neurais

Separar:

- `Genome`: configuracao herdavel.
- `BrainState`: pesos, estado recorrente, topologia.
- `BrainExecutor`: forward em lote ou individual.
- `BrainSerializer`: save/load versionado.

Tipos:

- MLP padrao: baseline, batch eficiente.
- Gated MLP: batch por arquitetura.
- Shortcut MLP: batch por arquitetura.
- Modulated MLP: batch por arquitetura.
- Simple RNN: batch por arquitetura com estado por agente.
- NEAT comum: fallback individual ou grupo por topologia.
- NEAT simplificada: fallback individual/grupo por assinatura.
- NEAT recorrente: fallback individual inicialmente.

Regra de performance:

- Redes densas devem ter executor em lote.
- NEAT deve ser opcional e nao degradar MLP quando nao usado.

## Estrategia para Renderizacao SFML

- SFML para viewport 2D, sprites/shapes e texto.
- Renderer deve ler snapshot ou views const dos stores.
- Overlay de debug separado.
- Render detalhado e simples como modos.
- Anti-aliasing/resolution scale opcional.
- Camera, zoom e pan independentes do engine.
- Renderizacao desligada nao deve executar custo de desenho.

## Estrategia para UI

Recomendacao: SFML + Dear ImGui para UI tecnica.

Justificativa:

- Menus, paineis, sliders, checkboxes, tabelas e janelas sao naturais em ImGui.
- Facilita ferramentas tecnicas e metricas.
- Menor atrito que recriar toda UI em SFML puro.
- Pode coexistir com viewport SFML.

Paineis propostos:

- Menu superior.
- Toolbar de simulacao/ferramentas.
- Editor de Genoma.
- Especies/Labels.
- Substrato.
- Preferencias.
- Sistema de Visao.
- Redes Neurais.
- Autosave.
- Aparencia.
- Agent Inspector.
- Neural Viewer.
- Metrics Chart.

## Estrategia para Parametros

- Criar schema declarativo com:
  - nome interno;
  - aliases legados;
  - tipo;
  - default;
  - min/max;
  - categoria;
  - UI control;
  - impacto runtime;
  - se requer rebuild de brain/sensor.
- Saves devem armazenar versao do schema.
- UI deve ser gerada parcialmente a partir do schema onde fizer sentido.

## Estrategia para Persistencia

Formato `.biosim` versionado:

- `version`
- `created_at`
- `config`
- `world`
- `camera`
- `species`
- `labels_legacy optional`
- `genomes`
- `agents`
- `foods`
- `obstacles`
- `metrics optional`
- `rng_state optional`
- `ui_preferences optional`

Compatibilidade:

- Loader deve aceitar nomes antigos `bacteria_`, `predator_`, `auto_export_substrate`.
- Export de genoma separado.
- Export de substrato legado opcional.

## Estrategia para Logs e Diagnostico

- Logger com niveis.
- Arquivo por sessao.
- Crash report.
- Heartbeat opcional.
- Estado minimo de recuperacao ao fechar/crash.
- Logs devem ser baratos quando desligados.

## Estrategia para Profiling

- `ScopedTimer` por sistema.
- Acumuladores por frame/step.
- Report CSV/JSON/Markdown.
- Medir render e simulacao separadamente.
- Profiler deve ter overhead minimo quando desligado.

## Estrategia para Metricas

- Contadores por step:
  - agentes;
  - comida;
  - predadores/especies;
  - mortes;
  - nascimentos;
  - alimentacao;
  - predacao;
  - energia media;
  - inteligencia/grupo.
- Ring buffers por taxa de amostragem.
- Graficos consomem buffers, nao recalculam mundo inteiro.

## Estrategia para Experimentos Headless

- CLI ou modo app sem janela.
- Le arquivo de config.
- Seed fixa.
- Numero de steps/duracao.
- Exporta resultados CSV/JSON.
- Sem UI/render por padrao.
- Usado em benchmark e regressao.

## Testes e Validacao de Paridade

- Testes unitarios de formulas:
  - energia;
  - clamp;
  - bounds circular/retangular;
  - retina input size;
  - canais;
  - reproducao/mutacao.
- Testes de sistema:
  - comida instantanea;
  - comida chunk;
  - predacao;
  - limites por especie;
  - save/load roundtrip.
- Benchmarks de paridade com seed fixa contra Python.

## Decisao Final Recomendada

Comecar C++ com:

1. Engine headless data-oriented.
2. SFML renderer simples.
3. Dear ImGui para UI tecnica.
4. Param schema antes de UI completa.
5. Benchmarks antes de otimizar.
6. Visao sector e MLP batch como primeiras areas de performance.
