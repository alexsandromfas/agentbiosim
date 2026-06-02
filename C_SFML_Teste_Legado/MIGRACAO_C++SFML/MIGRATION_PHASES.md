# Fases Propostas da Migracao

Cada fase deve ser pequena, testavel e revisavel. Nao migrar tudo de uma vez.

Estado atual oficial:

- Fases 0 a 25 implementadas e comitadas (Fase 24 inclui 24.1/24.2; Fase 25 migrou toda a UI para Dear ImGui e resolveu as Dividas 8 e 9). Tidy-up pendente da Fase 25: remover fisicamente o codigo de view SFML morto (ver `PHASE_25_IMGUI_UI_MIGRATION_STATUS.md`).
- A proxima fase de implementacao sera a Fase 26 (Agente Selecionado e Visualizador Neural), somente depois de autorizacao explicita do usuario.
- As Fases 0 a 25 nao devem ser renumeradas nem reabertas como trabalho pendente.
- Qualquer correcao em fase ja concluida deve ser tratada como bugfix ou microfase documentada (padrao 22.1, 23.1, 23.2, 24.1, 24.2).
- Cada fase deve citar explicitamente quais itens de `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md` e `UI_INVENTORY.md` cobre.
- Cada fase a partir da 25 tem um arquivo de prompt dedicado `PHASE_NN.md` na mesma pasta. Para executar uma fase, basta dizer "bora para a Fase NN" e seguir o `PHASE_NN.md` correspondente.

## Nota de re-sequenciamento (2026-06-01)

As fases 25 a 31 foram re-sequenciadas para refletir duas decisoes de arquitetura tomadas
apos a Fase 24:

1. **A UI precisa ser refundada antes de receber mais features.** Toda a camada de UI foi
   construida a mao em SFML imediato (UiPanel, UiPreferencesPanel, UiLeftDock), com geometria
   de hit-test duplicada entre desenho e clique. Isso e divida tecnica (ver Divida 9 em
   `TECHNICAL_DEBT_REGISTER.md`) e e a area mais fragil do projeto. Antes de empilhar visualizador
   neural, save/load e paridade, a UI inteira sera migrada para Dear ImGui (nova Fase 25). Junto
   vai a correcao da inversao de camada `sim -> ui` (Divida 8). Regra: corrigir a fundacao antes
   de construir em cima dela.
2. **O usuario quer um painel de performance dentro do app**, mostrando o custo de cada sistema
   (render, redes neurais, visao, fisica, percepcao...) como um teste headless visivel na propria
   UI. Isso virou a nova Fase 30, posicionada depois do profiler (Fase 27) e do benchmark runner
   (Fase 29), que sao a base de dados que ela consome.

Mapeamento (antigo -> novo):

| Tema | Numero antigo | Numero novo |
|---|---|---|
| Migracao Total da UI para Dear ImGui | (nao existia) | **25 (nova)** |
| Agente Selecionado e Visualizador Neural | 25 | 26 |
| Metricas, Inteligencia, Logs, Diagnostico e Profiler | 26 | 27 |
| Save/Load/Export/Import/Autosave | 27 | 28 |
| Benchmark Runner e Experimentos Headless Formais | 28 | 29 |
| Janela do Desenvolvedor (performance/profiling in-app) | (nao existia) | **30 (nova)** |
| Paridade Completa de UI | 29 | 31 |
| Otimizacao Data-Oriented e Escala | 30 | 32 |
| Campanha Final de Paridade + Prova de Performance | 31 | 33 |

Regra de usabilidade que vale para todas as fases de UI (25, 26, 30, 31): a **usabilidade,
as funcionalidades e os parametros** vem do programa Python (disposicao de botoes, fluxo de
menus, abas, o que cada controle faz). A **arquitetura de codigo NAO** vem do Python — vem da
arquitetura nova C++ (engine headless, stores data-oriented/SoA, UI desacoplada por comandos).
Parametros internos relevantes que nao existiam na UI Python mas que a arquitetura nova expoe
podem ser adicionados, desde que colocados no lugar logico/adequado seguindo o esqueleto de UI
ja existente, sempre com rotulo amigavel em PT-BR.

## Fase 0: Auditoria e Documentacao

Status: concluida.

- Objetivo: entender Python atual, inventariar parametros, UI, features, riscos e benchmarks.
- Entregaveis: arquivos Markdown de migracao.
- Criterio de conclusao: documentacao criada e revisada.
- Codigo: nenhum.

## Fase 1: Esqueleto C++/SFML/CMake

Status: concluida.

- Objetivo: criar projeto minimo que abre janela SFML e compila Debug/Release.
- Criterio: build limpo, executavel abre e fecha, sem simulacao ainda.
- Benchmark: tempo de startup e FPS vazio.

## Fase 2: Parametros/Configuracao

Status: concluida.

