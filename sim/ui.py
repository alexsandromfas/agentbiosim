"""PyQt6 interface for the simulation (port from `ui_tk.py`).

Goals:
    - Preserve all parameters and features from the Tkinter version.
    - Keep Engine headless; only orchestration / parameter control lives here.
    - Embed the existing PygameView inside a Qt widget (via native window id).
    - Provide parity for: apply_* param groups, export/import agent & substrate,
      auto substrate export, CSV persistence of UI values.

Notes:
    This file intentionally mirrors structure of `ui_tk.py` but adapts to Qt idioms.
    Public API kept similar: class SimulationUI(params, engine, pygame_view). Call
    `.run()` to start the Qt event loop (blocking) – analogous to Tk version.

    The original ui_tk used Tk Variable classes; here we bind widgets directly.
    `self.widgets` maps parameter names to the corresponding input widget so we
    can read/write values. Helper functions abstract value access.
"""
from __future__ import annotations

import os
import math
import csv
import json
import threading
import traceback
from typing import Dict, Any, Tuple

from PyQt6.QtCore import Qt, QTimer, QSize, QEvent
from PyQt6.QtGui import QIcon, QColor, QAction
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QTabWidget,
    QLabel, QPushButton, QSpinBox, QDoubleSpinBox, QCheckBox, QComboBox,
    QLineEdit, QTextEdit, QListWidget, QMessageBox, QFileDialog, QScrollArea,
    QFormLayout, QGridLayout, QGroupBox, QToolTip, QStackedWidget
    , QColorDialog
)

from .controllers import Params
from .engine import Engine
from .game import PygameView
from .profiler import profiler


# -------------------------- Helper abstractions --------------------------

def _spin_int(min_v: int, max_v: int, step: int = 1) -> QSpinBox:
    w = QSpinBox()
    w.setRange(min_v, max_v)
    w.setSingleStep(step)
    return w


def _spin_double(min_v: float, max_v: float, step: float = 0.1, decimals: int = 3) -> QDoubleSpinBox:
    w = QDoubleSpinBox()
    w.setRange(min_v, max_v)
    w.setSingleStep(step)
    w.setDecimals(decimals)
    return w


class ClickHelpLabel(QLabel):
    """Label de formulario que mostra explicacao ao clique."""

    def __init__(self, text: str, help_text: str):
        super().__init__(text)
        self.help_text = help_text
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.setToolTip("Clique para ver uma explicacao.")
        self.setStyleSheet("QLabel { color: #d8e5f5; } QLabel:hover { color: #9fc8ff; text-decoration: underline; }")

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            QToolTip.showText(self.mapToGlobal(self.rect().bottomLeft()), self.help_text, self)
            event.accept()
            return
        super().mousePressEvent(event)


