# Revisao Arquitetural do AgentBioSim Python

## Escopo

Esta revisao trata a versao Python atual como referencia funcional para uma futura migracao para C++/SFML/CMake. O objetivo aqui nao e traduzir arquivos Python literalmente, mas entender responsabilidades, acoplamentos, riscos e gargalos para projetar uma versao C++ mais adequada a simulacao 2D evolutiva em grande escala.

Nenhum codigo C++ deve ser criado nesta etapa. Nenhum arquivo Python deve ser alterado nesta etapa.

## Diagnostico da Arquitetura Atual

O AgentBioSim atual e uma aplicacao hibrida Python com:

- PyQt6 para UI tecnica, menus, janelas de configuracao e paineis.
- Pygame para renderizacao da simulacao e interacao direta no canvas.
- Um `Engine` headless em `sim/engine.py`, mas ainda bastante acoplado a estruturas Python mutaveis.
- Entidades como objetos Python (`Agent`, `Bacteria`, `Predator`, `Food`) com atributos dinamicos.
- Sistemas de interacao, reproducao, morte e colisao separados em `sim/systems.py`.
- Sensores de retina, raycast e visao setorial em `sim/sensors.py`.
- Kernels Numba/Numpy em `sim/fast_kernels.py`, usados como aceleradores pontuais.
- Uma camada parcial SoA em `sim/soa.py`, ainda pequena diante do estado total da simulacao.
- Persistencia, import/export, autosave e muitos fluxos de UI concentrados em `sim/ui.py`.

A estrutura tem uma boa separacao conceitual inicial, mas a evolucao do projeto criou sobreposicao entre UI, persistencia, aplicacao de parametros, spawn de entidades, ferramentas de canvas, labels, exportacao e detalhes do agente selecionado. A migracao para C++ deve preservar comportamento, mas redesenhar a arquitetura em torno de dados contiguos, sistemas independentes e UI desacoplada do nucleo de simulacao.

## Pontos Fortes

- Existe uma separacao reconhecivel entre parametros (`controllers.py`), mundo/camera (`world.py`), engine (`engine.py`), entidades (`entities.py`), sistemas (`systems.py`), sensores (`sensors.py`) e renderizacao (`render.py`).
- O `Engine` ja pode rodar sem depender exclusivamente da UI, o que ajuda uma futura versao headless.
- O projeto ja possui profiler interno, diagnosticos, benchmark headless e preocupacao explicita com performance.
- O sistema de parametros centralizado em `Params` facilita inventario e migracao.
- A visao ja tem modos alternativos (`single`, `fullbody`, `sector`) e opcoes de canais, olhos e bins.
- O sistema neural ja suporta varios tipos de cerebro: MLP, MLP com gates, MLP com atalho, MLP modulada, RNN simples, NEAT comum, NEAT simplificada e NEAT recorrente.
- Existe suporte a seed/RNG, snapshots, autosave, import/export de agentes/genomas e saves completos `.biosim`.
- Ha otimizacoes ja testadas: spatial hash, batch forward, cache de cerebros, Numba, agrupamento por visao, render simples, retina skip e render headless.
- A UI e rica e representa muito conhecimento acumulado do dominio. Ela deve ser preservada funcionalmente.

## Pontos Fracos

- Estado de simulacao vive majoritariamente em objetos Python, dificultando paralelizacao forte, cache locality e processamento em lote.
- `sim/ui.py` concentra muitas responsabilidades: construcao da UI, persistencia, genoma, labels, autosave, save/load, canvas tools, export/import, atalhos, ajuda, aplicacao de parametros e painel do agente.
- Muitos parametros possuem nomes historicos ou duplicados, por exemplo `auto_export_substrate` ainda usado como autosave.
- Ha convivencia de conceitos antigos e novos: `Bacteria/Predator`, `labels`, "especies", genoma, organismo generico, comida instantanea/chunk e abas antigas possivelmente ainda presentes no codigo.
- A camada SoA atual e pequena e atua mais como ponte temporaria do que como modelo central de dados.
- Alguns hot loops ainda dependem de atributos Python dinamicos, listas e objetos com referencias cruzadas.
- Parte da logica de UI parece conter secao nova e secao legada no mesmo arquivo, exigindo verificacao manual antes de remover qualquer comportamento.
- A renderizacao Pygame e a UI PyQt6 coexistem, mas isso aumenta complexidade de eventos, foco de teclado, overlay, camera e ciclo de render.
- Save/load e export/import estao fortemente ligados a detalhes concretos dos objetos Python.

## Responsabilidades Misturadas

- `ui.py`: UI, persistencia, aplicacao de parametros, export/import, autosave, shortcuts, canvas tools, labels, genoma, agente selecionado, neural viewer e parte da logica de reset/spawn.
- `engine.py`: loop, comandos, spawn, selecao, labels, metricas, camera, render flags, integracao com spatial hash, comida, fisica e parte da coordenacao da UI.
- `entities.py`: classes de dominio, construcao de sensores/cerebros/modelos de energia, update em lote, locomocao e criacao randomica.
- `systems.py`: sistemas relativamente bem separados, mas ainda dependentes de objetos concretos e parametros globais.
- `sensors.py`: contem logica de sensor, query de cena, normalizacao de parametros, varios modos de visao e caminhos otimizados.
- `brain.py`: contem interface, implementacoes de redes, serializacao, mutacao, caches e forward em lote.

