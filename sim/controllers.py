"""
Configuração central da simulação com validação e controle de população/comida.
"""
import math
from typing import Any, Dict, Callable, Optional
from copy import deepcopy


class Params:
    """
    Configuração central da simulação com validação, callbacks e perfis.
    Aplica princípio Single Responsibility para gerenciamento de parâmetros.
    """
    
    def __init__(self):
        self._data: Dict[str, Any] = {}
        self._callbacks: Dict[str, list] = {}
        self._setup_defaults()
    
    def _setup_defaults(self):
        """Define valores padrão e configurações iniciais."""
        defaults = {
            # Simulação geral
            'time_scale': 1.0,
            'fps': 60,
            'paused': False,
            # Timing fisico: a escala de tempo acumula tempo simulado, mas
            # cada substep usa dt fixo para preservar a dinamica.
            'physics_steps_per_second': 30,
            'max_physics_steps_per_frame': 8,
            'max_physics_backlog_seconds': 0.25,
            'use_spatial': True,
            'substrate_shape': 'rectangular',  # 'rectangular' ou 'circular'
            'world_w': 1000.0,  # Largura do substrato (retangular)
            'world_h': 700.0,   # Altura do substrato (retangular)
            'substrate_radius': 400.0,  # Raio do substrato (circular)
            'random_seed': -1,  # -1 disables fixed seeding; >=0 makes reset/start reproducible
            'agent_template_name': 'organismo_1',
            'max_deaths_per_step': 5,
            'population_min_rescue_enabled': True,
            
            # Performance
            'retina_skip': 0,
            # Retina vision mode:
            # 'single' = fast centroid approximation, kept as compatibility default.
            # 'fullbody' = geometric ray/body intersection for stricter experiments.
            'retina_vision_mode': 'single',
            'render_enabled': True,
            'simple_render': False,
            'show_spatial_hash': False,
            'render_resolution_scale': 1.0,
            'use_numba_kernels': True,
            'use_numba_batch_retina': False,
            'use_grouped_vision_batches': True,
            'use_persistent_perception_arrays': False,
            # Optional semantic tradeoff: sector vision keeps retina direction
            # but avoids body/ray intersections when groups become very large.
            'retina_high_scale_auto_sector': False,
            'retina_high_scale_sector_min_agents': 800,
            'retina_high_scale_global_sector': False,
            # Angular-bin vision controls. These are only used when
            # retina_vision_mode == 'sector', so the raycast path pays no cost.
            'retina_bins_mode': 'nearest',  # nearest, strongest, sum_saturating, weighted_average
            'retina_bins_distance_subdivisions': 5,
            'retina_bins_distance_distribution': 'near_detail',  # linear, near_detail
            'retina_bins_distance_falloff': 'linear',  # linear, quadratic, step, none
            'retina_bins_projection': 'center',  # center, center_edges, apparent_size
            'retina_bins_candidate_limit': 128,  # 0 = unlimited
            'retina_bins_obstacles_block_vision': False,
            'use_numba_locomotion_energy': False,
            'brain_cache_disable': False,
            'use_numba_brain_forward': False,
            'numba_brain_forward_min_batch': 256,
            'neural_network_type': 'mlp',
            'neural_gate_init': 1.0,
            'neural_gate_min': 0.0,
            'neural_gate_max': 2.0,
            'neural_gate_mutation_rate': -1.0,
            'neural_gate_mutation_strength': -1.0,
            'neural_shortcut_init_std': 0.05,
            'neural_shortcut_scale': 0.25,
            'neural_shortcut_mutation_rate': -1.0,
            'neural_shortcut_mutation_strength': -1.0,
            'neural_rnn_recurrent_init_std': 0.08,
            'neural_rnn_recurrent_scale': 0.35,
            'neural_rnn_memory_decay': 0.6,
            'neural_rnn_state_clip': 1.0,
            'neural_rnn_reset_state_on_copy': True,
            'neural_rnn_mutation_rate': -1.0,
            'neural_rnn_mutation_strength': -1.0,
            'reuse_spatial_grid': True,
            
            # Comida/substrato
            'food_mode': 'instant',  # instant, chunk
            'food_target': 50,
            'food_min_r': 4.5,
            'food_max_r': 5.0,
            'food_replenish_interval': 0.1,
            'food_bite_seconds': 6.0,
            'food_piece_particle_radius': 5.0,
            'food_piece_cluster_radius': 36.0,
            'food_piece_particle_spacing': 0.0,
            'food_piece_replenish_mode': 'spawn_cluster',  # spawn_cluster, grow_existing, grow_particles
            'food_trim_excess_enabled': True,
            'food_trim_max_per_step': 5,
            
            # Bactérias - população
            'bacteria_count': 150,
            'bacteria_min_r': 6.0,
            'bacteria_max_r': 12.0,
            'bacteria_min_limit': 0,
            'bacteria_max_limit': 0,
            
            # Bactérias - energia (modelo contínuo)
            'bacteria_initial_energy': 100.0,  # Energia inicial
            'bacteria_death_energy': 50.0,
            'bacteria_split_energy': 150.0,
            'bacteria_age_death_enabled': False,
            'bacteria_death_age': 3600.0,
            'bacteria_corpse_to_food': False,
            'bacteria_reproduction_min_age': 0.0,
            'bacteria_reproduction_cooldown': 0.0,
            # Bactérias - metabolismo avançado
            'bacteria_metab_v0_cost': 0.5,      # custo mínimo por segundo na velocidade 0
            'bacteria_metab_vmax_cost': 8.0,    # custo por segundo na velocidade vmax
            'bacteria_energy_cap': 400.0,       # limite de armazenamento

            # Bactérias - tamanho fixo
            'bacteria_body_size': 9.0,  # Raio fixo (antes variava com "massa")
            'bacteria_body_shape': 'ellipse',
            
            # Bactérias - movimento
            'bacteria_max_speed': 300.0,
            'bacteria_max_turn': math.pi,
            'bacteria_allow_reverse_locomotion': False,
            'bacteria_movement_mode': 'forward',
            
            # Bactérias - visão
            'bacteria_vision_radius': 120.0,
            'bacteria_vision_mode': 'frontal',
            'bacteria_retina_count': 18,
            'bacteria_retina_fov_degrees': 180.0,  # Campo de visão total em graus
            'bacteria_eye_count': 1,
            'bacteria_eye_angle_degrees': 60.0,
            'bacteria_eye_separation_degrees': 45.0,
            'bacteria_show_vision': False,
            'bacteria_retina_see_food': True,
            'bacteria_retina_see_bacteria': False,
            'bacteria_retina_see_predators': False,
            'bacteria_retina_see_obstacles': False,
            'bacteria_retina_see_all': False,
            'bacteria_retina_see_through_walls': True,
            'bacteria_retina_input_mode': 'distance_only',
            'bacteria_retina_channel_r': False,
            'bacteria_retina_channel_g': False,
            'bacteria_retina_channel_b': False,
            'bacteria_retina_channel_d': True,
            'bacteria_diet_food': True,
            'bacteria_diet_agents': False,
            'bacteria_diet_same_label': False,
            'bacteria_diet_food_efficiency': 1.0,
            'bacteria_diet_agent_efficiency': 0.7,
            
            # Bactérias - rede neural
            'bacteria_hidden_layers': 4,
            'bacteria_neurons_layer_1': 20,
            'bacteria_neurons_layer_2': 20,
            'bacteria_neurons_layer_3': 20,
            'bacteria_neurons_layer_4': 20,
            'bacteria_neurons_layer_5': 0,
            'bacteria_mutation_rate': 0.05,
            'bacteria_mutation_strength': 0.08,
            'bacteria_structural_jitter': 0,
            
            # Predadores - população
            'predators_enabled': False,
            'predator_count': 0,
            'predator_min_r': 10.0,
            'predator_max_r': 18.0,
            'predator_min_limit': 0,
            'predator_max_limit': 100,
            
            # Predadores - energia (modelo contínuo)
            'predator_initial_energy': 100.0,  # Energia inicial
            'predator_death_energy': 50.0,
            'predator_split_energy': 150.0,
            'predator_age_death_enabled': False,
            'predator_death_age': 3600.0,
            'predator_corpse_to_food': False,
            'predator_reproduction_min_age': 0.0,
            'predator_reproduction_cooldown': 0.0,
            # Predadores - metabolismo avançado
            'predator_metab_v0_cost': 1.0,
            'predator_metab_vmax_cost': 15.0,
            'predator_energy_cap': 600.0,

            # Predadores - tamanho fixo
            'predator_body_size': 14.0,
            'predator_body_shape': 'ellipse',
            
            # Predadores - movimento
            'predator_max_speed': 300.0,
            'predator_max_turn': math.pi,
            'predator_allow_reverse_locomotion': False,
            'predator_movement_mode': 'forward',
            
            # Predadores - visão
            'predator_vision_radius': 120.0,
            'predator_vision_mode': 'frontal',
            'predator_retina_count': 18,
            'predator_retina_fov_degrees': 180.0,  # Campo de visão total em graus
            'predator_eye_count': 1,
            'predator_eye_angle_degrees': 60.0,
            'predator_eye_separation_degrees': 45.0,
            'predator_show_vision': False,
            'predator_retina_see_food': True,
            'predator_retina_see_bacteria': True,
            'predator_retina_see_predators': False,
            'predator_retina_see_obstacles': False,
            'predator_retina_see_all': False,
            'predator_retina_see_through_walls': True,
            'predator_retina_input_mode': 'distance_only',
            'predator_retina_channel_r': False,
            'predator_retina_channel_g': False,
            'predator_retina_channel_b': False,
            'predator_retina_channel_d': True,
            'predator_diet_food': False,
            'predator_diet_agents': True,
            'predator_diet_same_label': False,
            'predator_diet_food_efficiency': 1.0,
            'predator_diet_agent_efficiency': 0.7,
            
            # Predadores - rede neural
            'predator_hidden_layers': 2,
            'predator_neurons_layer_1': 16,
            'predator_neurons_layer_2': 8,
            'predator_neurons_layer_3': 0,
            'predator_neurons_layer_4': 0,
            'predator_neurons_layer_5': 0,
            'predator_mutation_rate': 0.05,
            'predator_mutation_strength': 0.08,
            'predator_structural_jitter': 0,
            
            # Física geral
            'agents_inertia': 1.0,  # Inércia global (antes derivada de massa individual)
            # Mantem a locomocao historica por padrao. O modo suave e opt-in
            # porque altera a dinamica evolutiva de experimentos existentes.
            'smooth_locomotion_enabled': False,
            'smooth_linear_inertia_enabled': True,
            'smooth_max_linear_accel': 900.0,
            'smooth_linear_drag_enabled': True,
            'smooth_linear_drag': 0.75,
            'smooth_angular_inertia_enabled': True,
            'smooth_max_angular_accel': math.pi * 4.0,
            'smooth_angular_drag_enabled': True,
            'smooth_angular_drag': 1.5,
            'render_interpolation_enabled': False,
            'camera_follow_smoothing_enabled': True,
            'camera_follow_smoothing': 10.0,
            'agent_collision_enabled': True,
            'agent_collision_elasticity_enabled': True,
            'agent_collision_restitution': 0.12,
            'agent_collision_velocity_transfer': 0.35,
            'agent_collision_separation': 0.9,
            'agent_collision_max_impulse': 900.0,
            'global_viscosity_enabled': False,
            'global_viscosity_drag': 0.2,
            'movable_chunk_food_enabled': False,
            'chunk_food_collision_enabled': True,
            'chunk_food_adhesion_enabled': True,
            'chunk_food_adhesion_strength': 0.35,
            'chunk_food_mass_scale': 1.0,
            'chunk_food_drag': 1.6,
            'chunk_food_push_strength': 0.45,
            'brownian_motion_enabled': False,
            'brownian_motion_strength': 3.0,
            'allow_reverse_locomotion': False,
            'reproduction_min_age': 0.0,
            'reproduction_cooldown': 0.0,

            # UI/Debug
            'show_selected_details': True,
            'show_metrics_chart': False,
            'neural_view_dense_layout': 'fixed',
            'camera_follow_selected_agent': True,
            'metrics_chart_sample_seconds': 5,
            'debug_tracebacks': False,
            'diagnostic_heartbeat_minutes': 1.0,
            'save_recovery_on_close': True,
            # Autosave usa nomes internos antigos para manter compatibilidade
            # com snapshots/configuracoes criados antes da troca de nome.
            'auto_export_substrate': False,
            'auto_export_interval_minutes': 10.0,
            'export_substrate_include_brain_activations': False,
            'export_substrate_pretty_json': False,
            # Debug toggles
            'debug_reproduction_color': False,
            # Colors (RGB tuples)
            'substrate_bg_color': (10, 10, 20),
            'background_gradient_enabled': False,
            'background_color_top': (10, 10, 20),
            'background_color_bottom': (10, 10, 20),
            'substrate_gradient_enabled': False,
            'substrate_color_top': (10, 10, 20),
            'substrate_color_bottom': (10, 10, 20),
            'substrate_border_enabled': True,
            'substrate_border_color': (40, 200, 40),
            'food_color': (220, 30, 30),
            'bacteria_color': (220, 220, 220),
            'predator_color': (80, 120, 220),
        }
        
        self._data.update(defaults)
    
    def get(self, key: str, default=None) -> Any:
        """Obtém valor de parâmetro."""
        return self._data.get(key, default)
    
    def set(self, key: str, value: Any, validate: bool = True):
        """
        Define valor de parâmetro com validação opcional e callbacks.
        
        Args:
            key: Nome do parâmetro
            value: Valor a definir
            validate: Se deve validar o valor (padrão True)
        """
        if validate:
            value = self._validate_param(key, value)
        
        old_value = self._data.get(key)
        self._data[key] = value
        
        # Executa callbacks se valor mudou
        if old_value != value and key in self._callbacks:
            for callback in self._callbacks[key]:
                try:
                    callback(key, old_value, value)
                except Exception as e:
                    print(f"Erro em callback para {key}: {e}")
    
    def _validate_param(self, key: str, value: Any) -> Any:
        """Valida e clamp valores de parâmetros."""
        # Validações básicas por padrão de nome
        if key == 'random_seed':
            try:
                return int(float(value))
            except (TypeError, ValueError):
                return -1
        if key == 'food_mode':
            value = str(value)
            if value in {'chunk', 'pieces', 'pedacos', 'pedaços'}:
                return 'chunk'
            return 'instant'
        if key == 'food_piece_replenish_mode':
            value = str(value)
            return value if value in {'spawn_cluster', 'grow_existing', 'grow_particles'} else 'spawn_cluster'
        if key == 'retina_bins_mode':
            value = str(value)
            return value if value in {'nearest', 'strongest', 'sum_saturating', 'weighted_average'} else 'nearest'
        if key == 'retina_bins_distance_distribution':
            value = str(value)
            return value if value in {'linear', 'near_detail'} else 'near_detail'
        if key == 'retina_bins_distance_falloff':
            value = str(value)
            return value if value in {'linear', 'quadratic', 'step', 'none'} else 'linear'
        if key == 'retina_bins_projection':
            value = str(value)
            return value if value in {'center', 'center_edges', 'apparent_size'} else 'center'
        if key == 'neural_network_type':
            value = str(value)
            return value if value in {'mlp', 'gated_mlp', 'shortcut_mlp', 'modulated_mlp', 'simple_rnn'} else 'mlp'
        if key in {'food_bite_seconds'}:
            return max(0.05, float(value))
        if key in {'food_piece_particle_radius', 'food_piece_cluster_radius'}:
            return max(0.1, float(value))
        if key == 'food_piece_particle_spacing':
            return max(0.0, float(value))
        if key in ['reproduction_min_age', 'reproduction_cooldown'] or key.endswith('_death_age') or key.endswith('_reproduction_min_age') or key.endswith('_reproduction_cooldown'):
            return max(0.0, float(value))
        if 'count' in key or 'limit' in key:
            return max(0, int(value))
        elif 'energy' in key or 'radius' in key or key.endswith('_r'):
            return max(0.0, float(value))
        elif 'rate' in key and 'mutation' in key:
            return max(0.0, min(1.0, float(value)))
        elif key in ['time_scale', 'fps']:
            return max(0.1, float(value))
        elif key in ['physics_steps_per_second', 'max_physics_steps_per_frame', 'retina_high_scale_sector_min_agents', 'numba_brain_forward_min_batch']:
            return max(1, int(float(value)))
        elif key == 'retina_bins_distance_subdivisions':
            return max(1, min(99, int(float(value))))
        elif key == 'retina_bins_candidate_limit':
            return max(0, int(float(value)))
        elif key == 'max_physics_backlog_seconds':
            return max(0.0, float(value))
        elif key in {
            'agents_inertia',
            'smooth_max_linear_accel',
            'smooth_linear_drag',
            'smooth_max_angular_accel',
            'smooth_angular_drag',
            'camera_follow_smoothing',
            'agent_collision_restitution',
            'agent_collision_velocity_transfer',
            'agent_collision_separation',
            'agent_collision_max_impulse',
            'global_viscosity_drag',
            'chunk_food_adhesion_strength',
            'chunk_food_mass_scale',
            'chunk_food_drag',
            'chunk_food_push_strength',
            'brownian_motion_strength',
            'neural_gate_init',
            'neural_gate_min',
            'neural_gate_max',
            'neural_shortcut_init_std',
            'neural_shortcut_scale',
            'neural_rnn_recurrent_init_std',
            'neural_rnn_recurrent_scale',
            'neural_rnn_state_clip',
        }:
            return max(0.0, float(value))
        elif key in {'neural_gate_mutation_rate', 'neural_gate_mutation_strength', 'neural_shortcut_mutation_rate', 'neural_shortcut_mutation_strength', 'neural_rnn_mutation_rate', 'neural_rnn_mutation_strength'}:
            return max(-1.0, float(value))
        elif key == 'neural_rnn_memory_decay':
            return max(0.0, min(0.999, float(value)))
        elif key == 'render_resolution_scale':
            return max(1.0, min(3.0, float(value)))
        elif key.endswith('_fov_degrees'):
            return max(1.0, min(360.0, float(value)))
        else:
            return value
    
    def add_callback(self, key: str, callback: Callable[[str, Any, Any], None]):
        """Adiciona callback para mudanças em parâmetro específico."""
        if key not in self._callbacks:
            self._callbacks[key] = []
        self._callbacks[key].append(callback)
    
    def remove_callback(self, key: str, callback: Callable[[str, Any, Any], None]):
        """Remove callback de parâmetro específico."""
        if key in self._callbacks and callback in self._callbacks[key]:
            self._callbacks[key].remove(callback)
    
    def get_profile(self, name: str) -> Dict[str, Any]:
        """Obtém perfil de parâmetros predefinido."""
        profiles = {
            'default': dict(self._data),
            'performance': {
                **dict(self._data),
                'simple_render': True,
                'retina_skip': 5,
                'physics_steps_per_second': 30,
                'max_physics_steps_per_frame': 8,
                'max_physics_backlog_seconds': 0.25,
                'bacteria_count': 50,
                'predator_count': 5,
            },
            'large_population': {
                **dict(self._data),
                'bacteria_max_limit': 0,
                'predator_max_limit': 0,
                'use_spatial': True,
                'simple_render': True,
            }
        }
        return profiles.get(name, profiles['default'])
    
    def apply_profile(self, name: str):
        """Aplica perfil de parâmetros."""
        profile = self.get_profile(name)
        for key, value in profile.items():
            self.set(key, value, validate=False)
    
    def copy(self) -> 'Params':
        """Cria cópia dos parâmetros."""
        new_params = Params()
        new_params._data = deepcopy(self._data)
        new_params._callbacks = {}  # Não copia callbacks
        return new_params


