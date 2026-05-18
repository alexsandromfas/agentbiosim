"""
Interface Pygame para visualização e input da simulação.
Responsável por input, câmera e bootstrap da visualização.
"""
import os
import sys
import pygame
import math
from typing import Optional

from .engine import Engine
from .world import World, Camera
from .controllers import Params
from .render import SimpleRenderer, EllipseRenderer


class PygameView:
    """
    View Pygame para a simulação.
    
    - Gerencia input do mouse/teclado
    - Controla câmera 
    - Interface com o Engine
    """
    
    def __init__(self, engine: Engine, screen_width: int = 800, screen_height: int = 600):
        self.engine = engine
        self.screen_width = screen_width
        self.screen_height = screen_height
        
        # Estado de input
        self.dragging = False
        self.drag_last_pos = (0, 0)
        self.active_tool = 'food'
        self.brush_width = 16.0
        self.brush_color = (95, 95, 105)
        self.brush_erase = False
        self.drawing_obstacle = False
        self.last_brush_world_pos = None
        self.moving_object = None
        self.selection_drag_tool = None
        self.selection_start_world = None
        self.selection_lasso_points = []
        
        # Pygame
        self.screen: Optional[pygame.Surface] = None
        self._render_surface: Optional[pygame.Surface] = None
        self._render_surface_size: tuple[int, int] = (0, 0)
        self.render_scale = self._coerce_render_scale(self.engine.params.get('render_resolution_scale', 1.0))
        self.clock = pygame.time.Clock()
        self.running = False

    @staticmethod
    def _coerce_render_scale(value) -> float:
        try:
            scale = float(value)
        except (TypeError, ValueError):
            scale = 1.0
        return max(1.0, min(3.0, scale))

    def set_render_scale(self, scale: float):
        """Atualiza a escala de supersampling usada apenas na renderizacao."""
        new_scale = self._coerce_render_scale(scale)
        if abs(new_scale - self.render_scale) > 1e-6:
            self.render_scale = new_scale
            self._render_surface = None
            self._render_surface_size = (0, 0)

    def _current_render_scale(self) -> float:
        scale = self._coerce_render_scale(self.engine.params.get('render_resolution_scale', self.render_scale))
        if abs(scale - self.render_scale) > 1e-6:
            self.set_render_scale(scale)
        return self.render_scale

    def _get_render_target(self, scale: float) -> pygame.Surface:
        if self.screen is None:
            raise RuntimeError("Pygame screen not initialized")
        width, height = self.screen.get_size()
        target_size = (max(1, int(round(width * scale))), max(1, int(round(height * scale))))
        if self._render_surface is None or self._render_surface_size != target_size:
            self._render_surface = pygame.Surface(target_size)
            self._render_surface_size = target_size
        return self._render_surface

    def _render_frame(self):
        if self.screen is None:
            return
        scale = self._current_render_scale()
        if scale <= 1.01:
            self.engine.render(self.screen)
            self._draw_tool_preview()
            pygame.display.flip()
            return

        target = self._get_render_target(scale)
        camera = self.engine.camera
        old_zoom = camera.zoom
        camera.zoom = old_zoom * scale
        try:
            self.engine.render(target)
        finally:
            camera.zoom = old_zoom
        pygame.transform.smoothscale(target, self.screen.get_size(), self.screen)
        self._draw_tool_preview()
        pygame.display.flip()
    
    def initialize(self, window_id: Optional[str] = None):
        """
        Inicializa Pygame.
        
        Args:
            window_id: ID da janela para embedding (Windows)
        """
        # Configuração para embedding no Windows
        if window_id and sys.platform.startswith("win"):
            os.environ['SDL_WINDOWID'] = str(window_id)
            os.environ['SDL_VIDEODRIVER'] = 'windib'
        
        pygame.display.init()
        pygame.font.init()

        self.screen = pygame.display.set_mode((self.screen_width, self.screen_height))
        pygame.display.set_caption("AgentBioSim V1.0.0")
        self.set_render_scale(self.engine.params.get('render_resolution_scale', 1.0))
        
        # Configura renderer baseado nos parâmetros
        if self.engine.params.get('simple_render', False):
            self.engine.renderer = SimpleRenderer()
        else:
            self.engine.renderer = EllipseRenderer()
    
    def run(self):
        """Loop principal da view."""
        self.running = True
        
        while self.running:
            # Processa eventos
            self._process_events()
            
            # Atualiza simulação
            target_fps = max(1, int(self.engine.params.get('fps', 60)))
            elapsed_dt = self.clock.tick(target_fps) / 1000.0
            # A simulacao nao deve tentar "pagar" frames atrasados aumentando
            # a carga fisica do frame seguinte. Se a maquina nao acompanha, a
            # velocidade efetiva cai; o dt fisico continua fixo no Engine.
            real_dt = min(elapsed_dt, 1.0 / target_fps)
            state_lock = getattr(self.engine, 'state_lock', None)
            if state_lock is None:
                self.engine.step(real_dt)
            else:
                with state_lock:
                    self.engine.step(real_dt)
            
            # Renderiza
            if self.screen:
                if state_lock is None:
                    self._render_frame()
                else:
                    with state_lock:
                        self._render_frame()

    def _draw_tool_preview(self):
        """Desenha feedback visual leve para selecao por area."""
        if not self.screen:
            return
        color = (90, 170, 255)
        if self.selection_drag_tool == 'select_square' and self.selection_start_world is not None:
            start = self.engine.camera.world_to_screen(*self.selection_start_world)
            current = pygame.mouse.get_pos()
            x0, y0 = int(start[0]), int(start[1])
            x1, y1 = int(current[0]), int(current[1])
            rect = pygame.Rect(min(x0, x1), min(y0, y1), abs(x1 - x0), abs(y1 - y0))
            if rect.width > 1 and rect.height > 1:
                pygame.draw.rect(self.screen, color, rect, width=1)
        elif self.selection_drag_tool == 'select_lasso' and len(self.selection_lasso_points) >= 2:
            points = [
                (int(x), int(y))
                for x, y in (self.engine.camera.world_to_screen(px, py) for px, py in self.selection_lasso_points)
            ]
            pygame.draw.lines(self.screen, color, False, points, width=2)
    
    def stop(self):
        """Para a view."""
        self.running = False
        self.engine.stop()
    
    def cleanup(self):
        """Limpa recursos do Pygame."""
        try:
            pygame.display.quit()
            pygame.quit()
        except Exception as e:
            print(f"Erro ao limpar Pygame: {e}")
    
    def _process_events(self):
        """Processa eventos do Pygame."""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.stop()
            
            elif event.type == pygame.MOUSEWHEEL:
                self._handle_mouse_wheel(event)
            
            elif event.type == pygame.MOUSEBUTTONDOWN:
                self._handle_mouse_down(event)
            
            elif event.type == pygame.MOUSEBUTTONUP:
                self._handle_mouse_up(event)
            
            elif event.type == pygame.MOUSEMOTION:
                self._handle_mouse_motion(event)
            
            elif event.type == pygame.KEYDOWN:
                self._handle_key_down(event)
        
        # Processa keys contínuas
        keys = pygame.key.get_pressed()
        self._handle_continuous_keys(keys)
    
    def _handle_mouse_wheel(self, event):
        """Trata scroll do mouse para zoom."""
        # Determina fator de zoom
        if hasattr(event, 'y'):
            zoom_factor = 1.1 ** event.y
        else:
            # Fallback para sistemas mais antigos
            zoom_factor = 1.1 if event.button == 4 else (1/1.1 if event.button == 5 else 1.0)
        
        # Aplica zoom na posição do mouse
        mouse_x, mouse_y = pygame.mouse.get_pos()
        self.engine.camera.zoom_at(mouse_x, mouse_y, zoom_factor)
    
    def _handle_mouse_down(self, event):
        """Trata clique do mouse."""
        if event.button == 1:  # Botão esquerdo
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            if self.active_tool == 'select':
                self.engine.send_command('select_agent', world_x=world_x, world_y=world_y)
            elif self.active_tool == 'select_square':
                self.selection_drag_tool = 'select_square'
                self.selection_start_world = (world_x, world_y)
                self.selection_lasso_points = []
            elif self.active_tool == 'select_lasso':
                self.selection_drag_tool = 'select_lasso'
                self.selection_start_world = (world_x, world_y)
                self.selection_lasso_points = [(world_x, world_y)]
            elif self.active_tool == 'food':
                self.engine.send_command('add_food', world_x=world_x, world_y=world_y)
            elif self.active_tool == 'agent':
                self.engine.send_command('spawn_loaded_agent', world_x=world_x, world_y=world_y)
            elif self.active_tool == 'draw':
                self.drawing_obstacle = True
                self.last_brush_world_pos = (world_x, world_y)
                self.engine.send_command(
                    'paint_obstacle',
                    x0=world_x, y0=world_y, x1=world_x, y1=world_y,
                    radius=self.brush_width * 0.5,
                    color=self.brush_color,
                    erase=self.brush_erase,
                )
            elif self.active_tool == 'move':
                state_lock = getattr(self.engine, 'state_lock', None)
                if state_lock is None:
                    self.moving_object = self.engine.get_object_at_position(world_x, world_y)
                else:
                    with state_lock:
                        self.moving_object = self.engine.get_object_at_position(world_x, world_y)
                self.engine.dragged_object = self.moving_object
                if self.moving_object is not None and hasattr(self.moving_object, 'vx'):
                    self.moving_object.vx = 0.0
                    self.moving_object.vy = 0.0
            elif self.active_tool == 'dead':
                self.engine.send_command('remove_object_at', world_x=world_x, world_y=world_y)
        
        elif event.button == 2:  # Botão do meio reservado para ferramentas futuras
            pass
        
        elif event.button == 3:  # Botão direito - inicia pan
            self.dragging = True
            self.drag_last_pos = event.pos
    
    def _handle_mouse_up(self, event):
        """Trata soltar do mouse."""
        if event.button == 3:  # Botão direito
            self.dragging = False
        elif event.button == 1:
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            if self.selection_drag_tool == 'select_square' and self.selection_start_world is not None:
                x0, y0 = self.selection_start_world
                if math.hypot(world_x - x0, world_y - y0) < (4.0 / max(self.engine.camera.zoom, 1e-6)):
                    self.engine.send_command('select_agent', world_x=world_x, world_y=world_y)
                else:
                    self.engine.send_command('select_agents_rect', x0=x0, y0=y0, x1=world_x, y1=world_y)
            elif self.selection_drag_tool == 'select_lasso':
                points = list(self.selection_lasso_points)
                if len(points) < 3:
                    self.engine.send_command('select_agent', world_x=world_x, world_y=world_y)
                else:
                    self.engine.send_command('select_agents_lasso', points=points)
            self.selection_drag_tool = None
            self.selection_start_world = None
            self.selection_lasso_points = []
            self.drawing_obstacle = False
            self.last_brush_world_pos = None
            self.engine.dragged_object = None
            self.moving_object = None
    
    def _handle_mouse_motion(self, event):
        """Trata movimento do mouse."""
        if self.dragging:
            # Pan da câmera
            dx = event.pos[0] - self.drag_last_pos[0]
            dy = event.pos[1] - self.drag_last_pos[1]
            
            # Converte delta da tela para delta do mundo
            world_dx = -dx / self.engine.camera.zoom
            world_dy = -dy / self.engine.camera.zoom
            
            self.engine.camera.move(world_dx, world_dy)
            self.drag_last_pos = event.pos
        elif self.drawing_obstacle and self.active_tool == 'draw':
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            last_x, last_y = self.last_brush_world_pos or (world_x, world_y)
            self.engine.send_command(
                'paint_obstacle',
                x0=last_x, y0=last_y, x1=world_x, y1=world_y,
                radius=self.brush_width * 0.5,
                color=self.brush_color,
                erase=self.brush_erase,
            )
            self.last_brush_world_pos = (world_x, world_y)
        elif self.selection_drag_tool == 'select_lasso':
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            last_x, last_y = self.selection_lasso_points[-1] if self.selection_lasso_points else (world_x, world_y)
            min_step = 4.0 / max(self.engine.camera.zoom, 1e-6)
            if math.hypot(world_x - last_x, world_y - last_y) >= min_step:
                self.selection_lasso_points.append((world_x, world_y))
        elif self.moving_object is not None and self.active_tool == 'move':
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            state_lock = getattr(self.engine, 'state_lock', None)
            if state_lock is None:
                self.engine.move_object_to(self.moving_object, world_x, world_y)
            else:
                with state_lock:
                    self.engine.move_object_to(self.moving_object, world_x, world_y)
    
    def _handle_key_down(self, event):
        """Trata teclas pressionadas."""
        if event.key == pygame.K_SPACE:
            # Toggle pause
            paused = self.engine.params.get('paused', False)
            self.engine.params.set('paused', not paused)
        
        elif event.key == pygame.K_r:
            # Reset população
            self.engine.send_command('reset_population')
        
        elif event.key == pygame.K_f:
            # Fit world in view
            self.engine.camera.fit_world(self.engine.world, self.screen_width, self.screen_height)
        
        elif event.key == pygame.K_t:
            # Toggle renderer
            simple = self.engine.params.get('simple_render', False)
            self.engine.params.set('simple_render', not simple)
            self.engine.send_command('change_renderer', simple=not simple)
        
        elif event.key == pygame.K_v:
            # Toggle vision
            show_vision = self.engine.params.get('bacteria_show_vision', False)
            self.engine.params.set('bacteria_show_vision', not show_vision)
    
    def _handle_continuous_keys(self, keys):
        """Trata teclas mantidas pressionadas.""" 
        # Controle de time scale
        time_scale = self.engine.params.get('time_scale', 1.0)
        
        if keys[pygame.K_PLUS] or keys[pygame.K_EQUALS]:
            time_scale = min(50.0, time_scale * 1.05)
            self.engine.params.set('time_scale', time_scale)
        
        if keys[pygame.K_MINUS]:
            time_scale = max(0.1, time_scale * 0.95)
            self.engine.params.set('time_scale', time_scale)
        
        # Movimento de câmera com WASD
        move_speed = 100.0 / self.engine.camera.zoom  # Velocidade adaptativa
        
        if keys[pygame.K_w] or keys[pygame.K_UP]:
            self.engine.camera.move(0, -move_speed / 60.0)  # Por frame
        if keys[pygame.K_s] or keys[pygame.K_DOWN]:
            self.engine.camera.move(0, move_speed / 60.0)
        if keys[pygame.K_a] or keys[pygame.K_LEFT]:
            self.engine.camera.move(-move_speed / 60.0, 0)
        if keys[pygame.K_d] or keys[pygame.K_RIGHT]:
            self.engine.camera.move(move_speed / 60.0, 0)


def bootstrap_pygame_simulation(params: Params, width: int = 1000, height: int = 700,
                               screen_width: int = 800, screen_height: int = 600) -> PygameView:
    """
    Bootstrap completo da simulação com Pygame.
    
    Args:
        params: Parâmetros da simulação
        width: Largura do mundo
        height: Altura do mundo  
        screen_width: Largura da tela
        screen_height: Altura da tela
        
    Returns:
        View Pygame configurada e pronta para rodar
    """
    # Cria componentes principais
    world = World(width, height)
    camera = Camera()
    
    # Fit inicial da câmera
    camera.fit_world(world, screen_width, screen_height, margin=0.1)
    
    # Cria engine
    engine = Engine(world, camera, params)
    
    # Cria view
    view = PygameView(engine, screen_width, screen_height)
    
    return view