- Objetivo: criar schema de parametros e carregar defaults equivalentes ao Python.
- Criterio: dump dos defaults baseado em `PARAMETER_INVENTORY.md`.
- Benchmark: custo de cadastro/dump, quando aplicavel.

## Fase 3: Mundo/Camera/Tempo Fixo

Status: concluida.

- Objetivo: mundo retangular/circular, camera, pan/zoom e dt fixo.
- Criterio: camera e bounds funcionam; loop usa tempo fixo.
- Benchmark: loop vazio/headless simples.

## Fase 4: Entidades Basicas

Status: concluida.

- Objetivo: `AgentStore` e `FoodStore` com posicao, raio, energia e cor.
- Criterio: spawn deterministico por seed.
- Benchmark: criar/remover entidades.

## Fase 5: Renderizacao Simples

Status: concluida.

- Objetivo: render SFML de agentes/comida/substrato.
- Criterio: agentes/comida/mundo aparecem e camera continua funcional.
- Benchmark: observacao simples de FPS/render.

## Fase 6: Spatial Hash

Status: concluida.

- Objetivo: grid espacial com queries por raio.
- Criterio: queries corretas contra brute force.
- Benchmark: rebuild/query.

## Fase 7: Comida/Energia/Interacao

Status: concluida.

- Objetivo: comida instantanea, energia, morte por energia e interacao basica.
- Criterio: agente come comida instantanea, energia muda e morte ocorre.
- Benchmark: interacao com/sem spatial hash.

## Fase 8: Locomocao

Status: concluida.

- Objetivo: movimento forward/omni, giro, inercia/arrasto suave opcional.
- Criterio: outputs sinteticos movem agentes corretamente.
- Benchmark: movement update por quantidade de agentes.

## Fase 9: MLP Inicial com Arquitetura Neural Extensivel

- Objetivo: implementar a MLP baseline e a fundacao neural que nao bloqueie redes futuras.
- Escopo:
  - `BrainType` ou equivalente.
  - `BrainConfig`, `BrainState`, `BrainHandle` e `BrainFactory`.
  - `BrainExecutor` para MLP inicial.
  - Pesos, bias, forward, mutacao, clone/copy.
  - `ActivationTrace` minimo para uso futuro pelo visualizador neural.
  - Output temporario conectado ao `MovementSystem` da Fase 8.
- Fora de escopo:
  - Sensores reais.
  - Gated/Shortcut/Modulated MLP.
  - RNN.
  - NEAT.
  - Reproducao.
  - UI completa.
- Referencias:
  - `sim/brain.py`
  - `sim/actuators.py`
  - `sim/random_utils.py`
  - `CURRENT_MODULE_MAP.md`
  - `PARAMETER_INVENTORY.md`
  - `PROPOSED_CPP_ARCHITECTURE.md`
  - `PLANNING_COVERAGE_AUDIT.md`
- Criterios de conclusao:
  - MLP forward deterministico.
  - Mutacao com taxa zero nao altera pesos.
  - Mutacao com taxa maior que zero altera pesos.
  - Output size respeita modo de locomocao.
  - MLP nao fica hardcoded no `App`, `Renderer`, `MovementSystem` ou `AgentStore`.
- Testes minimos:
  - Arquitetura correta por hidden layers.
  - Forward com input fixo.
  - Clone/copy preserva pesos.
  - Output alimenta locomocao sem crash.
- Benchmark/microbenchmark:
  - Forward individual.
  - Forward para 100, 300, 600 e 1000 agentes.
  - MLP pequena, media e default do organismo base.
- Cobre:
  - MLP padrao.
  - Base arquitetural para redes avancadas.
  - Parametros `neural_network_type`, hidden layers, mutation rate/strength.

## Fase 10: Sensores, Canais de Retina e Visao Single

Nota tecnica: a Fase 10 nao e bloqueada por nenhuma divida tecnica pendente. Ver `TECHNICAL_DEBT_REGISTER.md` para a lista completa.

- Objetivo: ligar input neural real da retina single ao cerebro e locomocao.
- Escopo:
  - `RetinaConfig` e `SceneQuery` basico.
  - Calculo de `input_size` por retinas, olhos e canais.
  - Canais D/R/G/B e modos ponderados.
  - Integracao visao -> MLP -> MovementSystem.
- Fora de escopo:
  - Fullbody/raycast estrito.
  - Sector/bins.
  - Obstaculos/oclusao completa.
- Referencias: `sim/sensors.py`, `sim/entities.py`, `sim/brain.py`, `PARAMETER_INVENTORY.md`.
- Criterios de conclusao:
  - Input neural real substitui input sintetico.
  - Tamanho de entrada bate com canais/retinas/olhos.
  - Cores/distancia normalizadas em cenarios controlados.
- Testes minimos:
  - Uma comida em frente ativa retina esperada.
  - Canais desligados nao entram no vetor.
  - D dedicado e modos ponderados calculam input correto.
