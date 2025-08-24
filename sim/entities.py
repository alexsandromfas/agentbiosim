"""
Entidades da simulação: Agent (base), Bacteria, Predator, Food.
Usando herança onde há comportamento compartilhado e composição para capacidades.
"""
import math
import random
from abc import ABC, abstractmethod
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    from .brain import IBrain
    from .sensors import RetinaSensor
    from .actuators import Locomotion, EnergyModel
    from .world import World
    from .sensors import SceneQuery
    from .controllers import Params


class Entity(ABC):
    """
    Classe base abstrata para todas as entidades da simulação.
    Define interface mínima comum.
    """
    
    def __init__(self, x: float, y: float, r: float, color: tuple):
        self.x = x
        self.y = y
        self.r = r
        self.color = color
    
    @abstractmethod
    def draw(self, renderer):
        """Desenha a entidade usando o renderer fornecido."""
        pass


class Food(Entity):
    """
    Comida simples - apenas posição, tamanho e massa.
    """
    
    def __init__(self, x: float, y: float, r: float):
        super().__init__(x, y, r, (220, 30, 30))  # Cor vermelha
        self.mass = r * r  # Massa proporcional à área
    
    def draw(self, renderer):
        """Desenha comida como círculo simples."""
        renderer.draw_food(self)


class Agent(Entity):
    """
    Classe base para agentes inteligentes (Bacteria, Predator).
    
    Usa composição para cérebro, sensores, atuadores e modelo energético.
    Herança apenas para comportamentos realmente compartilhados.
    """
    
    def __init__(self, x: float, y: float, r: float, color: tuple,
                 brain: 'IBrain', sensor: 'RetinaSensor', 
                 locomotion: 'Locomotion', energy: 'EnergyModel',
                 angle: Optional[float] = None):
        super().__init__(x, y, r, color)
        
        # Estado físico
        self.angle = angle if angle is not None else random.uniform(0, math.pi * 2)
        self.vx = 0.0
        self.vy = 0.0
        self.m = r * r  # Massa inicial baseada no raio
        self.age = 0.0
        self.selected = False
        
        # Componentes (composição)
        self.brain = brain
        self.sensor = sensor
        self.locomotion = locomotion
        self.energy = energy
        
        # Estado para debugging/visualização
        self.last_brain_output = []
        self.last_brain_activations = []
    
    def update(self, dt: float, world: 'World', scene: 'SceneQuery', params: 'Params'):
        """
        Atualiza agente por um passo de tempo.
        
        Sequência: sense -> think -> act -> energy
        """
        self.age += dt
        
        # 1. Sensoriamento
        sensor_inputs = self.sensor.sense(self, scene, params)

        # Ajuste dinâmico do tamanho de entrada da rede caso número de retinas mude
        expected_input_size = self.brain.sizes[0] if hasattr(self.brain, 'sizes') else len(sensor_inputs)
        if len(sensor_inputs) != expected_input_size and hasattr(self.brain, 'resize_input'):
            self.brain.resize_input(len(sensor_inputs))
        
        # 2. Processamento neural
        brain_outputs = self.brain.forward(sensor_inputs)
        self.last_brain_output = list(brain_outputs)
        self.last_brain_activations = self.brain.activations(sensor_inputs)
        
        # 3. Atuação (movimento)
        self.locomotion.step(self, brain_outputs, dt, world, params)
        
        # 4. Modelo energético
        self.energy.apply(self, dt, params)
    
    def speed(self) -> float:
        """Calcula velocidade atual do agente."""
        return math.hypot(self.vx, self.vy)
    
    def set_mass(self, new_mass: float):
        """
        Define nova massa e atualiza raio correspondente.
        Mantém massa mínima para evitar divisão por zero.
        """
        self.m = max(0.1, new_mass)
        self.r = math.sqrt(max(0.25, self.m))
    
    def can_reproduce(self, params: 'Params') -> bool:
        """Verifica se pode se reproduzir baseado na massa."""
        return self.energy.can_reproduce(self)
    
    def should_die(self, params: 'Params') -> bool:
        """Verifica se deve morrer baseado na energia."""
        return self.energy.should_die(self)
    
    def reproduce(self, params: 'Params') -> 'Agent':
        """
        Cria filho através de reprodução assexuada.
        
        Returns:
            Novo agente (mesmo tipo da classe)
        """
        # Prepara reprodução (divide massa)
        child_mass = self.energy.prepare_reproduction(self)
        
        # Posição e velocidade do filho
        child_angle = self.angle + random.uniform(-0.5, 0.5)
        child_speed = self.speed() * 0.5 + random.uniform(-30, 30)
        child_x = self.x + math.cos(child_angle) * self.r * 0.5
        child_y = self.y + math.sin(child_angle) * self.r * 0.5
        
        # Cria cérebro do filho (cópia com mutação)
        child_brain = self.brain.copy()
        child_brain.mutate(
            rate=self._get_mutation_rate(params),
            strength=self._get_mutation_strength(params),
            structural_jitter=self._get_structural_jitter(params)
        )
        
        # Cria componentes do filho
        child_sensor = self._create_child_sensor(params)
        child_locomotion = self._create_child_locomotion(params)
        child_energy = self._create_child_energy_model(params)
        
        # Cria filho
        child = self.__class__(
            child_x, child_y, math.sqrt(child_mass),
            child_brain, child_sensor, child_locomotion, child_energy,
            child_angle
        )
        
        # Define velocidade inicial do filho
        child.vx = math.cos(child_angle) * child_speed
        child.vy = math.sin(child_angle) * child_speed
        
        return child
    
    def draw(self, renderer):
        """Desenha agente usando renderer."""
        renderer.draw_agent(self)
    
    # Métodos abstratos para subclasses customizarem
    def _get_mutation_rate(self, params: 'Params') -> float:
        """Taxa de mutação específica do tipo de agente."""
        raise NotImplementedError
    
    def _get_mutation_strength(self, params: 'Params') -> float:
        """Força de mutação específica do tipo de agente."""
        raise NotImplementedError
    
    def _get_structural_jitter(self, params: 'Params') -> int:
        """Jitter estrutural específico do tipo de agente."""
        raise NotImplementedError
    
    def _create_child_sensor(self, params: 'Params') -> 'RetinaSensor':
        """Cria sensor para o filho."""
        raise NotImplementedError
    
    def _create_child_locomotion(self, params: 'Params') -> 'Locomotion':
        """Cria sistema de locomoção para o filho."""
        raise NotImplementedError
    
    def _create_child_energy_model(self, params: 'Params') -> 'EnergyModel':
        """Cria modelo energético para o filho."""
        raise NotImplementedError


