"""
Motor principal da simulação - headless, não conhece UI.
Responsável pelo loop de simulação com tempo fixo e sistemas.
"""
import math
import time
import os
import sys
import ctypes
try:
    import psutil  # type: ignore
except Exception:  # ImportError ou outros
    psutil = None  # type: ignore
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
        # Lista unificada para evitar concatenações frequentes (bactérias depois predadores)
        self.all_agents = []

        # Sistemas
        self.interaction_system = InteractionSystem()
        self.reproduction_system = ReproductionSystem()
        self.death_system = DeathSystem(max_deaths_per_step=params.get('max_deaths_per_step', 1))
        self.collision_system = CollisionSystem()

        # Controladores
        self.food_controller = FoodController()
        self.population_controller = PopulationController()

        # Infraestrutura
        self.spatial_hash = None
        self.scene_query = None

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
        # Recursos (CPU/RAM)
        self.cpu_percent = 0.0
        self.mem_used_mb = 0.0
        self.mem_percent = 0.0
        self._psutil_process = psutil.Process() if psutil else None  # type: ignore
        self.resources_available = psutil is not None
        self.cpu_proc_percent = 0.0
        self._last_resource_sample = time.time()
        self._resource_log_emitted = False
        if psutil:
            try:
                # Primeiras chamadas para inicializar médias internas
                psutil.cpu_percent(interval=None)
                if self._psutil_process:
                    self._psutil_process.cpu_percent(interval=None)
            except Exception:
                pass
        else:
            # Log inicial para ajudar diagnóstico
            print(f"[engine] psutil não importado. Python: {sys.executable}")
        # Para fallback sem psutil
        self._fallback_last_wall = time.time()
        self._fallback_last_cpu = time.process_time()
        self._fallback_cpu_percent = 0.0

        # Estado para debugging
        self.selected_agent = None
        # Protótipos de agentes carregados via UI (dict name->data dict)
        self.loaded_agent_prototypes = {}
        self.current_agent_prototype = None  # nome da chave ativa
    
    def start(self):
        """Inicia a simulação."""
        self.running = True
        # Limpa/Configura cache multi_brain para evitar crescimento prévio
        try:
            from .brain import clear_multi_brain_cache, configure_multi_brain_cache
            configure_multi_brain_cache(max_entries=self.params.get('brain_cache_max_entries', 32),
                                        max_mb=self.params.get('brain_cache_max_mb', 512),
                                        disable=self.params.get('brain_cache_disable', False),
                                        log=self.params.get('brain_cache_log', False))
            clear_multi_brain_cache(verbose=True)
        except Exception:
            pass
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
            elapsed = now - self.last_fps_time
            if elapsed > 0:
                self.current_fps = self.frame_count / elapsed
            self.last_fps_time = now
            self.frame_count = 0
            # Atualiza métricas de recursos aproximadamente 1x por segundo
            # Tenta ativar psutil dinamicamente se ainda não disponível
            if not self.resources_available:
                try:
                    import psutil as _ps  # type: ignore
                    globals()['psutil'] = _ps  # substitui referência global
                    self._psutil_process = _ps.Process()
                    _ps.cpu_percent(interval=None)
                    self._psutil_process.cpu_percent(interval=None)
                    self.resources_available = True
                    if not self._resource_log_emitted:
                        print("[engine] psutil carregado dinamicamente; métricas de CPU/RAM ativadas.")
                        self._resource_log_emitted = True
                except Exception as e:
                    if not self._resource_log_emitted:
                        print(f"[engine] psutil indisponível (instale com 'pip install psutil'): {e}")
                        self._resource_log_emitted = True

            if psutil and self.resources_available:
                try:
                    self.cpu_percent = float(psutil.cpu_percent(interval=None))
                    if self._psutil_process:
                        self.cpu_proc_percent = float(self._psutil_process.cpu_percent(interval=None))
                        mem_info = self._psutil_process.memory_info()
                        self.mem_used_mb = mem_info.rss / (1024 * 1024)
                    vm = psutil.virtual_memory()  # type: ignore
                    self.mem_percent = float(getattr(vm, 'percent', 0.0))
                except Exception as e:
                    if not self._resource_log_emitted:
                        print(f"[engine] Falha coleta psutil: {e}")
                        self._resource_log_emitted = True
            else:
                # Fallback simples (estimativa) sem psutil
                wall_now = time.time()
                cpu_now = time.process_time()
                wall_dt = wall_now - self._fallback_last_wall
                cpu_dt = cpu_now - self._fallback_last_cpu
                if wall_dt > 0:
                    cores = max(1, os.cpu_count() or 1)
                    self._fallback_cpu_percent = min(100.0 * cores, max(0.0, (cpu_dt / wall_dt) * 100.0))
                self.cpu_percent = self._fallback_cpu_percent
                self.cpu_proc_percent = self._fallback_cpu_percent
                self.mem_used_mb, self.mem_percent = self._fallback_memory_usage()
                self._fallback_last_wall = wall_now
                self._fallback_last_cpu = cpu_now
            # DEBUG opcional: mem_diag_enable ativa logs periódicos de memória
            if self.params.get('mem_diag_enable', False):
                diag_interval = self.params.get('mem_diag_interval', 10.0)
                if getattr(self, '_last_mem_diag', 0) == 0:
                    self._last_mem_diag = now
                if now - getattr(self, '_last_mem_diag', 0) >= diag_interval:
                    self._last_mem_diag = now
                    try:
                        from .brain import get_multi_brain_cache_stats, estimate_brains_param_memory
                        cache_stats = get_multi_brain_cache_stats()
                        brain_stats = estimate_brains_param_memory([a.brain for a in self.all_agents if getattr(a,'brain',None)])
                    except Exception as e:
                        cache_stats = {'error': str(e)}
                        brain_stats = {'error': str(e)}
                    counts = {k: len(v) for k, v in self.entities.items()}
                    counts['all_agents'] = len(self.all_agents)
                    sel_act = []
                    if self.selected_agent and getattr(self.selected_agent, 'last_brain_activations', None):
                        sel_act = [len(layer) for layer in self.selected_agent.last_brain_activations]
                    print('[memdiag] RSS_MB=%.1f CPU%%=%s Agents=%s Archs=%s Params=%s ParamsMB=%.2f CacheMB=%.2f CacheEntries=%s' % (
                        self.mem_used_mb,
                        f'{self.cpu_proc_percent:.1f}',
                        counts.get('all_agents',0),
                        brain_stats.get('distinct_archs'),
                        brain_stats.get('total_params'),
                        brain_stats.get('approx_param_mb'),
                        cache_stats.get('approx_total_mb'),
                        cache_stats.get('entries')
                    ))
                    if cache_stats.get('largest'):
                        for entry in cache_stats['largest']:
                            print(f"[memdiag] cache_entry sizes={entry['sizes']} brains={entry['num_brains']} mb={entry['approx_mb']}")
                    if sel_act:
                        print(f"[memdiag] selected_agent_layers={sel_act}")
                    mem_warn = self.params.get('mem_warn_mb', 16000)
                    if self.mem_used_mb > mem_warn:
                        print('[memdiag][WARN] memória acima de limiar %d MB' % mem_warn)
                        try:
                            versions = {}
                            for a in self.all_agents:
                                v = getattr(a.brain, 'version', None)
                                if v is not None:
                                    versions[v] = versions.get(v, 0) + 1
                            top_versions = sorted(versions.items(), key=lambda x: -x[1])[:5]
                            print(f"[memdiag] top_brain_versions={top_versions}")
                        except Exception:
                            pass
    
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
        # Procura do mais próximo ao cursor (último desenhado = mais visível). Predadores são desenhados antes de bactérias.
        for agent in reversed(self.all_agents):
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

        from .entities import update_agents_batch
        with profile_section('agents_update'):
            # Agrupa agentes por classe e arquitetura do cérebro
            agent_groups = {}
            for agent in self.all_agents:
                key = (type(agent), tuple(agent.brain.sizes) if hasattr(agent.brain, 'sizes') else None)
                if key not in agent_groups:
                    agent_groups[key] = []
                agent_groups[key].append(agent)
            for group in agent_groups.values():
                update_agents_batch(group, dt, self.world, self.scene_query, self.params, selected_agent=self.selected_agent)

        with profile_section('interaction'):
            self.interaction_system.apply(
                self.entities['bacteria'], self.entities['predators'],
                self.entities['foods'], self.spatial_hash, self.params
            )

        with profile_section('reproduction'):
            new_agents = self.reproduction_system.apply(self.all_agents, self.params)

        if new_agents:
            for agent in new_agents:
                if agent.is_predator:
                    self.entities['predators'].append(agent)
                else:
                    self.entities['bacteria'].append(agent)
            # Acrescenta em bloco (ordem não crítica)
            self.all_agents.extend(new_agents)

        with profile_section('death'):
            surviving_bacteria, surviving_predators = self.death_system.apply(
                self.entities['bacteria'], self.entities['predators'], self.params
            )
            self.entities['bacteria'] = surviving_bacteria
            self.entities['predators'] = surviving_predators
            # Reconstroi lista unificada (custo O(n) mas uma vez por frame; elimina concatenações)
            self.all_agents = surviving_bacteria + surviving_predators

        with profile_section('collision'):
            self.collision_system.apply(self.all_agents, self.spatial_hash, self.params)
    
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
        
        elif command == 'spawn_loaded_agent':
            name = kwargs.get('prototype_name') or self.current_agent_prototype
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            if name and name in self.loaded_agent_prototypes:
                self._spawn_agent_from_prototype(self.loaded_agent_prototypes[name], world_x, world_y)
            else:
                print("Protótipo não encontrado para spawn.")
        
        else:
            print(f"Comando desconhecido: {command}")

    def _spawn_agent_from_prototype(self, data: dict, world_x: float, world_y: float):
        """Cria e insere um agente a partir de um dicionário de dados carregados."""
        try:
            agent_type = data.get('type','bacteria')
            from .brain import NeuralNet
            from .sensors import RetinaSensor
            from .actuators import Locomotion, EnergyModel
            from .entities import Bacteria, Predator
            import json as _json
            import math as _math
            # Cérebro
            sizes = _json.loads(data.get('brain_sizes','[]'))
            brain = NeuralNet(sizes if sizes else [1,2], init_std=0.01)
            weights=[]; biases=[]; idx=0
            while True:
                w_key=f'brain_weight_{idx}'; b_key=f'brain_bias_{idx}'
                if w_key not in data or b_key not in data: break
                weights.append(_json.loads(data[w_key])); biases.append(_json.loads(data[b_key])); idx+=1
            if weights and biases:
                brain.weights = weights; brain.biases = biases
            # Sensor
            def _f(k, default=0.0):
                try: return float(data.get(k, default))
                except Exception: return default
            def _i(k, default=0):
                try: return int(float(data.get(k, default)))
                except Exception: return default
            def _b(k, default=False):
                v = data.get(k, str(default)); return v in ('1','True','true','YES','yes')
            sensor = RetinaSensor(
                retina_count=_i('sensor_retina_count',18),
                vision_radius=_f('sensor_vision_radius',120.0),
                fov_degrees=_f('sensor_fov_degrees',180.0),
                skip=_i('sensor_skip',0),
                see_food=_b('sensor_see_food',True),
                see_bacteria=_b('sensor_see_bacteria',False),
                see_predators=_b('sensor_see_predators',False)
            )
            locomotion = Locomotion(max_speed=_f('locomotion_max_speed',300.0), max_turn=_f('locomotion_max_turn', _math.pi))
            def _pick_num(*names, default=0.0):
                for nm in names:
                    if nm in data:
                        return _f(nm, default)
                return default
            energy_model = EnergyModel(
                death_energy=_pick_num('energy_death_energy','death_energy', default=0.0),
                split_energy=_pick_num('energy_split_energy','split_energy', default=150.0),
                v0_cost=_pick_num('energy_v0_cost','metab_v0_cost','energy_loss_idle', default=0.5),
                vmax_cost=_pick_num('energy_vmax_cost','metab_vmax_cost','energy_loss_move', default=8.0),
                vmax_ref=_pick_num('energy_vmax_ref','locomotion_max_speed', default=300.0),
                energy_cap=_pick_num('energy_energy_cap','energy_cap', default=(600.0 if agent_type=='predator' else 400.0))
            )
            r = _f('r', 9.0)
            angle = _f('angle', 0.0)
            if agent_type == 'predator':
                agent = Predator(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            else:
                agent = Bacteria(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            # Ajustes adicionais
            agent.energy = _f('energy', 0.0)
            agent.age = _f('age', 0.0)
            # Inserção
            if agent.is_predator:
                self.entities['predators'].append(agent)
            else:
                self.entities['bacteria'].append(agent)
            self.all_agents.append(agent)
            self.selected_agent = agent
        except Exception as e:
            print(f"Falha ao spawnar protótipo: {e}")
    
    def _initialize_population(self):
        """Inicializa população baseada nos parâmetros."""
        # Limpa entidades existentes
        for entity_list in self.entities.values():
            entity_list.clear()
        all_entities = []
        self.all_agents.clear()

        # Cria bactérias
        bacteria_count = min(self.params.get('bacteria_count', 150), 10000)
        for _ in range(bacteria_count):
            bacterium = create_random_bacteria(all_entities, self.params,
                                               self.world.width, self.world.height)
            self.entities['bacteria'].append(bacterium)
            all_entities.append(bacterium)
            self.all_agents.append(bacterium)

        # Cria predadores se habilitados
        if self.params.get('predators_enabled', False):
            predator_count = min(self.params.get('predator_count', 0), 1000)
            for _ in range(predator_count):
                predator = create_random_predator(all_entities, self.params,
                                                  self.world.width, self.world.height)
                self.entities['predators'].append(predator)
                all_entities.append(predator)
                self.all_agents.append(predator)

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
            'cpu_percent': self.cpu_percent,  # CPU total
            'cpu_proc_percent': self.cpu_proc_percent,  # CPU só do processo
            'mem_used_mb': self.mem_used_mb,
            'mem_percent': self.mem_percent,
            'resources_available': self.resources_available,
            'fallback_metrics': (not self.resources_available),
            'max_speed': self.params.get('bacteria_max_speed', 300.0),
            'time_scale': self.params.get('time_scale', 1.0),
            'world_w': self.world.width,
            'world_h': self.world.height,
            'selected_agent': self.selected_agent,
            'show_selected_details': self.params.get('show_selected_details', True)
        }

    def _fallback_memory_usage(self):
        """Obtém memória aproximada (MB, %placeholder) sem psutil (Windows)."""
        try:
            class PROCESS_MEMORY_COUNTERS(ctypes.Structure):  # type: ignore
                _fields_ = [
                    ("cb", ctypes.c_ulong),
                    ("PageFaultCount", ctypes.c_ulong),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t),
                ]
            counters = PROCESS_MEMORY_COUNTERS()
            GetProcessMemoryInfo = ctypes.windll.psapi.GetProcessMemoryInfo  # type: ignore
            GetCurrentProcess = ctypes.windll.kernel32.GetCurrentProcess  # type: ignore
            handle = GetCurrentProcess()
            if GetProcessMemoryInfo(handle, ctypes.byref(counters), ctypes.sizeof(counters)):
                used_mb = counters.WorkingSetSize / (1024 * 1024)
                return used_mb, -1.0
        except Exception:
            return self.mem_used_mb or 0.0, -1.0
        return self.mem_used_mb or 0.0, -1.0
