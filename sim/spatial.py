"""
Indexação espacial para otimização de queries de proximidade.
"""
import math
from typing import Set, Any, Tuple, List

import numpy as np


class SpatialHash:
    """
    Hash espacial para acelerar consultas de proximidade.
    Reutilizável entre frames para melhor performance.
    """
    
    def __init__(self, cell_size: float, width: float, height: float,
                 min_x: float = 0.0, min_y: float = 0.0,
                 numeric_index_enabled: bool = False):
        self.cell_size = max(1.0, float(cell_size))
        self.width = max(1.0, float(width))
        self.height = max(1.0, float(height))
        self.min_x = float(min_x)
        self.min_y = float(min_y)
        self.max_x = self.min_x + self.width
        self.max_y = self.min_y + self.height
        self.numeric_index_enabled = bool(numeric_index_enabled)
        self.cols = max(1, int(math.ceil(self.width / self.cell_size)))
        self.rows = max(1, int(math.ceil(self.height / self.cell_size)))
        self.buckets = {}  # Dict[Tuple[int, int], List[Any]]
        # Compatibility object buckets stay above. Retina batches use these
        # parallel integer buckets plus reusable numeric arrays filled once
        # during each spatial rebuild.
        self.index_buckets = {}  # Dict[Tuple[int, int], List[int]]
        self._object_indices = {}
        self._numeric_objects = []
        self._numeric_count = 0
        self._numeric_capacity = 0
        self._numeric_snapshot_ready = False
        self.numeric_x = np.empty(0, dtype=np.float64)
        self.numeric_y = np.empty(0, dtype=np.float64)
        self.numeric_r = np.empty(0, dtype=np.float64)
        self.numeric_type = np.empty(0, dtype=np.int8)
        self.numeric_color_r = np.empty(0, dtype=np.float64)
        self.numeric_color_g = np.empty(0, dtype=np.float64)
        self.numeric_color_b = np.empty(0, dtype=np.float64)
    
    def clear(self):
        """Limpa todos os buckets para reuso."""
        self.buckets.clear()
        self.index_buckets.clear()
        self._object_indices.clear()
        self._numeric_objects.clear()
        self._numeric_count = 0
        self._numeric_snapshot_ready = False

    def _ensure_numeric_capacity(self, required: int):
        if required <= self._numeric_capacity:
            return
        new_capacity = max(64, required, self._numeric_capacity * 2)
        count = self._numeric_count

        def grow(old, dtype):
            new = np.empty(new_capacity, dtype=dtype)
            copy_count = min(count, int(getattr(old, "shape", (0,))[0]))
            if copy_count:
                new[:copy_count] = old[:copy_count]
            return new

        self.numeric_x = grow(self.numeric_x, np.float64)
        self.numeric_y = grow(self.numeric_y, np.float64)
        self.numeric_r = grow(self.numeric_r, np.float64)
        self.numeric_type = grow(self.numeric_type, np.int8)
        self.numeric_color_r = grow(self.numeric_color_r, np.float64)
        self.numeric_color_g = grow(self.numeric_color_g, np.float64)
        self.numeric_color_b = grow(self.numeric_color_b, np.float64)
        self._numeric_capacity = new_capacity

    @staticmethod
    def _color01(obj: Any) -> tuple[float, float, float]:
        color = getattr(obj, "color", (255, 255, 255)) or (255, 255, 255)
        try:
            r, g, b = color[:3]
            return (
                max(0.0, min(1.0, float(r) / 255.0)),
                max(0.0, min(1.0, float(g) / 255.0)),
                max(0.0, min(1.0, float(b) / 255.0)),
            )
        except Exception:
            return 1.0, 1.0, 1.0

    def _register_numeric_object(self, obj: Any, x: float, y: float, r: float) -> int:
        key = id(obj)
        existing = self._object_indices.get(key)
        if existing is not None:
            return existing
        index = self._numeric_count
        self._object_indices[key] = index
        self._numeric_objects.append(obj)
        self._numeric_count += 1
        self._numeric_snapshot_ready = False
        return index

    def ensure_numeric_snapshot(self):
        """Materialize reusable numeric perception arrays for current buckets."""
        if self._numeric_snapshot_ready:
            return
        count = self._numeric_count
        self._ensure_numeric_capacity(count)
        for index, obj in enumerate(self._numeric_objects):
            self.numeric_x[index] = float(getattr(obj, "x", 0.0))
            self.numeric_y[index] = float(getattr(obj, "y", 0.0))
            self.numeric_r[index] = max(0.0, float(getattr(obj, "r", 0.0)))
            self.numeric_type[index] = int(getattr(obj, "type_code", -1))
            cr, cg, cb = self._color01(obj)
            self.numeric_color_r[index] = cr
            self.numeric_color_g[index] = cg
            self.numeric_color_b[index] = cb
        self._numeric_snapshot_ready = True

    def index_of(self, obj: Any) -> int:
        """Return the numeric row for an object in the current rebuild."""
        return int(self._object_indices.get(id(obj), -1))
    
    def _get_cells(self, x: float, y: float, r: float) -> List[Tuple[int, int]]:
        """Calcula células que o objeto ocupa."""
        r = max(0.0, float(r))
        min_cx = math.floor((x - r - self.min_x) / self.cell_size)
        max_cx = math.floor((x + r - self.min_x) / self.cell_size)
        min_cy = math.floor((y - r - self.min_y) / self.cell_size)
        max_cy = math.floor((y + r - self.min_y) / self.cell_size)
        if max_cx < 0 or max_cy < 0 or min_cx >= self.cols or min_cy >= self.rows:
            return []
        min_cx = max(0, int(min_cx))
        max_cx = min(self.cols - 1, int(max_cx))
        min_cy = max(0, int(min_cy))
        max_cy = min(self.rows - 1, int(max_cy))
        
        cells = []
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                # Clamp para evitar células fora dos limites
                cells.append((cx, cy))
        return cells
    
    def insert(self, obj: Any, x: float, y: float, r: float):
        """Insere objeto nas células espaciais adequadas."""
        numeric_index = self._register_numeric_object(obj, x, y, r) if self.numeric_index_enabled else -1
        cells = self._get_cells(x, y, r)
        for cell in cells:
            if cell not in self.buckets:
                self.buckets[cell] = []
            self.buckets[cell].append(obj)
            if self.numeric_index_enabled:
                if cell not in self.index_buckets:
                    self.index_buckets[cell] = []
                self.index_buckets[cell].append(numeric_index)
    
    def _cell_range_for_circle(self, x: float, y: float, r: float):
        r = max(0.0, float(r))
        min_cx = math.floor((x - r - self.min_x) / self.cell_size)
        max_cx = math.floor((x + r - self.min_x) / self.cell_size)
        min_cy = math.floor((y - r - self.min_y) / self.cell_size)
        max_cy = math.floor((y + r - self.min_y) / self.cell_size)
        if max_cx < 0 or max_cy < 0 or min_cx >= self.cols or min_cy >= self.rows:
            return None
        return (
            max(0, int(min_cx)),
            min(self.cols - 1, int(max_cx)),
            max(0, int(min_cy)),
            min(self.rows - 1, int(max_cy)),
        )

    def query_ball(self, x: float, y: float, r: float) -> Set[Any]:
        """
        Consulta objetos dentro de um raio.
        
        Returns:
            Conjunto de objetos que podem estar dentro do raio
        """
        return self.query_ball_into(x, y, r, set())

    def query_ball_into(self, x: float, y: float, r: float, out: Set[Any]) -> Set[Any]:
        """Consulta por raio preenchendo um set reutilizavel."""
        out.clear()
        cell_range = self._cell_range_for_circle(x, y, r)
        if cell_range is None:
            return out
        min_cx, max_cx, min_cy, max_cy = cell_range
        buckets = self.buckets
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                bucket = buckets.get((cx, cy))
                if bucket:
                    out.update(bucket)
        
        return out

    def query_ball_filtered(self, x: float, y: float, r: float, type_codes) -> Set[Any]:
        """Consulta por raio retornando apenas objetos com type_code desejado."""
        return self.query_ball_filtered_into(x, y, r, type_codes, set())

    def query_ball_filtered_into(self, x: float, y: float, r: float, type_codes, out: Set[Any]) -> Set[Any]:
        """Consulta por raio/tipo preenchendo um set reutilizavel."""
        out.clear()
        cell_range = self._cell_range_for_circle(x, y, r)
        if cell_range is None:
            return out
        min_cx, max_cx, min_cy, max_cy = cell_range
        buckets = self.buckets
        single_type = None
        if isinstance(type_codes, int):
            single_type = int(type_codes)
        else:
            try:
                if len(type_codes) == 1:
                    single_type = int(next(iter(type_codes)))
            except Exception:
                single_type = None
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                bucket = buckets.get((cx, cy))
                if not bucket:
                    continue
                if single_type is not None:
                    for obj in bucket:
                        if getattr(obj, 'type_code', -1) == single_type:
                            out.add(obj)
                else:
                    for obj in bucket:
                        if getattr(obj, 'type_code', -1) in type_codes:
                            out.add(obj)
        return out

    def query_ball_filtered_indices_into(self, x: float, y: float, r: float, type_codes, out: Set[int]) -> Set[int]:
        """Consulta por raio/tipo retornando linhas dos buffers numericos."""
        if not self.numeric_index_enabled:
            out.clear()
            return out
        self.ensure_numeric_snapshot()
        out.clear()
        cell_range = self._cell_range_for_circle(x, y, r)
        if cell_range is None:
            return out
        min_cx, max_cx, min_cy, max_cy = cell_range
        single_type = None
        if isinstance(type_codes, int):
            single_type = int(type_codes)
        else:
            try:
                if len(type_codes) == 1:
                    single_type = int(next(iter(type_codes)))
            except Exception:
                single_type = None
        index_buckets = self.index_buckets
        numeric_type = self.numeric_type
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                bucket = index_buckets.get((cx, cy))
                if not bucket:
                    continue
                if single_type is not None:
                    for index in bucket:
                        if int(numeric_type[index]) == single_type:
                            out.add(index)
                else:
                    for index in bucket:
                        if int(numeric_type[index]) in type_codes:
                            out.add(index)
        return out
    
    def query_rectangle(self, min_x: float, min_y: float, 
                       max_x: float, max_y: float) -> Set[Any]:
        """Consulta objetos dentro de um retângulo."""
        min_cx = max(0, int(math.floor((min_x - self.min_x) / self.cell_size)))
        max_cx = min(self.cols - 1, int(math.floor((max_x - self.min_x) / self.cell_size)))
        min_cy = max(0, int(math.floor((min_y - self.min_y) / self.cell_size)))
        max_cy = min(self.rows - 1, int(math.floor((max_y - self.min_y) / self.cell_size)))
        if min_cx > max_cx or min_cy > max_cy:
            return set()
        
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
