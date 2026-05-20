import math
import random
from abc import ABC, abstractmethod
from typing import Optional, TYPE_CHECKING
import numpy as np
from .profiler import profile_section, profiler

if TYPE_CHECKING:  # Tipos somente para linting
    from .brain import IBrain
    from .sensors import RetinaSensor, SceneQuery
    from .actuators import Locomotion, EnergyModel
    from .world import World
    from .controllers import Params


def update_agents_batch(agents, dt, world, scene, params, selected_agent=None):
    """Atualiza um grupo de agentes usando processamento em lote.

    Otimização de memória: não armazenamos activations completas para todos
    os agentes a cada frame. Apenas o agente selecionado (se houver) terá
    suas activations calculadas e preservadas para debug/overlay.
    Exportações de substrato/agente recalculam activations on-demand.
    """
    if not agents:
        return
    from .sensors import batch_retina_sense
    # idade
    for a in agents:
        a.age += dt
    # sensoriamento
    with profile_section('agent_sensor'):
        inputs_list = batch_retina_sense(agents, scene, params)
    # resize se necessário
    for ag, inp in zip(agents, inputs_list):
        exp_size = ag.brain.sizes[0] if hasattr(ag.brain, 'sizes') else len(inp)
        if len(inp) != exp_size and hasattr(ag.brain, 'resize_input'):
            ag.brain.resize_input(len(inp))
    # forward (batch outputs)
    with profile_section('agent_brain_forward'):
        arr = np.array(inputs_list, dtype=np.float32)
        try:
            from .brain import forward_many_brains
            outs = forward_many_brains([a.brain for a in agents], arr)
        except Exception:
            outs = np.array([a.brain.forward(v.tolist()) for a, v in zip(agents, arr)], dtype=np.float32)
    # distribuir / atuar / energia (sem activations globais)
    for i, ag in enumerate(agents):
        ag.last_brain_output = outs[i].tolist()
        # Limpamos activations para reduzir pressão de memória (serão preenchidas somente no selecionado)
        if ag is not selected_agent:
            if ag.last_brain_activations:
                ag.last_brain_activations = []
    fast_backend = None
    if bool(params.get('use_numba_kernels', True)) and bool(params.get('use_numba_locomotion_energy', False)):
        with profile_section('agent_locomotion_energy_fast'):
            fast_backend = _apply_fast_locomotion_energy(agents, outs, dt, world, params)
    if fast_backend is None:
        with profile_section('agent_locomotion'):
            for ag in agents:
                ag.locomotion.step(ag, ag.last_brain_output, dt, world, params)
        with profile_section('agent_energy'):
            for ag in agents:
                ag.energy_model.apply(ag, dt, params)
    # Activations somente para agente selecionado. Nao depende do profiler:
    # o profiler mede custo; a UI precisa dos valores mesmo fora de benchmark.
    if selected_agent and not params.get('disable_brain_activations', False):
        try:
            idx = agents.index(selected_agent)
        except ValueError:
            return
        with profile_section('agent_brain_activations_sel'):
            # Usa o mesmo input já calculado para evitar nova leitura de sensores
            sel_inp = inputs_list[idx]
            # Ajuste dinâmico se necessário (já feito acima, mas segurança)
            if hasattr(selected_agent.brain, 'sizes') and len(sel_inp) != selected_agent.brain.sizes[0] and hasattr(selected_agent.brain, 'resize_input'):
                selected_agent.brain.resize_input(len(sel_inp))
            try:
                selected_agent.last_brain_activations = selected_agent.brain.activations(sel_inp)
            except Exception:
                selected_agent.last_brain_activations = []


