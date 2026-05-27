# Fases Propostas da Migracao

Cada fase deve ser pequena, testavel e revisavel. Nao migrar tudo de uma vez.

Estado atual oficial:

- Fase 0 concluida.
- Fases 1 a 8 implementadas e comitadas.
- A proxima fase de implementacao sera a Fase 9, somente depois de autorizacao explicita do usuario.
- As Fases 0 a 8 nao devem ser renumeradas nem reabertas como trabalho pendente.
- Qualquer correcao em fase ja concluida deve ser tratada como bugfix ou fase futura documentada.
- A partir da Fase 9, cada fase deve citar explicitamente quais itens de `FEATURE_INVENTORY.md`, `PARAMETER_INVENTORY.md` e `UI_INVENTORY.md` cobre.

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

## Fase 25: Agente Selecionado e Visualizador Neural

- Objetivo: conectar inspector e visualizador neural.
- Escopo:
  - Painel retratil.
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

## Fase 26: Metricas, Inteligencia, Logs, Diagnostico e Profiler

- Objetivo: preservar observabilidade da simulacao.
- Escopo:
  - MetricsSystem.
  - Graficos.
  - Inteligencia local/global/grupo.
  - Logger.
  - Heartbeat.
  - Crash/recovery hooks.
  - Scoped profiler.
- Fora de escopo:
  - Benchmark runner formal.
- Referencias: `sim/intelligence.py`, `sim/profiler.py`, `sim/diagnostics.py`, `UI_INVENTORY.md`.
- Criterios de conclusao:
  - Metricas batem em cenarios controlados.
  - Profiler mede por sistema.
- Testes minimos:
  - Series por especie.
  - Profiler on/off.
  - Log sem custo alto quando desligado.
- Benchmark:
  - Overhead metrics/profiler.
- Cobre:
  - Graficos/metricas.
  - Logs/diagnostico/profiling.

## Fase 27: Save/Load/Export/Import/Autosave

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

## Fase 28: Benchmark Runner e Experimentos Headless Formais

- Objetivo: criar infraestrutura formal de benchmark.
- Escopo:
  - CLI headless.
  - Scenarios.
  - CSV/JSON/Markdown reports.
  - Commit/build metadata.
  - Percentuais por sistema.
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

## Fase 29: Paridade Completa de UI

- Objetivo: fechar checklist de `UI_INVENTORY.md`.
- Escopo:
  - Menus.
  - Abas.
  - Botoes.
  - Sliders/spinboxes/checkboxes/combos.
  - Atalhos.
  - Ferramentas de canvas.
  - Fluxos criticos.
- Fora de escopo:
  - Novas features nao existentes.
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

## Fase 30: Otimizacao Data-Oriented e Escala

- Objetivo: otimizar gargalos medidos sem remover comportamento.
- Escopo:
  - Buffers persistentes.
  - Batch por assinatura.
  - Alocacao zero no loop quente.
  - Threading seletivo.
  - SIMD quando medido.
  - Cache neural por grupo.
- Fora de escopo:
  - Otimizar sem benchmark.
- Referencias: `BENCHMARK_PLAN.md`, `PROPOSED_CPP_ARCHITECTURE.md`.
- Criterios de conclusao:
  - Ganho medido antes/depois.
  - Sem regressao funcional.
- Testes minimos:
  - Regressao por seed.
  - Comparacao de metricas.
- Benchmark:
  - 600, 1000, 2000, 5000+ agentes.
- Cobre:
  - Performance/otimizacoes.

## Fase 31: Campanha Final de Paridade Python vs C++

- Objetivo: validar a migracao completa contra o Python.
- Escopo:
  - Paridade por seed.
  - Saves/genomas reais.
  - UI completa.
  - Benchmarks headless/renderizados.
  - Relatorio final.
- Fora de escopo:
  - Implementar novas features.
- Referencias: `BENCHMARK_PLAN.md`, `PLANNING_COVERAGE_AUDIT.md`, todos os inventarios.
- Criterios de conclusao:
  - Nenhum item inventariado sem decisao.
  - Diferencas conhecidas documentadas.
  - Performance C++ comprovada em Release.
- Testes minimos:
  - Suite final por seed.
  - Roundtrip de saves.
  - Checklist UI.
- Benchmark:
  - Plano completo de `BENCHMARK_PLAN.md`.
- Cobre:
  - Paridade completa.
  - Gate de migracao funcional.

## Regra de Avanco

Nao avancar para a fase seguinte sem:

- autorizacao explicita do usuario;
- criterio de conclusao cumprido;
- benchmark minimo quando aplicavel;
- revisao de escopo contra `PLANNING_COVERAGE_AUDIT.md`;
- confirmacao de que os itens de inventario cobertos pela fase foram citados;
- commit separado para a fase, quando solicitado.