## Gargalos Provaveis

1. Percepcao/visao: custo cresce com agentes, retinas, olhos, canais, candidatos proximos e obstaculos.
2. Forward neural: custo cresce com entradas, tamanho da rede, quantidade de tipos de cerebro e heterogeneidade de arquitetura.
3. Conversao objeto-array: quando kernels usam arrays temporarios extraidos de objetos Python.
4. Colisoes/interacoes: custo cresce com densidade local e comida em pedacos.
5. Spatial hash: rebuild/query pode ficar caro se reconstruido ou consultado de forma redundante.
6. Renderizacao: Pygame, anti-aliasing, overlays, visao selecionada, graficos e painel neural podem custar bastante quando ativos.
7. UI e metricas: atualizacao frequente de widgets e graficos pode afetar FPS se nao for desacoplada.
8. Persistencia: saves grandes com muitos pesos e casas decimais podem ser lentos, embora nao sejam custo de loop normal.

## Riscos da Migracao

- Perder parametros e comportamentos acumulados na UI.
- Migrar estrutura Python literalmente e acabar com C++ orientado a objetos pesado, sem ganho grande.
- Alterar dinamica cientifica ao otimizar tempo fixo, energia, colisao, visao ou reproducao.
- Quebrar compatibilidade de saves/genomas sem estrategia de versao.
- Criar UI bonita mas incompleta, sem paridade com menus e opcoes atuais.
- Fazer benchmark sem seed fixa e sem build Release, gerando conclusoes erradas.
- Otimizar visao ou rede sem medir percentuais de wall time.
- Usar NEAT/topologias heterogeneas de forma que inviabilize batch sem fallback claro.

## Decisoes Arquiteturais Recomendadas

- Separar nucleo headless da UI desde o inicio.
- Tratar parametros como schema versionado, nao como `map<string, any>` solto.
- Modelar agentes em `AgentStore`/SoA para estado quente: posicao, velocidade, energia, angulo, raio, label/especie, flags, sensores e indices de cerebro.
- Manter componentes complexos e heterogeneos em stores separados: `BrainStore`, `GenomeStore`, `SpeciesStore`, `FoodStore`, `ObstacleStore`.
- Processar sistemas em fases deterministicas: input de comandos, rebuild/update spatial, percepcao, brain, locomocao, energia/interacao, reproducao/morte, metricas, render.
- Agrupar agentes por `PerceptionSignature` e `BrainSignature` para manter batch mesmo com opcoes flexiveis.
- Implementar visao como modulo critico independente, com benchmarks isolados.
- Usar SFML para renderizacao 2D e considerar Dear ImGui para UI tecnica.
- Criar persistencia versionada (`.biosim`) com separacao entre configuracao, estado da simulacao, genomas, especies, entidades e historico opcional.
- Criar desde cedo benchmarks comparaveis Python vs C++.

## Preservar

- Todos os parametros atuais e seus valores padrao, mesmo que nomes sejam melhorados via alias.
- Todos os modos de visao e canais existentes.
- Todos os tipos de rede neural existentes.
- Sistema de comida instantanea e comida em pedacos/chunk.
- Obstaculos, desenho, colisao e bloqueio opcional de visao.
- Labels/grupos/especies e graficos associados.
- Save/load `.biosim`, import/export de substrato e agente/genoma.
- Autosave, logs, diagnostico e profiler.
- Ferramentas de canvas: selecao, lasso, square, mover, delete, comida, agente, obstaculo.
- Painel de agente selecionado e visualizador neural.
- Modo headless e benchmark.

## Redesenhar

- Representacao de entidades quentes: migrar de objetos individuais para stores contiguos.
- UI: evitar um arquivo unico gigante; separar menus, paineis e comandos.
- Persistencia: criar schema versionado em vez de serializar diretamente formato interno.
- Percepcao: criar pipelines por modo e assinatura, com batch e buffers persistentes.
- Cerebros: separar interface de genoma, estado neural e executor em lote.
- Metricas/graficos: coletar em buffer leve, renderizar apenas quando solicitado.
- Renderizacao: separar visual do estado de simulacao; overlays devem ser custo opcional.
- Parametros: separar configuracao de simulacao, genoma, especie, render, UI, debug e benchmark.

## Conclusao

A versao Python e uma boa referencia funcional, mas nao deve ser copiada literalmente. A futura versao C++ deve ser uma reimplementacao arquitetural orientada a dados, com sistemas independentes, UI tecnica desacoplada, persistencia versionada e benchmark desde a primeira fase. A visao/percepcao deve ser tratada como area critica, porque e o gargalo mais provavel quando a meta e escalar de centenas para milhares de organismos.
