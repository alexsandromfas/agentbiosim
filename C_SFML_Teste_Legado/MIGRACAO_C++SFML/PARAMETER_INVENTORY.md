# Inventario de Parametros

Fonte principal: `sim/controllers.py::Params._setup_defaults()` e usos encontrados em `sim/ui.py`, `sim/engine.py`, `sim/entities.py`, `sim/sensors.py`, `sim/systems.py` e `sim/brain.py`.

Status:

- `identificado`: default encontrado em `Params`.
- `parcialmente identificado`: usado no codigo, mas default principal nao foi localizado em `Params` nesta leitura.
- `nao rastreado`: citado conceitualmente ou historicamente, precisa verificacao manual.

## Terminologia de Parametros e Aliases

- Prefixos `bacteria_` e `predator_` sao parametros reais atuais e devem ser preservados no loader.
- Na arquitetura C++, esses prefixos devem migrar para configuracoes por `SpeciesConfig`, mantendo aliases.
- Metadados de `labels` devem virar metadados de `species`, mas o nome `labels` deve continuar aceito em saves antigos.
- `auto_export_substrate` e nome interno legado para autosave. Deve continuar aceito como alias.

## Parametros Gerais, Mundo e Tempo

| Parametro | Padrao | Tipo provavel | Categoria | Definido em | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|---|---|
| `time_scale` | `1.0` | float | tempo | `controllers.py` | sim/toolbar | Escala tempo simulado | Manter no runtime config | identificado |
| `fps` | `60` | int/float | render | `controllers.py` | preferencias | FPS alvo | Separar de physics Hz | identificado |
| `paused` | `False` | bool | simulacao | `controllers.py` | toolbar/atalho | Pausa update | Manter | identificado |
| `physics_steps_per_second` | `30` | int | fisica | `controllers.py` | preferencias | dt fixo | Critico para paridade | identificado |
| `max_physics_steps_per_frame` | `8` | int | fisica | `controllers.py` | preferencias | Limita backlog | Manter | identificado |
| `max_physics_backlog_seconds` | `0.25` | float | fisica | `controllers.py` | preferencias | Evita espiral de atraso | Manter | identificado |
| `use_spatial` | `True` | bool | performance | `controllers.py` | preferencias | Usa spatial hash | Manter | identificado |
| `substrate_shape` | `rectangular` | enum | mundo | `controllers.py` | substrato | Retangular/circular | Enum C++ | identificado |
| `world_w` | `1000.0` | float | mundo | `controllers.py` | substrato | Largura | Manter | identificado |
| `world_h` | `700.0` | float | mundo | `controllers.py` | substrato | Altura | Manter | identificado |
| `substrate_radius` | `400.0` | float | mundo | `controllers.py` | substrato | Raio circular | Manter | identificado |
| `random_seed` | `-1` | int | reproducibilidade | `controllers.py` | preferencias | Seed fixa/desligada | Manter RNG service | identificado |
| `agent_template_name` | `organismo_1` | string | genoma | `controllers.py` | editor | Nome/template | Migrar para Species/Genome | identificado |
| `max_deaths_per_step` | `5` | int | simulacao | `controllers.py` | nao identificado | Limita remocoes | Manter | identificado |
| `population_min_rescue_enabled` | `True` | bool | populacao | `controllers.py` | UI | Resgate minimo | Reavaliar por especie | identificado |

## Performance, Render e Percepcao Global