def _apply_fast_locomotion_energy(agents, outputs, dt, world, params, force_python: bool = False):
    """Apply locomotion and energy through temporary arrays.

    The neural outputs and agent-specific locomotion parameters stay separated
    per agent. This is safe for mixed neural architectures because callers
    already group agents before invoking this function.
    """
    if not agents:
        return None
    try:
        from .fast_kernels import apply_locomotion_energy_arrays, has_numba
    except Exception:
        return None
    if not force_python and not has_numba():
        return None
    outputs_arr = np.asarray(outputs, dtype=np.float64)
    if outputs_arr.ndim != 2 or outputs_arr.shape[1] < 2:
        return None
    first_loc = getattr(agents[0], 'locomotion', None)
    first_mode = getattr(first_loc, 'movement_mode', 'forward')
    first_reverse = bool(getattr(first_loc, 'allow_reverse', False))
    if str(first_mode) != 'forward':
        return None
    for ag in agents:
        loc = getattr(ag, 'locomotion', None)
        if getattr(loc, 'movement_mode', 'forward') != first_mode:
            return None
        if bool(getattr(loc, 'allow_reverse', False)) != first_reverse:
            return None

    n = len(agents)
    x = np.empty(n, dtype=np.float64)
    y = np.empty(n, dtype=np.float64)
    radius = np.empty(n, dtype=np.float64)
    angle = np.empty(n, dtype=np.float64)
    vx = np.empty(n, dtype=np.float64)
    vy = np.empty(n, dtype=np.float64)
    energy = np.empty(n, dtype=np.float64)
    max_speed = np.empty(n, dtype=np.float64)
    max_turn = np.empty(n, dtype=np.float64)

    for i, ag in enumerate(agents):
        x[i] = float(getattr(ag, 'x', 0.0))
        y[i] = float(getattr(ag, 'y', 0.0))
        radius[i] = float(getattr(ag, 'r', 0.0))
        angle[i] = float(getattr(ag, 'angle', 0.0))
        vx[i] = float(getattr(ag, 'vx', 0.0))
        vy[i] = float(getattr(ag, 'vy', 0.0))
        energy[i] = float(getattr(ag, 'energy', 0.0))
        loc = getattr(ag, 'locomotion', None)
        max_speed[i] = float(getattr(loc, 'max_speed', 0.0))
        max_turn[i] = float(getattr(loc, 'max_turn', 0.0))

    first_energy = getattr(agents[0], 'energy_model', None)
    is_predator = bool(getattr(agents[0], 'is_predator', False))
    if is_predator:
        v0_cost = float(getattr(first_energy, 'v0_cost', 1.0))
        vmax_cost = float(getattr(first_energy, 'vmax_cost', 15.0))
        vmax_ref = float(getattr(first_energy, 'vmax_ref', max(float(np.max(max_speed)), 1.0)))
        energy_cap = float(getattr(first_energy, 'energy_cap', 600.0))
    else:
        v0_cost = float(getattr(first_energy, 'v0_cost', 0.5))
        vmax_cost = float(getattr(first_energy, 'vmax_cost', 8.0))
        vmax_ref = float(getattr(first_energy, 'vmax_ref', max(float(np.max(max_speed)), 1.0)))
        energy_cap = float(getattr(first_energy, 'energy_cap', 400.0))

    backend = apply_locomotion_energy_arrays(
        x,
        y,
        radius,
        angle,
        vx,
        vy,
        energy,
        outputs_arr,
        max_speed,
        max_turn,
        float(dt),
        1 if getattr(world, 'shape', 'rectangular') == 'circular' else 0,
        float(getattr(world, 'width', 1.0)),
        float(getattr(world, 'height', 1.0)),
        float(getattr(world, 'cx', 0.0)),
        float(getattr(world, 'cy', 0.0)),
        float(getattr(world, 'radius', 1.0)),
        first_reverse,
        max(0.0, float(params.get('agents_inertia', 1.0))),
        v0_cost,
        vmax_cost,
        vmax_ref,
        energy_cap,
        prefer_numba=bool(params.get('use_numba_kernels', True)),
        allow_numpy=False,
        force_python=force_python,
    )
    if backend is None:
        return None

    for i, ag in enumerate(agents):
        ag.x = float(x[i])
        ag.y = float(y[i])
        ag.r = float(radius[i])
        ag.angle = float(angle[i])
        ag.vx = float(vx[i])
        ag.vy = float(vy[i])
        ag.energy = float(energy[i])
        energy_model = getattr(ag, 'energy_model', None)
        if energy_model is not None:
            energy_model.v0_cost = v0_cost
            energy_model.vmax_cost = vmax_cost
            energy_model.vmax_ref = max(1e-6, vmax_ref)
            energy_model.energy_cap = energy_cap
    return backend
"""
Entidades da simulação: Agent (base), Bacteria, Predator, Food.
Usando herança onde há comportamento compartilhado e composição para capacidades.
"""
        


class Entity(ABC):
    """Classe base abstrata para todas as entidades.

    Introduz __slots__ para reduzir overhead de memória por objeto e
    permitir escalar para milhares de instâncias com menor pressão de GC.
    Também adiciona um *type_code* inteiro para eliminar chamadas repetidas
    a hasattr()/isinstance em loops críticos (0=food,1=bacteria,2=predator).
    """

    __slots__ = ("x", "y", "r", "color", "type_code")

    def __init__(self, x: float, y: float, r: float, color: tuple):
        self.x = x
        self.y = y
        self.r = r
        self.color = color
        self.type_code = -1  # definido em subclasses

    @abstractmethod
    def draw(self, renderer):  # pragma: no cover - desenho não crítico aqui
        pass