class Bacteria(Agent):
    """
    Bactéria - agente que come comida e se reproduz.
    Implementação específica dos parâmetros de bactéria.
    """
    
    def __init__(self, x: float, y: float, r: float, brain: 'IBrain',
                 sensor: 'RetinaSensor', locomotion: 'Locomotion', 
                 energy: 'EnergyModel', angle: Optional[float] = None):
        color = (220, 220, 220)  # Cor branca/cinza claro
        super().__init__(x, y, r, color, brain, sensor, locomotion, energy, angle)
        self.is_predator = False
    
    def _get_mutation_rate(self, params: 'Params') -> float:
        return params.get('bacteria_mutation_rate', 0.05)
    
    def _get_mutation_strength(self, params: 'Params') -> float:
        return params.get('bacteria_mutation_strength', 0.08)
    
    def _get_structural_jitter(self, params: 'Params') -> int:
        return params.get('bacteria_structural_jitter', 0)
    
    def _create_child_sensor(self, params: 'Params') -> 'RetinaSensor':
        from .sensors import RetinaSensor
        return RetinaSensor(
            retina_count=params.get('bacteria_retina_count', 18),
            vision_radius=params.get('bacteria_vision_radius', 120.0),
            fov_degrees=params.get('bacteria_retina_fov_degrees', 180.0),
            skip=params.get('retina_skip', 0),
            see_food=params.get('bacteria_retina_see_food', True),
            see_bacteria=params.get('bacteria_retina_see_bacteria', False),
            see_predators=params.get('bacteria_retina_see_predators', False)
        )
    
    def _create_child_locomotion(self, params: 'Params') -> 'Locomotion':
        from .actuators import Locomotion
        return Locomotion(
            max_speed=params.get('bacteria_max_speed', 300.0),
            max_turn=params.get('bacteria_max_turn', math.pi)
        )
    
    def _create_child_energy_model(self, params: 'Params') -> 'EnergyModel':
        from .actuators import EnergyModel
        return EnergyModel(
            loss_idle=params.get('bacteria_energy_loss_idle', 0.01),
            loss_move=params.get('bacteria_energy_loss_move', 5.0),
            death_mass=params.get('bacteria_death_mass', 50.0),
            split_mass=params.get('bacteria_split_mass', 150.0)
        )


