"""
Sensores e consulta de cena para percepção dos agentes.
"""
import math
import numpy as np
from typing import List, Optional, TYPE_CHECKING, Set, Any, Sequence

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
        eye_offset = agent.r
        eye_x = agent.x + math.cos(agent.angle) * eye_offset
        eye_y = agent.y + math.sin(agent.angle) * eye_offset
        half_fov = math.radians(self.fov_degrees / 2.0)
        if self.retina_count > 1:
            rel_angle = (-half_fov) + (ray_index / (self.retina_count - 1)) * (2 * half_fov)
        else:
            rel_angle = 0.0
        ray_angle = agent.angle + rel_angle
        activation = self.last_inputs[ray_index] if ray_index < len(self.last_inputs) else 0.0
        shown_length = (1.0 - activation) * self.vision_radius
        end_x = eye_x + math.cos(ray_angle) * shown_length
        end_y = eye_y + math.sin(ray_angle) * shown_length
        return (eye_x, eye_y, end_x, end_y, activation)


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
    from .sensors import RetinaSensor as _RS  # evitar shadow
    if not all(isinstance(s, _RS) for s in sensors):  # fallback se algum não for retina
        return [a.sensor.sense(a, scene, params) for a in agents]

    # Atualiza parâmetros dinâmicos e determina quais precisam recalcular
    need_update_idx = []
    results: List[List[float]] = [None] * len(agents)  # type: ignore
    for idx, (agent, sensor) in enumerate(zip(agents, sensors)):
        prefix = 'predator' if getattr(agent, 'is_predator', False) else 'bacteria'
        # Valores atuais desejados
        desired_count = params.get(f'{prefix}_retina_count', sensor.retina_count)
        desired_fov = params.get(f'{prefix}_retina_fov_degrees', sensor.fov_degrees)
        desired_radius = params.get(f'{prefix}_vision_radius', sensor.vision_radius)
        desired_see_food = params.get(f'{prefix}_retina_see_food', sensor.see_food)
        desired_see_bacteria = params.get(f'{prefix}_retina_see_bacteria', sensor.see_bacteria)
        desired_see_predators = params.get(f'{prefix}_retina_see_predators', sensor.see_predators)
        if (desired_count != sensor.retina_count or desired_fov != sensor.fov_degrees or
            desired_radius != sensor.vision_radius or desired_see_food != sensor.see_food or
            desired_see_bacteria != sensor.see_bacteria or desired_see_predators != sensor.see_predators):
            sensor.retina_count = max(1, int(desired_count))
            sensor.fov_degrees = float(desired_fov)
            sensor.vision_radius = float(desired_radius)
            sensor.see_food = bool(desired_see_food)
            sensor.see_bacteria = bool(desired_see_bacteria)
            sensor.see_predators = bool(desired_see_predators)
            sensor.last_inputs = []
            sensor._countdown = 0
        if sensor._countdown > 0 and sensor.last_inputs:
            sensor._countdown -= 1
            results[idx] = list(sensor.last_inputs)
        else:
            need_update_idx.append(idx)

    if not need_update_idx:
        return results  # type: ignore

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
                inputs = [0.0] * sensor.retina_count
                sensor.last_inputs = inputs
                sensor._countdown = sensor.skip
                results[idx] = inputs
            return results  # type: ignore

    def angle_wrap(a):
        return (a + np.pi) % (2 * np.pi) - np.pi

    for idx in need_update_idx:
        agent = agents[idx]
        sensor = sensors[idx]
        # Seleciona candidatos locais usando spatial hash quando disponível
        if scene.spatial_hash is not None:
            # Raio de busca: visão + raio máximo entre tipos relevantes
            max_r = 0.0
            if sensor.see_food:
                max_r = max(max_r, float(getattr(params, 'get', lambda k, d=None: d)('food_max_r', 5.0)))
            if sensor.see_bacteria:
                max_r = max(max_r, float(params.get('bacteria_body_size', 9.0)))
            if sensor.see_predators:
                max_r = max(max_r, float(params.get('predator_body_size', 14.0)))
            search_r = sensor.vision_radius + max_r
            eye_x = agent.x + math.cos(agent.angle) * agent.r
            eye_y = agent.y + math.sin(agent.angle) * agent.r
            nearby = scene.spatial_hash.query_ball(eye_x, eye_y, search_r)
            # Filtra tipos de interesse
            candidates_local = [o for o in nearby if (
                (getattr(o, 'type_code', -1) == 0 and sensor.see_food) or
                (getattr(o, 'type_code', -1) == 1 and sensor.see_bacteria) or
                (getattr(o, 'type_code', -1) == 2 and sensor.see_predators)
            )]
        else:
            candidates_local = global_candidates
            eye_x = agent.x + math.cos(agent.angle) * agent.r
            eye_y = agent.y + math.sin(agent.angle) * agent.r
        if not candidates_local:
            inputs = [0.0] * sensor.retina_count
            sensor.last_inputs = inputs
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue
        # Máscara de auto-interseção (evita ver a si mesmo)
        is_self = np.array([c is agent for c in candidates_local], dtype=bool)
        # Constrói arrays numpy dos candidatos locais
        cand_x = np.array([c.x for c in candidates_local], dtype=np.float32)
        cand_y = np.array([c.y for c in candidates_local], dtype=np.float32)
        cand_r = np.array([getattr(c, 'r', 0.0) for c in candidates_local], dtype=np.float32)
        cand_tc = np.array([getattr(c, 'type_code', -1) for c in candidates_local], dtype=np.int8)
        # Vetores para candidatos
        dx = cand_x - eye_x
        dy = cand_y - eye_y
        dist = np.sqrt(dx*dx + dy*dy)
        # Filtra por raio de visão + raio objeto
        within = dist - cand_r <= sensor.vision_radius
        if not np.any(within):
            inputs = [0.0] * sensor.retina_count
            sensor.last_inputs = inputs
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue
        # Filtra por tipo visível
        tc = cand_tc[within]
        visible_mask = (
            ((tc == 0) & sensor.see_food) |
            ((tc == 1) & sensor.see_bacteria) |
            ((tc == 2) & sensor.see_predators)
        )
        # Remove o próprio agente da lista de visíveis
        if np.any(visible_mask):
            self_within = is_self[within]
            if np.any(self_within):
                visible_mask = visible_mask & (~self_within)
        if not np.any(visible_mask):
            inputs = [0.0] * sensor.retina_count
            sensor.last_inputs = inputs
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue
        sel_dx = dx[within][visible_mask]
        sel_dy = dy[within][visible_mask]
        sel_dist = dist[within][visible_mask]
        sel_r = cand_r[within][visible_mask]
        # Ângulos para objetos
        obj_angle = np.arctan2(sel_dy, sel_dx)
        # Distâncias efetivas (considera raio aprox)
        eff_dist = np.clip(sel_dist - sel_r, 0.0, sensor.vision_radius)

        # Decide modo de mapeamento: 'single' usa centro (idéia antiga), 'fullbody' ativa
        # todas as retinas cujo ângulo cai dentro do span angular do objeto.
        vision_mode = params.get('retina_vision_mode', 'single') if params is not None else 'single'

        half_fov = math.radians(sensor.fov_degrees/2.0)
        if half_fov <= 0:
            inputs = [0.0] * sensor.retina_count
            sensor.last_inputs = inputs
            sensor._countdown = sensor.skip
            results[idx] = inputs
            continue

        # ângulo relativo do centro do objeto (usado no modo 'single')
        ang_diff_objs = angle_wrap(obj_angle - agent.angle)
        # Pré-cálculos geométricos (usados apenas no modo 'single' para apressar filtro)
        dists = np.sqrt(sel_dx*sel_dx + sel_dy*sel_dy)
        with np.errstate(invalid='ignore', divide='ignore'):
            ratio = np.clip(sel_r / dists, 0.0, 1.0)
            half_span_all = np.arcsin(ratio)
        cover_all_mask_all = dists <= sel_r
        half_span_all[cover_all_mask_all] = math.pi

        if vision_mode == 'single' or sensor.retina_count == 1:
            # Aplica filtro de interseção com FOV apenas no modo 'single'
            inside = np.abs(ang_diff_objs) <= (half_fov + half_span_all)
            if not np.any(inside):
                inputs = [0.0] * sensor.retina_count
                sensor.last_inputs = inputs
                sensor._countdown = sensor.skip
                results[idx] = inputs
                continue
            # reduz arrays para objetos que intersectam o FOV (single)
            ang_diff_objs = ang_diff_objs[inside]
            eff_dist = eff_dist[inside]
            sel_r = sel_r[inside]
            # comportamento antigo (centro -> um índice)
            # comportamento antigo (centro -> um índice)
            if sensor.retina_count > 1:
                rels = (ang_diff_objs + half_fov) / (2*half_fov) * (sensor.retina_count - 1)
            else:
                rels = np.zeros_like(ang_diff_objs)
            ray_idx = np.clip(np.round(rels).astype(int), 0, sensor.retina_count - 1)
            ray_best = np.full((sensor.retina_count,), np.inf, dtype=np.float32)
            np.minimum.at(ray_best, ray_idx, eff_dist)
        else:
            # modo 'fullbody': interseção exata raio-círculo para todos os raios vs objetos
            # Direções dos raios no mundo
            if sensor.retina_count > 1:
                ray_rel = np.linspace(-half_fov, half_fov, sensor.retina_count, dtype=np.float32)
            else:
                ray_rel = np.array([0.0], dtype=np.float32)
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
            ray_best = np.min(ray_dists, axis=1)
            # Limita ao alcance da visão e aplica FOV por raio explicitamente
            # Nota: já geramos os raios dentro do FOV, mas reforçamos para robustez
            valid_range = (ray_best >= 0.0) & (ray_best <= sensor.vision_radius)
            ray_best = np.where(valid_range, ray_best, np.inf)

        # Converte para ativações
        activation = (sensor.vision_radius - ray_best) / sensor.vision_radius
        activation[~np.isfinite(ray_best)] = 0.0
        activation = np.clip(activation, 0.0, 1.0)
        inputs = activation.tolist()
        sensor.last_inputs = inputs
        sensor._countdown = sensor.skip
        results[idx] = inputs
    return results  # type: ignore