class Food(Entity):
    __slots__ = Entity.__slots__ + ("energy",)
    def __init__(self, x: float, y: float, r: float):
        super().__init__(x, y, r, (220, 30, 30))
        self.energy = r * r
        self.type_code = 0

    def draw(self, renderer):  # pragma: no cover
        renderer.draw_food(self)


class Agent(Entity):
    """Base para agentes inteligentes.

    __slots__ reduz custo por instância (~atributos fixos) e prepara terreno
    para futura migração completa para SoA (Struct of Arrays). Nesta etapa
    mantemos objetos para compatibilidade, mas minimizamos hasattr.
    """

    __slots__ = Entity.__slots__ + (
        "angle", "vx", "vy", "m", "age", "last_reproduction_age", "selected", "brain", "sensor",
        "locomotion", "energy_model", "last_brain_output", "last_brain_activations",
        "is_predator", "energy",
        "food_eaten_count", "food_energy_eaten_total",
        "prey_eaten_count", "prey_energy_eaten_total",
        "diet_food", "diet_agents", "diet_same_label", "diet_food_efficiency", "diet_agent_efficiency",
        "label_ids",
        "body_shape", "agent_name",
    )

    def __init__(self, x: float, y: float, r: float, color: tuple,
                 brain: 'IBrain', sensor: 'RetinaSensor',
                 locomotion: 'Locomotion', energy_model: 'EnergyModel',
                 angle: Optional[float] = None):
        super().__init__(x, y, r, color)

        # Estado físico (massa inercial fixa derivada do corpo)
        self.angle = angle if angle is not None else random.uniform(0, math.pi * 2)
        self.vx = 0.0
        self.vy = 0.0
        self.m = r * r  # mantido para cálculos físicos existentes
        self.energy = 0.0  # bateria interna
        self.age = 0.0
        self.last_reproduction_age = None
        self.selected = False

        # Componentes
        self.brain = brain
        self.sensor = sensor
        self.locomotion = locomotion
        self.energy_model = energy_model

        # Debug
        self.last_brain_output = []
        self.last_brain_activations = []
        self.is_predator = False
        self.food_eaten_count = 0
        self.food_energy_eaten_total = 0.0
        self.prey_eaten_count = 0
        self.prey_energy_eaten_total = 0.0
        self.diet_food = True
        self.diet_agents = False
        self.diet_same_label = False
        self.diet_food_efficiency = 1.0
        self.diet_agent_efficiency = 0.7
        self.label_ids = set()
        self.body_shape = getattr(locomotion, 'body_shape', 'ellipse')
        self.agent_name = None
    
    def update(self, dt: float, world: 'World', scene: 'SceneQuery', params: 'Params'):
        """
        Atualiza agente por um passo de tempo.
        
        Sequência: sense -> think -> act -> energy
        """
        self.age += dt
        
        # 1. Sensoriamento
        with profile_section('agent_sensor'):
            sensor_inputs = self.sensor.sense(self, scene, params)

        # Ajuste dinâmico do tamanho de entrada da rede caso número de retinas mude
        expected_input_size = self.brain.sizes[0] if hasattr(self.brain, 'sizes') else len(sensor_inputs)
        if len(sensor_inputs) != expected_input_size and hasattr(self.brain, 'resize_input'):
            self.brain.resize_input(len(sensor_inputs))
        
        # 2. Processamento neural
        with profile_section('agent_brain_forward'):
            brain_outputs = self.brain.forward(sensor_inputs)
        self.last_brain_output = list(brain_outputs)
        # Ativações só se profiler ligado (evita custo desnecessário em produção)
        # Activations opcional (custa tempo). Pula se param disable_brain_activations está setado.
        if profiler.enabled and not params.get('disable_brain_activations', False):
            with profile_section('agent_brain_activations'):
                self.last_brain_activations = self.brain.activations(sensor_inputs)
        else:
            self.last_brain_activations = []
        
        # 3. Atuação (movimento)
        with profile_section('agent_locomotion'):
            self.locomotion.step(self, brain_outputs, dt, world, params)
        
        # 4. Modelo energético
        with profile_section('agent_energy'):
            self.energy_model.apply(self, dt, params)
    
    def speed(self) -> float:
        """Calcula velocidade atual do agente."""
        return math.hypot(self.vx, self.vy)
    
    def set_energy(self, value: float):
        self.energy = max(0.0, value)

    def add_energy(self, delta: float, cap: Optional[float] = None):
        value = max(0.0, self.energy + delta)
        if cap is None and getattr(self, 'energy_model', None) is not None:
            cap = getattr(self.energy_model, 'energy_cap', None)
        if cap is not None:
            value = min(value, float(cap))
        self.energy = value
    
    def can_reproduce(self, params: 'Params') -> bool:
        return self.energy_model.can_reproduce(self)

    def should_die(self, params: 'Params') -> bool:
        return self.energy_model.should_die(self)
    
    def reproduce(self, params: 'Params') -> 'Agent':
        """Cria e retorna um novo agente filho dividindo energia interna."""
        child_energy_value = self.energy_model.prepare_reproduction(self)

        # Posição e velocidade do filho
        child_angle = self.angle + random.uniform(-0.5, 0.5)
        child_speed = self.speed() * 0.5 + random.uniform(-30, 30)
        child_x = self.x + math.cos(child_angle) * self.r * 0.5
        child_y = self.y + math.sin(child_angle) * self.r * 0.5

        # Cérebro do filho
        child_brain = self.brain.copy()
        child_brain.mutate(
            rate=self._get_mutation_rate(params),
            strength=self._get_mutation_strength(params),
            structural_jitter=self._get_structural_jitter(params)
        )

        # Componentes
        child_sensor = self._create_child_sensor(params)
        child_locomotion = self._create_child_locomotion(params)
        child_energy_model = self._create_child_energy_model(params)

        # Instância filho
        # Use explicit subclass constructors to avoid positional-argument mismatches
        # for subclasses that may define a different __init__ signature.
        if isinstance(self, Bacteria):
            # pass parent's color explicitly to avoid any race or override
            child = Bacteria(
                child_x, child_y, self.r,
                child_brain, child_sensor, child_locomotion, child_energy_model,
                child_angle,
                color=getattr(self, 'color', None)
            )
        elif isinstance(self, Predator):
            child = Predator(
                child_x, child_y, self.r,
                child_brain, child_sensor, child_locomotion, child_energy_model,
                child_angle,
                color=getattr(self, 'color', None)
            )
        else:
            child = self.__class__(
                child_x, child_y, self.r, self.color,
                child_brain, child_sensor, child_locomotion, child_energy_model,
                child_angle
            )
        child.energy = child_energy_value
        child.vx = math.cos(child_angle) * child_speed
        child.vy = math.sin(child_angle) * child_speed
        # Ensure child inherits parent's color (fallback) and optionally debug
        if getattr(self, 'color', None) is not None:
            try:
                child.color = getattr(self, 'color')
            except Exception:
                pass
        for attr in ("diet_food", "diet_agents", "diet_same_label", "diet_food_efficiency", "diet_agent_efficiency"):
            try:
                setattr(child, attr, getattr(self, attr))
            except Exception:
                pass
        parent_sensor = getattr(self, 'sensor', None)
        child_sensor = getattr(child, 'sensor', None)
        if parent_sensor is not None and child_sensor is not None:
            for attr in ("retina_count", "vision_radius", "fov_degrees", "skip", "see_food", "see_bacteria", "see_predators", "channels", "eye_count", "eye_angle_degrees", "eye_separation_degrees"):
                try:
                    value = getattr(parent_sensor, attr)
                    if attr == "channels":
                        value = tuple(value)
                    setattr(child_sensor, attr, value)
                except Exception:
                    pass
            try:
                child_sensor.last_inputs = []
                child_sensor.last_distance_inputs = []
                child_sensor._countdown = 0
            except Exception:
                pass
        parent_energy = getattr(self, 'energy_model', None)
        child_energy_model = getattr(child, 'energy_model', None)
        if parent_energy is not None and child_energy_model is not None:
            for attr in ("age_death_enabled", "death_age", "corpse_to_food", "reproduction_min_age", "reproduction_cooldown"):
                try:
                    setattr(child_energy_model, attr, getattr(parent_energy, attr))
                except Exception:
                    pass
        parent_loc = getattr(self, 'locomotion', None)
        child_loc = getattr(child, 'locomotion', None)
        if parent_loc is not None and child_loc is not None:
            for attr in ("max_speed", "max_turn", "allow_reverse", "movement_mode", "body_shape"):
                try:
                    setattr(child_loc, attr, getattr(parent_loc, attr))
                except Exception:
                    pass
            try:
                child.body_shape = getattr(parent_loc, 'body_shape', getattr(child, 'body_shape', 'ellipse'))
            except Exception:
                pass
        child.agent_name = getattr(self, 'agent_name', None)
        child.label_ids = set(getattr(self, 'label_ids', set()) or set())
        if params.get('debug_reproduction_color', False):
            print(f"[reproduce] parent_type={type(self).__name__} parent_color={getattr(self,'color',None)} -> child_type={type(child).__name__} child_color={getattr(child,'color',None)}")
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
    """Agente do tipo bactéria."""

    def __init__(self, x: float, y: float, r: float, brain: 'IBrain',
                 sensor: 'RetinaSensor', locomotion: 'Locomotion',
                 energy_model: 'EnergyModel', angle: Optional[float] = None, color: Optional[tuple] = None):
        # allow passing color as optional kwarg; default remains the type default
        color = (220, 220, 220) if color is None else color
        super().__init__(x, y, r, color, brain, sensor, locomotion, energy_model, angle)
        self.is_predator = False
        self.type_code = 1
        self.diet_food = True
        self.diet_agents = False
    
    def _get_mutation_rate(self, params: 'Params') -> float:
        return params.get('bacteria_mutation_rate', 0.05)
    
    def _get_mutation_strength(self, params: 'Params') -> float:
        return params.get('bacteria_mutation_strength', 0.08)
    
    def _get_structural_jitter(self, params: 'Params') -> int:
        return params.get('bacteria_structural_jitter', 0)
    
    def _create_child_sensor(self, params: 'Params') -> 'RetinaSensor':
        from .sensors import RetinaSensor, active_retina_channels
        return RetinaSensor(
            retina_count=params.get('bacteria_retina_count', 18),
            vision_radius=params.get('bacteria_vision_radius', 120.0),
            fov_degrees=params.get('bacteria_retina_fov_degrees', 180.0),
            skip=params.get('retina_skip', 0),
            see_food=params.get('bacteria_retina_see_food', True),
            see_bacteria=params.get('bacteria_retina_see_bacteria', False),
            see_predators=params.get('bacteria_retina_see_predators', False),
            channels=active_retina_channels(params, 'bacteria'),
            eye_count=params.get('bacteria_eye_count', 1),
            eye_angle_degrees=params.get('bacteria_eye_angle_degrees', 60.0),
            eye_separation_degrees=params.get('bacteria_eye_separation_degrees', 45.0),
        )
    
    def _create_child_locomotion(self, params: 'Params') -> 'Locomotion':
        from .actuators import Locomotion
        return Locomotion(
            max_speed=params.get('bacteria_max_speed', 300.0),
            max_turn=params.get('bacteria_max_turn', math.pi),
            allow_reverse=params.get('bacteria_allow_reverse_locomotion', False),
            movement_mode=params.get('bacteria_movement_mode', 'forward'),
            body_shape=params.get('bacteria_body_shape', 'ellipse'),
        )
    
    def _create_child_energy_model(self, params: 'Params') -> 'EnergyModel':
        from .actuators import EnergyModel
        return EnergyModel(
            death_energy=params.get('bacteria_death_energy', 0.0),
            split_energy=params.get('bacteria_split_energy', 150.0),
            v0_cost=params.get('bacteria_metab_v0_cost', 0.5),
            vmax_cost=params.get('bacteria_metab_vmax_cost', 8.0),
            vmax_ref=params.get('bacteria_max_speed',300.0),
            energy_cap=params.get('bacteria_energy_cap', 400.0),
            age_death_enabled=params.get('bacteria_age_death_enabled', False),
            death_age=params.get('bacteria_death_age', 3600.0),
            corpse_to_food=params.get('bacteria_corpse_to_food', False),
            reproduction_min_age=params.get('bacteria_reproduction_min_age', params.get('reproduction_min_age', 0.0)),
            reproduction_cooldown=params.get('bacteria_reproduction_cooldown', params.get('reproduction_cooldown', 0.0)),
        )


