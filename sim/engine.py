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
import json
import random
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


def _truthy(value: Any, default: bool = False) -> bool:
    if isinstance(value, bool):
        return value
    if value is None:
        return default
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "sim"}
    return bool(value)


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
        self._spatial_debug_font = None

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
        self._prototype_revision = 0
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
                                        disable=self.params.get('brain_cache_disable', True),
                                        log=self.params.get('brain_cache_log', False),
                                        numba_forward=self.params.get('use_numba_brain_forward', False),
                                        numba_min_batch=self.params.get('numba_brain_forward_min_batch', 256))
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
        setattr(self.renderer, 'interpolation_alpha', self._render_interpolation_alpha())
        self._draw_scene_background(surface)
        visible_bounds = self._visible_world_bounds(surface)
        
        if bool(self.params.get('show_spatial_hash', False)):
            self._draw_spatial_hash_grid(surface, visible_bounds)
        # Desenha limites do mundo
        if show_world_bounds:
            self._draw_world_bounds(surface)
        
        # Desenha entidades
        for food in self.entities['foods']:
            if not self._is_object_visible(food, visible_bounds):
                continue
            self.renderer.draw_food(food, surface, self.camera)

        if getattr(self.obstacles, 'has_obstacles', False):
            self.renderer.draw_obstacles(self.obstacles, surface, self.camera, visible_bounds=visible_bounds)
        
        predator_show_vision = bool(self.params.get('predator_show_vision', False))
        bacteria_show_vision = bool(self.params.get('bacteria_show_vision', False))

        selected_agents = getattr(self, 'selected_agents', set())
        for predator in self.entities['predators']:
            if not self._is_object_visible(predator, visible_bounds):
                continue
            selected = (predator is self.selected_agent) or (predator in selected_agents)
            self.renderer.draw_agent(predator, surface, self.camera, 
                                   show_head=True, show_vision=predator_show_vision, selected=selected)
        
        for bacterium in self.entities['bacteria']:
            if not self._is_object_visible(bacterium, visible_bounds):
                continue
            selected = (bacterium is self.selected_agent) or (bacterium in selected_agents)
            self.renderer.draw_agent(bacterium, surface, self.camera,
                                   show_head=True, show_vision=bacteria_show_vision, selected=selected)
        
        # Desenha overlay de informações
        info = self._gather_render_info()
        self.renderer.draw_overlay(surface, info)

    def _visible_world_bounds(self, surface, margin_px: float = 96.0):
        zoom = max(1e-6, float(getattr(self.camera, 'zoom', 1.0)))
        margin = float(margin_px) / zoom
        width = float(surface.get_width()) / zoom
        height = float(surface.get_height()) / zoom
        return (
            float(self.camera.x) - margin,
            float(self.camera.y) - margin,
            float(self.camera.x) + width + margin,
            float(self.camera.y) + height + margin,
        )

    def _render_interpolation_alpha(self) -> float:
        if not bool(self.params.get('render_interpolation_enabled', False)):
            return 1.0
        if bool(self.params.get('paused', False)):
            return 1.0
        physics_dt = self._physics_dt()
        if physics_dt <= 1e-12:
            return 1.0
        return max(0.0, min(1.0, float(self._sim_time_accumulator) / physics_dt))

    @staticmethod
    def _is_object_visible(obj, bounds) -> bool:
        min_x, min_y, max_x, max_y = bounds
        x = float(getattr(obj, 'x', 0.0))
        y = float(getattr(obj, 'y', 0.0))
        r = float(getattr(obj, 'r', 0.0))
        return x + r >= min_x and x - r <= max_x and y + r >= min_y and y - r <= max_y

    @staticmethod
    def _render_color(value, fallback=(10, 10, 20)):
        try:
            parts = [int(float(c)) for c in list(value)[:3]]
            if len(parts) < 3:
                raise ValueError
            return tuple(max(0, min(255, c)) for c in parts[:3])
        except Exception:
            return tuple(fallback)

    @staticmethod
    def _lerp_color(c0, c1, t: float):
        t = max(0.0, min(1.0, float(t)))
        return (
            int(c0[0] + (c1[0] - c0[0]) * t),
            int(c0[1] + (c1[1] - c0[1]) * t),
            int(c0[2] + (c1[2] - c0[2]) * t),
        )

    def _draw_scene_background(self, surface):
        """Desenha o fundo global e a area do substrato."""
        import pygame

        default_bg = self._render_color(self.params.get('substrate_bg_color', (10, 10, 20)))
        bg_top = self._render_color(self.params.get('background_color_top', default_bg), default_bg)
        bg_bottom = self._render_color(self.params.get('background_color_bottom', bg_top), bg_top)
        if bool(self.params.get('background_gradient_enabled', False)) and bg_top != bg_bottom:
            h = max(1, int(surface.get_height()))
            w = int(surface.get_width())
            for y in range(h):
                color = self._lerp_color(bg_top, bg_bottom, y / max(1, h - 1))
                pygame.draw.line(surface, color, (0, y), (w, y))
        else:
            surface.fill(bg_top)

        sub_top = self._render_color(self.params.get('substrate_color_top', default_bg), default_bg)
        sub_bottom = self._render_color(self.params.get('substrate_color_bottom', sub_top), sub_top)
        sub_gradient = bool(self.params.get('substrate_gradient_enabled', False)) and sub_top != sub_bottom
        if getattr(self.world, 'shape', 'rectangular') == 'circular':
            center_screen = self.camera.world_to_screen(self.world.cx, self.world.cy)
            radius_screen = int(max(0.0, self.world.radius * self.camera.zoom))
            if radius_screen <= 0:
                return
            cx, cy = int(center_screen[0]), int(center_screen[1])
            if not sub_gradient:
                pygame.draw.circle(surface, sub_top, (cx, cy), radius_screen)
                return
            y0 = max(0, cy - radius_screen)
            y1 = min(surface.get_height() - 1, cy + radius_screen)
            denom = max(1, radius_screen * 2)
            for y in range(y0, y1 + 1):
                dy = y - cy
                half_width = int(math.sqrt(max(0.0, radius_screen * radius_screen - dy * dy)))
                x0 = max(0, cx - half_width)
                x1 = min(surface.get_width() - 1, cx + half_width)
                if x1 < x0:
                    continue
                color = self._lerp_color(sub_top, sub_bottom, (y - (cy - radius_screen)) / denom)
                pygame.draw.line(surface, color, (x0, y), (x1, y))
        else:
            top_left = self.camera.world_to_screen(0, 0)
            bottom_right = self.camera.world_to_screen(self.world.width, self.world.height)
            left = int(min(top_left[0], bottom_right[0]))
            right = int(max(top_left[0], bottom_right[0]))
            top = int(min(top_left[1], bottom_right[1]))
            bottom = int(max(top_left[1], bottom_right[1]))
            rect = pygame.Rect(left, top, max(0, right - left), max(0, bottom - top))
            if rect.width <= 0 or rect.height <= 0:
                return
            if not sub_gradient:
                surface.fill(sub_top, rect)
                return
            y0 = max(0, rect.top)
            y1 = min(surface.get_height() - 1, rect.bottom)
            denom = max(1, rect.height)
            x0 = max(0, rect.left)
            x1 = min(surface.get_width() - 1, rect.right)
            for y in range(y0, y1 + 1):
                color = self._lerp_color(sub_top, sub_bottom, (y - rect.top) / denom)
                pygame.draw.line(surface, color, (x0, y), (x1, y))
    
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

    def ensure_default_agent_label(self) -> int:
        """Garante uma label base; novos organismos nunca devem ficar sem grupo."""
        if self.agent_labels:
            return int(sorted(self.agent_labels.keys())[0])
        try:
            default_count = max(0, int(self.params.get('bacteria_count', 150)))
        except Exception:
            default_count = 150
        return self.create_agent_label(name='organismo_1', color=(135, 220, 130), max_limit=default_count)

    def create_agent_label(self, name: Optional[str] = None, color: Optional[tuple] = None,
                           min_limit: int = 0, max_limit: int = 0) -> int:
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
            'name': name or f'organismo_{label_id}',
            'color': tuple(int(max(0, min(255, c))) for c in color[:3]),
            'show_chart': True,
            'min_limit': max(0, int(min_limit or 0)),
            'max_limit': max(0, int(max_limit or 0)),
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
            # Labels agora representam a linhagem/grupo principal do organismo.
            # Ao atribuir a uma nova label, removemos a anterior para evitar que
            # limites populacionais de grupos antigos continuem afetando o agente.
            agent.label_ids = {label_id}
            agent.color = label_color
            count += 1
        return count

    def remove_label_from_agents(self, label_id: int, agents) -> int:
        count = 0
        fallback_id = self.ensure_default_agent_label()
        fallback_color = tuple(self.agent_labels[fallback_id].get('color', (220, 220, 220)))
        for agent in agents:
            labels = getattr(agent, 'label_ids', None)
            if labels and label_id in labels:
                labels.discard(label_id)
                if not labels:
                    agent.label_ids = {fallback_id}
                    agent.color = fallback_color
                count += 1
        return count

    def delete_agent_label(self, label_id: int):
        self.agent_labels.pop(label_id, None)
        fallback_id = self.ensure_default_agent_label()
        fallback_color = tuple(self.agent_labels[fallback_id].get('color', (220, 220, 220)))
        for agent in self.all_agents:
            labels = getattr(agent, 'label_ids', None)
            if labels:
                labels.discard(label_id)
            if not getattr(agent, 'label_ids', set()):
                agent.label_ids = {fallback_id}
                agent.color = fallback_color

    def get_agents_by_label(self, label_id: int):
        return [
            agent for agent in self.all_agents
            if label_id in (getattr(agent, 'label_ids', set()) or set())
        ]

    def select_label(self, label_id: int):
        agents = self.get_agents_by_label(label_id)
        self.set_selected_agents(agents)
        return agents

    def reset_label_brains(self, label_id: int) -> int:
        """Reinicializa pesos e biases dos agentes vivos de uma label."""
        try:
            from .brain import create_brain, clear_multi_brain_cache
        except Exception:
            return 0

        def reset_locked() -> int:
            if label_id not in self.agent_labels:
                return 0
            reset_count = 0
            for agent in self.get_agents_by_label(label_id):
                brain = getattr(agent, 'brain', None)
                sizes = list(getattr(brain, 'sizes', []) or [])
                if len(sizes) < 2:
                    continue
                agent.brain = create_brain(
                    sizes,
                    params=self.params,
                    init_std=1.0,
                    brain_type=getattr(brain, 'brain_type', self.params.get('neural_network_type', 'mlp')),
                )
                agent.last_brain_output = []
                agent.last_brain_activations = []
                reset_count += 1
            if reset_count:
                clear_multi_brain_cache()
            return reset_count

        state_lock = getattr(self, 'state_lock', None)
        if state_lock is None:
            return reset_locked()
        with state_lock:
            return reset_locked()

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

    def _has_active_label_limits(self) -> bool:
        for meta in self.agent_labels.values():
            if int(meta.get('min_limit', 0) or 0) > 0 or int(meta.get('max_limit', 0) or 0) > 0:
                return True
        return False

    def _has_active_label_min_limits(self) -> bool:
        for meta in self.agent_labels.values():
            if int(meta.get('min_limit', 0) or 0) > 0:
                return True
        return False

    def _can_use_legacy_interactions(self) -> bool:
        """Fast path para o modelo antigo quando a dieta generica equivale a ele."""
        for agent in self.entities.get('bacteria', []):
            if (
                not bool(getattr(agent, 'diet_food', True))
                or bool(getattr(agent, 'diet_agents', False))
                or abs(float(getattr(agent, 'diet_food_efficiency', 1.0)) - 1.0) > 1e-9
            ):
                return False
        for agent in self.entities.get('predators', []):
            if (
                bool(getattr(agent, 'diet_food', False))
                or not bool(getattr(agent, 'diet_agents', True))
                or bool(getattr(agent, 'diet_same_label', False))
                or abs(float(getattr(agent, 'diet_agent_efficiency', 0.7)) - 0.7) > 1e-9
            ):
                return False
        if self.entities.get('predators') and self._has_active_label_min_limits():
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
        label_id = self.ensure_default_agent_label()
        bacterium.label_ids = {label_id}
        bacterium.color = tuple(self.agent_labels[label_id].get('color', bacterium.color))
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

    def clear_food(self) -> int:
        """Remove toda a comida atual sem alterar alvo/reposicao."""
        count = len(self.entities.get('foods', []))
        if count <= 0:
            return 0
        self.entities['foods'].clear()
        self.food_controller.food_debt = 0.0
        self.food_controller.food_energy_debt = 0.0
        self.food_controller.food_excess_debt = 0.0
        self._spatial_hash_dirty = True
        return count

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
            if hasattr(obj, 'angular_velocity'):
                obj.angular_velocity = 0.0
        for prev_attr, value in (
            ('prev_x', getattr(obj, 'x', world_x)),
            ('prev_y', getattr(obj, 'y', world_y)),
            ('prev_angle', getattr(obj, 'angle', 0.0)),
        ):
            if hasattr(obj, prev_attr):
                setattr(obj, prev_attr, value)
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
        self.interaction_system.last_foods_changed = False
        self.interaction_system.last_food_energy_consumed = 0.0
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

    def _resolve_solid_food_collisions(self, params, frozen_agents: set | None = None) -> int:
        """Trata comida em pedaços como obstáculo circular sólido, só nesse modo."""
        if str(params.get('food_mode', 'instant')) != 'chunk':
            return 0
        foods = self.entities.get('foods', [])
        if not foods or not self.all_agents:
            return 0
        frozen_agents = frozen_agents or set()
        max_food_radius = max((float(getattr(food, 'r', 0.0)) for food in foods), default=0.0)
        if max_food_radius <= 0.0:
            return 0
        nearby_buffer = set()
        resolved = 0
        movable_food = bool(params.get('movable_chunk_food_enabled', False))
        push_strength = max(0.0, min(1.0, float(params.get('chunk_food_push_strength', 0.45))))
        food_mass_scale = max(1e-6, float(params.get('chunk_food_mass_scale', 1.0)))
        for agent in self.all_agents:
            if agent in frozen_agents:
                continue
            if self.spatial_hash:
                nearby_foods = self.spatial_hash.query_ball_filtered_into(
                    agent.x, agent.y, float(getattr(agent, 'r', 0.0)) + max_food_radius, 0, nearby_buffer
                )
            else:
                nearby_foods = foods
            for food in nearby_foods:
                if str(getattr(food, 'kind', params.get('food_mode', 'instant'))) != 'chunk':
                    continue
                dx = float(agent.x) - float(food.x)
                dy = float(agent.y) - float(food.y)
                dist = math.hypot(dx, dy)
                agent_r = float(getattr(agent, 'r', 0.0))
                r_sum = agent_r + float(getattr(food, 'r', 0.0))
                if dist >= r_sum:
                    continue
                if dist <= 1e-9:
                    dx = math.cos(float(getattr(agent, 'angle', 0.0)))
                    dy = math.sin(float(getattr(agent, 'angle', 0.0)))
                    dist = 1.0
                nx, ny = dx / dist, dy / dist
                overlap = r_sum - dist
                if movable_food:
                    agent_m = max(1e-6, float(getattr(agent, 'm', agent_r * agent_r)))
                    food_m = max(1e-6, float(getattr(food, 'm', float(getattr(food, 'r', 1.0)) ** 2)) * food_mass_scale)
                    total_m = agent_m + food_m
                    sep = overlap + 1e-3
                    agent.x += nx * sep * (food_m / total_m)
                    agent.y += ny * sep * (food_m / total_m)
                    food.x -= nx * sep * (agent_m / total_m)
                    food.y -= ny * sep * (agent_m / total_m)
                    rvx = float(getattr(agent, 'vx', 0.0)) - float(getattr(food, 'vx', 0.0))
                    rvy = float(getattr(agent, 'vy', 0.0)) - float(getattr(food, 'vy', 0.0))
                    closing = rvx * nx + rvy * ny
                    if closing < 0.0 and push_strength > 0.0:
                        inv_agent = 1.0 / agent_m
                        inv_food = 1.0 / food_m
                        impulse = (-closing * push_strength) / max(1e-9, inv_agent + inv_food)
                        ix = impulse * nx
                        iy = impulse * ny
                        agent.vx += ix * inv_agent
                        agent.vy += iy * inv_agent
                        food.vx -= ix * inv_food
                        food.vy -= iy * inv_food
                    food.x, food.y = self.world.clamp_position(food.x, food.y, getattr(food, 'r', 0.0))
                    if getattr(self.obstacles, 'has_obstacles', False):
                        self.obstacles.resolve_agent(food)
                else:
                    agent.x += nx * (overlap + 1e-3)
                    agent.y += ny * (overlap + 1e-3)
                if hasattr(agent, 'vx'):
                    inward = agent.vx * nx + agent.vy * ny
                    if inward < 0.0:
                        agent.vx -= inward * nx
                        agent.vy -= inward * ny
                agent.x, agent.y = self.world.clamp_position(agent.x, agent.y, getattr(agent, 'r', 0.0))
                resolved += 1
                break
        return resolved

    def _resolve_chunk_food_contacts(self, params, max_iterations: int = 1) -> int:
        """Impede sobreposicao e aplica adesao apenas dentro do mesmo pedaco."""
        if str(params.get('food_mode', 'instant')) != 'chunk':
            return 0
        if not bool(params.get('movable_chunk_food_enabled', False)):
            return 0
        foods = [
            food for food in self.entities.get('foods', [])
            if str(getattr(food, 'kind', 'chunk')) == 'chunk'
        ]
        if len(foods) < 2:
            return 0
        collision_enabled = bool(params.get('chunk_food_collision_enabled', True))
        adhesion_enabled = bool(params.get('chunk_food_adhesion_enabled', True))
        if not (collision_enabled or adhesion_enabled):
            return 0

        edge_gap = max(0.0, float(params.get('food_piece_particle_spacing', 0.0)))
        adhesion_strength = max(0.0, min(1.0, float(params.get('chunk_food_adhesion_strength', 0.35))))
        mass_scale = max(1e-6, float(params.get('chunk_food_mass_scale', 1.0)))
        max_radius = max((float(getattr(food, 'r', 0.0)) for food in foods), default=0.0)
        if max_radius <= 0.0:
            return 0

        resolved = 0
        nearby_buffer = set()

        def _apply_pair(food, other, correction: float, nx: float, ny: float,
                        r1: float, r2: float, *, damp_closing: bool) -> None:
            m1 = max(1e-6, float(getattr(food, 'm', r1 * r1)) * mass_scale)
            m2 = max(1e-6, float(getattr(other, 'm', r2 * r2)) * mass_scale)
            total_m = m1 + m2
            move1 = correction * (m2 / total_m)
            move2 = correction * (m1 / total_m)
            food.prev_x = float(getattr(food, 'x', 0.0))
            food.prev_y = float(getattr(food, 'y', 0.0))
            other.prev_x = float(getattr(other, 'x', 0.0))
            other.prev_y = float(getattr(other, 'y', 0.0))
            food.x += nx * move1
            food.y += ny * move1
            other.x -= nx * move2
            other.y -= ny * move2

            if damp_closing:
                rvx = float(getattr(food, 'vx', 0.0)) - float(getattr(other, 'vx', 0.0))
                rvy = float(getattr(food, 'vy', 0.0)) - float(getattr(other, 'vy', 0.0))
                closing = rvx * nx + rvy * ny
                if closing < 0.0:
                    inv1 = 1.0 / m1
                    inv2 = 1.0 / m2
                    impulse = (-closing) / max(1e-9, inv1 + inv2)
                    ix = impulse * nx
                    iy = impulse * ny
                    food.vx += ix * inv1
                    food.vy += iy * inv1
                    other.vx -= ix * inv2
                    other.vy -= iy * inv2

            food.x, food.y = self.world.clamp_position(food.x, food.y, r1)
            other.x, other.y = self.world.clamp_position(other.x, other.y, r2)
            if getattr(self.obstacles, 'has_obstacles', False):
                self.obstacles.resolve_agent(food)
                self.obstacles.resolve_agent(other)

        if collision_enabled:
            index = {food: i for i, food in enumerate(foods)}
            collision_query_radius = max_radius * 2.0
            for _iteration in range(max(1, int(max_iterations))):
                pass_resolved = 0
                for i, food in enumerate(foods):
                    if self.spatial_hash:
                        nearby_foods = self.spatial_hash.query_ball_filtered_into(
                            float(food.x), float(food.y), float(food.r) + collision_query_radius, 0, nearby_buffer
                        )
                    else:
                        nearby_foods = foods
                    for other in nearby_foods:
                        j = index.get(other, -1)
                        if j <= i or str(getattr(other, 'kind', 'chunk')) != 'chunk':
                            continue
                        r1 = float(getattr(food, 'r', 0.0))
                        r2 = float(getattr(other, 'r', 0.0))
                        if r1 <= 0.0 or r2 <= 0.0:
                            continue
                        dx = float(food.x) - float(other.x)
                        dy = float(food.y) - float(other.y)
                        dist2 = dx * dx + dy * dy
                        min_dist = r1 + r2
                        if dist2 >= min_dist * min_dist:
                            continue
                        if dist2 <= 1e-12:
                            dist = 1.0
                            nx, ny = 1.0, 0.0
                        else:
                            dist = math.sqrt(dist2)
                            nx, ny = dx / dist, dy / dist
                        _apply_pair(food, other, min_dist - dist + 1e-4, nx, ny, r1, r2, damp_closing=True)
                        pass_resolved += 1
                resolved += pass_resolved
                if pass_resolved == 0:
                    break

        if adhesion_enabled and adhesion_strength > 0.0:
            chunks: dict[int, list] = {}
            for food in foods:
                chunk_id = int(getattr(food, 'chunk_id', 0) or 0)
                if chunk_id > 0:
                    chunks.setdefault(chunk_id, []).append(food)
            for chunk_foods in chunks.values():
                if len(chunk_foods) < 2:
                    continue
                for i, food in enumerate(chunk_foods):
                    r1 = float(getattr(food, 'r', 0.0))
                    if r1 <= 0.0:
                        continue
                    for other in chunk_foods[i + 1:]:
                        r2 = float(getattr(other, 'r', 0.0))
                        if r2 <= 0.0:
                            continue
                        dx = float(food.x) - float(other.x)
                        dy = float(food.y) - float(other.y)
                        dist2 = dx * dx + dy * dy
                        min_dist = r1 + r2
                        rest_dist = min_dist + edge_gap
                        capture = max(r1, r2) * 0.75 + edge_gap
                        max_dist = rest_dist + capture
                        if dist2 > max_dist * max_dist:
                            continue
                        if dist2 <= 1e-12:
                            dist = 1.0
                            nx, ny = 1.0, 0.0
                        else:
                            dist = math.sqrt(dist2)
                            nx, ny = dx / dist, dy / dist
                        if dist < rest_dist:
                            correction = (rest_dist - dist) * adhesion_strength
                        else:
                            correction = -(dist - rest_dist) * adhesion_strength * 0.18
                        if abs(correction) <= 1e-7:
                            continue
                        _apply_pair(food, other, correction, nx, ny, r1, r2, damp_closing=False)
                        m1 = max(1e-6, float(getattr(food, 'm', r1 * r1)) * mass_scale)
                        m2 = max(1e-6, float(getattr(other, 'm', r2 * r2)) * mass_scale)
                        total_m = m1 + m2
                        blend = min(0.2, adhesion_strength * 0.08)
                        avg_vx = (float(food.vx) * m1 + float(other.vx) * m2) / total_m
                        avg_vy = (float(food.vy) * m1 + float(other.vy) * m2) / total_m
                        food.vx += (avg_vx - float(food.vx)) * blend
                        food.vy += (avg_vy - float(food.vy)) * blend
                        other.vx += (avg_vx - float(other.vx)) * blend
                        other.vy += (avg_vy - float(other.vy)) * blend
                        resolved += 1
        return resolved

    def _apply_passive_physics(self, dt: float, params) -> int:
        """Aplica viscosidade global, movimento browniano e inercia de comida solida."""
        if dt <= 0.0:
            return 0
        viscosity_enabled = bool(params.get('global_viscosity_enabled', False))
        brownian_enabled = bool(params.get('brownian_motion_enabled', False))
        movable_food = (
            str(params.get('food_mode', 'instant')) == 'chunk'
            and bool(params.get('movable_chunk_food_enabled', False))
        )
        if not (viscosity_enabled or brownian_enabled or movable_food):
            return 0

        changed = 0
        if viscosity_enabled:
            drag = max(0.0, float(params.get('global_viscosity_drag', 0.2)))
            decay = math.exp(-drag * dt) if drag > 0.0 else 1.0
            for agent in self.all_agents:
                agent.vx *= decay
                agent.vy *= decay
                if hasattr(agent, 'angular_velocity'):
                    agent.angular_velocity *= decay

        if brownian_enabled:
            strength = max(0.0, float(params.get('brownian_motion_strength', 3.0)))
            if strength > 0.0:
                kick = strength * math.sqrt(max(0.0, dt))
                for agent in self.all_agents:
                    mass = max(1.0, float(getattr(agent, 'm', 1.0)))
                    scale = kick / math.sqrt(mass)
                    jx = random.uniform(-scale, scale)
                    jy = random.uniform(-scale, scale)
                    agent.vx += jx
                    agent.vy += jy
                    agent.x += jx * dt
                    agent.y += jy * dt
                    agent.x, agent.y = self.world.clamp_position(agent.x, agent.y, getattr(agent, 'r', 0.0))
                    changed += 1

        if movable_food:
            food_drag = max(0.0, float(params.get('chunk_food_drag', 1.6)))
            food_decay = math.exp(-food_drag * dt) if food_drag > 0.0 else 1.0
            food_kick_base = max(0.0, float(params.get('brownian_motion_strength', 3.0))) if brownian_enabled else 0.0
            for food in self.entities.get('foods', []):
                if str(getattr(food, 'kind', 'chunk')) != 'chunk':
                    continue
                food.prev_x = float(getattr(food, 'x', 0.0))
                food.prev_y = float(getattr(food, 'y', 0.0))
                if food_kick_base > 0.0:
                    mass = max(1.0, float(getattr(food, 'm', getattr(food, 'r', 1.0) ** 2)))
                    kick = food_kick_base * math.sqrt(max(0.0, dt)) / math.sqrt(mass)
                    food.vx += random.uniform(-kick, kick)
                    food.vy += random.uniform(-kick, kick)
                food.vx *= food_decay
                food.vy *= food_decay
                if abs(food.vx) < 1e-5:
                    food.vx = 0.0
                if abs(food.vy) < 1e-5:
                    food.vy = 0.0
                if food.vx or food.vy:
                    food.x += food.vx * dt
                    food.y += food.vy * dt
                    food.x, food.y = self.world.clamp_position(food.x, food.y, getattr(food, 'r', 0.0))
                    if getattr(self.obstacles, 'has_obstacles', False):
                        self.obstacles.resolve_agent(food)
                    changed += 1
        return changed
    
    def _simulate_substep(self, dt: float):
        """Executa um substep de física."""
        step_params = self._params_snapshot()
        dragged_agent = self.dragged_object if self.dragged_object in self.all_agents else None
        frozen_agents = {dragged_agent} if dragged_agent is not None else set()
        if dragged_agent is not None:
            dragged_agent.vx = 0.0
            dragged_agent.vy = 0.0
            dragged_agent.angular_velocity = 0.0
        with profile_section('food_control'):
            target_food = step_params.get('food_target', 300)
            new_foods = self.food_controller.update(
                self.entities['foods'], target_food, self.world.width, self.world.height, step_params, dt,
                obstacle_map=self.obstacles,
                agents=self.all_agents,
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
                sensor = getattr(agent, 'sensor', None)
                sensor_key = None
                if sensor is not None:
                    sensor_key = (
                        int(getattr(sensor, 'retina_count', 0) or 0),
                        round(float(getattr(sensor, 'vision_radius', 0.0) or 0.0), 6),
                        round(float(getattr(sensor, 'fov_degrees', 0.0) or 0.0), 6),
                        bool(getattr(sensor, 'see_food', False)),
                        bool(getattr(sensor, 'see_bacteria', False)),
                        bool(getattr(sensor, 'see_predators', False)),
                        bool(getattr(sensor, 'see_obstacles', False)),
                        bool(getattr(sensor, 'see_all', False)),
                        bool(getattr(sensor, 'see_through_walls', True)),
                        tuple(getattr(sensor, 'channels', ('d',)) or ('d',)),
                        int(getattr(sensor, 'eye_count', 1) or 1),
                        round(float(getattr(sensor, 'eye_angle_degrees', 60.0) or 0.0), 6),
                        round(float(getattr(sensor, 'eye_separation_degrees', 45.0) or 0.0), 6),
                    )
                locomotion = getattr(agent, 'locomotion', None)
                locomotion_key = None
                if locomotion is not None:
                    locomotion_key = (
                        round(float(getattr(locomotion, 'max_speed', 0.0) or 0.0), 6),
                        round(float(getattr(locomotion, 'max_turn', 0.0) or 0.0), 6),
                        bool(getattr(locomotion, 'allow_reverse', False)),
                        str(getattr(locomotion, 'movement_mode', 'forward')),
                        str(getattr(locomotion, 'body_shape', getattr(agent, 'body_shape', 'ellipse'))),
                    )
                energy_model = getattr(agent, 'energy_model', None)
                energy_key = None
                if energy_model is not None:
                    energy_key = (
                        round(float(getattr(energy_model, 'v0_cost', 0.0) or 0.0), 6),
                        round(float(getattr(energy_model, 'vmax_cost', 0.0) or 0.0), 6),
                        round(float(getattr(energy_model, 'vmax_ref', 0.0) or 0.0), 6),
                        round(float(getattr(energy_model, 'energy_cap', 0.0) or 0.0), 6),
                    )
                brain = getattr(agent, 'brain', None)
                brain_key = brain.batch_key() if hasattr(brain, 'batch_key') else (
                    getattr(brain, 'brain_type', 'mlp'),
                    tuple(getattr(brain, 'sizes', ()) or ()),
                )
                key = (
                    type(agent),
                    brain_key,
                    sensor_key,
                    locomotion_key,
                    energy_key,
                )
                if key not in agent_groups:
                    agent_groups[key] = []
                agent_groups[key].append(agent)
            for group in agent_groups.values():
                update_agents_batch(group, dt, self.world, self.scene_query, step_params, selected_agent=self.selected_agent)

        with profile_section('passive_physics'):
            passive_food_moved = bool(self._apply_passive_physics(dt, step_params))
            if passive_food_moved:
                self._spatial_hash_dirty = True

        with profile_section('obstacle_collision'):
            if self._resolve_obstacle_collisions(frozen_agents=frozen_agents):
                self._spatial_hash_dirty = True

        # Agentes se moveram; interações precisam do hash com as posições atuais.
        with profile_section('spatial_hash'):
            self._update_spatial_hash(force=True)

        with profile_section('food_collision'):
            food_pushed = bool(self._resolve_solid_food_collisions(step_params, frozen_agents=frozen_agents))
            if food_pushed:
                self._spatial_hash_dirty = True
                self._update_spatial_hash(force=True)

        if passive_food_moved or food_pushed:
            with profile_section('chunk_food_contacts'):
                if self._resolve_chunk_food_contacts(step_params):
                    self._spatial_hash_dirty = True
                    self._update_spatial_hash(force=True)

        topology_changed = False
        with profile_section('interaction'):
            if self._can_use_legacy_interactions():
                removed_agents = self.interaction_system.apply(
                    self.entities['bacteria'],
                    self.entities['predators'],
                    self.entities['foods'],
                    self.spatial_hash,
                    step_params,
                    frozen_agents=frozen_agents,
                    dt=dt,
                )
            else:
                removed_agents = self.interaction_system.apply_generic(
                    self.all_agents,
                    self.entities['foods'],
                    self.spatial_hash,
                    step_params,
                    frozen_agents=frozen_agents,
                    agent_labels=self.agent_labels,
                    dt=dt,
                )
            if getattr(self.interaction_system, 'last_foods_changed', False):
                self._spatial_hash_dirty = True
            consumed_food_energy = float(getattr(self.interaction_system, 'last_food_energy_consumed', 0.0) or 0.0)
            if consumed_food_energy > 0.0 and str(step_params.get('food_mode', 'instant')) == 'chunk':
                self.food_controller.note_food_energy_consumed(consumed_food_energy)
            if removed_agents:
                topology_changed = True
                self.entities['bacteria'] = [a for a in self.entities['bacteria'] if a not in removed_agents]
                self.entities['predators'] = [a for a in self.entities['predators'] if a not in removed_agents]
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
            new_agents = self.reproduction_system.apply(reproductive_agents, step_params, agent_labels=self.agent_labels)

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
                self.entities['bacteria'], self.entities['predators'], step_params, agent_labels=self.agent_labels
            )
            self.entities['bacteria'] = surviving_bacteria
            self.entities['predators'] = surviving_predators
            # Reconstroi lista unificada (custo O(n) mas uma vez por frame; elimina concatenações)
            self.all_agents = surviving_bacteria + surviving_predators
            after_death_count = len(self.all_agents)
            if after_death_count != before_death_count:
                topology_changed = True
                corpse_foods = self._create_food_from_dead_agents(getattr(self.death_system, 'last_deaths', []) or [], step_params)
                if corpse_foods:
                    self.entities['foods'].extend(corpse_foods)
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

    def _create_food_from_dead_agents(self, dead_agents, params) -> list:
        foods = []
        if not dead_agents:
            return foods
        for agent in dead_agents:
            energy_model = getattr(agent, 'energy_model', None)
            if not bool(getattr(energy_model, 'corpse_to_food', False)):
                continue
            min_r = max(0.1, float(params.get('food_min_r', 4.5)))
            max_r = max(min_r, float(params.get('food_max_r', 5.0)))
            radius = max(min_r, min(max_r, float(getattr(agent, 'r', max_r)) * 0.5))
            if not self.can_place_circle(float(getattr(agent, 'x', 0.0)), float(getattr(agent, 'y', 0.0)), radius):
                continue
            food_mode = 'chunk' if str(params.get('food_mode', 'instant')) == 'chunk' else 'instant'
            food = Food(float(getattr(agent, 'x', 0.0)), float(getattr(agent, 'y', 0.0)), radius,
                        kind=food_mode)
            try:
                food.color = tuple(params.get('food_color', food.color))
            except Exception:
                pass
            try:
                food.chunk_id = int(getattr(self.food_controller, '_new_chunk_id')())
            except Exception:
                food.chunk_id = 0
            base_energy = getattr(food, 'energy', radius * radius)
            corpse_energy = max(0.0, float(getattr(agent, 'energy', 0.0))) * 0.5
            food.energy = max(base_energy, corpse_energy)
            food.initial_energy = max(1e-9, float(food.energy))
            food.base_radius = float(radius)
            foods.append(food)
        return foods
    
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
            self.scene_query = SceneQuery(None, self.entities, self.params, obstacles=self.obstacles)
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
            abs(getattr(self.spatial_hash, 'min_y', 0.0) - min_y) < 1e-6 and
            bool(getattr(self.spatial_hash, 'numeric_index_enabled', False)) == bool(self.params.get('use_persistent_perception_arrays', False))):
            # Reutiliza: apenas limpa e reinsere
            self.spatial_hash.clear()
        else:
            # Recria
            self.spatial_hash = SpatialHash(
                cell_size,
                bounds_w,
                bounds_h,
                min_x=min_x,
                min_y=min_y,
                numeric_index_enabled=bool(self.params.get('use_persistent_perception_arrays', False)),
            )
        
        # Insere todas as entidades
        for food in self.entities['foods']:
            self.spatial_hash.insert(food, food.x, food.y, food.r)
        
        for bacterium in self.entities['bacteria']:
            self.spatial_hash.insert(bacterium, bacterium.x, bacterium.y, bacterium.r)
        
        for predator in self.entities['predators']:
            self.spatial_hash.insert(predator, predator.x, predator.y, predator.r)
        
        # Atualiza scene query
        self.scene_query = SceneQuery(self.spatial_hash, self.entities, self.params, obstacles=self.obstacles)
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

        elif command == 'clear_food':
            removed = self.clear_food()
            print(f"Comida limpa: {removed} itens removidos")
        
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
                self._spawn_agent_from_prototype(
                    self.loaded_agent_prototypes[name],
                    world_x,
                    world_y,
                    preserve_prototype_color=True,
                )
            else:
                print("Protótipo não encontrado para spawn.")
        
        elif command == 'pipette_agent':
            world_x = kwargs.get('world_x', 0)
            world_y = kwargs.get('world_y', 0)
            self.sample_agent_as_prototype(world_x, world_y)

        else:
            print(f"Comando desconhecido: {command}")

    def _spawn_agent_from_prototype(self, data: dict, world_x: float, world_y: float,
                                    label_id: Optional[int] = None, select: bool = True,
                                    preserve_prototype_color: bool = False):
        """Cria e insere um agente a partir de um dicionário de dados carregados."""
        try:
            agent_type = data.get('type','bacteria')
            from .brain import brain_from_data
            from .sensors import RetinaSensor
            from .actuators import Locomotion, EnergyModel
            from .entities import Bacteria, Predator
            import json as _json
            import math as _math
            # Cérebro
            sizes = _json.loads(data.get('brain_sizes','[]'))
            brain_data = dict(data)
            brain_data['brain_sizes'] = sizes if sizes else [1, 2]
            weights=[]; biases=[]; idx=0
            while True:
                w_key=f'brain_weight_{idx}'; b_key=f'brain_bias_{idx}'
                if w_key not in data or b_key not in data: break
                weights.append(_json.loads(data[w_key])); biases.append(_json.loads(data[b_key])); idx+=1
            if weights and biases:
                brain_data['brain_weights'] = weights
                brain_data['brain_biases'] = biases
            brain = brain_from_data(brain_data, params=self.params, init_std=0.01)
            # Sensor
            def _f(k, default=0.0):
                try: return float(data.get(k, default))
                except Exception: return default
            def _i(k, default=0):
                try: return int(float(data.get(k, default)))
                except Exception: return default
            def _b(k, default=False):
                v = data.get(k, str(default)); return v in ('1','True','true','YES','yes')
            def _channels(default=("d",)):
                raw = data.get('sensor_channels', None)
                if raw is None:
                    return default
                if isinstance(raw, str):
                    try:
                        raw = _json.loads(raw)
                    except Exception:
                        raw = [raw]
                return raw
            sensor = RetinaSensor(
                retina_count=_i('sensor_retina_count',18),
                vision_radius=_f('sensor_vision_radius',120.0),
                fov_degrees=_f('sensor_fov_degrees',180.0),
                skip=_i('sensor_skip',0),
                see_food=_b('sensor_see_food',True),
                see_bacteria=_b('sensor_see_bacteria',False),
                see_predators=_b('sensor_see_predators',False),
                see_obstacles=_b('sensor_see_obstacles',False),
                see_all=_b('sensor_see_all',False),
                see_through_walls=_b('sensor_see_through_walls',True),
                channels=_channels(),
                eye_count=_i('sensor_eye_count', 1),
                eye_angle_degrees=_f('sensor_eye_angle_degrees', 60.0),
                eye_separation_degrees=_f('sensor_eye_separation_degrees', 45.0),
            )
            locomotion = Locomotion(
                max_speed=_f('locomotion_max_speed',300.0),
                max_turn=_f('locomotion_max_turn', _math.pi),
                allow_reverse=_b('locomotion_allow_reverse', _b('locomotion_allow_reverse_locomotion', False)),
                movement_mode=data.get('locomotion_movement_mode', 'forward'),
                body_shape=data.get('locomotion_body_shape', data.get('body_shape', 'ellipse')),
            )
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
                energy_cap=_pick_num('energy_energy_cap','energy_cap', default=(600.0 if agent_type=='predator' else 400.0)),
                age_death_enabled=_b('energy_age_death_enabled', _b('age_death_enabled', False)),
                death_age=_pick_num('energy_death_age','death_age', default=3600.0),
                corpse_to_food=_b('energy_corpse_to_food', _b('corpse_to_food', False)),
                reproduction_min_age=_pick_num('energy_reproduction_min_age', 'reproduction_min_age', default=0.0),
                reproduction_cooldown=_pick_num('energy_reproduction_cooldown', 'reproduction_cooldown', default=0.0),
            )
            r = _f('r', 9.0)
            angle = _f('angle', 0.0)
            if not self.can_place_circle(world_x, world_y, r):
                return None
            if agent_type == 'predator':
                agent = Predator(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            else:
                agent = Bacteria(world_x, world_y, r, brain, sensor, locomotion, energy_model, angle)
            # Ajustes adicionais
            agent.energy = _f('energy', 0.0)
            agent.age = _f('age', 0.0)
            agent.angular_velocity = _f('angular_velocity', 0.0)
            agent.food_eaten_count = _i('food_eaten_count', 0)
            agent.food_energy_eaten_total = _f('food_energy_eaten_total', 0.0)
            agent.prey_eaten_count = _i('prey_eaten_count', 0)
            agent.prey_energy_eaten_total = _f('prey_energy_eaten_total', 0.0)
            agent.diet_food = _b('diet_food', getattr(agent, 'diet_food', not getattr(agent, 'is_predator', False)))
            agent.diet_agents = _b('diet_agents', getattr(agent, 'diet_agents', getattr(agent, 'is_predator', False)))
            agent.diet_same_label = _b('diet_same_label', getattr(agent, 'diet_same_label', False))
            agent.diet_food_efficiency = _f('diet_food_efficiency', getattr(agent, 'diet_food_efficiency', 1.0))
            agent.diet_agent_efficiency = _f('diet_agent_efficiency', getattr(agent, 'diet_agent_efficiency', 0.7))
            agent.body_shape = getattr(locomotion, 'body_shape', data.get('body_shape', 'ellipse'))
            agent.agent_name = str(data.get('agent_name') or self.params.get('agent_template_name', 'organismo_1') or 'organismo_1')
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
            try:
                label_id = int(label_id) if label_id is not None else self.ensure_default_agent_label()
            except Exception:
                label_id = self.ensure_default_agent_label()
            if label_id not in self.agent_labels:
                label_id = self.ensure_default_agent_label()
            agent.label_ids = {label_id}
            if not preserve_prototype_color:
                agent.color = tuple(self.agent_labels[label_id].get('color', getattr(agent, 'color', (220, 220, 220))))
            if agent.is_predator:
                self.entities['predators'].append(agent)
            else:
                self.entities['bacteria'].append(agent)
            self.all_agents.append(agent)
            if select:
                self.set_selected_agents([agent], primary=agent)
            self._spatial_hash_dirty = True
            try:
                from .brain import clear_multi_brain_cache
                clear_multi_brain_cache()
            except Exception:
                pass
            return agent
        except Exception as e:
            print(f"Falha ao spawnar protótipo: {e}")
            return None
    
    def _agent_to_prototype_data(self, agent, name: str | None = None) -> dict:
        """Serializa um agente vivo para o formato usado por spawn_loaded_agent."""
        data = {}

        def add(key, value):
            data[key] = str(value)

        add('agent_name', name or getattr(agent, 'agent_name', None) or self.params.get('agent_template_name', 'organismo_1'))
        add('type', 'predator' if getattr(agent, 'is_predator', False) else 'organism')
        for attr in ['x', 'y', 'r', 'angle', 'vx', 'vy', 'angular_velocity', 'energy', 'age']:
            add(attr, getattr(agent, attr, 0.0))
        for attr in ['food_eaten_count', 'food_energy_eaten_total', 'prey_eaten_count', 'prey_energy_eaten_total']:
            add(attr, getattr(agent, attr, 0.0))
        data['label_ids'] = json.dumps(sorted(int(v) for v in (getattr(agent, 'label_ids', set()) or set())))
        add('last_reproduction_age', getattr(agent, 'last_reproduction_age', ''))
        try:
            data['color'] = json.dumps(list(getattr(agent, 'color', (220, 220, 220))))
        except Exception:
            pass

        brain = getattr(agent, 'brain', None)
        if brain is not None and hasattr(brain, 'sizes'):
            try:
                from .brain import brain_to_data
                brain_data = brain_to_data(brain)
                data['brain_type'] = str(brain_data.get('brain_type', 'mlp'))
                data['brain_type_label'] = str(brain_data.get('brain_type_label', 'MLP padrao'))
                for extra_key, extra_value in brain_data.items():
                    if extra_key in {'brain_type', 'brain_type_label', 'brain_sizes', 'brain_weights', 'brain_biases', 'brain_version'}:
                        continue
                    data[extra_key] = json.dumps(extra_value) if isinstance(extra_value, (list, dict)) else str(extra_value)
            except Exception:
                pass
            data['brain_sizes'] = json.dumps(list(getattr(brain, 'sizes', [])))
            add('brain_version', getattr(brain, 'version', 0))
            for idx, (weights, biases) in enumerate(zip(getattr(brain, 'weights', []), getattr(brain, 'biases', []))):
                data[f'brain_weight_{idx}'] = json.dumps(weights.tolist() if hasattr(weights, 'tolist') else list(weights))
                data[f'brain_bias_{idx}'] = json.dumps(biases.tolist() if hasattr(biases, 'tolist') else list(biases))

        sensor = getattr(agent, 'sensor', None)
        if sensor is not None:
            for attr in ['retina_count', 'vision_radius', 'fov_degrees', 'skip', 'see_food', 'see_bacteria', 'see_predators', 'see_obstacles', 'see_all', 'see_through_walls', 'eye_count', 'eye_angle_degrees', 'eye_separation_degrees']:
                if hasattr(sensor, attr):
                    add(f'sensor_{attr}', getattr(sensor, attr))
            if hasattr(sensor, 'channels'):
                channels = tuple(getattr(sensor, 'channels', ('d',)))
                data['sensor_channels'] = json.dumps(list(channels))
                try:
                    from .sensors import retina_input_mode_from_channels
                    data['sensor_input_mode'] = retina_input_mode_from_channels(channels)
                    color_channels = sorted({ch[0] if len(ch) == 2 and ch.endswith('d') else ch for ch in channels if ch in ('r', 'g', 'b') or (len(ch) == 2 and ch.endswith('d'))})
                    data['sensor_color_channels'] = json.dumps(color_channels)
                except Exception:
                    pass

        for attr in ['diet_food', 'diet_agents', 'diet_same_label', 'diet_food_efficiency', 'diet_agent_efficiency']:
            if hasattr(agent, attr):
                add(attr, getattr(agent, attr))

        locomotion = getattr(agent, 'locomotion', None)
        if locomotion is not None:
            for attr in ['max_speed', 'max_turn', 'allow_reverse', 'movement_mode', 'body_shape']:
                if hasattr(locomotion, attr):
                    add(f'locomotion_{attr}', getattr(locomotion, attr))
        if hasattr(agent, 'body_shape'):
            add('body_shape', getattr(agent, 'body_shape'))

        energy_model = getattr(agent, 'energy_model', None)
        if energy_model is not None:
            for attr in getattr(energy_model, '__slots__', []):
                if attr.startswith('_'):
                    continue
                value = getattr(energy_model, attr, None)
                if isinstance(value, (int, float, bool)):
                    add(f'energy_{attr}', value)
            if hasattr(energy_model, 'v0_cost'):
                add('energy_loss_idle', getattr(energy_model, 'v0_cost'))
            if hasattr(energy_model, 'vmax_cost'):
                add('energy_loss_move', getattr(energy_model, 'vmax_cost'))

        return data

    def sample_agent_as_prototype(self, world_x: float, world_y: float):
        """Pipeta: copia um agente vivo para o prototipo ativo."""
        agent = self.get_agent_at_position(world_x, world_y)
        if agent is None:
            return None
        name = str(getattr(agent, 'agent_name', None) or self.params.get('agent_template_name', 'organismo_1') or 'organismo_1')
        data = self._agent_to_prototype_data(agent, name=name)
        self.loaded_agent_prototypes[name] = data
        self.current_agent_prototype = name
        self._prototype_revision += 1
        self.set_selected_agents([agent], primary=agent)
        return data

    def _apply_configured_random_seed(self, force: bool = False) -> Optional[int]:
        seed = normalize_seed(self.params.get('random_seed', -1))
        if seed is None:
            return None
        if force or self._applied_random_seed != seed:
            apply_global_seed(seed)
            self._applied_random_seed = seed
        return seed

    def _prototype_radius(self, data: dict) -> float:
        try:
            return max(0.1, float(data.get('r', self.params.get('bacteria_body_size', 9.0))))
        except Exception:
            return max(0.1, float(self.params.get('bacteria_body_size', 9.0)))

    def _random_spawn_point(self, radius: float) -> tuple[float, float]:
        shape = getattr(self.world, 'shape', self.params.get('substrate_shape', 'rectangular'))
        if shape == 'circular':
            cx = self.world.width / 2.0
            cy = self.world.height / 2.0
            max_r = max(0.0, float(getattr(self.world, 'radius', min(self.world.width, self.world.height) / 2.0)) - radius)
            ang = random.random() * 2.0 * math.pi
            rad = (random.random() ** 0.5) * max_r
            return cx + math.cos(ang) * rad, cy + math.sin(ang) * rad
        x = self.world.width / 2.0 if self.world.width <= 2 * radius else random.uniform(radius, self.world.width - radius)
        y = self.world.height / 2.0 if self.world.height <= 2 * radius else random.uniform(radius, self.world.height - radius)
        return x, y

    def _label_spawn_plan(self) -> list[tuple[int, int]]:
        default_count = min(max(0, int(self.params.get('bacteria_count', 150) or 0)), 10000)
        self.ensure_default_agent_label()
        plan: list[tuple[int, int]] = []
        for label_id, meta in sorted(self.agent_labels.items()):
            min_limit = max(0, int(meta.get('min_limit', 0) or 0))
            max_limit = max(0, int(meta.get('max_limit', 0) or 0))
            count = max_limit if max_limit > 0 else min_limit
            if count > 0:
                plan.append((int(label_id), min(count, 10000)))
        if not plan:
            first_label = self.ensure_default_agent_label()
            plan.append((first_label, default_count))
        return plan

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
        self.ensure_default_agent_label()
        self._sim_time_accumulator = 0.0
        self.simulation_backlog = 0.0
        self.last_physics_steps = 0
        self.last_simulated_dt = 0.0

        # Cria bactérias
        prototype = None
        if self.current_agent_prototype and self.current_agent_prototype in self.loaded_agent_prototypes:
            prototype = self.loaded_agent_prototypes[self.current_agent_prototype]

        for label_id, count in self._label_spawn_plan():
            label_color = tuple(self.agent_labels[label_id].get('color', (220, 220, 220)))
            for _ in range(count):
                organism = None
                attempts = 120 if self.obstacles.has_obstacles or prototype is not None else 1
                if prototype is not None:
                    radius = self._prototype_radius(prototype)
                    for _attempt in range(attempts):
                        x, y = self._random_spawn_point(radius)
                        if any(math.hypot(getattr(entity, 'x', 0.0) - x, getattr(entity, 'y', 0.0) - y) < getattr(entity, 'r', 0.0) + radius for entity in all_entities):
                            continue
                        organism = self._spawn_agent_from_prototype(prototype, x, y, label_id=label_id, select=False)
                        if organism is not None:
                            break
                else:
                    for _attempt in range(attempts):
                        candidate = create_random_bacteria(all_entities, self.params,
                                                           self.world.width, self.world.height)
                        if self.can_place_circle(candidate.x, candidate.y, candidate.r):
                            organism = candidate
                            break
                if organism is None:
                    continue
                organism.label_ids = {label_id}
                organism.color = label_color
                if organism not in self.all_agents:
                    self.entities['bacteria'].append(organism)
                    self.all_agents.append(organism)
                all_entities.append(organism)

        bacteria_count = 0
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
        if False and self.params.get('predators_enabled', False):
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
        if not bool(self.params.get('substrate_border_enabled', True)):
            return
        border_color = self._render_color(self.params.get('substrate_border_color', (40, 200, 40)), (40, 200, 40))
        if getattr(self.world, 'shape', 'rectangular') == 'circular':
            # Desenha círculo baseado em world.cx, world.cy, world.radius
            center_screen = self.camera.world_to_screen(self.world.cx, self.world.cy)
            radius_screen = int(self.world.radius * self.camera.zoom)
            if radius_screen > 1:
                pygame.draw.circle(surface, border_color, (int(center_screen[0]), int(center_screen[1])), radius_screen, width=1)
        else:
            top_left = self.camera.world_to_screen(0, 0)
            bottom_right = self.camera.world_to_screen(self.world.width, self.world.height)
            rect_x = int(top_left[0])
            rect_y = int(top_left[1])
            rect_w = int(bottom_right[0] - top_left[0])
            rect_h = int(bottom_right[1] - top_left[1])
            if rect_w >= 2 and rect_h >= 2:
                pygame.draw.rect(surface, border_color, pygame.Rect(rect_x, rect_y, rect_w, rect_h), width=1)

    def _draw_spatial_hash_grid(self, surface, visible_bounds):
        """Desenha a grade do SpatialHash apenas quando solicitado pela UI."""
        spatial = self.spatial_hash
        if spatial is None:
            return
        import pygame

        zoom = max(1e-6, float(getattr(self.camera, 'zoom', 1.0)))
        cell_size = max(1.0, float(getattr(spatial, 'cell_size', 1.0)))
        spacing_px = cell_size * zoom
        if spacing_px < 2.0:
            return

        min_x = float(getattr(spatial, 'min_x', 0.0))
        min_y = float(getattr(spatial, 'min_y', 0.0))
        width = float(getattr(spatial, 'width', self.world.width))
        height = float(getattr(spatial, 'height', self.world.height))
        cols = max(1, int(getattr(spatial, 'cols', math.ceil(width / cell_size))))
        rows = max(1, int(getattr(spatial, 'rows', math.ceil(height / cell_size))))
        max_x = min_x + width
        max_y = min_y + height
        view_min_x, view_min_y, view_max_x, view_max_y = visible_bounds
        if max_x < view_min_x or min_x > view_max_x or max_y < view_min_y or min_y > view_max_y:
            return

        color = (90, 140, 180)
        major_color = (120, 175, 220)
        alpha = 80
        overlay = pygame.Surface(surface.get_size(), pygame.SRCALPHA)

        first_col = max(0, int(math.floor((view_min_x - min_x) / cell_size)))
        last_col = min(cols, int(math.ceil((view_max_x - min_x) / cell_size)))
        first_row = max(0, int(math.floor((view_min_y - min_y) / cell_size)))
        last_row = min(rows, int(math.ceil((view_max_y - min_y) / cell_size)))

        top_screen = int((max(min_y, view_min_y) - self.camera.y) * zoom)
        bottom_screen = int((min(max_y, view_max_y) - self.camera.y) * zoom)
        left_screen = int((max(min_x, view_min_x) - self.camera.x) * zoom)
        right_screen = int((min(max_x, view_max_x) - self.camera.x) * zoom)
        top_screen = max(-1, min(surface.get_height() + 1, top_screen))
        bottom_screen = max(-1, min(surface.get_height() + 1, bottom_screen))
        left_screen = max(-1, min(surface.get_width() + 1, left_screen))
        right_screen = max(-1, min(surface.get_width() + 1, right_screen))

        for col in range(first_col, last_col + 1):
            x = min_x + col * cell_size
            sx = int((x - self.camera.x) * zoom)
            c = major_color if col % 5 == 0 else color
            pygame.draw.line(overlay, (*c, alpha), (sx, top_screen), (sx, bottom_screen), 1)
        for row in range(first_row, last_row + 1):
            y = min_y + row * cell_size
            sy = int((y - self.camera.y) * zoom)
            c = major_color if row % 5 == 0 else color
            pygame.draw.line(overlay, (*c, alpha), (left_screen, sy), (right_screen, sy), 1)

        surface.blit(overlay, (0, 0))

        if self._spatial_debug_font is None:
            try:
                self._spatial_debug_font = pygame.font.SysFont("Consolas", 13)
            except Exception:
                self._spatial_debug_font = False
        if self._spatial_debug_font:
            stats = spatial.get_stats() if hasattr(spatial, 'get_stats') else {}
            text = (
                f"SpatialHash celula={cell_size:.1f}u ({spacing_px:.1f}px) "
                f"grid={cols}x{rows} ocupadas={stats.get('occupied_cells', 0)}"
            )
            label = self._spatial_debug_font.render(text, True, (210, 235, 255))
            bg = pygame.Surface((label.get_width() + 10, label.get_height() + 6), pygame.SRCALPHA)
            bg.fill((6, 18, 26, 170))
            bg.blit(label, (5, 3))
            surface.blit(bg, (8, surface.get_height() - bg.get_height() - 8))
    
    def _gather_render_info(self) -> Dict[str, Any]:
        """Coleta informações para renderização."""
        return {
            'organism_count': len(self.all_agents),
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
            'physics_target_hz': self.params.get('physics_steps_per_second', 30),
            'world_w': self.world.width,
            'world_h': self.world.height,
            'world_shape': getattr(self.world, 'shape', 'rectangular'),
            'world_radius': getattr(self.world, 'radius', 0.0),
            'selected_agent': self.selected_agent,
            'obstacle_count': len(self.obstacles),
            'hide_overlay': False,
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
