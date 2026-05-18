#!/usr/bin/env python3
"""
Ponto de entrada principal da simulacao.
Cria Params, Engine, UI; conecta sinais/comandos.
"""
import argparse
import os
import sys

# Adiciona diretorio atual ao path para imports.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from sim.controllers import Params
from sim.diagnostics import install_qt_message_handler, log_event, log_exception, setup_runtime_diagnostics
from sim.engine import Engine
from sim.game import PygameView

_QT_IMPORT_ERROR = None
try:
    from sim.ui import SimulationUI as QtSimulationUI
    _HAS_QT = True
except ModuleNotFoundError as exc:
    if (exc.name or "").split(".")[0] != "PyQt6":
        raise
    QtSimulationUI = None  # type: ignore
    _HAS_QT = False
    _QT_IMPORT_ERROR = exc

from sim.world import Camera, World


def main(argv=None):
    """Inicializa engine e UI.

    Opcoes de linha de comando:
        --ui qt    Forca interface PyQt6
        padrao: usa PyQt6
    """
    argv = argv or sys.argv[1:]
    parser = argparse.ArgumentParser(description="AgentBioSim V1.0.0")
    parser.add_argument("--ui", choices=["qt"], help="Backend de interface suportado atualmente: qt.")
    args = parser.parse_args(argv)

    backend = args.ui or "qt"
    if not _HAS_QT:
        print("PyQt6 nao disponivel; instale as dependencias com: python -m pip install -r requirements.txt")
        if _QT_IMPORT_ERROR is not None:
            print(f"Detalhe: {_QT_IMPORT_ERROR}")
        return 1

    root_dir = os.path.dirname(os.path.abspath(__file__))
    log_path = setup_runtime_diagnostics(root_dir)
    print(f"Log de diagnostico: {log_path}")
    log_event("MAIN_START", backend=backend)

    try:
        print(f"Inicializando simulacao... (UI={backend})")

        # 1. Config
        params = Params()

        # 2. Mundo / camera
        world = World(
            width=params.get("world_w", 1000.0),
            height=params.get("world_h", 700.0),
            shape=params.get("substrate_shape", "rectangular"),
            radius=params.get("substrate_radius", 350.0),
        )
        camera = Camera()

        # 3. Engine headless
        engine = Engine(world, camera, params)

        # 4. View Pygame
        pygame_view = PygameView(engine, screen_width=800, screen_height=600)

        # 5. UI PyQt6
        from PyQt6.QtWidgets import QApplication

        app = QApplication.instance() or QApplication([])
        install_qt_message_handler()
        app.aboutToQuit.connect(lambda: log_event("QT_ABOUT_TO_QUIT"))
        ui = QtSimulationUI(params, engine, pygame_view)  # type: ignore

        # 6. Conexoes
        setup_connections(params, engine, pygame_view, ui)

        print("Configuracao concluida. Iniciando interface...")

        # 7. Run loop
        ui.run()  # chama show() + exec()

    except KeyboardInterrupt:
        print("\nSimulacao interrompida pelo usuario")
        log_event("KEYBOARD_INTERRUPT")
    except Exception as e:
        print(f"Erro critico: {e}")
        import traceback

        traceback.print_exc()
        log_exception("MAIN_CRITICAL_EXCEPTION", type(e), e, e.__traceback__)
        return 2
    finally:
        log_event("MAIN_FINALLY")
        print("Simulacao encerrada")
    return 0


def setup_connections(params: Params, engine: Engine, pygame_view: PygameView, ui):
    """
    Conecta callbacks e sinais entre componentes principais.
    """

    def on_param_change(param_name: str, old_value, new_value):
        """Callback quando um parametro muda."""
        if param_name == "fps":
            pygame_view.set_fps(new_value)
        elif param_name == "time_scale":
            engine.set_time_scale(new_value)
        elif param_name.endswith("_show_vision"):
            pass

    # Params usa callbacks por parametro especifico. Mantemos este ponto de
    # extensao documentado para futuras conexoes automaticas.
    _ = on_param_change, params, ui


if __name__ == "__main__":
    raise SystemExit(main())