class Predator(Agent):
    """Agente do tipo predador."""

    def __init__(self, x: float, y: float, r: float, brain: 'IBrain',
                 sensor: 'RetinaSensor', locomotion: 'Locomotion',
                 energy_model: 'EnergyModel', angle: Optional[float] = None, color: Optional[tuple] = None):
        # allow passing color as optional kwarg; default remains the type default
        color = (80, 120, 220) if color is None else color
        super().__init__(x, y, r, color, brain, sensor, locomotion, energy_model, angle)
        self.is_predator = True
        self.type_code = 2
        self.diet_food = False
        self.diet_agents = True
    
    def _get_mutation_rate(self, params: 'Params') -> float:
        return params.get('predator_mutation_rate', 0.05)
    
    def _get_mutation_strength(self, params: 'Params') -> float:
        return params.get('predator_mutation_strength', 0.08)
    
    def _get_structural_jitter(self, params: 'Params') -> int:
        return params.get('predator_structural_jitter', 0)
    
    def _create_child_sensor(self, params: 'Params') -> 'RetinaSensor':
        from .sensors import RetinaSensor, active_retina_channels
        return RetinaSensor(
            retina_count=params.get('predator_retina_count', 18),
            vision_radius=params.get('predator_vision_radius', 120.0),
            fov_degrees=params.get('predator_retina_fov_degrees', 180.0),
            skip=params.get('retina_skip', 0),
            see_food=params.get('predator_retina_see_food', True),
            see_bacteria=params.get('predator_retina_see_bacteria', True),
            see_predators=params.get('predator_retina_see_predators', False),
            channels=active_retina_channels(params, 'predator'),
            eye_count=params.get('predator_eye_count', 1),
            eye_angle_degrees=params.get('predator_eye_angle_degrees', 60.0),
            eye_separation_degrees=params.get('predator_eye_separation_degrees', 45.0),
        )
    
    def _create_child_locomotion(self, params: 'Params') -> 'Locomotion':
        from .actuators import Locomotion
        return Locomotion(
            max_speed=params.get('predator_max_speed', 300.0),
            max_turn=params.get('predator_max_turn', math.pi),
            allow_reverse=params.get('predator_allow_reverse_locomotion', False),
            movement_mode=params.get('predator_movement_mode', 'forward'),
            body_shape=params.get('predator_body_shape', 'ellipse'),
        )
    
    def _create_child_energy_model(self, params: 'Params') -> 'EnergyModel':
        from .actuators import EnergyModel
        return EnergyModel(
            death_energy=params.get('predator_death_energy', 0.0),
            split_energy=params.get('predator_split_energy', 150.0),
            v0_cost=params.get('predator_metab_v0_cost', 1.0),
            vmax_cost=params.get('predator_metab_vmax_cost', 15.0),
            vmax_ref=params.get('predator_max_speed',300.0),
            energy_cap=params.get('predator_energy_cap', 600.0),
            age_death_enabled=params.get('predator_age_death_enabled', False),
            death_age=params.get('predator_death_age', 3600.0),
            corpse_to_food=params.get('predator_corpse_to_food', False),
            reproduction_min_age=params.get('predator_reproduction_min_age', params.get('reproduction_min_age', 0.0)),
            reproduction_cooldown=params.get('predator_reproduction_cooldown', params.get('reproduction_cooldown', 0.0)),
        )


