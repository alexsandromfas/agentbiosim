"""
Mundo físico e câmera para conversões de coordenadas.
"""
import math
from typing import Tuple


class World:
    """Representa o mundo físico (retangular ou circular)."""
    def __init__(self, width: float, height: float, shape: str = 'rectangular', radius: float = 400.0):
        self.width = max(1.0, width)
        self.height = max(1.0, height)
        self.shape = shape  # 'rectangular' ou 'circular'
        self.radius = max(10.0, radius)
        # Centro do círculo (usamos centro do retângulo base)
        self.cx = self.width / 2.0
        self.cy = self.height / 2.0

    def configure(self, shape: str, radius: float, width: float | None = None, height: float | None = None):
        self.shape = shape
        self.radius = max(10.0, radius)
        if width is not None and height is not None:
            self.width = max(1.0, width)
            self.height = max(1.0, height)
        # Recalcula centro sempre que muda dimensão
        self.cx = self.width / 2.0
        self.cy = self.height / 2.0

    def is_inside(self, x: float, y: float, radius: float = 0.0) -> bool:
        if self.shape == 'circular':
            dx = x - self.cx
            dy = y - self.cy
            return (dx*dx + dy*dy) <= (self.radius - radius) ** 2
        return (radius <= x <= self.width - radius and radius <= y <= self.height - radius)

    def clamp_position(self, x: float, y: float, radius: float = 0.0) -> Tuple[float, float]:
        if self.shape == 'circular':
            dx = x - self.cx
            dy = y - self.cy
            dist = math.hypot(dx, dy)
            max_dist = max(1e-6, self.radius - radius)
            if dist > max_dist:
                scale = max_dist / dist
                x = self.cx + dx * scale
                y = self.cy + dy * scale
            return x, y
        x = max(radius, min(self.width - radius, x))
        y = max(radius, min(self.height - radius, y))
        return x, y

    def wrap_position(self, x: float, y: float) -> Tuple[float, float]:
        if self.shape == 'circular':
            # Para mundo circular, não faz wrap; apenas clamp radial
            return self.clamp_position(x, y)
        x = x % self.width
        y = y % self.height
        return x, y

    def distance_to_wall(self, x: float, y: float) -> float:
        if self.shape == 'circular':
            return self.radius - math.hypot(x - self.cx, y - self.cy)
        return min(x, y, self.width - x, self.height - y)


class Camera:
    """
    Câmera para conversões entre coordenadas do mundo e da tela.
    """
    
    def __init__(self, x: float = 0.0, y: float = 0.0, zoom: float = 1.0):
        self.x = x
        self.y = y
        self.zoom = max(0.01, zoom)
    
    def world_to_screen(self, world_x: float, world_y: float) -> Tuple[float, float]:
        """Converte coordenadas do mundo para coordenadas da tela."""
        screen_x = (world_x - self.x) * self.zoom
        screen_y = (world_y - self.y) * self.zoom
        return screen_x, screen_y
    
    def screen_to_world(self, screen_x: float, screen_y: float) -> Tuple[float, float]:
        """Converte coordenadas da tela para coordenadas do mundo."""
        world_x = screen_x / self.zoom + self.x
        world_y = screen_y / self.zoom + self.y
        return world_x, world_y
    
    def move(self, dx: float, dy: float):
        """Move a câmera por um delta."""
        self.x += dx
        self.y += dy
    
    def zoom_at(self, screen_x: float, screen_y: float, zoom_factor: float):
        """
        Aplica zoom focalizando em um ponto específico da tela.
        """
        # Posição do mundo antes do zoom
        world_pos_before = self.screen_to_world(screen_x, screen_y)
        
        # Aplica zoom com limites
        self.zoom = max(0.01, min(20.0, self.zoom * zoom_factor))
        
        # Posição do mundo depois do zoom
        world_pos_after = self.screen_to_world(screen_x, screen_y)
        
        # Ajusta posição da câmera para manter o ponto focal
        self.x += world_pos_before[0] - world_pos_after[0]
        self.y += world_pos_before[1] - world_pos_after[1]
    
    def fit_world(self, world: World, screen_width: float, screen_height: float, margin: float = 0.1):
        """
        Ajusta câmera para mostrar todo o mundo na tela.
        
        Args:
            world: Mundo a ser mostrado
            screen_width: Largura da tela
            screen_height: Altura da tela  
            margin: Margem percentual (0.1 = 10%)
        """
        # Calcula zoom para caber com margem
        zoom_x = screen_width / (world.width * (1 + margin))
        zoom_y = screen_height / (world.height * (1 + margin))
        self.zoom = min(zoom_x, zoom_y)
        
        # Centraliza
        self.x = world.width / 2 - screen_width / (2 * self.zoom)
        self.y = world.height / 2 - screen_height / (2 * self.zoom)
    
    def copy(self) -> 'Camera':
        """Cria cópia da câmera."""
        return Camera(self.x, self.y, self.zoom)
