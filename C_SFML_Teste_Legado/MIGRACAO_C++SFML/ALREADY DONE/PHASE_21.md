Você agora vai executar a Fase 21 da migração C++/SFML/CMake do AgentBioSim:

# Fase 21 — Colisões e Física Opcional

Atue como arquiteto sênior de software C++, engenheiro de performance e especialista em simulações evolutivas 2D com agentes, colisões, física leve opcional, comida em pedaços, viscosidade, ruído browniano, spatial hash, stores data-oriented e preservação de comportamento científico.

Esta fase é crítica porque fecha a camada física opcional que ficou pendente após:

* comida instantânea;
* comida chunk/pedaços;
* predadores/dieta genérica;
* espécies/labels/genomas;
* visão single;
* visão fullbody/raycast;
* visão sector/bins;
* obstáculos e oclusão;
* bloqueio de movimento por obstáculos;
* bloqueio de spawn por obstáculos.

A Fase 21 deve completar, de forma controlada e opcional:

* colisão agente-agente;
* separação de corpos;
* elasticidade leve;
* transferência de velocidade;
* limite de impulso;
* viscosidade global;
* Brownian motion;
* comida chunk móvel;
* colisão de comida chunk;
* adesão/cohesão de comida chunk;
* empurrão de comida por agentes;
* física ligada/desligada com custo quase zero quando desligada;
* diagnostics e benchmark física on/off;
* regressões das Fases 7 a 20.

O objetivo não é transformar a simulação em um motor físico complexo.

O objetivo é preservar a física opcional do Python, com arquitetura C++ limpa, headless, determinística, testável e data-oriented.

Até a Fase 20, o projeto já deve ter:

* `AgentStore`;
* `FoodStore`;
* `SpeciesStore`;
* `GenomeStore`;
* `ObstacleStore` ou `ObstacleMap`;
* `SpatialHash`;
* `MovementSystem`;
* `EnergySystem`;
* `InteractionSystem`;
* `ReproductionSystem`;
* `DeathSystem`;
* `PerceptionSystem`;
* `Renderer` SFML básico;
* comida instantânea;
* comida chunk/pedaços;
* predation/dieta genérica;
* obstáculos e oclusão;
* spawn blocking por obstáculos;
* bloqueio de movimento por obstáculos;
* selftests e diagnostics das Fases 7 a 20.

Agora a Fase 21 deve implementar ou finalizar:

* `CollisionSystem`;
* colisão agente-agente;
* colisão agente-comida chunk móvel;
* colisão comida-comida quando habilitada;
* elasticidade opcional;
* transferência de velocidade opcional;
* separação controlada;
* impulso máximo;
* viscosidade global;
* Brownian motion;
* mobilidade de comida chunk;
* drag de comida chunk;
* massa efetiva de comida chunk;
* adesão/cohesão de partículas de comida chunk;
* empurrão de comida por agentes;
* benchmark física on/off;
* documentação da Fase 21.

O prompt abaixo define o escopo mínimo obrigatório, mas ele não é limitante.

Se durante a implementação você perceber algum gap vindo das Fases 7 a 20, alguma conexão incompleta entre `AgentStore` → `FoodStore` → `ObstacleStore` → `SpatialHash` → `MovementSystem` → `InteractionSystem` → `ReproductionSystem` → `CollisionSystem` → `Renderer`, algum parâmetro cadastrado mas não usado, algum problema de seed/determinismo, algum acoplamento indevido ou alguma melhoria pequena necessária para que a Fase 21 fique correta, você está autorizado a corrigir, desde que:

1. a correção esteja dentro da migração C++/SFML;
2. nenhum arquivo Python seja alterado;
3. a correção seja documentada claramente;
4. você explique por que ela foi necessária;
5. você não avance indevidamente para a Fase 22;
6. você não implemente UI base;
7. você não implemente Dear ImGui;
8. você não implemente editor genético;
9. você não implemente painel de parâmetros;
10. você não implemente save/load;
11. você não implemente export/import;
12. você não implemente autosave;
13. você não implemente métricas/graficos completos da Fase 26;
14. você não implemente benchmark runner formal da Fase 28;
15. você não implemente otimização data-oriented profunda da Fase 30;
16. você não faça refatoração grande sem necessidade real;
17. você não esconda decisões arquiteturais importantes.

A intenção é evitar gaps entre fases. As fases são incrementais, mas o sistema precisa funcionar como um todo.

## Estado esperado antes de iniciar

Antes de qualquer alteração, confirme:

* Fases 0 a 8 concluídas e comitadas.
* Fase 9 concluída e comitada.
* Fase 10 concluída e comitada.
* Fase 11 concluída e comitada.
* Fase 12 concluída e comitada.
* Fase 13 concluída e comitada.
* Fase 14 concluída e comitada.
* Fase 15 concluída e comitada.
* Fase 16 concluída e comitada.
* Fase 17 concluída e comitada.
* Fase 18 concluída e comitada.
* Fase 19 concluída e comitada.
* Fase 20 concluída e comitada.
* Working tree limpo.
* Nenhum arquivo Python alterado.

Rode primeiro:

```bash
git status
git log --oneline -10
```

Se o working tree não estiver limpo, pare e reporte.

## Pré-requisitos técnicos desta fase

Antes de implementar a Fase 21, confirme o estado das dívidas técnicas registradas em:

```text
C_SFML_Teste_Legado/MIGRACAO_C++SFML/TECHNICAL_DEBT_REGISTER.md
```

Verifique especialmente:

* Dívida 1 — helpers de parâmetros duplicados: deve estar resolvida na Fase 13.
* Dívida 2 — BrainSlot dependente de MLPBrain: deve estar resolvida na Fase 14.
* Dívida 3 — nomes Numba/Python no C++: deve estar resolvida na Fase 14.
* Dívida 5 — alocações temporárias no NeuralSystem: não precisa ser resolvida agora, mas não deve piorar.
* Dívida 7 — `ReproductionSystem::apply` com risco de dangling reference: deve estar resolvida na Fase 18.
* Dívida 4 — App acumulando responsabilidades: não precisa ser resolvida agora, pois UI base é Fase 22.
* Dívida 6 — Version.hpp desatualizado: pode ser corrigida se for pequeno, mas não é obrigatório.

