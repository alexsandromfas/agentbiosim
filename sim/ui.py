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
from typing import Dict, Any, Tuple

from PyQt6.QtCore import Qt, QTimer, QSize
from PyQt6.QtGui import QIcon, QColor
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QTabWidget,
    QLabel, QPushButton, QSpinBox, QDoubleSpinBox, QCheckBox, QComboBox,
    QLineEdit, QTextEdit, QListWidget, QMessageBox, QFileDialog, QScrollArea,
    QFormLayout, QGridLayout, QGroupBox
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


class SimulationUI(QMainWindow):
    """PyQt6 version of the simulation control UI."""

    def __init__(self, params: Params, engine: Engine, pygame_view: PygameView):
        super().__init__()
        self.params = params
        self.engine = engine
        self.pygame_view = pygame_view
        self._ui_params_csv = os.path.join(os.path.dirname(__file__), 'ui_params.csv')

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

        # Right pygame placeholder (native window id host)
        self.pygame_host = QWidget()
        self.pygame_host.setObjectName("pygame_host")
        self.pygame_host.setStyleSheet("#pygame_host { background: #101214; }")
        lay.addWidget(self.pygame_host, stretch=1)

    def _build_tabs(self):
        """Construct all tabs in a fixed order."""
        self._build_tab_simulation()
        self._build_tab_substrate()
        self._build_tab_bacteria()
        self._build_tab_predator()
        self._build_tab_test()
        self._build_tab_help()

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
            self.widgets[name]=w; grid.addWidget(QLabel(label), r_exec,0); grid.addWidget(w,r_exec,1); r_exec+=1
        w=_spin_double(0.01,100.0,0.01,3); w.setValue(self.params.get('time_scale',1.0)); add_exec("Escala de tempo (x):",'time_scale',w)
        w=_spin_int(1,240); w.setValue(self.params.get('fps',60)); add_exec("FPS:",'fps',w)
        cb=QCheckBox(); cb.setChecked(self.params.get('paused',False)); add_exec("Pausado:",'paused',cb)
        v.addWidget(g_exec)
        # Grupo: Performance & Render
        g_perf = QGroupBox("Performance & Render")
        g_perf.setStyleSheet(card_style)
        grid2 = QGridLayout(g_perf)
        r_perf = 0
        def add_perf(label, name, w):
            nonlocal r_perf
            self.widgets[name] = w
            grid2.addWidget(QLabel(label), r_perf, 0)
            grid2.addWidget(w, r_perf, 1)
            r_perf += 1

        cb = QCheckBox()
        cb.setChecked(self.params.get('use_spatial', True))
        add_perf("Spatial Hash:", 'use_spatial', cb)

        w = _spin_int(0, 10)
        w.setValue(self.params.get('retina_skip', 0))
        add_perf("Retina skip:", 'retina_skip', w)

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

        v.addWidget(g_perf)
        # Grupo: Visualização / Debug
        g_vis = QGroupBox("Visualização / Debug"); g_vis.setStyleSheet(card_style); grid3=QGridLayout(g_vis); r_vis=0
        def add_vis(label,name,w):
            nonlocal r_vis
            self.widgets[name]=w; grid3.addWidget(QLabel(label), r_vis,0); grid3.addWidget(w,r_vis,1); r_vis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('show_selected_details',True)); add_vis("Detalhes agente selecionado:",'show_selected_details',cb)
        cb=QCheckBox(); cb.setChecked(not self.params.get('disable_brain_activations',False)); cb.toggled.connect(self._on_toggle_brain_activations); add_vis("Mostrar ativações neurais:",'enable_brain_activations',cb)
        v.addWidget(g_vis)
        # Grupo: Auto Export
        g_auto = QGroupBox("Auto Export"); g_auto.setStyleSheet(card_style); grid4=QGridLayout(g_auto); r_auto=0
        def add_auto(label,name,w):
            nonlocal r_auto
            self.widgets[name]=w; grid4.addWidget(QLabel(label), r_auto,0); grid4.addWidget(w,r_auto,1); r_auto+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('auto_export_substrate',False)); cb.toggled.connect(self._on_toggle_auto_export); add_auto("Auto Export Substrato:",'auto_export_substrate',cb)
        w=_spin_double(0.1,1440.0,0.5,2); w.setValue(self.params.get('auto_export_interval_minutes',10.0)); add_auto("Intervalo export (min):",'auto_export_interval_minutes',w)
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
            gf.addWidget(QLabel(label), r_food, 0)
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
            gw.addWidget(QLabel(label), r_world, 0)
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
            gp.addWidget(QLabel(label), r_bpop,0)
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
            gb.addWidget(QLabel(label), r_bbody,0)
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
            self.widgets[name]=w; gn.addWidget(QLabel(label), r_bnn,0); gn.addWidget(w,r_bnn,1); r_bnn+=1
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
            self.widgets[name]=w; gv.addWidget(QLabel(label), r_bvis,0); gv.addWidget(w,r_bvis,1); r_bvis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_show_vision',False)); add_vis("Mostrar visão:",'bacteria_show_vision',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_food',True)); add_vis("Ver comida:",'bacteria_retina_see_food',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_bacteria',False)); add_vis("Ver bactérias:",'bacteria_retina_see_bacteria',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_predators',False)); add_vis("Ver predadores:",'bacteria_retina_see_predators',cb)
        v.addWidget(g_vis)
        # Ações
        g_act = QGroupBox("Ações"); g_act.setStyleSheet(card_style); la=QVBoxLayout(g_act)
        b=QPushButton("Aplicar Parâmetros"); b.clicked.connect(self.apply_bacteria_params); la.addWidget(b); v.addWidget(g_act)
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
            gp.addWidget(QLabel(label), r_ppop,0)
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
            self.widgets[name]=w; gb.addWidget(QLabel(label), r_pbody,0); gb.addWidget(w,r_pbody,1); r_pbody+=1
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
            self.widgets[name]=w; gn.addWidget(QLabel(label), r_pnn,0); gn.addWidget(w,r_pnn,1); r_pnn+=1
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
            self.widgets[name]=w; gv.addWidget(QLabel(label), r_pvis,0); gv.addWidget(w,r_pvis,1); r_pvis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_food',True)); add_vis("Ver comida:",'predator_retina_see_food',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_bacteria',True)); add_vis("Ver bactérias:",'predator_retina_see_bacteria',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_predators',False)); add_vis("Ver predadores:",'predator_retina_see_predators',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_show_vision',False)); add_vis("Mostrar visão:",'predator_show_vision',cb)
        v.addWidget(g_vis)
        # Ações
        g_act = QGroupBox("Ações"); g_act.setStyleSheet(card_style); la=QVBoxLayout(g_act)
        b=QPushButton("Aplicar Parâmetros"); b.clicked.connect(self.apply_predator_params); la.addWidget(b); v.addWidget(g_act)
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
                grid.addWidget(QLabel(label_text), row_idx, 0)
                grid.addWidget(w, row_idx, 1)
                row_idx += 1
                param_index += 1
            v.addWidget(gb)

        v.addStretch(1)

    # ------------------------------------------------------------------
    # Live callbacks & embedding
    # ------------------------------------------------------------------
    def _setup_live_param_signals(self):
        for name in ['time_scale','fps','paused','simple_render','bacteria_show_vision','predator_show_vision','show_selected_details','retina_vision_mode']:
            w = self.widgets.get(name)
            if isinstance(w, (QSpinBox, QDoubleSpinBox)):
                w.valueChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(w, QCheckBox):
                w.toggled.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(w, QComboBox):
                w.currentTextChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
        # brain activation toggle handled separately

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
            print(f"Erro callback {name}: {e}")

    def _init_pygame_view(self):
        try:
            win_id = int(self.pygame_host.winId())  # native window id
            self.pygame_view.initialize(win_id)
            def runner():
                try:
                    self.pygame_view.run()
                except Exception as e:
                    print(f"Erro thread sim: {e}")
            self._sim_thread = threading.Thread(target=runner, daemon=True)
            self._sim_thread.start()
        except Exception as e:
            print(f"Falha ao inicializar pygame embutido: {e}")

    # ------------------------------------------------------------------
    # Apply parameter groups
    # ------------------------------------------------------------------
    def apply_simulation_params(self):
        for name in ['time_scale','fps','paused','use_spatial','retina_skip','retina_vision_mode','simple_render','reuse_spatial_grid','agents_inertia','show_selected_details']:
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
        try:
            shape = self.params.get('substrate_shape','rectangular')
            radius = self.params.get('substrate_radius',350.0)
            world = self.engine.world
            world_w = self.params.get('world_w', world.width)
            world_h = self.params.get('world_h', world.height)
            world.configure(shape, radius, world_w, world_h)
            if shape == 'circular':
                for entity in list(self.engine.agents) + list(self.engine.foods):
                    if hasattr(entity,'x') and hasattr(entity,'y') and hasattr(entity,'r'):
                        entity.x, entity.y = world.clamp_position(entity.x, entity.y, getattr(entity,'r',0.0))
            # color pickers already update params and propagate; nothing else to do here
            print("Parâmetros de substrato aplicados")
        except Exception as e:
            print(f"Erro apply substrate: {e}")

    def apply_bacteria_params(self):
        for name in [
            'bacteria_count','bacteria_initial_energy','bacteria_death_energy','bacteria_split_energy',
            'bacteria_metab_v0_cost','bacteria_metab_vmax_cost','bacteria_energy_cap',
            'bacteria_show_vision','bacteria_body_size','bacteria_vision_radius','bacteria_retina_count',
            'bacteria_retina_fov_degrees','bacteria_retina_see_food','bacteria_retina_see_bacteria','bacteria_retina_see_predators',
            'bacteria_max_speed','bacteria_min_limit','bacteria_max_limit','bacteria_hidden_layers','bacteria_mutation_rate',
            'bacteria_mutation_strength','bacteria_max_turn_deg'
        ]:
            if name in self.widgets:
                v = self._get_widget_value(name)
                if name == 'bacteria_max_turn_deg':
                    self.params.set('bacteria_max_turn', math.radians(v))
                else:
                    self.params.set(name, v)
        for i in range(1,6):
            nm = f'bacteria_neurons_layer_{i}'
            if nm in self.widgets:
                self.params.set(nm, self._get_widget_value(nm))
    # color picker button handles bacteria color persistence/propagation
    print("Parâmetros de bactérias aplicados")

    def apply_predator_params(self):
        for name in [
            'predators_enabled','predator_count','predator_initial_energy','predator_death_energy','predator_split_energy',
            'predator_metab_v0_cost','predator_metab_vmax_cost','predator_energy_cap',
            'predator_body_size','predator_show_vision','predator_vision_radius','predator_retina_see_food','predator_retina_count',
            'predator_retina_fov_degrees','predator_retina_see_bacteria','predator_retina_see_predators','predator_max_speed',
            'predator_min_limit','predator_max_limit','predator_hidden_layers','predator_mutation_rate','predator_mutation_strength','predator_max_turn_deg'
        ]:
            if name in self.widgets:
                v = self._get_widget_value(name)
                if name == 'predator_max_turn_deg':
                    self.params.set('predator_max_turn', math.radians(v))
                else:
                    self.params.set(name, v)
        for i in range(1,6):
            nm = f'predator_neurons_layer_{i}'
            if nm in self.widgets:
                self.params.set(nm, self._get_widget_value(nm))
    # color picker button handles predator color persistence/propagation
    print("Parâmetros de predadores aplicados")

    def apply_all_params(self):
        self.apply_simulation_params(); self.apply_substrate_params(); self.apply_bacteria_params(); self.apply_predator_params()
        self.params.set('auto_export_substrate', bool(self._get_widget_value('auto_export_substrate')), validate=False)
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

    # ------------------------------------------------------------------
    # Persistence CSV
    # ------------------------------------------------------------------
    def save_ui_params(self):
        try:
            rows = []
            for name in sorted(self.widgets.keys()):
                val = self._get_widget_value(name)
                rows.append({'name': name, 'value': val})
            # Ensure color params and substrate shape are saved too
            try:
                import json as _json
                rows.append({'name': 'substrate_shape', 'value': self._get_widget_value('substrate_shape')})
                rows.append({'name': 'substrate_bg_color', 'value': _json.dumps(list(self.params.get('substrate_bg_color', (10,10,20))))})
                rows.append({'name': 'food_color', 'value': _json.dumps(list(self.params.get('food_color', (220,30,30))))})
                rows.append({'name': 'bacteria_color', 'value': _json.dumps(list(self.params.get('bacteria_color', (220,220,220))))})
                rows.append({'name': 'predator_color', 'value': _json.dumps(list(self.params.get('predator_color', (80,120,220))))})
                # Camera position/zoom
                try:
                    cam = getattr(self.engine, 'camera', None)
                    if cam is not None:
                        rows.append({'name': 'camera_x', 'value': cam.x})
                        rows.append({'name': 'camera_y', 'value': cam.y})
                        rows.append({'name': 'camera_zoom', 'value': cam.zoom})
                except Exception:
                    pass
            except Exception:
                pass
            with open(self._ui_params_csv,'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['name','value']); writer.writeheader(); writer.writerows(rows)
            print(f"Parâmetros UI salvos em {self._ui_params_csv}")
        except Exception as e:
            print(f"Erro ao salvar parâmetros UI: {e}")

    def _load_ui_params_csv(self):
        if not os.path.exists(self._ui_params_csv):
            return
        try:
            with open(self._ui_params_csv,'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    name = row.get('name'); value = row.get('value')
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
                    except Exception:
                        pass
            # schedule auto export if active
            if self._get_widget_value('auto_export_substrate'):
                self._schedule_next_auto_export(initial=True)
            print(f"Parâmetros UI carregados de {self._ui_params_csv}")
        except Exception as e:
            print(f"Erro ao carregar parâmetros UI: {e}")

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
            self._export_selected_agent(agent, os.path.splitext(os.path.basename(name))[0])
            QMessageBox.information(self, "Exportar Agente", f"Agente exportado em {name}")
        except Exception as e:
            QMessageBox.warning(self, "Erro", str(e))

    def _export_selected_agent(self, agent, name: str) -> str:
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'agents'))
        os.makedirs(agents_dir, exist_ok=True)
        filename = f"{name}.agent.csv"
        path = os.path.join(agents_dir, filename)
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
            QMessageBox.warning(self, "Erro", str(e))

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
            QMessageBox.warning(self, "Erro", str(e))

    def open_import_substrate_window(self):
        manual_dir, auto_dir = self._get_substrate_dirs()
        start_dir = manual_dir if os.path.isdir(manual_dir) else auto_dir
        path, _ = QFileDialog.getOpenFileName(self, "Importar Substrato", start_dir, "Snapshot (*.json)")
        if not path: return
        try:
            self._import_substrate(path)
            QMessageBox.information(self, "Importar Substrato", "Importado.")
        except Exception as e:
            QMessageBox.warning(self, "Erro", str(e))

    def _export_substrate(self, prefix: str='substrato', manual: bool=True) -> str:
        import time
        prev_paused = bool(self._get_widget_value('paused'))
        self.widgets['paused'].setChecked(True)
        self.params.set('paused', True, validate=False)
        try:
            engine = self.engine
            # Aplica todos os parâmetros atuais antes de capturar snapshot
            try:
                self.apply_all_params()
            except Exception:
                pass
            manual_dir, auto_root = self._get_substrate_dirs()
            ts_full = time.strftime('%Y%m%d_%H%M%S')
            if manual:
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
            path = os.path.join(out_dir, filename)
            params_snapshot = dict(self.params._data)
            ui_snapshot = {k:self._get_widget_value(k) for k in self.widgets.keys()}
            world = engine.world; camera = engine.camera
            agents_data = []
            for agent in engine.all_agents:
                brain = getattr(agent,'brain',None); sensor = getattr(agent,'sensor',None); locomotion = getattr(agent,'locomotion',None); energy_model = getattr(agent,'energy_model',None)
                ad = {
                    'type': 'predator' if getattr(agent,'is_predator', False) else 'bacteria',
                    'x': agent.x,'y': agent.y,'r': agent.r,'angle': agent.angle,'vx': agent.vx,'vy': agent.vy,
                    'energy': getattr(agent,'energy',0.0),'age': getattr(agent,'age',0.0)
                }
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
                # On-demand forward/activations para snapshot completo sem manter arrays históricos
                out = getattr(agent,'last_brain_output', [])
                if (not out) and brain and sensor:
                    try:
                        sensor_inputs = sensor.sense(agent, self.engine.scene_query, self.params)
                        out = brain.forward(sensor_inputs)
                    except Exception:
                        out = []
                ad['last_brain_output'] = out
                acts = getattr(agent,'last_brain_activations', [])
                if (not acts) and brain and sensor:
                    try:
                        sensor_inputs = sensor.sense(agent, self.engine.scene_query, self.params)
                        acts = brain.activations(sensor_inputs)
                    except Exception:
                        acts = []
                ad['last_brain_activations'] = acts
                agents_data.append(ad)
            snapshot = {
                'version':2,'timestamp': ts_full,'params': params_snapshot,'ui_params': ui_snapshot,
                'world': {'width': world.width,'height': world.height,'shape': world.shape,'radius': world.radius},
                'camera': {'x': engine.camera.x,'y': engine.camera.y,'zoom': engine.camera.zoom},
                'simulation': {'total_simulation_time': engine.total_simulation_time},
                'food': {'count': len(engine.entities['foods']), 'target': self.params.get('food_target',0)},
                'agents': agents_data
            }
            with open(path,'w', encoding='utf-8') as f: json.dump(snapshot, f)
            print(f"Substrato exportado para {path}")
            return path
        finally:
            self.widgets['paused'].setChecked(prev_paused)
            self.params.set('paused', prev_paused, validate=False)

    def _import_substrate(self, path: str):
        import math as _m
        prev_paused = bool(self._get_widget_value('paused'))
        self.widgets['paused'].setChecked(True)
        self.params.set('paused', True, validate=False)
        try:
            with open(path,'r', encoding='utf-8') as f:
                data = json.load(f)
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
            # Clear current entities
            for lst in self.engine.entities.values(): lst.clear()
            self.engine.all_agents.clear(); self.engine.selected_agent = None
            from .entities import create_random_food
            food_count = int(data.get('food', {}).get('count', 0))
            for _ in range(food_count):
                food = create_random_food(self.engine.entities['foods'], self.params, world.width, world.height)
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
                agent.last_brain_output = ad.get('last_brain_output', []); agent.last_brain_activations = ad.get('last_brain_activations', [])
                if agent.is_predator: self.engine.entities['predators'].append(agent)
                else: self.engine.entities['bacteria'].append(agent)
                self.engine.all_agents.append(agent)
            print(f"Substrato importado de {path}")
        finally:
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
            print(f"[AUTO-EXPORT] Erro: {e}")
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