| Parametro | Padrao | Tipo | Categoria | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|---|
| `retina_skip` | `0` | int | percepcao | preferencias | Pula frames de retina | Manter como opcao experimental | identificado |
| `retina_vision_mode` | `single` | enum | percepcao | sistema de visao | Modo single/fullbody/sector | Strategy C++ | identificado |
| `render_enabled` | `True` | bool | render | preferencias | Desliga render | Manter headless | identificado |
| `simple_render` | `False` | bool | render | menu/atalho | Render rapido | SFML quality mode | identificado |
| `show_spatial_hash` | `False` | bool | debug visual | view | Mostra grid | Overlay opcional | identificado |
| `render_resolution_scale` | `1.0` | float | render | preferencias | Supersampling/escala | Medir custo | identificado |
| `use_numba_kernels` | `True` | bool | Python perf | preferencias | Usa Numba | Substituir por C++ nativo | identificado |
| `use_numba_batch_retina` | `False` | bool | Python perf | preferencias | Retina batch Numba | Substituir por kernels C++ | identificado |
| `use_grouped_vision_batches` | `True` | bool | percepcao | preferencias | Agrupa visao | Manter assinatura/batch | identificado |
| `use_persistent_perception_arrays` | `False` | bool | percepcao | preferencias | Buffers persistentes | Virar padrao C++ | identificado |
| `retina_high_scale_auto_sector` | `False` | bool | percepcao | preferencias | Auto sector em escala | Manter opcional | identificado |
| `retina_high_scale_sector_min_agents` | `800` | int | percepcao | preferencias | Limiar auto sector | Manter | identificado |
| `retina_high_scale_global_sector` | `False` | bool | percepcao | preferencias | Setor global experimental | Manter experimental | identificado |
| `reuse_spatial_grid` | `True` | bool | spatial | preferencias | Reutiliza grid | Manter | identificado |
| `use_numba_locomotion_energy` | `False` | bool | Python perf | preferencias | Locomocao/energia Numba | Substituir por batch C++ | identificado |
| `brain_cache_disable` | `False` | bool | neural/perf | preferencias | Desliga cache | C++ usar cache/store | identificado |
| `brain_cache_max_entries` | `32` em uso UI | int | neural/perf | UI uso | Tamanho cache | Precisa default formal | parcialmente identificado |
| `brain_cache_max_mb` | `512` em uso UI | int | neural/perf | UI uso | Memoria cache | Precisa default formal | parcialmente identificado |
| `brain_cache_log` | `False` em uso UI | bool | debug | UI uso | Log cache | Precisa default formal | parcialmente identificado |
| `use_numba_brain_forward` | `False` | bool | neural/perf | preferencias | Forward Numba | Substituir por C++ batch | identificado |
| `numba_brain_forward_min_batch` | `256` | int | neural/perf | preferencias | Min batch | Reavaliar C++ | identificado |

## Visao por Bins/Setores

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `retina_bins_mode` | `nearest` | enum | sistema de visao | Agregacao por bin | Enum `Nearest/Strongest/Sum/Weighted` | identificado |
| `retina_bins_distance_subdivisions` | `5` | int | sistema de visao | Resolucao radial sem mudar inputs | Manter 1..99 | identificado |
| `retina_bins_distance_distribution` | `near_detail` | enum | sistema de visao | Mais resolucao perto | Manter | identificado |
| `retina_bins_distance_falloff` | `linear` | enum | sistema de visao | Funcao intensidade/distancia | Manter | identificado |
| `retina_bins_projection` | `center` | enum | sistema de visao | Centro/bordas/aparent size | Manter | identificado |
| `retina_bins_candidate_limit` | `128` | int | sistema de visao | Limita candidatos | Medir | identificado |
| `retina_bins_obstacles_block_vision` | `False` | bool | sistema de visao | Oclusao | Manter opcional | identificado |

## Redes Neurais Globais

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `neural_network_type` | `mlp` | enum | redes neurais | Tipo de cerebro | `BrainType` | identificado |
| `neural_gate_init` | `1.0` | float | redes neurais | Gates iniciais | Gated MLP config | identificado |
| `neural_gate_min` | `0.0` | float | redes neurais | Clamp gate | Manter | identificado |
| `neural_gate_max` | `2.0` | float | redes neurais | Clamp gate | Manter | identificado |
| `neural_gate_mutation_rate` | `-1.0` | float | redes neurais | Override taxa | `-1` = usa taxa base | identificado |
| `neural_gate_mutation_strength` | `-1.0` | float | redes neurais | Override intensidade | Manter alias | identificado |
| `neural_shortcut_init_std` | `0.05` | float | redes neurais | Init atalho | Manter | identificado |
| `neural_shortcut_scale` | `0.25` | float | redes neurais | Escala atalho | Manter | identificado |
| `neural_shortcut_mutation_rate` | `-1.0` | float | redes neurais | Override taxa | Manter | identificado |
| `neural_shortcut_mutation_strength` | `-1.0` | float | redes neurais | Override intensidade | Manter | identificado |
| `neural_rnn_recurrent_init_std` | `0.08` | float | redes neurais | Init recorrente | Manter | identificado |
| `neural_rnn_recurrent_scale` | `0.35` | float | redes neurais | Escala recorrente | Manter | identificado |
| `neural_rnn_memory_decay` | `0.6` | float | redes neurais | Decaimento memoria | Manter clamp <1 | identificado |
| `neural_rnn_state_clip` | `1.0` | float | redes neurais | Clamp estado | Manter | identificado |
| `neural_rnn_reset_state_on_copy` | `True` | bool | redes neurais | Estado no filho | Manter | identificado |
| `neural_rnn_mutation_rate` | `-1.0` | float | redes neurais | Override taxa | Manter | identificado |
| `neural_rnn_mutation_strength` | `-1.0` | float | redes neurais | Override intensidade | Manter | identificado |

