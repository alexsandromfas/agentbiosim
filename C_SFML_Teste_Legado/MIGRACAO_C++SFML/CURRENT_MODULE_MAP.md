# Mapa dos Modulos Python Atuais

Este arquivo mapeia os principais arquivos Python encontrados no projeto atual. O Python e a referencia funcional. Historicos, prototipos e copias arquivadas podem informar contexto, mas nao devem comandar a arquitetura C++.

## Resumo Geral

| Arquivo | Responsabilidade atual | Destino C++ sugerido | Tratamento |
|---|---|---|---|
| `main.py` | Inicializacao da aplicacao | entrada principal futura em `app/` | Redesenhar |
| `sim/controllers.py` | Parametros, validacao, controladores de populacao/comida | `core/config`, `sim/systems/FoodSystem`, `sim/systems/PopulationSystem` | Preservar valores, redesenhar estrutura |
| `sim/engine.py` | Loop, estado, comandos, sistemas, metricas | `sim/SimulationEngine`, `sim/WorldState` | Redesenhar |
| `sim/entities.py` | Classes de entidades, agentes, comida, factories | `sim/stores`, `sim/components`, `sim/factories` | Substituir por SoA/stores |
| `sim/sensors.py` | Retina, raycast, sector vision, SceneQuery | `sim/perception` | Preservar comportamento, redesenhar performance |
| `sim/brain.py` | Redes neurais, mutacao, serializacao, batch/cache | `sim/neural`, `sim/genome` | Preservar tipos, redesenhar executores |
| `sim/actuators.py` | Locomocao e energia | `sim/systems/MovementSystem`, `EnergySystem` | Preservar modelo, redesenhar batch |
| `sim/systems.py` | Interacao, reproducao, morte, colisao | `sim/systems/*` | Preservar regras, redesenhar dados |
| `sim/spatial.py` | Spatial hash e colisao auxiliar | `sim/spatial/SpatialGrid` | Preservar conceito, reimplementar |
| `sim/render.py` | Render Pygame e overlays | `render/SfmlRenderer`, `render/Overlays` | Substituir por SFML |
| `sim/game.py` | PygameView, input e loop visual | `app/Viewport`, `app/InputRouter` | Substituir por SFML |
| `sim/ui.py` | UI PyQt6, menus, persistencia, ferramentas | `ui/*` com Dear ImGui ou outra UI | Redesenhar, preservar paridade |
| `sim/neural_viewer.py` | Visualizador neural PyQt | `ui/NeuralViewer` | Redesenhar em UI C++ |
| `sim/obstacles.py` | Mapa/stamps de obstaculos | `sim/obstacles/ObstacleMap` | Preservar conceito |
| `sim/world.py` | Mundo e camera | `sim/World`, `render/Camera2D` | Preservar conceito |
| `sim/fast_kernels.py` | Numba/Numpy hot kernels | `sim/kernels` C++ nativo/SIMD/threading | Substituir por C++ |
| `sim/soa.py` | Ponte parcial para arrays | `sim/stores/AgentStore` | Expandir como arquitetura central |
| `sim/profiler.py` | Profiler por secao | `core/profiling` | Preservar |
| `sim/diagnostics.py` | Logs, excecoes, Qt handler | `core/logging`, `core/diagnostics` | Preservar |
| `sim/intelligence.py` | Smart factors | `sim/metrics/IntelligenceMetrics` | Preservar com validacao |
| `sim/random_utils.py` | Seed/RNG state | `core/random` | Preservar reprodutibilidade |

## Detalhe por Modulo

### `controllers.py`

- Responsabilidade atual: `Params`, validacao, callbacks, perfis, `PopulationController`, `FoodController`.
- Principais classes/funcoes: `Params`, `PopulationController`, `FoodController`.
- Dependencias relevantes: `math`, `copy`, entidades em uso indireto pela engine.
- Problemas de acoplamento: parametros de simulacao, UI, render, neural, fisica, comida, labels e debug vivem no mesmo dicionario.
- Riscos de migracao: perder defaults, ranges, aliases historicos e validacoes.
- Destino C++: `ConfigRegistry`, `ParameterSchema`, `SimulationConfig`, `GenomeConfig`, `RenderConfig`, `DebugConfig`, `FoodSystem`, `PopulationSystem`.
- Tratamento: preservar todos os nomes como aliases de compatibilidade; redesenhar categorias.

### `engine.py`

