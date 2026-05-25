"""
Estratégias de renderização para visualização da simulação.
"""
import math
import pygame
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .entities import Agent, Food
    from .obstacles import ObstacleMap
    from .world import Camera


def _draw_food_bite_holes(food: 'Food', surface: pygame.Surface, camera: 'Camera'):
    holes = getattr(food, 'bite_holes', None)
    if not holes:
        return
    for hx, hy, hr in holes:
        sx, sy = camera.world_to_screen(float(hx), float(hy))
        sr = max(1, int(float(hr) * camera.zoom))
        x = max(0, min(surface.get_width() - 1, int(sx)))
        y = max(0, min(surface.get_height() - 1, int(sy)))
        fill = surface.get_at((x, y))[:3]
        pygame.draw.circle(surface, fill, (int(sx), int(sy)), sr)
        if sr >= 3:
            pygame.draw.circle(surface, (35, 25, 20), (int(sx), int(sy)), sr, width=1)


def _interpolated_agent_pose(agent: 'Agent', alpha: float) -> tuple[float, float, float]:
    """Retorna pose renderizada entre o substep anterior e o atual."""
    if alpha >= 1.0:
        return float(agent.x), float(agent.y), float(agent.angle)
    prev_x = float(getattr(agent, 'prev_x', agent.x))
    prev_y = float(getattr(agent, 'prev_y', agent.y))
    prev_angle = float(getattr(agent, 'prev_angle', agent.angle))
    x = prev_x + (float(agent.x) - prev_x) * alpha
    y = prev_y + (float(agent.y) - prev_y) * alpha
    delta_angle = (float(agent.angle) - prev_angle + math.pi) % (2.0 * math.pi) - math.pi
    return x, y, prev_angle + delta_angle * alpha


def _interpolated_food_pos(food: 'Food', alpha: float) -> tuple[float, float]:
    if alpha >= 1.0:
        return float(food.x), float(food.y)
    prev_x = float(getattr(food, 'prev_x', food.x))
    prev_y = float(getattr(food, 'prev_y', food.y))
    return (
        prev_x + (float(food.x) - prev_x) * alpha,
        prev_y + (float(food.y) - prev_y) * alpha,
    )