## NEAT

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `neural_neat_initial_topology` | `minimal` | enum | redes neurais | Topologia inicial | Manter minimal/layered | identificado |
| `neural_neat_weight_init_std` | `0.6` | float | redes neurais | Init pesos | Manter | identificado |
| `neural_neat_weight_mutation_rate` | `-1.0` | float | redes neurais | Mutacao peso | Manter | identificado |
| `neural_neat_weight_mutation_strength` | `-1.0` | float | redes neurais | Intensidade | Manter | identificado |
| `neural_neat_add_connection_rate` | `0.08` | float | redes neurais | Adiciona conexao | Manter | identificado |
| `neural_neat_add_node_rate` | `0.03` | float | redes neurais | Adiciona neuronio | Manter | identificado |
| `neural_neat_toggle_connection_rate` | `0.01` | float | redes neurais | Liga/desliga conexao | Manter | identificado |
| `neural_neat_remove_connection_rate` | `0.005` | float | redes neurais | Remove conexao | Manter | identificado |
| `neural_neat_reset_weight_rate` | `0.02` | float | redes neurais | Reseta peso | Manter | identificado |
| `neural_neat_max_hidden_nodes` | `64` | int | redes neurais | Limite nodes | Manter para evitar explosao | identificado |
| `neural_neat_max_connections` | `512` | int | redes neurais | Limite conexoes | Manter | identificado |
| `neural_proto_neat_initial_topology` | `minimal` | enum | redes neurais | Topologia inicial | Manter | identificado |
| `neural_proto_neat_weight_init_std` | `0.6` | float | redes neurais | Init pesos | Manter | identificado |
| `neural_proto_neat_weight_mutation_rate` | `-1.0` | float | redes neurais | Mutacao peso | Manter | identificado |
| `neural_proto_neat_weight_mutation_strength` | `-1.0` | float | redes neurais | Intensidade | Manter | identificado |
| `neural_proto_neat_add_connection_rate` | `0.04` | float | redes neurais | Adiciona conexao | Manter | identificado |
| `neural_proto_neat_add_node_rate` | `0.02` | float | redes neurais | Adiciona node | Manter | identificado |
| `neural_proto_neat_toggle_connection_rate` | `0.0` | float | redes neurais | Toggle conexao | Manter | identificado |
| `neural_proto_neat_remove_connection_rate` | `0.0` | float | redes neurais | Remove conexao | Manter | identificado |
| `neural_proto_neat_reset_weight_rate` | `0.03` | float | redes neurais | Reseta peso | Manter | identificado |
| `neural_proto_neat_max_hidden_nodes` | `48` | int | redes neurais | Limite nodes | Manter | identificado |
| `neural_proto_neat_max_connections` | `384` | int | redes neurais | Limite conexoes | Manter | identificado |
| `neural_recurrent_neat_initial_topology` | `minimal` | enum | redes neurais | Topologia inicial | Manter | identificado |
| `neural_recurrent_neat_weight_init_std` | `0.45` | float | redes neurais | Init pesos | Manter | identificado |
| `neural_recurrent_neat_weight_mutation_rate` | `-1.0` | float | redes neurais | Mutacao peso | Manter | identificado |
| `neural_recurrent_neat_weight_mutation_strength` | `-1.0` | float | redes neurais | Intensidade | Manter | identificado |
| `neural_recurrent_neat_add_connection_rate` | `0.08` | float | redes neurais | Add conn | Manter | identificado |
| `neural_recurrent_neat_add_node_rate` | `0.025` | float | redes neurais | Add node | Manter | identificado |
| `neural_recurrent_neat_toggle_connection_rate` | `0.01` | float | redes neurais | Toggle | Manter | identificado |
| `neural_recurrent_neat_remove_connection_rate` | `0.003` | float | redes neurais | Remove | Manter | identificado |
| `neural_recurrent_neat_reset_weight_rate` | `0.02` | float | redes neurais | Reset peso | Manter | identificado |
| `neural_recurrent_neat_max_hidden_nodes` | `64` | int | redes neurais | Limite nodes | Manter | identificado |
| `neural_recurrent_neat_max_connections` | `640` | int | redes neurais | Limite conexoes | Manter | identificado |
| `neural_recurrent_neat_recurrent_connection_rate` | `0.12` | float | redes neurais | Add recorrente | Manter | identificado |
| `neural_recurrent_neat_memory_decay` | `0.85` | float | redes neurais | Memoria | Manter | identificado |
| `neural_recurrent_neat_state_clip` | `1.0` | float | redes neurais | Clamp estado | Manter | identificado |
| `neural_recurrent_neat_reset_state_on_copy` | `True` | bool | redes neurais | Estado no filho | Manter | identificado |

