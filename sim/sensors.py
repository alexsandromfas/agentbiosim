"""
Sensores e consulta de cena para percepção dos agentes.
"""
import math
import numpy as np
from typing import List, Optional, TYPE_CHECKING, Set, Any, Sequence


RETINA_VISION_MODE_SINGLE = "single"
RETINA_VISION_MODE_FULLBODY = "fullbody"
RETINA_VISION_MODE_SECTOR = "sector"
RETINA_VISION_MODES = {RETINA_VISION_MODE_SINGLE, RETINA_VISION_MODE_FULLBODY, RETINA_VISION_MODE_SECTOR}
RETINA_INPUT_MODE_DISTANCE_ONLY = "distance_only"
RETINA_INPUT_MODE_COLOR_DISTANCE = "color_distance"
RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE = "color_plus_distance"
RETINA_INPUT_MODE_COLOR_ONLY = "color_only"
RETINA_INPUT_MODES = {
    RETINA_INPUT_MODE_DISTANCE_ONLY,
    RETINA_INPUT_MODE_COLOR_DISTANCE,
    RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
    RETINA_INPUT_MODE_COLOR_ONLY,
}
RETINA_CHANNEL_ORDER = ("r", "g", "b", "rd", "gd", "bd", "d")
RETINA_COLOR_CHANNELS = ("r", "g", "b")
_RETINA_RAY_CACHE: dict[tuple[int, float], tuple[np.ndarray, np.ndarray, np.ndarray]] = {}


def normalize_retina_input_mode(value: Any) -> str:
    if isinstance(value, str):
        value = value.strip().lower()
        aliases = {
            "distancia": RETINA_INPUT_MODE_DISTANCE_ONLY,
            "distancia_apenas": RETINA_INPUT_MODE_DISTANCE_ONLY,
            "distance": RETINA_INPUT_MODE_DISTANCE_ONLY,
            "weighted": RETINA_INPUT_MODE_COLOR_DISTANCE,
            "color_weighted": RETINA_INPUT_MODE_COLOR_DISTANCE,
            "cor_distancia": RETINA_INPUT_MODE_COLOR_DISTANCE,
            "color_distance": RETINA_INPUT_MODE_COLOR_DISTANCE,
            "separate": RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
            "separadas": RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
            "color_plus_distance": RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
            "cor_distancia_separadas": RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
            "color_only": RETINA_INPUT_MODE_COLOR_ONLY,
            "cor_apenas": RETINA_INPUT_MODE_COLOR_ONLY,
        }
        value = aliases.get(value, value)
        if value in RETINA_INPUT_MODES:
            return value
    return RETINA_INPUT_MODE_DISTANCE_ONLY


def normalize_retina_channels(channels: Sequence[str] | None) -> tuple[str, ...]:
    normalized = []
    for channel in channels or ("d",):
        ch = str(channel).strip().lower().replace("*", "")
        if ch in {"dr"}:
            ch = "rd"
        elif ch in {"dg"}:
            ch = "gd"
        elif ch in {"db"}:
            ch = "bd"
        if ch in RETINA_CHANNEL_ORDER and ch not in normalized:
            normalized.append(ch)
    if not normalized:
        normalized.append("d")
    order = {channel: index for index, channel in enumerate(RETINA_CHANNEL_ORDER)}
    return tuple(sorted(normalized, key=lambda ch: order.get(ch, 999)))


def retina_input_mode_from_channels(channels: Sequence[str] | None) -> str:
    channels = normalize_retina_channels(channels)
    has_d = "d" in channels
    has_weighted = any(ch in channels for ch in ("rd", "gd", "bd"))
    has_pure = any(ch in channels for ch in RETINA_COLOR_CHANNELS)
    if has_weighted:
        return RETINA_INPUT_MODE_COLOR_DISTANCE
    if has_pure and has_d:
        return RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE
    if has_pure:
        return RETINA_INPUT_MODE_COLOR_ONLY
    return RETINA_INPUT_MODE_DISTANCE_ONLY


def retina_input_mode_label(mode: Any) -> str:
    labels = {
        RETINA_INPUT_MODE_DISTANCE_ONLY: "Distancia apenas",
        RETINA_INPUT_MODE_COLOR_DISTANCE: "Cor ponderada pela distancia",
        RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE: "Cor + distancia separadas",
        RETINA_INPUT_MODE_COLOR_ONLY: "Cor apenas",
    }
    return labels.get(normalize_retina_input_mode(mode), labels[RETINA_INPUT_MODE_DISTANCE_ONLY])


def retina_channel_label(channel: str) -> str:
    channel = str(channel).lower()
    labels = {
        "r": "R",
        "g": "G",
        "b": "B",
        "rd": "R*D",
        "gd": "G*D",
        "bd": "B*D",
        "d": "D",
    }
    return labels.get(channel, channel.upper())


def retina_channels_description(channels: Sequence[str] | None) -> str:
    return ", ".join(retina_channel_label(ch) for ch in normalize_retina_channels(channels))


def active_retina_channels(params: Any, prefix: str, fallback: Sequence[str] | None = None) -> tuple[str, ...]:
    """Return enabled retina channels for an organism template."""
    get = params.get if hasattr(params, "get") else (lambda key, default=None: default)
    sentinel = object()
    fallback_channels = normalize_retina_channels(fallback)
    raw_mode = get(f"{prefix}_retina_input_mode", sentinel)
    if raw_mode is sentinel:
        mode = retina_input_mode_from_channels(fallback_channels)
    else:
        mode = normalize_retina_input_mode(raw_mode)

    selected_colors = []
    fallback_colors = {
        ch[0] if len(ch) == 2 and ch.endswith("d") else ch
        for ch in fallback_channels
        if ch in RETINA_COLOR_CHANNELS or (len(ch) == 2 and ch.endswith("d"))
    }
    for channel in RETINA_COLOR_CHANNELS:
        raw = get(f"{prefix}_retina_channel_{channel}", sentinel)
        enabled = (channel in fallback_colors) if raw is sentinel else bool(raw)
        if enabled:
            selected_colors.append(channel)

    if mode == RETINA_INPUT_MODE_DISTANCE_ONLY:
        return ("d",)
    if mode == RETINA_INPUT_MODE_COLOR_DISTANCE:
        if not selected_colors:
            return ("d",)
        return tuple(f"{channel}d" for channel in selected_colors)
    if mode == RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE:
        if not selected_colors:
            return ("d",)
        return tuple(selected_colors + ["d"])
    if mode == RETINA_INPUT_MODE_COLOR_ONLY:
        if not selected_colors:
            return ("d",)
        return tuple(selected_colors)
    return fallback_channels


def retina_input_size(params: Any, prefix: str, default_retina_count: int = 18) -> int:
    count = max(1, int((params.get if hasattr(params, "get") else lambda _k, d=None: d)(
        f"{prefix}_retina_count", default_retina_count
    )))
    eyes = normalize_eye_count((params.get if hasattr(params, "get") else lambda _k, d=None: d)(
        f"{prefix}_eye_count", 1
    ))
    return count * eyes * len(active_retina_channels(params, prefix))


def normalize_eye_count(value: Any) -> int:
    try:
        return 2 if int(float(value)) >= 2 else 1
    except (TypeError, ValueError):
        return 1


def _object_color01(obj: Any) -> tuple[float, float, float]:
    color = getattr(obj, "color", (255, 255, 255)) or (255, 255, 255)
    try:
        r, g, b = color[:3]
        return (
            max(0.0, min(1.0, float(r) / 255.0)),
            max(0.0, min(1.0, float(g) / 255.0)),
            max(0.0, min(1.0, float(b) / 255.0)),
        )
    except Exception:
        return (1.0, 1.0, 1.0)


def normalize_retina_vision_mode(value: Any) -> str:
    """Return a supported retina mapping mode.

    ``single`` maps each visible object's centroid to one retina ray. It is the
    historical fast approximation and is kept as the default for compatibility.

    ``fullbody`` casts each retina ray against the circular body of visible
    objects, so wide/near objects can activate more than one retina ray. It is
    the geometrically defined mode for stricter experiments.

    ``sector`` maps visible objects into angular sectors. It preserves the
    retina count, eyes, FOV and RGB/distance channels, but avoids per-ray
    circle intersections. It is intended for large populations.
    """
    if isinstance(value, str):
        value = value.strip().lower()
        aliases = {
            "setorial": RETINA_VISION_MODE_SECTOR,
            "sectorial": RETINA_VISION_MODE_SECTOR,
            "fast_sector": RETINA_VISION_MODE_SECTOR,
            "visao_setorial": RETINA_VISION_MODE_SECTOR,
            "visão_setorial": RETINA_VISION_MODE_SECTOR,
        }
        value = aliases.get(value, value)
        if value in RETINA_VISION_MODES:
            return value
    return RETINA_VISION_MODE_SINGLE


