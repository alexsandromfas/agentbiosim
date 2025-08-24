#!/usr/bin/env python3
"""
Ponto de entrada principal da simulação.
Cria Params, Engine, UI; conecta sinais/comandos.
"""
import sys
import os

# Adiciona diretório atual ao path para imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from sim.controllers import Params
from sim.engine import Engine
from sim.game import PygameView
from sim.ui_tk import SimulationUI
from sim.world import World, Camera


def main():
    """
    Função principal - bootstraps toda aplicação.
    """
    try:
        print("Inicializando simulação...")
        
        # 1. Cria configuração central
        params = Params()
        
        # 2. Cria mundo e câmera
        world = World(
            width=params.get('world_w', 1000.0),
            height=params.get('world_h', 700.0),
            shape=params.get('substrate_shape', 'rectangular'),
            radius=params.get('substrate_radius', 350.0)
        )
        camera = Camera()
        
        # 3. Cria engine headless
        engine = Engine(world, camera, params)
        
        # 4. Cria view Pygame
        pygame_view = PygameView(engine, screen_width=800, screen_height=600)
        
        # 5. Cria interface Tkinter 
        ui = SimulationUI(params, engine, pygame_view)
        
        # 6. Conecta callbacks entre componentes
        setup_connections(params, engine, pygame_view, ui)
        
        print("Configuração concluída. Iniciando interface...")
        
        # 7. Inicia aplicação
        ui.run()
        
    except KeyboardInterrupt:
        print("\nSimulação interrompida pelo usuário")
        
    except Exception as e:
        print(f"Erro crítico: {e}")
        import traceback
        traceback.print_exc()
        
    finally:
        print("Simulação encerrada")


def setup_connections(params: Params, engine: Engine, pygame_view: PygameView, ui: SimulationUI):
    """
    Conecta callbacks e sinais entre componentes principais.
    """
    # Exemplo de callbacks que podem ser configurados:
    
    # Quando parâmetros mudarem, notificar componentes relevantes
    def on_param_change(param_name: str, old_value, new_value):
        """Callback quando um parâmetro muda."""
        # Engine automaticamente lê params durante simulação
        
        # Casos especiais que precisam notificação imediata
        if param_name == 'fps':
            pygame_view.set_fps(new_value)
        elif param_name == 'time_scale':
            engine.set_time_scale(new_value)
        elif param_name.endswith('_show_vision'):
            # UI já mostra/oculta visão via rendering
            pass
    
    # Registra callback de parâmetros
    # Note: Params usa add_callback por parâmetro específico, não um callback global
    # Para simplicidade inicial, não configuramos callbacks automáticos
    # params.add_callback('fps', on_param_change)
    # params.add_callback('time_scale', on_param_change)
    
    # Outros callbacks podem ser adicionados aqui conforme necessário:
    # - Estado de pause/resume
    # - Mudanças de população
    # - Estatísticas da simulação
    # - etc.


if __name__ == '__main__':
    main()
