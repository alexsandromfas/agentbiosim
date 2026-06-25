# Fase 22 — UI Base, Menus e Canvas

Atue como arquiteto sênior de software C++, Designer de UI/UX, Designer Sênior, engenheiro de UI técnica, especialista em SFML, Dear ImGui/ImGui-SFML, input routing, arquitetura desacoplada engine/UI, viewport 2D, ferramentas de canvas, seleção interativa e preservação de funcionalidades de uma simulação evolutiva 2D.

Esta fase é crítica porque marca a transição do projeto de um motor funcional majoritariamente headless/renderizado para uma aplicação interativa com UI técnica inicial.

A Fase 22 deve criar a base de UI e interação sem comprometer a arquitetura C++ construída até agora.

Esta fase deve preservar a regra central da migração:

- o núcleo da simulação continua headless;
- a UI não pode virar dona da simulação;
- o Renderer não pode virar controlador de lógica;
- input deve virar comando;
- comandos devem ser aplicados pelo engine/runner;
- menus e toolbar não devem implementar lógica de simulação diretamente;
- a UI inicial deve preparar as Fases 23, 24, 25, 26, 27 e 29, sem invadir o escopo delas.

Até a Fase 21, o projeto já deve ter:

- `AgentStore`;
- `FoodStore`;
- `SpeciesStore`;
- `GenomeStore`;
- `ObstacleStore` ou `ObstacleMap`;
- `SpatialHash`;
- `PerceptionSystem`;
- `NeuralSystem`;
- `MovementSystem`;
- `EnergySystem`;
- `InteractionSystem`;
- `ReproductionSystem`;
- `DeathSystem`;
- `CollisionSystem`;
- `Renderer` SFML básico;
- câmera/mundo;
- comida instantânea;
- comida chunk/pedaços;
- predation/dieta genérica;
- obstáculos e oclusão;
- colisões e física opcional;
- selftests e diagnostics das Fases 7 a 21.

Agora a Fase 22 deve implementar ou finalizar:

- fatoração arquitetural necessária da `App`;
- `SimulationRunner`;
- `AppController`;
- `InputRouter`;
- base de UI técnica;
- integração SFML + Dear ImGui, se mantido;
- menu superior básico;
- toolbar básica;
- play/pause/stop/reset;
- pan/zoom/fit world;
- seleção unitária;
- seleção retangular;
- seleção por lasso, se viável nesta fase;
- ferramentas essenciais de canvas;
- atalhos essenciais;
- debug visual mínimo de seleção/canvas;
- benchmark UI ligada/desligada;
- regressões das Fases 7 a 21;
- documentação da Fase 22.

O objetivo não é implementar a UI completa.

O objetivo é criar uma base arquitetural sólida para que as próximas fases possam adicionar preferências, editor genético, espécies, substrato, agente selecionado, visualizador neural, métricas, save/load e paridade completa sem transformar `App.cpp` em um arquivo gigante.

Além da paridade funcional, a UI C++ deve ser concebida como uma evolução visual e ergonômica da UI Python, não como uma cópia literal. Priorize uma interface técnica moderna, limpa, elegante e profissional, com hierarquia visual clara, espaçamento consistente, controles bem organizados, estados visuais compreensíveis, toolbar objetiva, menus coerentes e uma experiência de uso superior à versão original. Use os recursos mais adequados da biblioteca escolhida, como Dear ImGui/ImGui-SFML ou alternativa equivalente, para criar uma base visual refinada, responsiva e extensível, sem sacrificar performance, testabilidade, desacoplamento do engine ou escopo da fase. Nesta Fase 22, o foco é estabelecer a linguagem visual, a estrutura de navegação e os padrões de interação que servirão de base para as próximas fases de UI.

O prompt abaixo define o escopo mínimo obrigatório, mas ele não é limitante.

Se durante a implementação você perceber algum gap vindo das Fases 7 a 21, alguma conexão incompleta entre `App` → `SimulationRunner` → `AppController` → `InputRouter` → `Renderer` → `SimulationEngine/WorldState`, algum parâmetro cadastrado mas não usado, algum problema de seed/determinismo, algum acoplamento indevido ou alguma melhoria pequena necessária para que a Fase 22 fique correta, você está autorizado a corrigir, desde que:

1. a correção esteja dentro da migração C++/SFML;
2. nenhum arquivo Python seja alterado;
3. a correção seja documentada claramente;
4. você explique por que ela foi necessária;
5. você não avance indevidamente para a Fase 23;
6. você não implemente a janela completa de preferências da Fase 23;
7. você não implemente o editor genético completo da Fase 24;
8. você não implemente painel completo de espécies/labels da Fase 24;
9. você não implemente painel completo de substrato da Fase 24;
10. você não implemente agente selecionado completo da Fase 25;
11. você não implemente visualizador neural da Fase 25;
12. você não implemente gráficos/métricas completos da Fase 26;
13. você não implemente save/load/export/import/autosave da Fase 27;
14. você não implemente benchmark runner formal da Fase 28;
15. você não implemente paridade completa de UI da Fase 29;
16. você não implemente otimização data-oriented profunda da Fase 30;
17. você não faça refatoração grande sem necessidade real;
18. você não esconda decisões arquiteturais importantes.

A intenção é evitar gaps entre fases. As fases são incrementais, mas o sistema precisa funcionar como um todo.

## Estado esperado antes de iniciar

Antes de qualquer alteração, confirme:

- Fases 0 a 8 concluídas e comitadas.
- Fase 9 concluída e comitada.
- Fase 10 concluída e comitada.
- Fase 11 concluída e comitada.
- Fase 12 concluída e comitada.
- Fase 13 concluída e comitada.
- Fase 14 concluída e comitada.
- Fase 15 concluída e comitada.
- Fase 16 concluída e comitada.
- Fase 17 concluída e comitada.
- Fase 18 concluída e comitada.
- Fase 19 concluída e comitada.
- Fase 20 concluída e comitada.
- Fase 21 concluída e comitada.
- Working tree limpo.
- Nenhum arquivo Python alterado.