- Benchmark:
  - Custo por agente/retina/canal.
- Cobre:
  - Sensores/visao single.
  - Canais de retina.
  - Integracao inicial visao -> cerebro -> locomocao.

## Fase 11: Visao Fullbody/Raycast e Debug Visual de Visao

- Objetivo: preservar o modo fullbody/raycast atual e a visualizacao de raios.
- Escopo:
  - Estrategia `RaycastVision`/`FullbodyVision`.
  - Filtros see_food/see_agents/see_predators/see_obstacles preparados.
  - Debug data para agente selecionado.
  - Controle de visao multi-selecao como pendencia de UI, sem custo quando desligado.
- Fora de escopo:
  - Sector/bins.
  - Oclusao final por obstaculos se `ObstacleStore` ainda nao existir.
- Referencias: `sim/sensors.py`, `sim/render.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Raycast/fullbody retorna distancia/cor coerentes.
  - Debug visual nao roda quando desligado.
- Testes minimos:
  - Objeto no raio correto.
  - Objeto fora do FOV ignorado.
  - Dois olhos preservam direcoes.
- Benchmark:
  - Single vs fullbody/raycast.
  - Retina counts 4, 8, 18, 32, 64.
- Cobre:
  - Visao fullbody/raycast.
  - Visualizacao de visao do agente selecionado.

## Fase 12: Visao Sector/Bins Otimizada

- Objetivo: implementar visao por setores/bins otimizada.
- Escopo:
  - `retina_bins_mode`.
  - `retina_bins_distance_subdivisions`.
  - `retina_bins_distance_distribution`.
  - `retina_bins_distance_falloff`.
  - `retina_bins_projection`.
  - `retina_bins_candidate_limit`.
  - High-scale auto sector.
  - Visual debug de setores e subdivisoes.
- Fora de escopo:
  - Oclusao final por obstaculos se a Fase 20 ainda nao existir.
- Referencias: `sim/sensors.py`, `sim/fast_kernels.py`, `PARAMETER_INVENTORY.md`, `BENCHMARK_PLAN.md`.
- Criterios de conclusao:
  - Subdivisoes de distancia alteram intensidade, nao tamanho de input.
  - Output neural mantem dimensao equivalente as retinas configuradas.
  - Custo so existe quando modo sector esta ativo.
- Testes minimos:
  - Objetos em setores conhecidos.
  - Subdivisoes 1, 5, 20 e 99.
  - Projection center/edges/apparent size.
- Benchmark:
  - Sector vs raycast.
  - Impacto de candidate limit, subdivisoes e canais.
- Cobre:
  - Visao sector/bins.
  - Performance de percepcao em alta escala.

## Fase 13: Reproducao, Mutacao Base e Genoma

Prerequisito tecnico: antes de iniciar esta fase, resolver a Divida 1 (helpers de parametros duplicados) de `TECHNICAL_DEBT_REGISTER.md`. A adicao de `ReproductionSystem` e `GenomeStore` adicionaria mais copias dos helpers se nao forem extraidos primeiro.

- Objetivo: implementar reproducao por energia, genoma inicial e mutacao base.
- Escopo:
  - Split energy.
  - Idade minima e cooldown.
  - Heranca e mutacao de pesos MLP.
  - `GenomeStore` inicial.
  - Population min rescue preparado para especies.
- Fora de escopo:
  - Redes avancadas.
  - UI completa de especies.
- Referencias: `sim/systems.py`, `sim/entities.py`, `sim/brain.py`.
- Criterios de conclusao:
  - Populacao cresce com comida suficiente.
  - Filho herda genoma e pesos com mutacao controlada.
- Testes minimos:
  - Energia dividida.
  - Cooldown respeitado.
  - Mutacao reprodutivel por seed.
- Benchmark:
  - Custo de nascimento/remocao.
- Cobre:
  - Evolucao, reproducao e mutacao MLP base.

## Fase 14: Redes Densas Avancadas

Prerequisitos tecnicos de `TECHNICAL_DEBT_REGISTER.md`:
- Resolver Divida 2: trocar `BrainSlot` de `unique_ptr<MLPBrain>` para variant/polimorfismo extensivel.
- Resolver Divida 3: renomear campos Numba (`useNumbaBrainForward`, `numbaBrainForwardMinBatch`) para conceitos C++ genericos de batch.
- Preparar executor batch por assinatura neural e fallback individual para NEAT.

- Objetivo: implementar Gated MLP, Shortcut MLP e Modulated MLP.
- Escopo:
  - `GatedNeuralNet`.
  - `ShortcutNeuralNet`.
  - `ModulatedNeuralNet`.
  - Parametros de gates/shortcut.
  - Mutacoes especificas.
  - Batch por assinatura.
- Fora de escopo:
  - RNN.
  - NEAT.
- Referencias: `sim/brain.py`, `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Cada tipo cria, executa, muta e clona corretamente.
  - MLP baseline nao perde performance quando tipos avancados estao desligados.