## Comida e Substrato

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `food_mode` | `instant` | enum | substrato | Tipo de comida | `Instant/Chunk` | identificado |
| `food_target` | `50` | int | substrato | Quantidade alvo | Manter | identificado |
| `food_min_r` | `4.5` | float | substrato | Raio minimo | Manter | identificado |
| `food_max_r` | `5.0` | float | substrato | Raio maximo | Manter | identificado |
| `food_replenish_interval` | `0.1` | float | substrato | Intervalo reposicao | Manter | identificado |
| `food_bite_seconds` | `6.0` | float | substrato | Tempo consumo chunk | Manter | identificado |
| `food_piece_particle_radius` | `5.0` | float | substrato | Raio particula | Manter | identificado |
| `food_piece_cluster_radius` | `36.0` | float | substrato | Raio aglomerado | Manter | identificado |
| `food_piece_particle_spacing` | `0.0` | float | substrato | Espacamento | Manter | identificado |
| `food_piece_replenish_mode` | `spawn_cluster` | enum | substrato | Reposicao chunk | Manter | identificado |
| `food_trim_excess_enabled` | `True` | bool | substrato | Remove excesso | Manter | identificado |
| `food_trim_max_per_step` | `5` | int | substrato | Limite trim | Manter | identificado |
| `food_color` | `(220,30,30)` | RGB | aparencia/substrato | Cor comida | Manter | identificado |

## Bacterias / Organismo Base

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `bacteria_count` | `150` | int | populacao/editor | Inicial | Migrar para especie default | identificado |
| `bacteria_min_r` | `6.0` | float | legado | Raio min spawn | Alias de body size? | identificado |
| `bacteria_max_r` | `12.0` | float | legado | Raio max spawn | Alias | identificado |
| `bacteria_min_limit` | `0` | int | populacao/label | Minimo | Migrar por especie | identificado |
| `bacteria_max_limit` | `0` | int | populacao/label | Maximo | Migrar por especie | identificado |
| `bacteria_initial_energy` | `100.0` | float | editor | Energia inicial | Genome/species config | identificado |
| `bacteria_death_energy` | `50.0` | float | editor | Morte por energia | Manter | identificado |
| `bacteria_split_energy` | `150.0` | float | editor | Reproducao | Manter | identificado |
| `bacteria_age_death_enabled` | `False` | bool | editor | Morte por idade | Manter | identificado |
| `bacteria_death_age` | `3600.0` | float | editor | Idade morte | Manter | identificado |
| `bacteria_corpse_to_food` | `False` | bool | editor | Cadaver vira comida | Manter | identificado |
| `bacteria_reproduction_min_age` | `0.0` | float | editor | Reproducao | Manter | identificado |
| `bacteria_reproduction_cooldown` | `0.0` | float | editor | Reproducao | Manter | identificado |
| `bacteria_metab_v0_cost` | `0.5` | float | editor | Custo parado | Critico | identificado |
| `bacteria_metab_vmax_cost` | `8.0` | float | editor | Custo vmax | Critico | identificado |
| `bacteria_energy_cap` | `400.0` | float | editor | Cap energia | Manter | identificado |
| `bacteria_body_size` | `9.0` | float | editor | Corpo | Manter | identificado |
| `bacteria_body_shape` | `ellipse` | enum | editor | Forma | Manter | identificado |
| `bacteria_max_speed` | `300.0` | float | editor | Movimento | Manter | identificado |
| `bacteria_max_turn` | `pi` | float | editor | Giro | Manter radianos/graus UI | identificado |
| `bacteria_allow_reverse_locomotion` | `False` | bool | editor | Re | Manter | identificado |
| `bacteria_movement_mode` | `forward` | enum | editor | Saidas RN | Critico para tamanho output | identificado |
| `bacteria_vision_radius` | `120.0` | float | editor | Visao | Manter | identificado |
| `bacteria_vision_mode` | `frontal` | enum | editor | Visao | Verificar uso | identificado |
| `bacteria_retina_count` | `18` | int | editor | Inputs | Manter | identificado |
| `bacteria_retina_fov_degrees` | `180.0` | float | editor | FOV | Manter | identificado |
| `bacteria_eye_count` | `1` | int | editor | Olhos | Manter | identificado |
| `bacteria_eye_angle_degrees` | `60.0` | float | editor | Abertura olhos | Manter | identificado |
| `bacteria_eye_separation_degrees` | `45.0` | float | editor | Separacao olhos | Manter | identificado |
| `bacteria_show_vision` | `False` | bool | editor/view | Debug visual | Manter | identificado |
| `bacteria_retina_see_food` | `True` | bool | editor | Ver comida | Manter | identificado |
| `bacteria_retina_see_bacteria` | `False` | bool | editor | Ver organismos | Manter | identificado |
| `bacteria_retina_see_predators` | `False` | bool | editor | Ver predadores | Manter | identificado |
| `bacteria_retina_see_obstacles` | `False` | bool | editor | Ver obstaculos | Manter | identificado |
| `bacteria_retina_see_all` | `False` | bool | editor | Ver tudo | Manter | identificado |
| `bacteria_retina_see_through_walls` | `True` | bool | editor | Oclusao | Manter | identificado |
| `bacteria_retina_input_mode` | `distance_only` | enum | editor | Canais | Manter alias | identificado |
| `bacteria_retina_channel_r` | `False` | bool | editor | Canal R | Manter | identificado |
| `bacteria_retina_channel_g` | `False` | bool | editor | Canal G | Manter | identificado |
| `bacteria_retina_channel_b` | `False` | bool | editor | Canal B | Manter | identificado |
| `bacteria_retina_channel_d` | `True` | bool | editor | Canal D | Manter | identificado |
| `bacteria_diet_food` | `True` | bool | editor | Dieta | Migrar para organismo generico | identificado |
| `bacteria_diet_agents` | `False` | bool | editor | Dieta | Manter | identificado |
| `bacteria_diet_same_label` | `False` | bool | editor | Canibalismo | Renomear especie | identificado |
| `bacteria_diet_food_efficiency` | `1.0` | float | editor | Energia comida | Manter | identificado |
| `bacteria_diet_agent_efficiency` | `0.7` | float | editor | Energia agente | Manter | identificado |
| `bacteria_hidden_layers` | `4` | int | editor | RN | Manter | identificado |
| `bacteria_neurons_layer_1` | `20` | int | editor | RN | Manter | identificado |
| `bacteria_neurons_layer_2` | `20` | int | editor | RN | Manter | identificado |
| `bacteria_neurons_layer_3` | `20` | int | editor | RN | Manter | identificado |
| `bacteria_neurons_layer_4` | `20` | int | editor | RN | Manter | identificado |
| `bacteria_neurons_layer_5` | `0` | int | editor | RN | Manter | identificado |
| `bacteria_mutation_rate` | `0.05` | float | editor | Evolucao | Manter | identificado |
| `bacteria_mutation_strength` | `0.08` | float | editor | Evolucao | Manter | identificado |
| `bacteria_structural_jitter` | `0` | int/float | editor | Evolucao | Verificar uso | identificado |
| `bacteria_color` | `(220,220,220)` | RGB | editor/aparencia | Cor | Manter | identificado |

