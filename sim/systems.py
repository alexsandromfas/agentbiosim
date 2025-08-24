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
    
    def apply(self, bacteria: List['Bacteria'], predators: List['Predator'], 
              foods: List['Food'], spatial_hash: 'SpatialHash', params: 'Params'):
        """
        Aplica interações por um frame.
        
        Args:
            bacteria: Lista de bactérias
            predators: Lista de predadores
            foods: Lista de comida
            spatial_hash: Hash espacial para otimização
            params: Parâmetros da simulação
        """
        self._foods_to_remove.clear()
        self._agents_to_remove.clear()
        
        # Bactérias comem comida
        self._bacteria_eat_food(bacteria, foods, spatial_hash, params)
        
        # Predadores comem bactérias
        if predators:
            self._predators_eat_bacteria(predators, bacteria, spatial_hash, params)
        
        # Remove comida consumida
        foods[:] = [f for f in foods if f not in self._foods_to_remove]
        
        # Remove bactérias predadas
        bacteria[:] = [b for b in bacteria if b not in self._agents_to_remove]
    
    def _bacteria_eat_food(self, bacteria: List['Bacteria'], foods: List['Food'],
                          spatial_hash: 'SpatialHash', params: 'Params'):
        """Processa bactérias comendo comida."""
        for bacterium in bacteria:
            if spatial_hash:
                # Usa spatial hash para encontrar comida próxima
                food_radius = params.get('food_max_r', 5.0)
                nearby_objects = spatial_hash.query_ball(
                    bacterium.x, bacterium.y, bacterium.r + food_radius
                )
                nearby_foods = [obj for obj in nearby_objects 
                              if hasattr(obj, 'mass') and not hasattr(obj, 'network')]
            else:
                # Fallback: busca linear
                nearby_foods = foods
            
            for food in nearby_foods:
                if food in self._foods_to_remove:
                    continue
                
                # Verifica colisão
                distance = math.hypot(bacterium.x - food.x, bacterium.y - food.y)
                if distance <= bacterium.r + food.r:
                    # Bactéria come comida
                    bacterium.set_mass(bacterium.m + food.mass)
                    self._foods_to_remove.add(food)
                    break  # Uma comida por frame por bactéria
    
    def _predators_eat_bacteria(self, predators: List['Predator'], 
                               bacteria: List['Bacteria'], spatial_hash: 'SpatialHash', 
                               params: 'Params'):
        """Processa predadores comendo bactérias."""
        for predator in predators:
            if spatial_hash:
                # Usa spatial hash
                bacteria_radius = params.get('bacteria_max_r', 12.0)
                nearby_objects = spatial_hash.query_ball(
                    predator.x, predator.y, predator.r + bacteria_radius
                )
                nearby_bacteria = [obj for obj in nearby_objects 
                                 if hasattr(obj, 'is_predator') and not obj.is_predator]
            else:
                # Fallback
                nearby_bacteria = bacteria
            
            # Se já estamos no mínimo de bactérias permitido, impedir predação adicional
            min_bact = params.get('bacteria_min_limit', 0)
            if len(bacteria) - len([a for a in self._agents_to_remove if hasattr(a, 'is_predator') and not a.is_predator]) <= min_bact:
                continue

            for bacterium in nearby_bacteria:
                if bacterium in self._agents_to_remove:
                    continue
                
                # Verifica colisão
                distance = math.hypot(predator.x - bacterium.x, predator.y - bacterium.y)
                if distance <= predator.r + bacterium.r:
                    # Predador come bactéria
                    predator.m += bacterium.m * 0.7  # Eficiência 70%
                    # Rechecar mínimo antes de remover
                    if len(bacteria) - len([a for a in self._agents_to_remove if hasattr(a, 'is_predator') and not a.is_predator]) <= min_bact:
                        break
                    self._agents_to_remove.add(bacterium)
                    break  # Uma bactéria por frame por predador


class ReproductionSystem:
    """
    Sistema de reprodução para agentes.
    Aplica regras de reprodução assexuada com mutação.
    """
    
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
        
        # Determina limites de população por tipo
        bacteria_count = sum(1 for a in agents if not getattr(a, 'is_predator', False))
        predator_count = sum(1 for a in agents if getattr(a, 'is_predator', False))
        
        bacteria_max = params.get('bacteria_max_limit', 300)
        predator_max = params.get('predator_max_limit', 100)
        
        for agent in agents:
            # Verifica se pode se reproduzir
            if not agent.can_reproduce(params):
                continue
            
            # Verifica limites de população
            is_predator = getattr(agent, 'is_predator', False)
            if is_predator:
                if predator_count + len([a for a in new_agents if getattr(a, 'is_predator', False)]) >= predator_max:
                    continue
            else:
                if bacteria_count + len([a for a in new_agents if not getattr(a, 'is_predator', False)]) >= bacteria_max:
                    continue
            
            # Cria filho
            try:
                child = agent.reproduce(params)
                new_agents.append(child)
                
                # Atualiza contadores
                if is_predator:
                    predator_count += 1
                else:
                    bacteria_count += 1
                    
            except Exception as e:
                print(f"Erro na reprodução: {e}")
                continue
        
        return new_agents