- Testes minimos:
  - Gate clamp min/max.
  - Shortcut scale.
  - Modulated = gates + shortcut.
- Benchmark:
  - MLP vs Gated vs Shortcut vs Modulated.
- Cobre:
  - Gated MLP.
  - Shortcut MLP.
  - Modulated MLP.

## Fase 15: RNN Simples

- Objetivo: implementar `SimpleRNNBrain` com estado recorrente por agente.
- Escopo:
  - Recurrent weights.
  - `memory_decay`.
  - `state_clip`.
  - `reset_state_on_copy`.
  - ActivationTrace simplificado para RNN.
- Fora de escopo:
  - NEAT recorrente.
- Referencias: `sim/brain.py`, `sim/neural_viewer.py`.
- Criterios de conclusao:
  - Estado recorrente persiste entre steps.
  - Estado reseta conforme configuracao no clone/reproducao.
- Testes minimos:
  - Decay 0, 0.6, 0.9.
  - Clip de estado.
  - Reset em filho.
- Benchmark:
  - RNN vs MLP em 100, 300, 600, 1000 agentes.
- Cobre:
  - Simple RNN.
  - Memoria curta neural.

## Fase 16: Familia NEAT

- Objetivo: implementar NEAT comum, NEAT simplificada e NEAT recorrente.
- Escopo:
  - Nodes/conexoes.
  - Add/remove/toggle connection.
  - Add node.
  - Reset weight.
  - Limites de nodes/conexoes.
  - Executor individual e agrupamento por topologia quando possivel.
  - Estado recorrente para NEAT recorrente.
- Fora de escopo:
  - Otimizacao profunda de NEAT.
- Referencias: `sim/brain.py`, `PARAMETER_INVENTORY.md`, `BENCHMARK_PLAN.md`.
- Criterios de conclusao:
  - Tres tipos NEAT executam e mutam.
  - Desligar NEAT nao adiciona custo ao caminho MLP.
- Testes minimos:
  - Mutacoes estruturais respeitam limites.
  - Conexoes desabilitadas nao contribuem.
  - Recurrent NEAT mantem estado.
- Benchmark:
  - NEAT comum vs simplificada vs recorrente.
  - NEAT individual vs grupos por assinatura.
- Cobre:
  - NEAT comum.
  - NEAT simplificada.
  - NEAT recorrente.

## Fase 17: Especies, Labels e Genomas

- Objetivo: migrar `Bacteria`, `Predator` e `Labels` para especies/genomas preservando aliases.
- Escopo:
  - `SpeciesStore`.
  - `GenomeStore` completo.
  - Nome, cor, min, max, inicial, grafico.
  - Genoma associado por especie.
  - Resetar rede neural por especie.
- Fora de escopo:
  - UI completa.
  - Save/load final.
- Referencias: `sim/ui.py`, `sim/engine.py`, `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Especies default equivalem a bacteria/predator.
  - Labels antigos continuam mapeaveis.
- Testes minimos:
  - Reset por especie.
  - Limites populacionais.
  - Cor e graph flag preservados.
- Benchmark:
  - Custo de agrupamento por especie e assinatura neural.
- Cobre:
  - Labels/grupos/especies.
  - Metadados de genoma.

## Fase 18: Predacao e Dieta Generica

- Objetivo: implementar predacao como dieta de especie.
- Escopo:
  - `diet_food`.
  - `diet_agents`.
  - `diet_same_label/species`.
  - `diet_food_efficiency`.
  - `diet_agent_efficiency`.
  - Corpse-to-food inicial.
- Fora de escopo:
  - UI final de especies.
- Referencias: `sim/systems.py`, `sim/entities.py`.
- Criterios de conclusao:
  - Predador come presa e ganha energia.
  - Presa/predador coexistem por especie/dieta.
- Testes minimos:
  - Predacao.
  - Canibalismo permitido/bloqueado.
  - Eficiencia de energia.
- Benchmark:
  - Cenarios com predadores.
- Cobre:
  - Predadores.
  - Dieta generica.

## Fase 19: Comida Chunk/Pedacos Completa

- Objetivo: preservar dinamica de comida por pedacos.
- Escopo:
  - `food_bite_seconds`.
  - `food_piece_particle_radius`.
  - `food_piece_cluster_radius`.
  - `food_piece_particle_spacing`.
  - `food_piece_replenish_mode`.
  - `spawn_cluster`, `grow_existing`, `grow_particles`.
  - `Limpar Comida`.
  - Trim de excesso.
- Fora de escopo:
  - Fisica avancada de chunks se depender da Fase 21.
- Referencias: `sim/controllers.py`, `sim/systems.py`, `sim/engine.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Chunk e consumido aos poucos.
  - Reposicao respeita modo.
  - Target/trim funcionam.