class SimulationUI(QMainWindow):
    """PyQt6 version of the simulation control UI."""

    def __init__(self, params: Params, engine: Engine, pygame_view: PygameView):
        super().__init__()
        self.params = params
        self.engine = engine
        self.pygame_view = pygame_view
        root_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
        self._ui_params_csv = os.path.join(root_dir, 'config', 'user_params.csv')
        self._legacy_ui_params_csv = os.path.join(os.path.dirname(__file__), 'ui_params.csv')

        self.setWindowTitle("AgentBioSim V1.0.0")
        # Define icon from project assets (only for main UI window)
        try:
            icon_path = os.path.join(os.path.dirname(__file__), '..', 'assets', 'icon.png')
            icon_path = os.path.normpath(icon_path)
            if os.path.exists(icon_path):
                self.setWindowIcon(QIcon(icon_path))
        except Exception:
            # if icon cannot be loaded, continue without failing
            pass
        self.resize(1400, 860)

        self.widgets: Dict[str, QWidget] = {}
        self._auto_export_timer: QTimer | None = None

        self._build_layout()
        self._build_tabs()
        self._load_ui_params_csv()  # load after widget creation so we can set values
        if 'obstacle_brush_width' in self.widgets:
            self._update_brush_width(self._get_widget_value('obstacle_brush_width'))
        if 'obstacle_brush_erase' in self.widgets:
            self._update_brush_erase()
        self._build_menu_bar()
        self._setup_live_param_signals()

        # Embed pygame view (defer until shown)
        QTimer.singleShot(100, self._init_pygame_view)

    # ------------------------------------------------------------------
    # Layout / Tabs
    # ------------------------------------------------------------------
    def _build_layout(self):
        central = QWidget()
        self.setCentralWidget(central)
        lay = QHBoxLayout(central)
        lay.setContentsMargins(6, 6, 6, 6)
        lay.setSpacing(6)

        # Left control area
        self.control_container = QWidget()
        self.control_container.setMinimumWidth(420)
        self.control_container.setMaximumWidth(520)
        lay.addWidget(self.control_container, stretch=0)

        v = QVBoxLayout(self.control_container)
        v.setContentsMargins(0, 0, 0, 0)

        self.tabs = QTabWidget()
        v.addWidget(self.tabs, stretch=1)

        # Right pygame placeholder (native window id host) + canvas tools
        self.sim_container = QWidget()
        sim_lay = QVBoxLayout(self.sim_container)
        sim_lay.setContentsMargins(0, 0, 0, 0)
        sim_lay.setSpacing(6)

        self.pygame_host = QWidget()
        self.pygame_host.setObjectName("pygame_host")
        self.pygame_host.setStyleSheet("#pygame_host { background: #101214; }")
        sim_lay.addWidget(self.pygame_host, stretch=1)
        sim_lay.addWidget(self._build_canvas_tools(), stretch=0)
        lay.addWidget(self.sim_container, stretch=1)

    def _build_canvas_tools(self):
        bar = QWidget()
        bar.setObjectName("canvas_tools")
        bar.setStyleSheet(
            "#canvas_tools { background: #15181d; border-top: 1px solid #303741; } "
            "QPushButton { min-width: 34px; min-height: 30px; padding: 2px 6px; } "
            "QPushButton[active='true'] { background: #355d8c; border: 1px solid #7fb2ff; }"
        )
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(8, 5, 8, 5)
        layout.setSpacing(6)
        layout.addStretch(1)

        self._canvas_tool_buttons = {}

        def add_tool(key: str, text: str, tooltip: str, icon_path: str | None = None):
            btn = QPushButton(text)
            btn.setCheckable(True)
            btn.setToolTip(tooltip)
            if icon_path and os.path.exists(icon_path):
                btn.setText("")
                btn.setIcon(QIcon(icon_path))
                btn.setIconSize(QSize(22, 22))
            btn.clicked.connect(lambda _checked=False, k=key: self._set_canvas_tool(k))
            self._canvas_tool_buttons[key] = btn
            layout.addWidget(btn)
            return btn

        add_tool('select', 'S', 'Seletor: clique esquerdo seleciona agente e mostra visao/detalhes.')
        add_tool('food', 'F', 'Comida: clique esquerdo adiciona comida.')
        add_tool('agent', 'A', 'Agente importado: clique esquerdo insere o agente carregado.')
        icon_path = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Assets', 'draw_icon.png'))
        add_tool('draw', 'P', 'Pincel: desenha barreiras solidas no substrato.', icon_path=icon_path)
        add_tool('move', 'M', 'Mover: clique e arraste comida, bacterias ou predadores.')
        add_tool('dead', 'D', 'Dead: remove o objeto clicado, inclusive comida.')

        layout.addSpacing(8)
        layout.addWidget(self._help_label("Pincel:", 'obstacle_brush_width'))
        width = _spin_double(1.0, 200.0, 1.0, 1)
        width.setValue(float(getattr(self.pygame_view, 'brush_width', 16.0)))
        width.setToolTip("Largura do pincel de obstaculos.")
        width.valueChanged.connect(self._update_brush_width)
        self.widgets['obstacle_brush_width'] = width
        layout.addWidget(width)

        self._brush_color_swatch = QLabel()
        self._brush_color_swatch.setFixedSize(28, 28)
        self._brush_color_swatch.setToolTip("Cor atual do obstaculo.")
        color_btn = QPushButton("Cor")
        color_btn.setToolTip("Selecionar cor dos obstaculos.")
        color_btn.clicked.connect(self._pick_brush_color)
        layout.addWidget(self._brush_color_swatch)
        layout.addWidget(color_btn)

        erase = QCheckBox("Apagar")
        erase.setToolTip("Quando ligado, o pincel apaga barreiras em vez de desenhar.")
        erase.setChecked(False)
        erase.stateChanged.connect(lambda _state: self._update_brush_erase())
        self.widgets['obstacle_brush_erase'] = erase
        layout.addWidget(erase)

        self._refresh_brush_color_swatch()
        self._set_canvas_tool('food')
        return bar

    def _set_canvas_tool(self, tool: str):
        if hasattr(self.pygame_view, 'active_tool'):
            self.pygame_view.active_tool = tool
        for key, btn in getattr(self, '_canvas_tool_buttons', {}).items():
            active = key == tool
            btn.setChecked(active)
            btn.setProperty('active', 'true' if active else 'false')
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    def _update_brush_width(self, value: float):
        if hasattr(self.pygame_view, 'brush_width'):
            self.pygame_view.brush_width = float(value)

    def _update_brush_erase(self):
        if hasattr(self.pygame_view, 'brush_erase'):
            self.pygame_view.brush_erase = bool(self.widgets['obstacle_brush_erase'].isChecked())

    def _pick_brush_color(self):
        current = getattr(self.pygame_view, 'brush_color', (95, 95, 105))
        col = QColorDialog.getColor(QColor(*current), self, "Cor dos obstaculos")
        if col.isValid():
            self.pygame_view.brush_color = (col.red(), col.green(), col.blue())
            self._refresh_brush_color_swatch()

    def _refresh_brush_color_swatch(self):
        color = getattr(self.pygame_view, 'brush_color', (95, 95, 105))
        self._brush_color_swatch.setStyleSheet(
            f"background: rgb({color[0]},{color[1]},{color[2]}); border: 1px solid #777; border-radius: 4px;"
        )

    def _build_tabs(self):
        """Construct all tabs in a fixed order."""
        self._build_tab_genetic_editor()
        self._build_tab_population()
        self._build_tab_environment()
        self._build_tab_experiment()

    def _build_menu_bar(self):
        bar = self.menuBar()

        file_menu = bar.addMenu("Arquivo")
        act_new = QAction("Novo", self)
        act_new.triggered.connect(self.new_biosim_project)
        file_menu.addAction(act_new)
        act_open = QAction("Abrir .biosim", self)
        act_open.triggered.connect(self.open_biosim_window)
        file_menu.addAction(act_open)
        act_save = QAction("Salvar .biosim", self)
        act_save.triggered.connect(self.save_biosim_window)
        file_menu.addAction(act_save)
        file_menu.addSeparator()
        act_export_sub = QAction("Exportar substrato JSON", self)
        act_export_sub.triggered.connect(self.open_export_substrate_window)
        file_menu.addAction(act_export_sub)
        act_import_sub = QAction("Importar substrato JSON", self)
        act_import_sub.triggered.connect(self.open_import_substrate_window)
        file_menu.addAction(act_import_sub)
        file_menu.addSeparator()
        act_save_params = QAction("Salvar preferencias da UI", self)
        act_save_params.triggered.connect(self.save_ui_params)
        file_menu.addAction(act_save_params)

        view_menu = bar.addMenu("View")
        self._add_bool_menu_action(view_menu, "Renderizacao simples", 'simple_render')
        self._add_bool_menu_action(view_menu, "Detalhes do agente selecionado", 'show_selected_details')
        act_brain = QAction("Mostrar ativacoes neurais", self)
        act_brain.setCheckable(True)
        act_brain.setChecked(not self.params.get('disable_brain_activations', False))
        act_brain.toggled.connect(self._on_toggle_brain_activations)
        view_menu.addAction(act_brain)
        self._add_bool_menu_action(view_menu, "Mostrar visao das bacterias", 'bacteria_show_vision')
        self._add_bool_menu_action(view_menu, "Mostrar visao dos predadores", 'predator_show_vision')

        pref_menu = bar.addMenu("Preferencias")
        self._add_bool_menu_action(pref_menu, "Auto exportar substrato", 'auto_export_substrate', callback=self._on_auto_export_menu_toggled)
        self._add_bool_menu_action(pref_menu, "Exportar ativacoes neurais nos snapshots", 'export_substrate_include_brain_activations')
        self._add_bool_menu_action(pref_menu, "JSON manual legivel", 'export_substrate_pretty_json')
        self._add_bool_menu_action(pref_menu, "Tracebacks no debug", 'debug_tracebacks')
        act_pref_tab = QAction("Abrir aba Experimento", self)
        act_pref_tab.triggered.connect(lambda: self.tabs.setCurrentIndex(3))
        pref_menu.addAction(act_pref_tab)

        agent_menu = bar.addMenu("Agente")
        act_export_agent = QAction("Exportar agente selecionado", self)
        act_export_agent.triggered.connect(self.open_export_agent_window)
        agent_menu.addAction(act_export_agent)
        act_load_agent = QAction("Carregar agente", self)
        act_load_agent.triggered.connect(self.open_load_agent_window)
        agent_menu.addAction(act_load_agent)
        act_lineage = QAction("Criar linhagem a partir do selecionado", self)
        act_lineage.triggered.connect(self.create_lineage_from_selected)
        agent_menu.addAction(act_lineage)

        help_menu = bar.addMenu("Ajuda")
        act_help = QAction("Ajuda e atalhos", self)
        act_help.triggered.connect(self.show_help_window)
        help_menu.addAction(act_help)

    def _add_bool_menu_action(self, menu, text: str, param_name: str, callback=None):
        action = QAction(text, self)
        action.setCheckable(True)
        action.setChecked(bool(self.params.get(param_name, False)))
        if callback is None:
            action.toggled.connect(lambda checked, name=param_name: self._set_bool_param_from_menu(name, checked))
        else:
            action.toggled.connect(callback)
        menu.addAction(action)
        return action

    def _set_bool_param_from_menu(self, name: str, checked: bool):
        widget = self.widgets.get(name)
        if isinstance(widget, QCheckBox) and widget.isChecked() != checked:
            widget.setChecked(checked)
        self.params.set(name, bool(checked), validate=False)
        if name == 'simple_render':
            self.engine.send_command('change_renderer', simple=bool(checked))

    def _on_auto_export_menu_toggled(self, checked: bool):
        widget = self.widgets.get('auto_export_substrate')
        if isinstance(widget, QCheckBox) and widget.isChecked() != checked:
            widget.setChecked(checked)
            return
        self.params.set('auto_export_substrate', bool(checked), validate=False)
        if checked:
            self._reschedule_auto_export()
        elif self._auto_export_timer:
            self._auto_export_timer.stop()
            self._auto_export_timer = None

    # ---------------------- Widget value helpers ----------------------
    def _set_widget_value(self, name: str, value: Any):
        w = self.widgets.get(name)
        if w is None:
            return
        if isinstance(w, QSpinBox):
            try:
                w.blockSignals(True)
                # Converte float/string para int de forma segura
                if isinstance(value, str):
                    try:
                        if value.strip() == '':
                            return
                        if '.' in value or 'e' in value.lower():
                            value_num = int(float(value))
                        else:
                            value_num = int(value)
                    except Exception:
                        return
                elif value is None and name == 'random_seed':
                    value_num = -1
                elif isinstance(value, float):
                    value_num = int(round(value))
                else:
                    value_num = int(value)
                w.setValue(value_num)
            finally:
                w.blockSignals(False)
        elif isinstance(w, QDoubleSpinBox):
            try:
                w.blockSignals(True)
                w.setValue(float(value))
            finally:
                w.blockSignals(False)
        elif isinstance(w, QCheckBox):
            w.blockSignals(True)
            w.setChecked(bool(value))
            w.blockSignals(False)
        elif isinstance(w, QComboBox):
            idx = w.findText(str(value))
            if idx >= 0:
                w.blockSignals(True)
                w.setCurrentIndex(idx)
                w.blockSignals(False)
        elif isinstance(w, QLineEdit):
            w.blockSignals(True)
            w.setText(str(value))
            w.blockSignals(False)

    def _get_widget_value(self, name: str) -> Any:
        w = self.widgets.get(name)
        if w is None:
            return None
        if isinstance(w, (QSpinBox, QDoubleSpinBox)):
            return w.value()
        if isinstance(w, QCheckBox):
            return w.isChecked()
        if isinstance(w, QComboBox):
            return w.currentText()
        if isinstance(w, QLineEdit):
            return w.text()
        return None

    def _format_exception(self, exc: BaseException) -> str:
        if self.params.get('debug_tracebacks', False):
            message = ''.join(traceback.format_exception(type(exc), exc, exc.__traceback__, limit=12))
            return message[-4000:]
        return str(exc)

    def _log_exception(self, prefix: str, exc: BaseException):
        print(f"{prefix}: {self._format_exception(exc)}")

    def _warn_exception(self, title: str, exc: BaseException):
        QMessageBox.warning(self, title, self._format_exception(exc))

    def _help_label(self, label: str, name: str) -> ClickHelpLabel:
        return ClickHelpLabel(label, self._param_help_text(name, label))

    def _param_help_text(self, name: str, label: str) -> str:
        help_by_name = {
            'time_scale': 'Multiplica a velocidade do tempo simulado. Valores altos aceleram a evolucao, mas podem deixar colisoes e dinamicas menos estaveis.',
            'fps': 'Limite de quadros por segundo da janela. Afeta fluidez visual e quanto tempo de CPU a interface tenta usar.',
            'paused': 'Pausa ou retoma o avanco da simulacao sem apagar agentes, comida ou obstaculos.',
            'population_min_rescue_enabled': 'Quando ativo, impede que a simulacao mate individuos abaixo do minimo configurado para aquela populacao.',
            'use_spatial': 'Usa uma grade espacial para acelerar buscas de proximidade, colisao, alimentacao e visao em populacoes grandes.',
            'retina_skip': 'Quantidade de frames que cada retina pode reutilizar a leitura anterior. Aumentar melhora desempenho, mas reduz precisao temporal da percepcao.',
            'random_seed': 'Seed do gerador aleatorio. Use -1 para aleatorio; use um numero fixo para repetir experimentos com o mesmo ponto de partida.',
            'retina_vision_mode': 'Modo de mapeamento da retina. single e mais rapido; fullbody considera o corpo inteiro dos objetos e e geometricamente mais fiel.',
            'simple_render': 'Troca para renderizacao mais simples e rapida. Use para populacoes grandes ou benchmarks visuais.',
            'reuse_spatial_grid': 'Reutiliza a estrutura da grade espacial entre frames quando possivel, reduzindo alocacoes.',
            'agents_inertia': 'Controla suavizacao da velocidade. 1 aplica o comando neural imediatamente; valores maiores deixam movimento mais inercial.',
            'allow_reverse_locomotion': 'Permite que a saida neural gere movimento para tras. Desligado preserva a locomocao historica apenas para frente.',
            'reproduction_min_age': 'Idade minima para um agente poder reproduzir. Ajuda a evitar reproducao imediata de recem-nascidos.',
            'reproduction_cooldown': 'Tempo minimo entre duas reproducoes do mesmo agente.',
            'show_selected_details': 'Mostra no canto da simulacao as metricas do agente selecionado: energia, idade, velocidade, retinas e rede neural.',
            'enable_brain_activations': 'Habilita calculo e exibicao das ativacoes neurais do agente selecionado. E util para diagnostico, mas tem custo extra.',
            'debug_tracebacks': 'Mostra tracebacks completos em erros da UI. Use para depurar; desligado deixa mensagens mais curtas.',
            'auto_export_substrate': 'Ativa salvamento automatico de snapshots do substrato em intervalos regulares.',
            'auto_export_interval_minutes': 'Intervalo, em minutos, entre exports automaticos do substrato.',
            'export_substrate_include_brain_activations': 'Inclui ativacoes neurais no snapshot exportado. Aumenta o arquivo e o custo de exportacao.',
            'export_substrate_pretty_json': 'Exporta JSON manual com indentacao legivel. Facilita inspecao humana, mas gera arquivos maiores.',
            'food_target': 'Quantidade alvo de comida. O controlador tenta repor comida ate aproximar esse valor.',
            'food_min_r': 'Raio minimo da comida nova. Afeta tamanho visual e energia disponivel por item.',
            'food_max_r': 'Raio maximo da comida nova. Tambem influencia energia e espaco ocupado.',
            'food_replenish_interval': 'Intervalo base de reposicao de comida. Valores menores repoe comida mais rapidamente.',
            'world_w': 'Largura do substrato retangular base.',
            'world_h': 'Altura do substrato retangular base.',
            'substrate_shape': 'Formato fisico do substrato: retangular ou circular.',
            'substrate_radius': 'Raio usado quando o substrato esta no modo circular.',
            'bacteria_count': 'Quantidade de bacterias criada ao resetar ou iniciar uma populacao nova.',
            'bacteria_min_limit': 'Numero minimo de bacterias que o sistema tenta preservar.',
            'bacteria_max_limit': 'Limite maximo de bacterias vivas permitido pela reproducao.',
            'bacteria_initial_energy': 'Energia inicial de bacterias novas criadas por reset ou spawn padrao.',
            'bacteria_death_energy': 'Energia abaixo da qual a bacteria vira candidata a morrer.',
            'bacteria_split_energy': 'Energia minima para a bacteria poder se dividir.',
            'bacteria_metab_v0_cost': 'Custo energetico por segundo quando a bacteria esta parada.',
            'bacteria_metab_vmax_cost': 'Custo energetico por segundo quando a bacteria se move perto da velocidade maxima.',
            'bacteria_energy_cap': 'Energia maxima que uma bacteria consegue armazenar.',
            'bacteria_body_size': 'Raio corporal da bacteria. Afeta colisao, renderizacao, area ocupada e posicionamento.',
            'bacteria_vision_radius': 'Distancia maxima que a retina da bacteria consegue perceber.',
            'bacteria_retina_count': 'Numero de raios/sensores da retina da bacteria. Mais retinas aumentam resolucao e custo.',
            'bacteria_retina_fov_degrees': 'Campo angular total de visao da bacteria, em graus.',
            'bacteria_max_speed': 'Velocidade maxima que a locomocao da bacteria pode atingir.',
            'bacteria_max_turn_deg': 'Velocidade maxima de rotacao da bacteria em graus por segundo.',
            'bacteria_hidden_layers': 'Quantidade de camadas ocultas no cerebro neural das novas bacterias. Alterar vivos pode recriar cerebros.',
            'bacteria_mutation_rate': 'Probabilidade de cada peso neural sofrer mutacao na reproducao.',
            'bacteria_mutation_strength': 'Intensidade/desvio das mutacoes numericas aplicadas aos pesos neurais.',
            'bacteria_show_vision': 'Desenha os raios de visao das bacterias quando habilitado.',
            'bacteria_retina_see_food': 'Define se a retina da bacteria detecta comida.',
            'bacteria_retina_see_bacteria': 'Define se a retina da bacteria detecta outras bacterias.',
            'bacteria_retina_see_predators': 'Define se a retina da bacteria detecta predadores.',
            'predators_enabled': 'Liga ou desliga criacao inicial e presenca configurada de predadores.',
            'predator_count': 'Quantidade de predadores criada ao resetar ou iniciar populacao nova.',
            'predator_min_limit': 'Numero minimo de predadores que o sistema tenta preservar.',
            'predator_max_limit': 'Limite maximo de predadores vivos permitido pela reproducao.',
            'predator_initial_energy': 'Energia inicial de predadores novos criados por reset ou spawn padrao.',
            'predator_death_energy': 'Energia abaixo da qual o predador vira candidato a morrer.',
            'predator_split_energy': 'Energia minima para o predador poder se dividir.',
            'predator_metab_v0_cost': 'Custo energetico por segundo quando o predador esta parado.',
            'predator_metab_vmax_cost': 'Custo energetico por segundo quando o predador se move perto da velocidade maxima.',
            'predator_energy_cap': 'Energia maxima que um predador consegue armazenar.',
            'predator_body_size': 'Raio corporal do predador. Afeta colisao, renderizacao, area ocupada e posicionamento.',
            'predator_vision_radius': 'Distancia maxima que a retina do predador consegue perceber.',
            'predator_retina_count': 'Numero de raios/sensores da retina do predador.',
            'predator_retina_fov_degrees': 'Campo angular total de visao do predador, em graus.',
            'predator_max_speed': 'Velocidade maxima que a locomocao do predador pode atingir.',
            'predator_max_turn_deg': 'Velocidade maxima de rotacao do predador em graus por segundo.',
            'predator_hidden_layers': 'Quantidade de camadas ocultas no cerebro neural dos novos predadores. Alterar vivos pode recriar cerebros.',
            'predator_mutation_rate': 'Probabilidade de cada peso neural do predador sofrer mutacao na reproducao.',
            'predator_mutation_strength': 'Intensidade/desvio das mutacoes numericas aplicadas aos pesos neurais do predador.',
            'predator_show_vision': 'Desenha os raios de visao dos predadores quando habilitado.',
            'predator_retina_see_food': 'Define se a retina do predador detecta comida.',
            'predator_retina_see_bacteria': 'Define se a retina do predador detecta bacterias.',
            'predator_retina_see_predators': 'Define se a retina do predador detecta outros predadores.',
            'obstacle_brush_width': 'Largura do pincel usado para desenhar ou apagar obstaculos solidos.',
            'obstacle_brush_erase': 'Quando ativo, o pincel apaga obstaculos em vez de desenhar novos.',
        }
        if name in help_by_name:
            return help_by_name[name]
        if name.startswith('bacteria_neurons_layer_'):
            layer = name.rsplit('_', 1)[-1]
            return f"Numero de neuronios na camada oculta {layer} das bacterias. Mudar isso em agentes vivos pode recriar o cerebro e apagar pesos atuais."
        if name.startswith('predator_neurons_layer_'):
            layer = name.rsplit('_', 1)[-1]
            return f"Numero de neuronios na camada oculta {layer} dos predadores. Mudar isso em agentes vivos pode recriar o cerebro e apagar pesos atuais."
        if name.startswith('test_param_'):
            return "Parametro experimental de teste da interface. Nao altera a simulacao principal."
        return f"{label} controla o parametro interno '{name}'. Clique no controle ao lado para alterar o valor."

    def _card_style(self) -> str:
        return (
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:28px; background:#1c1f24;} "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; margin-left:12px; "
            "padding:3px 12px 4px 12px; border-radius:8px; background:#262b31; color:#cfe1f5; "
            "font-weight:600; font-size:12px;}"
        )

    def _add_grid_param(self, grid: QGridLayout, row: int, label: str, name: str, widget: QWidget):
        self.widgets[name] = widget
        grid.addWidget(self._help_label(label, name), row, 0)
        grid.addWidget(widget, row, 1)
        return row + 1

    def _build_tab_genetic_editor(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Editor Genetico")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4, 4, 4, 4)
        outer.setSpacing(6)

        selector_row = QHBoxLayout()
        selector_row.addWidget(ClickHelpLabel("Tipo de organismo:", "Escolha qual template genetico sera editado. Os campos abaixo mudam entre bacteria e predador sem misturar com regras de populacao."))
        type_selector = QComboBox()
        type_selector.addItems(["Bacteria", "Predador"])
        type_selector.installEventFilter(self)
        selector_row.addWidget(type_selector)
        outer.addLayout(selector_row)

        stack = QStackedWidget()
        stack.addWidget(self._build_genetic_page('bacteria'))
        stack.addWidget(self._build_genetic_page('predator'))
        type_selector.currentIndexChanged.connect(stack.setCurrentIndex)
        outer.addWidget(stack, stretch=1)

    def _build_genetic_page(self, species: str) -> QWidget:
        page = QWidget()
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        holder = QWidget()
        scroll.setWidget(holder)
        root = QVBoxLayout(page)
        root.setContentsMargins(0, 0, 0, 0)
        root.addWidget(scroll)
        v = QVBoxLayout(holder)
        v.setContentsMargins(2, 2, 2, 20)
        v.setSpacing(18)
        card_style = self._card_style()

        is_bacteria = species == 'bacteria'
        defaults = {
            'initial_energy': 100.0,
            'death_energy': 0.0,
            'split_energy': 150.0,
            'metab_v0_cost': 0.5 if is_bacteria else 1.0,
            'metab_vmax_cost': 8.0 if is_bacteria else 15.0,
            'energy_cap': 400.0 if is_bacteria else 600.0,
            'body_size': 9.0 if is_bacteria else 14.0,
            'vision_radius': 120.0,
            'retina_count': 18,
            'retina_fov_degrees': 180.0,
            'max_speed': 300.0,
            'max_turn': math.pi,
            'hidden_layers': 4 if is_bacteria else 2,
            'mutation_rate': 0.05,
            'mutation_strength': 0.08,
        }

        g_energy = QGroupBox("Metabolismo & Energia")
        g_energy.setStyleSheet(card_style)
        grid = QGridLayout(g_energy)
        row = 0
        w = _spin_double(0.0, 200000.0, 1.0, 1); w.setValue(self.params.get(f'{species}_initial_energy', defaults['initial_energy'])); row = self._add_grid_param(grid, row, "Energia inicial:", f'{species}_initial_energy', w)
        w = _spin_double(0.0, 10000.0, 1.0, 1); w.setValue(self.params.get(f'{species}_death_energy', defaults['death_energy'])); row = self._add_grid_param(grid, row, "Energia morte:", f'{species}_death_energy', w)
        w = _spin_double(0.0, 400000.0, 5.0, 1); w.setValue(self.params.get(f'{species}_split_energy', defaults['split_energy'])); row = self._add_grid_param(grid, row, "Energia dividir:", f'{species}_split_energy', w)
        w = _spin_double(0.0, 5000.0, 0.01, 2); w.setValue(self.params.get(f'{species}_metab_v0_cost', defaults['metab_v0_cost'])); row = self._add_grid_param(grid, row, "Custo v=0 (s):", f'{species}_metab_v0_cost', w)
        w = _spin_double(0.0, 20000.0, 0.01, 2); w.setValue(self.params.get(f'{species}_metab_vmax_cost', defaults['metab_vmax_cost'])); row = self._add_grid_param(grid, row, "Custo v=vmax (s):", f'{species}_metab_vmax_cost', w)
        w = _spin_double(10.0, 1000000.0, 10.0, 1); w.setValue(self.params.get(f'{species}_energy_cap', defaults['energy_cap'])); row = self._add_grid_param(grid, row, "Cap energia:", f'{species}_energy_cap', w)
        v.addWidget(g_energy)

        g_body = QGroupBox("Corpo, Movimento & Sensores")
        g_body.setStyleSheet(card_style)
        grid = QGridLayout(g_body)
        row = 0
        w = _spin_double(1.0, 1000.0, 0.5, 1); w.setValue(self.params.get(f'{species}_body_size', defaults['body_size'])); row = self._add_grid_param(grid, row, "Tamanho corpo (raio):", f'{species}_body_size', w)
        w = _spin_double(1.0, 5000.0, 5.0, 1); w.setValue(self.params.get(f'{species}_vision_radius', defaults['vision_radius'])); row = self._add_grid_param(grid, row, "Raio visao:", f'{species}_vision_radius', w)
        w = _spin_int(1, 128); w.setValue(self.params.get(f'{species}_retina_count', defaults['retina_count'])); row = self._add_grid_param(grid, row, "Numero de retinas:", f'{species}_retina_count', w)
        w = _spin_double(1.0, 360.0, 1.0, 1); w.setValue(self.params.get(f'{species}_retina_fov_degrees', defaults['retina_fov_degrees'])); row = self._add_grid_param(grid, row, "Campo de visao (graus):", f'{species}_retina_fov_degrees', w)
        w = _spin_double(0.0, 10000.0, 10.0, 1); w.setValue(self.params.get(f'{species}_max_speed', defaults['max_speed'])); row = self._add_grid_param(grid, row, "Velocidade max:", f'{species}_max_speed', w)
        w = _spin_double(1.0, 5000.0, 1.0, 1); w.setValue(math.degrees(self.params.get(f'{species}_max_turn', defaults['max_turn']))); row = self._add_grid_param(grid, row, "Rotacao max (graus/s):", f'{species}_max_turn_deg', w)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_food', True)); row = self._add_grid_param(grid, row, "Ver comida:", f'{species}_retina_see_food', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_bacteria', False if is_bacteria else True)); row = self._add_grid_param(grid, row, "Ver bacterias:", f'{species}_retina_see_bacteria', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_predators', False)); row = self._add_grid_param(grid, row, "Ver predadores:", f'{species}_retina_see_predators', cb)
        v.addWidget(g_body)

        self._add_color_picker(v, species, card_style)

        g_brain = QGroupBox("Cerebro Neural & Mutacao")
        g_brain.setStyleSheet(card_style)
        grid = QGridLayout(g_brain)
        row = 0
        w = _spin_int(1, 5); w.setValue(self.params.get(f'{species}_hidden_layers', defaults['hidden_layers'])); row = self._add_grid_param(grid, row, "Camadas ocultas:", f'{species}_hidden_layers', w)
        neuron_widgets = []
        for i in range(1, 6):
            fallback = 20 if is_bacteria else (16 if i == 1 else (8 if i == 2 else 0))
            spin = _spin_int(0, 2048)
            spin.setValue(self.params.get(f'{species}_neurons_layer_{i}', fallback))
            row = self._add_grid_param(grid, row, f"Neuronios camada {i}:", f'{species}_neurons_layer_{i}', spin)
            neuron_widgets.append(spin)
        if is_bacteria:
            self._bacteria_neuron_widgets = neuron_widgets
        else:
            self._predator_neuron_widgets = neuron_widgets
        w = _spin_double(0.0, 1.0, 0.001, 3); w.setValue(self.params.get(f'{species}_mutation_rate', defaults['mutation_rate'])); row = self._add_grid_param(grid, row, "Taxa de mutacao:", f'{species}_mutation_rate', w)
        w = _spin_double(0.0, 10.0, 0.01, 2); w.setValue(self.params.get(f'{species}_mutation_strength', defaults['mutation_strength'])); row = self._add_grid_param(grid, row, "Forca de mutacao:", f'{species}_mutation_strength', w)
        v.addWidget(g_brain)

        hidden_spin = self.widgets[f'{species}_hidden_layers']
        def _update_neuron_enabled():
            layers = int(hidden_spin.value())
            for idx, spin in enumerate(neuron_widgets):
                spin.setEnabled(idx < layers)
        hidden_spin.valueChanged.connect(lambda _v: _update_neuron_enabled())
        _update_neuron_enabled()

        g_act = QGroupBox("Aplicacao do Template")
        g_act.setStyleSheet(card_style)
        la = QVBoxLayout(g_act)
        hint = QLabel("Esses botoes aplicam o template genetico/organismico. Mudancas de cerebro so devem ser aplicadas aos vivos quando voce aceitar reconstruir a rede.")
        hint.setWordWrap(True)
        la.addWidget(hint)
        if is_bacteria:
            buttons = [
                ("Aplicar a novos individuos", lambda _checked=False: self.apply_bacteria_params('template')),
                ("Aplicar a todos vivos", lambda _checked=False: self.apply_bacteria_params('all_alive')),
                ("Aplicar ao selecionado", lambda _checked=False: self.apply_bacteria_params('selected')),
            ]
        else:
            buttons = [
                ("Aplicar a novos individuos", lambda _checked=False: self.apply_predator_params('template')),
                ("Aplicar a todos vivos", lambda _checked=False: self.apply_predator_params('all_alive')),
                ("Aplicar ao selecionado", lambda _checked=False: self.apply_predator_params('selected')),
            ]
        for text, slot in buttons:
            btn = QPushButton(text)
            btn.clicked.connect(slot)
            la.addWidget(btn)
        v.addWidget(g_act)
        v.addStretch(1)
        return page

    def _add_color_picker(self, layout: QVBoxLayout, species: str, card_style: str):
        title = "Cor das bacterias" if species == 'bacteria' else "Cor dos predadores"
        key = f'{species}_color'
        default = (220, 220, 220) if species == 'bacteria' else (80, 120, 220)
        box = QGroupBox(title)
        box.setStyleSheet(card_style)
        row = QHBoxLayout(box)
        swatch = QLabel()
        swatch.setFixedSize(36, 36)
        color = self.params.get(key, default)
        swatch.setStyleSheet(f"background: rgb({color[0]},{color[1]},{color[2]}); border:1px solid #333; border-radius:4px;")
        if species == 'bacteria':
            self._swatch_bacteria = swatch
        else:
            self._swatch_predator = swatch
        button = QPushButton("Escolher cor")
        def _pick():
            current = self.params.get(key, color)
            col = QColorDialog.getColor(QColor(*current), self, title)
            if not col.isValid():
                return
            rgb = (col.red(), col.green(), col.blue())
            swatch.setStyleSheet(f"background: rgb({rgb[0]},{rgb[1]},{rgb[2]}); border:1px solid #333; border-radius:4px;")
            self.params.set(key, rgb, validate=False)
            entity_key = 'bacteria' if species == 'bacteria' else 'predators'
            for agent in self.engine.entities.get(entity_key, []):
                try:
                    agent.color = rgb
                except Exception:
                    pass
        button.clicked.connect(_pick)
        row.addWidget(swatch)
        row.addWidget(button)
        layout.addWidget(box)

    def _build_tab_population(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Populacao")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4, 4, 4, 4)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2, 2, 2, 20)
        v.setSpacing(18)
        card_style = self._card_style()

        g_b = QGroupBox("Bacterias")
        g_b.setStyleSheet(card_style)
        grid = QGridLayout(g_b)
        row = 0
        w = _spin_int(0, 20000); w.setValue(self.params.get('bacteria_count', 150)); row = self._add_grid_param(grid, row, "Quantidade inicial:", 'bacteria_count', w)
        w = _spin_int(0, 10000); w.setValue(self.params.get('bacteria_min_limit', 10)); row = self._add_grid_param(grid, row, "Minimo:", 'bacteria_min_limit', w)
        w = _spin_int(0, 50000); w.setValue(self.params.get('bacteria_max_limit', 300)); row = self._add_grid_param(grid, row, "Maximo:", 'bacteria_max_limit', w)
        v.addWidget(g_b)

        g_p = QGroupBox("Predadores")
        g_p.setStyleSheet(card_style)
        grid = QGridLayout(g_p)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('predators_enabled', False)); row = self._add_grid_param(grid, row, "Habilitar predadores:", 'predators_enabled', cb)
        w = _spin_int(0, 5000); w.setValue(self.params.get('predator_count', 0)); row = self._add_grid_param(grid, row, "Quantidade inicial:", 'predator_count', w)
        w = _spin_int(0, 5000); w.setValue(self.params.get('predator_min_limit', 0)); row = self._add_grid_param(grid, row, "Minimo:", 'predator_min_limit', w)
        w = _spin_int(0, 50000); w.setValue(self.params.get('predator_max_limit', 100)); row = self._add_grid_param(grid, row, "Maximo:", 'predator_max_limit', w)
        v.addWidget(g_p)

        g_rules = QGroupBox("Regras Populacionais")
        g_rules.setStyleSheet(card_style)
        grid = QGridLayout(g_rules)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('population_min_rescue_enabled', True)); row = self._add_grid_param(grid, row, "Resgate pop. minima:", 'population_min_rescue_enabled', cb)
        v.addWidget(g_rules)
        v.addStretch(1)

    def _build_tab_environment(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Ambiente")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4, 4, 4, 4)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2, 2, 2, 20)
        v.setSpacing(18)
        card_style = self._card_style()

        g_food = QGroupBox("Comida")
        g_food.setStyleSheet(card_style)
        grid = QGridLayout(g_food)
        row = 0
        w = _spin_int(0, 10000); w.setValue(self.params.get('food_target', 50)); row = self._add_grid_param(grid, row, "Target comida:", 'food_target', w)
        w = _spin_double(0.1, 100.0, 0.1, 2); w.setValue(self.params.get('food_min_r', 4.5)); row = self._add_grid_param(grid, row, "Comida raio min:", 'food_min_r', w)
        w = _spin_double(0.1, 100.0, 0.1, 2); w.setValue(self.params.get('food_max_r', 5.0)); row = self._add_grid_param(grid, row, "Comida raio max:", 'food_max_r', w)
        w = _spin_double(0.01, 60.0, 0.01, 2); w.setValue(self.params.get('food_replenish_interval', 0.1)); row = self._add_grid_param(grid, row, "Intervalo reposicao (s):", 'food_replenish_interval', w)
        v.addWidget(g_food)

        g_world = QGroupBox("Substrato")
        g_world.setStyleSheet(card_style)
        grid = QGridLayout(g_world)
        row = 0
        w = _spin_double(10.0, 20000.0, 10.0, 1); w.setValue(self.params.get('world_w', 1000.0)); row = self._add_grid_param(grid, row, "Largura do mundo:", 'world_w', w)
        w = _spin_double(10.0, 20000.0, 10.0, 1); w.setValue(self.params.get('world_h', 700.0)); row = self._add_grid_param(grid, row, "Altura do mundo:", 'world_h', w)
        shape = QComboBox(); shape.addItems(["rectangular", "circular"]); shape.setCurrentText(self.params.get('substrate_shape', 'rectangular')); row = self._add_grid_param(grid, row, "Formato do substrato:", 'substrate_shape', shape)
        w = _spin_double(1.0, 5000.0, 1.0, 1); w.setValue(self.params.get('substrate_radius', 400.0)); row = self._add_grid_param(grid, row, "Raio do substrato:", 'substrate_radius', w)
        v.addWidget(g_world)

        self._add_environment_color_pickers(v, card_style)

        g_act = QGroupBox("Acoes do Ambiente")
        g_act.setStyleSheet(card_style)
        la = QVBoxLayout(g_act)
        b = QPushButton("Aplicar ambiente")
        b.clicked.connect(self.apply_substrate_params)
        la.addWidget(b)
        row = QHBoxLayout()
        btn_export = QPushButton("Exportar substrato JSON")
        btn_export.clicked.connect(self.open_export_substrate_window)
        row.addWidget(btn_export)
        btn_import = QPushButton("Importar substrato JSON")
        btn_import.clicked.connect(self.open_import_substrate_window)
        row.addWidget(btn_import)
        wrap = QWidget()
        wrap.setLayout(row)
        la.addWidget(wrap)
        v.addWidget(g_act)
        v.addStretch(1)

    def _add_environment_color_pickers(self, layout: QVBoxLayout, card_style: str):
        def add_picker(title: str, key: str, default: tuple[int, int, int], apply_existing=None):
            box = QGroupBox(title)
            box.setStyleSheet(card_style)
            row = QHBoxLayout(box)
            swatch = QLabel()
            swatch.setFixedSize(36, 36)
            color = self.params.get(key, default)
            swatch.setStyleSheet(f"background: rgb({color[0]},{color[1]},{color[2]}); border:1px solid #333; border-radius:4px;")
            if key == 'substrate_bg_color':
                self._swatch_substrate = swatch
            button = QPushButton("Escolher cor")
            def _pick():
                current = self.params.get(key, color)
                col = QColorDialog.getColor(QColor(*current), self, title)
                if not col.isValid():
                    return
                rgb = (col.red(), col.green(), col.blue())
                swatch.setStyleSheet(f"background: rgb({rgb[0]},{rgb[1]},{rgb[2]}); border:1px solid #333; border-radius:4px;")
                self.params.set(key, rgb, validate=False)
                if apply_existing:
                    apply_existing(rgb)
            button.clicked.connect(_pick)
            row.addWidget(swatch)
            row.addWidget(button)
            layout.addWidget(box)

        add_picker("Cor da comida", 'food_color', (220, 30, 30),
                   lambda rgb: [setattr(food, 'color', rgb) for food in self.engine.entities.get('foods', [])])
        add_picker("Background Substrato", 'substrate_bg_color', (10, 10, 20))

    def _build_tab_experiment(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Experimento")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4, 4, 4, 4)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2, 2, 2, 20)
        v.setSpacing(18)
        card_style = self._card_style()

        g_time = QGroupBox("Tempo & Execucao")
        g_time.setStyleSheet(card_style)
        grid = QGridLayout(g_time)
        row = 0
        w = _spin_double(0.01, 100.0, 0.01, 3); w.setValue(self.params.get('time_scale', 1.0)); row = self._add_grid_param(grid, row, "Escala de tempo (x):", 'time_scale', w)
        w = _spin_int(1, 240); w.setValue(self.params.get('fps', 60)); row = self._add_grid_param(grid, row, "FPS:", 'fps', w)
        cb = QCheckBox(); cb.setChecked(self.params.get('paused', False)); row = self._add_grid_param(grid, row, "Pausado:", 'paused', cb)
        v.addWidget(g_time)

        g_perf = QGroupBox("Performance & Determinismo")
        g_perf.setStyleSheet(card_style)
        grid = QGridLayout(g_perf)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('use_spatial', True)); row = self._add_grid_param(grid, row, "Spatial Hash:", 'use_spatial', cb)
        w = _spin_int(0, 10); w.setValue(self.params.get('retina_skip', 0)); row = self._add_grid_param(grid, row, "Retina skip:", 'retina_skip', w)
        w = _spin_int(-1, 2147483647); w.setValue(int(self.params.get('random_seed', -1))); row = self._add_grid_param(grid, row, "Seed RNG (-1 aleatoria):", 'random_seed', w)
        mode = QComboBox(); mode.addItems(['single', 'fullbody']); mode.setCurrentText(self.params.get('retina_vision_mode', 'single')); row = self._add_grid_param(grid, row, "Visao retinas:", 'retina_vision_mode', mode)
        cb = QCheckBox(); cb.setChecked(self.params.get('reuse_spatial_grid', True)); row = self._add_grid_param(grid, row, "Reutilizar grid espacial:", 'reuse_spatial_grid', cb)
        w = _spin_double(0.1, 10.0, 0.1, 2); w.setValue(self.params.get('agents_inertia', 1.0)); row = self._add_grid_param(grid, row, "Inercia global:", 'agents_inertia', w)
        cb = QCheckBox(); cb.setChecked(self.params.get('allow_reverse_locomotion', False)); row = self._add_grid_param(grid, row, "Permitir marcha re:", 'allow_reverse_locomotion', cb)
        w = _spin_double(0.0, 3600.0, 0.1, 2); w.setValue(self.params.get('reproduction_min_age', 0.0)); row = self._add_grid_param(grid, row, "Idade min. reproducao:", 'reproduction_min_age', w)
        w = _spin_double(0.0, 3600.0, 0.1, 2); w.setValue(self.params.get('reproduction_cooldown', 0.0)); row = self._add_grid_param(grid, row, "Cooldown reproducao:", 'reproduction_cooldown', w)
        v.addWidget(g_perf)

        g_auto = QGroupBox("Preferencias & Auto Export")
        g_auto.setStyleSheet(card_style)
        grid = QGridLayout(g_auto)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('auto_export_substrate', False)); cb.toggled.connect(self._on_toggle_auto_export); row = self._add_grid_param(grid, row, "Auto Export Substrato:", 'auto_export_substrate', cb)
        w = _spin_double(0.1, 1440.0, 0.5, 2); w.setValue(self.params.get('auto_export_interval_minutes', 10.0)); row = self._add_grid_param(grid, row, "Intervalo export (min):", 'auto_export_interval_minutes', w)
        cb = QCheckBox(); cb.setChecked(self.params.get('export_substrate_include_brain_activations', False)); row = self._add_grid_param(grid, row, "Exportar ativacoes neurais:", 'export_substrate_include_brain_activations', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get('export_substrate_pretty_json', False)); row = self._add_grid_param(grid, row, "JSON legivel manual:", 'export_substrate_pretty_json', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get('debug_tracebacks', False)); row = self._add_grid_param(grid, row, "Tracebacks no debug:", 'debug_tracebacks', cb)
        def _on_interval_changed(_):
            if bool(self._get_widget_value('auto_export_substrate')):
                self._reschedule_auto_export()
        w.valueChanged.connect(_on_interval_changed)
        v.addWidget(g_auto)

        g_act = QGroupBox("Acoes")
        g_act.setStyleSheet(card_style)
        la = QVBoxLayout(g_act)
        buttons = [
            ("Aplicar TODOS", self.apply_all_params),
            ("Iniciar", self.start_simulation),
            ("Resetar Populacao", self.reset_population),
            ("Salvar preferencias UI", self.save_ui_params),
        ]
        for text, slot in buttons:
            b = QPushButton(text)
            b.clicked.connect(slot)
            la.addWidget(b)
        v.addWidget(g_act)
        v.addStretch(1)

    # ---------------------- Tabs: Simulation -------------------------
    def _build_tab_simulation(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Simulação")
        outer = QVBoxLayout(tab); outer.setContentsMargins(4,4,4,4); outer.setSpacing(6)
        scroll = QScrollArea(); scroll.setWidgetResizable(True); outer.addWidget(scroll)
        content = QWidget(); scroll.setWidget(content)
        v = QVBoxLayout(content); v.setContentsMargins(2,2,2,20); v.setSpacing(18)
        card_style = (
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:28px; background:#1c1f24;} "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; margin-left:12px; padding:3px 12px 4px 12px; border-radius:8px; background:#262b31; color:#cfe1f5; font-weight:600; font-size:12px;}"
        )
        # Grupo: Tempo & Execução
        g_exec = QGroupBox("Tempo & Execução"); g_exec.setStyleSheet(card_style); grid = QGridLayout(g_exec); r_exec=0
        def add_exec(label,name,w):
            nonlocal r_exec
            self.widgets[name]=w; grid.addWidget(self._help_label(label, name), r_exec,0); grid.addWidget(w,r_exec,1); r_exec+=1
        w=_spin_double(0.01,100.0,0.01,3); w.setValue(self.params.get('time_scale',1.0)); add_exec("Escala de tempo (x):",'time_scale',w)
        w=_spin_int(1,240); w.setValue(self.params.get('fps',60)); add_exec("FPS:",'fps',w)
        cb=QCheckBox(); cb.setChecked(self.params.get('paused',False)); add_exec("Pausado:",'paused',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('population_min_rescue_enabled',True)); add_exec("Resgate pop. minima:",'population_min_rescue_enabled',cb)
        v.addWidget(g_exec)
        # Grupo: Performance & Render
        g_perf = QGroupBox("Performance & Render")
        g_perf.setStyleSheet(card_style)
        grid2 = QGridLayout(g_perf)
        r_perf = 0
        def add_perf(label, name, w):
            nonlocal r_perf
            self.widgets[name] = w
            grid2.addWidget(self._help_label(label, name), r_perf, 0)
            grid2.addWidget(w, r_perf, 1)
            r_perf += 1

        cb = QCheckBox()
        cb.setChecked(self.params.get('use_spatial', True))
        add_perf("Spatial Hash:", 'use_spatial', cb)

        w = _spin_int(0, 10)
        w.setValue(self.params.get('retina_skip', 0))
        add_perf("Retina skip:", 'retina_skip', w)

        w = _spin_int(-1, 2147483647)
        w.setValue(int(self.params.get('random_seed', -1)))
        add_perf("Seed RNG (-1 aleatoria):", 'random_seed', w)

        # Retina vision mode selector (single = centroid per object, fullbody = span-aware)
        mode_cb = QComboBox()
        mode_cb.addItems(['single', 'fullbody'])
        mode_cb.setCurrentText(self.params.get('retina_vision_mode', 'single'))
        add_perf("Visão retinas:", 'retina_vision_mode', mode_cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('simple_render', False))
        add_perf("Renderização simples:", 'simple_render', cb)

        cb2 = QCheckBox()
        cb2.setChecked(self.params.get('reuse_spatial_grid', True))
        add_perf("Reutilizar grid espacial:", 'reuse_spatial_grid', cb2)

        w2 = _spin_double(0.1, 10.0, 0.1, 2)
        w2.setValue(self.params.get('agents_inertia', 1.0))
        add_perf("Inércia global:", 'agents_inertia', w2)

        cb = QCheckBox()
        cb.setChecked(self.params.get('allow_reverse_locomotion', False))
        add_perf("Permitir marcha re:", 'allow_reverse_locomotion', cb)

        w = _spin_double(0.0, 3600.0, 0.1, 2)
        w.setValue(self.params.get('reproduction_min_age', 0.0))
        add_perf("Idade min. reproducao:", 'reproduction_min_age', w)

        w = _spin_double(0.0, 3600.0, 0.1, 2)
        w.setValue(self.params.get('reproduction_cooldown', 0.0))
        add_perf("Cooldown reproducao:", 'reproduction_cooldown', w)

        v.addWidget(g_perf)
        # Grupo: Visualização / Debug
        g_vis = QGroupBox("Visualização / Debug"); g_vis.setStyleSheet(card_style); grid3=QGridLayout(g_vis); r_vis=0
        def add_vis(label,name,w):
            nonlocal r_vis
            self.widgets[name]=w; grid3.addWidget(self._help_label(label, name), r_vis,0); grid3.addWidget(w,r_vis,1); r_vis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('show_selected_details',True)); add_vis("Detalhes agente selecionado:",'show_selected_details',cb)
        cb=QCheckBox(); cb.setChecked(not self.params.get('disable_brain_activations',False)); cb.toggled.connect(self._on_toggle_brain_activations); add_vis("Mostrar ativações neurais:",'enable_brain_activations',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('debug_tracebacks',False)); add_vis("Tracebacks no debug:",'debug_tracebacks',cb)
        v.addWidget(g_vis)
        # Grupo: Auto Export
        g_auto = QGroupBox("Auto Export"); g_auto.setStyleSheet(card_style); grid4=QGridLayout(g_auto); r_auto=0
        def add_auto(label,name,w):
            nonlocal r_auto
            self.widgets[name]=w; grid4.addWidget(self._help_label(label, name), r_auto,0); grid4.addWidget(w,r_auto,1); r_auto+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('auto_export_substrate',False)); cb.toggled.connect(self._on_toggle_auto_export); add_auto("Auto Export Substrato:",'auto_export_substrate',cb)
        w=_spin_double(0.1,1440.0,0.5,2); w.setValue(self.params.get('auto_export_interval_minutes',10.0)); add_auto("Intervalo export (min):",'auto_export_interval_minutes',w)
        cb=QCheckBox(); cb.setChecked(self.params.get('export_substrate_include_brain_activations',False)); add_auto("Exportar ativacoes neurais:",'export_substrate_include_brain_activations',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('export_substrate_pretty_json',False)); add_auto("JSON legivel manual:",'export_substrate_pretty_json',cb)
        # Quando o intervalo muda e o auto-export estiver ativo, reagenda imediatamente
        def _on_interval_changed(_):
            if bool(self._get_widget_value('auto_export_substrate')):
                self._reschedule_auto_export()
        w.valueChanged.connect(_on_interval_changed)
        v.addWidget(g_auto)
        # Grupo: Ações
        g_act = QGroupBox("Ações"); g_act.setStyleSheet(card_style); act_layout = QVBoxLayout(g_act); act_layout.setSpacing(4)
        buttons = [
            ("Aplicar Parâmetros", self.apply_simulation_params),
            ("Aplicar TODOS", self.apply_all_params),
            ("Iniciar", self.start_simulation),
            ("Resetar População", self.reset_population),
            ("Salvar Params", self.save_ui_params),
            ("Exportar Agente", self.open_export_agent_window),
            ("Carregar Agente", self.open_load_agent_window),
        ]
        for text, slot in buttons:
            b = QPushButton(text); b.clicked.connect(slot); act_layout.addWidget(b)
        v.addWidget(g_act)
        v.addStretch(1)

    # test playground removed; color pickers moved into respective tabs (non-destructive previews)

    # ---------------------- Tab: Substrate ----------------------------
    def _build_tab_substrate(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Substrato")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4,4,4,4)
        outer.setSpacing(6)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2,2,2,20)
        v.setSpacing(18)
        card_style = (
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:28px; background:#1c1f24;} "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; margin-left:12px; padding:3px 12px 4px 12px; border-radius:8px; background:#262b31; color:#cfe1f5; font-weight:600; font-size:12px;}"
        )

        # Grupo Comida
        g_food = QGroupBox("Comida")
        g_food.setStyleSheet(card_style)
        gf = QGridLayout(g_food)
        r_food = 0
        def add_food(label, name, w):
            nonlocal r_food
            self.widgets[name] = w
            gf.addWidget(self._help_label(label, name), r_food, 0)
            gf.addWidget(w, r_food, 1)
            r_food += 1

        w = _spin_int(0,10000); w.setValue(self.params.get('food_target',50)); add_food("Target comida:", 'food_target', w)
        w = _spin_double(0.1,100.0,0.1,2); w.setValue(self.params.get('food_min_r',4.5)); add_food("Comida raio mín:", 'food_min_r', w)
        w = _spin_double(0.1,100.0,0.1,2); w.setValue(self.params.get('food_max_r',5.0)); add_food("Comida raio máx:", 'food_max_r', w)
        w = _spin_double(0.01,60.0,0.01,2); w.setValue(self.params.get('food_replenish_interval',0.1)); add_food("Intervalo reposição (s):", 'food_replenish_interval', w)
        v.addWidget(g_food)

        # Color picker for food (non-destructive UI -> updates params & entities)
        try:
            food_picker_box = QGroupBox("Cor da comida")
            food_picker_box.setStyleSheet(card_style)
            fp_layout = QHBoxLayout(food_picker_box)
            food_swatch = QLabel(); food_swatch.setFixedSize(36,36)
            fcol = self.params.get('food_color', (220,30,30))
            food_swatch.setStyleSheet(f"background: rgb({fcol[0]},{fcol[1]},{fcol[2]}); border:1px solid #333; border-radius:4px;")
            btn_food = QPushButton("Escolher cor da comida")
            def _pick_food_color():
                col = QColorDialog.getColor(QColor(*fcol), self, "Escolha cor da comida")
                if col.isValid():
                    r,g,b = col.red(), col.green(), col.blue()
                    food_swatch.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                    # persist and propagate
                    try:
                        self.params.set('food_color', (r,g,b))
                        if hasattr(self, 'engine') and self.engine is not None:
                            for food in self.engine.entities.get('foods', []):
                                try: food.color = (r,g,b)
                                except Exception: pass
                    except Exception:
                        pass
            btn_food.clicked.connect(_pick_food_color)
            fp_layout.addWidget(food_swatch); fp_layout.addWidget(btn_food)
            v.addWidget(food_picker_box)
        except Exception:
            pass

        # Grupo Mundo
        g_world = QGroupBox("Mundo")
        g_world.setStyleSheet(card_style)
        gw = QGridLayout(g_world)
        r_world = 0
        def add_world(label, name, w):
            nonlocal r_world
            self.widgets[name] = w
            gw.addWidget(self._help_label(label, name), r_world, 0)
            gw.addWidget(w, r_world, 1)
            r_world += 1

        w = _spin_double(10.0,20000.0,10.0,1); w.setValue(self.params.get('world_w',1000.0)); add_world("Largura do mundo:", 'world_w', w)
        w = _spin_double(10.0,20000.0,10.0,1); w.setValue(self.params.get('world_h',700.0)); add_world("Altura do mundo:", 'world_h', w)
        shape = QComboBox(); shape.addItems(["rectangular","circular"]); shape.setCurrentText(self.params.get('substrate_shape','rectangular')); add_world("Formato do substrato:", 'substrate_shape', shape)
        w = _spin_double(1.0,5000.0,1.0,1); w.setValue(self.params.get('substrate_radius',400.0)); add_world("Raio do substrato:", 'substrate_radius', w)
        v.addWidget(g_world)

        # Color picker for substrate background (updates params)
        try:
            sub_picker_box = QGroupBox("Background Substrato")
            sub_picker_box.setStyleSheet(card_style)
            sp_layout = QHBoxLayout(sub_picker_box)
            sub_swatch = QLabel(); sub_swatch.setFixedSize(36,36)
            # keep reference for persistence updates
            self._swatch_substrate = sub_swatch
            sbg = self.params.get('substrate_bg_color', (10,10,20))
            sub_swatch.setStyleSheet(f"background: rgb({sbg[0]},{sbg[1]},{sbg[2]}); border:1px solid #333; border-radius:4px;")
            btn_sub = QPushButton("Escolher cor do substrato")
            def _pick_sub_color():
                col = QColorDialog.getColor(QColor(*sbg), self, "Escolha cor do substrato")
                if col.isValid():
                    r,g,b = col.red(), col.green(), col.blue()
                    sub_swatch.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                    try:
                        self.params.set('substrate_bg_color', (r,g,b))
                    except Exception:
                        pass
            btn_sub.clicked.connect(_pick_sub_color)
            sp_layout.addWidget(sub_swatch); sp_layout.addWidget(btn_sub)
            v.addWidget(sub_picker_box)
        except Exception:
            pass

        

        # Grupo Ações
        g_act = QGroupBox("Ações")
        g_act.setStyleSheet(card_style)
        la = QVBoxLayout(g_act)
        b = QPushButton("Aplicar Parâmetros"); b.clicked.connect(self.apply_substrate_params); la.addWidget(b)
        h = QHBoxLayout(); btn_export = QPushButton("Exportar Substrato"); btn_export.clicked.connect(self.open_export_substrate_window); h.addWidget(btn_export)
        btn_import = QPushButton("Importar Substrato"); btn_import.clicked.connect(self.open_import_substrate_window); h.addWidget(btn_import)
        wrap = QWidget(); wrap.setLayout(h); la.addWidget(wrap)
        v.addWidget(g_act)
        v.addStretch(1)

    # ---------------------- Tab: Bacteria -----------------------------
    def _build_tab_bacteria(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Bactérias")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4,4,4,4)
        outer.setSpacing(6)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2,2,2,20)
        v.setSpacing(18)
        card_style = (
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:28px; background:#1c1f24;} "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; margin-left:12px; padding:3px 12px 4px 12px; border-radius:8px; background:#262b31; color:#cfe1f5; font-weight:600; font-size:12px;}"
        )

        # Grupo População & Energia
        g_pop = QGroupBox("População & Energia")
        g_pop.setStyleSheet(card_style)
        gp = QGridLayout(g_pop)
        r_bpop = 0
        def add_pop(label,name,w):
            nonlocal r_bpop
            self.widgets[name]=w
            gp.addWidget(self._help_label(label, name), r_bpop,0)
            gp.addWidget(w, r_bpop,1)
            r_bpop += 1

        w = _spin_int(0,20000); w.setValue(self.params.get('bacteria_count',150)); add_pop("Quantidade inicial:",'bacteria_count',w)
        w = _spin_int(0,10000); w.setValue(self.params.get('bacteria_min_limit',10)); add_pop("Mínimo:",'bacteria_min_limit',w)
        w = _spin_int(0,50000); w.setValue(self.params.get('bacteria_max_limit',300)); add_pop("Máximo:",'bacteria_max_limit',w)
        w = _spin_double(0.0,100000.0,1.0,1); w.setValue(self.params.get('bacteria_initial_energy',100.0)); add_pop("Energia inicial:",'bacteria_initial_energy',w)
        w = _spin_double(0.0,10000.0,1.0,1); w.setValue(self.params.get('bacteria_death_energy',0.0)); add_pop("Energia morte:",'bacteria_death_energy',w)
        w = _spin_double(0.0,200000.0,5.0,1); w.setValue(self.params.get('bacteria_split_energy',150.0)); add_pop("Energia dividir:",'bacteria_split_energy',w)
        # Metabolismo contínuo
        w = _spin_double(0.0,1000.0,0.01,2); w.setValue(self.params.get('bacteria_metab_v0_cost',0.5)); add_pop("Custo v=0 (s):",'bacteria_metab_v0_cost',w)
        w = _spin_double(0.0,10000.0,0.01,2); w.setValue(self.params.get('bacteria_metab_vmax_cost',8.0)); add_pop("Custo v=vmax (s):",'bacteria_metab_vmax_cost',w)
        w = _spin_double(10.0,1000000.0,10.0,1); w.setValue(self.params.get('bacteria_energy_cap',400.0)); add_pop("Cap energia:",'bacteria_energy_cap',w)
        v.addWidget(g_pop)

        # Corpo & Movimento
        g_body = QGroupBox("Corpo & Movimento")
        g_body.setStyleSheet(card_style)
        gb = QGridLayout(g_body)
        r_bbody = 0
        def add_body(label,name,w):
            nonlocal r_bbody
            self.widgets[name]=w
            gb.addWidget(self._help_label(label, name), r_bbody,0)
            gb.addWidget(w, r_bbody,1)
            r_bbody += 1

        w = _spin_double(1.0,500.0,0.5,1); w.setValue(self.params.get('bacteria_body_size',9.0)); add_body("Tamanho corpo (raio):",'bacteria_body_size',w)
        w = _spin_double(1.0,5000.0,5.0,1); w.setValue(self.params.get('bacteria_vision_radius',120.0)); add_body("Raio visão:",'bacteria_vision_radius',w)
        w = _spin_int(1,128); w.setValue(self.params.get('bacteria_retina_count',18)); add_body("Número de retinas:",'bacteria_retina_count',w)
        w = _spin_double(1.0,360.0,1.0,1); w.setValue(self.params.get('bacteria_retina_fov_degrees',180.0)); add_body("Campo de visão (°):",'bacteria_retina_fov_degrees',w)
        w = _spin_double(0.0,10000.0,10.0,1); w.setValue(self.params.get('bacteria_max_speed',300.0)); add_body("Velocidade máx:",'bacteria_max_speed',w)
        w = _spin_double(1.0,5000.0,1.0,1); w.setValue(math.degrees(self.params.get('bacteria_max_turn', math.pi))); add_body("Rotação máx (°/s):",'bacteria_max_turn_deg',w)
        v.addWidget(g_body)

        # Color picker for bacteria body color
        try:
            bac_picker_box = QGroupBox("Cor das bactérias")
            bac_picker_box.setStyleSheet(card_style)
            bpc_layout = QHBoxLayout(bac_picker_box)
            b_swatch = QLabel(); b_swatch.setFixedSize(36,36)
            # keep reference for persistence updates
            self._swatch_bacteria = b_swatch
            bcol = self.params.get('bacteria_color', (220,220,220))
            b_swatch.setStyleSheet(f"background: rgb({bcol[0]},{bcol[1]},{bcol[2]}); border:1px solid #333; border-radius:4px;")
            btn_bac = QPushButton("Escolher cor das bactérias")
            def _pick_bac_color():
                col = QColorDialog.getColor(QColor(*bcol), self, "Escolha cor das bactérias")
                if col.isValid():
                    r,g,b = col.red(), col.green(), col.blue()
                    b_swatch.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                    try:
                        self.params.set('bacteria_color', (r,g,b))
                        if hasattr(self, 'engine') and self.engine is not None:
                            for bact in self.engine.entities.get('bacteria', []):
                                try: bact.color = (r,g,b)
                                except Exception: pass
                    except Exception:
                        pass
            btn_bac.clicked.connect(_pick_bac_color)
            bpc_layout.addWidget(b_swatch); bpc_layout.addWidget(btn_bac)
            v.addWidget(bac_picker_box)
        except Exception:
            pass

        

        # Rede Neural & Mutação
        g_nn = QGroupBox("Rede Neural & Mutação"); g_nn.setStyleSheet(card_style); gn = QGridLayout(g_nn); r_bnn=0
        def add_nn(label,name,w):
            nonlocal r_bnn
            self.widgets[name]=w; gn.addWidget(self._help_label(label, name), r_bnn,0); gn.addWidget(w,r_bnn,1); r_bnn+=1
        w=_spin_int(1,5); w.setValue(self.params.get('bacteria_hidden_layers',4)); add_nn("Camadas ocultas:",'bacteria_hidden_layers',w)
        self._bacteria_neuron_widgets=[]
        for i in range(1,6):
            spin=_spin_int(0,2048); spin.setValue(self.params.get(f'bacteria_neurons_layer_{i}',20 if i<=4 else 0)); add_nn(f"Neurônios camada {i}:",f'bacteria_neurons_layer_{i}',spin); self._bacteria_neuron_widgets.append(spin)
        w=_spin_double(0.0,1.0,0.001,3); w.setValue(self.params.get('bacteria_mutation_rate',0.05)); add_nn("Taxa de mutação:",'bacteria_mutation_rate',w)
        w=_spin_double(0.0,10.0,0.01,2); w.setValue(self.params.get('bacteria_mutation_strength',0.08)); add_nn("Força de mutação:",'bacteria_mutation_strength',w)
        v.addWidget(g_nn)
        # Visão
        g_vis = QGroupBox("Visão"); g_vis.setStyleSheet(card_style); gv=QGridLayout(g_vis); r_bvis=0
        def add_vis(label,name,w):
            nonlocal r_bvis
            self.widgets[name]=w; gv.addWidget(self._help_label(label, name), r_bvis,0); gv.addWidget(w,r_bvis,1); r_bvis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_show_vision',False)); add_vis("Mostrar visão:",'bacteria_show_vision',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_food',True)); add_vis("Ver comida:",'bacteria_retina_see_food',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_bacteria',False)); add_vis("Ver bactérias:",'bacteria_retina_see_bacteria',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_predators',False)); add_vis("Ver predadores:",'bacteria_retina_see_predators',cb)
        v.addWidget(g_vis)
        # Ações
        g_act = QGroupBox("Ações"); g_act.setStyleSheet(card_style); la=QVBoxLayout(g_act)
        hint = QLabel("Novos altera o template. Vivos/Selecionado tambem atualiza agentes existentes; mudancas na rede podem recriar o cerebro.")
        hint.setWordWrap(True)
        la.addWidget(hint)
        b=QPushButton("Aplicar a novos individuos")
        b.clicked.connect(lambda _checked=False: self.apply_bacteria_params('template'))
        la.addWidget(b)
        b=QPushButton("Aplicar a todos vivos")
        b.clicked.connect(lambda _checked=False: self.apply_bacteria_params('all_alive'))
        la.addWidget(b)
        b=QPushButton("Aplicar ao selecionado")
        b.clicked.connect(lambda _checked=False: self.apply_bacteria_params('selected'))
        la.addWidget(b)
        v.addWidget(g_act)
        v.addStretch(1)
        hidden_layers_spin=self.widgets['bacteria_hidden_layers']
        def _update_bacteria_neurons():
            layers=int(hidden_layers_spin.value())
            for idx,spin in enumerate(self._bacteria_neuron_widgets): spin.setEnabled(idx<layers)
        hidden_layers_spin.valueChanged.connect(_update_bacteria_neurons); _update_bacteria_neurons()

    # ---------------------- Tab: Predator -----------------------------
    def _build_tab_predator(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Predadores")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4,4,4,4)
        outer.setSpacing(6)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2,2,2,20)
        v.setSpacing(18)
        card_style = (
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:28px; background:#1c1f24;} "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; margin-left:12px; padding:3px 12px 4px 12px; border-radius:8px; background:#262b31; color:#cfe1f5; font-weight:600; font-size:12px;}"
        )

        # População & Energia
        g_pop = QGroupBox("População & Energia")
        g_pop.setStyleSheet(card_style)
        gp = QGridLayout(g_pop)
        r_ppop = 0
        def add_pop(label,name,w):
            nonlocal r_ppop
            self.widgets[name]=w
            gp.addWidget(self._help_label(label, name), r_ppop,0)
            gp.addWidget(w, r_ppop,1)
            r_ppop += 1

        cb=QCheckBox(); cb.setChecked(self.params.get('predators_enabled',False)); add_pop("Habilitar predadores",'predators_enabled',cb)
        w=_spin_int(0,5000); w.setValue(self.params.get('predator_count',0)); add_pop("Quantidade inicial:",'predator_count',w)
        w=_spin_int(0,5000); w.setValue(self.params.get('predator_min_limit',0)); add_pop("Mínimo:",'predator_min_limit',w)
        w=_spin_int(0,50000); w.setValue(self.params.get('predator_max_limit',100)); add_pop("Máximo:",'predator_max_limit',w)
        w=_spin_double(0.0,200000.0,1.0,1); w.setValue(self.params.get('predator_initial_energy',100.0)); add_pop("Energia inicial:",'predator_initial_energy',w)
        w=_spin_double(0.0,10000.0,1.0,1); w.setValue(self.params.get('predator_death_energy',0.0)); add_pop("Energia morte:",'predator_death_energy',w)
        w=_spin_double(0.0,400000.0,10.0,1); w.setValue(self.params.get('predator_split_energy',150.0)); add_pop("Energia dividir:",'predator_split_energy',w)
        # Metabolismo contínuo
        w=_spin_double(0.0,5000.0,0.01,2); w.setValue(self.params.get('predator_metab_v0_cost',1.0)); add_pop("Custo v=0 (s):",'predator_metab_v0_cost',w)
        w=_spin_double(0.0,20000.0,0.01,2); w.setValue(self.params.get('predator_metab_vmax_cost',15.0)); add_pop("Custo v=vmax (s):",'predator_metab_vmax_cost',w)
        w=_spin_double(10.0,1000000.0,10.0,1); w.setValue(self.params.get('predator_energy_cap',600.0)); add_pop("Cap energia:",'predator_energy_cap',w)
        v.addWidget(g_pop)

        # Predator color selector
        pred_color_box = QGroupBox("Cor dos predadores")
        pred_color_box.setStyleSheet(card_style)
        try:
            pc_layout = QHBoxLayout(pred_color_box)
            p_swatch = QLabel(); p_swatch.setFixedSize(36,36)
            # keep reference for persistence updates
            self._swatch_predator = p_swatch
            pcol = self.params.get('predator_color', (80,120,220))
            p_swatch.setStyleSheet(f"background: rgb({pcol[0]},{pcol[1]},{pcol[2]}); border:1px solid #333; border-radius:4px;")
            btn_pred = QPushButton("Escolher cor dos predadores")
            def _pick_pred_color():
                col = QColorDialog.getColor(QColor(*pcol), self, "Escolha cor dos predadores")
                if col.isValid():
                    r,g,b = col.red(), col.green(), col.blue()
                    p_swatch.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                    try:
                        self.params.set('predator_color', (r,g,b))
                        if hasattr(self, 'engine') and self.engine is not None:
                            for pred in self.engine.entities.get('predators', []):
                                try: pred.color = (r,g,b)
                                except Exception: pass
                    except Exception:
                        pass
            btn_pred.clicked.connect(_pick_pred_color)
            pc_layout.addWidget(p_swatch); pc_layout.addWidget(btn_pred)
            v.addWidget(pred_color_box)
        except Exception:
            v.addWidget(pred_color_box)

        

        # Corpo & Movimento
        g_body = QGroupBox("Corpo & Movimento"); g_body.setStyleSheet(card_style); gb=QGridLayout(g_body); r_pbody=0
        def add_body(label,name,w):
            nonlocal r_pbody
            self.widgets[name]=w; gb.addWidget(self._help_label(label, name), r_pbody,0); gb.addWidget(w,r_pbody,1); r_pbody+=1
        w=_spin_double(1.0,1000.0,0.5,1); w.setValue(self.params.get('predator_body_size',14.0)); add_body("Tamanho corpo (raio):",'predator_body_size',w)
        w=_spin_double(1.0,5000.0,10.0,1); w.setValue(self.params.get('predator_vision_radius',120.0)); add_body("Raio visão:",'predator_vision_radius',w)
        w=_spin_int(1,128); w.setValue(self.params.get('predator_retina_count',18)); add_body("Qtd retinas:",'predator_retina_count',w)
        w=_spin_double(1.0,360.0,1.0,1); w.setValue(self.params.get('predator_retina_fov_degrees',180.0)); add_body("FOV (graus):",'predator_retina_fov_degrees',w)
        w=_spin_double(0.0,10000.0,10.0,1); w.setValue(self.params.get('predator_max_speed',300.0)); add_body("Velocidade máx:",'predator_max_speed',w)
        w=_spin_double(1.0,5000.0,1.0,1); w.setValue(math.degrees(self.params.get('predator_max_turn', math.pi))); add_body("Rotação máx (°/s):",'predator_max_turn_deg',w)
        v.addWidget(g_body)
        # Rede Neural & Mutação
        g_nn = QGroupBox("Rede Neural & Mutação"); g_nn.setStyleSheet(card_style); gn=QGridLayout(g_nn); r_pnn=0
        def add_nn(label,name,w):
            nonlocal r_pnn
            self.widgets[name]=w; gn.addWidget(self._help_label(label, name), r_pnn,0); gn.addWidget(w,r_pnn,1); r_pnn+=1
        w=_spin_int(1,5); w.setValue(self.params.get('predator_hidden_layers',2)); add_nn("Camadas ocultas:",'predator_hidden_layers',w)
        self._predator_neuron_widgets=[]
        for i in range(1,6):
            spin=_spin_int(0,2048); spin.setValue(self.params.get(f'predator_neurons_layer_{i}',16 if i==1 else (8 if i==2 else 0))); add_nn(f"Neurônios camada {i}:",f'predator_neurons_layer_{i}',spin); self._predator_neuron_widgets.append(spin)
        w=_spin_double(0.0,1.0,0.001,3); w.setValue(self.params.get('predator_mutation_rate',0.05)); add_nn("Taxa de mutação:",'predator_mutation_rate',w)
        w=_spin_double(0.0,10.0,0.01,2); w.setValue(self.params.get('predator_mutation_strength',0.08)); add_nn("Força de mutação:",'predator_mutation_strength',w)
        v.addWidget(g_nn)
        # Visão
        g_vis = QGroupBox("Visão"); g_vis.setStyleSheet(card_style); gv=QGridLayout(g_vis); r_pvis=0
        def add_vis(label,name,w):
            nonlocal r_pvis
            self.widgets[name]=w; gv.addWidget(self._help_label(label, name), r_pvis,0); gv.addWidget(w,r_pvis,1); r_pvis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_food',True)); add_vis("Ver comida:",'predator_retina_see_food',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_bacteria',True)); add_vis("Ver bactérias:",'predator_retina_see_bacteria',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_predators',False)); add_vis("Ver predadores:",'predator_retina_see_predators',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_show_vision',False)); add_vis("Mostrar visão:",'predator_show_vision',cb)
        v.addWidget(g_vis)
        # Ações
        g_act = QGroupBox("Ações"); g_act.setStyleSheet(card_style); la=QVBoxLayout(g_act)
        hint = QLabel("Novos altera o template. Vivos/Selecionado tambem atualiza agentes existentes; mudancas na rede podem recriar o cerebro.")
        hint.setWordWrap(True)
        la.addWidget(hint)
        b=QPushButton("Aplicar a novos individuos")
        b.clicked.connect(lambda _checked=False: self.apply_predator_params('template'))
        la.addWidget(b)
        b=QPushButton("Aplicar a todos vivos")
        b.clicked.connect(lambda _checked=False: self.apply_predator_params('all_alive'))
        la.addWidget(b)
        b=QPushButton("Aplicar ao selecionado")
        b.clicked.connect(lambda _checked=False: self.apply_predator_params('selected'))
        la.addWidget(b)
        v.addWidget(g_act)
        v.addStretch(1)
        hidden_layers_spin=self.widgets['predator_hidden_layers']
        def _update_predator_neurons():
            layers=int(hidden_layers_spin.value())
            for idx,spin in enumerate(self._predator_neuron_widgets): spin.setEnabled(idx<layers)
        hidden_layers_spin.valueChanged.connect(_update_predator_neurons); _update_predator_neurons()

    # ---------------------- Tab: Help --------------------------------
    def _build_tab_help(self):
        tab = QWidget(); self.tabs.addTab(tab, "Ajuda")
        txt = QTextEdit(); txt.setReadOnly(True)
        txt.setPlainText("""CONTROLES:\n\nMouse (área de simulação):\n• Scroll: Zoom focalizando no cursor\n• Botão esquerdo: Selecionar agente / Adicionar comida\n• Botão do meio: Adicionar bactéria\n• Botão direito + arrastar: Mover câmera\n\nTeclado:\n• Espaço: Pausar/Continuar\n• R: Resetar população\n• F: Enquadrar mundo na tela\n• T: Alternar renderização (simples/bonita)\n• V: Mostrar/ocultar visão do agente selecionado\n• +/-: Acelerar/desacelerar tempo\n• WASD / Setas: Mover câmera\n\nPARÂMETROS: (idênticos à versão Tk)\n\nDICAS:\n• Use renderização simples para populações grandes\n• Spatial Hash melhora performance\n• Time scale alto pode causar instabilidade\n• Predadores comem bactérias (70% eficiência)\n• Mutações estruturais são raras\n""")
        lay = QVBoxLayout(tab); lay.addWidget(txt)

    # ---------------------- Tab: Teste (agrupamentos UX) -----------
    def _build_tab_test(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Teste")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4,4,4,4)
        outer.setSpacing(6)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        outer.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        v = QVBoxLayout(content)
        v.setContentsMargins(2,2,2,20)
        # Espaçamento vertical entre grupos ligeiramente maior para separação visual
        v.setSpacing(18)

        # Estilo visual: borda fina, cantos arredondados, título em "pill" sobreposto
        card_style = (
            # margin-top reserva espaço interno para o título; aumentamos para evitar invasão do conteúdo
            "QGroupBox { border: 1px solid #4a4f58; border-radius: 8px; margin-top: 30px; background: #1c1f24; } "
            # Título: padding vertical levemente menor e sem offsets negativos
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; "
            "margin-left: 12px; padding: 3px 12px 4px 12px; border-radius: 8px; background: #262b31; "
            "color: #cfe1f5; font-weight:600; font-size:12px; line-height:14px; }"
        )

        # Gerar 30 parâmetros distribuídos em 3 grupos de 10
        groups_spec = [
            ("Metabolismo", 10),
            ("Locomoção & Percepção", 10),
            ("Evolução / Diversos", 10),
        ]

        param_index = 1
        for title, count in groups_spec:
            gb = QGroupBox(title)
            gb.setStyleSheet(card_style)
            grid = QGridLayout(gb)
            grid.setContentsMargins(10,14,10,10)
            grid.setHorizontalSpacing(6)
            grid.setVerticalSpacing(4)
            # Layout em coluna única: cada parâmetro ocupa uma linha (label, widget)
            row_idx = 0
            for _ in range(count):
                name = f'test_param_{param_index}'
                label_text = f"Parâmetro {param_index:02d}:"
                # alterna entre int e double para variedade
                if param_index % 3 == 0:
                    w = _spin_double(0.0, 1000.0, 0.1, 2)
                    w.setValue(float(param_index))
                else:
                    w = _spin_int(0, 10000)
                    w.setValue(param_index)
                self.widgets[name] = w
                grid.addWidget(self._help_label(label_text, name), row_idx, 0)
                grid.addWidget(w, row_idx, 1)
                row_idx += 1
                param_index += 1
            v.addWidget(gb)

        v.addStretch(1)

    # ------------------------------------------------------------------
    # Live callbacks & embedding
    # ------------------------------------------------------------------
    def _setup_live_param_signals(self):
        for widget in self.widgets.values():
            if isinstance(widget, (QSpinBox, QDoubleSpinBox, QComboBox)):
                widget.installEventFilter(self)
        for name in ['time_scale','fps','paused','simple_render','bacteria_show_vision','predator_show_vision','show_selected_details','retina_vision_mode']:
            w = self.widgets.get(name)
            if isinstance(w, (QSpinBox, QDoubleSpinBox)):
                w.valueChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(w, QCheckBox):
                w.toggled.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(w, QComboBox):
                w.currentTextChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
        # brain activation toggle handled separately

    def eventFilter(self, obj, event):
        if event.type() == QEvent.Type.Wheel and isinstance(obj, (QSpinBox, QDoubleSpinBox, QComboBox)):
            event.ignore()
            return True
        return super().eventFilter(obj, event)

    def _on_toggle_brain_activations(self, checked: bool):
        profiler.enabled = checked
        self.params.set('disable_brain_activations', not checked)

    def _update_param_real_time(self, name: str):
        try:
            value = self._get_widget_value(name)
            # degree conversion
            if name.endswith('_deg'):
                base = name[:-4]
                value = math.radians(value)
                self.params.set(base, value)
            else:
                self.params.set(name, value)
            if name == 'simple_render':
                self.engine.send_command('change_renderer', simple=bool(value))
        except Exception as e:
            self._log_exception(f"Erro callback {name}", e)

    def _init_pygame_view(self):
        try:
            win_id = int(self.pygame_host.winId())  # native window id
            self.pygame_view.initialize(win_id)
            def runner():
                try:
                    self.pygame_view.run()
                except Exception as e:
                    self._log_exception("Erro thread sim", e)
            self._sim_thread = threading.Thread(target=runner, daemon=True)
            self._sim_thread.start()
        except Exception as e:
            self._log_exception("Falha ao inicializar pygame embutido", e)

    # ------------------------------------------------------------------
    # Apply parameter groups
    # ------------------------------------------------------------------
    def apply_simulation_params(self):
        for name in ['time_scale','fps','paused','population_min_rescue_enabled','use_spatial','retina_skip','random_seed','retina_vision_mode','simple_render','reuse_spatial_grid','agents_inertia','allow_reverse_locomotion','reproduction_min_age','reproduction_cooldown','show_selected_details','debug_tracebacks']:
            if name in self.widgets:
                val = self._get_widget_value(name)
                if name == 'show_selected_details':
                    val = bool(val)
                self.params.set(name, val)
        if 'enable_brain_activations' in self.widgets:
            enabled = bool(self._get_widget_value('enable_brain_activations'))
            profiler.enabled = enabled
            self.params.set('disable_brain_activations', not enabled)
        print("Parâmetros de simulação aplicados")

    def apply_substrate_params(self):
        for name in ['food_target','food_min_r','food_max_r','food_replenish_interval','world_w','world_h','substrate_shape','substrate_radius']:
            if name in self.widgets:
                self.params.set(name, self._get_widget_value(name))
        # world reconfigure
        shape = self.params.get('substrate_shape','rectangular')
        radius = self.params.get('substrate_radius',350.0)
        world = self.engine.world
        world_w = self.params.get('world_w', world.width)
        world_h = self.params.get('world_h', world.height)
        world.configure(shape, radius, world_w, world_h)
        entities = list(self.engine.all_agents) + list(self.engine.entities.get('foods', []))
        for entity in entities:
            if hasattr(entity,'x') and hasattr(entity,'y') and hasattr(entity,'r'):
                entity.x, entity.y = world.clamp_position(entity.x, entity.y, getattr(entity,'r',0.0))
        self.engine._spatial_hash_dirty = True
        # color pickers already update params and propagate; nothing else to do here
        print("Parâmetros de substrato aplicados")

    def _agent_param_names(self, species: str) -> list[str]:
        if species == 'bacteria':
            return [
                'bacteria_count','bacteria_initial_energy','bacteria_death_energy','bacteria_split_energy',
                'bacteria_metab_v0_cost','bacteria_metab_vmax_cost','bacteria_energy_cap',
                'bacteria_show_vision','bacteria_body_size','bacteria_vision_radius','bacteria_retina_count',
                'bacteria_retina_fov_degrees','bacteria_retina_see_food','bacteria_retina_see_bacteria','bacteria_retina_see_predators',
                'bacteria_max_speed','bacteria_min_limit','bacteria_max_limit','bacteria_hidden_layers','bacteria_mutation_rate',
                'bacteria_mutation_strength','bacteria_max_turn_deg'
            ]
        return [
            'predators_enabled','predator_count','predator_initial_energy','predator_death_energy','predator_split_energy',
            'predator_metab_v0_cost','predator_metab_vmax_cost','predator_energy_cap',
            'predator_body_size','predator_show_vision','predator_vision_radius','predator_retina_see_food','predator_retina_count',
            'predator_retina_fov_degrees','predator_retina_see_bacteria','predator_retina_see_predators','predator_max_speed',
            'predator_min_limit','predator_max_limit','predator_hidden_layers','predator_mutation_rate','predator_mutation_strength','predator_max_turn_deg'
        ]

    def _collect_agent_params_from_widgets(self, species: str):
        turn_deg_key = f'{species}_max_turn_deg'
        turn_key = f'{species}_max_turn'
        for name in self._agent_param_names(species):
            if name in self.widgets:
                value = self._get_widget_value(name)
                if name == turn_deg_key:
                    self.params.set(turn_key, math.radians(value))
                else:
                    self.params.set(name, value)
        for i in range(1, 6):
            name = f'{species}_neurons_layer_{i}'
            if name in self.widgets:
                self.params.set(name, self._get_widget_value(name))

    def _desired_agent_brain_sizes(self, species: str) -> tuple[int, ...]:
        if species == 'bacteria':
            default_hidden_layers = 4
            default_neurons = lambda i: 20
        else:
            default_hidden_layers = 2
            default_neurons = lambda i: 16 if i == 1 else 8

        input_size = int(self.params.get(f'{species}_retina_count', 18))
        hidden_layers = int(self.params.get(f'{species}_hidden_layers', default_hidden_layers))
        sizes = [input_size]
        for i in range(1, 6):
            if i > hidden_layers:
                break
            neurons = int(self.params.get(f'{species}_neurons_layer_{i}', default_neurons(i)))
            if neurons > 0:
                sizes.append(neurons)
        sizes.append(2)
        return tuple(sizes)

    def _agent_factory_helpers(self, species: str) -> dict[str, Any]:
        if species == 'bacteria':
            from .entities import (
                _create_bacteria_brain,
                _create_bacteria_sensor,
                _create_bacteria_locomotion,
                _create_bacteria_energy_model,
            )
            return {
                'brain': _create_bacteria_brain,
                'sensor': _create_bacteria_sensor,
                'locomotion': _create_bacteria_locomotion,
                'energy': _create_bacteria_energy_model,
            }
        from .entities import (
            _create_predator_brain,
            _create_predator_sensor,
            _create_predator_locomotion,
            _create_predator_energy_model,
        )
        return {
            'brain': _create_predator_brain,
            'sensor': _create_predator_sensor,
            'locomotion': _create_predator_locomotion,
            'energy': _create_predator_energy_model,
        }

    def _target_live_agents(self, species: str, mode: str) -> list[Any]:
        is_predator = species == 'predator'
        if mode == 'all_alive':
            key = 'predators' if is_predator else 'bacteria'
            return list(self.engine.entities.get(key, []))

        selected = getattr(self.engine, 'selected_agent', None)
        if selected is None or bool(getattr(selected, 'is_predator', False)) != is_predator:
            try:
                label = "predador" if is_predator else "bacteria"
                QMessageBox.information(self, "Aplicar ao selecionado", f"Selecione um agente do tipo {label}.")
            except Exception:
                pass
            return []
        return [selected]

    def _agents_requiring_brain_rebuild(self, species: str, agents: list[Any]) -> list[Any]:
        desired = self._desired_agent_brain_sizes(species)
        changed = []
        for agent in agents:
            brain = getattr(agent, 'brain', None)
            current = tuple(getattr(brain, 'sizes', ()) or ())
            can_resize_input = (
                brain is not None
                and len(current) == len(desired)
                and current[1:] == desired[1:]
                and hasattr(brain, 'resize_input')
            )
            if current != desired and not can_resize_input:
                changed.append(agent)
        return changed

    def _confirm_agent_brain_rebuild(self, species: str, count: int) -> bool:
        label = "predadores" if species == 'predator' else "bacterias"
        msg = (
            f"{count} {label} tem arquitetura neural diferente do template atual.\n\n"
            "Recriar o cerebro apaga os pesos/aprendizado desses agentes. "
            "Clique em Sim para reconstruir, ou Nao para aplicar apenas corpo, sensor, movimento e metabolismo."
        )
        try:
            answer = QMessageBox.question(
                self,
                "Mudanca estrutural da rede neural",
                msg,
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
                QMessageBox.StandardButton.No,
            )
            return answer == QMessageBox.StandardButton.Yes
        except Exception:
            return False

    def _apply_agent_template_to_agents(self, species: str, agents: list[Any], rebuild_brain: bool) -> dict[str, int]:
        helpers = self._agent_factory_helpers(species)
        desired_sizes = self._desired_agent_brain_sizes(species)
        body_key = f'{species}_body_size'
        color_key = f'{species}_color'
        stats = {'agents': 0, 'brains_rebuilt': 0, 'brains_resized': 0, 'brains_kept': 0}

        for agent in agents:
            stats['agents'] += 1
            try:
                radius = float(self.params.get(body_key, getattr(agent, 'r', 1.0)))
                agent.r = max(0.1, radius)
                agent.m = agent.r * agent.r
            except Exception:
                pass

            agent.sensor = helpers['sensor'](self.params)
            agent.locomotion = helpers['locomotion'](self.params)
            agent.energy_model = helpers['energy'](self.params)
            cap = getattr(agent.energy_model, 'energy_cap', None)
            if cap is not None and getattr(agent, 'energy', 0.0) > cap:
                agent.energy = float(cap)

            try:
                color = self.params.get(color_key, None)
                if color is not None:
                    agent.color = tuple(color)
            except Exception:
                pass

            brain = getattr(agent, 'brain', None)
            current_sizes = tuple(getattr(brain, 'sizes', ()) or ())
            if current_sizes != desired_sizes:
                if brain is not None and len(current_sizes) == len(desired_sizes) and current_sizes[1:] == desired_sizes[1:] and hasattr(brain, 'resize_input'):
                    brain.resize_input(desired_sizes[0])
                    brain.version = int(getattr(brain, 'version', 0)) + 1
                    stats['brains_resized'] += 1
                elif rebuild_brain:
                    agent.brain = helpers['brain'](self.params)
                    stats['brains_rebuilt'] += 1
                else:
                    stats['brains_kept'] += 1

            agent.last_brain_output = []
            agent.last_brain_activations = []

        if stats['agents']:
            self.engine._spatial_hash_dirty = True
            try:
                from .brain import clear_multi_brain_cache
                clear_multi_brain_cache()
            except Exception:
                pass
        return stats

    def _apply_agent_params(self, species: str, mode: str = 'template', confirm_structural: bool = True):
        if mode not in {'template', 'all_alive', 'selected'}:
            mode = 'template'

        self._collect_agent_params_from_widgets(species)
        label = "predadores" if species == 'predator' else "bacterias"
        if mode == 'template':
            print(f"Parametros de {label} aplicados ao template de novos individuos")
            return

        lock = getattr(self.engine, 'state_lock', None)
        if lock is not None:
            with lock:
                agents = self._target_live_agents(species, mode)
                structural = self._agents_requiring_brain_rebuild(species, agents)
        else:
            agents = self._target_live_agents(species, mode)
            structural = self._agents_requiring_brain_rebuild(species, agents)

        if not agents:
            print(f"Nenhum agente vivo de {label} recebeu parametros")
            return

        rebuild_brain = False
        if structural:
            rebuild_brain = True if not confirm_structural else self._confirm_agent_brain_rebuild(species, len(structural))

        if lock is not None:
            with lock:
                stats = self._apply_agent_template_to_agents(species, agents, rebuild_brain)
        else:
            stats = self._apply_agent_template_to_agents(species, agents, rebuild_brain)

        scope = "selecionado" if mode == 'selected' else "todos vivos"
        print(
            f"Parametros de {label} aplicados a {scope}: "
            f"{stats['agents']} agentes, {stats['brains_rebuilt']} cerebros recriados, "
            f"{stats['brains_resized']} entradas redimensionadas, {stats['brains_kept']} cerebros preservados"
        )

    def apply_bacteria_params(self, mode: str = 'template', confirm_structural: bool = True):
        self._apply_agent_params('bacteria', mode, confirm_structural)

    def apply_predator_params(self, mode: str = 'template', confirm_structural: bool = True):
        self._apply_agent_params('predator', mode, confirm_structural)

    def apply_all_params(self):
        self.apply_simulation_params(); self.apply_substrate_params(); self.apply_bacteria_params(); self.apply_predator_params()
        for name in [
            'auto_export_substrate',
            'export_substrate_include_brain_activations',
            'export_substrate_pretty_json',
        ]:
            if name in self.widgets:
                self.params.set(name, bool(self._get_widget_value(name)), validate=False)
        if 'auto_export_interval_minutes' in self.widgets:
            self.params.set('auto_export_interval_minutes', self._get_widget_value('auto_export_interval_minutes'), validate=False)
        print("Todos os parâmetros aplicados")

    # ------------------------------------------------------------------
    # Engine actions
    # ------------------------------------------------------------------
    def reset_population(self):
        self.engine.send_command('reset_population')
        print("População resetada")

    def start_simulation(self):
        self.apply_all_params()
        if not self.engine.running:
            self.engine.start(); print("Simulação iniciada")
        else:
            print("Simulação já em execução")

    def save_biosim_window(self):
        default_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'substrates'))
        os.makedirs(default_dir, exist_ok=True)
        path, _ = QFileDialog.getSaveFileName(self, "Salvar BioSim", os.path.join(default_dir, 'projeto.biosim'), "BioSim (*.biosim)")
        if not path:
            return
        if not path.endswith('.biosim'):
            path += '.biosim'
        try:
            self._export_substrate(path_override=path, file_type='biosim')
            QMessageBox.information(self, "Salvar BioSim", f"Projeto salvo em {path}")
        except Exception as e:
            self._warn_exception("Erro ao salvar BioSim", e)

    def open_biosim_window(self):
        default_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'substrates'))
        os.makedirs(default_dir, exist_ok=True)
        path, _ = QFileDialog.getOpenFileName(self, "Abrir BioSim", default_dir, "BioSim (*.biosim);;JSON (*.json)")
        if not path:
            return
        try:
            self._import_substrate(path)
            QMessageBox.information(self, "Abrir BioSim", "Projeto carregado.")
        except Exception as e:
            self._warn_exception("Erro ao abrir BioSim", e)

    def new_biosim_project(self):
        answer = QMessageBox.question(
            self,
            "Novo projeto",
            "Criar um novo projeto com os parametros atuais? O estado vivo atual sera substituido.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        try:
            self.apply_all_params()
            state_lock = getattr(self.engine, 'state_lock', None)
            if state_lock is not None:
                state_lock.acquire()
            try:
                if hasattr(self.engine, 'obstacles'):
                    self.engine.obstacles.clear()
                self.engine.loaded_agent_prototypes.clear()
                self.engine.current_agent_prototype = None
                self.engine.selected_agent = None
                self.engine.dragged_object = None
                self.engine.total_simulation_time = 0.0
                self.engine.frame_count = 0
                self.engine._initialize_population()
                self.engine.camera.fit_world(self.engine.world, self.pygame_view.screen_width, self.pygame_view.screen_height)
            finally:
                if state_lock is not None:
                    state_lock.release()
            print("Novo projeto BioSim criado.")
        except Exception as e:
            self._warn_exception("Erro ao criar novo projeto", e)

    def create_lineage_from_selected(self):
        agent = self.engine.selected_agent
        if agent is None:
            QMessageBox.information(self, "Criar linhagem", "Selecione um agente primeiro.")
            return
        try:
            import time as _time
            kind = "predador" if getattr(agent, 'is_predator', False) else "bacteria"
            name = f"linhagem_{kind}_{_time.strftime('%Y%m%d_%H%M%S')}.agent.csv"
            path = self._export_selected_agent(agent, name)
            self._load_agent_from_csv(path)
            QMessageBox.information(self, "Criar linhagem", f"Linhagem criada e carregada: {os.path.basename(path)}")
        except Exception as e:
            self._warn_exception("Erro ao criar linhagem", e)

    def show_help_window(self):
        QMessageBox.information(
            self,
            "Ajuda",
            "Mouse: botao direito move a camera. Na barra inferior use S para selecionar, F para comida, A para agente importado, pincel para obstaculos, M para mover e D para remover.\n\n"
            "Menus superiores: Arquivo salva/abre projetos .biosim; View controla visualizacao; Preferencias controla export/debug; Agente exporta, carrega e cria linhagens."
        )

    # ------------------------------------------------------------------
    # Persistence CSV
    # ------------------------------------------------------------------
    def save_ui_params(self):
        try:
            rows_by_name = {}
            for name in sorted(self.widgets.keys()):
                if name.startswith('test_param_'):
                    continue
                val = self._get_widget_value(name)
                rows_by_name[name] = {'name': name, 'value': val}
            # Ensure color params and substrate shape are saved too
            try:
                import json as _json
                rows_by_name['substrate_shape'] = {'name': 'substrate_shape', 'value': self._get_widget_value('substrate_shape')}
                rows_by_name['substrate_bg_color'] = {'name': 'substrate_bg_color', 'value': _json.dumps(list(self.params.get('substrate_bg_color', (10,10,20))))}
                rows_by_name['food_color'] = {'name': 'food_color', 'value': _json.dumps(list(self.params.get('food_color', (220,30,30))))}
                rows_by_name['bacteria_color'] = {'name': 'bacteria_color', 'value': _json.dumps(list(self.params.get('bacteria_color', (220,220,220))))}
                rows_by_name['predator_color'] = {'name': 'predator_color', 'value': _json.dumps(list(self.params.get('predator_color', (80,120,220))))}
                for menu_param in ['simple_render', 'show_selected_details', 'bacteria_show_vision', 'predator_show_vision']:
                    rows_by_name[menu_param] = {'name': menu_param, 'value': self.params.get(menu_param, False)}
                rows_by_name['enable_brain_activations'] = {
                    'name': 'enable_brain_activations',
                    'value': not self.params.get('disable_brain_activations', False),
                }
                # Camera position/zoom
                try:
                    cam = getattr(self.engine, 'camera', None)
                    if cam is not None:
                        rows_by_name['camera_x'] = {'name': 'camera_x', 'value': cam.x}
                        rows_by_name['camera_y'] = {'name': 'camera_y', 'value': cam.y}
                        rows_by_name['camera_zoom'] = {'name': 'camera_zoom', 'value': cam.zoom}
                except Exception:
                    pass
            except Exception:
                pass
            rows = [rows_by_name[name] for name in sorted(rows_by_name.keys())]
            os.makedirs(os.path.dirname(self._ui_params_csv), exist_ok=True)
            with open(self._ui_params_csv,'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['name','value']); writer.writeheader(); writer.writerows(rows)
            print(f"Parâmetros UI salvos em {self._ui_params_csv}")
        except Exception as e:
            self._log_exception("Erro ao salvar parâmetros UI", e)

    def _load_ui_params_csv(self):
        load_path = self._ui_params_csv
        if not os.path.exists(load_path) and os.path.exists(getattr(self, '_legacy_ui_params_csv', '')):
            load_path = self._legacy_ui_params_csv
        if not os.path.exists(load_path):
            return
        try:
            with open(load_path,'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    name = row.get('name'); value = row.get('value')
                    if not name or name.startswith('test_param_'):
                        continue
                    if name in self.widgets:
                        w = self.widgets[name]
                        try:
                            if isinstance(w, QCheckBox):
                                self._set_widget_value(name, value in ('1','True','true'))
                            elif isinstance(w, QSpinBox):
                                self._set_widget_value(name, int(float(value)))
                            elif isinstance(w, QDoubleSpinBox):
                                self._set_widget_value(name, float(value))
                            elif isinstance(w, QComboBox):
                                self._set_widget_value(name, value)
                            elif isinstance(w, QLineEdit):
                                self._set_widget_value(name, value)
                        except Exception:
                            pass
                    # Additional: load saved color params or substrate shape even if not in widgets
                    try:
                        if name == 'substrate_bg_color':
                            import json as _json
                            col = _json.loads(value)
                            self.params.set('substrate_bg_color', tuple(col), validate=False)
                            if hasattr(self, '_swatch_substrate'):
                                r,g,b = col[:3]; self._swatch_substrate.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                        if name == 'food_color':
                            import json as _json
                            col = _json.loads(value)
                            self.params.set('food_color', tuple(col), validate=False)
                            if hasattr(self.engine, 'entities'):
                                for food in self.engine.entities.get('foods', []):
                                    try:
                                        if getattr(food, 'color', None) is None:
                                            food.color = tuple(col)
                                    except Exception:
                                        pass
                        if name == 'bacteria_color':
                            import json as _json
                            col = _json.loads(value)
                            self.params.set('bacteria_color', tuple(col), validate=False)
                            if hasattr(self, '_swatch_bacteria'):
                                r,g,b = col[:3]; self._swatch_bacteria.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                            if hasattr(self.engine, 'entities'):
                                for b in self.engine.entities.get('bacteria', []):
                                    try:
                                        if getattr(b, 'color', None) is None:
                                            b.color = tuple(col)
                                    except Exception:
                                        pass
                        if name == 'predator_color':
                            import json as _json
                            col = _json.loads(value)
                            self.params.set('predator_color', tuple(col), validate=False)
                            if hasattr(self, '_swatch_predator'):
                                r,g,b = col[:3]; self._swatch_predator.setStyleSheet(f"background: rgb({r},{g},{b}); border:1px solid #333; border-radius:4px;")
                            if hasattr(self.engine, 'entities'):
                                for p in self.engine.entities.get('predators', []):
                                    try:
                                        if getattr(p, 'color', None) is None:
                                            p.color = tuple(col)
                                    except Exception:
                                        pass
                        if name == 'substrate_shape':
                            # shape stored as plain string
                            self.params.set('substrate_shape', value, validate=False)
                            # update combo box widget if exists
                            if 'substrate_shape' in self.widgets:
                                try:
                                    w = self.widgets['substrate_shape']; idx = w.findText(value)
                                    if idx>=0: w.setCurrentIndex(idx)
                                except Exception:
                                    pass
                            # Apply to engine.world if available
                            try:
                                if hasattr(self, 'engine') and getattr(self.engine, 'world', None) is not None:
                                    world = self.engine.world
                                    # keep same radius/width/height from params
                                    world.configure(value, self.params.get('substrate_radius', world.radius), self.params.get('world_w', world.width), self.params.get('world_h', world.height))
                            except Exception:
                                pass
                        if name == 'camera_x':
                            try:
                                cam = getattr(self.engine, 'camera', None)
                                if cam is not None:
                                    cam.x = float(value)
                            except Exception:
                                pass
                        if name == 'camera_y':
                            try:
                                cam = getattr(self.engine, 'camera', None)
                                if cam is not None:
                                    cam.y = float(value)
                            except Exception:
                                pass
                        if name == 'camera_zoom':
                            try:
                                cam = getattr(self.engine, 'camera', None)
                                if cam is not None:
                                    cam.zoom = max(0.01, float(value))
                            except Exception:
                                pass
                        if name in ['simple_render', 'show_selected_details', 'bacteria_show_vision', 'predator_show_vision']:
                            self.params.set(name, value in ('1', 'True', 'true', 'yes', 'YES'), validate=False)
                        if name == 'enable_brain_activations':
                            enabled = value in ('1', 'True', 'true', 'yes', 'YES')
                            profiler.enabled = enabled
                            self.params.set('disable_brain_activations', not enabled, validate=False)
                    except Exception:
                        pass
            # schedule auto export if active
            if self._get_widget_value('auto_export_substrate'):
                self._schedule_next_auto_export(initial=True)
            print(f"Parâmetros UI carregados de {load_path}")
        except Exception as e:
            self._log_exception("Erro ao carregar parâmetros UI", e)

    # ------------------------------------------------------------------
    # Agent export/import
    # ------------------------------------------------------------------
    def open_export_agent_window(self):
        agent = self.engine.selected_agent
        if agent is None:
            QMessageBox.information(self, "Exportar Agente", "Selecione um agente na simulação primeiro.")
            return
        name, ok = QFileDialog.getSaveFileName(self, "Exportar Agente", os.path.join(os.path.dirname(__file__), '..', 'agents', 'agente.agent.csv'), "Agente (*.agent.csv)")
        if not ok or not name:
            return
        if not name.endswith('.agent.csv'):
            name += '.agent.csv'
        try:
            self._export_selected_agent(agent, name)
            QMessageBox.information(self, "Exportar Agente", f"Agente exportado em {name}")
        except Exception as e:
            self._warn_exception("Erro", e)

    def _export_selected_agent(self, agent, path_or_name: str) -> str:
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'agents'))
        if os.path.dirname(path_or_name):
            path = path_or_name
            if not path.endswith('.agent.csv'):
                path += '.agent.csv'
        else:
            filename = path_or_name
            if not filename.endswith('.agent.csv'):
                filename += '.agent.csv'
            path = os.path.join(agents_dir, filename)
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        # Garante que todos os widgets atuais estejam aplicados aos params antes do export
        try:
            self.apply_all_params()
        except Exception:
            pass
        brain = getattr(agent,'brain',None); sensor = getattr(agent,'sensor',None); locomotion = getattr(agent,'locomotion',None); energy_model = getattr(agent,'energy_model',None)
        rows = []
        def add(k,v): rows.append({'key':k,'value':v})
        add('type', 'predator' if getattr(agent,'is_predator', False) else 'bacteria')
        for attr in ['x','y','r','angle','vx','vy','energy','age']:
            add(attr, getattr(agent, attr, 0.0))
        add('last_reproduction_age', getattr(agent, 'last_reproduction_age', ''))
        # Cor do agente (RGB tuple) - exportada em JSON para compatibilidade
        try:
            col = getattr(agent, 'color', None)
            if col is not None:
                add('color', json.dumps(list(col)))
        except Exception:
            pass
        if brain is not None and hasattr(brain,'sizes'):
            add('brain_sizes', json.dumps(brain.sizes)); add('brain_version', getattr(brain,'version',0))
            for idx,(W,B) in enumerate(zip(brain.weights, brain.biases)):
                try:
                    w_list = W.tolist() if hasattr(W,'tolist') else list(W)
                    b_list = B.tolist() if hasattr(B,'tolist') else list(B)
                except Exception:
                    w_list = list(W); b_list = list(B)
                add(f'brain_weight_{idx}', json.dumps(w_list)); add(f'brain_bias_{idx}', json.dumps(b_list))
        if sensor is not None:
            for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators']:
                if hasattr(sensor, attr): add(f'sensor_{attr}', getattr(sensor, attr))
        if locomotion is not None:
            for attr in ['max_speed','max_turn']:
                if hasattr(locomotion, attr): add(f'locomotion_{attr}', getattr(locomotion, attr))
        if energy_model is not None:
            # Exporta dinamicamente todos os atributos simples do modelo de energia
            exported_energy_keys = set()
            for attr in getattr(energy_model, '__slots__', []):
                if attr.startswith('_'): continue
                val = getattr(energy_model, attr, None)
                if isinstance(val, (int, float, bool)):
                    add(f'energy_{attr}', val)
                    exported_energy_keys.add(attr)
            # Compat retroativa: também grava chaves antigas se aplicável
            if 'v0_cost' in exported_energy_keys:
                add('energy_loss_idle', getattr(energy_model, 'v0_cost'))
            if 'vmax_cost' in exported_energy_keys:
                add('energy_loss_move', getattr(energy_model, 'vmax_cost'))
        # On-demand activations (recalcula se vazio) para export sem poluir memória runtime
        last_out = getattr(agent,'last_brain_output', [])
        if not last_out and getattr(agent,'brain',None) and getattr(agent,'sensor',None):
            try:
                from .sensors import RetinaSensor
                scene = self.engine.scene_query
                sensor_inputs = agent.sensor.sense(agent, scene, self.params)
                last_out = agent.brain.forward(sensor_inputs)
            except Exception:
                pass
        add('last_brain_output', json.dumps(last_out))
        acts = getattr(agent,'last_brain_activations', [])
        if not acts and getattr(agent,'brain',None) and getattr(agent,'sensor',None):
            try:
                sensor_inputs = agent.sensor.sense(agent, self.engine.scene_query, self.params)
                acts = agent.brain.activations(sensor_inputs)
            except Exception:
                acts = []
        add('last_brain_activations', json.dumps(acts))
        with open(path,'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=['key','value']); writer.writeheader(); writer.writerows(rows)
        print(f"Agente exportado para {path}")
        return path

    def open_load_agent_window(self):
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'agents'))
        os.makedirs(agents_dir, exist_ok=True)
        files = [f for f in os.listdir(agents_dir) if f.endswith('.agent.csv')]
        if not files:
            QMessageBox.information(self, "Carregar Agente", "Nenhum arquivo .agent.csv encontrado.")
            return
        path, _ = QFileDialog.getOpenFileName(self, "Carregar Agente", agents_dir, "Agente (*.agent.csv)")
        if not path:
            return
        try:
            self._load_agent_from_csv(path)
            QMessageBox.information(self, "Carregar Agente", "Carregado com sucesso.")
        except Exception as e:
            self._warn_exception("Erro", e)

    def _load_agent_from_csv(self, path: str):
        data = {}
        with open(path,'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                data[row['key']] = row['value']
        base = os.path.basename(path)
        name = base.replace('.agent.csv','').replace('.csv','')
    # Normaliza campos de energia (suporta legacy loss_idle/loss_move e novos v0_cost/vmax_cost)
    # Mantemos dados originais; conversão será feita no spawn.
        self.engine.loaded_agent_prototypes[name] = data
        self.engine.current_agent_prototype = name
        print(f"Protótipo '{name}' carregado. Clique direito no substrato para inserir instâncias.")

    # ------------------------------------------------------------------
    # Substrate export/import
    # ------------------------------------------------------------------
    def _get_substrate_dirs(self) -> Tuple[str,str]:
            root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
            base = os.path.join(root_dir, 'substrates')
            manual = os.path.join(base, 'manual_exports')
            auto_root = os.path.join(base, 'auto_exports')
            # cria pastas raiz
            os.makedirs(base, exist_ok=True)
            os.makedirs(manual, exist_ok=True)
            os.makedirs(auto_root, exist_ok=True)
            return manual, auto_root

    def open_export_substrate_window(self):
        manual_dir, _ = self._get_substrate_dirs()
        path, _ = QFileDialog.getSaveFileName(self, "Exportar Substrato", os.path.join(manual_dir, 'substrato.json'), "JSON (*.json)")
        if not path: return
        prefix = os.path.splitext(os.path.basename(path))[0]
        try:
            out = self._export_substrate(prefix=prefix, manual=True)
            QMessageBox.information(self, "Exportar Substrato", f"Exportado: {os.path.basename(out)}")
        except Exception as e:
            self._warn_exception("Erro", e)

    def open_import_substrate_window(self):
        manual_dir, auto_dir = self._get_substrate_dirs()
        start_dir = manual_dir if os.path.isdir(manual_dir) else auto_dir
        path, _ = QFileDialog.getOpenFileName(self, "Importar Substrato", start_dir, "Snapshot (*.json)")
        if not path: return
        try:
            self._import_substrate(path)
            QMessageBox.information(self, "Importar Substrato", "Importado.")
        except Exception as e:
            self._warn_exception("Erro", e)

    def _export_substrate(self, prefix: str='substrato', manual: bool=True,
                          path_override: str | None = None, file_type: str = 'substrate') -> str:
        import time
        prev_paused = bool(self._get_widget_value('paused'))
        self.widgets['paused'].setChecked(True)
        self.params.set('paused', True, validate=False)
        state_lock = None
        try:
            engine = self.engine
            state_lock = getattr(engine, 'state_lock', None)
            if state_lock is not None:
                state_lock.acquire()
            # Aplica todos os parâmetros atuais antes de capturar snapshot
            try:
                self.apply_all_params()
            except Exception:
                pass
            manual_dir, auto_root = self._get_substrate_dirs()
            ts_full = time.strftime('%Y%m%d_%H%M%S')
            if path_override:
                path = os.path.abspath(path_override)
                os.makedirs(os.path.dirname(path), exist_ok=True)
            elif manual:
                filename = f"{prefix}_{ts_full}.json"
                out_dir = manual_dir
            else:
                date_br = time.strftime('%d.%m.%Y')  # formato para nome da subpasta
                # garante subpasta da data
                auto_dir = os.path.join(auto_root, date_br)
                os.makedirs(auto_dir, exist_ok=True)
                base_pref = f"autosave_substrate_{date_br}_"
                try:
                    existing = [f for f in os.listdir(auto_dir) if f.startswith(base_pref) and f.endswith('.json')]
                except Exception:
                    existing = []
                seq = 1
                if existing:
                    import re
                    pat = re.compile(rf"^autosave_substrate_{date_br}_(\d+)\.json$")
                    nums = []
                    for fname in existing:
                        m = pat.match(fname)
                        if m:
                            try: nums.append(int(m.group(1)))
                            except: pass
                    if nums:
                        seq = max(nums) + 1
                filename = f"{base_pref}{seq:02d}.json"
                out_dir = auto_dir
            if not path_override:
                path = os.path.join(out_dir, filename)
            params_snapshot = dict(self.params._data)
            ui_snapshot = {k:self._get_widget_value(k) for k in self.widgets.keys()}
            from .random_utils import capture_rng_state
            rng_state = capture_rng_state()
            include_brain_activations = bool(self.params.get('export_substrate_include_brain_activations', False))
            pretty_json = manual and bool(self.params.get('export_substrate_pretty_json', False))
            brain_outputs_recomputed = 0
            brain_activations_recomputed = 0
            if include_brain_activations and getattr(engine, 'scene_query', None) is None and hasattr(engine, '_update_spatial_hash'):
                try:
                    engine._update_spatial_hash(force=True)
                except Exception:
                    pass
            world = engine.world; camera = engine.camera
            foods_data = []
            for food in engine.entities['foods']:
                fd = {
                    'x': food.x,
                    'y': food.y,
                    'r': food.r,
                    'energy': getattr(food, 'energy', food.r * food.r)
                }
                try:
                    fd['color'] = list(getattr(food, 'color', (220, 30, 30)))
                except Exception:
                    pass
                foods_data.append(fd)
            agents_data = []
            for agent in engine.all_agents:
                brain = getattr(agent,'brain',None); sensor = getattr(agent,'sensor',None); locomotion = getattr(agent,'locomotion',None); energy_model = getattr(agent,'energy_model',None)
                ad = {
                    'type': 'predator' if getattr(agent,'is_predator', False) else 'bacteria',
                    'x': agent.x,'y': agent.y,'r': agent.r,'angle': agent.angle,'vx': agent.vx,'vy': agent.vy,
                    'energy': getattr(agent,'energy',0.0),'age': getattr(agent,'age',0.0),
                    'last_reproduction_age': getattr(agent, 'last_reproduction_age', None)
                }
                try:
                    ad['color'] = list(getattr(agent, 'color', (220, 220, 220)))
                except Exception:
                    pass
                if brain and hasattr(brain,'sizes'):
                    ad['brain_sizes'] = list(brain.sizes); ad['brain_version'] = getattr(brain,'version',0)
                    try:
                        ad['brain_weights'] = [w.tolist() if hasattr(w,'tolist') else list(w) for w in brain.weights]
                        ad['brain_biases'] = [b.tolist() if hasattr(b,'tolist') else list(b) for b in brain.biases]
                    except Exception:
                        ad['brain_weights'] = [list(w) for w in brain.weights]; ad['brain_biases'] = [list(b) for b in brain.biases]
                if sensor:
                    for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators']:
                        if hasattr(sensor, attr): ad[f'sensor_{attr}'] = getattr(sensor, attr)
                if locomotion:
                    for attr in ['max_speed','max_turn']:
                        if hasattr(locomotion, attr): ad[f'locomotion_{attr}'] = getattr(locomotion, attr)
                if energy_model:
                    exported_energy_keys = set()
                    for attr in getattr(energy_model,'__slots__', []):
                        if attr.startswith('_'): continue
                        val = getattr(energy_model, attr, None)
                        if isinstance(val,(int,float,bool)):
                            ad[f'energy_{attr}'] = val
                            exported_energy_keys.add(attr)
                    # chaves legacy para snapshots v1
                    if 'v0_cost' in exported_energy_keys and 'loss_idle' not in exported_energy_keys:
                        ad['energy_loss_idle'] = getattr(energy_model,'v0_cost')
                    if 'vmax_cost' in exported_energy_keys and 'loss_move' not in exported_energy_keys:
                        ad['energy_loss_move'] = getattr(energy_model,'vmax_cost')
                # Brain debug arrays are optional; recalculating them for every agent makes snapshots heavy.
                out = getattr(agent,'last_brain_output', [])
                if out:
                    ad['last_brain_output'] = out
                elif include_brain_activations and brain and sensor:
                    try:
                        sensor_inputs = sensor.sense(agent, self.engine.scene_query, self.params)
                        ad['last_brain_output'] = brain.forward(sensor_inputs)
                        brain_outputs_recomputed += 1
                    except Exception:
                        ad['last_brain_output'] = []
                else:
                    ad['last_brain_output'] = []
                if include_brain_activations:
                    acts = getattr(agent,'last_brain_activations', [])
                    if (not acts) and brain and sensor:
                        try:
                            sensor_inputs = sensor.sense(agent, self.engine.scene_query, self.params)
                            acts = brain.activations(sensor_inputs)
                            brain_activations_recomputed += 1
                        except Exception:
                            acts = []
                    ad['last_brain_activations'] = acts
                agents_data.append(ad)
            selected_agent_index = None
            if engine.selected_agent in engine.all_agents:
                try:
                    selected_agent_index = engine.all_agents.index(engine.selected_agent)
                except ValueError:
                    selected_agent_index = None
            snapshot = {
                'version':2,'file_type': file_type,'timestamp': ts_full,'params': params_snapshot,'ui_params': ui_snapshot,
                'world': {'width': world.width,'height': world.height,'shape': world.shape,'radius': world.radius},
                'camera': {'x': engine.camera.x,'y': engine.camera.y,'zoom': engine.camera.zoom},
                'simulation': {'total_simulation_time': engine.total_simulation_time},
                'rng_state': rng_state,
                'loaded_agent_prototypes': dict(getattr(engine, 'loaded_agent_prototypes', {})),
                'current_agent_prototype': getattr(engine, 'current_agent_prototype', None),
                'selected_agent_index': selected_agent_index,
                'tool_state': {
                    'active_tool': getattr(self.pygame_view, 'active_tool', 'food'),
                    'brush_width': getattr(self.pygame_view, 'brush_width', 16.0),
                    'brush_color': list(getattr(self.pygame_view, 'brush_color', (95, 95, 105))),
                    'brush_erase': getattr(self.pygame_view, 'brush_erase', False),
                },
                'export_options': {
                    'include_brain_activations': include_brain_activations,
                    'pretty_json': pretty_json,
                    'brain_outputs_recomputed': brain_outputs_recomputed,
                    'brain_activations_recomputed': brain_activations_recomputed,
                },
                'obstacles': engine.obstacles.to_dicts() if hasattr(engine, 'obstacles') else [],
                'food': {'count': len(engine.entities['foods']), 'target': self.params.get('food_target',0)},
                'foods': foods_data,
                'agents': agents_data
            }
            dump_kwargs = {'ensure_ascii': False}
            if pretty_json:
                dump_kwargs['indent'] = 2
            else:
                dump_kwargs['separators'] = (',', ':')
            with open(path,'w', encoding='utf-8') as f: json.dump(snapshot, f, **dump_kwargs)
            print(f"{'BioSim salvo' if file_type == 'biosim' else 'Substrato exportado'} para {path}")
            return path
        finally:
            if state_lock is not None:
                state_lock.release()
            self.widgets['paused'].setChecked(prev_paused)
            self.params.set('paused', prev_paused, validate=False)

    def _import_substrate(self, path: str):
        import math as _m
        prev_paused = bool(self._get_widget_value('paused'))
        self.widgets['paused'].setChecked(True)
        self.params.set('paused', True, validate=False)
        state_lock = None
        try:
            with open(path,'r', encoding='utf-8') as f:
                data = json.load(f)
            state_lock = getattr(self.engine, 'state_lock', None)
            if state_lock is not None:
                state_lock.acquire()
            for k,v in data.get('params', {}).items():
                self.params.set(k, v, validate=False)
                if k in self.widgets:
                    self._set_widget_value(k, v)
            wdata = data.get('world', {})
            world = self.engine.world
            world.configure(wdata.get('shape', world.shape), wdata.get('radius', world.radius), wdata.get('width', world.width), wdata.get('height', world.height))
            cam_data = data.get('camera', {})
            cam = self.engine.camera
            cam.x = cam_data.get('x', cam.x); cam.y = cam_data.get('y', cam.y); cam.zoom = cam_data.get('zoom', cam.zoom)
            sim_data = data.get('simulation', {})
            self.engine.total_simulation_time = sim_data.get('total_simulation_time', self.engine.total_simulation_time)
            self.engine.loaded_agent_prototypes = dict(data.get('loaded_agent_prototypes', {}))
            self.engine.current_agent_prototype = data.get('current_agent_prototype')
            tool_state = data.get('tool_state', {})
            if tool_state:
                if hasattr(self.pygame_view, 'active_tool'):
                    self.pygame_view.active_tool = tool_state.get('active_tool', self.pygame_view.active_tool)
                if hasattr(self.pygame_view, 'brush_width'):
                    self.pygame_view.brush_width = float(tool_state.get('brush_width', self.pygame_view.brush_width))
                    if 'obstacle_brush_width' in self.widgets:
                        self._set_widget_value('obstacle_brush_width', self.pygame_view.brush_width)
                if hasattr(self.pygame_view, 'brush_color'):
                    color = tool_state.get('brush_color', self.pygame_view.brush_color)
                    if isinstance(color, (list, tuple)) and len(color) >= 3:
                        self.pygame_view.brush_color = (int(color[0]), int(color[1]), int(color[2]))
                        if '_brush_color_swatch' in getattr(self, '__dict__', {}):
                            self._refresh_brush_color_swatch()
                if hasattr(self.pygame_view, 'brush_erase'):
                    self.pygame_view.brush_erase = bool(tool_state.get('brush_erase', self.pygame_view.brush_erase))
                    if 'obstacle_brush_erase' in self.widgets:
                        self._set_widget_value('obstacle_brush_erase', self.pygame_view.brush_erase)
            # Clear current entities
            for lst in self.engine.entities.values(): lst.clear()
            self.engine.all_agents.clear(); self.engine.selected_agent = None
            if hasattr(self.engine, 'obstacles'):
                self.engine.obstacles.load_dicts(data.get('obstacles', []))
            from .entities import create_random_food, Food
            food_items = data.get('foods') or data.get('food_items')
            if food_items:
                for fd in food_items:
                    food = Food(fd.get('x', 0.0), fd.get('y', 0.0), fd.get('r', 4.5))
                    food.energy = fd.get('energy', getattr(food, 'energy', food.r * food.r))
                    try:
                        color = fd.get('color')
                        if isinstance(color, (list, tuple)) and len(color) >= 3:
                            food.color = (int(color[0]), int(color[1]), int(color[2]))
                    except Exception:
                        pass
                    if not getattr(self.engine, 'obstacles', None) or not self.engine.obstacles.circle_overlaps(food.x, food.y, food.r):
                        self.engine.entities['foods'].append(food)
            else:
                food_count = int(data.get('food', {}).get('count', 0))
                for _ in range(food_count):
                    food = create_random_food(self.engine.entities['foods'], self.params, world.width, world.height)
                    if food is not None and self.engine.can_place_circle(food.x, food.y, food.r):
                        self.engine.entities['foods'].append(food)
            from .brain import NeuralNet
            from .sensors import RetinaSensor
            from .actuators import Locomotion, EnergyModel
            from .entities import Bacteria, Predator
            for ad in data.get('agents', []):
                sizes = ad.get('brain_sizes') or []
                brain = None
                if sizes:
                    brain = NeuralNet(list(sizes), init_std=0.01)
                    try:
                        bw = ad.get('brain_weights', []); bb = ad.get('brain_biases', [])
                        if bw and bb and len(bw)==len(bb):
                            brain.weights = bw; brain.biases = bb
                        brain.version = int(ad.get('brain_version', getattr(brain, 'version', 0)))
                    except Exception: pass
                sensor = RetinaSensor(
                    retina_count=ad.get('sensor_retina_count',18), vision_radius=ad.get('sensor_vision_radius',120.0),
                    fov_degrees=ad.get('sensor_fov_degrees',180.0), skip=ad.get('sensor_skip',0), see_food=ad.get('sensor_see_food',True),
                    see_bacteria=ad.get('sensor_see_bacteria',False), see_predators=ad.get('sensor_see_predators',False))
                locomotion = Locomotion(max_speed=ad.get('locomotion_max_speed',300.0), max_turn=ad.get('locomotion_max_turn', _m.pi))
                # Normalização de chaves de energia (v1 legacy e v2+ dinâmica)
                def _pick(*names, default=None):
                    for nm in names:
                        if nm in ad:
                            return ad.get(nm)
                    return default
                energy_model = EnergyModel(
                    death_energy=_pick('energy_death_energy','death_energy', default=0.0),
                    split_energy=_pick('energy_split_energy','split_energy', default=150.0),
                    v0_cost=_pick('energy_v0_cost','metab_v0_cost','energy_loss_idle', default=0.5),
                    vmax_cost=_pick('energy_vmax_cost','metab_vmax_cost','energy_loss_move', default=8.0),
                    vmax_ref=_pick('energy_vmax_ref','locomotion_max_speed', default=300.0),
                    energy_cap=_pick('energy_energy_cap','energy_cap', default=(600.0 if ad.get('type')=='predator' else 400.0))
                )
                cls = Predator if ad.get('type')=='predator' else Bacteria
                agent = cls(ad.get('x',0.0), ad.get('y',0.0), ad.get('r',9.0), brain, sensor, locomotion, energy_model, ad.get('angle',0.0))
                agent.vx = ad.get('vx',0.0); agent.vy = ad.get('vy',0.0); agent.energy = ad.get('energy',0.0); agent.age = ad.get('age',0.0)
                agent.last_reproduction_age = ad.get('last_reproduction_age', getattr(agent, 'last_reproduction_age', None))
                agent.last_brain_output = ad.get('last_brain_output', []); agent.last_brain_activations = ad.get('last_brain_activations', [])
                try:
                    color = ad.get('color')
                    if isinstance(color, (list, tuple)) and len(color) >= 3:
                        agent.color = (int(color[0]), int(color[1]), int(color[2]))
                except Exception:
                    pass
                if agent.is_predator: self.engine.entities['predators'].append(agent)
                else: self.engine.entities['bacteria'].append(agent)
                self.engine.all_agents.append(agent)
            selected_idx = data.get('selected_agent_index', None)
            if selected_idx is not None:
                try:
                    self.engine.selected_agent = self.engine.all_agents[int(selected_idx)]
                except Exception:
                    self.engine.selected_agent = None
            if hasattr(self.engine, '_resolve_obstacle_collisions'):
                self.engine._resolve_obstacle_collisions()
            if hasattr(self.engine, 'obstacles'):
                self.engine.obstacles.remove_food_overlaps(self.engine.entities['foods'])
            self.engine._spatial_hash_dirty = True
            rng_state = data.get('rng_state') or data.get('random_state')
            if rng_state:
                try:
                    from .random_utils import restore_rng_state
                    restore_rng_state(rng_state)
                except Exception as exc:
                    print(f"Falha ao restaurar estado RNG: {exc}")
            try:
                from .brain import clear_multi_brain_cache
                clear_multi_brain_cache()
            except Exception:
                pass
            print(f"Substrato importado de {path}")
        finally:
            if state_lock is not None:
                state_lock.release()
            self.widgets['paused'].setChecked(prev_paused)
            self.params.set('paused', prev_paused, validate=False)

    # ------------------------------------------------------------------
    # Auto export scheduling
    # ------------------------------------------------------------------
    def _on_toggle_auto_export(self):
        active = bool(self._get_widget_value('auto_export_substrate'))
        self.params.set('auto_export_substrate', active, validate=False)
        if active:
            self._reschedule_auto_export()
        else:
            if self._auto_export_timer:
                self._auto_export_timer.stop(); self._auto_export_timer = None

    def _reschedule_auto_export(self):
        if not bool(self._get_widget_value('auto_export_substrate')):
            return
        if self._auto_export_timer:
            self._auto_export_timer.stop()
        self._schedule_next_auto_export()

    def _schedule_next_auto_export(self, initial: bool=False):
        if not bool(self._get_widget_value('auto_export_substrate')):
            return
        minutes = float(self._get_widget_value('auto_export_interval_minutes') or 10.0)
        delay_ms = max(1, int(minutes * 60_000))
        if self._auto_export_timer is None:
            self._auto_export_timer = QTimer(self)
            self._auto_export_timer.timeout.connect(self._perform_auto_export)
        self._auto_export_timer.start(delay_ms)
        import datetime as _dt
        next_at = _dt.datetime.now() + _dt.timedelta(milliseconds=delay_ms)
        print(f"[AUTO-EXPORT] Agendado em {minutes} min (por volta de {next_at.strftime('%H:%M:%S')}).")

    def _perform_auto_export(self):
        if not bool(self._get_widget_value('auto_export_substrate')):
            return
        try:
            print("[AUTO-EXPORT] Iniciando export...")
            path = self._export_substrate(manual=False)
            print(f"[AUTO-EXPORT] Concluído: {path}")
        except Exception as e:
            self._log_exception("[AUTO-EXPORT] Erro", e)
        finally:
            self._schedule_next_auto_export()

    # ------------------------------------------------------------------
    # Public entry point
    # ------------------------------------------------------------------
    def run(self):  # mimic Tk version
        self.show()
        QApplication.instance().exec()


# Convenience runner (optional usage)
def run_ui(params: Params, engine: Engine, pygame_view: PygameView):
    app = QApplication.instance() or QApplication([])
    ui = SimulationUI(params, engine, pygame_view)
    ui.run()