def _get_retina_relative_rays(retina_count: int, half_fov: float) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    count = max(1, int(retina_count))
    key = (count, round(float(half_fov), 12))
    cached = _RETINA_RAY_CACHE.get(key)
    if cached is not None:
        return cached
    if count > 1:
        ray_rel = np.linspace(-half_fov, half_fov, count, dtype=np.float32)
    else:
        ray_rel = np.array([0.0], dtype=np.float32)
    cos_rel = np.cos(ray_rel).astype(np.float32)
    sin_rel = np.sin(ray_rel).astype(np.float32)
    cached = (ray_rel, cos_rel, sin_rel)
    _RETINA_RAY_CACHE[key] = cached
    return cached

if TYPE_CHECKING:
    from .entities import Agent
    from .controllers import Params
    from .spatial import SpatialHash


def ray_circle_intersect(px: float, py: float, dx: float, dy: float, 
                        cx: float, cy: float, cr: float) -> Optional[float]:
    """
    Calcula interseção entre raio e círculo.
    
    Args:
        px, py: Origem do raio
        dx, dy: Direção do raio (deve estar normalizada)
        cx, cy: Centro do círculo
        cr: Raio do círculo
        
    Returns:
        Distância até interseção ou None se não houver
    """
    # Vetor da origem do raio ao centro do círculo
    ox = px - cx
    oy = py - cy
    
    # Coeficientes da equação quadrática
    # (o + t*d)² = r²
    # t²(d·d) + 2t(d·o) + (o·o - r²) = 0
    b = dx * ox + dy * oy  # d·o
    c = ox * ox + oy * oy - cr * cr  # o·o - r²
    
    # Discriminante
    discriminant = b * b - c
    if discriminant < 0:
        return None  # Sem interseção
    
    sqrt_d = math.sqrt(discriminant)
    t1 = -b - sqrt_d
    t2 = -b + sqrt_d
    
    # Queremos a menor distância positiva
    candidates = [t for t in (t1, t2) if t >= 0]
    if not candidates:
        return None
    
    return min(candidates)


def _raycast_hit_from_candidates(px: float, py: float, dx: float, dy: float,
                                 max_distance: float, candidates,
                                 ignore: Any = None) -> Optional[tuple[float, Any]]:
    """Raycast sobre uma lista ja filtrada de candidatos.

    Evita fazer uma query espacial por raio; usado principalmente por retinas
    multiolho, onde varios raios compartilham praticamente a mesma vizinhanca.
    """
    best_distance = None
    best_obj = None
    for obj in candidates or ():
        if obj is ignore:
            continue
        distance = ray_circle_intersect(px, py, dx, dy, obj.x, obj.y, obj.r)
        if distance is not None and 0 <= distance <= max_distance:
            if best_distance is None or distance < best_distance:
                best_distance = distance
                best_obj = obj
    if best_distance is None:
        return None
    return best_distance, best_obj


def _retina_channel_codes(channels: Sequence[str] | None) -> np.ndarray:
    mapping = {"d": 0, "r": 1, "g": 2, "b": 3, "rd": 4, "gd": 5, "bd": 6}
    return np.asarray([mapping.get(ch, 0) for ch in normalize_retina_channels(channels)], dtype=np.int8)


def _channel_value(channel: str, activation: float, color: tuple[float, float, float]) -> float:
    if channel == "r":
        return color[0]
    if channel == "g":
        return color[1]
    if channel == "b":
        return color[2]
    if channel == "rd":
        return activation * color[0]
    if channel == "gd":
        return activation * color[1]
    if channel == "bd":
        return activation * color[2]
    return activation


def _sector_retina_from_candidates(
    sensor: 'RetinaSensor',
    agent: Any,
    candidates,
    type_codes: Sequence[int] | None = None,
    spatial_filtered: bool = True,
) -> tuple[list[float], list[float]]:
    """Fast angular-sector retina mapping for one agent.

    Each retina receives the nearest visible object whose center falls inside
    that angular sector. This is intentionally cheaper than ray/body
    intersections and keeps ``last_distance_inputs`` useful for drawing rays.
    """
    retina_count = max(1, int(getattr(sensor, "retina_count", 1)))
    eye_count = normalize_eye_count(getattr(sensor, "eye_count", 1))
    channels = normalize_retina_channels(getattr(sensor, "channels", ("d",)))
    stride = max(1, len(channels))
    total_rays = retina_count * eye_count
    inputs = [0.0] * (total_rays * stride)
    distance_inputs = [0.0] * total_rays
    vision_radius = max(1e-9, float(getattr(sensor, "vision_radius", 0.0)))
    half_fov = math.radians(float(getattr(sensor, "fov_degrees", 0.0)) * 0.5)
    if half_fov <= 0.0 or not candidates:
        return inputs, distance_inputs

    type_code_set = set(type_codes or ())
    specs = sensor._eye_specs() if hasattr(sensor, "_eye_specs") else [(0.0, 0.0)]
    for eye_idx in range(eye_count):
        pos_offset, gaze_offset = specs[min(eye_idx, len(specs) - 1)]
        position_angle = float(getattr(agent, "angle", 0.0)) + pos_offset
        eye_x = float(getattr(agent, "x", 0.0)) + math.cos(position_angle) * float(getattr(agent, "r", 0.0))
        eye_y = float(getattr(agent, "y", 0.0)) + math.sin(position_angle) * float(getattr(agent, "r", 0.0))
        gaze_angle = float(getattr(agent, "angle", 0.0)) + gaze_offset
        best_dist = [math.inf] * retina_count
        best_color = [(0.0, 0.0, 0.0)] * retina_count

        for obj in candidates:
            if obj is agent:
                continue
            if not spatial_filtered and type_code_set and getattr(obj, "type_code", -1) not in type_code_set:
                continue
            dx = float(getattr(obj, "x", 0.0)) - eye_x
            dy = float(getattr(obj, "y", 0.0)) - eye_y
            center_dist = math.hypot(dx, dy)
            obj_r = max(0.0, float(getattr(obj, "r", 0.0)))
            eff_dist = center_dist - obj_r
            if eff_dist > vision_radius:
                continue
            obj_angle = math.atan2(dy, dx)
            rel_angle = (obj_angle - gaze_angle + math.pi) % (2.0 * math.pi) - math.pi
            if abs(rel_angle) > half_fov:
                continue
            if eff_dist < 0.0:
                eff_dist = 0.0
            if retina_count > 1:
                rel = (rel_angle + half_fov) / (2.0 * half_fov) * (retina_count - 1)
                ray_idx = max(0, min(retina_count - 1, int(math.floor(rel + 0.5))))
            else:
                ray_idx = 0
            if eff_dist < best_dist[ray_idx]:
                best_dist[ray_idx] = eff_dist
                best_color[ray_idx] = _object_color01(obj)

        for local_idx in range(retina_count):
            ray_idx = eye_idx * retina_count + local_idx
            dist = best_dist[local_idx]
            if math.isfinite(dist):
                activation = max(0.0, min(1.0, (vision_radius - dist) / vision_radius))
                color = best_color[local_idx]
            else:
                activation = 0.0
                color = (0.0, 0.0, 0.0)
            distance_inputs[ray_idx] = activation
            start = ray_idx * stride
            for channel_idx, channel in enumerate(channels):
                inputs[start + channel_idx] = _channel_value(channel, activation, color)
    return inputs, distance_inputs