A Fase 21 não deve criar uma arquitetura de física acoplada à UI.

A física deve estar no núcleo headless.

## Validação curta de continuidade antes da Fase 21

Antes de implementar a Fase 21, faça uma inspeção rápida para evitar construir em cima de fases anteriores quebradas.

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
13. se `AgentStore` tem posição, velocidade, raio, energia, idade, espécie e flags;
14. se `FoodStore` diferencia comida instantânea e chunk/pedaços;
15. se `FoodStore` já suporta raio, posição, energia e partículas/chunks;
16. se `FoodStore` já tem ou pode receber velocidade/massa/drag para chunks móveis;
17. se `ObstacleStore` bloqueia movimento/spawn/visão sem depender de UI;
18. se `SpatialHash` suporta queries por agentes e comida;
19. se `SpatialHash` pode ser usado para pares agente-agente;
20. se `SpatialHash` pode ser usado para pares agente-comida;
21. se `SpatialHash` pode ser usado para pares comida-comida;
22. se `MovementSystem` aplica posições/velocidades antes da etapa de colisão;
23. se `InteractionSystem` consome comida/predação antes ou depois da colisão de forma documentada;
24. se `ReproductionSystem` não cria filhos em colisão impossível;
25. se `DeathSystem` remove entidades de forma determinística;
26. se o loop principal tem local claro para chamar `CollisionSystem`;
27. se obstáculos da Fase 20 continuam bloqueando movimento;
28. se comida chunk da Fase 19 continua sendo consumida corretamente;
29. se predation da Fase 18 continua funcionando;
30. se nenhum arquivo Python foi alterado.

Se houver problema pequeno e diretamente relacionado à continuidade da Fase 21, corrija e documente em `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md`.

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
* `C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_20_OBSTACLES_OCCLUSION_STATUS.md`

Leia também os status das Fases 18 e 19 se precisar confirmar decisões de predation/dieta e comida chunk.

Se algum desses documentos não existir no repositório, registre isso no status da Fase 21.

## Arquivos Python obrigatórios como referência funcional

Consulte os arquivos Python relevantes como referência funcional, não como arquitetura a ser copiada literalmente:

* `sim/systems.py`
* `sim/spatial.py`
* `sim/entities.py`
* `sim/actuators.py`
* `sim/controllers.py`
* `sim/engine.py`
* `sim/render.py`
* `sim/world.py`
* `sim/obstacles.py`
* `sim/random_utils.py`
* `sim/fast_kernels.py`

Se precisar consultar outros arquivos Python, pode consultar. Registre em `PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md` quais arquivos adicionais foram consultados e por quê.

## Objetivo da Fase 21

Completar colisões e física opcional.

Fluxo esperado:

```text
ParameterRegistry / configs
  agent_collision_enabled
  agent_collision_elasticity_enabled
  agent_collision_restitution
  agent_collision_velocity_transfer
  agent_collision_separation
  agent_collision_max_impulse
  global_viscosity_enabled
  global_viscosity_drag
  brownian_motion_enabled
  brownian_motion_strength
  movable_chunk_food_enabled
  chunk_food_collision_enabled
  chunk_food_adhesion_enabled
  chunk_food_adhesion_strength
  chunk_food_mass_scale
  chunk_food_drag
  chunk_food_push_strength
↓
AgentStore / FoodStore / ObstacleStore
  posições
  raios
  velocidades
  massas efetivas
  flags
  tipos
↓
SpatialHash
  pares agente-agente
  pares agente-comida chunk
  pares comida-comida
  vizinhança local
↓
MovementSystem
  aplica movimento neural/suave
↓
CollisionSystem
  separação agente-agente
  elasticidade opcional
  transferência de velocidade opcional
  impulso máximo
  colisão agente-comida chunk móvel
  colisão comida-comida opcional
  adesão/cohesão chunk opcional
  viscosidade global opcional
  Brownian motion opcional
↓
InteractionSystem
  alimentação/predação preservadas
↓
ReproductionSystem / DeathSystem
  criação/remoção preservadas
↓
Renderer
  visualização preservada
↓
Diagnostics/Benchmark
  física on/off
  custo por sistema
  regressões
```

A Fase 21 deve provar que:

* física desligada preserva custo básico;
* colisão agente-agente impede sobreposição persistente;
* separação respeita impulso máximo;
* elasticidade é opcional;
* transferência de velocidade é opcional;
* viscosidade global é opcional;
* Brownian motion é opcional e determinístico por seed;
* comida chunk móvel funciona quando habilitada;
* comida chunk imóvel preserva comportamento da Fase 19;
* colisão comida-comida funciona quando habilitada;
* adesão/cohesão de comida chunk funciona quando habilitada;
* empurrão de comida por agentes funciona quando habilitado;
* obstáculos da Fase 20 continuam funcionando;
* predation da Fase 18 continua funcionando;
* comida chunk da Fase 19 continua funcionando;
* Fases 7 a 20 continuam passando;
* Fase 22 não foi iniciada.

## Escopo mínimo obrigatório

Implemente ou ajuste:

1. `CollisionSystem`;
2. `CollisionConfig`;
3. leitura funcional dos parâmetros de colisão/física;
4. colisão agente-agente;
5. separação por sobreposição;
6. impulso máximo;
7. elasticidade opcional;
8. transferência de velocidade opcional;
9. viscosidade global opcional;
10. Brownian motion opcional;
11. comida chunk móvel opcional;
12. velocidade de partículas/chunks de comida quando habilitada;
13. massa efetiva de comida chunk;
14. drag de comida chunk;
15. empurrão de comida por agentes;
16. colisão agente-comida chunk;
17. colisão comida-comida opcional;
18. adesão/cohesão de comida chunk opcional;
19. interação com obstáculos preservada;
20. interação com mundo retangular preservada;
21. interação com mundo circular preservada;
22. integração com SpatialHash;
23. integração com `AgentStore`;
24. integração com `FoodStore`;
25. integração com `MovementSystem`;
26. integração com `InteractionSystem`;
27. integração com `ReproductionSystem`;
28. integração com `DeathSystem`;
29. integração com Renderer sem acoplamento;
30. selftest da Fase 21;
31. diagnostics/microbenchmark da Fase 21;
32. regressões das Fases 7 a 20;
33. documentação da Fase 21.