# Factory functions para criar entidades com parâmetros
def _agent_spawn_radius(params: 'Params', body_key: str, min_key: str, max_key: str,
                        default_min: float, default_max: float) -> float:
    body_size = params.get(body_key, None)
    if body_size is not None:
        try:
            return max(0.1, float(body_size))
        except (TypeError, ValueError):
            pass
    min_r = max(0.1, float(params.get(min_key, default_min)))
    max_r = max(min_r, float(params.get(max_key, default_max)))
    return random.uniform(min_r, max_r)


def _random_position_for_radius(shape: str, world_w: float, world_h: float,
                                radius_sub: float, r: float) -> tuple[float, float]:
    cx = world_w / 2
    cy = world_h / 2
    if shape == 'circular':
        ang = random.random() * 2 * math.pi
        rad = (random.random() ** 0.5) * max(0.0, radius_sub - r)
        return cx + math.cos(ang) * rad, cy + math.sin(ang) * rad
    x = cx if world_w <= 2 * r else random.uniform(r, world_w - r)
    y = cy if world_h <= 2 * r else random.uniform(r, world_h - r)
    return x, y


def _apply_diet_from_params(agent: 'Agent', params: 'Params', prefix: str):
    agent.diet_food = bool(params.get(f'{prefix}_diet_food', not getattr(agent, 'is_predator', False)))
    agent.diet_agents = bool(params.get(f'{prefix}_diet_agents', getattr(agent, 'is_predator', False)))
    agent.diet_same_label = bool(params.get(f'{prefix}_diet_same_label', False))
    agent.diet_food_efficiency = float(params.get(f'{prefix}_diet_food_efficiency', 1.0))
    agent.diet_agent_efficiency = float(params.get(f'{prefix}_diet_agent_efficiency', 0.7))