Rode primeiro:

```bash
git status
git log --oneline -10
````

Se o working tree não estiver limpo, pare e reporte.

## Pré-requisito técnico obrigatório da Fase 22

Antes ou durante esta fase, resolver a Dívida 4 de:

```text
C_SFML_Teste_Legado/MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md
```

A Dívida 4 é:

```text
App acumulando responsabilidades
```

A Fase 22 deve fatorar a arquitetura para evitar que a UI fique acoplada à orquestração do engine.

Crie ou finalize, conforme o estado atual do código:

```text
SimulationRunner
AppController
InputRouter
```

ou nomes equivalentes, desde que a responsabilidade fique clara.

Regras:

* `SimulationRunner` deve executar steps, gerenciar sistemas/stores e manter o modo headless possível.
* `AppController` deve ligar janela, UI, input, renderer e runner.
* `InputRouter` deve traduzir eventos SFML/UI em comandos para o engine/runner.
* `App` não deve continuar crescendo como classe “Deus”.
* `Renderer` não deve processar lógica de input.
* `UI` não deve modificar stores diretamente sem passar por comando/função controlada.
* A simulação deve continuar rodando sem Dear ImGui/UI quando possível.
* O código deve continuar compilando Debug e Release.

Se o projeto atual já tiver componentes equivalentes, reusar/refinar em vez de duplicar.

## Validação curta de continuidade antes da Fase 22

Antes de implementar a Fase 22, faça uma inspeção rápida para evitar construir em cima de fases anteriores quebradas.

Verifique:

1. se `PHASE_9_MLP_STATUS.md` existe;
2. se `PHASE_10_PERCEPTION_SINGLE_STATUS.md` existe;
3. se `PHASE_11_RAYCAST_VISION_STATUS.md` existe;
4. se `PHASE_12_SECTOR_BINS_STATUS.md` existe;
5. se `PHASE_13_REPRODUCTION_GENOME_STATUS.md` existe;
6. se `PHASE_14_DENSE_ADVANCED_NETWORKS_STATUS.md` existe;
7. se `PHASE_15_SIMPLE_RNN_STATUS.md` existe;
8. se `PHASE_16_NEAT_FAMILY_STATUS.md` existe;
9. se `PHASE_17_SPECIES_LABELS_GENOMES_STATUS.md` existe;
10. se `PHASE_18_PREDATION_DIET_STATUS.md` existe;
11. se `PHASE_19_CHUNK_FOOD_STATUS.md` existe;
12. se `PHASE_20_OBSTACLES_OCCLUSION_STATUS.md` existe;
13. se `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md` existe;
14. se a aplicação renderizada ainda abre janela SFML;
15. se o modo headless/testes ainda funciona;
16. se câmera pan/zoom/fit world básico já existe ou precisa ser exposto via input;
17. se o Renderer consegue desenhar agentes, comida, obstáculos e mundo;
18. se existe forma de converter coordenadas de tela para mundo;
19. se existe forma de selecionar agente por coordenada;
20. se existe query espacial suficiente para seleção;
21. se existe comando ou função segura para pausar/retomar simulação;
22. se existe comando ou função segura para reset/stop;
23. se existe função segura para spawn de comida;
24. se existe função segura para spawn de agente;
25. se existe função segura para apagar/matar selecionados;
26. se existe função segura para pintar/apagar obstáculo da Fase 20;
27. se Fase 21 não acoplou física à UI;
28. se nenhum arquivo Python foi alterado.

Se houver problema pequeno e diretamente relacionado à continuidade da Fase 22, corrija e documente em `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md`.

Se houver problema grande, pare e reporte antes de continuar.

## Documentos obrigatórios

Leia antes de implementar:

* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/CODEX_MIGRATION_GUIDE.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/MIGRATION_PHASES.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PLANNING_COVERAGE_AUDIT.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PROPOSED_CPP_ARCHITECTURE.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/ARCHITECTURE_REVIEW.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/FEATURE_INVENTORY.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PARAMETER_INVENTORY.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/UI_INVENTORY.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/CURRENT_MODULE_MAP.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/BENCHMARK_PLAN.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/MIGRATION_RISKS.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md`
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`

Leia também os status das Fases 20 e 21 se precisar confirmar decisões de obstáculos/canvas e física.

Se algum desses documentos não existir no repositório, registre isso no status da Fase 22.

## Arquivos Python obrigatórios como referência funcional

Consulte os arquivos Python relevantes como referência funcional, não como arquitetura a ser copiada literalmente:

* `sim/ui.py`
* `sim/game.py`
* `sim/render.py`
* `sim/engine.py`
* `sim/world.py`
* `sim/controllers.py`
* `sim/entities.py`
* `sim/systems.py`
* `sim/spatial.py`
* `sim/obstacles.py`
* `sim/sensors.py`
* `sim/neural_viewer.py`
* `sim/random_utils.py`

Se precisar consultar outros arquivos Python, pode consultar. Registre em `PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md` quais arquivos adicionais foram consultados e por quê.

## Objetivo da Fase 22

Criar a UI técnica inicial, menus básicos e ferramentas essenciais de canvas.

Fluxo esperado:

```text
SFML Window
↓
AppController
  gerencia janela, eventos, ciclo visual e chamada da UI
↓
InputRouter
  traduz eventos SFML em comandos de alto nível
↓
UI Layer / ImGui Layer
  menus
  toolbar
  tool state
  flags visuais
↓
SimulationRunner
  mantém engine/headless
  executa steps
  aplica comandos
↓
WorldState / Stores / Systems
  não dependem de UI
↓
Renderer
  desenha mundo
  desenha overlays básicos
↓
Diagnostics/Benchmark
  UI on/off
  input/canvas smoke tests
