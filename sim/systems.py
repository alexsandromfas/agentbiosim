"""
Sistemas de regras da simulação: interação, reprodução, morte.
Aplicam regras específicas e operam sobre conjuntos de entidades.
"""
import math
import random
from typing import List, Set, TYPE_CHECKING

if TYPE_CHECKING:
    from .entities import Agent, Food, Bacteria, Predator
    from .controllers import Params
    from .spatial import SpatialHash


class InteractionSystem:
    """
    Sistema que gerencia interações entre entidades (comer, predar).
    
    - Bactérias comem comida
    - Predadores comem bactérias  
    - Usa spatial hash para otimização
    """
    
    def __init__(self):
        self._foods_to_remove: Set['Food'] = set()
        self._agents_to_remove: Set['Agent'] = set()
        self._removed_bacteria_count = 0
        self.last_foods_eaten = 0
        self.last_agents_predated = 0
    
    def apply(self, bacteria: List['Bacteria'], predators: List['Predator'],
              foods: List['Food'], spatial_hash: 'SpatialHash', params: 'Params',
              frozen_agents: Set['Agent'] | None = None) -> Set['Agent']:
        """Aplica interações por um frame e retorna agentes removidos."""
        self._foods_to_remove.clear()
        self._agents_to_remove.clear()
        self._removed_bacteria_count = 0
        frozen_agents = frozen_agents or set()

        # Bactérias comem comida (ganham energia)
        self._bacteria_eat_food(bacteria, foods, spatial_hash, params, frozen_agents)

        # Predadores comem bactérias
        if predators:
            self._predators_eat_bacteria(predators, bacteria, spatial_hash, params, frozen_agents)

        # Remove comida consumida
        if self._foods_to_remove:
            foods[:] = [f for f in foods if f not in self._foods_to_remove]

        # Remove bactérias predadas
        if self._agents_to_remove:
            bacteria[:] = [b for b in bacteria if b not in self._agents_to_remove]
        self.last_foods_eaten = len(self._foods_to_remove)
        self.last_agents_predated = len(self._agents_to_remove)
        return set(self._agents_to_remove) if self._agents_to_remove else set()

    def apply_generic(self, agents: List['Agent'], foods: List['Food'], spatial_hash: 'SpatialHash',
                      params: 'Params', frozen_agents: Set['Agent'] | None = None,
                      agent_labels: dict | None = None) -> Set['Agent']:
        """Aplica dieta genÃ©rica: organismos podem comer comida e/ou outros organismos."""
        self._foods_to_remove.clear()
        self._agents_to_remove.clear()
        self._removed_bacteria_count = 0
        frozen_agents = frozen_agents or set()
        agent_labels = agent_labels or {}
        agent_eaters = [agent for agent in agents if bool(getattr(agent, 'diet_agents', False))]
        max_agent_radius = max((float(getattr(agent, 'r', 0.0)) for agent in agents), default=0.0)
        self._agents_eat_food_generic(agents, foods, spatial_hash, frozen_agents)
        if agent_eaters:
            label_limits_active = any(
                int(meta.get('min_limit', 0) or 0) > 0 or int(meta.get('max_limit', 0) or 0) > 0
                for meta in agent_labels.values()
            )
            label_counts = self._label_counts(agents) if label_limits_active else {}
            self._agents_eat_agents_generic(
                agents, agent_eaters, spatial_hash, frozen_agents, agent_labels, label_counts, max_agent_radius
            )
        if self._foods_to_remove:
            foods[:] = [f for f in foods if f not in self._foods_to_remove]
        self.last_foods_eaten = len(self._foods_to_remove)
        self.last_agents_predated = len(self._agents_to_remove)
        return set(self._agents_to_remove) if self._agents_to_remove else set()

    @staticmethod
    def _label_counts(agents: List['Agent']) -> dict[int, int]:
        counts: dict[int, int] = {}
        for agent in agents:
            for label_id in getattr(agent, 'label_ids', set()) or set():
                try:
                    label_id = int(label_id)
                except Exception:
                    continue
                counts[label_id] = counts.get(label_id, 0) + 1
        return counts

    @staticmethod
    def _shares_label(a: 'Agent', b: 'Agent') -> bool:
        labels_a = getattr(a, 'label_ids', set()) or set()
        labels_b = getattr(b, 'label_ids', set()) or set()
        return bool(labels_a and labels_b and labels_a.intersection(labels_b))

    @staticmethod
    def _can_remove_by_label_limits(agent: 'Agent', agent_labels: dict, label_counts: dict[int, int]) -> bool:
        for label_id in getattr(agent, 'label_ids', set()) or set():
            try:
                label_id = int(label_id)
            except Exception:
                continue
            meta = agent_labels.get(label_id, {}) if agent_labels else {}
            min_limit = int(meta.get('min_limit', 0) or 0)
            if min_limit > 0 and label_counts.get(label_id, 0) <= min_limit:
                return False
        return True

    @staticmethod
    def _register_label_removal(agent: 'Agent', label_counts: dict[int, int]):
        for label_id in getattr(agent, 'label_ids', set()) or set():
            try:
                label_id = int(label_id)
            except Exception:
                continue
            label_counts[label_id] = max(0, label_counts.get(label_id, 0) - 1)

    def _agents_eat_food_generic(self, agents: List['Agent'], foods: List['Food'],
                                 spatial_hash: 'SpatialHash', frozen_agents: Set['Agent']):
        nearby_buffer = set()
        for agent in agents:
            if agent in frozen_agents or not bool(getattr(agent, 'diet_food', False)):
                continue
            if spatial_hash:
                nearby_foods = spatial_hash.query_ball_filtered_into(agent.x, agent.y, agent.r + 8.0, 0, nearby_buffer)
            else:
                nearby_foods = foods
            for food in nearby_foods:
                if food in self._foods_to_remove:
                    continue
                dx = agent.x - food.x
                dy = agent.y - food.y
                r_sum = agent.r + food.r
                if dx*dx + dy*dy <= r_sum * r_sum:
                    cap = getattr(getattr(agent, 'energy_model', None), 'energy_cap', None)
                    before_energy = getattr(agent, 'energy', 0.0)
                    efficiency = max(0.0, float(getattr(agent, 'diet_food_efficiency', 1.0)))
                    agent.add_energy(food.energy * efficiency, cap=cap)
                    gained = max(0.0, getattr(agent, 'energy', 0.0) - before_energy)
                    agent.food_eaten_count = getattr(agent, 'food_eaten_count', 0) + 1
                    agent.food_energy_eaten_total = getattr(agent, 'food_energy_eaten_total', 0.0) + gained
                    self._foods_to_remove.add(food)
                    break

    def _agents_eat_agents_generic(self, agents: List['Agent'], agent_eaters: List['Agent'], spatial_hash: 'SpatialHash',
                                   frozen_agents: Set['Agent'], agent_labels: dict,
                                   label_counts: dict[int, int], max_agent_radius: float):
        nearby_buffer = set()
        label_limits_active = bool(label_counts)
        for eater in agent_eaters:
            if eater in frozen_agents:
                continue
            if spatial_hash:
                type_filter = 1 if bool(getattr(eater, 'is_predator', False)) else (1, 2)
                nearby_objects = spatial_hash.query_ball_filtered_into(
                    eater.x, eater.y, eater.r + max_agent_radius, type_filter, nearby_buffer
                )
            else:
                nearby_objects = agents
            for prey in nearby_objects:
                if (
                    prey is eater or prey in frozen_agents or prey in self._agents_to_remove
                    or not hasattr(prey, 'energy_model')
                ):
                    continue
                if bool(getattr(eater, 'is_predator', False)) and bool(getattr(prey, 'is_predator', False)):
                    continue
                if not bool(getattr(eater, 'diet_same_label', False)) and self._shares_label(eater, prey):
                    continue
                if label_limits_active and not self._can_remove_by_label_limits(prey, agent_labels, label_counts):
                    continue
                dx = eater.x - prey.x
                dy = eater.y - prey.y
                r_sum = eater.r + prey.r
                if dx*dx + dy*dy <= r_sum * r_sum:
                    cap = getattr(getattr(eater, 'energy_model', None), 'energy_cap', None)
                    before_energy = getattr(eater, 'energy', 0.0)
                    efficiency = max(0.0, float(getattr(eater, 'diet_agent_efficiency', 0.7)))
                    eater.add_energy(getattr(prey, 'energy', 0.0) * efficiency, cap=cap)
                    gained = max(0.0, getattr(eater, 'energy', 0.0) - before_energy)
                    eater.prey_eaten_count = getattr(eater, 'prey_eaten_count', 0) + 1
                    eater.prey_energy_eaten_total = getattr(eater, 'prey_energy_eaten_total', 0.0) + gained
                    self._agents_to_remove.add(prey)
                    if label_limits_active:
                        self._register_label_removal(prey, label_counts)
                    break
    
    def _bacteria_eat_food(self, bacteria: List['Bacteria'], foods: List['Food'],
                          spatial_hash: 'SpatialHash', params: 'Params',
                          frozen_agents: Set['Agent']):
        """Processa bactérias comendo comida (energia += food.energy)."""
        food_radius = params.get('food_max_r', 5.0)
        configured_cap = params.get('bacteria_energy_cap', None)
        nearby_buffer = set()
        for bacterium in bacteria:
            if bacterium in frozen_agents:
                continue
            if spatial_hash:
                # Usa spatial hash para encontrar comida próxima
                nearby_foods = spatial_hash.query_ball_filtered_into(
                    bacterium.x, bacterium.y, bacterium.r + food_radius, 0, nearby_buffer
                )
            else:
                # Fallback: busca linear
                nearby_foods = foods
            
            for food in nearby_foods:
                if food in self._foods_to_remove:
                    continue
                
                # Verifica colisão
                dx = bacterium.x - food.x
                dy = bacterium.y - food.y
                r_sum = bacterium.r + food.r
                if dx*dx + dy*dy <= r_sum * r_sum:
                    # Bactéria come comida -> ganha energia respeitando cap.
                    cap = configured_cap if configured_cap is not None else getattr(bacterium.energy_model, 'energy_cap', None)
                    before_energy = getattr(bacterium, 'energy', 0.0)
                    bacterium.add_energy(food.energy, cap=cap)
                    gained = max(0.0, getattr(bacterium, 'energy', 0.0) - before_energy)
                    bacterium.food_eaten_count = getattr(bacterium, 'food_eaten_count', 0) + 1
                    bacterium.food_energy_eaten_total = getattr(bacterium, 'food_energy_eaten_total', 0.0) + gained
                    self._foods_to_remove.add(food)
                    break  # Uma comida por frame por bactéria
    
    def _predators_eat_bacteria(self, predators: List['Predator'], 
                               bacteria: List['Bacteria'], spatial_hash: 'SpatialHash', 
                               params: 'Params', frozen_agents: Set['Agent']):
        """Processa predadores comendo bactérias."""
        bacteria_radius = params.get('bacteria_max_r', 12.0)
        min_bact = params.get('bacteria_min_limit', 0)
        configured_cap = params.get('predator_energy_cap', None)
        nearby_buffer = set()
        for predator in predators:
            if predator in frozen_agents:
                continue
            if spatial_hash:
                # Usa spatial hash
                nearby_bacteria = spatial_hash.query_ball_filtered_into(
                    predator.x, predator.y, predator.r + bacteria_radius, 1, nearby_buffer
                )
            else:
                # Fallback
                nearby_bacteria = bacteria
            
            # Se já estamos no mínimo de bactérias permitido, impedir predação adicional
            if len(bacteria) - self._removed_bacteria_count <= min_bact:
                continue

            for bacterium in nearby_bacteria:
                if bacterium in self._agents_to_remove or bacterium in frozen_agents:
                    continue
                
                # Verifica colisão
                dx = predator.x - bacterium.x
                dy = predator.y - bacterium.y
                r_sum = predator.r + bacterium.r
                if dx*dx + dy*dy <= r_sum * r_sum:
                    # Predador come bactéria -> ganha parte da energia respeitando cap.
                    cap = configured_cap if configured_cap is not None else getattr(predator.energy_model, 'energy_cap', None)
                    before_energy = getattr(predator, 'energy', 0.0)
                    predator.add_energy(bacterium.energy * 0.7, cap=cap)
                    gained = max(0.0, getattr(predator, 'energy', 0.0) - before_energy)
                    predator.prey_eaten_count = getattr(predator, 'prey_eaten_count', 0) + 1
                    predator.prey_energy_eaten_total = getattr(predator, 'prey_energy_eaten_total', 0.0) + gained
                    # Rechecar mínimo antes de remover
                    if len(bacteria) - self._removed_bacteria_count <= min_bact:
                        break
                    self._agents_to_remove.add(bacterium)
                    self._removed_bacteria_count += 1
                    break  # Uma bactéria por frame por predador


