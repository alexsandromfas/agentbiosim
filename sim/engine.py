"""
Motor principal da simulação - headless, não conhece UI.
Responsável pelo loop de simulação com tempo fixo e sistemas.
"""
import math
import time
import os
import sys
import ctypes
import threading
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
from .obstacles import ObstacleMap
from .sensors import SceneQuery
from .random_utils import apply_global_seed, normalize_seed
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
        self.obstacles = ObstacleMap()
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
        self._spatial_hash_dirty = True
        self.spatial_hash_rebuilds = 0
        self.spatial_hash_skips = 0

        # Renderização
        if headless or SimpleRenderer is None:
            self.renderer = None
        else:
            self.renderer = renderer or SimpleRenderer()

        # Controle de execução
        self.running = False
        self.command_queue = Queue()
        self.state_lock = threading.RLock()

        # Métricas
        self.total_simulation_time = 0.0
        self.frame_count = 0
        self.last_fps_time = time.time()
        self.current_fps = 0.0
        self._sim_time_accumulator = 0.0
        self.last_requested_sim_dt = 0.0
        self.last_simulated_dt = 0.0
        self.last_physics_steps = 0
        self.last_physics_dt = 0.0
        self.simulation_backlog = 0.0
        self.dropped_simulation_time = 0.0
        self.physics_steps_per_wall_second = 0.0
        self.effective_time_scale = 0.0
        self._physics_steps_since_fps = 0
        self._simulated_since_fps = 0.0
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
        self.selected_agents = set()
        self.agent_labels = {}
        self._next_agent_label_id = 1
        self._applied_random_seed = None
        # Protótipos de agentes carregados via UI (dict name->data dict)
        self.loaded_agent_prototypes = {}
        self.current_agent_prototype = None  # nome da chave ativa
        self.dragged_object = None
    
    def start(self, initialize: Optional[bool] = None):
        """Inicia a simulação.

        Por padrão, só cria uma população nova se o engine estiver vazio.
        Isso preserva substratos importados antes de clicar em "Iniciar".
        """
        self.running = True
        self._sim_time_accumulator = 0.0
        self.simulation_backlog = 0.0
        self.last_physics_steps = 0
        self.last_simulated_dt = 0.0
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
        has_loaded_state = bool(self.all_agents or self.entities['foods'])
        should_initialize = (not has_loaded_state) if initialize is None else bool(initialize)
        if should_initialize:
            self._initialize_population()
        else:
            self._spatial_hash_dirty = True
            self._update_spatial_hash(force=True)
    
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
        requested_sim_dt = real_dt * max(0.0, self.params.get('time_scale', 1.0))
        self.last_requested_sim_dt = requested_sim_dt
        
        simulated_dt = 0.0
        if requested_sim_dt > 0 and not self.params.get('paused', False):
            simulated_dt = self._simulate_physics_fixed(requested_sim_dt)
        else:
            self._clear_step_event_counters()
            self.last_simulated_dt = 0.0
            self.last_physics_steps = 0
            self.last_physics_dt = 0.0
            self.simulation_backlog = self._sim_time_accumulator
        
        # Atualiza métricas
        self.total_simulation_time += simulated_dt
        self._simulated_since_fps += simulated_dt
        self._physics_steps_since_fps += self.last_physics_steps
        self.frame_count += 1
        
        # Calcula FPS
        now = time.time()
        if now - self.last_fps_time >= 1.0:
            elapsed = now - self.last_fps_time
            if elapsed > 0:
                self.current_fps = self.frame_count / elapsed
                self.physics_steps_per_wall_second = self._physics_steps_since_fps / elapsed
                self.effective_time_scale = self._simulated_since_fps / elapsed
            self.last_fps_time = now
            self.frame_count = 0
            self._physics_steps_since_fps = 0
            self._simulated_since_fps = 0.0
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
        # Limpa tela usando cor configurável
        bg = self.params.get('substrate_bg_color', (10, 10, 20))
        try:
            bg_color = tuple(int(max(0, min(255, c))) for c in bg)
        except Exception:
            bg_color = (10, 10, 20)
        surface.fill(bg_color)
        
        # Desenha limites do mundo
        if show_world_bounds:
            self._draw_world_bounds(surface)
        
        # Desenha entidades
        for food in self.entities['foods']:
            self.renderer.draw_food(food, surface, self.camera)

        if getattr(self.obstacles, 'has_obstacles', False):
            self.renderer.draw_obstacles(self.obstacles, surface, self.camera)
        
        predator_show_vision = bool(self.params.get('predator_show_vision', False))
        bacteria_show_vision = bool(self.params.get('bacteria_show_vision', False))

        selected_agents = getattr(self, 'selected_agents', set())
        for predator in self.entities['predators']:
            selected = (predator is self.selected_agent) or (predator in selected_agents)
            self.renderer.draw_agent(predator, surface, self.camera, 
                                   show_head=True, show_vision=predator_show_vision, selected=selected)
        
        for bacterium in self.entities['bacteria']:
            selected = (bacterium is self.selected_agent) or (bacterium in selected_agents)
            self.renderer.draw_agent(bacterium, surface, self.camera,
                                   show_head=True, show_vision=bacteria_show_vision, selected=selected)
        
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

    def set_selected_agents(self, agents, primary: Optional[Agent] = None):
        """Define a selecao persistente de agentes vivos."""
        live = [agent for agent in agents if agent in self.all_agents]
        self.selected_agents = set(live)
        if primary in self.selected_agents:
            self.selected_agent = primary
        else:
            self.selected_agent = live[0] if live else None

    def _clear_selection(self):
        self.selected_agent = None
        self.selected_agents.clear()

    def create_agent_label(self, name: Optional[str] = None, color: Optional[tuple] = None) -> int:
        label_id = int(self._next_agent_label_id)
        self._next_agent_label_id += 1
        if color is None:
            palette = [
                (240, 94, 94), (90, 170, 255), (135, 220, 130),
                (245, 195, 75), (180, 130, 255), (255, 140, 85),
            ]
            color = palette[(label_id - 1) % len(palette)]
        self.agent_labels[label_id] = {
            'id': label_id,
            'name': name or f'Label {label_id}',
            'color': tuple(int(max(0, min(255, c))) for c in color[:3]),
            'show_chart': True,
        }
        return label_id

    def assign_label_to_agents(self, label_id: int, agents) -> int:
        if label_id not in self.agent_labels:
            return 0
        count = 0
        live = set(self.all_agents)
        label_color = tuple(self.agent_labels[label_id].get('color', (220, 220, 220)))
        for agent in agents:
            if agent not in live:
                continue
            if not hasattr(agent, 'label_ids'):
                agent.label_ids = set()
            agent.label_ids.add(label_id)
            agent.color = label_color
            count += 1
        return count

    def remove_label_from_agents(self, label_id: int, agents) -> int:
        count = 0
        for agent in agents:
            labels = getattr(agent, 'label_ids', None)
            if labels and label_id in labels:
                labels.discard(label_id)
                count += 1
        return count

    def delete_agent_label(self, label_id: int):
        self.agent_labels.pop(label_id, None)
        for agent in self.all_agents:
            labels = getattr(agent, 'label_ids', None)
            if labels:
                labels.discard(label_id)

    def get_agents_by_label(self, label_id: int):
        return [
            agent for agent in self.all_agents
            if label_id in (getattr(agent, 'label_ids', set()) or set())
        ]

    def select_label(self, label_id: int):
        agents = self.get_agents_by_label(label_id)
        self.set_selected_agents(agents)
        return agents

    def _cleanup_agent_labels(self):
        live_ids = set()
        for agent in self.all_agents:
            labels = getattr(agent, 'label_ids', None)
            if not labels:
                continue
            labels.intersection_update(self.agent_labels.keys())
            live_ids.update(labels)
        for label_id in list(self.agent_labels.keys()):
            if label_id not in live_ids:
                # Mantem labels vazias para o usuario poder reutilizar/avaliar.
                continue

    def get_agents_in_rect(self, x0: float, y0: float, x1: float, y1: float):
        min_x, max_x = sorted((float(x0), float(x1)))
        min_y, max_y = sorted((float(y0), float(y1)))
        return [
            agent for agent in self.all_agents
            if (agent.x + agent.r) >= min_x and (agent.x - agent.r) <= max_x
            and (agent.y + agent.r) >= min_y and (agent.y - agent.r) <= max_y
        ]

    @staticmethod
    def _point_in_polygon(x: float, y: float, points) -> bool:
        inside = False
        if len(points) < 3:
            return False
        j = len(points) - 1
        for i, (xi, yi) in enumerate(points):
            xj, yj = points[j]
            if ((yi > y) != (yj > y)) and (x < (xj - xi) * (y - yi) / ((yj - yi) or 1e-12) + xi):
                inside = not inside
            j = i
        return inside

    def get_agents_in_polygon(self, points):
        if len(points) < 3:
            return []
        min_x = min(p[0] for p in points)
        max_x = max(p[0] for p in points)
        min_y = min(p[1] for p in points)
        max_y = max(p[1] for p in points)
        return [
            agent for agent in self.get_agents_in_rect(min_x, min_y, max_x, max_y)
            if self._point_in_polygon(agent.x, agent.y, points)
        ]

    def get_object_at_position(self, world_x: float, world_y: float):
        """Encontra agente ou comida na posição do mundo."""
        agent = self.get_agent_at_position(world_x, world_y)
        if agent is not None:
            return agent
        for food in reversed(self.entities['foods']):
            distance = math.hypot(food.x - world_x, food.y - world_y)
            if distance <= food.r:
                return food
        return None

    def can_place_circle(self, world_x: float, world_y: float, radius: float) -> bool:
        """Valida substrato e obstáculos para criação/movimento de objetos."""
        radius = max(0.0, float(radius))
        if not self.world.is_inside(world_x, world_y, radius):
            return False
        if self.obstacles.circle_overlaps(world_x, world_y, radius):
            return False
        return True
    
    def add_food_at(self, world_x: float, world_y: float):
        """Adiciona comida na posição especificada."""
        max_r = float(self.params.get('food_max_r', 5.0))
        if not self.can_place_circle(world_x, world_y, max_r):
            return None
        food = create_random_food(self.entities['foods'], self.params, 
                                self.world.width, self.world.height, 
                                at=(world_x, world_y))
        if food is None or not self.can_place_circle(food.x, food.y, food.r):
            return None
        self.entities['foods'].append(food)
        self._spatial_hash_dirty = True
        return food
    
    def add_bacteria_at(self, world_x: float, world_y: float):
        """Adiciona bactéria na posição especificada."""
        radius = float(self.params.get('bacteria_body_size', self.params.get('bacteria_max_r', 12.0)))
        if not self.can_place_circle(world_x, world_y, radius):
            return None
        all_entities = (self.entities['bacteria'] + self.entities['predators'] + 
                       self.entities['foods'])
        bacterium = create_random_bacteria(all_entities, self.params,
                                         self.world.width, self.world.height,
                                         at=(world_x, world_y))
        if bacterium is None or not self.can_place_circle(bacterium.x, bacterium.y, bacterium.r):
            return None
        self.entities['bacteria'].append(bacterium)
        self.all_agents.append(bacterium)
        self._spatial_hash_dirty = True
        return bacterium

    def remove_object_at(self, world_x: float, world_y: float) -> bool:
        """Remove agente ou comida sob o cursor."""
        obj = self.get_object_at_position(world_x, world_y)
        if obj is None:
            return False
        if getattr(obj, 'type_code', None) == 0:
            try:
                self.entities['foods'].remove(obj)
            except ValueError:
                return False
        else:
            key = 'predators' if getattr(obj, 'is_predator', False) else 'bacteria'
            try:
                self.entities[key].remove(obj)
            except ValueError:
                pass
            try:
                self.all_agents.remove(obj)
            except ValueError:
                pass
            if self.selected_agent is obj:
                self.selected_agent = None
            self.selected_agents.discard(obj)
            if self.selected_agent is None and self.selected_agents:
                self.selected_agent = next(iter(self.selected_agents), None)
        if self.dragged_object is obj:
            self.dragged_object = None
        self._spatial_hash_dirty = True
        return True

    def remove_selected_agents(self) -> int:
        """Remove todos os organismos atualmente selecionados."""
        victims = set(getattr(self, 'selected_agents', set()) or set())
        if self.selected_agent is not None:
            victims.add(self.selected_agent)
        live_victims = {agent for agent in victims if agent in self.all_agents}
        if not live_victims:
            self._clear_selection()
            return 0

        for key in ('bacteria', 'predators'):
            self.entities[key][:] = [agent for agent in self.entities[key] if agent not in live_victims]
        self.all_agents[:] = [agent for agent in self.all_agents if agent not in live_victims]
        if self.dragged_object in live_victims:
            self.dragged_object = None
        self._clear_selection()
        self._spatial_hash_dirty = True
        return len(live_victims)

    def move_object_to(self, obj, world_x: float, world_y: float) -> bool:
        """Move objeto existente respeitando substrato e obstáculos."""
        if obj is None:
            return False
        radius = float(getattr(obj, 'r', 0.0))
        if not self.can_place_circle(world_x, world_y, radius):
            return False
        obj.x = float(world_x)
        obj.y = float(world_y)
        if hasattr(obj, 'vx'):
            obj.vx = 0.0
            obj.vy = 0.0
        self._spatial_hash_dirty = True
        return True

    def paint_obstacle(self, x0: float, y0: float, x1: float, y1: float,
                       radius: float, color: tuple[int, int, int], erase: bool = False) -> int:
        """Desenha ou apaga barreiras sólidas."""
        if erase:
            changed = self.obstacles.erase_brush_line(x0, y0, x1, y1, radius, world=self.world)
        else:
            changed = self.obstacles.add_brush_line(x0, y0, x1, y1, radius, color, world=self.world)
        if changed:
            self.obstacles.remove_food_overlaps(self.entities['foods'])
            self._resolve_obstacle_collisions()
            self._spatial_hash_dirty = True
        return changed
    
    def _physics_dt(self) -> float:
        hz = max(1.0, float(self.params.get('physics_steps_per_second', 30)))
        return 1.0 / hz

    def _clear_step_event_counters(self):
        self.food_controller.last_foods_added = 0
        self.food_controller.last_foods_removed = 0
        self.interaction_system.last_foods_eaten = 0
        self.interaction_system.last_agents_predated = 0
        self.reproduction_system.last_births = []
        self.reproduction_system.last_blocked_by_age = 0
        self.reproduction_system.last_blocked_by_cooldown = 0
        self.death_system.last_deaths = []
        self.collision_system.last_collisions_resolved = 0

    def _simulate_physics_fixed(self, requested_dt: float) -> float:
        """Avanca a simulacao usando dt fisico fixo."""
        physics_dt = self._physics_dt()
        max_substeps = max(1, int(self.params.get('max_physics_steps_per_frame', 8)))
        max_backlog = max(physics_dt, float(self.params.get('max_physics_backlog_seconds', 2.0)))

        self._sim_time_accumulator += max(0.0, requested_dt)
        if self._sim_time_accumulator > max_backlog:
            dropped = self._sim_time_accumulator - max_backlog
            self._sim_time_accumulator = max_backlog
            self.dropped_simulation_time += dropped

        available_steps = int((self._sim_time_accumulator + physics_dt * 1e-9) / physics_dt)
        steps = min(available_steps, max_substeps)

        foods_added = foods_removed = foods_eaten = predations = 0
        births = []
        deaths = []
        births_blocked_age = births_blocked_cooldown = collisions = 0

        for _ in range(steps):
            self._simulate_substep(physics_dt)
            foods_added += int(getattr(self.food_controller, 'last_foods_added', 0))
            foods_removed += int(getattr(self.food_controller, 'last_foods_removed', 0))
            foods_eaten += int(getattr(self.interaction_system, 'last_foods_eaten', 0))
            predations += int(getattr(self.interaction_system, 'last_agents_predated', 0))
            births.extend(getattr(self.reproduction_system, 'last_births', []) or [])
            deaths.extend(getattr(self.death_system, 'last_deaths', []) or [])
            births_blocked_age += int(getattr(self.reproduction_system, 'last_blocked_by_age', 0))
            births_blocked_cooldown += int(getattr(self.reproduction_system, 'last_blocked_by_cooldown', 0))
            collisions += int(getattr(self.collision_system, 'last_collisions_resolved', 0))

        simulated_dt = steps * physics_dt
        if steps < available_steps:
            # Se o motor nao consegue processar todos os passos pedidos neste
            # frame, descartamos o excedente em vez de tentar recuperar depois.
            # Isso preserva dt fixo e evita a espiral: frame lento -> dt maior
            # -> mais substeps -> frame ainda mais lento.
            dropped = max(0.0, self._sim_time_accumulator - simulated_dt)
            self.dropped_simulation_time += dropped
            self._sim_time_accumulator = 0.0
        else:
            self._sim_time_accumulator = max(0.0, self._sim_time_accumulator - simulated_dt)
            if self._sim_time_accumulator < physics_dt * 1e-6:
                self._sim_time_accumulator = 0.0

        self.last_physics_steps = steps
        self.last_physics_dt = physics_dt if steps else 0.0
        self.last_simulated_dt = simulated_dt
        self.simulation_backlog = self._sim_time_accumulator

        self.food_controller.last_foods_added = foods_added
        self.food_controller.last_foods_removed = foods_removed
        self.interaction_system.last_foods_eaten = foods_eaten
        self.interaction_system.last_agents_predated = predations
        self.reproduction_system.last_births = births
        self.reproduction_system.last_blocked_by_age = births_blocked_age
        self.reproduction_system.last_blocked_by_cooldown = births_blocked_cooldown
        self.death_system.last_deaths = deaths
        self.collision_system.last_collisions_resolved = collisions
        return simulated_dt

    def _simulate_physics(self, world_dt: float) -> float:
        """Compatibilidade para chamadas antigas: delega para o passo fixo."""
        return self._simulate_physics_fixed(world_dt)

    def _resolve_obstacle_collisions(self, frozen_agents: set | None = None, max_iterations: int = 3) -> int:
        """Empurra agentes para fora das barreiras desenhadas."""
        if not getattr(self.obstacles, 'has_obstacles', False):
            return 0
        frozen_agents = frozen_agents or set()
        resolved = 0
        for _iteration in range(max(1, int(max_iterations))):
            pass_resolved = 0
            for agent in self.all_agents:
                if agent in frozen_agents:
                    continue
                pass_resolved += self.obstacles.resolve_agent(agent)
                agent.x, agent.y = self.world.clamp_position(agent.x, agent.y, agent.r)
            resolved += pass_resolved
            if pass_resolved == 0:
                break
        return resolved
    
    def _simulate_substep(self, dt: float):
        """Executa um substep de física."""
        step_params = self._params_snapshot()
        dragged_agent = self.dragged_object if self.dragged_object in self.all_agents else None
        frozen_agents = {dragged_agent} if dragged_agent is not None else set()
        if dragged_agent is not None:
            dragged_agent.vx = 0.0
            dragged_agent.vy = 0.0
        with profile_section('food_control'):
            target_food = step_params.get('food_target', 300)
            new_foods = self.food_controller.update(
                self.entities['foods'], target_food, self.world.width, self.world.height, step_params, dt,
                obstacle_map=self.obstacles
            )
            self.entities['foods'].extend(new_foods)
            if new_foods or getattr(self.food_controller, 'last_foods_removed', 0):
                self._spatial_hash_dirty = True

        # Cena atual para sensores: inclui comida recém-reposta e posições pré-movimento.
        with profile_section('spatial_hash'):
            self._update_spatial_hash(force=self._spatial_hash_dirty)

        from .entities import update_agents_batch
        with profile_section('agents_update'):
            # Agrupa agentes por classe e arquitetura do cérebro
            agent_groups = {}
            for agent in self.all_agents:
                if agent in frozen_agents:
                    continue
                key = (type(agent), tuple(agent.brain.sizes) if hasattr(agent.brain, 'sizes') else None)
                if key not in agent_groups:
                    agent_groups[key] = []
                agent_groups[key].append(agent)
            for group in agent_groups.values():
                update_agents_batch(group, dt, self.world, self.scene_query, step_params, selected_agent=self.selected_agent)

        with profile_section('obstacle_collision'):
            if self._resolve_obstacle_collisions(frozen_agents=frozen_agents):
                self._spatial_hash_dirty = True

        # Agentes se moveram; interações precisam do hash com as posições atuais.
        with profile_section('spatial_hash'):
            self._update_spatial_hash(force=True)

        topology_changed = False
        with profile_section('interaction'):
            removed_agents = self.interaction_system.apply(
                self.entities['bacteria'], self.entities['predators'],
                self.entities['foods'], self.spatial_hash, step_params,
                frozen_agents=frozen_agents
            )
            if removed_agents:
                topology_changed = True
                self.all_agents = [a for a in self.all_agents if a not in removed_agents]
                if self.selected_agent in removed_agents:
                    self.selected_agent = None
                self.selected_agents.difference_update(removed_agents)
                if self.selected_agent is None and self.selected_agents:
                    self.selected_agent = next(iter(self.selected_agents), None)
                if self.dragged_object in removed_agents:
                    self.dragged_object = None

        with profile_section('reproduction'):
            reproductive_agents = [a for a in self.all_agents if a not in frozen_agents]
            new_agents = self.reproduction_system.apply(reproductive_agents, step_params)

        if new_agents:
            for agent in new_agents:
                if agent.is_predator:
                    self.entities['predators'].append(agent)
                else:
                    self.entities['bacteria'].append(agent)
            # Acrescenta em bloco (ordem não crítica)
            self.all_agents.extend(new_agents)
            self._resolve_obstacle_collisions()
            topology_changed = True

        with profile_section('death'):
            before_death_count = len(self.entities['bacteria']) + len(self.entities['predators'])
            surviving_bacteria, surviving_predators = self.death_system.apply(
                self.entities['bacteria'], self.entities['predators'], step_params
            )
            self.entities['bacteria'] = surviving_bacteria
            self.entities['predators'] = surviving_predators
            # Reconstroi lista unificada (custo O(n) mas uma vez por frame; elimina concatenações)
            self.all_agents = surviving_bacteria + surviving_predators
            after_death_count = len(self.all_agents)
            if after_death_count != before_death_count:
                topology_changed = True
                live_set = set(self.all_agents)
                self.selected_agents.intersection_update(live_set)
                if self.selected_agent not in live_set:
                    self.selected_agent = next(iter(self.selected_agents), None)

        if topology_changed:
            # Nascimentos, mortes ou predação mudam os objetos presentes no broad-phase.
            with profile_section('spatial_hash'):
                self._update_spatial_hash(force=True)

        with profile_section('collision'):
            collision_agents = [a for a in self.all_agents if a not in frozen_agents]
            collisions_resolved = self.collision_system.apply(collision_agents, self.spatial_hash, step_params)
            if collisions_resolved:
                self._spatial_hash_dirty = True
            # Colisões agente-agente podem empurrar organismos para dentro de
            # uma barreira. Resolva obstáculos novamente para manter divisórias
            # estanques.
            if self._resolve_obstacle_collisions(frozen_agents=frozen_agents):
                self._spatial_hash_dirty = True
    
    def _params_snapshot(self):
        data = getattr(self.params, '_data', None)
        if isinstance(data, dict):
            return dict(data)
        return self.params

    def _spatial_bounds(self):
        """Bounds do broad-phase para cobrir todo o substrato ativo."""
        if getattr(self.world, 'shape', 'rectangular') == 'circular':
            radius = max(1.0, float(getattr(self.world, 'radius', 1.0)))
            return (
                float(self.world.cx) - radius,
                float(self.world.cy) - radius,
                radius * 2.0,
                radius * 2.0,
            )
        return 0.0, 0.0, float(self.world.width), float(self.world.height)

    def _update_spatial_hash(self, force: bool = True):
        """Atualiza ou recria spatial hash."""
        if (not force and not self._spatial_hash_dirty and
            self.scene_query is not None and
            (self.spatial_hash is not None or not self.params.get('use_spatial', True))):
            self.spatial_hash_skips += 1
            return False
        if not self.params.get('use_spatial', True):
            self.spatial_hash = None
            self.scene_query = SceneQuery(None, self.entities, self.params)
            self._spatial_hash_dirty = False
            self.spatial_hash_rebuilds += 1
            return True
        
        # Calcula tamanho de célula baseado no maior objeto
        max_radius = max(
            self.params.get('food_max_r', 5.0),
            self.params.get('bacteria_max_r', 12.0),
            self.params.get('predator_max_r', 18.0)
        )
        cell_size = max_radius * 2.0
        min_x, min_y, bounds_w, bounds_h = self._spatial_bounds()
        
        # Reutiliza hash existente se possível
        if (self.spatial_hash and 
            self.params.get('reuse_spatial_grid', True) and
            abs(self.spatial_hash.cell_size - cell_size) < 1e-6 and
            abs(self.spatial_hash.width - bounds_w) < 1e-6 and
            abs(self.spatial_hash.height - bounds_h) < 1e-6 and
            abs(getattr(self.spatial_hash, 'min_x', 0.0) - min_x) < 1e-6 and
            abs(getattr(self.spatial_hash, 'min_y', 0.0) - min_y) < 1e-6):
            # Reutiliza: apenas limpa e reinsere
            self.spatial_hash.clear()
        else:
            # Recria
            self.spatial_hash = SpatialHash(cell_size, bounds_w, bounds_h, min_x=min_x, min_y=min_y)
        
        # Insere todas as entidades
        for food in self.entities['foods']:
            self.spatial_hash.insert(food, food.x, food.y, food.r)
        
        for bacterium in self.entities['bacteria']:
            self.spatial_hash.insert(bacterium, bacterium.x, bacterium.y, bacterium.r)
        
        for predator in self.entities['predators']:
            self.spatial_hash.insert(predator, predator.x, predator.y, predator.r)
        
        # Atualiza scene query
        self.scene_query = SceneQuery(self.spatial_hash, self.entities, self.params)
        self._spatial_hash_dirty = False
        self.spatial_hash_rebuilds += 1
        return True
    
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
            agent = self.get_agent_at_position(world_x, world_y)
            self.set_selected_agents([agent] if agent is not None else [], primary=agent)

        elif command == 'select_agents_rect':
            agents = self.get_agents_in_rect(
                kwargs.get('x0', 0), kwargs.get('y0', 0),
                kwargs.get('x1', 0), kwargs.get('y1', 0),
            )
            self.set_selected_agents(agents)

        elif command == 'select_agents_lasso':
            points = kwargs.get('points') or []
            agents = self.get_agents_in_polygon(points)
            self.set_selected_agents(agents)

        elif command == 'clear_selection':
            self._clear_selection()

        elif command == 'select_or_add_food':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            agent = self.get_agent_at_position(world_x, world_y)
            if agent:
                self.set_selected_agents([agent], primary=agent)
            else:
                self.add_food_at(world_x, world_y)
                self._clear_selection()
        
        elif command == 'add_food':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.add_food_at(world_x, world_y)
        
        elif command == 'add_bacteria':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.add_bacteria_at(world_x, world_y)

        elif command == 'paint_obstacle':
            self.paint_obstacle(
                kwargs.get('x0', kwargs.get('world_x', 0)),
                kwargs.get('y0', kwargs.get('world_y', 0)),
                kwargs.get('x1', kwargs.get('world_x', 0)),
                kwargs.get('y1', kwargs.get('world_y', 0)),
                kwargs.get('radius', 8.0),
                tuple(kwargs.get('color', (95, 95, 105))),
                bool(kwargs.get('erase', False)),
            )

        elif command == 'remove_object_at':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.remove_object_at(world_x, world_y)

        elif command == 'remove_selected_agents':
            self.remove_selected_agents()
        
        elif command == 'reset_population':
            self._initialize_population()
            self._clear_selection()
        
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
            if not self.can_place_circle(world_x, world_y, r):
                return
            if agent_type == 'predator':
                agent = Predator(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            else:
                agent = Bacteria(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            # Ajustes adicionais
            agent.energy = _f('energy', 0.0)
            agent.age = _f('age', 0.0)
            agent.food_eaten_count = _i('food_eaten_count', 0)
            agent.food_energy_eaten_total = _f('food_energy_eaten_total', 0.0)
            agent.prey_eaten_count = _i('prey_eaten_count', 0)
            agent.prey_energy_eaten_total = _f('prey_energy_eaten_total', 0.0)
            if 'last_reproduction_age' in data:
                agent.last_reproduction_age = _f('last_reproduction_age', agent.age)
            # Cor importada (suporta JSON array ou legacy tuple string)
            try:
                if 'color' in data:
                    try:
                        c = _json.loads(data.get('color'))
                        if isinstance(c, (list, tuple)) and len(c) >= 3:
                            agent.color = (int(c[0]), int(c[1]), int(c[2]))
                    except Exception:
                        # try parse simple tuple-like string '(r, g, b)'
                        raw = data.get('color')
                        if isinstance(raw, str) and raw.startswith('('):
                            try:
                                parts = raw.strip('() ').split(',')
                                r,g,b = [int(float(p.strip())) for p in parts[:3]]
                                agent.color = (r,g,b)
                            except Exception:
                                pass
            except Exception:
                pass
            # Inserção
            if agent.is_predator:
                self.entities['predators'].append(agent)
            else:
                self.entities['bacteria'].append(agent)
            self.all_agents.append(agent)
            self.set_selected_agents([agent], primary=agent)
            self._spatial_hash_dirty = True
            try:
                from .brain import clear_multi_brain_cache
                clear_multi_brain_cache()
            except Exception:
                pass
        except Exception as e:
            print(f"Falha ao spawnar protótipo: {e}")
    
    def _apply_configured_random_seed(self, force: bool = False) -> Optional[int]:
        seed = normalize_seed(self.params.get('random_seed', -1))
        if seed is None:
            return None
        if force or self._applied_random_seed != seed:
            apply_global_seed(seed)
            self._applied_random_seed = seed
        return seed

    def _initialize_population(self):
        """Inicializa população baseada nos parâmetros."""
        self._apply_configured_random_seed(force=True)
        try:
            from .brain import clear_multi_brain_cache
            clear_multi_brain_cache()
        except Exception:
            pass

        # Limpa entidades existentes
        for entity_list in self.entities.values():
            entity_list.clear()
        all_entities = []
        self.all_agents.clear()
        self._clear_selection()
        self.agent_labels.clear()
        self._next_agent_label_id = 1
        self._sim_time_accumulator = 0.0
        self.simulation_backlog = 0.0
        self.last_physics_steps = 0
        self.last_simulated_dt = 0.0

        # Cria bactérias
        bacteria_count = min(self.params.get('bacteria_count', 150), 10000)
        for _ in range(bacteria_count):
            bacterium = None
            for _attempt in range(120 if self.obstacles.has_obstacles else 1):
                candidate = create_random_bacteria(all_entities, self.params,
                                                   self.world.width, self.world.height)
                if self.can_place_circle(candidate.x, candidate.y, candidate.r):
                    bacterium = candidate
                    break
            if bacterium is None:
                continue
            self.entities['bacteria'].append(bacterium)
            all_entities.append(bacterium)
            self.all_agents.append(bacterium)

        # Cria predadores se habilitados
        if self.params.get('predators_enabled', False):
            predator_count = min(self.params.get('predator_count', 0), 1000)
            for _ in range(predator_count):
                predator = None
                for _attempt in range(120 if self.obstacles.has_obstacles else 1):
                    candidate = create_random_predator(all_entities, self.params,
                                                      self.world.width, self.world.height)
                    if self.can_place_circle(candidate.x, candidate.y, candidate.r):
                        predator = candidate
                        break
                if predator is None:
                    continue
                self.entities['predators'].append(predator)
                all_entities.append(predator)
                self.all_agents.append(predator)

        # Cria comida
        food_target = self.params.get('food_target', 50)
        for _ in range(food_target):
            food = None
            for _attempt in range(120 if self.obstacles.has_obstacles else 1):
                candidate = create_random_food(self.entities['foods'], self.params,
                                               self.world.width, self.world.height)
                if candidate is not None and self.can_place_circle(candidate.x, candidate.y, candidate.r):
                    food = candidate
                    break
            if food is None:
                continue
            self.entities['foods'].append(food)
        self._resolve_obstacle_collisions()
        self._spatial_hash_dirty = True
        self._update_spatial_hash(force=True)
    
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
            'physics_steps_per_wall_second': self.physics_steps_per_wall_second,
            'effective_time_scale': self.effective_time_scale,
            'simulation_backlog': self.simulation_backlog,
            'last_physics_steps': self.last_physics_steps,
            'last_physics_dt': self.last_physics_dt,
            'dropped_simulation_time': self.dropped_simulation_time,
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
            'obstacle_count': len(self.obstacles),
            'hide_overlay': True,
            'show_selected_details': False
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