- Testes minimos:
  - Consumo parcial.
  - Reposicao por cluster.
  - Clear food.
- Benchmark:
  - Instant vs chunk.
  - Modos de reposicao.
- Cobre:
  - Comida chunk/pedacos.

## Fase 20: Obstaculos e Oclusao

- Objetivo: implementar obstaculos desenhaveis, bloqueio e oclusao.
- Escopo:
  - `ObstacleStore`/`ObstacleMap`.
  - Pincel e apagar obstaculo.
  - Bloqueio de movimento.
  - Bloqueio de spawn/comida.
  - Obstaculos visiveis por sensores.
  - Ver atraves das paredes/oclusao.
- Fora de escopo:
  - UI final polida.
- Referencias: `sim/obstacles.py`, `sim/sensors.py`, `sim/render.py`, `sim/game.py`.
- Criterios de conclusao:
  - Agente nao atravessa obstaculo quando colisao ativa.
  - Comida nao nasce em obstaculo.
  - Visao bloqueia ou atravessa conforme parametro.
- Testes minimos:
  - Obstaculo entre agente e comida.
  - Pincel e apagar.
  - Spawn blocking.
- Benchmark:
  - Visao com/sem oclusao.
- Cobre:
  - Obstaculos.
  - Oclusao.

## Fase 21: Colisoes e Fisica Opcional

- Objetivo: completar fisica opcional e colisoes.
- Escopo:
  - Agente-agente.
  - Elasticidade.
  - Transferencia de velocidade.
  - Separacao e impulso maximo.
  - Viscosidade global.
  - Brownian motion.
  - Comida chunk movel/colisao/adesao reavaliada.
- Fora de escopo:
  - Comportamento nao documentado.
- Referencias: `sim/systems.py`, `sim/spatial.py`, `PARAMETER_INVENTORY.md`.
- Criterios de conclusao:
  - Dois corpos nao ocupam mesmo espaco em cenario controlado.
  - Fisica desligada retorna ao custo basico.
- Testes minimos:
  - Separacao agente-agente.
  - Impulso max.
  - Food chunk collision on/off.
- Benchmark:
  - Fisica on/off.
- Cobre:
  - Colisao/interacao fisica.

## Fase 22: UI Base, Menus e Canvas

Prerequisito tecnico: antes ou durante esta fase, resolver a Divida 4 de `TECHNICAL_DEBT_REGISTER.md` — fatorar `App` em `SimulationRunner`, `AppController` e `InputRouter` para que a UI nao fique acoplada a orquestracao do engine.

- Objetivo: criar a base de UI tecnica e ferramentas de viewport.
- Escopo:
  - Janela SFML + Dear ImGui, se mantido.
  - Menu superior basico.
  - Toolbar.
  - Play/pause/stop/reset.
  - Pan/zoom/fit.
  - Selecao unica, lasso e retangular.
  - Atalhos essenciais.
- Fora de escopo:
  - Editor completo.
  - Save/load completo.
