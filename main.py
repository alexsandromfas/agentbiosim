#!/usr/bin/env python3
"""
Ponto de entrada principal da simulação.
Cria Params, Engine, UI; conecta sinais/comandos.
"""
import sys
import os
import argparse

# Adiciona diretório atual ao path para imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from sim.controllers import Params
from sim.engine import Engine
from sim.game import PygameView
# Integração UI: preferir PyQt6 (ui.py) com fallback para Tkinter (ui_tk.py)
try:
    from sim.ui import SimulationUI as QtSimulationUI
    _HAS_QT = True
except Exception:
    QtSimulationUI = None  # type: ignore
    _HAS_QT = False
try:
    from sim.ui_tk import SimulationUI as TkSimulationUI
    _HAS_TK = True
except Exception:
    TkSimulationUI = None  # type: ignore
    _HAS_TK = False
from sim.world import World, Camera


def main(argv=None):
    """Função principal - inicializa engine e UI.

    Opções de linha de comando:
        --ui qt    Força interface PyQt6
        --ui tk    Força interface Tkinter
        (padrão: tenta qt, fallback tk)
    """
    argv = argv or sys.argv[1:]
    parser = argparse.ArgumentParser(description="AgentBioSim")
    parser.add_argument('--ui', choices=['qt','tk'], help='Escolhe backend de interface (qt ou tk).')
    args = parser.parse_args(argv)

    requested_ui = args.ui

    # Seleção de backend
    backend = None
    if requested_ui == 'qt':
        if not _HAS_QT:
            print("[WARN] PyQt6 não disponível; abortando.")
            return 1
        backend = 'qt'
    elif requested_ui == 'tk':
        if not _HAS_TK:
            print("[WARN] Tkinter UI não disponível.")
            return 1
        backend = 'tk'
    else:
        # Auto: preferir qt
        if _HAS_QT:
            backend = 'qt'
        elif _HAS_TK:
            backend = 'tk'
        else:
            print("Nenhuma UI disponível (PyQt6 ou Tkinter). Instale PyQt6 ou habilite Tk.")
            return 1

    try:
        print(f"Inicializando simulação... (UI={backend})")

        # 1. Config
        params = Params()

        # 2. Mundo / câmera
        world = World(
            width=params.get('world_w', 1000.0),
            height=params.get('world_h', 700.0),
            shape=params.get('substrate_shape', 'rectangular'),
            radius=params.get('substrate_radius', 350.0)
        )
        camera = Camera()

        # 3. Engine headless
        engine = Engine(world, camera, params)

        # 4. View Pygame
        pygame_view = PygameView(engine, screen_width=800, screen_height=600)

        # 5. UI conforme backend
        if backend == 'qt':
            from PyQt6.QtWidgets import QApplication  # local import para evitar custo se tk
            app = QApplication.instance() or QApplication([])
            ui = QtSimulationUI(params, engine, pygame_view)  # type: ignore
        else:
            ui = TkSimulationUI(params, engine, pygame_view)  # type: ignore

        # 6. Conexões
        setup_connections(params, engine, pygame_view, ui)

        print("Configuração concluída. Iniciando interface...")

        # 7. Run loop
        if backend == 'qt':
            ui.run()  # chama show() + exec()
        else:
            ui.run()

    except KeyboardInterrupt:
        print("\nSimulação interrompida pelo usuário")
    except Exception as e:
        print(f"Erro crítico: {e}")
        import traceback
        traceback.print_exc()
        return 2
    finally:
        print("Simulação encerrada")
    return 0


def setup_connections(params: Params, engine: Engine, pygame_view: PygameView, ui):
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
    raise SystemExit(main())
