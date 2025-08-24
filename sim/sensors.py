"""
Sensores e consulta de cena para percepção dos agentes.
"""
import math
from typing import List, Optional, TYPE_CHECKING, Set, Any

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


class SceneQuery:
    """
    Serviço para consultas espaciais da cena.
    Usado pelos sensores para fazer raycasts e queries de proximidade.
    """
    
    def __init__(self, spatial_hash: Optional['SpatialHash'], entities: dict, params: 'Params'):
        self.spatial_hash = spatial_hash
        self.entities = entities  # {'bacteria': [...], 'predators': [...], 'foods': [...]}
        self.params = params
    
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
            filtered = set()
            for obj in candidates:
                obj_type = self._get_object_type(obj)
                if ((obj_type == 'food' and see_food) or
                    (obj_type == 'bacteria' and see_bacteria) or
                    (obj_type == 'predator' and see_predators)):
                    filtered.add(obj)
            candidates = filtered
        
        return candidates
    
    def _get_object_type(self, obj) -> str:
        """Determina tipo do objeto baseado na classe ou atributos."""
        # Nova lógica: primeiro distingue agentes (têm atributo is_predator ou brain)
        if hasattr(obj, 'is_predator'):
            return 'predator' if getattr(obj, 'is_predator') else 'bacteria'
        if hasattr(obj, 'brain'):
            return 'bacteria'
        # Caso contrário assume comida
        return 'food'


class RetinaSensor:
    """
    Sensor de retina para visão dos agentes.
    Configura raios de visão em leque e detecta objetos.
    """
    
    def __init__(self, retina_count: int = 18, vision_radius: float = 120.0,
                 fov_degrees: float = 180.0, skip: int = 0,
                 see_food: bool = True, see_bacteria: bool = False, see_predators: bool = False):
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
        
        # Estado interno
        self._countdown = 0
        self.last_inputs: List[float] = []
    
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
        desired_count = params.get(f'{prefix}_retina_count', self.retina_count)
        desired_fov = params.get(f'{prefix}_retina_fov_degrees', self.fov_degrees)
        desired_radius = params.get(f'{prefix}_vision_radius', self.vision_radius)
        desired_see_food = params.get(f'{prefix}_retina_see_food', self.see_food)
        desired_see_bacteria = params.get(f'{prefix}_retina_see_bacteria', self.see_bacteria)
        desired_see_predators = params.get(f'{prefix}_retina_see_predators', self.see_predators)
        if (desired_count != self.retina_count or desired_fov != self.fov_degrees or
            desired_radius != self.vision_radius or desired_see_food != self.see_food or
            desired_see_bacteria != self.see_bacteria or desired_see_predators != self.see_predators):
            self.retina_count = max(1, int(desired_count))
            self.fov_degrees = float(desired_fov)
            self.vision_radius = float(desired_radius)
            self.see_food = bool(desired_see_food)
            self.see_bacteria = bool(desired_see_bacteria)
            self.see_predators = bool(desired_see_predators)
            # Força recálculo completo
            self.last_inputs = []
            self._countdown = 0

        # Sistema de skip para otimização (após possível atualização dinâmica)
        if self._countdown > 0:
            self._countdown -= 1
            if self.last_inputs:
                return list(self.last_inputs)
        
        # Calcula posição do "olho" (frente do agente)
        eye_offset = agent.r
        eye_x = agent.x + math.cos(agent.angle) * eye_offset
        eye_y = agent.y + math.sin(agent.angle) * eye_offset
        
        # Determina campo de visão
        half_fov = math.radians(self.fov_degrees / 2.0)
        
        # Faz raycasts
        inputs = []
        for i in range(self.retina_count):
            # Calcula ângulo relativo do raio
            if self.retina_count > 1:
                rel_angle = (-half_fov) + (i / (self.retina_count - 1)) * (2 * half_fov)
            else:
                rel_angle = 0
            
            ray_angle = agent.angle + rel_angle
            ray_dx = math.cos(ray_angle)
            ray_dy = math.sin(ray_angle)
            
            # Faz raycast
            distance = scene.raycast(
                eye_x, eye_y, ray_dx, ray_dy, self.vision_radius,
                ignore=agent,
                see_food=self.see_food,
                see_bacteria=self.see_bacteria,
                see_predators=self.see_predators
            )
            
            # Converte distância para ativação [0..1]
            if distance is None:
                activation = 0.0
            else:
                # Ativação inversamente proporcional à distância
                activation = max(0.0, min(1.0, (self.vision_radius - distance) / self.vision_radius))
            
            inputs.append(activation)
        
        # Atualiza estado e countdown
        self.last_inputs = list(inputs)
        self._countdown = self.skip
        
        return inputs
    
    def get_ray_info(self, agent: 'Agent', ray_index: int) -> tuple:
        """Retorna (start_x, start_y, end_x, end_y, activation) para um raio.
        Se índice inválido ou sem dados ainda, retorna None.
        """
        if ray_index < 0 or ray_index >= self.retina_count or not self.last_inputs:
            return None

        # Posição do "olho" na frente do agente
        eye_offset = agent.r
        eye_x = agent.x + math.cos(agent.angle) * eye_offset
        eye_y = agent.y + math.sin(agent.angle) * eye_offset

        # Ângulo relativo dentro do FOV
        half_fov = math.radians(self.fov_degrees / 2.0)
        if self.retina_count > 1:
            rel_angle = (-half_fov) + (ray_index / (self.retina_count - 1)) * (2 * half_fov)
        else:
            rel_angle = 0.0
        ray_angle = agent.angle + rel_angle

        # Ativação correspondente
        activation = self.last_inputs[ray_index] if ray_index < len(self.last_inputs) else 0.0

        # Comprimento inverso: quanto mais forte (mais perto), menor o raio desenhado
        shown_length = (1.0 - activation) * self.vision_radius
        end_x = eye_x + math.cos(ray_angle) * shown_length
        end_y = eye_y + math.sin(ray_angle) * shown_length
        return (eye_x, eye_y, end_x, end_y, activation)