class SceneQuery:
    """
    Serviço para consultas espaciais da cena.
    Usado pelos sensores para fazer raycasts e queries de proximidade.
    """
    
    def __init__(self, spatial_hash: Optional['SpatialHash'], entities: dict, params: 'Params',
                 obstacles: Any = None):
        self.spatial_hash = spatial_hash
        self.entities = entities  # {'bacteria': [...], 'predators': [...], 'foods': [...]}
        self.params = params
        self.obstacles = obstacles
    
    def raycast(self, px: float, py: float, dx: float, dy: float, max_distance: float,
                ignore: Any = None, see_food: bool = True, see_bacteria: bool = False, 
                see_predators: bool = False) -> Optional[float]:
        """
        Faz raycast na cena procurando pela primeira interseção.
        
        Args:
            px, py: Origem do raio
            dx, dy: Direção do raio (assumido já normalizado)
            max_distance: Distância máxima do raio
            ignore: Objeto a ignorar (normalmente o próprio agente)
            see_food: Se deve detectar comida
            see_bacteria: Se deve detectar bactérias
            see_predators: Se deve detectar predadores
            
        Returns:
            Distância até primeira interseção ou None
        """
        # Normaliza direção para garantir
        mag = math.hypot(dx, dy)
        if mag == 0:
            return None
        dxn, dyn = dx / mag, dy / mag
        
        # Coleta candidatos
        candidates = self._get_candidates_for_ray(px, py, dxn, dyn, max_distance, 
                                                see_food, see_bacteria, see_predators)
        
        # Testa interseções
        best_distance = None
        for obj in candidates:
            if obj is ignore:
                continue
                
            # Assume que todos os objetos têm x, y, r
            distance = ray_circle_intersect(px, py, dxn, dyn, obj.x, obj.y, obj.r)
            if distance is not None and 0 <= distance <= max_distance:
                if best_distance is None or distance < best_distance:
                    best_distance = distance
        
        return best_distance

    def raycast_hit(self, px: float, py: float, dx: float, dy: float, max_distance: float,
                    ignore: Any = None, see_food: bool = True, see_bacteria: bool = False,
                    see_predators: bool = False) -> Optional[tuple[float, Any]]:
        """Faz raycast e retorna ``(distancia, objeto)`` do primeiro impacto."""
        mag = math.hypot(dx, dy)
        if mag == 0:
            return None
        dxn, dyn = dx / mag, dy / mag
        candidates = self._get_candidates_for_ray(
            px, py, dxn, dyn, max_distance, see_food, see_bacteria, see_predators
        )
        best_distance = None
        best_obj = None
        for obj in candidates:
            if obj is ignore:
                continue
            distance = ray_circle_intersect(px, py, dxn, dyn, obj.x, obj.y, obj.r)
            if distance is not None and 0 <= distance <= max_distance:
                if best_distance is None or distance < best_distance:
                    best_distance = distance
                    best_obj = obj
        if best_distance is None:
            return None
        return best_distance, best_obj
    
    def _get_candidates_for_ray(self, px: float, py: float, dx: float, dy: float, 
                               max_distance: float, see_food: bool, see_bacteria: bool, 
                               see_predators: bool) -> Set[Any]:
        """Coleta candidatos para teste de raycast."""
        candidates = set()
        
        # Usa spatial hash se disponível
        if self.spatial_hash:
            # Calcula região aproximada do raio
            end_x = px + dx * max_distance
            end_y = py + dy * max_distance
            margin = max(
                self.params.get('food_max_r', 5.0),
                self.params.get('bacteria_max_r', 12.0),
                self.params.get('predator_max_r', 18.0)
            )
            
            # Query região expandida
            query_radius = max_distance + margin
            spatial_candidates = self.spatial_hash.query_ball(
                px + dx * (max_distance * 0.5), 
                py + dy * (max_distance * 0.5), 
                query_radius
            )
            candidates.update(spatial_candidates)
        else:
            # Fallback: busca linear
            if see_food:
                candidates.update(self.entities.get('foods', []))
            if see_bacteria:
                candidates.update(self.entities.get('bacteria', []))
            if see_predators:
                candidates.update(self.entities.get('predators', []))
        
        # Filtra por tipo se necessário (spatial hash pode retornar todos os tipos)
        if self.spatial_hash:
            # type_code: 0=food,1=bacteria,2=predator
            filtered = set()
            for obj in candidates:
                tc = getattr(obj, 'type_code', -1)
                if (tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators):
                    filtered.add(obj)
            candidates = filtered
        
        return candidates
    
    def _get_object_type(self, obj) -> str:
        """Determina tipo do objeto baseado na classe ou atributos."""
        tc = getattr(obj, 'type_code', -1)
        if tc == 0:
            return 'food'
        if tc == 1:
            return 'bacteria'
        if tc == 2:
            return 'predator'
        # Fallback lento (deve desaparecer após migração completa)
        if hasattr(obj, 'is_predator'):
            return 'predator' if getattr(obj, 'is_predator') else 'bacteria'
        return 'food'


# Retina mode semantics:
# - single: historical centroid approximation, at most one ray per object.
# - fullbody: geometric ray-circle intersection, wide objects can hit many rays.
# - sector: fast angular-sector mapping, optimized for large populations.
class RetinaSensor:
    """
    Sensor de retina para visão dos agentes.
    Configura raios de visão em leque e detecta objetos.
    """
    
    def __init__(self, retina_count: int = 18, vision_radius: float = 120.0,
                 fov_degrees: float = 180.0, skip: int = 0,
                 see_food: bool = True, see_bacteria: bool = False, see_predators: bool = False,
                 see_obstacles: bool = False, see_all: bool = False, see_through_walls: bool = True,
                 channels: Sequence[str] | None = None,
                 eye_count: int = 1, eye_angle_degrees: float = 60.0,
                 eye_separation_degrees: float = 45.0):
        """
        Args:
            retina_count: Número de raios da retina
            vision_radius: Alcance máximo da visão
            fov_degrees: Campo de visão em graus (ex: 180.0 para meio círculo)
            skip: Quantos frames pular entre atualizações (otimização)
            see_food: Se deve detectar comida
            see_bacteria: Se deve detectar outras bactérias
            see_predators: Se deve detectar predadores
        """
        self.retina_count = retina_count
        self.vision_radius = vision_radius
        self.fov_degrees = fov_degrees
        self.skip = skip
        self.see_food = see_food
        self.see_bacteria = see_bacteria
        self.see_predators = see_predators
        self.see_obstacles = bool(see_obstacles)
        self.see_all = bool(see_all)
        self.see_through_walls = bool(see_through_walls)
        self.channels = normalize_retina_channels(channels)
        self.eye_count = normalize_eye_count(eye_count)
        self.eye_angle_degrees = float(eye_angle_degrees)
        self.eye_separation_degrees = float(eye_separation_degrees)
        
        # Estado interno
        self._countdown = 0
        self.last_inputs: List[float] = []
        self.last_distance_inputs: List[float] = []

    def total_ray_count(self) -> int:
        return max(1, int(self.retina_count)) * normalize_eye_count(getattr(self, "eye_count", 1))

    def _eye_specs(self) -> list[tuple[float, float]]:
        if normalize_eye_count(getattr(self, "eye_count", 1)) <= 1:
            return [(0.0, 0.0)]
        gaze_half = math.radians(float(getattr(self, "eye_angle_degrees", 60.0)) * 0.5)
        pos_half = math.radians(float(getattr(self, "eye_separation_degrees", 45.0)) * 0.5)
        return [(-pos_half, -gaze_half), (pos_half, gaze_half)]

    def _ray_pose(self, agent: 'Agent', ray_index: int) -> tuple[float, float, float] | None:
        count = max(1, int(self.retina_count))
        total = self.total_ray_count()
        if ray_index < 0 or ray_index >= total:
            return None
        eye_index = ray_index // count
        local_index = ray_index % count
        specs = self._eye_specs()
        pos_offset, gaze_offset = specs[min(eye_index, len(specs) - 1)]
        position_angle = agent.angle + pos_offset
        eye_x = agent.x + math.cos(position_angle) * agent.r
        eye_y = agent.y + math.sin(position_angle) * agent.r
        half_fov = math.radians(self.fov_degrees / 2.0)
        if count > 1:
            rel_angle = (-half_fov) + (local_index / (count - 1)) * (2 * half_fov)
        else:
            rel_angle = 0.0
        return eye_x, eye_y, agent.angle + gaze_offset + rel_angle
    
    def sense(self, agent: 'Agent', scene: SceneQuery, params: 'Params') -> List[float]:
        """
        Executa sensoriamento da retina.
        
        Args:
            agent: Agente que está sensoriando
            scene: Serviço de consulta de cena
            params: Parâmetros da simulação
            
        Returns:
            Lista de valores de ativação da retina [0..1]
        """
        # Atualização dinâmica dos parâmetros a cada frame (permite alterar na UI em tempo real)
        prefix = 'predator' if getattr(agent, 'is_predator', False) else 'bacteria'
        desired_count = self.retina_count
        desired_fov = self.fov_degrees
        desired_radius = self.vision_radius
        desired_skip = max(0, int(params.get('retina_skip', self.skip)))
        desired_see_food = self.see_food
        desired_see_bacteria = self.see_bacteria
        desired_see_predators = self.see_predators
        desired_see_obstacles = bool(getattr(self, "see_obstacles", False))
        desired_see_all = bool(getattr(self, "see_all", False))
        desired_see_through_walls = bool(getattr(self, "see_through_walls", True))
        desired_channels = normalize_retina_channels(self.channels)
        desired_eye_count = normalize_eye_count(getattr(self, "eye_count", 1))
        desired_eye_angle = float(getattr(self, "eye_angle_degrees", 60.0))
        desired_eye_separation = float(getattr(self, "eye_separation_degrees", 45.0))
        if (desired_count != self.retina_count or desired_fov != self.fov_degrees or
            desired_radius != self.vision_radius or desired_skip != self.skip or
            desired_see_food != self.see_food or
            desired_see_bacteria != self.see_bacteria or desired_see_predators != self.see_predators or
            desired_see_obstacles != bool(getattr(self, "see_obstacles", False)) or
            desired_see_all != bool(getattr(self, "see_all", False)) or
            desired_see_through_walls != bool(getattr(self, "see_through_walls", True)) or
            desired_channels != self.channels or desired_eye_count != normalize_eye_count(getattr(self, "eye_count", 1)) or
            desired_eye_angle != float(getattr(self, "eye_angle_degrees", 60.0)) or
            desired_eye_separation != float(getattr(self, "eye_separation_degrees", 45.0))):
            self.retina_count = max(1, int(desired_count))
            self.fov_degrees = float(desired_fov)
            self.vision_radius = float(desired_radius)
            self.skip = desired_skip
            self.see_food = bool(desired_see_food)
            self.see_bacteria = bool(desired_see_bacteria)
            self.see_predators = bool(desired_see_predators)
            self.see_obstacles = bool(desired_see_obstacles)
            self.see_all = bool(desired_see_all)
            self.see_through_walls = bool(desired_see_through_walls)
            self.channels = desired_channels
            self.eye_count = desired_eye_count
            self.eye_angle_degrees = desired_eye_angle
            self.eye_separation_degrees = desired_eye_separation
            # Força recálculo completo
            self.last_inputs = []
            self.last_distance_inputs = []
            self._countdown = 0

        # Sistema de skip para otimização (após possível atualização dinâmica)
        if self._countdown > 0:
            self._countdown -= 1
            if self.last_inputs:
                return list(self.last_inputs)
        
        # Calcula posição do "olho" (frente do agente)
        type_codes = []
        max_seen_radius = 0.0
        see_all = bool(getattr(self, "see_all", False))
        see_obstacles = bool(getattr(self, "see_obstacles", False)) or see_all
        see_through_walls = bool(getattr(self, "see_through_walls", True))
        if self.see_food or see_all:
            type_codes.append(0)
            max_seen_radius = max(max_seen_radius, float(params.get('food_max_r', 5.0)))
        if self.see_bacteria or see_all:
            type_codes.append(1)
            max_seen_radius = max(max_seen_radius, float(params.get('bacteria_body_size', 9.0)))
        if self.see_predators or see_all:
            type_codes.append(2)
            max_seen_radius = max(max_seen_radius, float(params.get('predator_body_size', 14.0)))

        if scene.spatial_hash is not None and type_codes:
            search_r = float(self.vision_radius) + float(getattr(agent, 'r', 0.0)) + max_seen_radius
            candidates = scene.spatial_hash.query_ball_filtered(agent.x, agent.y, search_r, tuple(type_codes))
        else:
            candidates = []
            if self.see_food or see_all:
                candidates.extend(scene.entities.get('foods', []))
            if self.see_bacteria or see_all:
                candidates.extend(scene.entities.get('bacteria', []))
            if self.see_predators or see_all:
                candidates.extend(scene.entities.get('predators', []))

        obstacle_candidates = []
        obstacles = getattr(scene, "obstacles", None)
        if (see_obstacles or not see_through_walls) and getattr(obstacles, "has_obstacles", False):
            obstacle_radius = float(self.vision_radius) + float(getattr(agent, 'r', 0.0)) + max(1.0, float(params.get('food_max_r', 5.0)))
            obstacle_candidates = [obstacles.stamps[i] for i in obstacles.query_indices(agent.x, agent.y, obstacle_radius)]
            if see_obstacles:
                if not isinstance(candidates, list):
                    candidates = list(candidates)
                candidates.extend(obstacle_candidates)
                if 3 not in type_codes:
                    type_codes.append(3)
        
        if normalize_retina_vision_mode(params.get('retina_vision_mode', RETINA_VISION_MODE_SINGLE)) == RETINA_VISION_MODE_SECTOR and see_through_walls:
            inputs, distance_inputs = _sector_retina_from_candidates(
                self,
                agent,
                candidates,
                type_codes=tuple(type_codes),
                spatial_filtered=scene.spatial_hash is not None,
            )
            self.last_inputs = list(inputs)
            self.last_distance_inputs = list(distance_inputs)
            self._countdown = self.skip
            return inputs

        inputs = []
        distance_inputs = []
        for i in range(self.total_ray_count()):
            # Calcula ângulo relativo do raio
            pose = self._ray_pose(agent, i)
            if pose is None:
                continue
            eye_x, eye_y, ray_angle = pose
            ray_dx = math.cos(ray_angle)
            ray_dy = math.sin(ray_angle)
            
            # Faz raycast
            hit = _raycast_hit_from_candidates(
                eye_x, eye_y, ray_dx, ray_dy, self.vision_radius, candidates, ignore=agent
            )
            if not see_through_walls and obstacle_candidates:
                wall_hit = _raycast_hit_from_candidates(
                    eye_x, eye_y, ray_dx, ray_dy, self.vision_radius, obstacle_candidates, ignore=None
                )
                if wall_hit is not None and (hit is None or wall_hit[0] < hit[0]):
                    hit = wall_hit if see_obstacles else None
            
            # Converte distância para ativação [0..1]
            if hit is None:
                activation = 0.0
                color = (0.0, 0.0, 0.0)
            else:
                distance, obj = hit
                # Ativação inversamente proporcional à distância
                activation = max(0.0, min(1.0, (self.vision_radius - distance) / self.vision_radius))
                color = _object_color01(obj)
            distance_inputs.append(float(activation))
            
            for channel in self.channels:
                if channel == "r":
                    inputs.append(color[0])
                elif channel == "g":
                    inputs.append(color[1])
                elif channel == "b":
                    inputs.append(color[2])
                elif channel == "rd":
                    inputs.append(activation * color[0])
                elif channel == "gd":
                    inputs.append(activation * color[1])
                elif channel == "bd":
                    inputs.append(activation * color[2])
                else:
                    inputs.append(activation)
        
        # Atualiza estado e countdown
        self.last_inputs = list(inputs)
        self.last_distance_inputs = list(distance_inputs)
        self._countdown = self.skip
        
        return inputs

    def get_ray_info(self, agent: 'Agent', ray_index: int) -> tuple:
        """Retorna (start_x, start_y, end_x, end_y, activation) para um raio.
        Se índice inválido ou sem dados ainda, retorna None.
        """
        if ray_index < 0 or ray_index >= self.total_ray_count() or not self.last_inputs:
            return None
        pose = self._ray_pose(agent, ray_index)
        if pose is None:
            return None
        eye_x, eye_y, ray_angle = pose
        stride = max(1, len(getattr(self, "channels", ("d",))))
        start = ray_index * stride
        values = self.last_inputs[start:start + stride]
        distance_values = getattr(self, "last_distance_inputs", None)
        if distance_values and ray_index < len(distance_values):
            activation = float(distance_values[ray_index])
        elif not values:
            activation = 0.0
        elif "d" in self.channels:
            activation = values[self.channels.index("d")]
        else:
            activation = max(values)
        shown_length = (1.0 - activation) * self.vision_radius
        end_x = eye_x + math.cos(ray_angle) * shown_length
        end_y = eye_y + math.sin(ray_angle) * shown_length
        return (eye_x, eye_y, end_x, end_y, activation)