class RendererStrategy(ABC):
    """
    Interface para estratégias de renderização.
    Permite trocar entre renderização rápida e bonita.
    """
    
    @abstractmethod
    def draw_agent(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera',
                   show_head: bool = True, show_vision: bool = False, selected: bool = False):
        """Desenha um agente na superfície."""
        pass
    
    @abstractmethod  
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida na superfície."""
        pass

    @abstractmethod
    def draw_obstacles(self, obstacles: 'ObstacleMap', surface: pygame.Surface, camera: 'Camera',
                       visible_bounds=None):
        """Desenha obstáculos sólidos."""
        pass
    
    @abstractmethod
    def draw_overlay(self, surface: pygame.Surface, info: dict):
        """Desenha informações de overlay (FPS, contadores, etc)."""
        pass


class SimpleRenderer(RendererStrategy):
    """
    Renderização simples e rápida.
    Usa círculos simples em vez de elipses rotacionadas.
    """
    
    def __init__(self):
        # Inicializa fonts se necessário
        if not pygame.font.get_init():
            pygame.font.init()
        self.font = pygame.font.SysFont(None, 18)
        self.small_font = pygame.font.SysFont(None, 14)
    
    def draw_agent(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera',
                   show_head: bool = True, show_vision: bool = False, selected: bool = False):
        """Desenha agente como círculo simples."""
        # Converte posição para tela
        pose_x, pose_y, pose_angle = _interpolated_agent_pose(
            agent, float(getattr(self, 'interpolation_alpha', 1.0))
        )
        screen_x, screen_y = camera.world_to_screen(pose_x, pose_y)
        screen_radius = max(1, int(agent.r * camera.zoom))
        
        # Cor do corpo
        if selected:
            color = (100, 220, 100)  # Verde quando selecionado
        else:
            color = agent.color
        # Desenha corpo
        pygame.draw.circle(surface, color, (int(screen_x), int(screen_y)), screen_radius)
        
        # Desenha "cabeça" (ponto na frente)
        if show_head:
            head_radius = max(1, int(agent.r * 0.25 * camera.zoom))
            sensor = getattr(agent, 'sensor', None)
            eye_count = 1
            if sensor is not None:
                try:
                    eye_count = 2 if int(getattr(sensor, 'eye_count', 1)) >= 2 else 1
                except Exception:
                    eye_count = 1
            if eye_count == 2 and sensor is not None and hasattr(sensor, '_eye_specs'):
                for pos_offset, _gaze_offset in sensor._eye_specs():
                    eye_angle = pose_angle + pos_offset
                    head_world_x = pose_x + math.cos(eye_angle) * agent.r
                    head_world_y = pose_y + math.sin(eye_angle) * agent.r
                    head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
                    pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
            else:
                head_world_x = pose_x + math.cos(pose_angle) * agent.r
                head_world_y = pose_y + math.sin(pose_angle) * agent.r
                head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
                pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha raios de visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera, selected=selected)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo simples."""
        fx, fy = _interpolated_food_pos(food, float(getattr(self, 'interpolation_alpha', 1.0)))
        screen_x, screen_y = camera.world_to_screen(fx, fy)
        screen_radius = max(1, int(food.r * camera.zoom))
        pygame.draw.circle(surface, food.color, (int(screen_x), int(screen_y)), screen_radius)
        _draw_food_bite_holes(food, surface, camera)
        if getattr(food, 'kind', 'instant') == 'chunk' and screen_radius >= 3:
            pygame.draw.circle(surface, (35, 25, 20), (int(screen_x), int(screen_y)), screen_radius, width=1)

    def draw_obstacles(self, obstacles: 'ObstacleMap', surface: pygame.Surface, camera: 'Camera',
                       visible_bounds=None):
        """Desenha barreiras criadas com o pincel."""
        if not getattr(obstacles, 'has_obstacles', False):
            return
        for stamp in obstacles.stamps:
            if visible_bounds is not None:
                min_x, min_y, max_x, max_y = visible_bounds
                if (stamp.x + stamp.r < min_x or stamp.x - stamp.r > max_x or
                        stamp.y + stamp.r < min_y or stamp.y - stamp.r > max_y):
                    continue
            screen_x, screen_y = camera.world_to_screen(stamp.x, stamp.y)
            screen_radius = max(1, int(stamp.r * camera.zoom))
            pygame.draw.circle(surface, stamp.color, (int(screen_x), int(screen_y)), screen_radius)
    
    def draw_overlay(self, surface: pygame.Surface, info: dict):
        """Desenha informações de overlay."""
        if info.get('hide_overlay', False):
            return
        self._draw_sim_hud(surface, info)
        return
        bacteria_count = info.get('bacteria_count', 0)
        predator_count = info.get('predator_count', 0)
        organism_count = info.get('organism_count', bacteria_count + predator_count)
        food_count = info.get('food_count', 0)
        food_target = info.get('food_target', 0)
        fps = info.get('fps', 0)
        cpu_percent = info.get('cpu_percent')
        cpu_proc_percent = info.get('cpu_proc_percent')
        mem_used_mb = info.get('mem_used_mb')
        resources_available = info.get('resources_available', True)
        fallback_metrics = info.get('fallback_metrics', False)

        obstacle_count = info.get('obstacle_count', 0)
        main_info = f"Bactérias: {bacteria_count}  |  Predadores: {predator_count}  |  Comida: {food_count}  |  Target: {food_target}  |  Obstáculos: {obstacle_count}  |  FPS: {int(fps)}"
        main_info = f"Organismos: {organism_count}  |  Comida: {food_count}  |  Target: {food_target}  |  Obstaculos: {obstacle_count}  |  FPS: {int(fps)}"
        if cpu_percent is not None and mem_used_mb is not None:
            try:
                if resources_available:
                    if cpu_proc_percent is not None:
                        main_info += f"  |  CPU: {cpu_percent:4.1f}% (proc {cpu_proc_percent:4.1f}%)  |  RAM: {mem_used_mb:.0f} MB"
                    else:
                        main_info += f"  |  CPU: {cpu_percent:4.1f}%  |  RAM: {mem_used_mb:.0f} MB"
                else:
                    if fallback_metrics and (cpu_percent > 0 or mem_used_mb > 0):
                        main_info += f"  |  CPU~: {cpu_percent:4.1f}%  |  RAM~: {mem_used_mb:.0f} MB"
                    else:
                        main_info += "  |  CPU: N/A  |  RAM: N/A"
            except Exception:
                pass

        surface.blit(self.font.render(main_info, True, (220, 220, 220)), (8, 8))

        max_speed = info.get('max_speed', 0)
        time_scale = info.get('time_scale', 1.0)
        world_w = info.get('world_w', 0)
        world_h = info.get('world_h', 0)
        secondary_info = f"Max speed: {max_speed:.1f}  |  Time x: {time_scale:.2f}  |  World: {world_w:.0f}x{world_h:.0f}"
        surface.blit(self.font.render(secondary_info, True, (180, 180, 220)), (8, 28))

        # Aviso se métricas indisponíveis
        if not resources_available:
            hint = "Instale psutil para métricas: pip install psutil"
            surf_hint = self.small_font.render(hint, True, (160, 120, 120))
            surface.blit(surf_hint, (8, 46))

        selected_agent = info.get('selected_agent')
        if selected_agent and info.get('show_selected_details', True):
            self._draw_agent_details(selected_agent, surface)
    
    def _draw_sim_hud(self, surface: pygame.Surface, info: dict):
        organisms = int(info.get('organism_count', info.get('bacteria_count', 0) + info.get('predator_count', 0)))
        physics_now = float(info.get('physics_steps_per_wall_second', 0.0) or 0.0)
        physics_target = float(info.get('physics_target_hz', 0.0) or 0.0)
        backlog = float(info.get('simulation_backlog', 0.0) or 0.0)
        dropped = float(info.get('dropped_simulation_time', 0.0) or 0.0)
        lines = [
            f"Organismos  {organisms}",
            f"Comida      {int(info.get('food_count', 0))}",
            f"Alvo comida {int(info.get('food_target', 0))}",
            f"Obstaculos  {int(info.get('obstacle_count', 0))}",
            f"FPS         {int(info.get('fps', 0))}",
            f"Tempo       {float(info.get('time_scale', 1.0)):.2f}x",
            f"Efetivo     {float(info.get('effective_time_scale', 0.0)):.2f}x",
            f"Fisica      {physics_now:.0f}/{physics_target:.0f} Hz",
            f"Atraso      {backlog:.2f}s" if dropped <= 0 else f"Atraso      {backlog:.2f}s drop {dropped:.1f}s",
        ]
        if str(info.get('world_shape', 'rectangular')) == 'circular':
            lines.append(f"Mundo       circular r {float(info.get('world_radius', 0.0)):.0f}")
        else:
            lines.append(f"Mundo       {float(info.get('world_w', 0.0)):.0f}x{float(info.get('world_h', 0.0)):.0f}")
        if info.get('resources_available', True):
            proc_cpu = info.get('cpu_proc_percent')
            ram = info.get('mem_used_mb')
            if proc_cpu is not None:
                lines.append(f"CPU proc    {float(proc_cpu):.0f}%")
            if ram is not None:
                lines.append(f"RAM         {float(ram):.0f} MB")
        else:
            lines.append("CPU/RAM     N/A")
        rendered = [self.small_font.render(line, True, (222, 232, 242)) for line in lines]
        pad, top, line_gap = 10, 9, 16
        width = max((item.get_width() for item in rendered), default=0)
        x = max(pad, surface.get_width() - width - pad)
        for row, item in enumerate(rendered):
            y = top + row * line_gap
            surface.blit(self.small_font.render(lines[row], True, (6, 10, 14)), (x + 1, y + 1))
            surface.blit(item, (x, y))

    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera', selected: bool = False):
        """Desenha raios de visão com intensidade proporcional à ativação."""
        if not hasattr(agent, 'sensor'):
            return
        
        sensor = agent.sensor
        if str(getattr(sensor, 'last_vision_mode', 'single')) == 'sector':
            self._draw_vision_bins(agent, surface, camera, selected=selected)
            return
        ray_count = sensor.total_ray_count() if hasattr(sensor, 'total_ray_count') else int(getattr(sensor, 'retina_count', len(sensor.last_inputs)))
        for i in range(int(ray_count)):
            ray_info = sensor.get_ray_info(agent, i)
            if ray_info is None:
                continue
            
            start_x, start_y, end_x, end_y, activation = ray_info
            
            if activation > 0.001:
                # Cor baseada na proximidade: vermelho (perto) -> amarelo (longe)
                red_intensity = min(255, int(200 + 55 * activation))
                green_intensity = min(255, int(100 + 100 * (1.0 - activation)))
                color = (red_intensity, green_intensity, 0)
                thickness = max(1, int(1 + 2 * activation))
            else:
                color = (60, 60, 100)
                thickness = 1
            
            # Converte para coordenadas da tela
            screen_start = camera.world_to_screen(start_x, start_y)
            screen_end = camera.world_to_screen(end_x, end_y)
            
            # Desenha linha
            pygame.draw.line(surface, color, 
                           (int(screen_start[0]), int(screen_start[1])),
                           (int(screen_end[0]), int(screen_end[1])), thickness)

    def _ray_activation(self, sensor, ray_index: int) -> float:
        distance_values = getattr(sensor, 'last_distance_inputs', None)
        if distance_values and ray_index < len(distance_values):
            try:
                return max(0.0, min(1.0, float(distance_values[ray_index])))
            except Exception:
                return 0.0
        stride = max(1, len(getattr(sensor, 'channels', ('d',))))
        start = ray_index * stride
        values = getattr(sensor, 'last_inputs', [])[start:start + stride]
        if not values:
            return 0.0
        try:
            return max(0.0, min(1.0, max(float(v) for v in values)))
        except Exception:
            return 0.0

    def _ray_visual_color(self, sensor, ray_index: int, activation: float) -> tuple[int, int, int]:
        channels = tuple(getattr(sensor, 'channels', ('d',)))
        stride = max(1, len(channels))
        start = ray_index * stride
        values = getattr(sensor, 'last_inputs', [])[start:start + stride]
        rgb = [0.0, 0.0, 0.0]
        has_color = False
        for channel, value in zip(channels, values):
            try:
                v = max(0.0, min(1.0, float(value)))
            except Exception:
                v = 0.0
            if channel == 'r':
                rgb[0] = max(rgb[0], v); has_color = True
            elif channel == 'g':
                rgb[1] = max(rgb[1], v); has_color = True
            elif channel == 'b':
                rgb[2] = max(rgb[2], v); has_color = True
            elif channel == 'rd':
                rgb[0] = max(rgb[0], v / max(activation, 1e-6)); has_color = True
            elif channel == 'gd':
                rgb[1] = max(rgb[1], v / max(activation, 1e-6)); has_color = True
            elif channel == 'bd':
                rgb[2] = max(rgb[2], v / max(activation, 1e-6)); has_color = True
        if has_color:
            return (
                int(max(20, min(255, rgb[0] * 255))),
                int(max(20, min(255, rgb[1] * 255))),
                int(max(20, min(255, rgb[2] * 255))),
            )
        return (255, int(180 + 60 * (1.0 - activation)), 35)

    def _vision_bin_radius_fraction(self, fraction: float, options: dict) -> float:
        fraction = max(0.0, min(1.0, float(fraction)))
        if str(options.get('distribution', 'linear')) == 'near_detail':
            return fraction * fraction
        return fraction

    def _vision_radius_from_activation(self, activation: float, radius: float, options: dict) -> float:
        activation = max(0.0, min(1.0, float(activation)))
        falloff = str(options.get('falloff', 'linear'))
        if activation <= 0.0:
            return radius
        if falloff == 'quadratic':
            norm = 1.0 - math.sqrt(activation)
        elif falloff == 'none':
            norm = 1.0
        else:
            norm = 1.0 - activation
        return max(radius * 0.04, min(radius, radius * norm))

    def _draw_bin_grid(self, surface: pygame.Surface, camera: 'Camera', eye_x: float, eye_y: float,
                       gaze_angle: float, half_fov: float, radius: float, retina_count: int,
                       subdivisions: int, options: dict):
        grid_color = (58, 170, 230, 82)
        left = gaze_angle - half_fov
        right = gaze_angle + half_fov
        # Angular boundaries.
        for boundary_idx in range(retina_count + 1):
            angle = left + (boundary_idx / max(1, retina_count)) * (right - left)
            sx, sy = camera.world_to_screen(eye_x, eye_y)
            ex, ey = camera.world_to_screen(eye_x + math.cos(angle) * radius, eye_y + math.sin(angle) * radius)
            pygame.draw.line(surface, grid_color, (int(sx), int(sy)), (int(ex), int(ey)), 1)
        # Distance subdivision arcs.
        subdivisions = max(1, int(subdivisions))
        arc_steps = max(12, min(96, retina_count * 3))
        for band_idx in range(1, subdivisions):
            frac = self._vision_bin_radius_fraction(band_idx / subdivisions, options)
            rr = radius * frac
            points = []
            for step in range(arc_steps + 1):
                angle = left + (step / arc_steps) * (right - left)
                points.append(camera.world_to_screen(eye_x + math.cos(angle) * rr, eye_y + math.sin(angle) * rr))
            if len(points) >= 2:
                pygame.draw.lines(surface, grid_color, False, [(int(x), int(y)) for x, y in points], 1)

    def _draw_vision_bins(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera', selected: bool = False):
        sensor = getattr(agent, 'sensor', None)
        if sensor is None or not getattr(sensor, 'last_inputs', None):
            return
        count = max(1, int(getattr(sensor, 'retina_count', 1) or 1))
        total = sensor.total_ray_count() if hasattr(sensor, 'total_ray_count') else count
        half_fov = math.radians(float(getattr(sensor, 'fov_degrees', 180.0)) * 0.5)
        radius = float(getattr(sensor, 'vision_radius', 0.0))
        if half_fov <= 0.0 or radius <= 0.0:
            return
        eye_count = max(1, int(total / count))
        specs = sensor._eye_specs() if hasattr(sensor, '_eye_specs') else [(0.0, 0.0)]
        overlay = pygame.Surface(surface.get_size(), pygame.SRCALPHA) if selected else None
        grid_overlay = pygame.Surface(surface.get_size(), pygame.SRCALPHA) if selected else None
        options = getattr(sensor, 'last_bins_options', {}) or {}
        subdivisions = max(1, int(options.get('subdivisions', 1) or 1))
        if selected and grid_overlay is not None:
            for eye_idx in range(eye_count):
                pos_offset, gaze_offset = specs[min(eye_idx, len(specs) - 1)]
                position_angle = float(agent.angle) + pos_offset
                eye_x = float(agent.x) + math.cos(position_angle) * float(agent.r)
                eye_y = float(agent.y) + math.sin(position_angle) * float(agent.r)
                gaze_angle = float(agent.angle) + gaze_offset
                self._draw_bin_grid(grid_overlay, camera, eye_x, eye_y, gaze_angle, half_fov, radius, count, subdivisions, options)
        for ray_index in range(int(total)):
            eye_idx = min(eye_count - 1, ray_index // count)
            local_idx = ray_index % count
            pos_offset, gaze_offset = specs[min(eye_idx, len(specs) - 1)]
            position_angle = float(agent.angle) + pos_offset
            eye_x = float(agent.x) + math.cos(position_angle) * float(agent.r)
            eye_y = float(agent.y) + math.sin(position_angle) * float(agent.r)
            gaze_angle = float(agent.angle) + gaze_offset
            sector_width = (2.0 * half_fov) / count
            left_angle = gaze_angle - half_fov + local_idx * sector_width
            right_angle = left_angle + sector_width
            activation = self._ray_activation(sensor, ray_index)
            color = self._ray_visual_color(sensor, ray_index, activation)
            if selected and activation > 0.001 and overlay is not None:
                arc_steps = max(2, min(10, int(abs(right_angle - left_angle) * max(8.0, radius * camera.zoom) / 36.0) + 1))
                lit_radius = self._vision_radius_from_activation(activation, radius, options)
                points = [camera.world_to_screen(eye_x, eye_y)]
                for step in range(arc_steps + 1):
                    t = step / arc_steps
                    angle = left_angle + (right_angle - left_angle) * t
                    px = eye_x + math.cos(angle) * lit_radius
                    py = eye_y + math.sin(angle) * lit_radius
                    points.append(camera.world_to_screen(px, py))
                alpha = int(28 + 105 * activation)
                pygame.draw.polygon(overlay, (*color, alpha), [(int(x), int(y)) for x, y in points])
            if selected or activation > 0.001:
                center_angle = (left_angle + right_angle) * 0.5
                lit_radius = self._vision_radius_from_activation(activation, radius, options) if activation > 0.001 else radius
                sx, sy = camera.world_to_screen(eye_x, eye_y)
                ex, ey = camera.world_to_screen(
                    eye_x + math.cos(center_angle) * lit_radius,
                    eye_y + math.sin(center_angle) * lit_radius,
                )
                line_color = color if activation > 0.001 else (70, 150, 210)
                thickness = max(1, int(1 + activation * 2))
                pygame.draw.line(surface, line_color, (int(sx), int(sy)), (int(ex), int(ey)), thickness)
        if overlay is not None:
            surface.blit(overlay, (0, 0))
        if grid_overlay is not None:
            surface.blit(grid_overlay, (0, 0))
    
    def _draw_agent_details(self, agent: 'Agent', surface: pygame.Surface):
        """Desenha detalhes do agente selecionado no canto direito."""
        box_w = 360
        pad = 8

        lines = [
            "--- Agente Selecionado ---",
            f"Tipo: {'Predador' if getattr(agent, 'is_predator', False) else 'Bactéria'}",
            f"Pos: {agent.x:.1f}, {agent.y:.1f}",
            f"Idade: {agent.age:.2f}s",
            f"Velocidade: {agent.speed():.2f}",
            f"Direção (deg): {math.degrees(agent.angle):.1f}",
            f"Energia: {getattr(agent, 'energy', 0.0):.2f}  Raio: {agent.r:.2f}",
        ]

        if len(lines) > 1:
            lines[1] = "Tipo: Organismo legado" if getattr(agent, 'is_predator', False) else "Tipo: Organismo"

        if hasattr(agent, 'sensor') and getattr(agent.sensor, 'last_inputs', None):
            lines.append(f"Retinas ({len(agent.sensor.last_inputs)}):")
            for i, val in enumerate(agent.sensor.last_inputs):
                lines.append(f" R{i:02d}: {val:.3f}")

        if hasattr(agent, 'last_brain_activations') and agent.last_brain_activations:
            lines.append("Ativações neurais (por camada):")
            for li, layer in enumerate(agent.last_brain_activations):
                preview = ", ".join([f"{x:.3f}" for x in layer[:10]])
                if len(layer) > 10:
                    preview += ", ..."
                lines.append(f" L{li} ({len(layer)}): {preview}")

        x0 = surface.get_width() - box_w - pad
        y0 = pad
        h = max(120, 16 * len(lines))
        pygame.draw.rect(surface, (20, 20, 30), pygame.Rect(x0, y0, box_w, h))
        pygame.draw.rect(surface, (80, 200, 80), pygame.Rect(x0, y0, box_w, 20))
        yy = y0 + 4
        for line in lines:
            surface.blit(self.small_font.render(line, True, (220, 220, 220)), (x0 + 6, yy))
            yy += 16


class EllipseRenderer(RendererStrategy):
    """
    Renderização bonita com elipses rotacionadas.
    Mais custosa computacionalmente mas visualmente melhor.
    """
    
    def __init__(self):
        if not pygame.font.get_init():
            pygame.font.init()
        self.font = pygame.font.SysFont(None, 18)
        self.small_font = pygame.font.SysFont(None, 14)
        self._simple_renderer = SimpleRenderer()
    
    def draw_agent(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera',
                   show_head: bool = True, show_vision: bool = False, selected: bool = False):
        """Desenha agente como elipse rotacionada."""
        pose_x, pose_y, pose_angle = _interpolated_agent_pose(
            agent, float(getattr(self, 'interpolation_alpha', 1.0))
        )
        screen_x, screen_y = camera.world_to_screen(pose_x, pose_y)
        
        # Dimensões do corpo
        body_length = max(1, int(agent.r * 2 * camera.zoom))  # Comprimento
        body_width = max(1, int(agent.r * 1.0 * camera.zoom))  # Largura
        
        # Cor do corpo
        if selected:
            color = (100, 220, 100)
        else:
            color = agent.color
        
        # Cria superfície para elipse rotacionada
        if getattr(agent, 'body_shape', getattr(getattr(agent, 'locomotion', None), 'body_shape', 'ellipse')) == 'circle':
            self._simple_renderer.interpolation_alpha = float(getattr(self, 'interpolation_alpha', 1.0))
            self._simple_renderer.draw_agent(agent, surface, camera, show_head=show_head, show_vision=show_vision, selected=selected)
            return

        ellipse_surf = pygame.Surface((body_length, body_width), pygame.SRCALPHA)
        pygame.draw.ellipse(ellipse_surf, color, pygame.Rect(0, 0, body_length, body_width))
        
        # Rotaciona
        angle_degrees = -math.degrees(pose_angle)
        rotated_surf = pygame.transform.rotate(ellipse_surf, angle_degrees)
        
        # Desenha centralizado
        rect = rotated_surf.get_rect()
        rect.center = (int(screen_x), int(screen_y))
        surface.blit(rotated_surf, rect)
        
        # Desenha cabeça
        if show_head:
            head_offset = agent.r
            head_world_x = pose_x + math.cos(pose_angle) * head_offset
            head_world_y = pose_y + math.sin(pose_angle) * head_offset
            head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
            head_radius = max(1, int(agent.r * 0.25 * camera.zoom))
            sensor = getattr(agent, 'sensor', None)
            eye_count = int(getattr(sensor, 'eye_count', 1) or 1) if sensor is not None else 1
            if eye_count >= 2 and sensor is not None and hasattr(sensor, '_eye_specs'):
                for pos_offset, _gaze_offset in sensor._eye_specs():
                    eye_angle = pose_angle + pos_offset
                    ex = pose_x + math.cos(eye_angle) * agent.r
                    ey = pose_y + math.sin(eye_angle) * agent.r
                    sx, sy = camera.world_to_screen(ex, ey)
                    pygame.draw.circle(surface, (0, 0, 0), (int(sx), int(sy)), head_radius)
            else:
                pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera, selected=selected)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo (igual ao SimpleRenderer)."""
        fx, fy = _interpolated_food_pos(food, float(getattr(self, 'interpolation_alpha', 1.0)))
        screen_x, screen_y = camera.world_to_screen(fx, fy)
        screen_radius = max(1, int(food.r * camera.zoom))
        pygame.draw.circle(surface, food.color, (int(screen_x), int(screen_y)), screen_radius)
        _draw_food_bite_holes(food, surface, camera)
        if getattr(food, 'kind', 'instant') == 'chunk' and screen_radius >= 3:
            pygame.draw.circle(surface, (35, 25, 20), (int(screen_x), int(screen_y)), screen_radius, width=1)

    def draw_obstacles(self, obstacles: 'ObstacleMap', surface: pygame.Surface, camera: 'Camera',
                       visible_bounds=None):
        """Reutiliza implementação do SimpleRenderer."""
        self._simple_renderer.draw_obstacles(obstacles, surface, camera, visible_bounds=visible_bounds)
    
    def draw_overlay(self, surface: pygame.Surface, info: dict):
        """Reutiliza implementação do SimpleRenderer."""
        self._simple_renderer.draw_overlay(surface, info)
    
    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera', selected: bool = False):
        """Reutiliza implementação do SimpleRenderer."""
        self._simple_renderer._draw_vision_rays(agent, surface, camera, selected=selected)