class ReproductionSystem:
    """
    Sistema de reprodução para agentes.
    Aplica regras de reprodução assexuada com mutação.
    """

    def __init__(self):
        self.last_births = []
        self.last_blocked_by_age = 0
        self.last_blocked_by_cooldown = 0
    
    def apply(self, agents: List['Agent'], params: 'Params', agent_labels: dict | None = None) -> List['Agent']:
        """
        Processa reprodução de agentes.
        
        Args:
            agents: Lista de agentes a verificar
            params: Parâmetros da simulação
            
        Returns:
            Lista de novos agentes criados
        """
        new_agents = []
        self.last_births = []
        self.last_blocked_by_age = 0
        self.last_blocked_by_cooldown = 0
        min_age = max(0.0, float(params.get('reproduction_min_age', 0.0)))
        cooldown = max(0.0, float(params.get('reproduction_cooldown', 0.0)))
        
        # Determina limites de população por tipo
        bacteria_count = sum(1 for a in agents if not getattr(a, 'is_predator', False))
        predator_count = sum(1 for a in agents if getattr(a, 'is_predator', False))
        bacteria_new_count = 0
        predator_new_count = 0
        
        bacteria_max = max(0, int(params.get('bacteria_max_limit', 0) or 0))
        predator_max = max(0, int(params.get('predator_max_limit', 0) or 0))
        agent_labels = agent_labels or {}
        label_counts = InteractionSystem._label_counts(agents)
        label_new_counts: dict[int, int] = {}
        
        for agent in agents:
            if min_age > 0.0 and getattr(agent, 'age', 0.0) < min_age:
                self.last_blocked_by_age += 1
                continue
            last_reproduction_age = getattr(agent, 'last_reproduction_age', None)
            if cooldown > 0.0 and last_reproduction_age is not None:
                if getattr(agent, 'age', 0.0) - float(last_reproduction_age) < cooldown:
                    self.last_blocked_by_cooldown += 1
                    continue

            # Verifica se pode se reproduzir
            if not agent.can_reproduce(params):
                continue
            
            # Verifica limites de população
            is_predator = getattr(agent, 'is_predator', False)
            if is_predator:
                if predator_max > 0 and predator_count + predator_new_count >= predator_max:
                    continue
            else:
                if bacteria_max > 0 and bacteria_count + bacteria_new_count >= bacteria_max:
                    continue
            if self._blocked_by_label_max(agent, agent_labels, label_counts, label_new_counts):
                continue
            
            # Cria filho
            try:
                child = agent.reproduce(params)
                if hasattr(agent, 'last_reproduction_age'):
                    agent.last_reproduction_age = getattr(agent, 'age', 0.0)
                if hasattr(child, 'last_reproduction_age'):
                    child.last_reproduction_age = getattr(child, 'age', 0.0)
                new_agents.append(child)
                
                # Atualiza contadores
                if is_predator:
                    predator_new_count += 1
                else:
                    bacteria_new_count += 1
                for label_id in getattr(child, 'label_ids', set()) or set():
                    try:
                        label_id = int(label_id)
                    except Exception:
                        continue
                    label_new_counts[label_id] = label_new_counts.get(label_id, 0) + 1
                    
            except Exception as e:
                print(f"Erro na reprodução: {e}")
                continue
        
        self.last_births = list(new_agents)
        return new_agents

    @staticmethod
    def _blocked_by_label_max(agent: 'Agent', agent_labels: dict,
                              label_counts: dict[int, int],
                              label_new_counts: dict[int, int]) -> bool:
        for label_id in getattr(agent, 'label_ids', set()) or set():
            try:
                label_id = int(label_id)
            except Exception:
                continue
            meta = agent_labels.get(label_id, {}) if agent_labels else {}
            max_limit = int(meta.get('max_limit', 0) or 0)
            if max_limit > 0 and label_counts.get(label_id, 0) + label_new_counts.get(label_id, 0) >= max_limit:
                return True
        return False