- Responsabilidade atual: loop de simulacao, acumulador de tempo, comandos thread-safe, entidades, spatial hash, sistemas, render flags, metricas, selecao, labels, autosave hooks.
- Principais classes/funcoes: `Engine`, `_truthy`.
- Dependencias relevantes: `entities`, `systems`, `spatial`, `world`, `sensors`, `brain`, `random_utils`, `profiler`.
- Problemas de acoplamento: engine conhece detalhes de entidades, UI, labels, camera, comida em pedacos, persistencia e parametros.
- Riscos de migracao: alterar ordem de update e mudar comportamento evolutivo.
- Destino C++: `SimulationEngine`, `WorldState`, `CommandQueue`, `SystemScheduler`.
- Tratamento: redesenhar mantendo ordem de sistemas documentada.

### `entities.py`

- Responsabilidade atual: `Entity`, `Food`, `Agent`, `Bacteria`, `Predator`, factories e update em lote.
- Principais classes/funcoes: `Entity`, `Food`, `Agent`, `Bacteria`, `Predator`, `update_agents_batch`, `create_random_bacteria`, `create_random_predator`, `create_random_food`.
- Dependencias relevantes: `brain`, `sensors`, `actuators`, `world`, `fast_kernels`.
- Problemas de acoplamento: factories leem `Params`, criam sensor/cerebro/energia/locomocao, definem cores e dietas.
- Riscos de migracao: copiar hierarquia `Bacteria/Predator` pode bloquear futuro "organismo generico por dieta".
- Destino C++: `AgentStore`, `FoodStore`, `SpeciesStore`, `GenomeFactory`, `EntitySpawnSystem`.
- Tratamento: substituir por stores; preservar compatibilidade de bacteria/predator como especies iniciais ou aliases.

### `sensors.py`

- Responsabilidade atual: retina, canais, visao por raycast/fullbody/sector, `SceneQuery`, batch sensing.
- Principais classes/funcoes: `RetinaSensor`, `SceneQuery`, `batch_retina_sense`, `retina_input_size`, `normalize_retina_input_mode`, `_sector_retina_from_candidates`.
- Dependencias relevantes: `numpy`, `spatial`, `fast_kernels`, entidades.
- Problemas de acoplamento: sensor mistura normalizacao de parametros, query de cena e algoritmo de percepcao.
- Riscos de migracao: principal gargalo; pequenas mudancas alteram comportamento aprendido.
- Destino C++: `PerceptionSystem`, `RetinaConfig`, `VisionRaycast`, `VisionSectorBins`, `SceneQuery`.
- Tratamento: preservar outputs numericos por modo; redesenhar em batch por assinatura.

### `brain.py`

- Responsabilidade atual: tipos de cerebro, mutacao, forward, ativacoes, serializacao, cache multi-brain.
- Principais classes/funcoes: `NeuralNet`, `GatedNeuralNet`, `ShortcutNeuralNet`, `ModulatedNeuralNet`, `SimpleRNNBrain`, `NEATGraphBrain`, `create_brain`, `brain_to_data`, `brain_from_data`, `forward_many_brains`.
- Dependencias relevantes: `numpy`, `fast_kernels`, parametros.
- Problemas de acoplamento: um arquivo concentra tipos densos, recorrentes, NEAT, serializacao e cache.
- Riscos de migracao: topologias heterogeneas quebram batch se nao houver agrupamento.
- Destino C++: `BrainType`, `BrainStore`, `DenseBrainExecutor`, `RnnExecutor`, `NeatExecutor`, `GenomeSerializer`.
- Tratamento: preservar tipos, dividir executor/estado/genoma/serializacao.

### `actuators.py`

- Responsabilidade atual: locomocao, modos de movimento, formato do corpo e modelo energetico.
- Principais classes/funcoes: `Locomotion`, `EnergyModel`, `locomotion_output_size`, `normalize_movement_mode`, `normalize_body_shape`.
- Dependencias relevantes: `math`, parametros.
- Problemas de acoplamento: locomocao e energia ainda vivem como objetos em cada agente.
- Riscos de migracao: saida da rede depende do modo de movimento; mudar tamanho quebra cerebros.
- Destino C++: `MovementSystem`, `EnergySystem`, `LocomotionConfig`.
- Tratamento: preservar formulas; executar em lote por modo.

### `systems.py`

