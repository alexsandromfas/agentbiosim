"""
Componentes atuadores: locomoção e modelo energético.
"""
import math
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .entities import Agent
    from .world import World
    from .controllers import Params


MOVEMENT_MODE_FORWARD = "forward"
MOVEMENT_MODE_OMNI = "omni"
BODY_SHAPE_ELLIPSE = "ellipse"
BODY_SHAPE_CIRCLE = "circle"


def normalize_movement_mode(value) -> str:
    if isinstance(value, str):
        text = value.strip().lower()
        if text in {MOVEMENT_MODE_FORWARD, "frontal", "frente"}:
            return MOVEMENT_MODE_FORWARD
        if text in {MOVEMENT_MODE_OMNI, "4way", "four_way", "quatro_direcoes", "quatro direcoes"}:
            return MOVEMENT_MODE_OMNI
    return MOVEMENT_MODE_FORWARD


def locomotion_output_size(value) -> int:
    return 3 if normalize_movement_mode(value) == MOVEMENT_MODE_OMNI else 2


def normalize_body_shape(value) -> str:
    if isinstance(value, str):
        text = value.strip().lower()
        if text in {BODY_SHAPE_CIRCLE, "circular", "circulo", "círculo"}:
            return BODY_SHAPE_CIRCLE
    return BODY_SHAPE_ELLIPSE