class DeathSystem:
    """
    Sistema de morte controlada para agentes.
    
    Implementa morte limitada por frame e respeita limites mínimos de população.
    """
    
    def __init__(self, max_deaths_per_step: int = 1):
        self.max_deaths_per_step = max_deaths_per_step
        self._death_queue = []  # Fila de agentes marcados para morrer
        self.last_deaths = []
    
    def apply(self, bacteria: List['Bacteria'], predators: List['Predator'],
              params: 'Params', agent_labels: dict | None = None) -> tuple:
        """
        Aplica sistema de morte controlada.
        
        Args:
            bacteria: Lista de bactérias
            predators: Lista de predadores  
            params: Parâmetros da simulação
            
        Returns:
            Tuple (bacteria_survivors, predator_survivors)
        """
        self.last_deaths = []

        # Processa morte de bactérias
        bacteria_survivors = self._process_deaths(
            bacteria,
            params.get('bacteria_min_limit', 0),
            params.get('bacteria_death_energy', 0.0),
            params,
            agent_labels or {}
        )
        
        # Processa morte de predadores
        predator_survivors = self._process_deaths(
            predators,
            params.get('predator_min_limit', 0),
            params.get('predator_death_energy', 0.0),
            params,
            agent_labels or {}
        )
        
        return bacteria_survivors, predator_survivors
    
    def _process_deaths(self, agents: List['Agent'], min_limit: int,
                       death_energy: float, params: 'Params', agent_labels: dict | None = None) -> List['Agent']:
        """Processa morte de um tipo específico de agente."""
        if not agents:
            return agents

        current_count = len(agents)
        death_candidates = [a for a in agents if a.should_die(params)]
        min_rescue_enabled = bool(params.get('population_min_rescue_enabled', True))
        protected_min = min_limit if min_rescue_enabled else 0

        # Limite mínimo preservado
        if min_rescue_enabled and current_count <= min_limit:
            for a in death_candidates:
                a.energy = max(a.energy, death_energy + 0.001)
            return agents

        deaths_available = min(current_count - protected_min,
                                self.max_deaths_per_step,
                                len(death_candidates))
        if deaths_available <= 0:
            if min_rescue_enabled:
                for a in death_candidates:
                    a.energy = max(a.energy, death_energy + 0.001)
            return agents

        death_candidates.sort(key=lambda a: a.energy)
        label_counts = InteractionSystem._label_counts(agents)
        agents_to_kill = []
        for candidate in death_candidates:
            if len(agents_to_kill) >= deaths_available:
                break
            if not InteractionSystem._can_remove_by_label_limits(candidate, agent_labels or {}, label_counts):
                continue
            agents_to_kill.append(candidate)
            InteractionSystem._register_label_removal(candidate, label_counts)
        self.last_deaths.extend(agents_to_kill)
        if min_rescue_enabled:
            for a in death_candidates[deaths_available:]:
                a.energy = max(a.energy, death_energy + 0.001)
        return [a for a in agents if a not in agents_to_kill]


