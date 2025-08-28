"""
Motor principal da simulação - headless, não conhece UI.
Responsável pelo loop de simulação com tempo fixo e sistemas.
"""
import math
import time
from typing import Dict, List, Optional, Any
from queue import Queue, Empty

from .world import World, Camera
from .spatial import SpatialHash
from .controllers import Params, FoodController, PopulationController
from .entities import Agent, Bacteria, Predator, Food, create_random_bacteria, create_random_predator, create_random_food
from .sensors import SceneQuery
from .systems import InteractionSystem, ReproductionSystem, DeathSystem, CollisionSystem
try:
    from .render import RendererStrategy, SimpleRenderer, EllipseRenderer  # noqa
except Exception:  # pygame import pode falhar em headless puro
    RendererStrategy = object  # type: ignore
    SimpleRenderer = EllipseRenderer = None  # type: ignore
from .profiler import profile_section, profiler


class Engine:
    """
    Motor principal da simulação.
    
    - Headless: não conhece Tkinter ou UI específicas
    - Tempo fixo: garante determinismo independente de FPS  
    - Modular: usa sistemas injetados para diferentes regras
    - Thread-safe: aceita comandos via queue
    """
    
    def __init__(self, world: World, camera: Camera, params: Params, renderer: Optional[Any] = None, headless: bool = False):
        self.world = world
        self.camera = camera
        self.params = params
        self.headless = headless

        # Entidades
        self.entities = {
            'bacteria': [],
            'predators': [],
            'foods': []
        }

        # Sistemas
        self.interaction_system = InteractionSystem()
        self.reproduction_system = ReproductionSystem()
        self.death_system = DeathSystem(max_deaths_per_step=params.get('max_deaths_per_step', 1))
        self.collision_system = CollisionSystem()

        # Controladores
        self.food_controller = FoodController()
        self.population_controller = PopulationController()

        # Infraestrutura
        self.spatial_hash: Optional[SpatialHash] = None
        self.scene_query: Optional[SceneQuery] = None

        # Renderização
        if headless or SimpleRenderer is None:
            self.renderer = None
        else:
            self.renderer = renderer or SimpleRenderer()

        # Controle de execução
        self.running = False
        self.command_queue = Queue()

        # Métricas
        self.total_simulation_time = 0.0
        self.frame_count = 0
        self.last_fps_time = time.time()
        self.current_fps = 0.0

        # Estado para debugging
        self.selected_agent: Optional[Agent] = None
    
    def start(self):
        """Inicia a simulação."""
        self.running = True
        self._initialize_population()
    
    def stop(self):
        """Para a simulação."""
        self.running = False
    
    def step(self, real_dt: float):
        """
        Executa um passo completo da simulação.
        
        Args:
            real_dt: Delta tempo real (em segundos)
        """
        if not self.running:
            return
        
        # Processa comandos da UI
        self._process_commands()
        
        # Calcula tempo físico com time_scale
        world_dt = real_dt * max(0.0, self.params.get('time_scale', 1.0))
        
        # Executa substeps com tempo fixo
        if world_dt > 0 and not self.params.get('paused', False):
            self._simulate_physics(world_dt)
        
        # Atualiza métricas
        self.total_simulation_time += world_dt
        self.frame_count += 1
        
        # Calcula FPS
        now = time.time()
        if now - self.last_fps_time >= 1.0:
            self.current_fps = self.frame_count / (now - self.last_fps_time)
            self.last_fps_time = now
            self.frame_count = 0
    
    def render(self, surface, show_world_bounds: bool = True):
        """
        Renderiza a simulação na superfície fornecida.
        
        Args:
            surface: Superfície pygame para desenhar
            show_world_bounds: Se deve mostrar limites do mundo
        """
        if self.headless or self.renderer is None:
            return
        # Limpa tela
        surface.fill((10, 10, 20))
        
        # Desenha limites do mundo
        if show_world_bounds:
            self._draw_world_bounds(surface)
        
        # Desenha entidades
        for food in self.entities['foods']:
            self.renderer.draw_food(food, surface, self.camera)
        
        for predator in self.entities['predators']:
            selected = (predator is self.selected_agent)
            self.renderer.draw_agent(predator, surface, self.camera, 
                                   show_head=True, show_vision=False, selected=selected)
        
        for bacterium in self.entities['bacteria']:
            selected = (bacterium is self.selected_agent)
            self.renderer.draw_agent(bacterium, surface, self.camera,
                                   show_head=True, show_vision=False, selected=selected)
        
        # Desenha overlay de informações
        info = self._gather_render_info()
        self.renderer.draw_overlay(surface, info)
    
    def send_command(self, command: str, **kwargs):
        """
        Envia comando para a simulação de forma thread-safe.
        
        Args:
            command: Nome do comando
            **kwargs: Argumentos do comando
        """
        self.command_queue.put((command, kwargs))
    
    def get_agent_at_position(self, world_x: float, world_y: float) -> Optional[Agent]:
        """Encontra agente na posição do mundo especificada."""
        all_agents = self.entities['bacteria'] + self.entities['predators']
        
        # Procura do mais próximo ao cursor (último desenhado = mais visível)
        for agent in reversed(all_agents):
            distance = math.hypot(agent.x - world_x, agent.y - world_y)
            if distance <= agent.r:
                return agent
        
        return None
    
    def add_food_at(self, world_x: float, world_y: float):
        """Adiciona comida na posição especificada."""
        food = create_random_food(self.entities['foods'], self.params, 
                                self.world.width, self.world.height, 
                                at=(world_x, world_y))
        self.entities['foods'].append(food)
    
    def add_bacteria_at(self, world_x: float, world_y: float):
        """Adiciona bactéria na posição especificada."""
        all_entities = (self.entities['bacteria'] + self.entities['predators'] + 
                       self.entities['foods'])
        bacterium = create_random_bacteria(all_entities, self.params,
                                         self.world.width, self.world.height,
                                         at=(world_x, world_y))
        self.entities['bacteria'].append(bacterium)
    
    def _simulate_physics(self, world_dt: float):
        """Simula física por um delta tempo do mundo."""
        # Calcula substeps com tempo fixo
        physics_dt = 1.0 / 60.0  # 60 FPS físico
        max_substeps = 8
        
        if world_dt <= 0:
            return
        
        estimated_steps = int(round(world_dt / physics_dt))
        if estimated_steps <= 0:
            steps = 1
            step_dt = world_dt
        elif estimated_steps > max_substeps:
            steps = max_substeps
            step_dt = world_dt / steps
        else:
            steps = estimated_steps  
            step_dt = physics_dt
        
        # Executa substeps
        for _ in range(steps):
            self._simulate_substep(step_dt)
    
    def _simulate_substep(self, dt: float):
        """Executa um substep de física."""
        with profile_section('spatial_hash'):
            self._update_spatial_hash()

        with profile_section('food_control'):
            target_food = self.params.get('food_target', 300)
            new_foods = self.food_controller.update(
                self.entities['foods'], target_food, self.world.width, self.world.height, self.params, dt
            )
            self.entities['foods'].extend(new_foods)

        all_agents = self.entities['bacteria'] + self.entities['predators']
        with profile_section('agents_update'):
            for agent in all_agents:
                agent.update(dt, self.world, self.scene_query, self.params)

        with profile_section('interaction'):
            self.interaction_system.apply(
                self.entities['bacteria'], self.entities['predators'],
                self.entities['foods'], self.spatial_hash, self.params
            )

        with profile_section('reproduction'):
            new_agents = self.reproduction_system.apply(all_agents, self.params)

        for agent in new_agents:
            if getattr(agent, 'is_predator', False):
                self.entities['predators'].append(agent)
            else:
                self.entities['bacteria'].append(agent)

        with profile_section('death'):
            surviving_bacteria, surviving_predators = self.death_system.apply(
                self.entities['bacteria'], self.entities['predators'], self.params
            )
            self.entities['bacteria'] = surviving_bacteria
            self.entities['predators'] = surviving_predators

        with profile_section('collision'):
            self.collision_system.apply(all_agents, self.spatial_hash, self.params)
    
    def _update_spatial_hash(self):
        """Atualiza ou recria spatial hash."""
        if not self.params.get('use_spatial', True):
            self.spatial_hash = None
            self.scene_query = SceneQuery(None, self.entities, self.params)
            return
        
        # Calcula tamanho de célula baseado no maior objeto
        max_radius = max(
            self.params.get('food_max_r', 5.0),
            self.params.get('bacteria_max_r', 12.0),
            self.params.get('predator_max_r', 18.0)
        )
        cell_size = max_radius * 2.0
        
        # Reutiliza hash existente se possível
        if (self.spatial_hash and 
            self.params.get('reuse_spatial_grid', True) and
            abs(self.spatial_hash.cell_size - cell_size) < 1e-6 and
            self.spatial_hash.width == self.world.width and
            self.spatial_hash.height == self.world.height):
            # Reutiliza: apenas limpa e reinsere
            self.spatial_hash.clear()
        else:
            # Recria
            self.spatial_hash = SpatialHash(cell_size, self.world.width, self.world.height)
        
        # Insere todas as entidades
        for food in self.entities['foods']:
            self.spatial_hash.insert(food, food.x, food.y, food.r)
        
        for bacterium in self.entities['bacteria']:
            self.spatial_hash.insert(bacterium, bacterium.x, bacterium.y, bacterium.r)
        
        for predator in self.entities['predators']:
            self.spatial_hash.insert(predator, predator.x, predator.y, predator.r)
        
        # Atualiza scene query
        self.scene_query = SceneQuery(self.spatial_hash, self.entities, self.params)
    
    def _process_commands(self):
        """Processa comandos da fila."""
        while True:
            try:
                command, kwargs = self.command_queue.get_nowait()
                self._execute_command(command, **kwargs)
            except Empty:
                break
    
    def _execute_command(self, command: str, **kwargs):
        """Executa um comando específico."""
        if command == 'select_agent':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.selected_agent = self.get_agent_at_position(world_x, world_y)
        
        elif command == 'add_food':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.add_food_at(world_x, world_y)
        
        elif command == 'add_bacteria':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.add_bacteria_at(world_x, world_y)
        
        elif command == 'reset_population':
            self._initialize_population()
            self.selected_agent = None
        
        elif command == 'change_renderer':
            simple = kwargs.get('simple', False)
            self.renderer = SimpleRenderer() if simple else EllipseRenderer()
        
        else:
            print(f"Comando desconhecido: {command}")
    
    def _initialize_population(self):
        """Inicializa população baseada nos parâmetros."""
        # Limpa entidades existentes
        for entity_list in self.entities.values():
            entity_list.clear()
        
        all_entities = []
        
        # Cria bactérias
        bacteria_count = min(self.params.get('bacteria_count', 150), 10000)
        for _ in range(bacteria_count):
            bacterium = create_random_bacteria(all_entities, self.params,
                                             self.world.width, self.world.height)
            self.entities['bacteria'].append(bacterium)
            all_entities.append(bacterium)
        
        # Cria predadores se habilitados
        if self.params.get('predators_enabled', False):
            predator_count = min(self.params.get('predator_count', 0), 1000)
            for _ in range(predator_count):
                predator = create_random_predator(all_entities, self.params,
                                                self.world.width, self.world.height)
                self.entities['predators'].append(predator)
                all_entities.append(predator)
        
        # Cria comida
        food_target = self.params.get('food_target', 50)
        for _ in range(food_target):
            food = create_random_food(self.entities['foods'], self.params,
                                    self.world.width, self.world.height)
            self.entities['foods'].append(food)
    
    def _draw_world_bounds(self, surface):
        """Desenha limites do mundo.""" 
        import pygame
        if getattr(self.world, 'shape', 'rectangular') == 'circular':
            # Desenha círculo baseado em world.cx, world.cy, world.radius
            center_screen = self.camera.world_to_screen(self.world.cx, self.world.cy)
            radius_screen = int(self.world.radius * self.camera.zoom)
            if radius_screen > 1:
                pygame.draw.circle(surface, (40, 200, 40), (int(center_screen[0]), int(center_screen[1])), radius_screen, width=1)
        else:
            top_left = self.camera.world_to_screen(0, 0)
            bottom_right = self.camera.world_to_screen(self.world.width, self.world.height)
            rect_x = int(top_left[0])
            rect_y = int(top_left[1])
            rect_w = int(bottom_right[0] - top_left[0])
            rect_h = int(bottom_right[1] - top_left[1])
            if rect_w >= 2 and rect_h >= 2:
                pygame.draw.rect(surface, (40, 200, 40), pygame.Rect(rect_x, rect_y, rect_w, rect_h), width=1)
    
    def _gather_render_info(self) -> Dict[str, Any]:
        """Coleta informações para renderização."""
        return {
            'bacteria_count': len(self.entities['bacteria']),
            'predator_count': len(self.entities['predators']),
            'food_count': len(self.entities['foods']),
            'food_target': self.params.get('food_target', 0),
            'fps': self.current_fps,
            'max_speed': self.params.get('bacteria_max_speed', 300.0),
            'time_scale': self.params.get('time_scale', 1.0),
            'world_w': self.world.width,
            'world_h': self.world.height,
            'selected_agent': self.selected_agent,
            'show_selected_details': self.params.get('show_selected_details', True)
        }
