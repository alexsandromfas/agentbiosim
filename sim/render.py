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
        screen_x, screen_y = camera.world_to_screen(agent.x, agent.y)
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
                    eye_angle = agent.angle + pos_offset
                    head_world_x = agent.x + math.cos(eye_angle) * agent.r
                    head_world_y = agent.y + math.sin(eye_angle) * agent.r
                    head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
                    pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
            else:
                head_world_x = agent.x + math.cos(agent.angle) * agent.r
                head_world_y = agent.y + math.sin(agent.angle) * agent.r
                head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
                pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha raios de visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo simples."""
        screen_x, screen_y = camera.world_to_screen(food.x, food.y)
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
    
    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera'):
        """Desenha raios de visão com intensidade proporcional à ativação."""
        if not hasattr(agent, 'sensor'):
            return
        
        sensor = agent.sensor
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
        screen_x, screen_y = camera.world_to_screen(agent.x, agent.y)
        
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
            self._simple_renderer.draw_agent(agent, surface, camera, show_head=show_head, show_vision=show_vision, selected=selected)
            return

        ellipse_surf = pygame.Surface((body_length, body_width), pygame.SRCALPHA)
        pygame.draw.ellipse(ellipse_surf, color, pygame.Rect(0, 0, body_length, body_width))
        
        # Rotaciona
        angle_degrees = -math.degrees(agent.angle)  # Pygame usa graus, negativo para correção
        rotated_surf = pygame.transform.rotate(ellipse_surf, angle_degrees)
        
        # Desenha centralizado
        rect = rotated_surf.get_rect()
        rect.center = (int(screen_x), int(screen_y))
        surface.blit(rotated_surf, rect)
        
        # Desenha cabeça
        if show_head:
            head_offset = agent.r
            head_world_x = agent.x + math.cos(agent.angle) * head_offset
            head_world_y = agent.y + math.sin(agent.angle) * head_offset
            head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
            head_radius = max(1, int(agent.r * 0.25 * camera.zoom))
            sensor = getattr(agent, 'sensor', None)
            eye_count = int(getattr(sensor, 'eye_count', 1) or 1) if sensor is not None else 1
            if eye_count >= 2 and sensor is not None and hasattr(sensor, '_eye_specs'):
                for pos_offset, _gaze_offset in sensor._eye_specs():
                    eye_angle = agent.angle + pos_offset
                    ex = agent.x + math.cos(eye_angle) * agent.r
                    ey = agent.y + math.sin(eye_angle) * agent.r
                    sx, sy = camera.world_to_screen(ex, ey)
                    pygame.draw.circle(surface, (0, 0, 0), (int(sx), int(sy)), head_radius)
            else:
                pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo (igual ao SimpleRenderer)."""
        screen_x, screen_y = camera.world_to_screen(food.x, food.y)
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
    
    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera'):
        """Reutiliza implementação do SimpleRenderer."""
        self._simple_renderer._draw_vision_rays(agent, surface, camera)
