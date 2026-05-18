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
        foods[:] = [f for f in foods if f not in self._foods_to_remove]

        # Remove bactérias predadas
        bacteria[:] = [b for b in bacteria if b not in self._agents_to_remove]
        self.last_foods_eaten = len(self._foods_to_remove)
        self.last_agents_predated = len(self._agents_to_remove)
        return set(self._agents_to_remove)
    
    def _bacteria_eat_food(self, bacteria: List['Bacteria'], foods: List['Food'],
                          spatial_hash: 'SpatialHash', params: 'Params',
                          frozen_agents: Set['Agent']):
        """Processa bactérias comendo comida (energia += food.energy)."""
        for bacterium in bacteria:
            if bacterium in frozen_agents:
                continue
            if spatial_hash:
                # Usa spatial hash para encontrar comida próxima
                food_radius = params.get('food_max_r', 5.0)
                nearby_objects = spatial_hash.query_ball(
                    bacterium.x, bacterium.y, bacterium.r + food_radius
                )
                # type_code 0 = Food
                nearby_foods = [obj for obj in nearby_objects if getattr(obj, 'type_code', -1) == 0]
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
                    cap = params.get('bacteria_energy_cap', getattr(bacterium.energy_model, 'energy_cap', None))
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
        for predator in predators:
            if predator in frozen_agents:
                continue
            if spatial_hash:
                # Usa spatial hash
                bacteria_radius = params.get('bacteria_max_r', 12.0)
                nearby_objects = spatial_hash.query_ball(
                    predator.x, predator.y, predator.r + bacteria_radius
                )
                # type_code 1 = Bacteria
                nearby_bacteria = [obj for obj in nearby_objects if getattr(obj, 'type_code', -1) == 1]
            else:
                # Fallback
                nearby_bacteria = bacteria
            
            # Se já estamos no mínimo de bactérias permitido, impedir predação adicional
            min_bact = params.get('bacteria_min_limit', 0)
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
                    cap = params.get('predator_energy_cap', getattr(predator.energy_model, 'energy_cap', None))
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
    
    def apply(self, agents: List['Agent'], params: 'Params') -> List['Agent']:
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
        
        bacteria_max = params.get('bacteria_max_limit', 300)
        predator_max = params.get('predator_max_limit', 100)
        
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
                if predator_count + predator_new_count >= predator_max:
                    continue
            else:
                if bacteria_count + bacteria_new_count >= bacteria_max:
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
                    
            except Exception as e:
                print(f"Erro na reprodução: {e}")
                continue
        
        self.last_births = list(new_agents)
        return new_agents


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
              params: 'Params') -> tuple:
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
            params.get('bacteria_min_limit', 10),
            params.get('bacteria_death_energy', 0.0),
            params
        )
        
        # Processa morte de predadores
        predator_survivors = self._process_deaths(
            predators,
            params.get('predator_min_limit', 0),
            params.get('predator_death_energy', 0.0),
            params
        )
        
        return bacteria_survivors, predator_survivors
    
    def _process_deaths(self, agents: List['Agent'], min_limit: int,
                       death_energy: float, params: 'Params') -> List['Agent']:
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
        agents_to_kill = death_candidates[:deaths_available]
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
        for agent in agents:
            # Busca vizinhos próximos
            nearby_objects = spatial_hash.query_ball(agent.x, agent.y, agent.r * 2.0)
            
            for other in nearby_objects:
                if (other is agent or 
                    not hasattr(other, 'vx') or  # Não é agente
                    not hasattr(other, 'vy')):
                    continue
                
                # Evita processar o mesmo par duas vezes
                pair_key = tuple(sorted((id(agent), id(other))))
                if pair_key in self._processed_pairs:
                    continue
                self._processed_pairs.add(pair_key)
                
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
