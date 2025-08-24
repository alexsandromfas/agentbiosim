"""
Indexação espacial para otimização de queries de proximidade.
"""
import math
from typing import Set, Any, Tuple, List


class SpatialHash:
    """
    Hash espacial para acelerar consultas de proximidade.
    Reutilizável entre frames para melhor performance.
    """
    
    def __init__(self, cell_size: float, width: float, height: float):
        self.cell_size = max(1.0, float(cell_size))
        self.width = width
        self.height = height
        self.cols = int(math.ceil(width / self.cell_size))
        self.rows = int(math.ceil(height / self.cell_size))
        self.buckets = {}  # Dict[Tuple[int, int], List[Any]]
    
    def clear(self):
        """Limpa todos os buckets para reuso."""
        self.buckets.clear()
    
    def _get_cells(self, x: float, y: float, r: float) -> List[Tuple[int, int]]:
        """Calcula células que o objeto ocupa."""
        min_cx = int((x - r) // self.cell_size)
        max_cx = int((x + r) // self.cell_size)
        min_cy = int((y - r) // self.cell_size)
        max_cy = int((y + r) // self.cell_size)
        
        cells = []
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                # Clamp para evitar células fora dos limites
                if 0 <= cx < self.cols and 0 <= cy < self.rows:
                    cells.append((cx, cy))
        return cells
    
    def insert(self, obj: Any, x: float, y: float, r: float):
        """Insere objeto nas células espaciais adequadas."""
        cells = self._get_cells(x, y, r)
        for cell in cells:
            if cell not in self.buckets:
                self.buckets[cell] = []
            self.buckets[cell].append(obj)
    
    def query_ball(self, x: float, y: float, r: float) -> Set[Any]:
        """
        Consulta objetos dentro de um raio.
        
        Returns:
            Conjunto de objetos que podem estar dentro do raio
        """
        cells = self._get_cells(x, y, r)
        found = set()
        
        for cell in cells:
            if cell in self.buckets:
                found.update(self.buckets[cell])
        
        return found
    
    def query_rectangle(self, min_x: float, min_y: float, 
                       max_x: float, max_y: float) -> Set[Any]:
        """Consulta objetos dentro de um retângulo."""
        min_cx = max(0, int(min_x // self.cell_size))
        max_cx = min(self.cols - 1, int(max_x // self.cell_size))
        min_cy = max(0, int(min_y // self.cell_size))
        max_cy = min(self.rows - 1, int(max_y // self.cell_size))
        
        found = set()
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                cell = (cx, cy)
                if cell in self.buckets:
                    found.update(self.buckets[cell])
        
        return found
    
    def get_stats(self) -> dict:
        """Retorna estatísticas para debugging."""
        total_objects = sum(len(bucket) for bucket in self.buckets.values())
        occupied_cells = len(self.buckets)
        total_cells = self.cols * self.rows
        
        return {
            'total_objects': total_objects,
            'occupied_cells': occupied_cells,
            'total_cells': total_cells,
            'occupancy_rate': occupied_cells / total_cells if total_cells > 0 else 0,
            'avg_objects_per_occupied_cell': total_objects / occupied_cells if occupied_cells > 0 else 0
        }


def resolve_collision(obj1, obj2):
    """
    Resolve colisão elástica simples entre dois objetos.
    Assume que objetos têm atributos x, y, vx, vy, m, r.
    """
    dx = obj1.x - obj2.x
    dy = obj1.y - obj2.y
    dist = math.hypot(dx, dy)
    
    if dist == 0:
        # Evita divisão por zero
        dist = 0.01
        dx = 0.01
        dy = 0
    
    overlap = obj1.r + obj2.r - dist
    if overlap <= 0:
        return  # Sem colisão
    
    # Separar objetos
    push_x = dx / dist * overlap
    push_y = dy / dist * overlap
    total_m = obj1.m + obj2.m
    
    if total_m == 0:
        total_m = 1.0
    
    # Separação baseada na massa
    obj1.x += push_x * (obj2.m / total_m)
    obj1.y += push_y * (obj2.m / total_m)
    obj2.x -= push_x * (obj1.m / total_m)  
    obj2.y -= push_y * (obj1.m / total_m)
    
    # Resposta elástica
    nx = dx / dist
    ny = dy / dist
    
    # Velocidade relativa
    dvx = obj1.vx - obj2.vx
    dvy = obj1.vy - obj2.vy
    rel_vel = dvx * nx + dvy * ny
    
    if rel_vel > 0:
        return  # Objetos se afastando
    
    # Impulso
    impulse = (2 * rel_vel) / (obj1.m + obj2.m)
    obj1.vx -= impulse * obj2.m * nx
    obj1.vy -= impulse * obj2.m * ny
    obj2.vx += impulse * obj1.m * nx
    obj2.vy += impulse * obj1.m * ny


def clamp_speed(obj, max_speed: float):
    """
    Limita velocidade de um objeto.
    Assume que objeto tem atributos vx, vy.
    """
    if max_speed is None or max_speed <= 0:
        obj.vx = 0.0
        obj.vy = 0.0
        return
    
    speed = math.hypot(obj.vx, obj.vy)
    if speed > max_speed:
        factor = max_speed / speed
        obj.vx *= factor
        obj.vy *= factor