## Escopo permitido adicional

Você pode fazer pequenas correções ou melhorias além do escopo mínimo se elas forem necessárias para evitar gaps entre fases.

Exemplos permitidos:

* ajustar `FoodStore` para guardar velocidade apenas para comida chunk;
* ajustar `FoodStore` para guardar massa efetiva;
* adicionar helpers geométricos de colisão;
* adicionar query de pares próximos no SpatialHash;
* adicionar contadores de colisão para diagnostics;
* corrigir clamp em mundo circular/retangular;
* corrigir comportamento de spawn/reprodução se colisão revelar sobreposição extrema;
* corrigir ordem do loop se `CollisionSystem` estiver no lugar errado;
* corrigir render básico de comida chunk móvel se necessário;
* atualizar `TECHNICAL_DEBT_REGISTER.md` se a física revelar nova dívida técnica real;
* adicionar testes pequenos que protejam integração entre Fases 18 a 21.

Essas melhorias devem ser pequenas, justificadas e documentadas.

## Fora de escopo

Não implemente nesta fase:

* Fase 22 UI base;
* Dear ImGui;
* toolbar;
* menus;
* seleção/lasso/retângulo;
* editor genético;
* janelas de preferências;
* painel de parâmetros;
* agente selecionado;
* visualizador neural;
* gráficos/métricas completos da Fase 26;
* save/load;
* export/import;
* autosave;
* benchmark runner formal da Fase 28;
* otimização data-oriented profunda da Fase 30;
* threading amplo;
* SIMD avançado;
* paralelização complexa;
* física rígida complexa;
* rotação física de corpos;
* torque real;
* colisão elipse-elipse perfeita se círculo aproximado for suficiente;
* solver físico iterativo pesado;
* comportamento não documentado no Python.

Não altere arquivos Python.

## Parâmetros obrigatórios

Usar funcionalmente, quando disponíveis no `ParameterRegistry`, os parâmetros de física:

```text
agent_collision_enabled
agent_collision_elasticity_enabled
agent_collision_restitution
agent_collision_velocity_transfer
agent_collision_separation
agent_collision_max_impulse
global_viscosity_enabled
global_viscosity_drag
movable_chunk_food_enabled
chunk_food_collision_enabled
chunk_food_adhesion_enabled
chunk_food_adhesion_strength
chunk_food_mass_scale
chunk_food_drag
chunk_food_push_strength
brownian_motion_enabled
brownian_motion_strength
```

Preservar compatibilidade com parâmetros de locomoção já implementados:

```text
agents_inertia
smooth_locomotion_enabled
smooth_linear_inertia_enabled
smooth_max_linear_accel
smooth_linear_drag_enabled
smooth_linear_drag
smooth_angular_inertia_enabled
smooth_max_angular_accel
smooth_angular_drag_enabled
smooth_angular_drag
```

Preservar compatibilidade com parâmetros de mundo/tempo:

```text
physics_steps_per_second
max_physics_steps_per_frame
max_physics_backlog_seconds
time_scale
substrate_shape
world_w
world_h
substrate_radius
random_seed
use_spatial
```

Preservar compatibilidade com comida chunk:

```text
food_mode
food_target
food_bite_seconds
food_piece_particle_radius
food_piece_cluster_radius
food_piece_particle_spacing
food_piece_replenish_mode
food_trim_excess_enabled
food_trim_max_per_step
```

Regras:

* se `agent_collision_enabled=false`, colisão agente-agente não deve executar;
* se `agent_collision_elasticity_enabled=false`, não aplicar bounce/elasticidade;
* se `agent_collision_velocity_transfer=0`, não transferir velocidade;
* se `global_viscosity_enabled=false`, não aplicar viscosidade;
* se `brownian_motion_enabled=false`, não aplicar ruído browniano;
* se `movable_chunk_food_enabled=false`, comida chunk deve preservar comportamento imóvel da Fase 19;
* se `chunk_food_collision_enabled=false`, não executar colisão comida-comida;
* se `chunk_food_adhesion_enabled=false`, não executar adesão/cohesão;
* se algum parâmetro ainda não existir no C++ atual, cadastre ou documente claramente;
* não ignore parâmetro silenciosamente;
* se algum parâmetro só será exposto na UI futura, documente.

## `CollisionSystem`

Criar ou finalizar `CollisionSystem`.

Requisitos:

* deve operar sobre stores, não sobre objetos polimórficos;
* deve ser chamado pelo loop de simulação, não pelo Renderer;
* deve funcionar em modo headless;
* deve aceitar `AgentStore`, `FoodStore`, `ObstacleStore` se necessário, `SpatialHash` e `WorldBounds`;
* deve respeitar dt fixo;
* deve ser determinístico;
* deve ter custo zero ou quase zero quando tudo estiver desligado;
* deve usar SpatialHash quando disponível;
* deve ter fallback seguro se `use_spatial=false`;
* deve não alterar genomas/cérebros;
* deve não alterar espécies;
* deve não executar percepção;
* deve não executar alimentação/predação;
* deve não executar reprodução;
* deve não executar morte;
* deve não depender de SFML;
* deve não depender de UI.

## Ordem do loop

A ordem recomendada, salvo motivo forte documentado, é:

```text
1. SpatialHash update/rebuild
2. PerceptionSystem
3. NeuralSystem
4. MovementSystem
5. Obstacle movement blocking, se ainda separado
6. CollisionSystem
7. EnergySystem
8. InteractionSystem
9. ReproductionSystem
10. DeathSystem
11. Metrics/debug/render snapshot
```

Se o projeto atual usa outra ordem por causa das fases anteriores, preserve ou ajuste com cuidado.

Documente a ordem final e a justificativa.

## Colisão agente-agente

Implementar separação agente-agente quando `agent_collision_enabled=true`.