## Predadores

Os parametros de predador replicam a estrutura de bacteria com prefixo `predator_`.

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `predators_enabled` | `False` | bool | predadores | Habilita predador | Migrar para especie/dieta | identificado |
| `predator_count` | `0` | int | predadores | Inicial | Migrar para especie | identificado |
| `predator_min_r` | `10.0` | float | legado | Raio min | Alias | identificado |
| `predator_max_r` | `18.0` | float | legado | Raio max | Alias | identificado |
| `predator_min_limit` | `0` | int | predadores | Min | Por especie | identificado |
| `predator_max_limit` | `100` | int | predadores | Max | Por especie | identificado |
| `predator_initial_energy` | `100.0` | float | editor | Energia | Manter | identificado |
| `predator_death_energy` | `50.0` | float | editor | Morte | Manter | identificado |
| `predator_split_energy` | `150.0` | float | editor | Reproducao | Manter | identificado |
| `predator_age_death_enabled` | `False` | bool | editor | Idade | Manter | identificado |
| `predator_death_age` | `3600.0` | float | editor | Idade morte | Manter | identificado |
| `predator_corpse_to_food` | `False` | bool | editor | Cadaver | Manter | identificado |
| `predator_reproduction_min_age` | `0.0` | float | editor | Reproducao | Manter | identificado |
| `predator_reproduction_cooldown` | `0.0` | float | editor | Reproducao | Manter | identificado |
| `predator_metab_v0_cost` | `1.0` | float | editor | Custo parado | Manter | identificado |
| `predator_metab_vmax_cost` | `15.0` | float | editor | Custo vmax | Manter | identificado |
| `predator_energy_cap` | `600.0` | float | editor | Cap | Manter | identificado |
| `predator_body_size` | `14.0` | float | editor | Corpo | Manter | identificado |
| `predator_body_shape` | `ellipse` | enum | editor | Forma | Manter | identificado |
| `predator_max_speed` | `300.0` | float | editor | Velocidade | Manter | identificado |
| `predator_max_turn` | `pi` | float | editor | Giro | Manter | identificado |
| `predator_allow_reverse_locomotion` | `False` | bool | editor | Re | Manter | identificado |
| `predator_movement_mode` | `forward` | enum | editor | Saidas RN | Manter | identificado |
| `predator_vision_radius` | `120.0` | float | editor | Visao | Manter | identificado |
| `predator_vision_mode` | `frontal` | enum | editor | Visao | Verificar uso | identificado |
| `predator_retina_count` | `18` | int | editor | Inputs | Manter | identificado |
| `predator_retina_fov_degrees` | `180.0` | float | editor | FOV | Manter | identificado |
| `predator_eye_count` | `1` | int | editor | Olhos | Manter | identificado |
| `predator_eye_angle_degrees` | `60.0` | float | editor | Abertura olhos | Manter | identificado |
| `predator_eye_separation_degrees` | `45.0` | float | editor | Separacao | Manter | identificado |
| `predator_show_vision` | `False` | bool | editor/view | Debug | Manter | identificado |
| `predator_retina_see_food` | `True` | bool | editor | Ver comida | Manter | identificado |
| `predator_retina_see_bacteria` | `True` | bool | editor | Ver organismos | Manter | identificado |
| `predator_retina_see_predators` | `False` | bool | editor | Ver predador | Manter | identificado |
| `predator_retina_see_obstacles` | `False` | bool | editor | Ver obstaculos | Manter | identificado |
| `predator_retina_see_all` | `False` | bool | editor | Ver tudo | Manter | identificado |
| `predator_retina_see_through_walls` | `True` | bool | editor | Oclusao | Manter | identificado |
| `predator_retina_input_mode` | `distance_only` | enum | editor | Canais | Manter | identificado |
| `predator_retina_channel_r` | `False` | bool | editor | R | Manter | identificado |
| `predator_retina_channel_g` | `False` | bool | editor | G | Manter | identificado |
| `predator_retina_channel_b` | `False` | bool | editor | B | Manter | identificado |
| `predator_retina_channel_d` | `True` | bool | editor | D | Manter | identificado |
| `predator_diet_food` | `False` | bool | editor | Dieta | Manter | identificado |
| `predator_diet_agents` | `True` | bool | editor | Dieta | Manter | identificado |
| `predator_diet_same_label` | `False` | bool | editor | Canibalismo | Manter | identificado |
| `predator_diet_food_efficiency` | `1.0` | float | editor | Energia | Manter | identificado |
| `predator_diet_agent_efficiency` | `0.7` | float | editor | Energia | Manter | identificado |
| `predator_hidden_layers` | `2` | int | editor | RN | Manter | identificado |
| `predator_neurons_layer_1` | `16` | int | editor | RN | Manter | identificado |
| `predator_neurons_layer_2` | `8` | int | editor | RN | Manter | identificado |
| `predator_neurons_layer_3` | `0` | int | editor | RN | Manter | identificado |
| `predator_neurons_layer_4` | `0` | int | editor | RN | Manter | identificado |
| `predator_neurons_layer_5` | `0` | int | editor | RN | Manter | identificado |
| `predator_mutation_rate` | `0.05` | float | editor | Evolucao | Manter | identificado |
| `predator_mutation_strength` | `0.08` | float | editor | Evolucao | Manter | identificado |
| `predator_structural_jitter` | `0` | int/float | editor | Evolucao | Verificar uso | identificado |
| `predator_color` | `(80,120,220)` | RGB | editor/aparencia | Cor | Manter | identificado |

