"""
Estratégias de renderização para visualização da simulação.
"""
import math
import pygame
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .entities import Agent, Food
    from .world import Camera


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
            head_offset = agent.r
            head_world_x = agent.x + math.cos(agent.angle) * head_offset
            head_world_y = agent.y + math.sin(agent.angle) * head_offset
            head_screen_x, head_screen_y = camera.world_to_screen(head_world_x, head_world_y)
            head_radius = max(1, int(agent.r * 0.25 * camera.zoom))
            pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha raios de visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo simples."""
        screen_x, screen_y = camera.world_to_screen(food.x, food.y)
        screen_radius = max(1, int(food.r * camera.zoom))
        pygame.draw.circle(surface, food.color, (int(screen_x), int(screen_y)), screen_radius)
    
    def draw_overlay(self, surface: pygame.Surface, info: dict):
        """Desenha informações de overlay."""
        # Linha principal de informações
        bacteria_count = info.get('bacteria_count', 0)
        predator_count = info.get('predator_count', 0)  
        food_count = info.get('food_count', 0)
        food_target = info.get('food_target', 0)
        fps = info.get('fps', 0)
        
        main_info = f"Bactérias: {bacteria_count}  |  Predadores: {predator_count}  |  Comida: {food_count}  |  Target: {food_target}  |  FPS: {int(fps)}"
        surf = self.font.render(main_info, True, (220, 220, 220))
        surface.blit(surf, (8, 8))
        
        # Linha secundária
        max_speed = info.get('max_speed', 0)
        time_scale = info.get('time_scale', 1.0)
        world_w = info.get('world_w', 0)
        world_h = info.get('world_h', 0)
        
        secondary_info = f"Max speed: {max_speed:.1f}  |  Time x: {time_scale:.2f}  |  World: {world_w:.0f}x{world_h:.0f}"
        surf2 = self.font.render(secondary_info, True, (180, 180, 220))
        surface.blit(surf2, (8, 28))
        
        # Detalhes do agente selecionado
        selected_agent = info.get('selected_agent')
        if selected_agent and info.get('show_selected_details', True):
            self._draw_agent_details(selected_agent, surface)
    
    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera'):
        """Desenha raios de visão com intensidade proporcional à ativação."""
        if not hasattr(agent, 'sensor'):
            return
        
        sensor = agent.sensor
        for i in range(len(sensor.last_inputs)):
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
            pygame.draw.circle(surface, (0, 0, 0), (int(head_screen_x), int(head_screen_y)), head_radius)
        
        # Desenha visão se solicitado OU se o agente estiver selecionado
        if (show_vision or selected) and hasattr(agent, 'sensor') and agent.sensor.last_inputs:
            self._draw_vision_rays(agent, surface, camera)
    
    def draw_food(self, food: 'Food', surface: pygame.Surface, camera: 'Camera'):
        """Desenha comida como círculo (igual ao SimpleRenderer)."""
        screen_x, screen_y = camera.world_to_screen(food.x, food.y)
        screen_radius = max(1, int(food.r * camera.zoom))
        pygame.draw.circle(surface, food.color, (int(screen_x), int(screen_y)), screen_radius)
    
    def draw_overlay(self, surface: pygame.Surface, info: dict):
        """Reutiliza implementação do SimpleRenderer."""
        simple = SimpleRenderer()
        simple.draw_overlay(surface, info)
    
    def _draw_vision_rays(self, agent: 'Agent', surface: pygame.Surface, camera: 'Camera'):
        """Reutiliza implementação do SimpleRenderer."""
        simple = SimpleRenderer()
        simple._draw_vision_rays(agent, surface, camera)