Comportamento esperado:

* dois agentes sobrepostos devem ser separados;
* separação deve respeitar raio/tamanho corporal;
* aproximação circular é aceitável se elipse/círculo perfeito ainda não for necessário;
* separação deve ser simétrica quando massas equivalentes;
* separação deve respeitar espécies diferentes e iguais;
* predador/presa também são agentes e devem colidir se habilitado;
* colisão não deve impedir predation quando houver contato válido;
* colisão não deve criar energia;
* colisão não deve gerar NaN/Inf;
* colisão não deve lançar agente para fora do mundo;
* colisão deve respeitar obstáculos;
* colisão deve respeitar mundo retangular/circular.

## Separação e impulso máximo

Usar:

```text
agent_collision_separation
agent_collision_max_impulse
```

Requisitos:

* `agent_collision_separation` controla intensidade/fator de separação;
* `agent_collision_max_impulse` limita correção de posição/velocidade por step;
* sobreposição grande deve ser resolvida gradualmente ou com clamp seguro;
* não permitir explosão numérica;
* não permitir teleporte absurdo;
* não permitir velocidades infinitas;
* documentar unidade e interpretação prática dos parâmetros.

## Elasticidade

Usar:

```text
agent_collision_elasticity_enabled
agent_collision_restitution
```

Requisitos:

* quando desligada, apenas separar corpos;
* quando ligada, aplicar componente simples de bounce;
* restitution deve controlar intensidade de ricochete;
* restitution deve ser clampada para faixa segura;
* não criar energia excessiva;
* não quebrar smooth locomotion;
* não quebrar movimento forward/omni;
* não quebrar predation.

## Transferência de velocidade

Usar:

```text
agent_collision_velocity_transfer
```

Requisitos:

* controlar quanto da velocidade relativa é transferida;
* `0` deve equivaler a sem transferência;
* valores altos devem ser clampados para faixa segura;
* colisões não devem gerar velocidade infinita;
* colisões não devem parar todos os agentes indevidamente;
* documentar fórmula.

## Viscosidade global

Usar:

```text
global_viscosity_enabled
global_viscosity_drag
```

Requisitos:

* aplicar drag global adicional quando habilitado;
* não duplicar indevidamente drag já existente da locomoção suave;
* se coexistir com `smooth_linear_drag`, documentar a composição;
* desligada deve ter custo zero ou quase zero;
* deve afetar agentes;
* pode afetar comida chunk móvel, se coerente;
* não deve afetar obstáculos;
* não deve afetar genomas/cérebros.

## Brownian motion

Usar:

```text
brownian_motion_enabled
brownian_motion_strength
```

Requisitos:

* ruído pseudoaleatório determinístico por seed;
* desligado deve ter custo zero ou quase zero;
* deve afetar agentes;
* pode afetar comida chunk móvel, se coerente;
* não deve afetar obstáculos;
* não deve gerar NaN/Inf;
* não deve empurrar entidades para fora do mundo sem clamp;
* intensidade deve ser documentada;
* deve ser testável por seed;
* duas execuções com mesma seed devem gerar mesmo resultado.

## Comida chunk móvel

Usar:

```text
movable_chunk_food_enabled
chunk_food_mass_scale
chunk_food_drag
chunk_food_push_strength
```

Requisitos:

* quando desligada, comida chunk preserva comportamento imóvel da Fase 19;
* quando ligada, partículas/chunks podem ter velocidade;
* massa efetiva deve depender de raio/energia ou parâmetro documentado;
* drag deve amortecer velocidade;
* agentes podem empurrar comida chunk;
* obstáculos bloqueiam comida chunk, se implementado na Fase 20;
* mundo retangular/circular deve ser respeitado;
* comida chunk não deve fugir infinitamente;
* comida chunk não deve gerar NaN/Inf;
* consumo de comida chunk continua funcionando;
* reposição chunk continua funcionando;
* trim continua funcionando;
* clear food continua funcionando.

## Colisão agente-comida chunk

Requisitos:

* agentes podem empurrar comida chunk quando `movable_chunk_food_enabled=true`;
* `chunk_food_push_strength` controla empurrão;
* massa efetiva da comida deve moderar empurrão;
* agentes ainda podem consumir comida quando em contato;
* empurrão não deve impedir mordidas/consumo;
* predadores com dieta de comida, se houver, seguem mesma regra;
* comida instantânea não deve receber física móvel;
* partículas chunk não devem atravessar obstáculos sem política documentada.

## Colisão comida-comida

Usar:

```text
chunk_food_collision_enabled
```

Requisitos:

* quando desligada, não executar colisão comida-comida;
* quando ligada, partículas/chunks não devem se sobrepor de forma persistente;
* separação simples é suficiente;
* não aplicar solver pesado;
* respeitar massa/raio;
* respeitar drag;
* respeitar obstáculos/mundo;
* não quebrar reposição `spawn_cluster`, `grow_existing`, `grow_particles`;
* não quebrar trim;
* custo medido em diagnostics.

## Adesão/cohesão de comida chunk

Usar:

```text
chunk_food_adhesion_enabled
chunk_food_adhesion_strength
```

Requisitos:

* quando desligada, custo zero ou quase zero;
* quando ligada, partículas próximas do mesmo cluster podem ter atração/cohesão leve;
* não criar instabilidade;
* não colapsar todas as partículas em um ponto;
* não explodir velocidades;
* respeitar `chunk_food_adhesion_strength`;
* documentar aproximação;
* se clusters explícitos não existirem, usar política local simples e documentar.

## Obstáculos e colisão

Preservar Fase 20.

Requisitos:

* agentes continuam não atravessando obstáculos;
* comida chunk móvel não deve atravessar obstáculos, se a geometria permitir;
* colisão agente-agente não deve empurrar agente para dentro de obstáculo;
* separação deve tentar fallback seguro se agente fica preso entre obstáculos;
* spawn blocking da Fase 20 continua funcionando;
* oclusão da Fase 20 continua funcionando;
* não alterar lógica de visão por causa da física.

## Mundo retangular/circular

Requisitos:

* física respeita mundo retangular;
* física respeita mundo circular;
* colisão não deve colocar entidades fora do mundo;
* Brownian motion deve respeitar bounds;
* viscosidade não depende do formato do mundo;
* comida chunk móvel deve respeitar bounds;
* clamp/projeção deve ser determinística;
* comportamento deve ser testado em ambos.

## Integração com SpatialHash

Requisitos:

* usar SpatialHash para reduzir pares de colisão;
* agente-agente deve usar pares próximos;
* agente-comida chunk deve usar pares próximos;
* comida-comida deve usar pares próximos;
* evitar O(N²) quando `use_spatial=true`;
* se `use_spatial=false`, fallback brute force pode existir para testes;
* evitar pares duplicados;
* evitar auto-colisão;
* rebuild/update deve ocorrer na ordem correta;
* medir custo de rebuild/query;
* documentar se comida chunk usa grid próprio ou compartilhado.

## Integração com EnergySystem

Requisitos:

* colisão não deve alterar energia diretamente, salvo se já documentado no Python;
* viscosidade/Brownian não devem alterar energia diretamente;
* custo metabólico continua sendo responsabilidade de `EnergySystem`;
* movimento alterado por colisão não deve quebrar cálculo de custo;
* comida consumida continua adicionando energia via InteractionSystem;
* predation continua adicionando energia via InteractionSystem.

## Integração com InteractionSystem

Requisitos:

* alimentação continua funcionando com agentes e comida chunk;
* empurrão de comida não impede consumo;
* predation continua funcionando;
* colisão agente-agente não deve impedir contato predatório;
* se colisão separa predador/presa antes da interação, documentar ordem e validar que predation ainda ocorre quando apropriado;
* dietas continuam respeitadas.

## Integração com ReproductionSystem

Requisitos:

* filhos não devem nascer em colisão extrema;
* se o filho nascer sobre outro agente, CollisionSystem deve separar de forma segura;
* se o filho nascer perto de obstáculos, Fase 20 deve continuar protegendo;
* reprodução não deve depender de física ligada;
* mutação/genoma não devem ser alterados por colisão;
* espécies/labels não devem ser alteradas por colisão.

## Integração com DeathSystem

Requisitos:

* remoção por morte continua determinística;
* CollisionSystem não deve manter índices inválidos após DeathSystem;
* FoodStore/AgentStore continuam consistentes após swap-remove;
* SpatialHash deve ser atualizado no momento correto;
* não usar referências persistentes inválidas.

## Integração com Renderer

Requisitos:

* renderização deve refletir posições pós-física;
* Renderer não decide colisão;
* Renderer não decide viscosidade;
* Renderer não decide Brownian motion;
* Renderer não decide empurrão de comida;
* render desligado não executa custo de desenho;
* se comida chunk móvel tiver velocidade, render apenas desenha posição atual;
* debug visual opcional de colisões pode ser básico, mas não implementar painel UI.

## Determinismo

Com mesma seed, mesma configuração e mesma sequência de operações:

* colisão agente-agente deve ser determinística;
* ordem de pares deve ser determinística;
* separação deve ser determinística;
* Brownian motion deve ser determinístico;
* comida chunk móvel deve ser determinística;
* adesão/cohesão deve ser determinística;
* fallback de colisão deve ser determinístico;
* diagnostics deve ser reproduzível.

Não use `std::random_device` no hot path ou no caminho determinístico.

## Performance

A Fase 21 deve medir custo física on/off.

Requisitos:

* medir com todas as físicas desligadas;
* medir apenas agente-agente ligado;
* medir agente-agente + elasticidade;
* medir agente-agente + transferência de velocidade;
* medir viscosidade ligada/desligada;
* medir Brownian ligado/desligado;
* medir comida chunk móvel ligada/desligada;
* medir comida-comida ligada/desligada;
* medir adesão ligada/desligada;
* medir com obstáculos ligados/desligados;
* medir com predadores ligados/desligados;
* medir 100, 300, 600 e 1000 agentes;
* medir 100, 300, 600 e 1000 partículas/comidas quando aplicável;
* medir `use_spatial=true`;
* medir `use_spatial=false` em cenário pequeno;
* não otimizar prematuramente além do necessário;
* registrar gargalos observados.

## Testes obrigatórios

Criar comando:

```text
--phase21-selftest
```

Testar pelo menos:

### Configuração e parâmetros

1. `CollisionConfig` carrega defaults.
2. `agent_collision_enabled=false` desliga colisão agente-agente.
3. `agent_collision_enabled=true` liga colisão agente-agente.
4. `agent_collision_elasticity_enabled=false` remove bounce.
5. `agent_collision_elasticity_enabled=true` aplica bounce.
6. `agent_collision_restitution` é clampado para faixa segura.
7. `agent_collision_velocity_transfer=0` não transfere velocidade.
8. `agent_collision_separation` altera intensidade de separação.
9. `agent_collision_max_impulse` limita correção.
10. `global_viscosity_enabled=false` não aplica viscosidade.
11. `global_viscosity_enabled=true` aplica viscosidade.
12. `brownian_motion_enabled=false` não aplica ruído.
13. `brownian_motion_enabled=true` aplica ruído.
14. `movable_chunk_food_enabled=false` preserva comida imóvel.
15. `movable_chunk_food_enabled=true` permite comida móvel.
16. `chunk_food_collision_enabled=false` não colide comida-comida.
17. `chunk_food_collision_enabled=true` colide comida-comida.
18. `chunk_food_adhesion_enabled=false` desliga adesão.
19. `chunk_food_adhesion_enabled=true` liga adesão.
20. Nenhum parâmetro obrigatório é ignorado silenciosamente.

### CollisionSystem básico

21. `CollisionSystem` existe.
22. `CollisionSystem` não depende de SFML.
23. `CollisionSystem` não depende de UI.
24. `CollisionSystem` funciona headless.
25. `CollisionSystem` não altera genomas.
26. `CollisionSystem` não altera espécies.
27. `CollisionSystem` não executa percepção.
28. `CollisionSystem` não executa reprodução.
29. `CollisionSystem` não executa morte.
30. `CollisionSystem` não executa render.