class Predator(Agent):
    """
    Predador - agente que come bactérias.
    Herda comportamento de Agent mas com parâmetros específicos.
    """
    
    def __init__(self, x: float, y: float, r: float, brain: 'IBrain',
                 sensor: 'RetinaSensor', locomotion: 'Locomotion', 
                 energy: 'EnergyModel', angle: Optional[float] = None):
        color = (80, 120, 220)  # Cor azul
        super().__init__(x, y, r, color, brain, sensor, locomotion, energy, angle)
        self.is_predator = True
    
    def _get_mutation_rate(self, params: 'Params') -> float:
        return params.get('predator_mutation_rate', 0.05)
    
    def _get_mutation_strength(self, params: 'Params') -> float:
        return params.get('predator_mutation_strength', 0.08)
    
    def _get_structural_jitter(self, params: 'Params') -> int:
        return params.get('predator_structural_jitter', 0)
    
    def _create_child_sensor(self, params: 'Params') -> 'RetinaSensor':
        from .sensors import RetinaSensor
        return RetinaSensor(
            retina_count=params.get('predator_retina_count', 18),
            vision_radius=params.get('predator_vision_radius', 120.0),
            fov_degrees=params.get('predator_retina_fov_degrees', 180.0),
            skip=params.get('retina_skip', 0),
            see_food=params.get('predator_retina_see_food', True),
            see_bacteria=params.get('predator_retina_see_bacteria', True),
            see_predators=params.get('predator_retina_see_predators', False)
        )
    
    def _create_child_locomotion(self, params: 'Params') -> 'Locomotion':
        from .actuators import Locomotion
        return Locomotion(
            max_speed=params.get('predator_max_speed', 300.0),
            max_turn=params.get('predator_max_turn', math.pi)
        )
    
    def _create_child_energy_model(self, params: 'Params') -> 'EnergyModel':
        from .actuators import EnergyModel
        return EnergyModel(
            loss_idle=params.get('predator_energy_loss_idle', 0.01),
            loss_move=params.get('predator_energy_loss_move', 5.0),
            death_mass=params.get('predator_death_mass', 50.0),
            split_mass=params.get('predator_split_mass', 150.0)
        )


# Factory functions para criar entidades com parâmetros
def create_random_bacteria(existing_entities: list, params: 'Params', 
                          world_w: float, world_h: float,
                          at: Optional[tuple] = None) -> Bacteria:
    """Cria bactéria aleatória evitando sobreposições."""
    from .brain import NeuralNet
    from .sensors import RetinaSensor  
    from .actuators import Locomotion, EnergyModel
    
    min_r = params.get('bacteria_min_r', 6.0)
    max_r = params.get('bacteria_max_r', 12.0)
    shape = params.get('substrate_shape', 'rectangular')
    radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
    cx = world_w/2
    cy = world_h/2
    for _ in range(300):
        r = random.uniform(min_r, max_r)
        if at is None:
            if shape == 'circular':
                ang = random.random() * 2*math.pi
                rad = (random.random() ** 0.5) * (radius_sub - r)
                x = cx + math.cos(ang)*rad
                y = cy + math.sin(ang)*rad
            else:
                x = random.uniform(r, world_w - r)
                y = random.uniform(r, world_h - r)
        else:
            x, y = at
        if shape == 'circular' and math.hypot(x-cx, y-cy) > (radius_sub - r):
            continue
        overlaps = False
        for entity in existing_entities:
            if hasattr(entity, 'x') and hasattr(entity, 'r'):
                if math.hypot(entity.x - x, entity.y - y) < entity.r + r:
                    overlaps = True
                    break
        if not overlaps or at is not None:
            break
    else:
        r = random.uniform(min_r, max_r)
        if shape == 'circular':
            ang = random.random() * 2*math.pi
            rad = (random.random() ** 0.5) * (radius_sub - r)
            x = cx + math.cos(ang)*rad
            y = cy + math.sin(ang)*rad
        else:
            x = random.uniform(r, world_w - r)
            y = random.uniform(r, world_h - r)
    
    # Cria componentes
    brain = _create_bacteria_brain(params)
    sensor = _create_bacteria_sensor(params)
    locomotion = _create_bacteria_locomotion(params)
    energy = _create_bacteria_energy_model(params)
    
    # Cria bactéria com massa inicial customizável
    bacterium = Bacteria(x, y, r, brain, sensor, locomotion, energy)
    bacterium.m = params.get('bacteria_initial_mass', r * r)  # Usa massa inicial ou padrão baseado no raio
    
    return bacterium