class Locomotion:
    """
    Sistema de locomoção para agentes.
    Interpreta comandos da rede neural e atualiza posição/velocidade.
    """
    
    def __init__(self, max_speed: float = 300.0, max_turn: float = math.pi,
                 allow_reverse: bool = False, movement_mode: str = MOVEMENT_MODE_FORWARD,
                 body_shape: str = BODY_SHAPE_ELLIPSE):
        self.max_speed = max_speed
        self.max_turn = max_turn  # radianos/segundo
        self.allow_reverse = bool(allow_reverse)
        self.movement_mode = normalize_movement_mode(movement_mode)
        self.body_shape = normalize_body_shape(body_shape)
    
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
        if bool(params.get('render_interpolation_enabled', False)):
            self._capture_render_pose(agent)
        if bool(params.get('smooth_locomotion_enabled', False)):
            self._smooth_step(agent, control_output, dt, world, params)
            return

        if normalize_movement_mode(getattr(self, "movement_mode", MOVEMENT_MODE_FORWARD)) == MOVEMENT_MODE_OMNI:
            if len(control_output) < 3:
                return
            forward_cmd = math.tanh(float(control_output[0]))
            strafe_cmd = math.tanh(float(control_output[1]))
            steer_cmd = math.tanh(float(control_output[2]))
            mag = math.hypot(forward_cmd, strafe_cmd)
            if mag > 1.0:
                forward_cmd /= mag
                strafe_cmd /= mag

            agent.angle += steer_cmd * self.max_turn * dt
            agent.angle = self._normalize_angle(agent.angle)
            ca = math.cos(agent.angle)
            sa = math.sin(agent.angle)
            desired_vx = (ca * forward_cmd - sa * strafe_cmd) * self.max_speed
            desired_vy = (sa * forward_cmd + ca * strafe_cmd) * self.max_speed

            inertia = max(0.0, float(params.get('agents_inertia', 1.0)))
            if inertia <= 1.0:
                agent.vx = desired_vx
                agent.vy = desired_vy
            else:
                alpha = min(1.0, 1.0 / inertia)
                agent.vx += (desired_vx - agent.vx) * alpha
                agent.vy += (desired_vy - agent.vy) * alpha
            agent.x += agent.vx * dt
            agent.y += agent.vy * dt
            self._handle_wall_collisions(agent, world)
            return
        
        # Interpreta comandos
        speed_raw = control_output[0]
        steer_raw = control_output[1]
        
        # Normaliza comandos. Por padrao preserva o modelo antigo 0..1.
        # Quando habilitado explicitamente, tanh permite velocidade assinada.
        if bool(getattr(self, "allow_reverse", False)):
            speed_cmd = math.tanh(speed_raw)   # -1..1
        else:
            speed_cmd = self._sigmoid(speed_raw)  # 0..1
        steer_cmd = math.tanh(steer_raw)       # -1..1
        
        # Aplica velocidade desejada
        desired_speed = speed_cmd * self.max_speed
        
        # Atualiza orientação (steering)
        agent.angle += steer_cmd * self.max_turn * dt
        agent.angle = self._normalize_angle(agent.angle)
        
        desired_vx = math.cos(agent.angle) * desired_speed
        desired_vy = math.sin(agent.angle) * desired_speed

        inertia = max(0.0, float(params.get('agents_inertia', 1.0)))
        if inertia <= 1.0:
            agent.vx = desired_vx
            agent.vy = desired_vy
        else:
            alpha = min(1.0, 1.0 / inertia)
            agent.vx += (desired_vx - agent.vx) * alpha
            agent.vy += (desired_vy - agent.vy) * alpha
        
        # Move agente
        agent.x += agent.vx * dt
        agent.y += agent.vy * dt
        
        # Colisão com paredes (wall bounce)
        self._handle_wall_collisions(agent, world)
    
    @staticmethod
    def _capture_render_pose(agent: 'Agent'):
        agent.prev_x = float(getattr(agent, 'x', 0.0))
        agent.prev_y = float(getattr(agent, 'y', 0.0))
        agent.prev_angle = float(getattr(agent, 'angle', 0.0))

    def _smooth_step(self, agent: 'Agent', control_output: list, dt: float,
                     world: 'World', params: 'Params'):
        """Atuador com aceleracao limitada e arrastos opcionais."""
        movement_mode = normalize_movement_mode(getattr(self, "movement_mode", MOVEMENT_MODE_FORWARD))
        if movement_mode == MOVEMENT_MODE_OMNI:
            if len(control_output) < 3:
                return
            forward_cmd = math.tanh(float(control_output[0]))
            strafe_cmd = math.tanh(float(control_output[1]))
            steer_cmd = math.tanh(float(control_output[2]))
            magnitude = math.hypot(forward_cmd, strafe_cmd)
            if magnitude > 1.0:
                forward_cmd /= magnitude
                strafe_cmd /= magnitude
        else:
            speed_raw = float(control_output[0])
            if bool(getattr(self, "allow_reverse", False)):
                forward_cmd = math.tanh(speed_raw)
            else:
                forward_cmd = self._sigmoid(speed_raw)
            strafe_cmd = 0.0
            steer_cmd = math.tanh(float(control_output[1]))

        self._update_smooth_rotation(agent, steer_cmd, dt, params)
        ca = math.cos(agent.angle)
        sa = math.sin(agent.angle)
        desired_vx = (ca * forward_cmd - sa * strafe_cmd) * self.max_speed
        desired_vy = (sa * forward_cmd + ca * strafe_cmd) * self.max_speed
        self._update_smooth_velocity(agent, desired_vx, desired_vy, dt, params)
        agent.x += agent.vx * dt
        agent.y += agent.vy * dt
        self._handle_wall_collisions(agent, world)

    def _update_smooth_rotation(self, agent: 'Agent', steer_cmd: float, dt: float,
                                params: 'Params'):
        target_omega = steer_cmd * self.max_turn
        omega = float(getattr(agent, 'angular_velocity', 0.0))
        if bool(params.get('smooth_angular_inertia_enabled', True)):
            max_accel = max(0.0, float(params.get('smooth_max_angular_accel', math.pi * 4.0)))
            max_delta = max_accel * max(0.0, dt)
            delta = target_omega - omega
            if delta > max_delta:
                delta = max_delta
            elif delta < -max_delta:
                delta = -max_delta
            omega += delta
        else:
            omega = target_omega
        if bool(params.get('smooth_angular_drag_enabled', True)):
            omega *= self._drag_decay(float(params.get('smooth_angular_drag', 1.5)), dt)
        if omega > self.max_turn:
            omega = self.max_turn
        elif omega < -self.max_turn:
            omega = -self.max_turn
        agent.angular_velocity = omega
        agent.angle = self._normalize_angle(agent.angle + omega * dt)

    def _update_smooth_velocity(self, agent: 'Agent', desired_vx: float, desired_vy: float,
                                dt: float, params: 'Params'):
        vx = float(getattr(agent, 'vx', 0.0))
        vy = float(getattr(agent, 'vy', 0.0))
        if bool(params.get('smooth_linear_inertia_enabled', True)):
            dx = desired_vx - vx
            dy = desired_vy - vy
            delta = math.hypot(dx, dy)
            max_delta = max(0.0, float(params.get('smooth_max_linear_accel', 900.0))) * max(0.0, dt)
            if delta > max_delta and delta > 1e-12:
                scale = max_delta / delta
                dx *= scale
                dy *= scale
            vx += dx
            vy += dy
        else:
            vx = desired_vx
            vy = desired_vy
        if bool(params.get('smooth_linear_drag_enabled', True)):
            decay = self._drag_decay(float(params.get('smooth_linear_drag', 0.75)), dt)
            vx *= decay
            vy *= decay
        agent.vx = vx
        agent.vy = vy

    @staticmethod
    def _drag_decay(drag: float, dt: float) -> float:
        drag = max(0.0, drag)
        if drag <= 0.0 or dt <= 0.0:
            return 1.0
        return math.exp(-drag * dt)

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
    """Modelo energético contínuo baseado na velocidade.

    cost(v) = v0_cost + ( clamp(v,0,vmax_ref) / vmax_ref ) * (vmax_cost - v0_cost)

    Campos legacy (loss_idle/loss_move) removidos – agora somente curva contínua.
    """

    __slots__ = (
        "death_energy", "split_energy", "v0_cost", "vmax_cost", "vmax_ref", "energy_cap",
        "age_death_enabled", "death_age", "corpse_to_food",
        "reproduction_min_age", "reproduction_cooldown",
    )

    def __init__(self, *, death_energy: float = 0.0, split_energy: float = 150.0,
                 v0_cost: float = 0.5, vmax_cost: float = 8.0, vmax_ref: float = 300.0,
                 energy_cap: float = 400.0, age_death_enabled: bool = False,
                 death_age: float = 0.0, corpse_to_food: bool = False,
                 reproduction_min_age: float = 0.0, reproduction_cooldown: float = 0.0):
        self.death_energy = death_energy
        self.split_energy = split_energy
        self.v0_cost = v0_cost
        self.vmax_cost = vmax_cost
        self.vmax_ref = max(1e-6, vmax_ref)
        self.energy_cap = energy_cap
        self.age_death_enabled = bool(age_death_enabled)
        self.death_age = max(0.0, float(death_age))
        self.corpse_to_food = bool(corpse_to_food)
        self.reproduction_min_age = max(0.0, float(reproduction_min_age))
        self.reproduction_cooldown = max(0.0, float(reproduction_cooldown))

    def metabolic_cost_per_sec(self, speed: float) -> float:
        # Linear por enquanto; speed saturado em vmax_ref
        s = max(0.0, min(speed, self.vmax_ref)) / self.vmax_ref
        return self.v0_cost + s * (self.vmax_cost - self.v0_cost)

    def apply(self, agent: 'Agent', dt: float, params: 'Params'):
        speed = agent.speed()
        # Metabolismo agora e regra do organismo/label, aplicado no EnergyModel.
        params = {}
        # Permitir override per-tipo via params (dinâmico)
        if agent.is_predator:
            v0 = params.get('predator_metab_v0_cost', self.v0_cost)
            vmaxc = params.get('predator_metab_vmax_cost', self.vmax_cost)
            vmaxr = params.get('predator_max_speed', getattr(agent.locomotion, 'max_speed', self.vmax_ref))
            cap = params.get('predator_energy_cap', self.energy_cap)
        else:
            v0 = params.get('bacteria_metab_v0_cost', self.v0_cost)
            vmaxc = params.get('bacteria_metab_vmax_cost', self.vmax_cost)
            vmaxr = params.get('bacteria_max_speed', getattr(agent.locomotion, 'max_speed', self.vmax_ref))
            cap = params.get('bacteria_energy_cap', self.energy_cap)
        # atualização dinâmica dos campos (mantém introspecção consistente)
        self.v0_cost = v0; self.vmax_cost = vmaxc; self.vmax_ref = max(1e-6, vmaxr); self.energy_cap = cap
        cost_sec = self.metabolic_cost_per_sec(speed)
        energy_loss = cost_sec * dt
        agent.energy = max(0.0, agent.energy - energy_loss)
        # Cap de armazenamento (aplicado após ganhos externos em outro lugar; reforço aqui por segurança)
        if agent.energy > self.energy_cap:
            agent.energy = self.energy_cap

    # --- Queries ---
    def should_die(self, agent: 'Agent') -> bool:
        if self.age_death_enabled and self.death_age > 0.0 and getattr(agent, 'age', 0.0) >= self.death_age:
            return True
        return agent.energy <= self.death_energy

    def can_reproduce(self, agent: 'Agent') -> bool:
        return agent.energy >= self.split_energy

    def prepare_reproduction(self, agent: 'Agent') -> float:
        """Divide energia do agente em dois e retorna energia do filho."""
        child_energy = agent.energy * 0.5
        agent.energy = child_energy
        return child_energy