```

A Fase 22 deve provar que:

* a janela renderizada ainda abre;
* engine continua headless;
* `App` foi fatorada ou reduzida conforme Dívida 4;
* menus básicos existem;
* toolbar básica existe;
* play/pause funciona;
* stop/reset funciona de forma segura;
* pan/zoom/fit world funcionam;
* seleção unitária funciona;
* seleção retangular funciona;
* lasso funciona ou fica documentada como pendência curta se tecnicamente inviável nesta fase;
* ferramentas essenciais de canvas existem em nível inicial;
* atalhos essenciais funcionam;
* input não fica espalhado por Renderer;
* UI não manipula stores diretamente de forma perigosa;
* benchmarks UI on/off existem;
* regressões das Fases 7 a 21 continuam passando;
* Fase 23 não foi iniciada.

## Escopo mínimo obrigatório

Implemente ou ajuste:

1. `SimulationRunner`;
2. `AppController`;
3. `InputRouter`;
4. base de UI técnica com SFML + Dear ImGui/ImGui-SFML, se mantido;
5. fallback/documentação se Dear ImGui ainda não puder ser integrado;
6. menu superior básico;
7. toolbar básica;
8. play/pause;
9. stop/reset;
10. step único ou avanço manual, se simples;
11. controle de `time_scale` básico na toolbar, se já existir no registry;
12. pan de câmera;
13. zoom de câmera;
14. fit world;
15. seleção unitária;
16. seleção retangular;
17. seleção por lasso, se viável;
18. limpar seleção;
19. mover selecionados, se já houver infraestrutura simples;
20. deletar/matar selecionados;
21. ferramenta de inserir comida;
22. ferramenta de inserir agente/genoma default;
23. ferramenta de pintar obstáculo usando Fase 20;
24. ferramenta de apagar obstáculo usando Fase 20;
25. ferramenta de pan com botão direito;
26. overlay visual de seleção;
27. overlay visual de retângulo/lasso;
28. overlay visual de ferramenta ativa;
29. atalhos essenciais;
30. separação clara de UI/input/engine/render;
31. selftest/smoke test da Fase 22;
32. diagnostics/microbenchmark da Fase 22;
33. regressões das Fases 7 a 21;
34. documentação da Fase 22.

## Escopo permitido adicional

Você pode fazer pequenas correções ou melhorias além do escopo mínimo se elas forem necessárias para evitar gaps entre fases.

Exemplos permitidos:

* criar `CommandQueue` simples se ainda não existir;
* criar enum de ferramentas de canvas;
* criar `SelectionStore` ou `SelectionState` leve;
* criar helpers de conversão tela↔mundo;
* criar helpers geométricos para seleção retangular/lasso;
* criar `UiState` leve para tool atual, flags e janelas básicas;
* criar overlay básico de seleção no Renderer;
* refinar Camera2D para suportar pan/zoom/fit;
* fatorar `App.cpp` se estiver acumulando responsabilidades;
* adicionar stubs documentados para menus que só serão implementados em fases futuras;
* adicionar contadores de UI para diagnostics;
* corrigir atalhos se já existiam parcialmente;
* corrigir spawn por clique para respeitar obstáculos/mundo;
* corrigir seleção para respeitar mundo/câmera;
* atualizar `TECHNICAL_DEBT_REGISTER.md` se a UI revelar nova dívida técnica real;
* adicionar testes pequenos que protejam integração entre Fases 20 a 22.

Essas melhorias devem ser pequenas, justificadas e documentadas.

## Fora de escopo

Não implemente nesta fase:

* UI de parâmetros e preferências completa da Fase 23;
* janelas completas de simulação/física/performance;
* janela completa de sistema de visão;
* janela completa de redes neurais;
* janela completa de autosave;
* janela completa de aparência;
* editor genético completo da Fase 24;
* painel completo de espécies/labels da Fase 24;
* painel completo de substrato da Fase 24;
* import/export de genoma;
* import/export de substrato;
* save/load `.biosim`;
* autosave/recovery;
* agente selecionado completo da Fase 25;
* visualizador neural da Fase 25;
* gráficos/métricas completos da Fase 26;
* benchmark runner formal da Fase 28;
* paridade completa da UI da Fase 29;
* otimização data-oriented profunda da Fase 30;
* redesign visual final;
* tema final/polimento estético;
* persistência de layout de UI;
* drag-and-drop de arquivos;
* atalhos configuráveis;
* scripting;
* multi-janela avançada.

Não altere arquivos Python.

## Dependência Dear ImGui / ImGui-SFML

A arquitetura recomenda UI técnica com Dear ImGui junto da viewport SFML.

Nesta fase:

* verifique se Dear ImGui/ImGui-SFML já existe no projeto/vcpkg/CMake;
* se existir, integre de forma limpa;
* se não existir, avalie o menor caminho seguro para integrar sem quebrar build;
* se a integração for pequena e segura, faça;
* se a integração for grande/riscada, não invente gambiarra: implemente uma UI técnica mínima em SFML ou stubs controlados, documente a decisão e registre pendência para revisão;
* preserve build Debug/Release;
* preserve engine headless;
* não acople Dear ImGui ao núcleo da simulação;
* não faça a simulação depender de Dear ImGui.

Documente claramente a decisão:

```text
Dear ImGui integrado nesta fase
```

ou

```text
Dear ImGui adiado, UI base implementada com alternativa mínima/stubs
```

com justificativa.

## Menus superiores mínimos

Criar menu superior básico, mesmo que algumas ações sejam stubs documentados para fases futuras.

Menus esperados:

```text
Arquivo
View
Preferencias
Agente/Genoma
Ajuda
```

### Arquivo

Nesta fase, implementar apenas o que for seguro:

* `Novo` pode chamar reset seguro, se já existir.
* `Abrir Simulação` deve ser stub/desabilitado até Fase 27.
* `Salvar Simulação` deve ser stub/desabilitado até Fase 27.
* `Salvar Como` deve ser stub/desabilitado até Fase 27.
* `Exportar substrato JSON` deve ser stub/desabilitado até Fase 27.
* `Importar substrato JSON` deve ser stub/desabilitado até Fase 27.
* `Sair` pode fechar janela com segurança.

Não implementar save/load real nesta fase.

### View

Implementar o que já existir tecnicamente:

* alternar render simples/detalhado, se já existir;
* mostrar/ocultar spatial hash, se já existir;
* mostrar/ocultar visão do agente selecionado, se já existir sem custo alto;
* fit world;
* reset camera;
* mostrar/ocultar overlays de seleção;
* placeholders para gráfico e neural viewer até Fases 25/26.

### Preferências

Nesta fase:

* criar menu placeholder;
* não criar janelas completas;
* permitir abrir stubs ou mensagens “implementado na Fase 23”;
* não implementar controles completos de parâmetros.

### Agente/Genoma

Nesta fase:

* criar menu placeholder;
* não implementar import/export real de genoma/agente;
* permitir ação básica como “criar agente default” se já houver função segura;
* deixar export/import para Fase 24/27;
* preservar terminologia com compatibilidade: Agente agora, Genoma como destino conceitual.

### Ajuda

Nesta fase:

* criar janela simples ou overlay com atalhos essenciais;
* listar teclas implementadas;
* listar ferramentas implementadas;
* indicar que UI completa vem em fases futuras.

## Toolbar mínima

Criar toolbar básica com:

* Play/Pause;
* Stop/Reset;
* Step, se simples;
* ferramenta ativa;
* botão selecionar;
* botão seleção retangular;
* botão lasso, se implementado;
* botão comida;
* botão agente default;
* botão obstáculo/pincel;
* botão apagar obstáculo;
* botão mover, se implementado;
* botão deletar/matar selecionados;
* botão fit world;
* indicador básico de quantidade de agentes/comida/obstáculos;
* indicador básico de FPS/steps, se já houver contador barato.

Não implementar painel completo de métricas.

Não implementar gráfico.

## Ferramentas de canvas

Implementar ou preparar enum/estado para:

```text
Select
RectangleSelect
LassoSelect
AddFood
AddAgent
PaintObstacle
EraseObstacle
Move
Delete
Pan
```

Regras:

* ferramenta atual deve ficar em `UiState`/`InputRouter`, não em Renderer;
* Renderer pode desenhar overlay da ferramenta atual;
* clique em mundo deve usar conversão tela→mundo;
* comandos de alteração devem passar por runner/engine;
* seleção deve usar ids/índices estáveis ou mapping seguro;
* seleção não deve guardar ponteiros inválidos;
* deletar/matar selecionados deve ser seguro com swap-remove;
* inserir comida deve respeitar mundo e obstáculos;
* inserir agente deve respeitar mundo, obstáculos e espécie/genoma default;
* pintar obstáculo deve chamar infraestrutura da Fase 20;
* apagar obstáculo deve chamar infraestrutura da Fase 20;
* mover selecionados não deve atravessar obstáculos sem política documentada;
* pan com botão direito deve continuar funcionando independentemente da ferramenta ativa.

## Seleção

Implementar:

### Seleção unitária

* clique em agente seleciona o agente mais próximo sob o cursor;
* clique vazio limpa ou preserva seleção conforme política documentada;
* shift/ctrl para adicionar/remover seleção, se simples;
* seleção usa ids estáveis quando possível;
* seleção sobre comida/obstáculo pode ser adiada, mas documentar.

### Seleção retangular

* arrastar retângulo;
* selecionar agentes dentro da região;
* considerar coordenadas mundo, não coordenadas tela;
* overlay visual do retângulo;
* shift/ctrl para adicionar/remover, se simples;
* não selecionar entidades mortas/removidas.

### Lasso

* implementar se viável nesta fase;
* se não for viável, criar estrutura/enum e deixar pendência documentada curta;
* se implementado, usar polígono em coordenadas mundo;
* overlay visual do lasso;
* selecionar agentes dentro do polígono;
* performance suficiente para 1000 agentes.

## Pan, zoom e fit world

Requisitos:

* pan com botão direito;
* pan por WASD/setas, se já existia ou for simples;
* zoom por scroll centralizado no cursor;
* fit world com tecla/botão;
* reset camera, se simples;
* câmera deve respeitar mundo retangular/circular;
* zoom deve ter min/max seguro;
* pan/zoom não deve alterar simulação;
* câmera deve continuar independente do engine headless.

## Atalhos essenciais

Implementar ou confirmar:

```text
Space: pausar/retomar
Esc: limpar seleção/cancelar ferramenta ativa
Delete: deletar/matar selecionados
R: reset população/simulação, se já houver função segura
F: fit world
T: alternar renderer simples/detalhado, se já existir
V: alternar visão/ativação visual, se já existir sem custo alto
WASD/setas: mover câmera
Scroll: zoom
S: ferramenta seleção
F ou tecla alternativa: inserir comida, se não conflitar com fit world
A: inserir agente default
M: mover selecionados
D: ferramenta delete/matar
O ou tecla alternativa: obstáculo/pincel
E ou tecla alternativa: apagar obstáculo
```
Todos esses atalhos devem estar no menu "ajuda".

Se houver conflito de tecla, escolha uma política clara e documente.

Não implemente sistema de atalhos configuráveis.

## InputRouter

Criar ou finalizar `InputRouter`.

Responsabilidades:

* receber eventos SFML;
* receber estado da UI, se necessário;
* decidir se evento foi capturado pela UI;
* converter mouse para mundo;
* acionar comandos de camera;
* acionar comandos de ferramenta;
* acionar atalhos;
* não modificar stores diretamente;
* não chamar sistemas diretamente;
* não desenhar;
* não depender de detalhes internos de rede neural;
* permitir testes/smoke sem janela quando possível.

## AppController

Criar ou finalizar `AppController`.

Responsabilidades:

* possuir/gerenciar janela SFML;
* inicializar UI;
* inicializar renderer;
* conectar input router;
* chamar runner;
* chamar renderer;
* chamar UI draw;
* não conter regras de simulação;
* não conter regras de colisão;
* não conter regras de percepção;
* não conter regras de reprodução;
* não conter lógica de rede neural.

## SimulationRunner

Criar ou finalizar `SimulationRunner`.

Responsabilidades:

* manter estado da simulação;
* executar dt fixo;
* aplicar comandos;
* chamar sistemas na ordem correta;
* expor API segura para UI/input;
* permitir modo headless;
* permitir diagnostics;
* permitir reset/stop/play/pause;
* não depender de Dear ImGui;
* não depender de elementos visuais da UI.

## CommandQueue / comandos

Se ainda não existir, criar uma camada simples de comandos.

Comandos mínimos:

```text
PauseToggle
SetPaused
ResetSimulation
FitWorldCamera
SelectAtWorldPoint
SelectRect
SelectLasso
ClearSelection
DeleteSelected
MoveSelected
SpawnFoodAt
SpawnAgentAt
PaintObstacleAt
EraseObstacleAt
SetCanvasTool
ToggleSimpleRender
ToggleSpatialHashOverlay
ToggleSelectedVisionOverlay
```

Nem todos precisam ser formalizados como classe robusta nesta fase, mas a arquitetura deve apontar para isso.

Não espalhar lógica de comando pelo `App.cpp`.

## Renderer / overlays

Renderer deve continuar desenhando:

* mundo/substrato;
* agentes;
* comida;
* obstáculos;
* overlays já existentes;
* overlay de seleção;
* overlay de retângulo/lasso;
* overlay de ferramenta ativa;
* overlay simples de spatial hash, se já existir.

Regras:

* Renderer não decide seleção;
* Renderer não decide comandos;
* Renderer não decide ferramentas;
* Renderer não altera stores;
* Renderer pode receber `SelectionState` e `CanvasOverlayState` somente leitura;
* custo de overlays deve ser zero ou quase zero quando desligados.

## Engine headless

A Fase 22 não pode quebrar o modo headless.

Requisitos:

* selftests continuam sem janela;
* diagnostics continuam rodando sem UI;
* runner deve permitir execução sem SFML window, quando aplicável;
* UI não deve ser necessária para executar sistemas;
* comandos devem ser testáveis sem interação visual, quando possível.

## Determinismo

A UI em si pode ser interativa, mas comandos aplicados com mesma sequência devem ser determinísticos.

Requisitos:

* spawn por clique deve respeitar seed quando houver aleatoriedade;
* reset deve restaurar estado determinístico quando configurado;
* seleção não deve depender de ordem instável de containers;
* deletar selecionados deve ser determinístico;
* lasso/retângulo deve usar coordenadas mundo estáveis;
* input não deve usar `std::random_device`.

## Performance

A Fase 22 deve medir custo UI ligada/desligada.

Requisitos:

* medir app headless;
* medir janela SFML sem UI;
* medir janela SFML com UI base;
* medir toolbar/menu abertos;
* medir overlays desligados;
* medir overlay de seleção ligado;
* medir seleção unitária;
* medir seleção retangular;
* medir lasso, se implementado;
* medir pan/zoom;
* medir canvas tools básicas;
* medir 100, 300, 600 e 1000 agentes;
* medir comida/obstáculos em cenários moderados;
* registrar custo de UI, render e simulação separadamente quando possível;
* não otimizar prematuramente além do necessário;
* registrar gargalos observados.

## Testes obrigatórios

Criar comando:

```text
--phase22-selftest
```

Testar pelo menos:

### Arquitetura

1. `SimulationRunner` existe ou equivalente documentado.
2. `AppController` existe ou equivalente documentado.
3. `InputRouter` existe ou equivalente documentado.
4. `App` foi reduzida/fatorada conforme Dívida 4 ou justificativa documentada.
5. Engine continua headless.
6. Renderer continua desacoplado da lógica.
7. UI não depende de stores mutáveis diretamente.
8. InputRouter não desenha.
9. Renderer não processa input.
10. SimulationRunner não depende de Dear ImGui.
11. SimulationRunner não depende da UI.
12. AppController não contém regras de simulação.
13. CommandQueue/comandos existem ou equivalente documentado.
14. Nenhum arquivo Python foi alterado.

### Build/UI

15. Build Debug passa.
16. Build Release passa.
17. Aplicação renderizada abre janela.
18. Aplicação fecha sem crash.
19. UI base inicializa sem crash, se integrada.
20. Dear ImGui integrado corretamente ou decisão de adiamento documentada.
21. Menu superior aparece ou fallback documentado.
22. Toolbar aparece ou fallback documentado.
23. UI desligada/fallback não quebra simulação.
24. Render desligado ainda permite headless.
25. Render ligado continua mostrando mundo/agentes/comida/obstáculos.

### Menus

26. Menu `Arquivo` existe.
27. `Arquivo > Novo` executa reset seguro ou stub documentado.
28. `Arquivo > Abrir` é stub/desabilitado até Fase 27.
29. `Arquivo > Salvar` é stub/desabilitado até Fase 27.
30. `Arquivo > Salvar Como` é stub/desabilitado até Fase 27.
31. `Arquivo > Exportar substrato` é stub/desabilitado até Fase 27.
32. `Arquivo > Importar substrato` é stub/desabilitado até Fase 27.
33. `Arquivo > Sair` fecha janela.
34. Menu `View` existe.
35. `View > Fit world` funciona.
36. `View > Spatial hash` alterna overlay se já existir.
37. `View > Render simples` alterna modo se já existir.
38. Menu `Preferencias` existe como placeholder.
39. Menu `Agente/Genoma` existe como placeholder.
40. Menu `Ajuda` existe.
41. Ajuda mostra atalhos implementados ou equivalente.

### Toolbar

42. Botão Play/Pause funciona.
43. Botão Stop/Reset funciona.
44. Botão Step funciona ou é stub documentado.
45. Botão Fit World funciona.
46. Botão Select ativa ferramenta de seleção.
47. Botão Rectangle Select ativa ferramenta.
48. Botão Lasso ativa ferramenta ou stub documentado.
49. Botão Food ativa ferramenta de comida.
50. Botão Agent ativa ferramenta de agente default.
51. Botão Paint Obstacle ativa pincel.
52. Botão Erase Obstacle ativa borracha.
53. Botão Move ativa ferramenta ou stub documentado.
54. Botão Delete ativa ferramenta ou executa delete selecionados.
55. Toolbar mostra ferramenta ativa.
56. Toolbar não altera stores diretamente.

### Câmera

57. Pan com botão direito funciona.
58. Pan por teclado funciona ou é documentado.
59. Zoom por scroll funciona.
60. Zoom centralizado no cursor funciona ou política documentada.
61. Zoom tem limites seguros.
62. Fit world enquadra mundo retangular.
63. Fit world enquadra mundo circular.
64. Reset camera funciona, se implementado.
65. Camera não altera estado da simulação.
66. Conversão tela→mundo funciona.
67. Conversão mundo→tela funciona.

### Seleção unitária

68. Clique em agente seleciona agente.
69. Clique no vazio limpa seleção ou política documentada.
70. Agente mais próximo sob cursor é selecionado.
71. Seleção não usa ponteiro inválido.
72. Seleção sobre agente removido é limpa com segurança.
73. Shift/Ctrl adiciona/remove seleção ou pendência documentada.
74. Seleção é determinística.
75. Overlay de seleção aparece.
76. Overlay de seleção desliga quando não há seleção.

### Seleção retangular

77. Arrastar retângulo cria overlay.
78. Soltar retângulo seleciona agentes dentro.
79. Retângulo usa coordenadas mundo.
80. Retângulo funciona com zoom diferente de 1.
81. Retângulo funciona com câmera deslocada.
82. Retângulo ignora agentes mortos/removidos.
83. Retângulo é determinístico.
84. Retângulo não seleciona comida/obstáculo salvo política documentada.

### Lasso

85. Lasso existe ou pendência documentada.
86. Se implementado, overlay de lasso aparece.
87. Se implementado, lasso seleciona agentes dentro do polígono.
88. Se implementado, lasso usa coordenadas mundo.
89. Se implementado, lasso funciona com zoom/câmera.
90. Se implementado, lasso é determinístico.
91. Se adiado, enum/estrutura de ferramenta está preparada.

### Canvas tools

92. Ferramenta de comida insere comida em posição válida.
93. Inserir comida respeita mundo retangular.
94. Inserir comida respeita mundo circular.
95. Inserir comida respeita obstáculos.
96. Ferramenta de agente insere agente default em posição válida.
97. Inserir agente respeita mundo retangular.
98. Inserir agente respeita mundo circular.
99. Inserir agente respeita obstáculos.
100. Ferramenta pintar obstáculo chama Fase 20.
101. Ferramenta apagar obstáculo chama Fase 20.
102. Pintar/apagar obstáculo não quebra visão/oclusão.
103. Ferramenta mover selecionados funciona ou stub documentado.
104. Mover selecionados respeita mundo.
105. Mover selecionados respeita obstáculos ou política documentada.
106. Delete/matar selecionados funciona.
107. Delete/matar selecionados é seguro com swap-remove.
108. Delete/matar selecionados atualiza seleção.

### Atalhos

109. Space pausa/retoma.
110. Esc limpa seleção/cancela ferramenta.
111. Delete remove/mata selecionados.
112. R reseta simulação/população ou stub documentado.
113. F faz fit world ou conflito documentado.
114. T alterna render simples/detalhado, se existir.
115. V alterna visão/overlay, se existir.
116. WASD/setas movem câmera ou pendência documentada.
117. Scroll faz zoom.
118. S ativa seleção.
119. Tecla de comida funciona sem conflito.
120. A ativa inserir agente.
121. M ativa mover ou stub documentado.
122. D ativa delete ou stub documentado.
123. Tecla de obstáculo funciona ou pendência documentada.
124. Tecla de apagar obstáculo funciona ou pendência documentada.

### Integração com sistemas anteriores

125. Fase 7 comida instantânea continua funcionando.
126. Fase 8 locomoção continua funcionando.
127. Fase 9 MLP continua funcionando.
128. Fase 10 visão single continua funcionando.
129. Fase 11 raycast/fullbody continua funcionando.
130. Fase 12 sector/bins continua funcionando.
131. Fase 13 reprodução/genoma continua funcionando.
132. Fase 14 redes densas avançadas continuam funcionando.
133. Fase 15 RNN continua funcionando.
134. Fase 16 NEAT continua funcionando.
135. Fase 17 espécies/labels/genomas continuam funcionando.
136. Fase 18 predation/dieta continua funcionando.
137. Fase 19 comida chunk continua funcionando.
138. Fase 20 obstáculos/oclusão continuam funcionando.
139. Fase 21 colisões/física continuam funcionando.
140. Headless continua funcionando.
141. Render continua funcionando.
142. UI ligada não altera simulação quando o usuário não interage.
143. UI desligada preserva custo básico.

### Fora de escopo

144. Fase 23 não foi iniciada.
145. Preferências completas não foram implementadas.
146. Fase 24 não foi iniciada.
147. Editor genético completo não foi implementado.
148. Fase 25 não foi iniciada.
149. Visualizador neural completo não foi implementado.
150. Fase 26 não foi iniciada.
151. Gráficos completos não foram implementados.
152. Fase 27 não foi iniciada.
153. Save/load final não foi implementado.
154. Fase 28 não foi iniciada.
155. Benchmark runner formal não foi implementado.
156. Fase 29 não foi iniciada.
157. Paridade completa de UI não foi implementada.
158. Nenhum arquivo Python foi alterado.

## Diagnostics/microbenchmark obrigatório

Criar comando:

```text
--phase22-diagnostics
```

Medir de forma simples:

### Cenários mínimos

1. Headless:

   * 100 agentes / 100 comidas;
   * 300 agentes / 150 comidas;
   * 600 agentes / 300 comidas;
   * 1000 agentes / 500 comidas.

2. Janela SFML sem UI, se possível:

   * 100 agentes;
   * 300 agentes;
   * 600 agentes;
   * 1000 agentes.

3. Janela SFML com UI base:

   * menu fechado;
   * menu aberto;
   * toolbar visível;
   * ajuda aberta;
   * overlays desligados;
   * overlay seleção ligado.

4. Input/canvas:

   * pan/zoom;
   * seleção unitária;
   * seleção retangular;
   * lasso, se implementado;
   * inserir comida;
   * inserir agente;
   * pintar obstáculo;
   * apagar obstáculo;
   * deletar selecionados.

5. Render/UI:

   * render simples;
   * render detalhado;
   * spatial hash overlay on/off;
   * seleção overlay on/off;
   * tool overlay on/off.

### Métricas obrigatórias

Registrar:

* wall time total;
* FPS médio;
* steps simulados;
* us/step;
* tempo de simulação;
* tempo de render;
* tempo de UI;
* tempo de input;
* tempo de overlays;
* tempo de seleção unitária;
* tempo de seleção retangular;
* tempo de lasso, se implementado;
* tempo de pan/zoom;
* tempo de spawn por ferramenta;
* tempo de pintar/apagar obstáculo;
* número de agentes;
* número de comidas;
* número de obstáculos;
* número de entidades selecionadas;
* número de comandos processados;
* overhead UI off vs on;
* overhead overlays off vs on;
* build Debug/Release;
* seed;
* commit hash;
* observações.

Não criar benchmark runner formal completo ainda.

## Build obrigatório

Compilar:

```bash
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Debug
cmake --build C_SFML_Teste_Legado/AgentBioSimCpp/build --config Release
```

Se o caminho do executável for diferente, adapte e documente.

## Testes/regressões obrigatórios

Rode:

```bash
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase22-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --phase22-diagnostics