def create_random_predator(existing_entities: list, params: 'Params',
                          world_w: float, world_h: float,
                          at: Optional[tuple] = None) -> Predator:
    """Cria predador aleatório evitando sobreposições."""
    from .brain import NeuralNet
    from .sensors import RetinaSensor
    from .actuators import Locomotion, EnergyModel
    
    min_r = params.get('predator_min_r', 10.0)
    max_r = params.get('predator_max_r', 18.0)
    shape = params.get('substrate_shape', 'rectangular')
    radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
    cx = world_w/2
    cy = world_h/2
    for _ in range(300):
        r = random.uniform(min_r, max_r)
        if at is None:
            if shape == 'circular':
                ang = random.random() * 2*math.pi
                rad = (random.random() ** 0.5) * (radius_sub - r)
                x = cx + math.cos(ang)*rad
                y = cy + math.sin(ang)*rad
            else:
                x = random.uniform(r, world_w - r)
                y = random.uniform(r, world_h - r)
        else:
            x, y = at
        if shape == 'circular' and math.hypot(x-cx, y-cy) > (radius_sub - r):
            continue
        overlaps = False
        for entity in existing_entities:
            if hasattr(entity, 'x') and hasattr(entity, 'r'):
                if math.hypot(entity.x - x, entity.y - y) < entity.r + r:
                    overlaps = True
                    break
        if not overlaps or at is not None:
            break
    else:
        r = random.uniform(min_r, max_r)
        if shape == 'circular':
            ang = random.random() * 2*math.pi
            rad = (random.random() ** 0.5) * (radius_sub - r)
            x = cx + math.cos(ang)*rad
            y = cy + math.sin(ang)*rad
        else:
            x = random.uniform(r, world_w - r)
            y = random.uniform(r, world_h - r)
    
    # Cria componentes
    brain = _create_predator_brain(params)
    sensor = _create_predator_sensor(params)
    locomotion = _create_predator_locomotion(params)
    energy = _create_predator_energy_model(params)
    
    # Cria predador com massa inicial customizável
    predator = Predator(x, y, r, brain, sensor, locomotion, energy)
    predator.m = params.get('predator_initial_mass', r * r)  # Usa massa inicial ou padrão baseado no raio
    
    return predator


def create_random_food(existing_food: list, params: 'Params',
                      world_w: float, world_h: float,
                      at: Optional[tuple] = None) -> Food:
    """Cria comida aleatória evitando sobreposições."""
    min_r = params.get('food_min_r', 4.5)
    max_r = params.get('food_max_r', 5.0)
    shape = params.get('substrate_shape', 'rectangular')
    radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
    cx = world_w/2
    cy = world_h/2
    for _ in range(300):
        r = random.uniform(min_r, max_r)
        if at is None:
            if shape == 'circular':
                ang = random.random() * 2*math.pi
                rad = (random.random() ** 0.5) * (radius_sub - r)
                x = cx + math.cos(ang)*rad
                y = cy + math.sin(ang)*rad
            else:
                x = random.uniform(r, world_w - r)
                y = random.uniform(r, world_h - r)
        else:
            x, y = at
        if shape == 'circular' and math.hypot(x-cx, y-cy) > (radius_sub - r):
            continue
        overlaps = False
        for food in existing_food:
            if math.hypot(food.x - x, food.y - y) < food.r + r:
                overlaps = True
                break
        if not overlaps or at is not None:
            break
    else:
        r = random.uniform(min_r, max_r)
        if shape == 'circular':
            ang = random.random() * 2*math.pi
            rad = (random.random() ** 0.5) * (radius_sub - r)
            x = cx + math.cos(ang)*rad
            y = cy + math.sin(ang)*rad
        else:
            x = random.uniform(r, world_w - r)
            y = random.uniform(r, world_h - r)
    
    return Food(x, y, r)