- Referencias: `sim/ui.py`, `sim/game.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Fluxos basicos operam sem depender de Python.
  - Engine continua headless.
- Testes minimos:
  - Atalhos e ferramentas de selecao.
  - Custo UI ligada/desligada.
- Benchmark:
  - UI off vs on.
- Cobre:
  - UI inicial.
  - Menus/toolbar/canvas basicos.

## Fase 23: UI de Parametros e Preferencias

- Objetivo: expor parametros pelo schema.
- Escopo:
  - Preferencias de simulacao.
  - Sistema de visao.
  - Redes neurais.
  - Autosave.
  - Aparencia.
  - Performance/novas mudancas.
- Fora de escopo:
  - Persistencia final de saves.
  - Editor genetico completo.
- Referencias: `PARAMETER_INVENTORY.md`, `UI_INVENTORY.md`, `sim/ui.py`.
- Criterios de conclusao:
  - Cada parametro relevante tem controle, descricao e validacao.
  - Parametro de comportamento nao fica apenas cadastrado: aponta para fase funcional.
- Testes minimos:
  - Alterar parametro e confirmar efeito ou pendencia documentada.
- Benchmark:
  - Custo das janelas abertas.
- Cobre:
  - Preferencias.
  - Parametros de visao/neural/fisica/render.

## Fase 24: Editor Genetico, Especies e Substrato

Status: concluida (Fase 24 + microfases 24.1 e 24.2, 2026-05-31). Entregou o painel lateral
esquerdo fixo com 3 abas (Editor Genetico, Substrato, Labels) reproduzindo a usabilidade do
Python, ainda em SFML imediato (a refundacao em Dear ImGui e a Fase 25).

- Objetivo: reconstruir paineis operacionais principais.
- Escopo:
  - Editor de genoma.
  - Importar/exportar genoma como UI, usando backend quando existir.
  - Especies/labels.
  - Populacao.
  - Substrato/comida.
  - Aplicar aos selecionados/especie.
  - Pipeta/coletar genoma, se confirmada ativa.
- Fora de escopo:
  - Neural viewer completo.
  - Save/load final.
- Referencias: `UI_INVENTORY.md`, `sim/ui.py`.
- Criterios de conclusao:
  - Fluxos principais de configuracao funcionam.
  - Itens legados sao classificados antes de remocao.
- Testes minimos:
  - Aplicar genoma.
  - Atribuir especie.
  - Limpar comida.
- Benchmark:
  - Painel aberto/fechado.
- Cobre:
  - Editor genetico.
  - Labels/especies UI.
  - Substrato UI.

## Fase 25: Migracao Total da UI para Dear ImGui

Prompt dedicado: `PHASE_25.md`.

Prerequisito tecnico obrigatorio: resolver a Divida 8 de `TECHNICAL_DEBT_REGISTER.md` — mover
`ui::Command` (e a CommandQueue) para uma camada neutra para que `sim::SimulationRunner` deixe de
depender de `ui::`. A seta de dependencia deve apontar so UI -> engine. Como esta fase reescreve
toda a fronteira UI/engine, este e o momento certo de corrigir a inversao de camada antes de
construir a nova UI sobre ela.

- Objetivo: substituir 100% da UI feita a mao em SFML imediato (UiPanel, UiPreferencesPanel,
  UiLeftDock e qualquer hit-test manual) por uma UI Dear ImGui (via ImGui-SFML), de nivel
  especialista em UI/UX, elegante, moderna e coerente, preservando a arquitetura (engine headless,
  stores SoA, UI desacoplada por comandos) e toda a usabilidade/funcionalidade ja entregue.
- Escopo:
  - Integrar Dear ImGui + ImGui-SFML no CMake (Debug/Release), sem acoplar ao engine.
  - Resolver Divida 8 (Command em camada neutra).
  - Reimplementar em ImGui: menu superior (Arquivo/Exibir/Preferencias/Agente/Ajuda), toolbar de
    ferramentas de canvas, painel lateral esquerdo (Editor Genetico, Substrato, Labels), janela de
    Preferencias (todos os grupos da Fase 23), overlays de ajuda/seleccao.
  - Sistema de tema/estilo proprio (cores, espacamento, raio de borda, tipografia, icones da pasta
    Assets), aplicando hierarquia visual e principios de Gestalt.
  - Manter todos os comandos, parametros, combos, color pickers, sliders, edicao por texto, labels
    PT-BR, e o roteamento de eventos (a UI consome o mouse antes do canvas via `WantCaptureMouse`).
- Fora de escopo:
  - Visualizador neural completo (Fase 26), metricas/profiler (Fase 27), save/load (Fase 28),
    janela do desenvolvedor (Fase 30), paridade final exaustiva (Fase 31).
- Referencias: `sim/ui.py` (usabilidade/disposicao), `UI_INVENTORY.md`, `PARAMETER_INVENTORY.md`,
  `PROPOSED_CPP_ARCHITECTURE.md`, status das Fases 22, 22.1, 23, 23.1, 23.2, 24.
- Criterios de conclusao:
  - Nenhum widget critico depende mais de hit-test manual; Divida 9 resolvida.
  - Engine continua headless; `sim/` nao inclui mais `ui/`.
  - Paridade de usabilidade com o que existia antes (nada de funcionalidade perdida).
- Testes minimos:
  - `--phase25-selftest`: comandos disparados pela UI continuam chegando ao runner; engine sem ImGui.
  - Smoke: app abre, todos os paineis/menus renderizam, clique no painel nao pinta no canvas.
- Benchmark:
  - Custo de UI ImGui on/off vs baseline SFML manual.
- Cobre:
  - Refundacao da camada de UI.
  - Divida 8 e Divida 9.

## Fase 26: Agente Selecionado e Visualizador Neural

Prompt dedicado: `PHASE_26.md`.

- Objetivo: conectar inspector e visualizador neural, ja em Dear ImGui.
- Escopo:
  - Painel retratil do agente selecionado.
  - Abas Genoma e Rede Neural.
  - ActivationTrace.
  - MLP, RNN simplificada e NEAT apropriada.
  - Layout fixed/preencher painel.
  - Visao selecionada sem custo quando oculta.
- Fora de escopo:
  - Paridade completa de todos os menus.
- Referencias: `sim/neural_viewer.py`, `sim/ui.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Painel oculto nao calcula dados caros.
  - Visualizador muda por tipo neural.
- Testes minimos:
  - Selecionar agente.
  - Ver MLP.
  - Ver RNN simplificada.
  - Ver NEAT sem quebrar MLP.
- Benchmark:
  - Viewer on/off.