### Colisão agente-agente

31. Dois agentes sem sobreposição não são movidos.
32. Dois agentes sobrepostos são separados.
33. Separação respeita raios.
34. Separação é simétrica para massas equivalentes.
35. Separação respeita `agent_collision_separation`.
36. Separação respeita `agent_collision_max_impulse`.
37. Sobreposição grande não gera NaN/Inf.
38. Sobreposição grande não gera teleporte absurdo.
39. Agentes de mesma espécie colidem quando habilitado.
40. Agentes de espécies diferentes colidem quando habilitado.
41. Predador e presa colidem quando habilitado.
42. Colisão desligada preserva sobreposição no teste controlado.
43. Colisão não altera energia diretamente.
44. Colisão não altera brain handle.
45. Colisão não altera genome id.
46. Colisão é determinística.

### Elasticidade e transferência

47. Elasticidade desligada apenas separa.
48. Elasticidade ligada altera velocidades de forma controlada.
49. Restitution 0 não ricocheteia.
50. Restitution positivo ricocheteia.
51. Restitution alto é clampado.
52. Transferência 0 não transfere velocidade.
53. Transferência positiva transfere parte da velocidade.
54. Transferência alta é clampada.
55. Velocidade resultante é finita.
56. Movimento forward continua funcionando.
57. Movimento omni continua funcionando.
58. Smooth locomotion continua funcionando.

### Viscosidade global

59. Viscosidade desligada preserva velocidade.
60. Viscosidade ligada reduz velocidade.
61. Viscosidade não inverte direção indevidamente.
62. Viscosidade não gera NaN/Inf.
63. Viscosidade compõe corretamente com smooth drag.
64. Viscosidade respeita dt fixo.
65. Viscosidade é determinística.

### Brownian motion

66. Brownian desligado não altera velocidade/posição.
67. Brownian ligado altera movimento.
68. Brownian usa seed determinística.
69. Mesma seed gera mesmo resultado.
70. Seed diferente gera resultado diferente.
71. Brownian não gera NaN/Inf.
72. Brownian respeita mundo retangular.
73. Brownian respeita mundo circular.
74. Brownian respeita obstáculos ou fallback seguro.
75. Brownian desligado tem custo quase zero.

### Comida chunk móvel

76. Chunk imóvel preserva comportamento da Fase 19.
77. Chunk móvel recebe velocidade.
78. Chunk móvel sofre drag.
79. Chunk móvel respeita massa efetiva.
80. Chunk móvel respeita mundo retangular.
81. Chunk móvel respeita mundo circular.
82. Chunk móvel não atravessa obstáculo sem política documentada.
83. Chunk móvel continua consumível.
84. Chunk móvel continua reabastecível.
85. Trim continua funcionando.
86. Clear food continua funcionando.
87. Clear obstacles não remove comida.
88. Clear food não remove obstáculos.

### Agente-comida chunk

89. Agente empurra comida chunk quando habilitado.
90. `chunk_food_push_strength=0` não empurra.
91. `chunk_food_push_strength>0` empurra.
92. Massa maior reduz deslocamento.
93. Drag reduz movimento após empurrão.
94. Empurrão não impede consumo.
95. Empurrão não gera NaN/Inf.
96. Empurrão é determinístico.

### Comida-comida

97. Comida-comida desligada não separa partículas.
98. Comida-comida ligada separa partículas sobrepostas.
99. Separação respeita raios.
100. Separação respeita massa efetiva.
101. Separação não explode velocidades.
102. Separação respeita mundo retangular.
103. Separação respeita mundo circular.
104. Separação respeita obstáculos ou fallback documentado.
105. Custo é medido.

### Adesão/cohesão

106. Adesão desligada não aplica força.
107. Adesão ligada aproxima partículas próximas.
108. Adesão respeita `chunk_food_adhesion_strength`.
109. Adesão não colapsa tudo em um ponto.
110. Adesão não gera NaN/Inf.
111. Adesão é determinística.
112. Adesão não quebra consumo.
113. Adesão não quebra reposição.

### Obstáculos e mundo

114. Agentes continuam não atravessando obstáculos.
115. Colisão agente-agente não empurra agente para dentro de obstáculo.
116. Comida chunk móvel respeita obstáculos, se implementado.
117. Mundo retangular continua funcionando.
118. Mundo circular continua funcionando.
119. Oclusão da Fase 20 continua funcionando.
120. Spawn blocking da Fase 20 continua funcionando.

### Integração com sistemas

121. EnergySystem continua aplicando metabolismo.
122. InteractionSystem continua consumindo comida.
123. InteractionSystem continua predation.
124. ReproductionSystem continua criando filhos.
125. DeathSystem continua removendo mortos.
126. SpatialHash não retorna pares duplicados.
127. SpatialHash não retorna auto-colisão.
128. `use_spatial=true` funciona.
129. `use_spatial=false` funciona em cenário pequeno.
130. Renderer mostra posições pós-física.
131. Render desligado funciona.
132. Headless funciona.

### Regressões

133. Fase 7 ainda passa.
134. Fase 8 ainda passa.
135. Fase 9 ainda passa.
136. Fase 10 ainda passa.
137. Fase 11 ainda passa.
138. Fase 12 ainda passa.
139. Fase 13 ainda passa.
140. Fase 14 ainda passa.
141. Fase 15 ainda passa.
142. Fase 16 ainda passa.
143. Fase 17 ainda passa.
144. Fase 18 ainda passa.
145. Fase 19 ainda passa.
146. Fase 20 ainda passa.
147. Nenhum arquivo Python foi alterado.
148. Fase 22 não foi iniciada.
149. UI base não foi implementada.
150. Save/load final não foi implementado.

## Diagnostics/microbenchmark obrigatório

Criar comando:

```text
--phase21-diagnostics
```

Medir de forma simples:

### Cenários mínimos

1. Física desligada:

   * 100 agentes / 100 comidas;
   * 300 agentes / 150 comidas;
   * 600 agentes / 300 comidas;
   * 1000 agentes / 500 comidas.

