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
        
        # Pygame
        self.screen: Optional[pygame.Surface] = None
        self.clock = pygame.time.Clock()
        self.running = False
    
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
            real_dt = self.clock.tick(self.engine.params.get('fps', 60)) / 1000.0
            self.engine.step(real_dt)
            
            # Renderiza
            if self.screen:
                self.engine.render(self.screen)
                pygame.display.flip()
    
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
            
            # Tenta selecionar agente
            agent = self.engine.get_agent_at_position(world_x, world_y)
            if agent:
                self.engine.selected_agent = agent
            else:
                # Adiciona comida se não selecionou nada
                self.engine.send_command('add_food', world_x=world_x, world_y=world_y)
                self.engine.selected_agent = None
        
        elif event.button == 2:  # Botão do meio
            world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
            self.engine.send_command('add_bacteria', world_x=world_x, world_y=world_y)
        
        elif event.button == 3:  # Botão direito - inicia pan
            # Se existir protótipo carregado, insere instância no local do clique
            if getattr(self.engine, 'current_agent_prototype', None) and \
               self.engine.current_agent_prototype in getattr(self.engine, 'loaded_agent_prototypes', {}):
                world_x, world_y = self.engine.camera.screen_to_world(event.pos[0], event.pos[1])
                self.engine.send_command('spawn_loaded_agent', world_x=world_x, world_y=world_y)
            else:
                # Comportamento original: iniciar pan
                self.dragging = True
                self.drag_last_pos = event.pos
    
    def _handle_mouse_up(self, event):
        """Trata soltar do mouse."""
        if event.button == 3:  # Botão direito
            self.dragging = False
    
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
            time_scale = min(10.0, time_scale * 1.05)
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