# Batch semantics are selected by params['retina_vision_mode']:
# - single preserves the historical fast centroid approximation.
# - fullbody is the stricter geometric ray/body intersection mode.
# - sector uses fast angular sectors and supports color channels.
def batch_retina_sense(agents: Sequence['Agent'], scene: SceneQuery, params: 'Params') -> List[List[float]]:
    """Processa percepção (retina) em lote para vários agentes que usam RetinaSensor.
    Combina pré-filtragem por tipo e operações numpy para reduzir custo de loops Python.
    Respeita skip individual e atualizações dinâmicas de parâmetros.
    Retorna lista de listas (inputs por agente) na mesma ordem de entrada.
    """
    if not agents:
        return []
    # Coleta sensores e verifica tipo
    sensors = [a.sensor for a in agents]
    _RS = RetinaSensor  # referenciar a classe local diretamente
    if not all(isinstance(s, _RS) for s in sensors):  # fallback se algum não for retina
        return [a.sensor.sense(a, scene, params) for a in agents]
    if any(
        bool(getattr(s, "see_obstacles", False)) or
        bool(getattr(s, "see_all", False)) or
        not bool(getattr(s, "see_through_walls", True))
        for s in sensors
    ):
        return [a.sensor.sense(a, scene, params) for a in agents]

    # Atualiza parâmetros dinâmicos e determina quais precisam recalcular
    param_get = params.get if params is not None else (lambda _key, default=None: default)
    vision_mode = normalize_retina_vision_mode(param_get('retina_vision_mode') if params is not None else None)
    sensor_configs = {}
    fast_retina_single = None
    fast_retina_fullbody = None
    fast_retina_fullbody_precomputed = None
    fast_retina_batch_single = None
    fast_retina_batch_fullbody = None
    fast_retina_batch_sector = None
    fast_retina_batch_sector_distance = None
    if bool(param_get('use_numba_kernels', True)):
        try:
            from .fast_kernels import (
                has_numba,
                retina_batch_fullbody_kernel,
                retina_batch_sector_distance_kernel,
                retina_batch_sector_kernel,
                retina_batch_single_kernel,
                retina_fullbody_precomputed_kernel,
                retina_fullbody_kernel,
                retina_single_kernel,
            )
            if has_numba():
                fast_retina_single = retina_single_kernel
                fast_retina_fullbody = retina_fullbody_kernel
                fast_retina_fullbody_precomputed = retina_fullbody_precomputed_kernel
                fast_retina_batch_single = retina_batch_single_kernel
                fast_retina_batch_fullbody = retina_batch_fullbody_kernel
                fast_retina_batch_sector = retina_batch_sector_kernel
                fast_retina_batch_sector_distance = retina_batch_sector_distance_kernel
        except Exception:
            fast_retina_single = None
            fast_retina_fullbody = None
            fast_retina_fullbody_precomputed = None
            fast_retina_batch_single = None
            fast_retina_batch_fullbody = None
            fast_retina_batch_sector = None
            fast_retina_batch_sector_distance = None

    def _species_sensor_config(prefix, sensor):
        desired_skip = max(0, int(param_get('retina_skip', sensor.skip)))
        desired_channels = normalize_retina_channels(getattr(sensor, "channels", ("d",)))
        cache_key = (
            prefix,
            int(getattr(sensor, "retina_count", 18)),
            round(float(getattr(sensor, "fov_degrees", 180.0)), 6),
            round(float(getattr(sensor, "vision_radius", 120.0)), 6),
            desired_skip,
            bool(getattr(sensor, "see_food", True)),
            bool(getattr(sensor, "see_bacteria", False)),
            bool(getattr(sensor, "see_predators", False)),
            desired_channels,
            normalize_eye_count(getattr(sensor, "eye_count", 1)),
            round(float(getattr(sensor, "eye_angle_degrees", 60.0)), 6),
            round(float(getattr(sensor, "eye_separation_degrees", 45.0)), 6),
        )
        config = sensor_configs.get(cache_key)
        if config is not None:
            return config
        desired_count = max(1, int(getattr(sensor, "retina_count", 18)))
        desired_fov = float(getattr(sensor, "fov_degrees", 180.0))
        desired_radius = float(getattr(sensor, "vision_radius", 120.0))
        desired_see_food = bool(getattr(sensor, "see_food", True))
        desired_see_bacteria = bool(getattr(sensor, "see_bacteria", False))
        desired_see_predators = bool(getattr(sensor, "see_predators", False))
        desired_eye_count = normalize_eye_count(getattr(sensor, "eye_count", 1))
        desired_eye_angle = float(getattr(sensor, "eye_angle_degrees", 60.0))
        desired_eye_separation = float(getattr(sensor, "eye_separation_degrees", 45.0))
        type_codes = []
        max_seen_radius = 0.0
        if desired_see_food:
            type_codes.append(0)
            max_seen_radius = max(max_seen_radius, float(param_get('food_max_r', 5.0)))
        if desired_see_bacteria:
            type_codes.append(1)
            max_seen_radius = max(max_seen_radius, float(param_get('bacteria_body_size', 9.0)))
        if desired_see_predators:
            type_codes.append(2)
            max_seen_radius = max(max_seen_radius, float(param_get('predator_body_size', 14.0)))
        config = (
            desired_count,
            desired_fov,
            desired_radius,
            desired_skip,
            desired_see_food,
            desired_see_bacteria,
            desired_see_predators,
            desired_channels,
            desired_eye_count,
            desired_eye_angle,
            desired_eye_separation,
            tuple(type_codes),
            max_seen_radius,
        )
        sensor_configs[cache_key] = config
        return config

    need_update_idx = []
    results: List[List[float]] = [None] * len(agents)  # type: ignore
    runtime_configs = [None] * len(agents)
    for idx, (agent, sensor) in enumerate(zip(agents, sensors)):
        prefix = 'predator' if getattr(agent, 'is_predator', False) else 'bacteria'
        config = _species_sensor_config(prefix, sensor)
        runtime_configs[idx] = config
        (
            desired_count,
            desired_fov,
            desired_radius,
            desired_skip,
            desired_see_food,
            desired_see_bacteria,
            desired_see_predators,
            desired_channels,
            desired_eye_count,
            desired_eye_angle,
            desired_eye_separation,
            _type_codes,
            _max_seen_radius,
        ) = config
        if (desired_count != sensor.retina_count or desired_fov != sensor.fov_degrees or
            desired_radius != sensor.vision_radius or desired_skip != sensor.skip or
            desired_see_food != sensor.see_food or
            desired_see_bacteria != sensor.see_bacteria or desired_see_predators != sensor.see_predators or
            desired_channels != getattr(sensor, "channels", ("d",)) or
            desired_eye_count != normalize_eye_count(getattr(sensor, "eye_count", 1)) or
            desired_eye_angle != float(getattr(sensor, "eye_angle_degrees", 60.0)) or
            desired_eye_separation != float(getattr(sensor, "eye_separation_degrees", 45.0))):
            sensor.retina_count = desired_count
            sensor.fov_degrees = desired_fov
            sensor.vision_radius = desired_radius
            sensor.skip = desired_skip
            sensor.see_food = desired_see_food
            sensor.see_bacteria = desired_see_bacteria
            sensor.see_predators = desired_see_predators
            sensor.channels = desired_channels
            sensor.eye_count = desired_eye_count
            sensor.eye_angle_degrees = desired_eye_angle
            sensor.eye_separation_degrees = desired_eye_separation
            sensor.last_inputs = []
            sensor.last_distance_inputs = []
            sensor._countdown = 0
        if sensor._countdown > 0 and sensor.last_inputs:
            sensor._countdown -= 1
            results[idx] = list(sensor.last_inputs)
        else:
            need_update_idx.append(idx)

    if not need_update_idx:
        return results  # type: ignore

    def _empty_retina_inputs(sensor):
        rays = sensor.total_ray_count() if hasattr(sensor, "total_ray_count") else int(sensor.retina_count)
        return [0.0] * (int(rays) * max(1, len(getattr(sensor, "channels", ("d",)))))

    def _store_retina_inputs(sensor, inputs, distance_inputs=None):
        sensor.last_inputs = list(inputs)
        if distance_inputs is None:
            rays = sensor.total_ray_count() if hasattr(sensor, "total_ray_count") else int(sensor.retina_count)
            if len(inputs) == int(rays):
                distance_inputs = inputs
            else:
                distance_inputs = [0.0] * int(rays)
        sensor.last_distance_inputs = list(distance_inputs)

    # Preparação para fallback sem spatial hash (global candidates)
    global_candidates = None
    if scene.spatial_hash is None:
        any_food = any(sensors[i].see_food for i in need_update_idx)
        any_bact = any(sensors[i].see_bacteria for i in need_update_idx)
        any_pred = any(sensors[i].see_predators for i in need_update_idx)
        cand_list = []
        if any_food:
            cand_list.extend(scene.entities.get('foods', []))
        if any_bact:
            cand_list.extend(scene.entities.get('bacteria', []))
        if any_pred:
            cand_list.extend(scene.entities.get('predators', []))
        global_candidates = cand_list
        if not global_candidates:
            for idx in need_update_idx:
                sensor = sensors[idx]
                inputs = _empty_retina_inputs(sensor)
                _store_retina_inputs(sensor, inputs)
                sensor._countdown = sensor.skip
                results[idx] = inputs
            return results  # type: ignore

    if vision_mode == RETINA_VISION_MODE_SECTOR:
        remaining_sector_idx = []
        if scene.spatial_hash is not None and fast_retina_batch_sector is not None:
            groups = {}
            for idx in need_update_idx:
                sensor = sensors[idx]
                key = (
                    int(sensor.retina_count),
                    normalize_eye_count(getattr(sensor, "eye_count", 1)),
                    normalize_retina_channels(getattr(sensor, "channels", ("d",))),
                )
                groups.setdefault(key, []).append(idx)

            candidate_buffer = set()
            for (retina_count, eye_count, channels), group_indices in groups.items():
                n_update = len(group_indices)
                n_eye_rows = n_update * eye_count
                channel_count = max(1, len(channels))
                eye_x_arr = np.empty(n_eye_rows, dtype=np.float64)
                eye_y_arr = np.empty(n_eye_rows, dtype=np.float64)
                angle_arr = np.empty(n_eye_rows, dtype=np.float64)
                vision_radius_arr = np.empty(n_eye_rows, dtype=np.float64)
                half_fov_arr = np.empty(n_eye_rows, dtype=np.float64)
                cand_start = np.empty(n_eye_rows, dtype=np.int64)
                cand_count = np.empty(n_eye_rows, dtype=np.int64)
                cand_x_values = []
                cand_y_values = []
                cand_r_values = []
                needs_color = any(ch != "d" for ch in channels)
                cand_color_r_values = [] if needs_color else None
                cand_color_g_values = [] if needs_color else None
                cand_color_b_values = [] if needs_color else None
                self_values = []
                total_candidates = 0

                for local_idx, idx in enumerate(group_indices):
                    agent = agents[idx]
                    sensor = sensors[idx]
                    (
                        _desired_count,
                        _desired_fov,
                        _desired_radius,
                        _desired_skip,
                        _desired_see_food,
                        _desired_see_bacteria,
                        _desired_see_predators,
                        _desired_channels,
                        _desired_eye_count,
                        _desired_eye_angle,
                        _desired_eye_separation,
                        type_codes,
                        max_seen_radius,
                    ) = runtime_configs[idx]
                    specs = sensor._eye_specs() if hasattr(sensor, "_eye_specs") else [(0.0, 0.0)]
                    for eye_idx in range(eye_count):
                        row = local_idx * eye_count + eye_idx
                        pos_offset, gaze_offset = specs[min(eye_idx, len(specs) - 1)]
                        position_angle = agent.angle + pos_offset
                        eye_x = agent.x + math.cos(position_angle) * agent.r
                        eye_y = agent.y + math.sin(position_angle) * agent.r
                        search_r = sensor.vision_radius + max_seen_radius
                        candidates_local = (
                            scene.spatial_hash.query_ball_filtered_into(eye_x, eye_y, search_r, type_codes, candidate_buffer)
                            if type_codes else ()
                        )
                        eye_x_arr[row] = float(eye_x)
                        eye_y_arr[row] = float(eye_y)
                        angle_arr[row] = float(agent.angle + gaze_offset)
                        vision_radius_arr[row] = float(sensor.vision_radius)
                        half_fov_arr[row] = math.radians(sensor.fov_degrees / 2.0)
                        cand_start[row] = total_candidates
                        count = 0
                        for candidate in candidates_local:
                            cand_x_values.append(float(candidate.x))
                            cand_y_values.append(float(candidate.y))
                            cand_r_values.append(float(getattr(candidate, "r", 0.0)))
                            if needs_color:
                                cr, cg, cb = _object_color01(candidate)
                                cand_color_r_values.append(cr)
                                cand_color_g_values.append(cg)
                                cand_color_b_values.append(cb)
                            self_values.append(candidate is agent)
                            count += 1
                        cand_count[row] = count
                        total_candidates += count

                if total_candidates == 0:
                    for idx in group_indices:
                        sensor = sensors[idx]
                        inputs = _empty_retina_inputs(sensor)
                        _store_retina_inputs(sensor, inputs)
                        sensor._countdown = sensor.skip
                        results[idx] = inputs
                    continue

                cand_x_arr = np.asarray(cand_x_values, dtype=np.float64)
                cand_y_arr = np.asarray(cand_y_values, dtype=np.float64)
                cand_r_arr = np.asarray(cand_r_values, dtype=np.float64)
                self_arr = np.asarray(self_values, dtype=bool)
                if channels == ("d",) and fast_retina_batch_sector_distance is not None:
                    out = np.empty((n_eye_rows, retina_count), dtype=np.float64)
                    ok = fast_retina_batch_sector_distance(
                        eye_x_arr,
                        eye_y_arr,
                        angle_arr,
                        vision_radius_arr,
                        half_fov_arr,
                        cand_start,
                        cand_count,
                        cand_x_arr,
                        cand_y_arr,
                        cand_r_arr,
                        self_arr,
                        int(retina_count),
                        out,
                    )
                    if ok:
                        out32 = out.astype(np.float32)
                        for local_idx, idx in enumerate(group_indices):
                            sensor = sensors[idx]
                            start_row = local_idx * eye_count
                            inputs = out32[start_row:start_row + eye_count].reshape(-1).tolist()
                            _store_retina_inputs(sensor, inputs, inputs)
                            sensor._countdown = sensor.skip
                            results[idx] = inputs
                        continue
                    remaining_sector_idx.extend(group_indices)
                    continue
                cand_color_r_arr = np.asarray(cand_color_r_values or [], dtype=np.float64)
                cand_color_g_arr = np.asarray(cand_color_g_values or [], dtype=np.float64)
                cand_color_b_arr = np.asarray(cand_color_b_values or [], dtype=np.float64)
                channel_codes = _retina_channel_codes(channels)
                out = np.empty((n_eye_rows, retina_count, channel_count), dtype=np.float64)
                distance_out = np.empty((n_eye_rows, retina_count), dtype=np.float64)
                ok = fast_retina_batch_sector(
                    eye_x_arr,
                    eye_y_arr,
                    angle_arr,
                    vision_radius_arr,
                    half_fov_arr,
                    cand_start,
                    cand_count,
                    cand_x_arr,
                    cand_y_arr,
                    cand_r_arr,
                    self_arr,
                    cand_color_r_arr,
                    cand_color_g_arr,
                    cand_color_b_arr,
                    channel_codes,
                    int(retina_count),
                    out,
                    distance_out,
                )
                if not ok:
                    remaining_sector_idx.extend(group_indices)
                    continue
                out32 = out.astype(np.float32)
                distance32 = distance_out.astype(np.float32)
                for local_idx, idx in enumerate(group_indices):
                    sensor = sensors[idx]
                    start_row = local_idx * eye_count
                    inputs = out32[start_row:start_row + eye_count].reshape(-1).tolist()
                    distances = distance32[start_row:start_row + eye_count].reshape(-1).tolist()
                    _store_retina_inputs(sensor, inputs, distances)
                    sensor._countdown = sensor.skip
                    results[idx] = inputs
        else:
            remaining_sector_idx = list(need_update_idx)

        candidate_buffer = set()
        for idx in remaining_sector_idx:
            agent = agents[idx]
            sensor = sensors[idx]
            (
                _desired_count,
                _desired_fov,
                _desired_radius,
                _desired_skip,
                _desired_see_food,
                _desired_see_bacteria,
                _desired_see_predators,
                _desired_channels,
                _desired_eye_count,
                _desired_eye_angle,
                _desired_eye_separation,
                type_codes,
                max_seen_radius,
            ) = runtime_configs[idx]
            if scene.spatial_hash is not None:
                search_r = sensor.vision_radius + float(getattr(agent, "r", 0.0)) + max_seen_radius
                candidates_local = (
                    scene.spatial_hash.query_ball_filtered_into(agent.x, agent.y, search_r, type_codes, candidate_buffer)
                    if type_codes else ()
                )
                spatial_filtered = True
            else:
                candidates_local = global_candidates
                spatial_filtered = False
            if not candidates_local:
                inputs = _empty_retina_inputs(sensor)
                _store_retina_inputs(sensor, inputs)
            else:
                inputs, distance_inputs = _sector_retina_from_candidates(
                    sensor,
                    agent,
                    candidates_local,
                    type_codes=type_codes,
                    spatial_filtered=spatial_filtered,
                )
                _store_retina_inputs(sensor, inputs, distance_inputs)
            sensor._countdown = sensor.skip
            results[idx] = inputs
        return results  # type: ignore

    multi_eye_batch_candidate = any(
        normalize_eye_count(getattr(sensors[i], "eye_count", 1)) > 1 for i in need_update_idx
    )
    batch_retina_enabled = (
        (bool(param_get('use_numba_batch_retina', True)) or multi_eye_batch_candidate)
        and scene.spatial_hash is not None
        and sum(normalize_eye_count(getattr(sensors[i], "eye_count", 1)) for i in need_update_idx) >= 8
        and all(getattr(sensors[i], "channels", ("d",)) == ("d",) for i in need_update_idx)
        and (
            (vision_mode == 'fullbody' and fast_retina_batch_fullbody is not None)
            or ((vision_mode == 'single' or sensors[need_update_idx[0]].retina_count == 1) and fast_retina_batch_single is not None)
        )
    )
    if batch_retina_enabled:
        retina_counts = {int(sensors[i].retina_count) for i in need_update_idx}
        eye_counts = {normalize_eye_count(getattr(sensors[i], "eye_count", 1)) for i in need_update_idx}
        if len(retina_counts) == 1 and len(eye_counts) == 1:
            retina_count = int(next(iter(retina_counts)))
            eye_count = int(next(iter(eye_counts)))
            n_update = len(need_update_idx)
            n_eye_rows = n_update * eye_count
            eye_x_arr = np.empty(n_eye_rows, dtype=np.float64)
            eye_y_arr = np.empty(n_eye_rows, dtype=np.float64)
            angle_arr = np.empty(n_eye_rows, dtype=np.float64)
            vision_radius_arr = np.empty(n_eye_rows, dtype=np.float64)
            half_fov_arr = np.empty(n_eye_rows, dtype=np.float64)
            cand_start = np.empty(n_eye_rows, dtype=np.int64)
            cand_count = np.empty(n_eye_rows, dtype=np.int64)
            cand_x_values = []
            cand_y_values = []
            cand_r_values = []
            self_values = []
            candidate_buffer = set()
            total_candidates = 0
            first_sensor = sensors[need_update_idx[0]]
            for local_idx, idx in enumerate(need_update_idx):
                agent = agents[idx]
                sensor = sensors[idx]
                (
                    _desired_count,
                    _desired_fov,
                    _desired_radius,
                    _desired_skip,
                    _desired_see_food,
                    _desired_see_bacteria,
                    _desired_see_predators,
                    _desired_channels,
                    _desired_eye_count,
                    _desired_eye_angle,
                    _desired_eye_separation,
                    type_codes,
                    max_seen_radius,
                ) = runtime_configs[idx]
                specs = sensor._eye_specs() if hasattr(sensor, "_eye_specs") else [(0.0, 0.0)]
                for eye_idx in range(eye_count):
                    row = local_idx * eye_count + eye_idx
                    pos_offset, gaze_offset = specs[min(eye_idx, len(specs) - 1)]
                    position_angle = agent.angle + pos_offset
                    eye_x = agent.x + math.cos(position_angle) * agent.r
                    eye_y = agent.y + math.sin(position_angle) * agent.r
                    search_r = sensor.vision_radius + max_seen_radius
                    candidates_local = (
                        scene.spatial_hash.query_ball_filtered_into(eye_x, eye_y, search_r, type_codes, candidate_buffer)
                        if type_codes else ()
                    )
                    eye_x_arr[row] = float(eye_x)
                    eye_y_arr[row] = float(eye_y)
                    angle_arr[row] = float(agent.angle + gaze_offset)
                    vision_radius_arr[row] = float(sensor.vision_radius)
                    half_fov_arr[row] = math.radians(sensor.fov_degrees / 2.0)
                    cand_start[row] = total_candidates
                    count = 0
                    for candidate in candidates_local:
                        cand_x_values.append(float(candidate.x))
                        cand_y_values.append(float(candidate.y))
                        cand_r_values.append(float(getattr(candidate, 'r', 0.0)))
                        self_values.append(candidate is agent)
                        count += 1
                    cand_count[row] = count
                    total_candidates += count

            if total_candidates == 0:
                for idx in need_update_idx:
                    sensor = sensors[idx]
                    inputs = _empty_retina_inputs(sensor)
                    _store_retina_inputs(sensor, inputs)
                    sensor._countdown = sensor.skip
                    results[idx] = inputs
                return results  # type: ignore

            cand_x_arr = np.asarray(cand_x_values, dtype=np.float64)
            cand_y_arr = np.asarray(cand_y_values, dtype=np.float64)
            cand_r_arr = np.asarray(cand_r_values, dtype=np.float64)
            cand_type_arr = np.zeros(total_candidates, dtype=np.int8)
            self_arr = np.asarray(self_values, dtype=bool)
            out = np.empty((n_eye_rows, retina_count), dtype=np.float64)
            if vision_mode == 'fullbody' and fast_retina_batch_fullbody is not None:
                ok = fast_retina_batch_fullbody(
                    eye_x_arr,
                    eye_y_arr,
                    angle_arr,
                    vision_radius_arr,
                    half_fov_arr,
                    cand_start,
                    cand_count,
                    cand_x_arr,
                    cand_y_arr,
                    cand_r_arr,
                    cand_type_arr,
                    self_arr,
                    retina_count,
                    bool(first_sensor.see_food),
                    bool(first_sensor.see_bacteria),
                    bool(first_sensor.see_predators),
                    True,
                    out,
                )
            elif fast_retina_batch_single is not None:
                ok = fast_retina_batch_single(
                    eye_x_arr,
                    eye_y_arr,
                    angle_arr,
                    vision_radius_arr,
                    half_fov_arr,
                    cand_start,
                    cand_count,
                    cand_x_arr,
                    cand_y_arr,
                    cand_r_arr,
                    cand_type_arr,
                    self_arr,
                    retina_count,
                    bool(first_sensor.see_food),
                    bool(first_sensor.see_bacteria),
                    bool(first_sensor.see_predators),
                    True,
                    out,
                )
            else:
                ok = False
            if ok:
                out32 = out.astype(np.float32)
                for local_idx, idx in enumerate(need_update_idx):
                    sensor = sensors[idx]
                    if eye_count == 1:
                        inputs = out32[local_idx].tolist()
                    else:
                        start_row = local_idx * eye_count
                        inputs = out32[start_row:start_row + eye_count].reshape(-1).tolist()
                    _store_retina_inputs(sensor, inputs)
                    sensor._countdown = sensor.skip
                    results[idx] = inputs
                return results  # type: ignore

    def angle_wrap(a):
        return (a + np.pi) % (2 * np.pi) - np.pi

    candidate_buffer = set()
    for idx in need_update_idx:
        agent = agents[idx]
        sensor = sensors[idx]
        (
            _desired_count,
            _desired_fov,
            _desired_radius,
            _desired_skip,
            _desired_see_food,
            _desired_see_bacteria,
            _desired_see_predators,
            _desired_channels,
            _desired_eye_count,
            _desired_eye_angle,
            _desired_eye_separation,
            type_codes,
            max_seen_radius,
        ) = runtime_configs[idx]

        # Seleciona candidatos locais usando spatial hash quando disponível
        if normalize_eye_count(getattr(sensor, "eye_count", 1)) != 1:
            inputs = sensor.sense(agent, scene, params)
            results[idx] = inputs
            continue

        if scene.spatial_hash is not None:
            # Raio de busca: visao + maior raio entre os tipos relevantes.
            search_r = sensor.vision_radius + max_seen_radius
            eye_x = agent.x + math.cos(agent.angle) * agent.r
            eye_y = agent.y + math.sin(agent.angle) * agent.r
            candidates_local = (
                scene.spatial_hash.query_ball_filtered_into(eye_x, eye_y, search_r, type_codes, candidate_buffer)
                if type_codes else ()
            )
        else:
            candidates_local = global_candidates
            eye_x = agent.x + math.cos(agent.angle) * agent.r
            eye_y = agent.y + math.sin(agent.angle) * agent.r
        if not candidates_local:
            inputs = _empty_retina_inputs(sensor)
            _store_retina_inputs(sensor, inputs)
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue

        # Máscara de auto-interseção (evita ver a si mesmo)
        xs = []
        ys = []
        rs = []
        self_flags = []
        type_flags = [] if scene.spatial_hash is None else None
        use_color_channels = getattr(sensor, "channels", ("d",)) != ("d",)
        colors = [] if use_color_channels else None
        for candidate in candidates_local:
            xs.append(candidate.x)
            ys.append(candidate.y)
            rs.append(getattr(candidate, 'r', 0.0))
            self_flags.append(candidate is agent)
            if colors is not None:
                colors.append(_object_color01(candidate))
            if type_flags is not None:
                type_flags.append(getattr(candidate, 'type_code', -1))
        is_self = np.array(self_flags, dtype=bool)
        fast_retina_enabled = fast_retina_single is not None or fast_retina_fullbody is not None
        candidate_dtype = np.float64 if fast_retina_enabled else np.float32
        cand_x = np.array(xs, dtype=candidate_dtype)
        cand_y = np.array(ys, dtype=candidate_dtype)
        cand_r = np.array(rs, dtype=candidate_dtype)

        if not use_color_channels and fast_retina_single is not None and (vision_mode == 'single' or sensor.retina_count == 1):
            half_fov_fast = math.radians(sensor.fov_degrees / 2.0)
            if half_fov_fast > 0:
                if type_flags is None:
                    cand_type_fast = np.zeros(cand_x.shape[0], dtype=np.int8)
                    spatial_filtered = True
                else:
                    cand_type_fast = np.array(type_flags, dtype=np.int8)
                    spatial_filtered = False
                out = np.empty((sensor.retina_count,), dtype=np.float64)
                ok = fast_retina_single(
                    cand_x,
                    cand_y,
                    cand_r,
                    cand_type_fast,
                    is_self,
                    float(eye_x),
                    float(eye_y),
                    float(agent.angle),
                    float(sensor.vision_radius),
                    float(half_fov_fast),
                    int(sensor.retina_count),
                    bool(sensor.see_food),
                    bool(sensor.see_bacteria),
                    bool(sensor.see_predators),
                    bool(spatial_filtered),
                    out,
                )
                if ok:
                    inputs = out.astype(np.float32).tolist()
                    _store_retina_inputs(sensor, inputs)
                    sensor._countdown = sensor.skip
                    results[idx] = inputs
                    continue
        elif not use_color_channels and (fast_retina_fullbody_precomputed is not None or fast_retina_fullbody is not None) and vision_mode == 'fullbody':
            half_fov_fast = math.radians(sensor.fov_degrees / 2.0)
            if half_fov_fast > 0:
                if type_flags is None:
                    cand_type_fast = np.zeros(cand_x.shape[0], dtype=np.int8)
                    spatial_filtered = True
                else:
                    cand_type_fast = np.array(type_flags, dtype=np.int8)
                    spatial_filtered = False
                out = np.empty((sensor.retina_count,), dtype=np.float64)
                if fast_retina_fullbody_precomputed is not None:
                    _ray_rel, cos_rel, sin_rel = _get_retina_relative_rays(sensor.retina_count, half_fov_fast)
                    ok = fast_retina_fullbody_precomputed(
                        cand_x,
                        cand_y,
                        cand_r,
                        cand_type_fast,
                        is_self,
                        float(eye_x),
                        float(eye_y),
                        float(agent.angle),
                        float(sensor.vision_radius),
                        float(half_fov_fast),
                        cos_rel,
                        sin_rel,
                        int(sensor.retina_count),
                        bool(sensor.see_food),
                        bool(sensor.see_bacteria),
                        bool(sensor.see_predators),
                        bool(spatial_filtered),
                        out,
                    )
                else:
                    ok = fast_retina_fullbody(
                        cand_x,
                        cand_y,
                        cand_r,
                        cand_type_fast,
                        is_self,
                        float(eye_x),
                        float(eye_y),
                        float(agent.angle),
                        float(sensor.vision_radius),
                        float(half_fov_fast),
                        int(sensor.retina_count),
                        bool(sensor.see_food),
                        bool(sensor.see_bacteria),
                        bool(sensor.see_predators),
                        bool(spatial_filtered),
                        out,
                    )
                if ok:
                    inputs = out.astype(np.float32).tolist()
                    _store_retina_inputs(sensor, inputs)
                    sensor._countdown = sensor.skip
                    results[idx] = inputs
                    continue

        # Vetores para candidatos
        dx = cand_x - eye_x
        dy = cand_y - eye_y
        dist = np.sqrt(dx*dx + dy*dy)
        # Filtra por raio de visão + raio objeto
        within = dist - cand_r <= sensor.vision_radius
        if not np.any(within):
            inputs = _empty_retina_inputs(sensor)
            _store_retina_inputs(sensor, inputs)
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue
        within_idx = np.flatnonzero(within)
        # Filtra por tipo visivel apenas no fallback sem spatial hash. No caminho
        # normal, query_ball_filtered ja retornou somente os tipos desejados.
        if scene.spatial_hash is not None:
            visible_mask = np.ones(within_idx.size, dtype=bool)
        else:
            cand_tc = np.array(type_flags, dtype=np.int8)
            tc = cand_tc[within_idx]
            visible_mask = (
                ((tc == 0) & sensor.see_food) |
                ((tc == 1) & sensor.see_bacteria) |
                ((tc == 2) & sensor.see_predators)
            )
        # Remove o próprio agente da lista de visíveis
        if np.any(visible_mask):
            self_within = is_self[within_idx]
            if np.any(self_within):
                visible_mask = visible_mask & (~self_within)
        if not np.any(visible_mask):
            inputs = _empty_retina_inputs(sensor)
            _store_retina_inputs(sensor, inputs)
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue
        visible_idx = within_idx[visible_mask]
        sel_dx = dx[visible_idx]
        sel_dy = dy[visible_idx]
        sel_dist = dist[visible_idx]
        sel_r = cand_r[visible_idx]
        if colors is not None:
            cand_colors = np.array(colors, dtype=np.float32)
            sel_colors = cand_colors[visible_idx]
        else:
            sel_colors = None
        # Ângulos para objetos
        # Distâncias efetivas (considera raio aprox)

        # Decide modo de mapeamento: 'single' usa centro (idéia antiga), 'fullbody' ativa
        # todas as retinas cujo ângulo cai dentro do span angular do objeto.
        half_fov = math.radians(sensor.fov_degrees/2.0)
        if half_fov <= 0:
            inputs = _empty_retina_inputs(sensor)
            _store_retina_inputs(sensor, inputs)
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue

        # ângulo relativo do centro do objeto (usado no modo 'single')
        # Pré-cálculos geométricos (usados apenas no modo 'single' para apressar filtro)
        if vision_mode == 'single' or sensor.retina_count == 1:
            obj_angle = np.arctan2(sel_dy, sel_dx)
            eff_dist = np.clip(sel_dist - sel_r, 0.0, sensor.vision_radius)
            ang_diff_objs = angle_wrap(obj_angle - agent.angle)
            dists = sel_dist
            with np.errstate(invalid='ignore', divide='ignore'):
                ratio = np.clip(sel_r / dists, 0.0, 1.0)
                half_span_all = np.arcsin(ratio)
            half_span_all[dists <= sel_r] = math.pi
            # Aplica filtro de interseção com FOV apenas no modo 'single'
            inside = np.abs(ang_diff_objs) <= (half_fov + half_span_all)
            if not np.any(inside):
                inputs = _empty_retina_inputs(sensor)
                _store_retina_inputs(sensor, inputs)
                sensor._countdown = sensor.skip
                results[idx] = inputs
                continue
            # reduz arrays para objetos que intersectam o FOV (single)
            ang_diff_objs = ang_diff_objs[inside]
            eff_dist = eff_dist[inside]
            sel_r = sel_r[inside]
            if sel_colors is not None:
                sel_colors = sel_colors[inside]
            # comportamento antigo (centro -> um índice)
            if sensor.retina_count > 1:
                rels = (ang_diff_objs + half_fov) / (2*half_fov) * (sensor.retina_count - 1)
            else:
                rels = np.zeros_like(ang_diff_objs)
            ray_idx = np.clip(np.round(rels).astype(int), 0, sensor.retina_count - 1)
            ray_best = np.full((sensor.retina_count,), np.inf, dtype=np.float32)
            ray_color = np.zeros((sensor.retina_count, 3), dtype=np.float32) if sel_colors is not None else None
            if sel_colors is None:
                np.minimum.at(ray_best, ray_idx, eff_dist)
            else:
                order = np.argsort(eff_dist)
                for obj_idx in order:
                    r_idx = int(ray_idx[obj_idx])
                    distance_value = float(eff_dist[obj_idx])
                    if distance_value < ray_best[r_idx]:
                        ray_best[r_idx] = distance_value
                        ray_color[r_idx] = sel_colors[obj_idx]
        else:
            obj_angle = np.arctan2(sel_dy, sel_dx)
            ang_diff_objs = angle_wrap(obj_angle - agent.angle)
            dists = sel_dist
            with np.errstate(invalid='ignore', divide='ignore'):
                ratio = np.clip(sel_r / dists, 0.0, 1.0)
                half_span_all = np.arcsin(ratio)
            half_span_all[dists <= sel_r] = math.pi
            inside = np.abs(ang_diff_objs) <= (half_fov + half_span_all)
            if not np.any(inside):
                inputs = _empty_retina_inputs(sensor)
                _store_retina_inputs(sensor, inputs)
                sensor._countdown = sensor.skip
                results[idx] = inputs
                continue
            sel_dx = sel_dx[inside]
            sel_dy = sel_dy[inside]
            sel_r = sel_r[inside]
            if sel_colors is not None:
                sel_colors = sel_colors[inside]
            # modo 'fullbody': interseção exata raio-círculo para todos os raios vs objetos
            # Direções dos raios no mundo
            ray_rel, _, _ = _get_retina_relative_rays(sensor.retina_count, half_fov)
            ray_angles = agent.angle + ray_rel
            dir_x = np.cos(ray_angles).astype(np.float32)
            dir_y = np.sin(ray_angles).astype(np.float32)

            # Vetores origem->centro (negativos de sel_dx/dy)
            ox = (-sel_dx).astype(np.float32)
            oy = (-sel_dy).astype(np.float32)
            rr = (sel_r).astype(np.float32)

            # b = d·o, c = o·o - r^2; broadcast em (R, M)
            # dir_x: (R,), dir_y: (R,) -> (R,1) para broadcast
            dxm = dir_x[:, None]
            dym = dir_y[:, None]
            omx = ox[None, :]
            omy = oy[None, :]
            b = dxm * omx + dym * omy
            c = (omx * omx + omy * omy) - (rr[None, :] * rr[None, :])
            disc = b * b - c
            # Inicializa com inf (sem interseção)
            ray_dists = np.full((sensor.retina_count, ox.shape[0]), np.inf, dtype=np.float32)
            hit = disc >= 0.0
            if np.any(hit):
                sqrt_disc = np.zeros_like(disc)
                sqrt_disc[hit] = np.sqrt(disc[hit])
                t1 = -b - sqrt_disc
                t2 = -b + sqrt_disc
                # Menor t positivo entre t1 e t2 (apenas onde há interseção)
                t_pos1 = np.where(t1 >= 0.0, t1, np.inf)
                t_pos2 = np.where(t2 >= 0.0, t2, np.inf)
                t_min = np.minimum(t_pos1, t_pos2)
                # Invalida pares sem interseção
                t_min = np.where(hit, t_min, np.inf)
                ray_dists = t_min
            # Melhor (menor) distância por raio entre todos objetos
            best_obj_idx = np.argmin(ray_dists, axis=1)
            ray_best = ray_dists[np.arange(sensor.retina_count), best_obj_idx]
            # Limita ao alcance da visão e aplica FOV por raio explicitamente
            # Nota: já geramos os raios dentro do FOV, mas reforçamos para robustez
            valid_range = (ray_best >= 0.0) & (ray_best <= sensor.vision_radius)
            ray_best = np.where(valid_range, ray_best, np.inf)
            if sel_colors is not None:
                ray_color = np.zeros((sensor.retina_count, 3), dtype=np.float32)
                if np.any(valid_range):
                    ray_color[valid_range] = sel_colors[best_obj_idx[valid_range]]
            else:
                ray_color = None

        # Converte para ativações
        activation = (sensor.vision_radius - ray_best) / sensor.vision_radius
        activation[~np.isfinite(ray_best)] = 0.0
        activation = np.clip(activation, 0.0, 1.0)
        if not use_color_channels:
            inputs = activation.tolist()
        else:
            if ray_color is None:
                ray_color = np.zeros((sensor.retina_count, 3), dtype=np.float32)
            inputs = []
            channels = getattr(sensor, "channels", ("d",))
            for ray_idx_out in range(sensor.retina_count):
                act = float(activation[ray_idx_out])
                color = ray_color[ray_idx_out]
                for channel in channels:
                    if channel == "r":
                        inputs.append(float(color[0]))
                    elif channel == "g":
                        inputs.append(float(color[1]))
                    elif channel == "b":
                        inputs.append(float(color[2]))
                    elif channel == "rd":
                        inputs.append(act * float(color[0]))
                    elif channel == "gd":
                        inputs.append(act * float(color[1]))
                    elif channel == "bd":
                        inputs.append(act * float(color[2]))
                    else:
                        inputs.append(act)
        _store_retina_inputs(sensor, inputs, activation.tolist())
        sensor._countdown = sensor.skip
        results[idx] = inputs
    return results  # type: ignore