2. Agente-agente:

   * colisão off;
   * colisão on;
   * colisão + elasticidade;
   * colisão + transferência de velocidade;
   * colisão + impulso máximo baixo;
   * colisão + impulso máximo alto.

3. Viscosidade:

   * off;
   * drag 0.05;
   * drag 0.2;
   * drag 0.8.

4. Brownian:

   * off;
   * strength 1.0;
   * strength 3.0;
   * strength 10.0.

5. Comida chunk:

   * chunk imóvel;
   * chunk móvel;
   * chunk móvel + drag;
   * chunk móvel + agente empurrando;
   * chunk móvel + colisão comida-comida;
   * chunk móvel + adesão.

6. Obstáculos:

   * sem obstáculos;
   * com obstáculos e física off;
   * com obstáculos e física on;
   * comida chunk móvel perto de obstáculo;
   * agentes colidindo perto de obstáculo.

7. Predação:

   * sem predadores;
   * 30 predadores / 300 presas;
   * 100 predadores / 1000 presas;
   * predation com colisão off;
   * predation com colisão on.

8. Spatial:

   * `use_spatial=true`;
   * `use_spatial=false` apenas em cenário pequeno.

### Métricas obrigatórias

Registrar:

* wall time total;
* steps simulados;
* us/step;
* us/agente;
* tempo do MovementSystem;
* tempo do CollisionSystem;
* tempo de colisão agente-agente;
* tempo de colisão agente-comida;
* tempo de colisão comida-comida;
* tempo de viscosidade;
* tempo de Brownian;
* tempo de adesão/cohesão;
* tempo do SpatialHash;
* tempo do InteractionSystem;
* tempo do EnergySystem;
* tempo do ReproductionSystem;
* tempo do DeathSystem;
* tempo de renderização;
* número de agentes;
* número de comidas instantâneas;
* número de partículas/chunks;
* número de predadores;
* número de obstáculos;
* número de pares agente-agente testados;
* número de colisões agente-agente resolvidas;
* número de pares agente-comida testados;
* número de empurrões de comida;
* número de pares comida-comida testados;
* número de colisões comida-comida resolvidas;
* número de adesões aplicadas;
* número de movimentos Brownian aplicados;
* energia média;
* eventos de comida consumida;
* eventos de predação;
* nascimentos;
* mortes;
* overhead com física desligada;
* overhead com física ligada;
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
C_SFML_Teste_Legado/AgentBioSimCpp/build/Debug/AgentBioSimCpp.exe --phase21-selftest
C_SFML_Teste_Legado/AgentBioSimCpp/build/Release/AgentBioSimCpp.exe --phase21-diagnostics

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
```

Se algum comando anterior não existir, registre claramente.

Se algum teste falhar, corrija se for pequeno e diretamente relacionado. Se for falha estrutural grande, pare e reporte.

## Documentação obrigatória

Criar:

```text
C_SFML_Teste_Legado/MIGRACAO_C++SFML/PHASE_21_COLLISIONS_OPTIONAL_PHYSICS_STATUS.md
```

Esse arquivo deve conter:

* escopo executado;
* fora de escopo;
* documentos consultados;
* arquivos Python consultados;
* arquivos Python adicionais consultados;
* arquivos C++ criados;
* arquivos C++ modificados;
* correções de continuidade vindas das Fases 7 a 20, se houver;
* melhorias além do prompt mínimo, se houver;
* justificativa de cada melhoria além do escopo mínimo;
* decisão sobre `CollisionSystem`;
* decisão sobre ordem do loop;
* decisão sobre colisão agente-agente;
* decisão sobre separação;
* decisão sobre impulso máximo;
* decisão sobre elasticidade;
* decisão sobre transferência de velocidade;
* decisão sobre viscosidade global;
* decisão sobre Brownian motion;
* decisão sobre comida chunk móvel;
* decisão sobre massa efetiva de chunk;
* decisão sobre drag de chunk;
* decisão sobre empurrão de comida por agentes;
* decisão sobre colisão agente-comida chunk;
* decisão sobre colisão comida-comida;
* decisão sobre adesão/cohesão;
* decisão sobre integração com obstáculos;
* decisão sobre integração com mundo retangular;
* decisão sobre integração com mundo circular;
* decisão sobre integração com SpatialHash;
* decisão sobre integração com EnergySystem;
* decisão sobre integração com InteractionSystem;
* decisão sobre integração com ReproductionSystem;
* decisão sobre integração com DeathSystem;
* decisão sobre integração com Renderer;
* decisão sobre determinismo;
* decisão sobre performance;
* parâmetros usados;
* parâmetros pendentes;
* itens cobertos de `FEATURE_INVENTORY.md`;
* itens cobertos de `PARAMETER_INVENTORY.md`;
* itens de `UI_INVENTORY.md` impactados ou preservados para futuro;
* como comida instantânea da Fase 7 foi protegida;
* como locomoção da Fase 8 foi protegida;
* como visão das Fases 10 a 12 foi protegida;
* como reprodução/genoma da Fase 13 foi protegida;
* como redes neurais das Fases 14 a 16 foram protegidas;
* como espécies/genomas da Fase 17 foram protegidos;
* como predation/dieta da Fase 18 foi protegida;
* como comida chunk da Fase 19 foi protegida;
* como obstáculos/oclusão da Fase 20 foram protegidos;
* testes executados;
* resultado dos testes;
* resultado dos diagnostics/microbenchmark;
* diagnóstico visual/runtime;
* divergências contra Python;
* limitações atuais;
* pendências para Fase 22;
* pendências para Fase 23;
* pendências para Fase 24;
* pendências para Fase 26;
* pendências para Fase 27;
* pendências para Fase 28;
* pendências para Fase 29;
* pendências para Fase 30;
* confirmação de que não implementou UI base;
* confirmação de que não implementou save/load final;
* confirmação de que não implementou benchmark runner formal;
* confirmação de que nenhum Python foi alterado;
* confirmação de que não avançou para Fase 22.

## Atualizações documentais permitidas

Atualize, se necessário:

* `TECHNICAL_DEBT_REGISTER.md`, se a física revelar nova dívida técnica real;
* `MIGRATION_RISKS.md`, caso encontre risco novo;
* `BENCHMARK_PLAN.md`, apenas se encontrar lacuna objetiva sobre a Fase 21;
* `MIGRATION_PHASES.md`, apenas se houver ajuste pequeno de status ou pendência factual;
* `PHASE_20_OBSTACLES_OCCLUSION_STATUS.md`, apenas se encontrar erro factual que precisa ser corrigido.

Não reescreva cronograma inteiro.

Não renumere fases.

Não altere fases futuras por preferência pessoal.

## Qualidade arquitetural esperada

A solução deve:

* manter engine/headless possível;
* manter SFML/Renderer desacoplado;
* manter UI desacoplada;
* manter `CollisionSystem` independente de UI/SFML;
* manter lógica física fora de App sempre que possível;
* manter lógica física fora do Renderer;
* manter lógica física fora do NeuralSystem;
* preservar comida instantânea;
* preservar comida chunk;
* preservar obstáculos;
* preservar oclusão;
* preservar DietConfig;
* preservar predation;
* preservar SpeciesStore;
* preservar GenomeStore;
* preservar todos os tipos neurais;
* preservar determinismo por seed;
* preservar dt fixo;
* preparar caminho claro para Fase 22 UI base/canvas;
* preparar caminho claro para Fase 23 preferências de física;
* preparar caminho claro para Fase 24 editor de substrato;
* preparar caminho claro para Fase 26 métricas/profiler;
* preparar caminho claro para Fase 27 save/load;
* preparar caminho claro para Fase 30 otimização;
* medir antes/depois.

## Dívidas técnicas

Consulte `TECHNICAL_DEBT_REGISTER.md`.

Nesta fase, não é obrigatório resolver:

```text
Dívida 4 — App acumulando responsabilidades
Dívida 5 — Alocações temporárias no NeuralSystem
Dívida 6 — Version.hpp desatualizado
```

A menos que isso seja pequeno, seguro e útil.

Não implemente Fase 22.

Não faça refatoração grande do App.

Se uma dívida atrapalhar diretamente `CollisionSystem`, `SpatialHash`, `FoodStore`, `AgentStore`, `ObstacleStore`, `WorldBounds`, `MovementSystem`, `InteractionSystem` ou Renderer básico, corrija ou documente.

## Saída final esperada

Ao final, responda com:

1. resumo executivo;
2. git status;
3. último commit antes da Fase 21;
4. arquivos criados;
5. arquivos modificados;
6. arquivos Python consultados;
7. documentos consultados;
8. correções de continuidade das Fases 7 a 20, se houver;
9. melhorias além do escopo mínimo, se houver;
10. parâmetros usados;
11. parâmetros pendentes;
12. como funciona `CollisionSystem`;
13. qual a ordem final do loop;
14. como funciona colisão agente-agente;
15. como funciona separação;
16. como funciona impulso máximo;
17. como funciona elasticidade;
18. como funciona transferência de velocidade;
19. como funciona viscosidade global;
20. como funciona Brownian motion;
21. como funciona comida chunk móvel;
22. como funciona massa efetiva de chunk;
23. como funciona drag de chunk;
24. como funciona empurrão de comida por agentes;
25. como funciona colisão agente-comida chunk;
26. como funciona colisão comida-comida;
27. como funciona adesão/cohesão;
28. como obstáculos foram preservados;
29. como mundo retangular foi respeitado;
30. como mundo circular foi respeitado;
31. como SpatialHash foi integrado;
32. como EnergySystem foi preservado;
33. como InteractionSystem foi preservado;
34. como ReproductionSystem foi preservado;
35. como DeathSystem foi preservado;
36. como Renderer foi integrado;
37. como determinismo foi garantido;
38. como comida instantânea da Fase 7 foi protegida;
39. como locomoção da Fase 8 foi protegida;
40. como visão das Fases 10 a 12 foi protegida;
41. como reprodução/genoma da Fase 13 foi protegida;
42. como redes neurais das Fases 14 a 16 foram protegidas;
43. como SpeciesStore/GenomeStore da Fase 17 foram protegidos;
44. como dieta/predação da Fase 18 foram protegidas;
45. como comida chunk da Fase 19 foi protegida;
46. como obstáculos/oclusão da Fase 20 foram protegidos;
47. como isso prepara a Fase 22;
48. como isso prepara a Fase 23;
49. como isso prepara a Fase 24;
50. como isso prepara a Fase 26;
51. como isso prepara a Fase 27;
52. como isso prepara a Fase 30;
53. resultado do build Debug;
54. resultado do build Release;
55. resultado do `--phase21-selftest`;
56. resultado do `--phase21-diagnostics`;
57. resultado das regressões das Fases 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 e 20;
58. custo com física desligada;
59. custo com colisão agente-agente;
60. custo com elasticidade;
61. custo com transferência de velocidade;
62. custo com viscosidade;
63. custo com Brownian motion;
64. custo com comida chunk móvel;
65. custo com colisão comida-comida;
66. custo com adesão/cohesão;
67. custo com obstáculos + física;
68. custo de SpatialHash;
69. número de pares agente-agente testados;
70. número de colisões agente-agente resolvidas;
71. número de pares agente-comida testados;
72. número de empurrões de comida;
73. número de pares comida-comida testados;
74. número de colisões comida-comida resolvidas;
75. número de adesões aplicadas;
76. eventos de comida consumida;
77. eventos de predação;
78. nascimentos;
79. mortes;
80. diagnóstico visual/runtime;
81. divergências contra Python;
82. pendências para Fase 22;
83. pendências para Fase 23;
84. pendências para Fase 24;
85. pendências para Fase 26;
86. pendências para Fase 27;
87. pendências para Fase 28;
88. pendências para Fase 29;
89. pendências para Fase 30;
90. confirmação de que nenhum Python foi alterado;
91. confirmação de que não implementou UI base;
92. confirmação de que não implementou save/load final;
93. confirmação de que não implementou benchmark runner formal;
94. confirmação de que não avançou para Fase 22;
95. decisão: pronto para commit da Fase 21 ou precisa correção.

Se tudo estiver certo, pode fazer o commit da Fase 21.