def _brain_output_size(params: 'Params', prefix: str) -> int:
    from .actuators import locomotion_output_size
    return locomotion_output_size(params.get(f'{prefix}_movement_mode', 'forward'))


def create_random_bacteria(existing_entities: list, params: 'Params', 
                          world_w: float, world_h: float,
                          at: Optional[tuple] = None) -> Bacteria:
    """Cria bactéria aleatória evitando sobreposições."""
    from .brain import NeuralNet
    from .sensors import retina_input_size
    from .sensors import RetinaSensor, active_retina_channels
    from .actuators import Locomotion, EnergyModel
    
    r = _agent_spawn_radius(params, 'bacteria_body_size', 'bacteria_min_r', 'bacteria_max_r', 6.0, 12.0)
    shape = params.get('substrate_shape', 'rectangular')
    radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
    cx = world_w/2
    cy = world_h/2
    for _ in range(300):
        if at is None:
            x, y = _random_position_for_radius(shape, world_w, world_h, radius_sub, r)
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
        x, y = _random_position_for_radius(shape, world_w, world_h, radius_sub, r)
    
    # Cria componentes
    brain = _create_bacteria_brain(params)
    sensor = _create_bacteria_sensor(params)
    locomotion = _create_bacteria_locomotion(params)
    energy = _create_bacteria_energy_model(params)
    
    # Cria bactéria com cor baseada em parâmetros e massa inicial customizável
    bacterium = Bacteria(x, y, r, brain, sensor, locomotion, energy)
    # Override color from params if provided
    try:
        bacterium.color = tuple(params.get('bacteria_color', bacterium.color))
    except Exception:
        pass
    bacterium.body_shape = getattr(locomotion, 'body_shape', getattr(bacterium, 'body_shape', 'ellipse'))
    bacterium.agent_name = str(params.get('agent_template_name', 'organismo_1') or 'organismo_1')
    _apply_diet_from_params(bacterium, params, 'bacteria')
    bacterium.energy = params.get('bacteria_initial_energy', 100.0)
    
    return bacterium


