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
            'use_spatial': True,
            'substrate_shape': 'rectangular',  # 'rectangular' ou 'circular'
            'world_w': 1000.0,  # Largura do substrato (retangular)
            'world_h': 700.0,   # Altura do substrato (retangular)
            'substrate_radius': 400.0,  # Raio do substrato (circular)
            'max_deaths_per_step': 1,
            
            # Performance
            'retina_skip': 0,
            'simple_render': False,
            'reuse_spatial_grid': True,
            
            # Comida/substrato
            'food_target': 50,
            'food_min_r': 4.5,
            'food_max_r': 5.0,
            'food_replenish_interval': 0.1,
            
            # Bactérias - população
            'bacteria_count': 150,
            'bacteria_min_r': 6.0,
            'bacteria_max_r': 12.0,
            'bacteria_min_limit': 10,
            'bacteria_max_limit': 300,
            
            # Bactérias - energia (legacy classe fixa - manter campos essenciais apenas)
            'bacteria_initial_mass': 100.0,  # Massa inicial
            'bacteria_death_mass': 50.0,
            'bacteria_split_mass': 150.0,
            
            # Bactérias - movimento
            'bacteria_max_speed': 300.0,
            'bacteria_max_turn': math.pi,
            
            # Bactérias - visão
            'bacteria_vision_radius': 120.0,
            'bacteria_vision_mode': 'frontal',
            'bacteria_retina_count': 18,
            'bacteria_retina_fov_degrees': 180.0,  # Campo de visão total em graus
            'bacteria_show_vision': False,
            'bacteria_retina_see_food': True,
            'bacteria_retina_see_bacteria': False,
            'bacteria_retina_see_predators': False,
            
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
            
            # Predadores - energia
            'predator_initial_mass': 100.0,  # Massa inicial
            'predator_death_mass': 50.0,
            'predator_split_mass': 150.0,
            
            # Predadores - movimento
            'predator_max_speed': 300.0,
            'predator_max_turn': math.pi,
            
            # Predadores - visão
            'predator_vision_radius': 120.0,
            'predator_vision_mode': 'frontal',
            'predator_retina_count': 18,
            'predator_retina_fov_degrees': 180.0,  # Campo de visão total em graus
            'predator_show_vision': False,
            'predator_retina_see_food': True,
            'predator_retina_see_bacteria': True,
            'predator_retina_see_predators': False,
            
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
            
            # UI/Debug
            'show_selected_details': True,
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
        if 'count' in key or 'limit' in key:
            return max(0, int(value))
        elif 'mass' in key or 'radius' in key or key.endswith('_r'):
            return max(0.0, float(value))
        elif 'rate' in key and 'mutation' in key:
            return max(0.0, min(1.0, float(value)))
        elif key in ['time_scale', 'fps']:
            return max(0.1, float(value))
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
                'retina_skip': 2,
                'bacteria_count': 50,
                'predator_count': 5,
            },
            'large_population': {
                **dict(self._data),
                'bacteria_max_limit': 1000,
                'predator_max_limit': 200,
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
        self.last_update_time = 0.0
    
    def update(self, current_foods: list, target_count: int, world_w: float, 
               world_h: float, params: 'Params', dt: float) -> list:
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
        current_count = len(current_foods)
        difference = target_count - current_count
        
        # Acumula dívida baseado na diferença
        replenish_rate = params.get('food_replenish_interval', 0.1)
        debt_increment = (difference * dt) / replenish_rate
        self.food_debt += debt_increment
        
        # Cria comida quando dívida é suficiente
        new_foods = []
        while self.food_debt >= 1.0:
            food = self._create_random_food(current_foods + new_foods, world_w, world_h, params)
            if food:
                new_foods.append(food)
                self.food_debt -= 1.0
            else:
                # Se não conseguiu criar comida, não tenta mais neste frame
                break
        
        # Decaimento natural da dívida para evitar acúmulo excessivo
        self.food_debt = max(0, self.food_debt * 0.99)
        
        return new_foods
    
    def _create_random_food(self, existing_foods: list, world_w: float, 
                           world_h: float, params: 'Params'):
        """Cria comida em posição aleatória válida."""
        from .entities import Food
        import random
        
        min_r = params.get('food_min_r', 4.5)
        max_r = params.get('food_max_r', 5.0)
        
        # Tenta encontrar posição válida
        for attempt in range(50):  # Reduzido de 200 para 50 para performance
            r = random.uniform(min_r, max_r)
            x = random.uniform(r, world_w - r)
            y = random.uniform(r, world_h - r)
            
            # Verifica sobreposição com comida existente
            overlaps = False
            for food in existing_foods:
                if hasattr(food, 'x') and hasattr(food, 'r'):
                    distance = ((food.x - x)**2 + (food.y - y)**2)**0.5
                    if distance < food.r + r + 2:  # Pequena margem
                        overlaps = True
                        break
            
            if not overlaps:
                return Food(x, y, r)
        
        # Se não encontrou posição válida, retorna None
        return None