## Fisica

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `agents_inertia` | `1.0` | float | preferencias | Inercia global | Manter | identificado |
| `smooth_locomotion_enabled` | `False` | bool | preferencias | Locomocao suave | Manter opcional | identificado |
| `smooth_linear_inertia_enabled` | `True` | bool | preferencias | Inercia linear | Manter | identificado |
| `smooth_max_linear_accel` | `900.0` | float | preferencias | Aceleracao | Manter | identificado |
| `smooth_linear_drag_enabled` | `True` | bool | preferencias | Arrasto linear | Manter | identificado |
| `smooth_linear_drag` | `0.75` | float | preferencias | Arrasto | Manter | identificado |
| `smooth_angular_inertia_enabled` | `True` | bool | preferencias | Inercia angular | Manter | identificado |
| `smooth_max_angular_accel` | `4*pi` | float | preferencias | Acel angular | Manter | identificado |
| `smooth_angular_drag_enabled` | `True` | bool | preferencias | Arrasto angular | Manter | identificado |
| `smooth_angular_drag` | `1.5` | float | preferencias | Arrasto angular | Manter | identificado |
| `render_interpolation_enabled` | `False` | bool | preferencias | Suaviza render | Manter | identificado |
| `camera_follow_smoothing_enabled` | `True` | bool | preferencias/codigo | Camera suave | Pode virar sempre ligado | identificado |
| `camera_follow_smoothing` | `10.0` | float | preferencias/codigo | Suavizacao camera | Manter | identificado |
| `agent_collision_enabled` | `True` | bool | preferencias | Colisao agentes | Manter | identificado |
| `agent_collision_elasticity_enabled` | `True` | bool | preferencias | Elasticidade | Manter | identificado |
| `agent_collision_restitution` | `0.12` | float | preferencias | Ricochete | Manter | identificado |
| `agent_collision_velocity_transfer` | `0.35` | float | preferencias | Transf velocidade | Manter | identificado |
| `agent_collision_separation` | `0.9` | float | preferencias | Separacao | Manter | identificado |
| `agent_collision_max_impulse` | `900.0` | float | preferencias | Impulso max | Manter | identificado |
| `global_viscosity_enabled` | `False` | bool | preferencias | Viscosidade | Manter | identificado |
| `global_viscosity_drag` | `0.2` | float | preferencias | Drag global | Manter | identificado |
| `movable_chunk_food_enabled` | `False` | bool | preferencias | Comida movel | Manter opcional | identificado |
| `chunk_food_collision_enabled` | `True` | bool | preferencias | Colisao comida | Manter opcional | identificado |
| `chunk_food_adhesion_enabled` | `True` | bool | preferencias | Adesao comida | Reavaliar custo | identificado |
| `chunk_food_adhesion_strength` | `0.35` | float | preferencias | Forca adesao | Reavaliar | identificado |
| `chunk_food_mass_scale` | `1.0` | float | preferencias | Massa comida | Manter | identificado |
| `chunk_food_drag` | `1.6` | float | preferencias | Drag comida | Manter | identificado |
| `chunk_food_push_strength` | `0.45` | float | preferencias | Empurrao comida | Manter | identificado |
| `brownian_motion_enabled` | `False` | bool | preferencias | Ruido | Manter opcional | identificado |
| `brownian_motion_strength` | `3.0` | float | preferencias | Intensidade ruido | Manter | identificado |
| `allow_reverse_locomotion` | `False` | bool | legado/global | Re global | Preferir por especie | identificado |
| `reproduction_min_age` | `0.0` | float | legado/global | Idade min | Preferir por especie | identificado |
| `reproduction_cooldown` | `0.0` | float | legado/global | Cooldown | Preferir por especie | identificado |

