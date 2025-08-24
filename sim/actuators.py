"""
Componentes atuadores: locomoção e modelo energético.
"""
import math
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .entities import Agent
    from .world import World
    from .controllers import Params


class Locomotion:
    """
    Sistema de locomoção para agentes.
    Interpreta comandos da rede neural e atualiza posição/velocidade.
    """
    
    def __init__(self, max_speed: float = 300.0, max_turn: float = math.pi):
        self.max_speed = max_speed
        self.max_turn = max_turn  # radianos/segundo
    
    def step(self, agent: 'Agent', control_output: list, dt: float, world: 'World', params: 'Params'):
        """
        Atualiza posição e velocidade do agente baseado na saída do cérebro.
        
        Args:
            agent: Agente a ser controlado
            control_output: Saída da rede neural [speed_cmd, steer_cmd]
            dt: Delta tempo físico
            world: Mundo físico
            params: Parâmetros da simulação
        """
        if len(control_output) < 2:
            return
        
        # Interpreta comandos
        speed_raw = control_output[0]
        steer_raw = control_output[1]
        
        # Normaliza comandos
        speed_cmd = self._sigmoid(speed_raw)  # 0..1
        steer_cmd = math.tanh(steer_raw)      # -1..1
        
        # Aplica velocidade desejada
        desired_speed = speed_cmd * self.max_speed
        
        # Atualiza orientação (steering)
        agent.angle += steer_cmd * self.max_turn * dt
        agent.angle = self._normalize_angle(agent.angle)
        
        # Velocidade instantânea comandada
        agent.vx = math.cos(agent.angle) * desired_speed
        agent.vy = math.sin(agent.angle) * desired_speed
        
        # Move agente
        agent.x += agent.vx * dt
        agent.y += agent.vy * dt
        
        # Colisão com paredes (wall bounce)
        self._handle_wall_collisions(agent, world)
    
    def _sigmoid(self, x: float) -> float:
        """Função sigmoid para normalizar speed command."""
        try:
            return 1.0 / (1.0 + math.exp(-x))
        except OverflowError:
            return 0.0 if x < 0 else 1.0
    
    def _normalize_angle(self, angle: float) -> float:
        """Normaliza ângulo para [-π, π]."""
        while angle > math.pi:
            angle -= 2 * math.pi
        while angle < -math.pi:
            angle += 2 * math.pi
        return angle
    
    def _handle_wall_collisions(self, agent: 'Agent', world: 'World'):
        """Trata colisões com limites do mundo (circular ou retangular)."""
        if world.shape == 'circular':
            dx = agent.x - world.cx
            dy = agent.y - world.cy
            dist = math.hypot(dx, dy)
            max_dist = max(1e-6, world.radius - agent.r)
            if dist > max_dist:
                nx = dx / dist
                ny = dy / dist
                agent.x = world.cx + nx * max_dist
                agent.y = world.cy + ny * max_dist
                vrad = agent.vx * nx + agent.vy * ny
                agent.vx -= 1.5 * vrad * nx
                agent.vy -= 1.5 * vrad * ny
        else:
            if agent.x - agent.r < 0:
                agent.x = agent.r
                agent.vx *= -0.5
            elif agent.x + agent.r > world.width:
                agent.x = world.width - agent.r
                agent.vx *= -0.5
            if agent.y - agent.r < 0:
                agent.y = agent.r
                agent.vy *= -0.5
            elif agent.y + agent.r > world.height:
                agent.y = world.height - agent.r
                agent.vy *= -0.5


class EnergyModel:
    """
    Sistema de energia para agentes.
    Gerencia consumo energético e limites de vida/reprodução.
    """
    
    def __init__(self, loss_idle: float = 0.01, loss_move: float = 5.0,
                 death_mass: float = 50.0, split_mass: float = 150.0):
        self.loss_idle = loss_idle      # Perda por segundo parado
        self.loss_move = loss_move      # Perda por segundo movendo
        self.death_mass = death_mass    # Massa mínima para viver
        self.split_mass = split_mass    # Massa para reproduzir
    
    def apply(self, agent: 'Agent', dt: float, params: 'Params'):
        """
        Aplica modelo energético ao agente.
        
        IMPORTANTE: Perdas são lineares no tempo simulado, não no FPS.
        
        Args:
            agent: Agente afetado
            dt: Delta tempo FÍSICO (já com time_scale aplicado)
            params: Parâmetros da simulação
        """
        # Determina tipo de perda baseado na velocidade
        speed = agent.speed()
        if speed > 1.0:
            energy_loss = self.loss_move * dt
        else:
            energy_loss = self.loss_idle * dt
        
        # Aplica perda energética (diminui massa)
        new_mass = agent.m - energy_loss
        agent.set_mass(max(new_mass, self.death_mass))
    
    def should_die(self, agent: 'Agent') -> bool:
        """Verifica se agente deve morrer por falta de energia."""
        return agent.m <= self.death_mass
    
    def can_reproduce(self, agent: 'Agent') -> bool:
        """Verifica se agente pode se reproduzir."""
        return agent.m >= self.split_mass
    
    def prepare_reproduction(self, agent: 'Agent') -> float:
        """
        Prepara reprodução dividindo massa do agente.
        
        Returns:
            Massa do filho
        """
        child_mass = agent.m / 2.0
        agent.set_mass(child_mass)
        return child_mass