def create_random_predator(existing_entities: list, params: 'Params',
                          world_w: float, world_h: float,
                          at: Optional[tuple] = None) -> Predator:
    """Cria predador aleatório evitando sobreposições."""
    from .brain import NeuralNet
    from .sensors import RetinaSensor, active_retina_channels
    from .actuators import Locomotion, EnergyModel
    
    r = _agent_spawn_radius(params, 'predator_body_size', 'predator_min_r', 'predator_max_r', 10.0, 18.0)
    shape = params.get('substrate_shape', 'rectangular')
    radius_sub = params.get('substrate_radius', min(world_w, world_h)/2)
    cx = world_w/2
    cy = world_h/2
    for _ in range(300):
        if at is None:
            x, y = _random_position_for_radius(shape, world_w, world_h, radius_sub, r)
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
        x, y = _random_position_for_radius(shape, world_w, world_h, radius_sub, r)
    
    # Cria componentes
    brain = _create_predator_brain(params)
    sensor = _create_predator_sensor(params)
    locomotion = _create_predator_locomotion(params)
    energy = _create_predator_energy_model(params)
    
    # Cria predador com cor baseada em parâmetros e massa inicial customizável
    predator = Predator(x, y, r, brain, sensor, locomotion, energy)
    try:
        predator.color = tuple(params.get('predator_color', predator.color))
    except Exception:
        pass
    predator.body_shape = getattr(locomotion, 'body_shape', getattr(predator, 'body_shape', 'ellipse'))
    predator.agent_name = str(params.get('agent_template_name', 'organismo_1') or 'organismo_1')
    _apply_diet_from_params(predator, params, 'predator')
    predator.energy = params.get('predator_initial_energy', 100.0)
    
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
    
    food = Food(x, y, r)
    try:
        food.color = tuple(params.get('food_color', food.color))
    except Exception:
        pass
    return food


# Helper functions para criar componentes
def _create_bacteria_brain(params: 'Params'):
    """Cria cérebro para bactéria baseado nos parâmetros."""
    from .brain import NeuralNet
    
    from .sensors import retina_input_size
    input_size = retina_input_size(params, 'bacteria', params.get('bacteria_retina_count', 18))
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
    
    layer_sizes.append(_brain_output_size(params, 'bacteria'))
    
    return NeuralNet(layer_sizes, init_std=1.0)


def _create_bacteria_sensor(params: 'Params'):
    """Cria sensor para bactéria."""
    from .sensors import RetinaSensor, active_retina_channels
    return RetinaSensor(
        retina_count=params.get('bacteria_retina_count', 18),
        vision_radius=params.get('bacteria_vision_radius', 120.0),
        fov_degrees=params.get('bacteria_retina_fov_degrees', 180.0),
        skip=params.get('retina_skip', 0),
        see_food=params.get('bacteria_retina_see_food', True),
        see_bacteria=params.get('bacteria_retina_see_bacteria', False),
        see_predators=params.get('bacteria_retina_see_predators', False),
        channels=active_retina_channels(params, 'bacteria'),
        eye_count=params.get('bacteria_eye_count', 1),
        eye_angle_degrees=params.get('bacteria_eye_angle_degrees', 60.0),
        eye_separation_degrees=params.get('bacteria_eye_separation_degrees', 45.0),
    )