## UI, Debug, Save e Aparencia

| Parametro | Padrao | Tipo | UI | Impacto | Migracao | Status |
|---|---:|---|---|---|---|---|
| `show_selected_details` | `True` | bool | view/aba | Painel agente | Manter | identificado |
| `show_multi_selected_vision` | `False` | bool | visao | Visao multi-selecao | Manter | identificado |
| `show_metrics_chart` | `False` | bool | view | Grafico | Manter | identificado |
| `neural_view_dense_layout` | `fixed` | enum | view | Layout viewer | Manter | identificado |
| `camera_follow_selected_agent` | `True` | bool | view | Seguir agente | Manter | identificado |
| `metrics_chart_sample_seconds` | `5` | int | preferencias | Amostragem grafico | Manter | identificado |
| `debug_tracebacks` | `False` | bool | autosave/debug | Tracebacks | Manter | identificado |
| `diagnostic_heartbeat_minutes` | `1.0` | float | debug | Heartbeat | Manter | identificado |
| `save_recovery_on_close` | `True` | bool | save | Recovery | Manter | identificado |
| `auto_export_substrate` | `False` | bool | autosave | Autosave | Renomear alias interno | identificado |
| `auto_export_interval_minutes` | `10.0` | float | autosave | Intervalo | Manter | identificado |
| `export_substrate_include_brain_activations` | `False` | bool | autosave/export | Salva ativacoes | Manter | identificado |
| `export_substrate_pretty_json` | `False` | bool | autosave/export | JSON legivel | Manter | identificado |
| `debug_reproduction_color` | `False` | bool | debug | Cor debug | Manter | identificado |
| `substrate_bg_color` | `(10,10,20)` | RGB | aparencia | Fundo legado | Alias | identificado |
| `background_gradient_enabled` | `False` | bool | aparencia | Gradiente fundo | Manter | identificado |
| `background_color_top` | `(10,10,20)` | RGB | aparencia | Fundo topo | Manter | identificado |
| `background_color_bottom` | `(10,10,20)` | RGB | aparencia | Fundo baixo | Manter | identificado |
| `substrate_gradient_enabled` | `False` | bool | aparencia | Gradiente substrato | Manter | identificado |
| `substrate_color_top` | `(10,10,20)` | RGB | aparencia | Substrato topo | Manter | identificado |
| `substrate_color_bottom` | `(10,10,20)` | RGB | aparencia | Substrato baixo | Manter | identificado |
| `substrate_border_enabled` | `True` | bool | aparencia | Borda | Manter | identificado |
| `substrate_border_color` | `(40,200,40)` | RGB | aparencia | Cor borda | Manter | identificado |