- Responsabilidade atual: interacao/comida/predacao, reproducao, morte, colisao.
- Principais classes: `InteractionSystem`, `ReproductionSystem`, `DeathSystem`, `CollisionSystem`.
- Dependencias relevantes: entidades, spatial hash, parametros.
- Problemas de acoplamento: sistemas operam sobre listas de objetos e consultam parametros globais.
- Riscos de migracao: regras de min/max population, rescue e labels podem ser perdidas.
- Destino C++: `InteractionSystem`, `ReproductionSystem`, `DeathSystem`, `CollisionSystem`, `FoodConsumptionSystem`.
- Tratamento: preservar regras; trocar listas por indices e stores.

### `spatial.py`

- Responsabilidade atual: spatial hash, queries por raio, colisao auxiliar e clamp de velocidade.
- Principais classes/funcoes: `SpatialHash`, `resolve_collision`, `clamp_speed`.
- Dependencias relevantes: objetos com `x`, `y`, `r`.
- Problemas de acoplamento: grid depende de objetos dinamicos; precisao/cell size precisa de benchmark.
- Riscos de migracao: ruim cell size pode destruir performance ou visao.
- Destino C++: `SpatialGrid` com arrays de indices e buckets contiguos.
- Tratamento: reimplementar e medir.

### `render.py`

- Responsabilidade atual: render de substrato, entidades, detalhes, visao, comida em pedacos, interpolacao.
- Principais classes/funcoes: `RendererStrategy`, `SimpleRenderer`, `EllipseRenderer`, `_draw_food_bite_holes`.
- Dependencias relevantes: Pygame, camera, entidades.
- Problemas de acoplamento: render conhece muitos detalhes internos dos objetos.
- Riscos de migracao: perda de feedback visual e ferramentas de analise.
- Destino C++: `SfmlRenderer`, `OverlayRenderer`, `VisionDebugRenderer`.
- Tratamento: substituir Pygame por SFML mantendo overlays.

### `game.py`

- Responsabilidade atual: `PygameView`, input de mouse/teclado, loop visual, integracao Qt/Pygame.
- Principais classes/funcoes: `PygameView`, `bootstrap_pygame_simulation`.
- Dependencias relevantes: Pygame, engine, renderer, world/camera.
- Problemas de acoplamento: input, render e comandos no mesmo objeto.
- Riscos de migracao: camera, pan, zoom e ferramentas podem divergir.
- Destino C++: `Viewport`, `InputRouter`, `CanvasToolController`.
- Tratamento: substituir.

### `ui.py`

- Responsabilidade atual: UI principal PyQt6, menus, abas, janelas, graficos, save/load, import/export, autosave, labels, editor genetico, painel de agente, shortcuts.
- Principais classes/funcoes: `SimulationUI`, `MetricHistoryChart`, `ChartResizeHandle`, `ClickHelpLabel`, `run_ui`.
- Dependencias relevantes: PyQt6, engine, pygame view, parametros, sensores, cerebros, entidades.
- Problemas de acoplamento: arquivo muito grande e com UI nova/legada coexistindo.
- Riscos de migracao: alto risco de perder menus, botoes e parametros.
- Destino C++: `ui/MainWindow`, `ui/MenuBar`, `ui/GenomePanel`, `ui/SpeciesPanel`, `ui/SubstratePanel`, `ui/PreferencesWindows`, `ui/Charts`, `ui/AgentInspector`.
- Tratamento: redesenhar por paineis, com inventario completo.

### `neural_viewer.py`

- Responsabilidade atual: visualizador grafico de rede neural do agente selecionado.
- Principais classes/funcoes: `AgentNeuralNetworkView`, `NeuralViewConfig`, `NeuronItem`, `WeightItem`, `ChannelSlot`.
- Dependencias relevantes: PyQt6 graphics view, sensores/brain data.
- Problemas de acoplamento: visualizador depende de estruturas Python e Qt.
- Riscos de migracao: perder ferramenta diagnostica importante.
- Destino C++: `ui/NeuralViewer`.
- Tratamento: redesenhar visualmente, preservar informacoes.

### `obstacles.py`

- Responsabilidade atual: obstaculos desenhados, stamps e consultas de colisao/bloqueio.
- Principais classes: `ObstacleStamp`, `ObstacleMap`.
- Dependencias relevantes: geometria 2D.
- Problemas de acoplamento: precisa conversar com comida, spawn, agentes e visao.
- Riscos de migracao: obstaculos bloquearem movimento mas nao visao, ou vice versa.
- Destino C++: `ObstacleMap`, `ObstacleCollisionSystem`, `VisionOcclusion`.
- Tratamento: preservar.