# Helper functions para criar componentes
def _create_bacteria_brain(params: 'Params'):
    """Cria cérebro para bactéria baseado nos parâmetros."""
    from .brain import NeuralNet
    
    input_size = params.get('bacteria_retina_count', 18)
    hidden_layers = params.get('bacteria_hidden_layers', 4)
    
    # Coleta neurônios por camada
    layer_sizes = [input_size]
    for i in range(1, 6):  # Até 5 camadas ocultas
        if i <= hidden_layers:
            neurons = params.get(f'bacteria_neurons_layer_{i}', 20)
            if neurons > 0:
                layer_sizes.append(neurons)
        else:
            break
    
    layer_sizes.append(2)  # Saída: [speed, steering]
    
    return NeuralNet(layer_sizes, init_std=1.0)


def _create_bacteria_sensor(params: 'Params'):
    """Cria sensor para bactéria."""
    from .sensors import RetinaSensor
    return RetinaSensor(
        retina_count=params.get('bacteria_retina_count', 18),
        vision_radius=params.get('bacteria_vision_radius', 120.0),
        fov_degrees=params.get('bacteria_retina_fov_degrees', 180.0),
        skip=params.get('retina_skip', 0),
        see_food=params.get('bacteria_retina_see_food', True),
        see_bacteria=params.get('bacteria_retina_see_bacteria', False),
        see_predators=params.get('bacteria_retina_see_predators', False)
    )


def _create_bacteria_locomotion(params: 'Params'):
    """Cria locomoção para bactéria."""
    from .actuators import Locomotion
    return Locomotion(
        max_speed=params.get('bacteria_max_speed', 300.0),
        max_turn=params.get('bacteria_max_turn', math.pi)
    )


def _create_bacteria_energy_model(params: 'Params'):
    """Cria modelo energético para bactéria."""
    from .actuators import EnergyModel
    return EnergyModel(
        loss_idle=params.get('bacteria_energy_loss_idle', 0.01),
        loss_move=params.get('bacteria_energy_loss_move', 5.0),
        death_mass=params.get('bacteria_death_mass', 50.0),
        split_mass=params.get('bacteria_split_mass', 150.0)
    )


def _create_predator_brain(params: 'Params'):
    """Cria cérebro para predador.""" 
    from .brain import NeuralNet
    
    input_size = params.get('predator_retina_count', 18)
    hidden_layers = params.get('predator_hidden_layers', 2)
    
    # Coleta neurônios por camada
    layer_sizes = [input_size]
    for i in range(1, 6):
        if i <= hidden_layers:
            neurons = params.get(f'predator_neurons_layer_{i}', 16 if i == 1 else 8)
            if neurons > 0:
                layer_sizes.append(neurons)
        else:
            break
    
    layer_sizes.append(2)  # Saída: [speed, steering]
    
    return NeuralNet(layer_sizes, init_std=1.0)


def _create_predator_sensor(params: 'Params'):
    """Cria sensor para predador."""
    from .sensors import RetinaSensor
    return RetinaSensor(
        retina_count=params.get('predator_retina_count', 18),
        vision_radius=params.get('predator_vision_radius', 120.0),
        fov_degrees=params.get('predator_retina_fov_degrees', 180.0),
        skip=params.get('retina_skip', 0),
        see_food=params.get('predator_retina_see_food', True),
        see_bacteria=params.get('predator_retina_see_bacteria', True),
        see_predators=params.get('predator_retina_see_predators', False)
    )


def _create_predator_locomotion(params: 'Params'):
    """Cria locomoção para predador."""
    from .actuators import Locomotion
    return Locomotion(
        max_speed=params.get('predator_max_speed', 300.0),
        max_turn=params.get('predator_max_turn', math.pi)
    )


def _create_predator_energy_model(params: 'Params'):
    """Cria modelo energético para predador."""
    from .actuators import EnergyModel
    return EnergyModel(
        loss_idle=params.get('predator_energy_loss_idle', 0.01),
        loss_move=params.get('predator_energy_loss_move', 5.0),
        death_mass=params.get('predator_death_mass', 50.0),
        split_mass=params.get('predator_split_mass', 150.0)
    )