class DeathSystem:
    """
    Sistema de morte controlada para agentes.
    
    Implementa morte limitada por frame e respeita limites mínimos de população.
    """
    
    def __init__(self, max_deaths_per_step: int = 1):
        self.max_deaths_per_step = max_deaths_per_step
        self._death_queue = []  # Fila de agentes marcados para morrer
    
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
        # Processa morte de bactérias
        bacteria_survivors = self._process_deaths(
            bacteria, 
            params.get('bacteria_min_limit', 10),
            params.get('bacteria_death_mass', 50.0),
            params
        )
        
        # Processa morte de predadores
        predator_survivors = self._process_deaths(
            predators,
            params.get('predator_min_limit', 0), 
            params.get('predator_death_mass', 50.0),
            params
        )
        
        return bacteria_survivors, predator_survivors
    
    def _process_deaths(self, agents: List['Agent'], min_limit: int, 
                       death_mass: float, params: 'Params') -> List['Agent']:
        """
        Processa morte de um tipo específico de agente.
        """
        if not agents:
            return agents
        
        current_count = len(agents)
        
        # Identifica candidatos à morte
        death_candidates = [agent for agent in agents if agent.should_die(params)]
        
        # Se estamos no limite mínimo, não mata ninguém
        if current_count <= min_limit:
            # Clamp massa para evitar morte
            for agent in death_candidates:
                agent.set_mass(max(agent.m, death_mass))
            return agents
        
        # Calcula quantas mortes são permitidas
        deaths_available = min(
            current_count - min_limit,  # Não pode ficar abaixo do mínimo
            self.max_deaths_per_step,   # Limite por frame
            len(death_candidates)       # Não pode matar mais que os candidatos
        )
        
        if deaths_available <= 0:
            # Clamp massa dos candidatos
            for agent in death_candidates:
                agent.set_mass(max(agent.m, death_mass))
            return agents
        
        # Seleciona agentes para morrer (os com menor massa primeiro)
        death_candidates.sort(key=lambda a: a.m)
        agents_to_kill = death_candidates[:deaths_available]
        
        # Clamp massa dos que não vão morrer
        for agent in death_candidates[deaths_available:]:
            agent.set_mass(max(agent.m, death_mass))
        
        # Retorna sobreviventes
        survivors = [agent for agent in agents if agent not in agents_to_kill]
        
        return survivors


class CollisionSystem:
    """
    Sistema de resolução de colisões entre agentes.
    """
    
    def __init__(self):
        self._processed_pairs = set()
    
    def apply(self, agents: List['Agent'], spatial_hash: 'SpatialHash', params: 'Params'):
        """
        Resolve colisões entre agentes.
        
        Args:
            agents: Lista de todos os agentes
            spatial_hash: Hash espacial para otimização
            params: Parâmetros da simulação
        """
        self._processed_pairs.clear()
        
        if spatial_hash:
            self._resolve_with_spatial_hash(agents, spatial_hash)
        else:
            self._resolve_brute_force(agents)
    
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
                
                self._resolve_collision_pair(agent, other)
    
    def _resolve_brute_force(self, agents: List['Agent']):
        """Resolve colisões com busca bruta (para populações pequenas)."""
        n = len(agents)
        for i in range(n):
            for j in range(i + 1, n):
                self._resolve_collision_pair(agents[i], agents[j])
    
    def _resolve_collision_pair(self, agent1: 'Agent', agent2: 'Agent'):
        """
        Resolve colisão entre dois agentes específicos.
        Implementa resposta elástica simples.
        """
        dx = agent1.x - agent2.x
        dy = agent1.y - agent2.y
        distance = math.hypot(dx, dy)
        
        if distance == 0:
            # Evita divisão por zero
            distance = 0.01
            dx = 0.01
            dy = 0
        
        overlap = agent1.r + agent2.r - distance
        if overlap <= 0:
            return  # Sem colisão
        
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
        nx = dx / distance  # Normal x
        ny = dy / distance  # Normal y
        
        # Velocidade relativa
        dvx = agent1.vx - agent2.vx
        dvy = agent1.vy - agent2.vy
        relative_velocity_normal = dvx * nx + dvy * ny
        
        if relative_velocity_normal > 0:
            return  # Objetos se afastando
        
        # Impulso elástico
        impulse = (2 * relative_velocity_normal) / total_mass
        impulse_x = impulse * nx
        impulse_y = impulse * ny
        
        agent1.vx -= impulse_x * agent2.m
        agent1.vy -= impulse_y * agent2.m
        agent2.vx += impulse_x * agent1.m
        agent2.vy += impulse_y * agent1.m