## Parametros/Estados de Save e UI Encontrados Fora de Defaults

| Nome | Tipo provavel | Onde aparece | Observacao | Status |
|---|---|---|---|---|
| `camera_x` | float | save/load UI | Estado de camera em save | parcialmente identificado |
| `camera_y` | float | save/load UI | Estado de camera em save | parcialmente identificado |
| `camera_zoom` | float | save/load UI | Estado de camera em save | parcialmente identificado |
| `enable_brain_activations` | bool | UI/save | Alias invertido de `disable_brain_activations` | parcialmente identificado |
| `disable_brain_activations` | bool | entities/ui | Desliga ativacoes detalhadas | parcialmente identificado |
| `mem_diag_enable` | bool | engine | Diagnostico memoria | parcialmente identificado |
| `mem_diag_interval` | float/int | engine | Intervalo diag memoria | parcialmente identificado |
| `mem_warn_mb` | float/int | engine | Aviso memoria | parcialmente identificado |
| `ui_params.csv` | arquivo | README | Persistencia UI citada | precisa verificacao manual |
| `labels` metadata | objeto/lista | engine/ui/save | Nome, cor, min, max, inicial, grafico | parcialmente identificado |
| `agent_labels` | dict | engine/ui/systems | Label por agente | parcialmente identificado |
| `current_biosim_path` | path | ui | Caminho save atual | parcialmente identificado |

## Metadados de Labels/Especies

Estes itens nao aparecem como defaults simples em `Params`, mas sao importantes para save/load, UI e futura migracao para especies.

| Nome conceitual | Tipo provavel | Onde aparece | Impacto | Migracao | Status |
|---|---|---|---|---|---|
| `label_id` / `species_id` | int | engine/ui/save | Identidade do grupo/especie | Id estavel por especie | parcialmente identificado |
| `label_name` / `species_name` | string | UI/save | Nome exibido e filtro de grupo | Preservar alias `label` | parcialmente identificado |
| `label_color` / `species_color` | RGB | UI/render/save | Cor do corpo/grupo | Deve afetar organismo, nao apenas contorno | parcialmente identificado |
| `label_min` / `species_min` | int | UI/systems | Populacao minima | Migrar para `SpeciesConfig` | parcialmente identificado |
| `label_max` / `species_max` | int | UI/systems | Populacao maxima | Migrar para `SpeciesConfig` | parcialmente identificado |
| `label_initial` / `species_initial` | int | UI/reset | Quantidade inicial ao resetar | Precisa verificacao manual no estado atual | parcialmente identificado |
| `label_show_graph` | bool | UI/grafico | Serie no grafico | Manter como config de UI/metrica | parcialmente identificado |
| `label_genome_ref` | id/path/object | conceito/UI | Genoma associado a especie | Recomendado para C++ | nao rastreado |

## Observacoes de Migracao

- Separar parametros em schemas: `SimulationConfig`, `WorldConfig`, `RenderConfig`, `PhysicsConfig`, `FoodConfig`, `VisionConfig`, `NeuralConfig`, `GenomeConfig`, `SpeciesConfig`, `UiConfig`, `DebugConfig`.
- Preservar nomes atuais como aliases no loader para compatibilidade.
- Parametros por prefixo `bacteria_` e `predator_` devem virar instancias de `SpeciesConfig`, mas o C++ deve conseguir ler saves antigos.
- Parametros globais de rede neural sao configuracao do tipo de cerebro; parametros de camadas/neuronios sao genoma/especie.
- Parametros de UI nao devem resetar quando criar novo arquivo de simulacao, a menos que o usuario escolha resetar preferencias.