class CollisionSystem:
    """
    Sistema de resolução de colisões entre agentes.
    """
    
    def __init__(self):
        self._processed_pairs = set()
        self.last_collisions_resolved = 0
    
    def apply(self, agents: List['Agent'], spatial_hash: 'SpatialHash', params: 'Params'):
        """
        Resolve colisões entre agentes.
        
        Args:
            agents: Lista de todos os agentes
            spatial_hash: Hash espacial para otimização
            params: Parâmetros da simulação
        """
        self._processed_pairs.clear()
        self.last_collisions_resolved = 0
        
        if spatial_hash:
            self._resolve_with_spatial_hash(agents, spatial_hash)
        else:
            self._resolve_brute_force(agents)
        return self.last_collisions_resolved
    
    def _resolve_with_spatial_hash(self, agents: List['Agent'], spatial_hash: 'SpatialHash'):
        """Resolve colisões usando spatial hash."""
        agent_index = {agent: idx for idx, agent in enumerate(agents)}
        nearby_buffer = set()
        for idx, agent in enumerate(agents):
            # Busca vizinhos próximos
            nearby_objects = spatial_hash.query_ball_into(agent.x, agent.y, agent.r * 2.0, nearby_buffer)
            
            for other in nearby_objects:
                if (other is agent or 
                    not hasattr(other, 'vx') or  # Não é agente
                    not hasattr(other, 'vy')):
                    continue
                
                # Evita processar o mesmo par duas vezes sem criar tuple/sorted em loop quente.
                other_idx = agent_index.get(other, -1)
                if other_idx <= idx:
                    continue
                
                if self._resolve_collision_pair(agent, other):
                    self.last_collisions_resolved += 1
    
    def _resolve_brute_force(self, agents: List['Agent']):
        """Resolve colisões com busca bruta (para populações pequenas)."""
        n = len(agents)
        for i in range(n):
            for j in range(i + 1, n):
                if self._resolve_collision_pair(agents[i], agents[j]):
                    self.last_collisions_resolved += 1
    
    def _resolve_collision_pair(self, agent1: 'Agent', agent2: 'Agent'):
        """
        Resolve colisão entre dois agentes específicos.
        Implementa resposta elástica simples.
        """
        dx = agent1.x - agent2.x
        dy = agent1.y - agent2.y
        dist2 = dx*dx + dy*dy
        r_sum = agent1.r + agent2.r
        r_sum2 = r_sum * r_sum
        if dist2 >= r_sum2:
            return False  # Sem colisão
        if dist2 == 0:
            distance = 0.01
            dx = 0.01
            dy = 0.0
        else:
            distance = math.sqrt(dist2)
        overlap = r_sum - distance
        
        # Separação dos objetos
        push_x = dx / distance * overlap
        push_y = dy / distance * overlap
        total_mass = agent1.m + agent2.m
        
        if total_mass == 0:
            total_mass = 1.0
        
        # Separação baseada na massa
        mass_ratio_1 = agent2.m / total_mass
        mass_ratio_2 = agent1.m / total_mass
        
        agent1.x += push_x * mass_ratio_1
        agent1.y += push_y * mass_ratio_1
        agent2.x -= push_x * mass_ratio_2
        agent2.y -= push_y * mass_ratio_2

        # Resposta elástica nas velocidades
        inv_dist = 1.0 / distance
        nx = dx * inv_dist  # Normal x
        ny = dy * inv_dist  # Normal y

        # Velocidade relativa
        dvx = agent1.vx - agent2.vx
        dvy = agent1.vy - agent2.vy
        relative_velocity_normal = dvx * nx + dvy * ny

        if relative_velocity_normal > 0:
            return True  # Objetos separados, mas velocidades ja estavam se afastando

        # Impulso elástico
        impulse = (2 * relative_velocity_normal) / total_mass
        impulse_x = impulse * nx
        impulse_y = impulse * ny

        agent1.vx -= impulse_x * agent2.m
        agent1.vy -= impulse_y * agent2.m
        agent2.vx += impulse_x * agent1.m
        agent2.vy += impulse_y * agent1.m
        return True