C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase7-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase8-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase9-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase10-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase11-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase12-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase13-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase14-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase15-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase16-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase17-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase18-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase19-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase20-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase21-selftest
```

Se algum comando anterior não existir, registre claramente.

Se algum teste falhar, corrija se for pequeno e diretamente relacionado. Se for falha estrutural grande, pare e reporte.

## Documentação obrigatória

Criar:

```text
C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_22_UI_BASE_MENUS_CANVAS_STATUS.md
```

Esse arquivo deve conter:

* escopo executado;
* fora de escopo;
* documentos consultados;
* arquivos Python consultados;
* arquivos Python adicionais consultados;
* arquivos C++ criados;
* arquivos C++ modificados;
* correções de continuidade vindas das Fases 7 a 21, se houver;
* melhorias além do prompt mínimo, se houver;
* justificativa de cada melhoria além do escopo mínimo;
* decisão sobre Dear ImGui/ImGui-SFML;
* decisão sobre fallback de UI, se houver;
* decisão sobre fatoração da `App`;
* decisão sobre `SimulationRunner`;
* decisão sobre `AppController`;
* decisão sobre `InputRouter`;
* decisão sobre CommandQueue/comandos;
* decisão sobre menus;
* decisão sobre toolbar;
* decisão sobre play/pause;
* decisão sobre stop/reset;
* decisão sobre step manual;
* decisão sobre pan;
* decisão sobre zoom;
* decisão sobre fit world;
* decisão sobre seleção unitária;
* decisão sobre seleção retangular;
* decisão sobre lasso;
* decisão sobre seleção múltipla;
* decisão sobre `SelectionState`;
* decisão sobre ferramentas de canvas;
* decisão sobre inserir comida;
* decisão sobre inserir agente;
* decisão sobre pintar obstáculo;
* decisão sobre apagar obstáculo;
* decisão sobre mover selecionados;
* decisão sobre deletar/matar selecionados;
* decisão sobre atalhos;
* decisão sobre overlays;
* decisão sobre render/UI separation;
* decisão sobre engine headless;
* decisão sobre determinismo;
* decisão sobre performance;
* parâmetros usados;
* parâmetros pendentes;
* itens cobertos de `FEATURE_INVENTORY.md`;
* itens cobertos de `PARAMETER_INVENTORY.md`;
* itens cobertos de `UI_INVENTORY.md`;
* itens de UI deixados para Fase 23;
* itens de UI deixados para Fase 24;
* itens de UI deixados para Fase 25;
* itens de UI deixados para Fase 26;
* itens de UI deixados para Fase 27;
* itens de UI deixados para Fase 29;
* como comida instantânea da Fase 7 foi protegida;
* como locomoção da Fase 8 foi protegida;
* como visão das Fases 10 a 12 foi protegida;
* como reprodução/genoma da Fase 13 foi protegida;
* como redes neurais das Fases 14 a 16 foram protegidas;
* como espécies/genomas da Fase 17 foram protegidos;
* como predation/dieta da Fase 18 foi protegida;
* como comida chunk da Fase 19 foi protegida;
* como obstáculos/oclusão da Fase 20 foram protegidos;
* como colisões/física da Fase 21 foram protegidas;
* testes executados;
* resultado dos testes;
* resultado dos diagnostics/microbenchmark;
* diagnóstico visual/runtime;
* divergências contra Python;
* limitações atuais;
* pendências para Fase 23;
* pendências para Fase 24;
* pendências para Fase 25;
* pendências para Fase 26;
* pendências para Fase 27;
* pendências para Fase 28;
* pendências para Fase 29;
* pendências para Fase 30;
* confirmação de que não implementou preferências completas;
* confirmação de que não implementou editor genético completo;
* confirmação de que não implementou visualizador neural completo;
* confirmação de que não implementou save/load final;
* confirmação de que não implementou benchmark runner formal;
* confirmação de que nenhum Python foi alterado;
* confirmação de que não avançou para Fase 23.

## Atualizações documentais permitidas

Atualize, se necessário:

* `TECHNICAL_DEBT_REGISTER.md`, especialmente para marcar a Dívida 4 como resolvida ou parcialmente resolvida;
* `MIGRATION_RISKS.md`, caso encontre risco novo;
* `BENCHMARK_PLAN.md`, apenas se encontrar lacuna objetiva sobre a Fase 22;
* `MIGRATION_PHASES.md`, apenas se houver ajuste pequeno de status ou pendência factual;
* `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`, apenas se encontrar erro factual que precisa ser corrigido.

Não reescreva cronograma inteiro.

Não renumere fases.

Não altere fases futuras por preferência pessoal.

## Qualidade arquitetural esperada

A solução deve:

* manter engine/headless possível;
* manter SFML/Renderer desacoplado;
* manter UI desacoplada;
* resolver ou mitigar fortemente a Dívida 4;
* impedir que `App.cpp` vire classe gigante;
* separar input de render;
* separar input de engine;
* separar UI de sistemas;
* usar comandos ou API controlada para alterações;
* preservar comida instantânea;
* preservar comida chunk;
* preservar obstáculos;
* preservar oclusão;
* preservar colisões/física;
* preservar DietConfig;
* preservar predation;
* preservar SpeciesStore;
* preservar GenomeStore;
* preservar todos os tipos neurais;
* preservar determinismo por seed;
* preservar dt fixo;
* preparar caminho claro para Fase 23 preferências;
* preparar caminho claro para Fase 24 editor genético/espécies/substrato;
* preparar caminho claro para Fase 25 agente selecionado/neural viewer;
* preparar caminho claro para Fase 26 métricas/profiler;
* preparar caminho claro para Fase 27 save/load;
* preparar caminho claro para Fase 29 paridade completa de UI;
* preparar caminho claro para Fase 30 otimização;
* medir antes/depois.

## Dívidas técnicas

Consulte `TECHNICAL_DEBT_REGISTER.md`.

Nesta fase, a Dívida 4 deve ser resolvida ou, no mínimo, mitigada de forma concreta e documentada:

```text
Dívida 4 — App acumulando responsabilidades
```

Nesta fase, não é obrigatório resolver:

```text
Dívida 5 — Alocações temporárias no NeuralSystem
Dívida 6 — Version.hpp desatualizado
```

A menos que isso seja pequeno, seguro e útil.

Não implemente Fase 23.

Se uma dívida atrapalhar diretamente `SimulationRunner`, `AppController`, `InputRouter`, `Renderer`, `Camera2D`, `CommandQueue`, `SelectionState` ou ferramentas de canvas, corrija ou documente.

## Saída final esperada

Ao final, responda com:

1. resumo executivo;
2. git status;
3. último commit antes da Fase 22;
4. arquivos criados;
5. arquivos modificados;
6. arquivos Python consultados;
7. documentos consultados;
8. correções de continuidade das Fases 7 a 21, se houver;
9. melhorias além do escopo mínimo, se houver;
10. decisão sobre Dear ImGui/ImGui-SFML;
11. decisão sobre fallback de UI, se houver;
12. como a Dívida 4 foi resolvida/mitigada;
13. como ficou `SimulationRunner`;
14. como ficou `AppController`;
15. como ficou `InputRouter`;
16. como ficou CommandQueue/comandos;
17. como ficou a separação App/UI/Renderer/Engine;
18. como funciona menu superior;
19. como funciona toolbar;
20. como funciona play/pause;
21. como funciona stop/reset;
22. como funciona step manual, se implementado;
23. como funciona pan;
24. como funciona zoom;
25. como funciona fit world;
26. como funciona conversão tela↔mundo;
27. como funciona seleção unitária;
28. como funciona seleção retangular;
29. como funciona lasso ou por que foi adiado;
30. como funciona seleção múltipla;
31. como funciona `SelectionState`;
32. como funciona ferramenta de comida;
33. como funciona ferramenta de agente;
34. como funciona ferramenta de obstáculo/pincel;
35. como funciona ferramenta de apagar obstáculo;
36. como funciona ferramenta mover;
37. como funciona deletar/matar selecionados;
38. como funcionam atalhos;
39. como funcionam overlays;
40. como engine headless foi preservado;
41. como determinismo foi preservado;
42. parâmetros usados;
43. parâmetros pendentes;
44. itens cobertos de `FEATURE_INVENTORY.md`;
45. itens cobertos de `PARAMETER_INVENTORY.md`;
46. itens cobertos de `UI_INVENTORY.md`;
47. como comida instantânea da Fase 7 foi protegida;
48. como locomoção da Fase 8 foi protegida;
49. como visão das Fases 10 a 12 foi protegida;
50. como reprodução/genoma da Fase 13 foi protegida;
51. como redes neurais das Fases 14 a 16 foram protegidas;
52. como SpeciesStore/GenomeStore da Fase 17 foram protegidos;
53. como dieta/predação da Fase 18 foram protegidas;
54. como comida chunk da Fase 19 foi protegida;
55. como obstáculos/oclusão da Fase 20 foram protegidos;
56. como colisões/física da Fase 21 foram protegidas;
57. como isso prepara a Fase 23;
58. como isso prepara a Fase 24;
59. como isso prepara a Fase 25;
60. como isso prepara a Fase 26;
61. como isso prepara a Fase 27;
62. como isso prepara a Fase 29;
63. como isso prepara a Fase 30;
64. resultado do build Debug;
65. resultado do build Release;
66. resultado do `--phase22-selftest`;
67. resultado do `--phase22-diagnostics`;
68. resultado das regressões das Fases 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 e 21;
69. custo headless;
70. custo janela SFML sem UI;
71. custo janela SFML com UI base;
72. custo menu/toolbar;
73. custo overlays desligados;
74. custo overlays ligados;
75. custo seleção unitária;
76. custo seleção retangular;
77. custo lasso, se implementado;
78. custo pan/zoom;
79. custo ferramentas de canvas;
80. diagnóstico visual/runtime;
81. divergências contra Python;
82. pendências para Fase 23;
83. pendências para Fase 24;
84. pendências para Fase 25;
85. pendências para Fase 26;
86. pendências para Fase 27;
87. pendências para Fase 28;
88. pendências para Fase 29;
89. pendências para Fase 30;
90. confirmação de que nenhum Python foi alterado;
91. confirmação de que não implementou preferências completas;
92. confirmação de que não implementou editor genético completo;
93. confirmação de que não implementou visualizador neural completo;
94. confirmação de que não implementou save/load final;
95. confirmação de que não implementou benchmark runner formal;
96. confirmação de que não avançou para Fase 23;
97. decisão: pronto para commit da Fase 22 ou precisa correção.

Se tudo estiver certo, pode fazer o commit da Fase 22.