def _create_bacteria_locomotion(params: 'Params'):
    """Cria locomoção para bactéria."""
    from .actuators import Locomotion
    return Locomotion(
        max_speed=params.get('bacteria_max_speed', 300.0),
        max_turn=params.get('bacteria_max_turn', math.pi),
        allow_reverse=params.get('bacteria_allow_reverse_locomotion', False),
        movement_mode=params.get('bacteria_movement_mode', 'forward'),
        body_shape=params.get('bacteria_body_shape', 'ellipse'),
    )


def _create_bacteria_energy_model(params: 'Params'):
    """Cria modelo energético para bactéria."""
    from .actuators import EnergyModel
    return EnergyModel(
        death_energy=params.get('bacteria_death_energy', 0.0),
    split_energy=params.get('bacteria_split_energy', 150.0),
    v0_cost=params.get('bacteria_metab_v0_cost', 0.5),
    vmax_cost=params.get('bacteria_metab_vmax_cost', 8.0),
    vmax_ref=params.get('bacteria_max_speed',300.0),
    energy_cap=params.get('bacteria_energy_cap', 400.0),
    age_death_enabled=params.get('bacteria_age_death_enabled', False),
    death_age=params.get('bacteria_death_age', 3600.0),
    corpse_to_food=params.get('bacteria_corpse_to_food', False),
    reproduction_min_age=params.get('bacteria_reproduction_min_age', params.get('reproduction_min_age', 0.0)),
    reproduction_cooldown=params.get('bacteria_reproduction_cooldown', params.get('reproduction_cooldown', 0.0)),
    )


def _create_predator_brain(params: 'Params'):
    """Cria cérebro para predador.""" 
    from .brain import NeuralNet
    
    from .sensors import retina_input_size
    input_size = retina_input_size(params, 'predator', params.get('predator_retina_count', 18))
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
    
    layer_sizes.append(_brain_output_size(params, 'predator'))
    
    return NeuralNet(layer_sizes, init_std=1.0)


def _create_predator_sensor(params: 'Params'):
    """Cria sensor para predador."""
    from .sensors import RetinaSensor, active_retina_channels
    return RetinaSensor(
        retina_count=params.get('predator_retina_count', 18),
        vision_radius=params.get('predator_vision_radius', 120.0),
        fov_degrees=params.get('predator_retina_fov_degrees', 180.0),
        skip=params.get('retina_skip', 0),
        see_food=params.get('predator_retina_see_food', True),
        see_bacteria=params.get('predator_retina_see_bacteria', True),
        see_predators=params.get('predator_retina_see_predators', False),
        channels=active_retina_channels(params, 'predator'),
        eye_count=params.get('predator_eye_count', 1),
        eye_angle_degrees=params.get('predator_eye_angle_degrees', 60.0),
        eye_separation_degrees=params.get('predator_eye_separation_degrees', 45.0),
    )


def _create_predator_locomotion(params: 'Params'):
    """Cria locomoção para predador."""
    from .actuators import Locomotion
    return Locomotion(
        max_speed=params.get('predator_max_speed', 300.0),
        max_turn=params.get('predator_max_turn', math.pi),
        allow_reverse=params.get('predator_allow_reverse_locomotion', False),
        movement_mode=params.get('predator_movement_mode', 'forward'),
        body_shape=params.get('predator_body_shape', 'ellipse'),
    )


def _create_predator_energy_model(params: 'Params'):
    """Cria modelo energético para predador."""
    from .actuators import EnergyModel
    return EnergyModel(
        death_energy=params.get('predator_death_energy', 0.0),
    split_energy=params.get('predator_split_energy', 150.0),
    v0_cost=params.get('predator_metab_v0_cost', 1.0),
    vmax_cost=params.get('predator_metab_vmax_cost', 15.0),
    vmax_ref=params.get('predator_max_speed',300.0),
    energy_cap=params.get('predator_energy_cap', 600.0),
    age_death_enabled=params.get('predator_age_death_enabled', False),
    death_age=params.get('predator_death_age', 3600.0),
    corpse_to_food=params.get('predator_corpse_to_food', False),
    reproduction_min_age=params.get('predator_reproduction_min_age', params.get('reproduction_min_age', 0.0)),
    reproduction_cooldown=params.get('predator_reproduction_cooldown', params.get('reproduction_cooldown', 0.0)),
    )