class PopulationController:
    """
    Controlador de população para manter limites min/máx suaves.
    """
    
    def apply_limits(self, entities: list, min_limit: int, max_limit: int, 
                    entity_type: str = "entity") -> list:
        """
        Aplica limites de população de forma suave.
        
        Args:
            entities: Lista de entidades (bacteria/predators)
            min_limit: Limite mínimo de entidades
            max_limit: Limite máximo de entidades  
            entity_type: Tipo da entidade para logging
            
        Returns:
            Lista com entidades após aplicação dos limites
        """
        current_count = len(entities)
        
        # Limite máximo: remove excesso
        if current_count > max_limit:
            # Remove entidades mais fracas (menor massa)
            entities_sorted = sorted(entities, key=lambda e: getattr(e, 'm', 0))
            excess = current_count - max_limit
            entities = entities_sorted[excess:]
            
        # Limite mínimo: previne morte se abaixo do mínimo
        elif current_count <= min_limit:
            # Garante massa mínima para sobrevivência
            min_survival_mass = 60.0  # Um pouco acima do death_mass
            for entity in entities:
                if hasattr(entity, 'm') and entity.m < min_survival_mass:
                    entity.set_mass(min_survival_mass)
        
        return entities


class FoodController:
    """
    Controlador inteligente de comida com sistema de dívida.
    Evita oscilações bruscas na quantidade de comida.
    """
    
    def __init__(self):
        self.food_debt = 0.0  # Dívida de comida para criação suave
        self.food_excess_debt = 0.0
        self.food_energy_debt = 0.0
        self.last_foods_added = 0
        self.last_foods_removed = 0
        self.last_update_time = 0.0
        self._next_chunk_id = 1

    def _new_chunk_id(self) -> int:
        chunk_id = int(self._next_chunk_id)
        self._next_chunk_id += 1
        return chunk_id

    def note_food_energy_consumed(self, amount: float):
        try:
            self.food_energy_debt += max(0.0, float(amount))
        except Exception:
            pass
    
    def update(self, current_foods: list, target_count: int, world_w: float,
               world_h: float, params: 'Params', dt: float, obstacle_map=None, agents: list | None = None) -> list:
        """
        Atualiza sistema de comida com controle PID simplificado.
        
        Args:
            current_foods: Lista atual de comida
            target_count: Quantidade desejada de comida
            world_w: Largura do mundo
            world_h: Altura do mundo  
            params: Parâmetros da simulação
            dt: Delta time em segundos
            
        Returns:
            Lista de novas comidas a criar
        """
        target_count = max(0, int(target_count))
        current_count = len(current_foods)
        self.last_foods_added = 0
        self.last_foods_removed = 0

        if current_count > target_count and params.get('food_trim_excess_enabled', True):
            self.food_excess_debt += ((current_count - target_count) * dt) / max(1e-6, params.get('food_replenish_interval', 0.1))
            max_remove = max(0, int(params.get('food_trim_max_per_step', 5)))
            while self.food_excess_debt >= 1.0 and len(current_foods) > target_count and self.last_foods_removed < max_remove:
                self._remove_lowest_energy_food(current_foods)
                self.food_excess_debt -= 1.0
                self.last_foods_removed += 1
        else:
            self.food_excess_debt = max(0.0, self.food_excess_debt * 0.99)

        current_count = len(current_foods)
        difference = target_count - current_count
        if str(params.get('food_mode', 'instant')) == 'chunk':
            return self._update_piece_food(
                current_foods, target_count, difference, world_w, world_h,
                params, dt, obstacle_map=obstacle_map, agents=agents,
            )
        
        # Acumula dívida baseado na diferença
        replenish_rate = params.get('food_replenish_interval', 0.1)
        debt_increment = (difference * dt) / replenish_rate
        self.food_debt += debt_increment
        
        # Cria comida quando dívida é suficiente
        new_foods = []
        while self.food_debt >= 1.0:
            food = self._create_random_food(current_foods + new_foods, world_w, world_h, params, obstacle_map=obstacle_map)
            if food:
                new_foods.append(food)
                self.last_foods_added += 1
                self.food_debt -= 1.0
            else:
                # Se não conseguiu criar comida, não tenta mais neste frame
                break
        
        # Decaimento natural da dívida para evitar acúmulo excessivo
        self.food_debt = max(0, self.food_debt * 0.99)
        
        return new_foods

    def _remove_lowest_energy_food(self, foods: list):
        if not foods:
            return None
        idx = min(
            range(len(foods)),
            key=lambda i: (
                float(getattr(foods[i], 'energy', getattr(foods[i], 'r', 0.0) ** 2)),
                float(getattr(foods[i], 'r', 0.0)),
                float(getattr(foods[i], 'x', 0.0)),
                float(getattr(foods[i], 'y', 0.0)),
            ),
        )
        return foods.pop(idx)

    @staticmethod
    def _piece_params(params: 'Params') -> tuple[float, float, float, str]:
        particle_r = max(0.1, float(params.get('food_piece_particle_radius', params.get('food_max_r', 5.0))))
        cluster_r = max(particle_r, float(params.get('food_piece_cluster_radius', particle_r * 6.0)))
        edge_gap = max(0.0, float(params.get('food_piece_particle_spacing', 0.0)))
        spacing = max(particle_r * 2.0, particle_r * 2.0 + edge_gap)
        mode = str(params.get('food_piece_replenish_mode', 'spawn_cluster'))
        if mode not in {'spawn_cluster', 'grow_existing', 'grow_particles'}:
            mode = 'spawn_cluster'
        return particle_r, cluster_r, spacing, mode

    @staticmethod
    def _food_initial_energy(radius: float) -> float:
        return max(1e-9, float(radius) * float(radius))

    def _estimate_cluster_count(self, params: 'Params', max_count: int | None = None) -> int:
        particle_r, cluster_r, spacing, _mode = self._piece_params(params)
        count = 0
        y = -cluster_r
        while y <= cluster_r + 1e-6:
            x = -cluster_r
            while x <= cluster_r + 1e-6:
                if x * x + y * y <= cluster_r * cluster_r:
                    count += 1
                    if max_count is not None and count >= max_count:
                        return max_count
                x += spacing
            y += spacing
        return max(1, count)

    def _piece_cluster_energy(self, params: 'Params', max_count: int | None = None) -> float:
        particle_r, _cluster_r, _spacing, _mode = self._piece_params(params)
        return self._estimate_cluster_count(params, max_count=max_count) * self._food_initial_energy(particle_r)

    def _make_piece_food(self, x: float, y: float, params: 'Params', radius: float,
                         chunk_id: int | None = None):
        from .entities import Food
        f = Food(x, y, radius, kind='chunk')
        try:
            f.color = tuple(params.get('food_color', f.color))
        except Exception:
            pass
        f.initial_energy = self._food_initial_energy(radius)
        f.energy = f.initial_energy
        f.base_radius = float(radius)
        f.chunk_id = int(chunk_id if chunk_id is not None else self._new_chunk_id())
        return f

    @staticmethod
    def _piece_inside_world(x: float, y: float, r: float, world_w: float, world_h: float,
                            shape: str, radius_sub: float) -> bool:
        if shape == 'circular':
            return math.hypot(x - world_w / 2, y - world_h / 2) <= (radius_sub - r)
        return r <= x <= world_w - r and r <= y <= world_h - r

    @staticmethod
    def _cluster_clear_of_agents(cx: float, cy: float, cluster_r: float, agents: list | None) -> bool:
        if not agents:
            return True
        clearance = max(10.0, cluster_r * 0.35)
        limit_base = cluster_r + clearance
        for agent in agents:
            ar = float(getattr(agent, 'r', 0.0))
            dx = cx - float(getattr(agent, 'x', 0.0))
            dy = cy - float(getattr(agent, 'y', 0.0))
            limit = limit_base + ar
            if dx * dx + dy * dy <= limit * limit:
                return False
        return True

    def _piece_overlaps_food(self, existing_foods: list, x: float, y: float, r: float, margin: float) -> bool:
        for food in existing_foods:
            if hasattr(food, 'x') and hasattr(food, 'r'):
                total = float(getattr(food, 'r', 0.0)) + r + margin
                dx = float(food.x) - x
                dy = float(food.y) - y
                if dx * dx + dy * dy < total * total:
                    return True
        return False

    def _create_piece_cluster(self, existing_foods: list, world_w: float, world_h: float,
                              params: 'Params', max_count: int, obstacle_map=None,
                              agents: list | None = None, near_existing: bool = False) -> list:
        import random

        particle_r, cluster_r, spacing, _mode = self._piece_params(params)
        shape = params.get('substrate_shape', 'rectangular')
        radius_sub = params.get('substrate_radius', min(world_w, world_h) / 2)
        max_count = max(1, int(max_count))
        anchors = [food for food in existing_foods if str(getattr(food, 'kind', 'chunk')) == 'chunk']

        fallback_chunk_id = self._new_chunk_id()
        for _attempt in range(80):
            chunk_id = fallback_chunk_id
            if near_existing and anchors:
                anchor = random.choice(anchors)
                anchor_chunk = int(getattr(anchor, 'chunk_id', 0) or 0)
                if anchor_chunk <= 0:
                    anchor_chunk = self._new_chunk_id()
                    try:
                        anchor.chunk_id = anchor_chunk
                    except Exception:
                        pass
                chunk_id = anchor_chunk
                ang = random.random() * 2 * math.pi
                dist = float(getattr(anchor, 'r', particle_r)) + cluster_r * random.uniform(0.6, 1.1)
                cx = float(anchor.x) + math.cos(ang) * dist
                cy = float(anchor.y) + math.sin(ang) * dist
            elif shape == 'circular':
                ang = random.random() * 2 * math.pi
                usable = max(0.0, radius_sub - cluster_r)
                rad = (random.random() ** 0.5) * usable
                cx = world_w / 2 + math.cos(ang) * rad
                cy = world_h / 2 + math.sin(ang) * rad
            else:
                cx = random.uniform(cluster_r, max(cluster_r, world_w - cluster_r))
                cy = random.uniform(cluster_r, max(cluster_r, world_h - cluster_r))

            if not self._cluster_clear_of_agents(cx, cy, cluster_r, agents):
                continue

            created = []
            y = -cluster_r
            while y <= cluster_r + 1e-6 and len(created) < max_count:
                x = -cluster_r
                while x <= cluster_r + 1e-6 and len(created) < max_count:
                    if x * x + y * y <= cluster_r * cluster_r:
                        edge_gap = max(0.0, float(params.get('food_piece_particle_spacing', 0.0)))
                        jitter = min(edge_gap * 0.25, particle_r * 0.3)
                        px = cx + x + random.uniform(-jitter, jitter)
                        py = cy + y + random.uniform(-jitter, jitter)
                        if not self._piece_inside_world(px, py, particle_r, world_w, world_h, shape, radius_sub):
                            x += spacing
                            continue
                        if obstacle_map is not None and obstacle_map.circle_overlaps(px, py, particle_r):
                            x += spacing
                            continue
                        if self._piece_overlaps_food(existing_foods + created, px, py, particle_r, margin=0.0):
                            x += spacing
                            continue
                        created.append(self._make_piece_food(px, py, params, particle_r, chunk_id=chunk_id))
                    x += spacing
                y += spacing
            if created:
                return created
        return []

    def _create_piece_particle_near_existing(self, existing_foods: list, world_w: float, world_h: float,
                                             params: 'Params', obstacle_map=None) -> list:
        import random

        particle_r, _cluster_r, _spacing, _mode = self._piece_params(params)
        shape = params.get('substrate_shape', 'rectangular')
        radius_sub = params.get('substrate_radius', min(world_w, world_h) / 2)
        anchors = [food for food in existing_foods if str(getattr(food, 'kind', 'chunk')) == 'chunk']
        if not anchors:
            return []
        for _attempt in range(80):
            anchor = random.choice(anchors)
            anchor_chunk = int(getattr(anchor, 'chunk_id', 0) or 0)
            if anchor_chunk <= 0:
                anchor_chunk = self._new_chunk_id()
                try:
                    anchor.chunk_id = anchor_chunk
                except Exception:
                    pass
            ang = random.random() * 2 * math.pi
            edge_gap = max(0.0, float(params.get('food_piece_particle_spacing', 0.0)))
            spacing = float(getattr(anchor, 'r', particle_r)) + particle_r + edge_gap
            x = float(anchor.x) + math.cos(ang) * spacing
            y = float(anchor.y) + math.sin(ang) * spacing
            if not self._piece_inside_world(x, y, particle_r, world_w, world_h, shape, radius_sub):
                continue
            if obstacle_map is not None and obstacle_map.circle_overlaps(x, y, particle_r):
                continue
            if self._piece_overlaps_food(existing_foods, x, y, particle_r, margin=0.0):
                continue
            return [self._make_piece_food(x, y, params, particle_r, chunk_id=anchor_chunk)]
        return []

    def _update_piece_food(self, current_foods: list, target_count: int, difference: int,
                           world_w: float, world_h: float, params: 'Params', dt: float,
                           obstacle_map=None, agents: list | None = None) -> list:
        if target_count <= 0 or difference <= 0:
            self.food_debt = max(0.0, self.food_debt * 0.99)
            return []

        _particle_r, _cluster_r, _spacing, replenish_mode = self._piece_params(params)
        cluster_count = self._estimate_cluster_count(params, max_count=difference)
        cluster_energy = self._piece_cluster_energy(params, max_count=cluster_count)

        should_spawn = False
        near_existing = False
        if replenish_mode == 'grow_particles' and current_foods:
            self.food_debt += (difference * dt) / max(1e-6, params.get('food_replenish_interval', 0.1))
            max_new = min(difference, int(self.food_debt), 8)
            created = []
            for _ in range(max_new):
                piece = self._create_piece_particle_near_existing(
                    current_foods + created, world_w, world_h, params, obstacle_map=obstacle_map
                )
                if not piece:
                    break
                created.extend(piece)
            if created:
                self.last_foods_added += len(created)
                self.food_debt = max(0.0, self.food_debt - len(created))
            return created
        if replenish_mode == 'grow_existing' and current_foods:
            self.food_debt += (difference * dt) / max(1e-6, params.get('food_replenish_interval', 0.1))
            should_spawn = self.food_debt >= max(1.0, cluster_count * 0.5)
            near_existing = True
        elif not current_foods:
            self.food_debt += (difference * dt) / max(1e-6, params.get('food_replenish_interval', 0.1))
            should_spawn = self.food_debt >= 1.0
        else:
            should_spawn = self.food_energy_debt >= cluster_energy

        if not should_spawn:
            return []

        created = self._create_piece_cluster(
            current_foods, world_w, world_h, params,
            max_count=min(difference, cluster_count),
            obstacle_map=obstacle_map,
            agents=agents,
            near_existing=near_existing,
        )
        if created:
            self.last_foods_added += len(created)
            self.food_debt = max(0.0, self.food_debt - max(1.0, len(created)))
            if current_foods:
                spent = sum(float(getattr(food, 'initial_energy', food.r * food.r)) for food in created)
                self.food_energy_debt = max(0.0, self.food_energy_debt - spent)
        return created
    
    def _create_random_food(self, existing_foods: list, world_w: float,
                           world_h: float, params: 'Params', obstacle_map=None):
        """Cria comida em posição aleatória válida."""
        from .entities import Food
        import random
        
        min_r = params.get('food_min_r', 4.5)
        max_r = params.get('food_max_r', 5.0)
        shape = params.get('substrate_shape', 'rectangular')
        radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
        cx = world_w / 2
        cy = world_h / 2
        food_mode = 'chunk' if str(params.get('food_mode', 'instant')) == 'chunk' else 'instant'

        def _inside_world(x: float, y: float, r: float) -> bool:
            if shape == 'circular':
                return math.hypot(x - cx, y - cy) <= (radius_sub - r)
            return r <= x <= world_w - r and r <= y <= world_h - r

        def _overlaps_food(x: float, y: float, r: float, margin: float = 2.0) -> bool:
            for food in existing_foods:
                if hasattr(food, 'x') and hasattr(food, 'r'):
                    distance = math.hypot(food.x - x, food.y - y)
                    if distance < food.r + r + margin:
                        return True
            return False

        if food_mode == 'chunk':
            anchors = [food for food in existing_foods if str(getattr(food, 'kind', 'chunk')) == 'chunk']
            if anchors:
                for _ in range(80):
                    r = random.uniform(min_r, max_r)
                    anchor = random.choice(anchors)
                    anchor_chunk = int(getattr(anchor, 'chunk_id', 0) or 0)
                    if anchor_chunk <= 0:
                        anchor_chunk = self._new_chunk_id()
                        try:
                            anchor.chunk_id = anchor_chunk
                        except Exception:
                            pass
                    ang = random.random() * 2 * math.pi
                    edge_gap = max(0.0, float(params.get('food_piece_particle_spacing', 0.0)))
                    spacing = float(getattr(anchor, 'r', r)) + r + edge_gap
                    x = float(anchor.x) + math.cos(ang) * spacing
                    y = float(anchor.y) + math.sin(ang) * spacing
                    if not _inside_world(x, y, r):
                        continue
                    if obstacle_map is not None and obstacle_map.circle_overlaps(x, y, r):
                        continue
                    if _overlaps_food(x, y, r, margin=0.0):
                        continue
                    f = Food(x, y, r, kind=food_mode)
                    try:
                        f.color = tuple(params.get('food_color', f.color))
                    except Exception:
                        pass
                    f.initial_energy = max(1e-9, float(getattr(f, 'energy', r * r)))
                    f.base_radius = float(r)
                    f.chunk_id = anchor_chunk
                    return f
        
        # Tenta encontrar posição válida
        for attempt in range(80):  # um pouco mais tentativas para círculo
            r = random.uniform(min_r, max_r)
            if shape == 'circular':
                ang = random.random() * 2*math.pi
                rad = (random.random() ** 0.5) * (radius_sub - r)
                x = cx + math.cos(ang) * rad
                y = cy + math.sin(ang) * rad
            else:
                x = random.uniform(r, world_w - r)
                y = random.uniform(r, world_h - r)
            
            # Verifica se está dentro do círculo (segurança extra)
            if not _inside_world(x, y, r):
                continue

            if obstacle_map is not None and obstacle_map.circle_overlaps(x, y, r):
                continue
            
            # Verifica sobreposição com comida existente
            overlaps = _overlaps_food(x, y, r)

            if not overlaps:
                f = Food(x, y, r, kind=food_mode)
                try:
                    f.color = tuple(params.get('food_color', f.color))
                except Exception:
                    pass
                f.initial_energy = max(1e-9, float(getattr(f, 'energy', r * r)))
                f.base_radius = float(r)
                if food_mode == 'chunk':
                    f.chunk_id = self._new_chunk_id()
                return f

        # Se não encontrou posição válida, retorna None
        return None