### `world.py`

- Responsabilidade atual: mundo retangular/circular e camera.
- Principais classes: `World`, `Camera`.
- Dependencias relevantes: engine/render.
- Problemas de acoplamento: camera usada por render e selecao; mundo usado por spawn, bordas, comida e visao.
- Riscos de migracao: bug de borda circular/retangular impactar alimentacao e visao.
- Destino C++: `WorldBounds`, `Camera2D`.
- Tratamento: preservar conceito e criar testes de borda.

### `fast_kernels.py`

- Responsabilidade atual: kernels Numba/Numpy para locomocao/energia, retinas e forward neural.
- Principais funcoes: `apply_locomotion_energy_arrays`, `retina_single_kernel`, `retina_fullbody_kernel`, `retina_batch_*`, `brain_layer_forward_kernel`.
- Dependencias relevantes: Numpy, Numba opcional.
- Problemas de acoplamento: duplicacao parcial de logica do Python; precisa fallback.
- Riscos de migracao: C++ deve substituir esses kernels, nao portar Numba literalmente.
- Destino C++: kernels nativos, possivelmente SIMD/thread pool.
- Tratamento: substituir.

### `soa.py`

- Responsabilidade atual: `AgentArrays` e conversao simples de agentes para arrays.
- Principais classes/funcoes: `AgentArrays`, `agents_to_arrays`.
- Dependencias relevantes: Numpy e objetos Agent.
- Problemas de acoplamento: snapshot temporario, nao store persistente.
- Riscos de migracao: achar que SoA ja esta resolvido quando ainda e parcial.
- Destino C++: `AgentStore`.
- Tratamento: expandir como fundamento.

### `profiler.py`

- Responsabilidade atual: profiler por secao, contexto `profile_section`, estatisticas.
- Principais classes/funcoes: `Profiler`, `SectionStats`, `set_enabled`, `profile_section`.
- Destino C++: `Profiler`, `ScopedTimer`.
- Tratamento: preservar desde a fase inicial.

### `diagnostics.py`

- Responsabilidade atual: log de eventos, excecoes, diagnosticos runtime e handler Qt.
- Principais funcoes: `log_event`, `log_exception`, `setup_runtime_diagnostics`, `install_qt_message_handler`.
- Destino C++: `Logger`, `CrashDiagnostics`.
- Tratamento: preservar.

### `intelligence.py`

- Responsabilidade atual: metricas de inteligencia local/global/grupo.
- Principais funcoes: `intelligence_for_agent`, `group_intelligence_snapshot`, `opportunity_group_intelligence_value`.
- Problemas de acoplamento: depende de acesso ao engine e pools de recursos.
- Destino C++: `MetricsSystem`, `IntelligenceMetrics`.
- Tratamento: preservar como metrica experimental, medir custo.

### `random_utils.py`

- Responsabilidade atual: seed, captura/restauracao de estado RNG.
- Principais funcoes: `normalize_seed`, `apply_global_seed`, `capture_rng_state`, `restore_rng_state`.
- Destino C++: `RandomService`.
- Tratamento: preservar reprodutibilidade.

## Arquivos Historicos/Legados Identificados

- `C_SFML_Teste_Legado/`: contem prototipos e builds de testes C++/SFML. Deve ser tratado como contexto secundario, nao como fonte principal da migracao.
- `C_SFML_Teste_Legado/MIGRACAO_C++SFML/AgentBioSim_Cpp_Architecture_Guide.txt`: arquivo historico removido intencionalmente pelo usuario. Nao deve ser restaurado nem usado como fonte.
- Abas e secoes legadas dentro de `ui.py`: foram encontrados trechos de UI antiga e nova no mesmo arquivo (`Labels` duplicado, abas `Bacterias`, `Predadores`, `Teste`, `Simulacao`). Precisa de verificacao manual na aplicacao antes de declarar qualquer uma como removivel.

## Nota de Terminologia

- `Labels` no codigo atual deve ser tratado como predecessor operacional de `Especies`.
- `Bacteria` e `Predator` devem continuar mapeaveis a especies default para compatibilidade.
- `Agente` no menu atual deve ser relacionado a `Genoma` apenas quando o fluxo de import/export estiver preservado.