- Cobre:
  - Agente selecionado.
  - Visualizador neural.

## Fase 27: Metricas, Inteligencia, Logs, Diagnostico e Profiler

Prompt dedicado: `PHASE_27.md`.

- Objetivo: preservar observabilidade da simulacao e criar o profiler por sistema que a Fase 30
  (Janela do Desenvolvedor) vai consumir.
- Escopo:
  - MetricsSystem.
  - Graficos.
  - Inteligencia local/global/grupo.
  - Logger.
  - Heartbeat.
  - Crash/recovery hooks.
  - Scoped profiler com instrumentacao por sistema (perception, neural, movement, collision,
    energy, interaction, food, reproduction, death, spatialhash, render).
- Fora de escopo:
  - Benchmark runner formal (Fase 29).
  - Janela do desenvolvedor (Fase 30).
- Referencias: `sim/intelligence.py`, `sim/profiler.py`, `sim/diagnostics.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Metricas batem em cenarios controlados.
  - Profiler mede por sistema com overhead baixo e desligavel.
- Testes minimos:
  - Series por especie.
  - Profiler on/off.
  - Log sem custo alto quando desligado.
- Benchmark:
  - Overhead metrics/profiler.
- Cobre:
  - Graficos/metricas.
  - Logs/diagnostico/profiling.

## Fase 28: Save/Load/Export/Import/Autosave

Prompt dedicado: `PHASE_28.md`.

- Objetivo: persistencia versionada e compatibilidade.
- Escopo:
  - `.biosim`.
  - Export/import substrato JSON.
  - Export/import genoma/agente legado.
  - Autosave.
  - Recovery on close.
  - RNG state.
  - Camera.
  - Labels/species.
  - Cerebros MLP/Gated/Shortcut/Modulated/RNN/NEAT.
- Fora de escopo:
  - Novas features fora do Python.
- Referencias: `sim/ui.py`, `sim/random_utils.py`, `PARAMETER_INVENTORY.md`.
- Criterios de conclusao:
  - Roundtrip preserva estado.
  - Loader aceita aliases antigos.
  - Genomas por tipo neural salvam/carregam.
- Testes minimos:
  - Salvar/abrir `.biosim`.
  - Export/import genoma.
  - Autosave e recovery.
- Benchmark:
  - Save/load grande.
- Cobre:
  - Save/load.
  - Export/import.
  - Autosave.
  - Compatibilidade de saves/genomas.

## Fase 29: Benchmark Runner e Experimentos Headless Formais

Prompt dedicado: `PHASE_29.md`.

- Objetivo: criar infraestrutura formal de benchmark — base de dados da Fase 30 (Janela do
  Desenvolvedor) e da prova de performance da Fase 33.
- Escopo:
  - CLI headless.
  - Scenarios.
  - CSV/JSON/Markdown reports.
  - Commit/build metadata.
  - Percentuais por sistema (reusa o profiler da Fase 27).
- Fora de escopo:
  - Otimizacao sem relatorio.
- Referencias: `BENCHMARK_PLAN.md`, `sim/profiler.py`.
- Criterios de conclusao:
  - Suite minima roda com seed fixa.
  - Resultados sao reprodutiveis.
- Testes minimos:
  - Repeticoes com media/min/max/desvio.
  - Headless sem janela.
- Benchmark:
  - Plano minimo formal.
- Cobre:
  - Execucao headless.
  - Benchmarks formais.

## Fase 30: Janela do Desenvolvedor — Performance e Profiling In-App

Prompt dedicado: `PHASE_30.md`.

Posicionada apos a Fase 27 (profiler por sistema) e a Fase 29 (benchmark runner) porque consome
ambos como fonte de dados. Surface visual da Divida 10 (performance prometida ainda nao
comprovada ponta a ponta).

- Objetivo: uma janela de desenvolvedor (Dear ImGui) que mostra, em tempo real e de forma facil de
  ler, o custo de cada parte do sistema (render, redes neurais, visao/percepcao, fisica/colisao,
  energia, interacao, comida, reproducao, morte, spatial hash, UI), como um teste headless visivel
  dentro da propria UI — para identificar o que consome mais sem precisar rodar benchmark externo.
- Escopo:
  - Overlay/janela com graficos de tempo por sistema (us/step e % do frame), historico (sparklines),
    FPS, us/step, contagem de agentes/comida/obstaculos, memoria aproximada.
  - Botao para rodar um cenario de benchmark embutido (reusando a Fase 29) e ver o resultado na UI.
  - Toggles para ligar/desligar sistemas e medir o delta de custo.
  - Tudo desligavel e com custo proximo de zero quando oculto.
- Fora de escopo:
  - Otimizacao em si (Fase 32) — esta fase so MEDE e MOSTRA.
- Referencias: `sim/profiler.py`, `BENCHMARK_PLAN.md`, status da Fase 27 e Fase 29.
- Criterios de conclusao:
  - Soma dos tempos por sistema bate com o tempo total medido (dentro de margem).
  - Janela oculta nao adiciona custo relevante.
- Testes minimos:
  - `--phase30-selftest`: numeros do profiler expostos batem com os do benchmark headless.
- Benchmark:
  - Overhead da janela aberta vs fechada.
- Cobre:
  - Observabilidade de performance dentro do app.
  - Surface da Divida 10.

## Fase 31: Paridade Completa de UI

Prompt dedicado: `PHASE_31.md`.

- Objetivo: fechar checklist de `UI_INVENTORY.md` sobre a UI ImGui.
- Escopo:
  - Menus.
  - Abas.
  - Botoes.
  - Sliders/spinboxes/checkboxes/combos.
  - Atalhos.
  - Ferramentas de canvas.
  - Fluxos criticos.
- Fora de escopo:
  - Novas features nao existentes (exceto as ja justificadas em fases anteriores).
- Referencias: `UI_INVENTORY.md`, `FEATURE_INVENTORY.md`.
- Criterios de conclusao:
  - Checklist revisado item a item.
  - Itens legados classificados como preservados, redesenhados ou removidos com autorizacao.
- Testes minimos:
  - Revisao manual guiada.
  - Smoke de cada janela/painel.
- Benchmark:
  - UI completa com simulacao.
- Cobre:
  - Paridade completa de UI.

## Fase 32: Otimizacao Data-Oriented e Escala

Prompt dedicado: `PHASE_32.md`.

Prerequisito tecnico: resolver a Divida 5 de `TECHNICAL_DEBT_REGISTER.md` — substituir alocacoes
temporarias de `std::vector<double>` por agente no `NeuralSystem` por buffers persistentes
pre-alocados por assinatura neural. Se a Fase 30 (Janela do Desenvolvedor) ou os benchmarks das
Fases 12/14/29 ja mostrarem gargalo, ataca-lo primeiro, guiado pelos numeros.

- Objetivo: otimizar gargalos medidos sem remover comportamento. Atacar a Divida 10 (prova de
  performance) com base nos dados da Fase 30 e Fase 29.
- Escopo:
  - Buffers persistentes.
  - Batch por assinatura.
  - Alocacao zero no loop quente.
  - Threading seletivo.
  - SIMD quando medido.
  - Cache neural por grupo.
- Fora de escopo:
  - Otimizar sem benchmark.
- Referencias: `BENCHMARK_PLAN.md`, `PROPOSED_CPP_ARCHITECTURE.md`, status da Fase 29 e Fase 30.
- Criterios de conclusao:
  - Ganho medido antes/depois (mostrado tambem na Janela do Desenvolvedor).
  - Sem regressao funcional.
- Testes minimos:
  - Regressao por seed.
  - Comparacao de metricas.
- Benchmark:
  - 600, 1000, 2000, 5000+ agentes.
- Cobre:
  - Performance/otimizacoes.

## Fase 33: Campanha Final de Paridade + Prova de Performance Python vs C++

Prompt dedicado: `PHASE_33.md`.

- Objetivo: validar a migracao completa contra o Python e PROVAR a promessa de performance da
  arquitetura (resolver definitivamente a Divida 10): a versao C++ deve ser comprovadamente mais
  rapida que a Python em escala (1000+, 2000+, 5000+ agentes), com numeros reproduziveis.
- Escopo:
  - Paridade por seed.
  - Saves/genomas reais.
  - UI completa.
  - Benchmarks headless/renderizados, C++ vs Python lado a lado.
  - Relatorio final de performance (tabelas, speedup por cenario, gargalos restantes).
- Fora de escopo:
  - Implementar novas features.
- Referencias: `BENCHMARK_PLAN.md`, `PLANNING_COVERAGE_AUDIT.md`, todos os inventarios.
- Criterios de conclusao:
  - Nenhum item inventariado sem decisao.
  - Diferencas conhecidas documentadas.
  - Performance C++ comprovada em Release contra o Python, com relatorio (Divida 10 fechada).
- Testes minimos:
  - Suite final por seed.
  - Roundtrip de saves.
  - Checklist UI.
  - Comparativo de tempo/step C++ vs Python por cenario.
- Benchmark:
  - Plano completo de `BENCHMARK_PLAN.md` + comparativo Python vs C++.
- Cobre:
  - Paridade completa.
  - Prova de performance ponta a ponta.
  - Gate de migracao funcional.

## Regra de Avanco

Nao avancar para a fase seguinte sem:

- autorizacao explicita do usuario;
- criterio de conclusao cumprido;
- benchmark minimo quando aplicavel;
- revisao de escopo contra `PLANNING_COVERAGE_AUDIT.md`;
- confirmacao de que os itens de inventario cobertos pela fase foram citados;
- commit separado para a fase, quando solicitado.
