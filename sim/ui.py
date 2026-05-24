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
import time
from typing import Dict, Any, Tuple

from PyQt6.QtCore import Qt, QTimer, QSize, QEvent
from PyQt6.QtGui import QIcon, QColor, QAction, QActionGroup, QPixmap, QPainter, QPen, QBrush, QShortcut, QKeySequence
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QTabWidget,
    QLabel, QPushButton, QSpinBox, QDoubleSpinBox, QCheckBox, QComboBox,
    QLineEdit, QTextEdit, QMessageBox, QFileDialog, QScrollArea,
    QFormLayout, QGridLayout, QGroupBox, QToolTip, QStackedWidget,
    QColorDialog, QSlider, QRadioButton, QButtonGroup, QFrame,
    QTableWidget, QTableWidgetItem, QHeaderView, QAbstractItemView, QAbstractSpinBox, QDialog
)

from .controllers import Params
from .diagnostics import log_event, log_exception
from .engine import Engine
from .game import PygameView
from .neural_viewer import AgentNeuralNetworkView
from .profiler import profiler


# -------------------------- Helper abstractions --------------------------

def _spin_int(min_v: int, max_v: int, step: int = 1) -> QSpinBox:
    w = QSpinBox()
    w.setRange(min_v, max_v)
    w.setSingleStep(step)
    return w


def _param_int(params, name: str, default: int) -> int:
    try:
        return int(float(params.get(name, default)))
    except Exception:
        return int(default)


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


class MetricHistoryChart(QWidget):
    """Grafico leve com normalizacao independente por metrica."""

    def __init__(self):
        super().__init__()
        self.setMinimumHeight(150)
        self.setMaximumHeight(190)
        self.history: list[dict[str, float]] = []
        self.visible_keys: set[str] | None = None
        self.time_window_seconds: float | None = 3600.0
        self.metric_defs = [
            ("organisms", "Organismos", QColor(118, 205, 255)),
            ("foods", "Comida", QColor(255, 208, 92)),
            ("effective_time_scale", "Tempo efetivo", QColor(120, 180, 255)),
        ]

    def set_history(self, history: list[dict[str, float]]):
        self.history = history
        self.update()

    def set_metric_defs(self, metric_defs):
        self.metric_defs = list(metric_defs)
        self.update()

    def set_visible_keys(self, keys):
        self.visible_keys = set(keys) if keys is not None else None
        self.update()

    def set_time_window_seconds(self, seconds):
        self.time_window_seconds = None if seconds is None else float(seconds)
        self.update()

    @staticmethod
    def _compact(value: float) -> str:
        number = abs(float(value))
        sign = "-" if value < 0 else ""
        if number >= 1_000_000:
            return f"{sign}{number / 1_000_000:.1f}M"
        if number >= 1_000:
            return f"{sign}{number / 1_000:.1f}k"
        return f"{value:.2f}"

    @staticmethod
    def _format_seconds(seconds: float) -> str:
        seconds = max(0.0, float(seconds))
        if seconds >= 86400:
            return f"{seconds / 86400:.1f}d"
        if seconds >= 3600:
            return f"{seconds / 3600:.1f}h"
        if seconds >= 60:
            return f"{seconds / 60:.1f}min"
        return f"{seconds:.0f}s"

    def _visible_history(self, max_points: int):
        rows = list(self.history)
        if not rows:
            return rows
        if self.time_window_seconds is not None:
            end_t = float(rows[-1].get('t', 0.0) or 0.0)
            start_t = end_t - self.time_window_seconds
            rows = [row for row in rows if float(row.get('t', 0.0) or 0.0) >= start_t]
        if len(rows) > max_points:
            step = max(1, int(math.ceil(len(rows) / max_points)))
            rows = rows[::step]
            if rows[-1] is not self.history[-1]:
                rows.append(self.history[-1])
        return rows

    def paintEvent(self, event):
        painter = QPainter(self)
        try:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            w = self.width()
            h = self.height()
            painter.fillRect(0, 0, w, h, QColor(14, 17, 22))
            painter.setPen(QPen(QColor(48, 55, 65), 1))
            painter.drawRect(0, 0, w - 1, h - 1)

            visible_rows = self._visible_history(max(300, w * 2))
            if len(visible_rows) < 2:
                painter.setPen(QColor(150, 164, 181))
                painter.drawText(12, 28, "Grafico aguardando amostras da simulacao...")
                return

            left, top, right, bottom = 52, 14, 12, 34
            plot_w = max(1, w - left - right)
            plot_h = max(1, h - top - bottom)
            painter.setPen(QPen(QColor(38, 45, 54), 1))
            for frac in (0.25, 0.5, 0.75):
                y = int(top + plot_h * frac)
                painter.drawLine(left, y, left + plot_w, y)

            visible_defs = []
            for key, label, color in self.metric_defs:
                if self.visible_keys is not None and key not in self.visible_keys:
                    continue
                values = [float(row.get(key, 0.0) or 0.0) for row in visible_rows]
                if key == "predators" and max(values) <= 0.0:
                    continue
                visible_defs.append((key, label, color, values))

            n = len(visible_rows)
            if not visible_defs:
                painter.setPen(QColor(150, 164, 181))
                painter.drawText(12, 28, "Nenhuma metrica selecionada.")
                return
            for key, label, color, values in visible_defs:
                vmin = min(values)
                vmax = max(values)
                span = vmax - vmin
                if span <= 1e-9:
                    span = max(abs(vmax), 1.0)
                    vmin = 0.0
                painter.setPen(QPen(color, 2))
                prev = None
                for idx, value in enumerate(values):
                    x = int(left + (idx / max(1, n - 1)) * plot_w)
                    norm = (value - vmin) / span
                    y = int(top + (1.0 - max(0.0, min(1.0, norm))) * plot_h)
                    if prev is not None:
                        painter.drawLine(prev[0], prev[1], x, y)
                    prev = (x, y)

            first_t = float(visible_rows[0].get('t', 0.0) or 0.0)
            last_t = float(visible_rows[-1].get('t', 0.0) or 0.0)
            mid_t = (first_t + last_t) * 0.5
            painter.setPen(QColor(142, 155, 172))
            painter.drawLine(left, top + plot_h, left + plot_w, top + plot_h)
            painter.drawText(left, h - 9, self._format_seconds(first_t))
            mid_text = self._format_seconds(mid_t)
            painter.drawText(left + plot_w // 2 - 24, h - 9, mid_text)
            end_text = self._format_seconds(last_t)
            painter.drawText(left + plot_w - 48, h - 9, end_text)
        finally:
            painter.end()


class ChartResizeHandle(QFrame):
    """Pequeno controle para ajustar a altura do painel de grafico."""

    def __init__(self, chart: MetricHistoryChart):
        super().__init__()
        self.chart = chart
        self._drag_start_y = None
        self._drag_start_h = None
        self.setCursor(Qt.CursorShape.SizeVerCursor)
        self.setToolTip("Arraste para aumentar ou diminuir a altura do grafico.")
        self.setFixedSize(72, 7)
        self.setStyleSheet(
            "QFrame { background:#3b4652; border-radius:3px; } "
            "QFrame:hover { background:#6f8195; }"
        )

    def mousePressEvent(self, event):
        if event.button() == Qt.MouseButton.LeftButton:
            self._drag_start_y = event.globalPosition().y()
            self._drag_start_h = self.chart.height()
            event.accept()
            return
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if self._drag_start_y is None or self._drag_start_h is None:
            return super().mouseMoveEvent(event)
        dy = event.globalPosition().y() - self._drag_start_y
        new_h = int(max(100, min(520, self._drag_start_h + dy)))
        self.chart.setMinimumHeight(new_h)
        self.chart.setMaximumHeight(new_h)
        event.accept()

    def mouseReleaseEvent(self, event):
        self._drag_start_y = None
        self._drag_start_h = None
        super().mouseReleaseEvent(event)


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

        self._app_title = "AgentBioSim V1.0.0"
        self.setWindowTitle(f"{self._app_title} - Sem salvar")
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
        self._live_param_signals_ready = False
        self._auto_export_timer: QTimer | None = None
        self._diagnostic_timer: QTimer | None = None
        self._recovery_snapshot_saved = False
        self._current_biosim_path: str | None = None
        self._intelligence_cache: Dict[str, float] = {}
        self._intelligence_cache_agent = None
        self._intelligence_cache_time = 0.0
        self._metrics_history: list[dict[str, float]] = []
        self._chart_current_values: dict[str, float] = {}
        self._chart_metric_checkboxes: dict[str, QCheckBox] = {}
        self._chart_intake_cache: dict[int, dict[Any, dict[str, float]]] = {}
        self._chart_group_smart_ema: dict[int, float] = {}
        self._ui_params_autosave_ready = False
        self._ui_params_save_timer = QTimer(self)
        self._ui_params_save_timer.setSingleShot(True)
        self._ui_params_save_timer.timeout.connect(lambda: self.save_ui_params(silent=True))

        self._build_layout()
        self._update_window_title()
        self._build_tabs()
        self._load_ui_params_csv()  # load after widget creation so we can set values
        if 'obstacle_brush_width' in self.widgets:
            self._update_brush_width(self._get_widget_value('obstacle_brush_width'))
        if 'obstacle_brush_erase' in self.widgets:
            self._update_brush_erase()
        self._build_menu_bar()
        self._setup_keyboard_shortcuts()
        self._setup_live_param_signals()
        self._live_param_signals_ready = True
        self._ui_params_autosave_ready = True
        self._start_diagnostic_heartbeat()
        self._start_prototype_sync_timer()
        log_event("UI_CREATED")

        # Embed pygame view (defer until shown)
        QTimer.singleShot(100, self._init_pygame_view)

    # ------------------------------------------------------------------
    # Layout / Tabs
    # ------------------------------------------------------------------
    def _build_layout(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(6, 6, 6, 6)
        root.setSpacing(6)
        root.addWidget(self._build_top_tools_strip(), stretch=0)

        lay = QHBoxLayout()
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(6)
        root.addLayout(lay, stretch=1)

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
        self.metrics_panel = self._build_metrics_panel()
        self.metrics_panel.setVisible(bool(self.params.get('show_metrics_chart', False)))
        sim_lay.addWidget(self.metrics_panel, stretch=0)
        lay.addWidget(self.sim_container, stretch=1)
        self.agent_details_panel = self._build_selected_agent_panel()
        lay.addWidget(self.agent_details_panel, stretch=0)

    def _base_chart_metric_defs(self):
        return [
            ("organisms", "Organismos", QColor(118, 205, 255)),
            ("foods", "Comida", QColor(255, 208, 92)),
            ("effective_time_scale", "Tempo efetivo", QColor(120, 180, 255)),
        ]

    def _build_metrics_panel(self) -> QWidget:
        panel = QWidget()
        panel.setObjectName("metrics_panel")
        panel.setStyleSheet(
            "#metrics_panel { background:#15181d; border:1px solid #303741; } "
            "QLabel, QCheckBox, QRadioButton { color:#dce7f3; } "
            "QComboBox { min-height:22px; }"
        )
        outer = QVBoxLayout(panel)
        outer.setContentsMargins(8, 6, 8, 6)
        outer.setSpacing(5)

        top = QHBoxLayout()
        top.setSpacing(8)
        top.addWidget(QLabel("Grafico"))
        top.addWidget(QLabel("Amostragem:"))
        self._chart_sample_group = QButtonGroup(self)
        chart_sample = int(self.params.get('metrics_chart_sample_seconds', 5) or 5)
        for seconds, label in ((1, "1s"), (5, "5s"), (30, "30s"), (60, "1min"), (600, "10min")):
            rb = QRadioButton(label)
            rb.setChecked(seconds == chart_sample)
            self._chart_sample_group.addButton(rb, seconds)
            top.addWidget(rb)
        self._chart_sample_group.idClicked.connect(self._set_chart_sample_seconds)

        top.addWidget(QLabel("Janela:"))
        self._chart_window_combo = QComboBox()
        self._chart_window_combo.addItem("Ultima 1h", 3600)
        self._chart_window_combo.addItem("Ultimas 5h", 5 * 3600)
        self._chart_window_combo.addItem("Ultimas 10h", 10 * 3600)
        self._chart_window_combo.addItem("Ultimas 24h", 24 * 3600)
        self._chart_window_combo.addItem("Tudo", -1)
        self._chart_window_combo.currentIndexChanged.connect(self._on_chart_window_changed)
        top.addWidget(self._chart_window_combo)
        top.addStretch(1)

        self.metrics_chart = MetricHistoryChart()
        resize = ChartResizeHandle(self.metrics_chart)
        top.addWidget(resize)
        outer.addLayout(top)

        checks_row = QHBoxLayout()
        checks_row.setSpacing(8)
        checks_row.addWidget(QLabel("Linhas:"))
        self._chart_checks_widget = QWidget()
        self._chart_checks_layout = QHBoxLayout(self._chart_checks_widget)
        self._chart_checks_layout.setContentsMargins(0, 0, 0, 0)
        self._chart_checks_layout.setSpacing(8)
        checks_row.addWidget(self._chart_checks_widget, stretch=1)
        outer.addLayout(checks_row)

        outer.addWidget(self.metrics_chart)
        self._hidden_chart_metrics = set()
        self._refresh_chart_metric_checkboxes()

        self._metrics_chart_timer = QTimer(self)
        self._metrics_chart_timer.timeout.connect(self._update_metrics_chart)
        self._metrics_chart_timer.start(max(1, chart_sample) * 1000)
        return panel

    def _build_top_tools_strip(self) -> QWidget:
        strip = QWidget()
        strip.setObjectName("top_tools_strip")
        strip.setStyleSheet(
            "#top_tools_strip { background:#15181d; border:1px solid #303741; }"
        )
        layout = QHBoxLayout(strip)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self.runtime_toolbar = self._build_runtime_toolbar()
        self.canvas_tools = self._build_canvas_tools()
        layout.addWidget(self.runtime_toolbar, stretch=0)
        layout.addWidget(self.canvas_tools, stretch=1)

        self._runtime_sync_timer = QTimer(self)
        self._runtime_sync_timer.timeout.connect(self._sync_time_toolbar_from_params)
        self._runtime_sync_timer.start(500)
        return strip

    def _build_runtime_toolbar(self) -> QWidget:
        bar = QWidget()
        bar.setObjectName("runtime_toolbar")
        bar.setStyleSheet(
            "#runtime_toolbar { background:#15181d; border-top:1px solid #303741; } "
            "QLabel { color:#dce7f3; }"
        )
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(8, 5, 8, 5)
        layout.setSpacing(8)
        asset_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Assets'))

        def add_command(text: str, tooltip: str, slot, icon_path: str | None = None):
            btn = QPushButton(text)
            btn.setToolTip(tooltip)
            btn.setFixedSize(34, 30)
            if icon_path and os.path.exists(icon_path):
                btn.setText("")
                btn.setIcon(QIcon(icon_path))
                btn.setIconSize(QSize(22, 22))
            btn.clicked.connect(slot)
            layout.addWidget(btn)
            return btn

        add_command('Play', 'Iniciar ou despausar a simulacao.', self.play_simulation, os.path.join(asset_dir, 'Play.png'))
        add_command('Pause', 'Pausar a simulacao sem apagar o estado atual.', self.pause_simulation, os.path.join(asset_dir, 'Pause.png'))
        add_command('Stop', 'Parar e resetar a populacao usando os setups atuais.', self.stop_simulation, os.path.join(asset_dir, 'Stop.png'))
        layout.addSpacing(12)
        layout.addWidget(QLabel("Velocidade"))

        self._time_scale_slider = QSlider(Qt.Orientation.Horizontal)
        self._time_scale_slider.setRange(10, 5000)
        self._time_scale_slider.setSingleStep(10)
        self._time_scale_slider.setPageStep(100)
        self._time_scale_slider.setFixedWidth(220)
        self._time_scale_slider.setValue(int(float(self.params.get('time_scale', 1.0)) * 100))
        self._time_scale_slider.valueChanged.connect(self._on_time_scale_slider_changed)
        layout.addWidget(self._time_scale_slider, stretch=0)

        spin = _spin_double(0.1, 50.0, 0.1, 2)
        spin.setValue(float(self.params.get('time_scale', 1.0)))
        spin.setSuffix("x")
        spin.setFixedWidth(90)
        spin.valueChanged.connect(self._on_time_scale_spin_changed)
        self.widgets['time_scale'] = spin
        self._time_scale_spin = spin
        layout.addWidget(spin)
        return bar

    def _build_status_bar(self) -> QWidget:
        bar = QWidget()
        bar.setObjectName("simulation_status_bar")
        bar.setStyleSheet(
            "#simulation_status_bar { background:#15181d; border:1px solid #303741; } "
            "QLabel { color:#dce7f3; } QLabel.metric_name { color:#91a2b5; }"
        )
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(10, 5, 10, 5)
        layout.setSpacing(12)
        self._status_labels: Dict[str, QLabel] = {}

        def add_metric(key: str, name: str):
            title = QLabel(f"{name}:")
            title.setProperty("class", "metric_name")
            value = QLabel("-")
            value.setMinimumWidth(42)
            layout.addWidget(title)
            layout.addWidget(value)
            self._status_labels[key] = value

        for key, name in [
            ('organisms', 'Organismos'),
            ('foods', 'Comida'),
            ('food_target', 'Alvo'),
            ('obstacles', 'Obstaculos'),
            ('fps', 'FPS'),
            ('cpu', 'CPU'),
            ('ram', 'RAM'),
            ('time_scale', 'Tempo'),
            ('effective_time_scale', 'Efetivo'),
            ('physics', 'Fisica'),
            ('backlog', 'Atraso'),
            ('world', 'Mundo'),
        ]:
            add_metric(key, name)
        layout.addStretch(1)

        self._status_timer = QTimer(self)
        self._status_timer.timeout.connect(self._update_status_bar)
        self._status_timer.start(500)
        return bar

    def _setup_keyboard_shortcuts(self):
        shortcuts = [
            ("Delete", self._shortcut_delete_selected_agents),
            ("Space", self._shortcut_toggle_pause),
            ("Esc", self._shortcut_clear_selection),
        ]
        self._keyboard_shortcuts = []
        for sequence, slot in shortcuts:
            shortcut = QShortcut(QKeySequence(sequence), self)
            shortcut.setContext(Qt.ShortcutContext.WindowShortcut)
            shortcut.activated.connect(slot)
            self._keyboard_shortcuts.append(shortcut)

    def _keyboard_shortcut_allowed(self) -> bool:
        focus = QApplication.focusWidget()
        if isinstance(focus, (QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox)):
            return False
        if isinstance(focus, QTextEdit) and not focus.isReadOnly():
            return False
        if isinstance(focus, QTableWidget):
            try:
                return focus.state() != QAbstractItemView.State.EditingState
            except Exception:
                return True
        return True

    def _shortcut_delete_selected_agents(self):
        if not self._keyboard_shortcut_allowed():
            return
        self.delete_selected_agents()

    def _shortcut_toggle_pause(self):
        if not self._keyboard_shortcut_allowed():
            return
        if bool(self.params.get('paused', False)):
            self.play_simulation()
        else:
            self.pause_simulation()

    def _shortcut_clear_selection(self):
        if not self._keyboard_shortcut_allowed():
            return
        state_lock = getattr(self.engine, 'state_lock', None)
        if state_lock is None:
            self.engine._clear_selection()
        else:
            with state_lock:
                self.engine._clear_selection()

    def _update_status_bar(self):
        labels = self.__dict__.get('_status_labels', {})
        if not labels:
            return
        engine = self.engine
        labels['organisms'].setText(str(len(getattr(engine, 'all_agents', []))))
        labels['foods'].setText(str(len(engine.entities.get('foods', []))))
        labels['food_target'].setText(str(self.params.get('food_target', 0)))
        labels['obstacles'].setText(str(len(getattr(engine, 'obstacles', []))))
        labels['fps'].setText(str(int(getattr(engine, 'current_fps', 0.0))))
        if getattr(engine, 'resources_available', False):
            labels['cpu'].setText(f"{float(getattr(engine, 'cpu_proc_percent', 0.0)):.0f}%")
            labels['ram'].setText(f"{float(getattr(engine, 'mem_used_mb', 0.0)):.0f} MB")
        else:
            labels['cpu'].setText("N/A")
            labels['ram'].setText("N/A")
        labels['time_scale'].setText(f"{float(self.params.get('time_scale', 1.0)):.2f}x")
        labels['effective_time_scale'].setText(f"{float(getattr(engine, 'effective_time_scale', 0.0)):.2f}x")
        physics_hz = float(self.params.get('physics_steps_per_second', 30))
        actual_physics = float(getattr(engine, 'physics_steps_per_wall_second', 0.0))
        labels['physics'].setText(f"{actual_physics:.0f}/{physics_hz:.0f} Hz")
        backlog = float(getattr(engine, 'simulation_backlog', 0.0))
        dropped = float(getattr(engine, 'dropped_simulation_time', 0.0))
        labels['backlog'].setText(f"{backlog:.2f}s" if dropped <= 0 else f"{backlog:.2f}s drop {dropped:.1f}s")
        world = engine.world
        if getattr(world, 'shape', 'rectangular') == 'circular':
            labels['world'].setText(f"circular r {float(getattr(world, 'radius', 0.0)):.0f}")
        else:
            labels['world'].setText(f"{float(getattr(world, 'width', 0.0)):.0f}x{float(getattr(world, 'height', 0.0)):.0f}")
        self._sync_time_toolbar_from_params()

    def _sync_time_toolbar_from_params(self):
        value = max(0.1, min(50.0, float(self.params.get('time_scale', 1.0))))
        slider = getattr(self, '_time_scale_slider', None)
        spin = getattr(self, '_time_scale_spin', None)
        if slider is not None and not slider.isSliderDown():
            slider.blockSignals(True)
            slider.setValue(int(round(value * 100)))
            slider.blockSignals(False)
        if spin is not None and not spin.hasFocus():
            spin.blockSignals(True)
            spin.setValue(value)
            spin.blockSignals(False)

    def _set_time_scale_value(self, value: float):
        value = max(0.1, min(50.0, float(value)))
        self.params.set('time_scale', value, validate=False)
        slider = getattr(self, '_time_scale_slider', None)
        spin = getattr(self, '_time_scale_spin', None)
        if slider is not None:
            slider.blockSignals(True)
            slider.setValue(int(round(value * 100)))
            slider.blockSignals(False)
        if spin is not None:
            spin.blockSignals(True)
            spin.setValue(value)
            spin.blockSignals(False)
        self._schedule_ui_params_save()

    def _on_time_scale_slider_changed(self, raw_value: int):
        self._set_time_scale_value(float(raw_value) / 100.0)

    def _on_time_scale_spin_changed(self, value: float):
        self._set_time_scale_value(float(value))

    def _current_chart_metric_defs(self):
        defs = list(self._base_chart_metric_defs())
        for label_id, meta in sorted(getattr(self.engine, 'agent_labels', {}).items()):
            if not bool(meta.get('show_chart', True)):
                continue
            color = QColor(*meta.get('color', (220, 220, 220)))
            name = str(meta.get('name', f'Label {label_id}'))
            count_color = QColor(
                min(255, color.red() + 45),
                min(255, color.green() + 45),
                min(255, color.blue() + 45),
            )
            defs.append((f'label_{label_id}_count', f'{name} individuos', count_color))
            defs.append((f'label_{label_id}_smart', f'{name} inteligencia', color))
        return defs

    def _refresh_chart_metric_checkboxes(self):
        state = self.__dict__
        layout = state.get('_chart_checks_layout')
        chart = state.get('metrics_chart')
        if layout is None or chart is None:
            return
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
        metric_defs = self._current_chart_metric_defs()
        visible = []
        boxes = {}
        state['_chart_metric_checkboxes'] = boxes
        for key, label, color in metric_defs:
            cb = QCheckBox(self._chart_metric_text(key, label))
            cb.setProperty("metric_label", label)
            cb.setChecked(key not in self._hidden_chart_metrics)
            cb.setStyleSheet(f"QCheckBox {{ color: rgb({color.red()},{color.green()},{color.blue()}); }}")
            cb.toggled.connect(lambda checked, metric_key=key: self._on_chart_metric_toggled(metric_key, checked))
            layout.addWidget(cb)
            boxes[key] = cb
            if cb.isChecked():
                visible.append(key)
        layout.addStretch(1)
        chart.set_metric_defs(metric_defs)
        chart.set_visible_keys(visible)
        self._update_chart_metric_value_labels()

    def _chart_metric_text(self, key: str, label: str) -> str:
        value = self.__dict__.setdefault('_chart_current_values', {}).get(key)
        if value is None:
            return f"{label}: -"
        return f"{label}: {MetricHistoryChart._compact(float(value))}"

    def _update_chart_metric_value_labels(self):
        for key, cb in self.__dict__.setdefault('_chart_metric_checkboxes', {}).items():
            label = cb.property("metric_label") or cb.text().split(":", 1)[0]
            cb.setText(self._chart_metric_text(key, str(label)))

    def _on_chart_metric_toggled(self, key: str, checked: bool):
        if checked:
            self._hidden_chart_metrics.discard(key)
        else:
            self._hidden_chart_metrics.add(key)
        visible = [k for k, _label, _color in self._current_chart_metric_defs() if k not in self._hidden_chart_metrics]
        self.metrics_chart.set_visible_keys(visible)

    def _set_chart_sample_seconds(self, seconds: int):
        seconds = max(1, int(seconds))
        self.params.set('metrics_chart_sample_seconds', seconds, validate=False)
        interval = max(1, int(seconds)) * 1000
        state = self.__dict__
        timer = state.get('_metrics_chart_timer')
        if timer is not None:
            timer.setInterval(interval)
        group = state.get('_chart_sample_group')
        if group is not None:
            btn = group.button(seconds)
            if btn is not None and not btn.isChecked():
                btn.blockSignals(True)
                btn.setChecked(True)
                btn.blockSignals(False)
        for action_seconds, action in state.get('_chart_sample_actions', {}).items():
            action.blockSignals(True)
            action.setChecked(int(action_seconds) == seconds)
            action.blockSignals(False)
        self._schedule_ui_params_save()

    def _on_chart_window_changed(self):
        value = self._chart_window_combo.currentData()
        self.metrics_chart.set_time_window_seconds(None if int(value) < 0 else int(value))
        self._schedule_ui_params_save()

    def _build_selected_agent_panel(self) -> QWidget:
        shell = QWidget()
        shell.setObjectName("agent_details_shell")
        shell_layout = QHBoxLayout(shell)
        shell_layout.setContentsMargins(0, 0, 0, 0)
        shell_layout.setSpacing(0)

        self._agent_panel_collapsed = False
        self._agent_panel_toggle = QPushButton(">")
        self._agent_panel_toggle.setFixedWidth(18)
        self._agent_panel_toggle.setToolTip("Recolher ou abrir o painel do agente.")
        self._agent_panel_toggle.setStyleSheet(
            "QPushButton { background:#20262e; color:#dce7f3; border:1px solid #303741; "
            "border-right:0; border-radius:7px; padding:0; } "
            "QPushButton:hover { background:#2d3946; }"
        )
        self._agent_panel_toggle.clicked.connect(self._toggle_selected_agent_panel)
        shell_layout.addWidget(self._agent_panel_toggle, stretch=0)

        panel = QWidget()
        panel.setObjectName("agent_details_panel")
        panel.setStyleSheet(
            "#agent_details_panel { background:#171a1f; border-left:1px solid #303741; } "
            "#agent_details_panel QLabel { color:#dce7f3; } "
            "#agent_details_panel QTabWidget::pane { border:1px solid #303741; background:#14181e; } "
            "#agent_details_panel QTabBar::tab { background:#20262e; color:#cbd8e6; padding:7px 12px; } "
            "#agent_details_panel QTabBar::tab:selected { background:#315b80; color:#f4f8ff; }"
        )
        self._agent_details_content = panel
        shell_layout.addWidget(panel, stretch=0)

        panel_layout = QVBoxLayout(panel)
        panel_layout.setContentsMargins(8, 8, 8, 8)
        panel_layout.setSpacing(7)
        title = QLabel("Agente selecionado")
        title.setStyleSheet("font-weight:700; font-size:14px; color:#f0f5ff;")
        panel_layout.addWidget(title)

        self._agent_detail_tabs = QTabWidget()
        self._agent_detail_tabs.currentChanged.connect(self._on_selected_agent_tab_changed)
        panel_layout.addWidget(self._agent_detail_tabs, stretch=1)

        genome_scroll = QScrollArea()
        genome_scroll.setWidgetResizable(True)
        genome_scroll.setFrameShape(QFrame.Shape.NoFrame)
        genome_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        genome_host = QWidget()
        genome_layout = QVBoxLayout(genome_host)
        genome_layout.setContentsMargins(7, 7, 7, 7)
        genome_layout.setSpacing(7)
        genome_scroll.setWidget(genome_host)
        self._agent_detail_tabs.addTab(genome_scroll, "Genoma")

        self._agent_portrait = QLabel()
        self._agent_portrait.setFixedHeight(126)
        self._agent_portrait.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._agent_portrait.setStyleSheet("background:#0f1217; border:1px solid #303741; border-radius:6px;")
        genome_layout.addWidget(self._agent_portrait)
        self._agent_detail_labels: Dict[str, QLabel] = {}

        def add_detail_group(group_title: str, rows: list[tuple[str, str]]):
            box = QGroupBox(group_title)
            box.setStyleSheet(self._card_style())
            grid = QGridLayout(box)
            grid.setContentsMargins(8, 10, 8, 8)
            grid.setHorizontalSpacing(7)
            grid.setVerticalSpacing(3)
            for row_idx, (key, caption) in enumerate(rows):
                left = QLabel(f"{caption}:")
                left.setStyleSheet("color:#9fb1c4;")
                right = QLabel("-")
                right.setWordWrap(True)
                right.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
                grid.addWidget(left, row_idx, 0, alignment=Qt.AlignmentFlag.AlignTop)
                grid.addWidget(right, row_idx, 1)
                self._agent_detail_labels[key] = right
            grid.setColumnStretch(1, 1)
            genome_layout.addWidget(box)

        add_detail_group("Identidade", [
            ('species', 'Tipo'),
            ('brain_type', 'Rede neural'),
            ('selected_count', 'Selecionados'),
            ('age', 'Idade'),
            ('position', 'Posicao'),
        ])
        add_detail_group("Corpo & Movimento", [
            ('body', 'Corpo'),
            ('speed', 'Velocidade'),
            ('locomotion', 'Motor'),
        ])
        add_detail_group("Visao", [
            ('sensor', 'Retina'),
        ])
        add_detail_group("Energia & Dieta", [
            ('energy', 'Energia'),
            ('metabolism', 'Metabolismo'),
            ('diet', 'Dieta'),
            ('smart_local', 'Fator intel. local'),
            ('intake', 'Alimentacao'),
        ])
        genome_layout.addStretch(1)

        neural_page = QWidget()
        neural_layout = QVBoxLayout(neural_page)
        neural_layout.setContentsMargins(0, 0, 0, 0)
        self._agent_neural_view = AgentNeuralNetworkView(self.params.get('neural_view_dense_layout', 'fixed'))
        neural_layout.addWidget(self._agent_neural_view, stretch=1)
        self._agent_detail_tabs.addTab(neural_page, "Rede Neural")

        shell.hide()
        self._refresh_selected_agent_panel_width()
        self._agent_panel_timer = QTimer(self)
        self._agent_panel_timer.timeout.connect(self._update_selected_agent_panel)
        self._agent_panel_timer.start(350)
        return shell

    def _toggle_selected_agent_panel(self):
        self._agent_panel_collapsed = not bool(getattr(self, '_agent_panel_collapsed', False))
        self._refresh_selected_agent_panel_width()

    def _on_selected_agent_tab_changed(self, _index: int):
        self._refresh_selected_agent_panel_width()
        if getattr(self, '_agent_detail_tabs', None) is not None and self._agent_detail_tabs.currentIndex() == 1:
            self.params.set('disable_brain_activations', False, validate=False)
            viewer = getattr(self, '_agent_neural_view', None)
            if viewer is not None:
                viewer.set_dense_layout(self.params.get('neural_view_dense_layout', 'fixed'))
                viewer.set_agent(getattr(self.engine, 'selected_agent', None))

    def _refresh_selected_agent_panel_width(self):
        content = getattr(self, '_agent_details_content', None)
        toggle = getattr(self, '_agent_panel_toggle', None)
        if content is None or toggle is None:
            return
        shell = toggle.parentWidget()
        if bool(getattr(self, '_agent_panel_collapsed', False)):
            content.hide()
            toggle.setText("<")
            if shell is not None:
                shell.setFixedWidth(toggle.width())
            return
        content.show()
        toggle.setText(">")
        neural_tab = getattr(self, '_agent_detail_tabs', None) is not None and self._agent_detail_tabs.currentIndex() == 1
        if neural_tab:
            content_width = max(620, min(980, int(max(1280, self.width()) * 0.48)))
        else:
            content_width = 326
        content.setFixedWidth(content_width)
        if shell is not None:
            shell.setFixedWidth(toggle.width() + content_width)

    @staticmethod
    def _short_float(value: Any, digits: int = 2) -> str:
        try:
            return f"{float(value):.{digits}f}"
        except Exception:
            return "-"

    @staticmethod
    def _compact_float(value: Any, digits: int = 2) -> str:
        try:
            number = float(value)
        except Exception:
            return "-"
        sign = "-" if number < 0 else ""
        number = abs(number)
        for suffix, scale in (("M", 1_000_000.0), ("k", 1_000.0)):
            if number >= scale:
                return f"{sign}{number / scale:.{digits}f}{suffix}"
        return f"{sign}{number:.{digits}f}"

    def _flatten_numeric_values(self, values: Any) -> list[float]:
        if hasattr(values, 'tolist'):
            values = values.tolist()
        if isinstance(values, (int, float)):
            return [float(values)]
        if not isinstance(values, (list, tuple)):
            return []
        flat: list[float] = []
        for item in values:
            flat.extend(self._flatten_numeric_values(item))
        return flat

    def _draw_selected_agent_portrait(self, agent: Any):
        pix = QPixmap(300, 140)
        pix.fill(QColor(15, 18, 23))
        painter = QPainter(pix)
        try:
            painter.setRenderHint(QPainter.RenderHint.Antialiasing, True)
            color = getattr(agent, 'color', (220, 220, 220)) or (220, 220, 220)
            try:
                qcolor = QColor(int(color[0]), int(color[1]), int(color[2]))
            except Exception:
                qcolor = QColor(220, 220, 220)
            center_x, center_y = 150, 70
            radius = max(16, min(48, int(float(getattr(agent, 'r', 10.0)) * 2.6)))
            painter.setPen(QPen(QColor(230, 238, 248), 2))
            painter.setBrush(QBrush(qcolor))
            if getattr(agent, 'body_shape', getattr(getattr(agent, 'locomotion', None), 'body_shape', 'ellipse')) == 'circle':
                painter.drawEllipse(center_x - radius, center_y - radius, radius * 2, radius * 2)
            else:
                painter.save()
                painter.translate(center_x, center_y)
                painter.rotate(math.degrees(float(getattr(agent, 'angle', 0.0))))
                painter.drawEllipse(-radius, int(-radius * 0.5), radius * 2, radius)
                painter.restore()
            angle = float(getattr(agent, 'angle', 0.0))
            painter.setPen(QPen(QColor(255, 255, 255), 3))
            painter.drawLine(center_x, center_y, int(center_x + math.cos(angle) * radius), int(center_y + math.sin(angle) * radius))
            painter.setBrush(QBrush(QColor(0, 0, 0)))
            sensor = getattr(agent, 'sensor', None)
            eyes = 2 if sensor is not None and int(getattr(sensor, 'eye_count', 1) or 1) >= 2 else 1
            offsets = [0.0]
            if eyes == 2:
                sep = math.radians(float(getattr(sensor, 'eye_separation_degrees', 45.0)) * 0.5)
                offsets = [-sep, sep]
            for offset in offsets:
                ex = center_x + math.cos(angle + offset) * radius
                ey = center_y + math.sin(angle + offset) * radius
                painter.drawEllipse(int(ex - 4), int(ey - 4), 8, 8)
            painter.setPen(QPen(QColor(93, 174, 255), 1))
            vision = getattr(getattr(agent, 'sensor', None), 'vision_radius', None)
            if vision is not None:
                painter.drawText(10, 130, f"visao {self._short_float(vision, 0)}")
        finally:
            painter.end()
        self._agent_portrait.setPixmap(pix)

    def _get_intelligence_metrics(self, agent: Any, max_age_s: float = 1.0) -> Dict[str, float]:
        now = time.monotonic()
        if (
            agent is self._intelligence_cache_agent
            and self._intelligence_cache
            and (now - self._intelligence_cache_time) < max_age_s
        ):
            return self._intelligence_cache
        if agent is None:
            metrics = {}
        else:
            try:
                from .intelligence import local_intelligence_for_agent
                metrics = local_intelligence_for_agent(self.engine, agent)
            except Exception:
                metrics = {}
        self._intelligence_cache_agent = agent
        self._intelligence_cache = metrics
        self._intelligence_cache_time = now
        return metrics

    def _reset_metrics_history(self):
        state = self.__dict__
        history = state.setdefault('_metrics_history', [])
        history.clear()
        state.setdefault('_chart_current_values', {}).clear()
        state.setdefault('_chart_intake_cache', {}).clear()
        state.setdefault('_chart_group_smart_ema', {}).clear()
        chart = state.get('metrics_chart')
        if chart is not None:
            chart.set_history(history)
        self._update_chart_metric_value_labels()

    def _cleanup_chart_intake_cache(self):
        state = self.__dict__
        cache = state.setdefault('_chart_intake_cache', {})
        if not cache:
            return
        engine = state.get('engine')
        live = set(getattr(engine, 'all_agents', []) or [])
        for label_id, label_cache in list(cache.items()):
            if not isinstance(label_cache, dict):
                cache.pop(label_id, None)
                continue
            for agent in list(label_cache.keys()):
                if agent not in live:
                    label_cache.pop(agent, None)

    def _group_chart_intelligence_value(self, label_id: int, agents, now_t: float, sample_size: int = 96) -> float:
        state = self.__dict__
        engine = state.get('engine')
        ema = state.setdefault('_chart_group_smart_ema', {})
        cache = state.setdefault('_chart_intake_cache', {})
        previous = float(ema.get(label_id, 0.0) or 0.0)
        pool = [agent for agent in agents if agent in getattr(engine, 'all_agents', [])]
        if not pool:
            ema[label_id] = 0.0
            cache.pop(label_id, None)
            return 0.0
        try:
            from .intelligence import opportunity_group_intelligence_value
        except Exception:
            return previous
        label_cache = cache.setdefault(label_id, {})
        metrics = opportunity_group_intelligence_value(
            engine,
            pool,
            label_cache,
            now_t,
            previous_score=previous,
            sample_size=sample_size,
            alpha=0.08,
        )
        value = float(metrics.get('smart_factor', previous) or 0.0)
        ema[label_id] = value
        return value

    def _update_metrics_chart(self, force: bool = False):
        state = self.__dict__
        chart = state.get('metrics_chart')
        if chart is None:
            return
        engine = state.get('engine')
        if engine is None:
            return
        label_ids = tuple(sorted(getattr(engine, 'agent_labels', {}).keys()))
        if label_ids != state.get('_chart_known_label_ids', ()):
            self._chart_known_label_ids = label_ids
            self._refresh_chart_metric_checkboxes()
        if bool(self.params.get('paused', False)) or not bool(getattr(engine, 'running', False)):
            return
        self._cleanup_chart_intake_cache()
        now_t = float(getattr(engine, 'total_simulation_time', 0.0))
        row = {
            't': now_t,
            'organisms': float(len(getattr(engine, 'all_agents', []))),
            'foods': float(len(engine.entities.get('foods', []))),
            'effective_time_scale': float(getattr(engine, 'effective_time_scale', 0.0) or 0.0),
        }
        if getattr(engine, 'agent_labels', None):
            try:
                for label_id, meta in engine.agent_labels.items():
                    if not bool(meta.get('show_chart', True)):
                        continue
                    agents = engine.get_agents_by_label(label_id)
                    row[f'label_{label_id}_count'] = float(len(agents))
                    row[f'label_{label_id}_smart'] = self._group_chart_intelligence_value(label_id, agents, now_t)
            except Exception:
                pass
        history = state.setdefault('_metrics_history', [])
        history.append(row)
        current_values = state.setdefault('_chart_current_values', {})
        for key, _label, _color in self._current_chart_metric_defs():
            if key in row:
                current_values[key] = float(row[key])
        chart.set_history(history)
        self._update_chart_metric_value_labels()

    def _set_metrics_chart_visible(self, checked: bool):
        self.params.set('show_metrics_chart', bool(checked), validate=False)
        state = self.__dict__
        panel = state.get('metrics_panel')
        if panel is not None:
            panel.setVisible(bool(checked))
            if checked:
                self._refresh_chart_metric_checkboxes()
                chart = state.get('metrics_chart')
                if chart is not None:
                    chart.set_history(state.setdefault('_metrics_history', []))
        self._schedule_ui_params_save()

    def _update_selected_agent_panel(self):
        panel = getattr(self, 'agent_details_panel', None)
        if panel is None:
            return
        agent = getattr(self.engine, 'selected_agent', None)
        visible = bool(agent is not None and self.params.get('show_selected_details', True))
        panel.setVisible(visible)
        if not visible:
            viewer = getattr(self, '_agent_neural_view', None)
            if viewer is not None:
                viewer.set_agent(None)
            return

        self._refresh_selected_agent_panel_width()
        self._draw_selected_agent_portrait(agent)
        selected_count = len(getattr(self.engine, 'selected_agents', set()) or [])
        speed = math.hypot(float(getattr(agent, 'vx', 0.0)), float(getattr(agent, 'vy', 0.0)))
        brain = getattr(agent, 'brain', None)
        sensor = getattr(agent, 'sensor', None)
        locomotion = getattr(agent, 'locomotion', None)
        energy_model = getattr(agent, 'energy_model', None)

        labels = self._agent_detail_labels
        labels['species'].setText("Organismo legado" if getattr(agent, 'is_predator', False) else "Organismo")
        labels['brain_type'].setText(str(getattr(brain, 'display_name', getattr(brain, 'brain_type', 'MLP padrao')) if brain is not None else "-"))
        labels['selected_count'].setText(str(selected_count or 1))
        labels['energy'].setText(self._short_float(getattr(agent, 'energy', 0.0), 2))
        labels['speed'].setText(self._short_float(speed, 2))
        labels['age'].setText(self._short_float(getattr(agent, 'age', 0.0), 1))
        labels['position'].setText(f"x {self._short_float(getattr(agent, 'x', 0.0), 1)} / y {self._short_float(getattr(agent, 'y', 0.0), 1)}")
        labels['body'].setText(
            f"r {self._short_float(getattr(agent, 'r', 0.0), 1)}, "
            f"{getattr(agent, 'body_shape', getattr(locomotion, 'body_shape', 'ellipse'))}"
        )
        if sensor is not None:
            try:
                from .sensors import retina_channels_description, retina_input_mode_from_channels, retina_input_mode_label
                channels = tuple(getattr(sensor, 'channels', ('d',)) or ('d',))
                vision_type = retina_input_mode_label(retina_input_mode_from_channels(channels))
                channel_text = retina_channels_description(channels)
            except Exception:
                channels = tuple(getattr(sensor, 'channels', ('d',)) or ('d',))
                vision_type = "-"
                channel_text = ''.join(str(c).upper() for c in channels)
            retina_count = int(getattr(sensor, 'retina_count', 0) or 0)
            eye_count = int(getattr(sensor, 'eye_count', 1) or 1)
            total_inputs = retina_count * eye_count * max(1, len(channels))
            mapping_mode = str(self.params.get('retina_vision_mode', 'single'))
            visible_parts = []
            if bool(getattr(sensor, 'see_all', False)):
                visible_parts.append("tudo")
            else:
                if bool(getattr(sensor, 'see_food', False)):
                    visible_parts.append("comida")
                if bool(getattr(sensor, 'see_bacteria', False)) or bool(getattr(sensor, 'see_predators', False)):
                    visible_parts.append("organismos")
                if bool(getattr(sensor, 'see_obstacles', False)):
                    visible_parts.append("obstaculos")
            occlusion = "atravessa paredes" if bool(getattr(sensor, 'see_through_walls', True)) else "paredes bloqueiam"
            labels['sensor'].setText(
                f"{vision_type}; mapa {mapping_mode}; canais {channel_text}; "
                f"ve {', '.join(visible_parts) if visible_parts else 'nada'}; {occlusion}; "
                f"{eye_count} olho(s), {retina_count} retinas/olho = {total_inputs} entradas; "
                f"raio {self._short_float(getattr(sensor, 'vision_radius', 0.0), 0)}, "
                f"FOV {self._short_float(getattr(sensor, 'fov_degrees', 0.0), 0)}, "
                f"abertura {self._short_float(getattr(sensor, 'eye_angle_degrees', 0.0), 0)}, "
                f"sep {self._short_float(getattr(sensor, 'eye_separation_degrees', 0.0), 0)}"
            )
        else:
            labels['sensor'].setText("-")
        diet_parts = []
        if bool(getattr(agent, 'diet_food', False)):
            diet_parts.append("comida")
        if bool(getattr(agent, 'diet_agents', False)):
            diet_parts.append("organismos")
        if not diet_parts:
            diet_parts.append("nenhuma")
        labels['diet'].setText(
            f"{' + '.join(diet_parts)}; mesma label {'sim' if bool(getattr(agent, 'diet_same_label', False)) else 'nao'}"
        )
        if locomotion is not None:
            labels['locomotion'].setText(
                f"max {self._short_float(getattr(locomotion, 'max_speed', 0.0), 1)}, "
                f"giro {self._short_float(math.degrees(getattr(locomotion, 'max_turn', 0.0)), 1)} deg/s, "
                f"{getattr(locomotion, 'movement_mode', 'forward')}, "
                f"re {'sim' if bool(getattr(locomotion, 'allow_reverse', False)) else 'nao'}"
            )
        else:
            labels['locomotion'].setText("-")
        if energy_model is not None:
            labels['metabolism'].setText(
                f"v0 {self._short_float(getattr(energy_model, 'v0_cost', 0.0), 3)}, "
                f"vmax {self._short_float(getattr(energy_model, 'vmax_cost', 0.0), 3)}, "
                f"cap {self._short_float(getattr(energy_model, 'energy_cap', 0.0), 1)}"
            )
        else:
            labels['metabolism'].setText("-")
        smart = self._get_intelligence_metrics(agent)
        labels['smart_local'].setText(
            f"{self._compact_float(smart.get('local_factor', 0.0), 2)} "
            f"(dens {self._short_float(smart.get('local_density', 0.0), 5)})"
        )
        labels['intake'].setText(
            f"taxa {self._short_float(smart.get('intake_rate', 0.0), 2)}/s, "
            f"total {self._short_float(smart.get('intake_energy', 0.0), 1)}, "
            f"eventos {int(smart.get('intake_events', 0.0) or 0)}"
        )
        tabs = getattr(self, '_agent_detail_tabs', None)
        viewer = getattr(self, '_agent_neural_view', None)
        if tabs is not None and viewer is not None and tabs.currentIndex() == 1 and not bool(getattr(self, '_agent_panel_collapsed', False)):
            self.params.set('disable_brain_activations', False, validate=False)
            viewer.set_dense_layout(self.params.get('neural_view_dense_layout', 'fixed'))
            viewer.set_agent(agent)

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

        asset_dir = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'Assets'))
        layout.addWidget(QLabel("Selecao"))
        add_tool('select', 'S', 'Selecao unitaria: clique esquerdo seleciona um agente.', os.path.join(asset_dir, 'Selection.png'))
        add_tool('select_square', 'Q', 'Selecao quadrada: arraste uma area para selecionar agentes.', os.path.join(asset_dir, 'Square.png'))
        add_tool('select_lasso', 'L', 'Selecao lasso: desenhe um contorno livre para selecionar agentes.', os.path.join(asset_dir, 'Lasso.png'))
        layout.addSpacing(10)
        layout.addWidget(QLabel("Ferramentas"))
        add_tool('food', 'F', 'Comida: clique esquerdo adiciona comida.')
        add_tool('agent', 'A', 'Agente importado: clique esquerdo insere o agente carregado.')
        add_tool('pipette', '', 'Pipeta: clique em um organismo para copiar todo o agente para o editor genetico.', os.path.join(asset_dir, 'pipette.png'))
        icon_path = os.path.join(asset_dir, 'draw_icon.png')
        add_tool('draw', 'P', 'Pincel: desenha barreiras solidas no substrato.', icon_path=icon_path)
        add_tool('move', 'M', 'Mover: clique e arraste comida ou organismos.')
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
        layout.addStretch(1)

        self._refresh_brush_color_swatch()
        self._set_canvas_tool('food')
        return bar

    def _set_canvas_tool(self, tool: str):
        if hasattr(self.pygame_view, 'active_tool'):
            self.pygame_view.active_tool = tool
        buttons = self.__dict__.get('_canvas_tool_buttons', {})
        for key, btn in buttons.items():
            active = key == tool
            btn.setChecked(active)
            btn.setProperty('active', 'true' if active else 'false')
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    def _update_window_title(self):
        state = object.__getattribute__(self, '__dict__')
        path = state.get('_current_biosim_path')
        save_name = os.path.basename(path) if path else "Sem salvar"
        title = f"{state.get('_app_title', 'AgentBioSim')} - {save_name}"
        try:
            self.setWindowTitle(title)
        except RuntimeError:
            return
        try:
            import pygame
            if pygame.display.get_init():
                pygame.display.set_caption(title)
        except Exception:
            pass

    def _start_prototype_sync_timer(self):
        self._last_applied_prototype_revision = int(getattr(self.engine, '_prototype_revision', 0) or 0)
        self._prototype_sync_timer = QTimer(self)
        self._prototype_sync_timer.timeout.connect(self._sync_editor_from_current_prototype)
        self._prototype_sync_timer.start(500)

    def _sync_editor_from_current_prototype(self):
        revision = int(getattr(self.engine, '_prototype_revision', 0) or 0)
        if revision == getattr(self, '_last_applied_prototype_revision', None):
            return
        name = getattr(self.engine, 'current_agent_prototype', None)
        data = getattr(self.engine, 'loaded_agent_prototypes', {}).get(name)
        if data:
            self._apply_agent_data_to_genetic_editor(data, str(name))
            self._last_applied_prototype_revision = revision

    def _update_brush_width(self, value: float):
        if hasattr(self.pygame_view, 'brush_width'):
            self.pygame_view.brush_width = float(value)
        self._schedule_ui_params_save()

    def _update_brush_erase(self):
        if hasattr(self.pygame_view, 'brush_erase'):
            self.pygame_view.brush_erase = bool(self.widgets['obstacle_brush_erase'].isChecked())
        self._schedule_ui_params_save()

    def _pick_brush_color(self):
        current = getattr(self.pygame_view, 'brush_color', (95, 95, 105))
        col = QColorDialog.getColor(QColor(*current), self, "Cor dos obstaculos")
        if col.isValid():
            self.pygame_view.brush_color = (col.red(), col.green(), col.blue())
            self._refresh_brush_color_swatch()
            self._schedule_ui_params_save()

    def _refresh_brush_color_swatch(self):
        color = getattr(self.pygame_view, 'brush_color', (95, 95, 105))
        self._brush_color_swatch.setStyleSheet(
            f"background: rgb({color[0]},{color[1]},{color[2]}); border: 1px solid #777; border-radius: 4px;"
        )

    def _build_tabs(self):
        """Construct all tabs in a fixed order."""
        self._build_tab_genetic_editor()
        self._build_tab_environment()
        self._build_tab_labels()

    def _build_menu_bar(self):
        bar = self.menuBar()

        file_menu = bar.addMenu("Arquivo")
        act_new = QAction("Novo", self)
        act_new.triggered.connect(self.new_biosim_project)
        file_menu.addAction(act_new)
        act_open = QAction("Abrir Simulacao", self)
        act_open.triggered.connect(self.open_biosim_window)
        file_menu.addAction(act_open)
        act_save = QAction("Salvar Simulacao", self)
        act_save.triggered.connect(self.save_biosim_window)
        file_menu.addAction(act_save)
        act_save_as = QAction("Salvar Como", self)
        act_save_as.triggered.connect(self.save_biosim_as_window)
        file_menu.addAction(act_save_as)
        file_menu.addSeparator()
        act_export_sub = QAction("Exportar substrato JSON", self)
        act_export_sub.triggered.connect(self.open_export_substrate_window)
        file_menu.addAction(act_export_sub)
        act_import_sub = QAction("Importar substrato JSON", self)
        act_import_sub.triggered.connect(self.open_import_substrate_window)
        file_menu.addAction(act_import_sub)

        view_menu = bar.addMenu("View")
        self._add_bool_menu_action(view_menu, "Renderizacao simples", 'simple_render')
        self._add_bool_menu_action(view_menu, "Detalhes do agente selecionado", 'show_selected_details')
        act_brain = QAction("Mostrar ativacoes neurais", self)
        act_brain.setCheckable(True)
        act_brain.setChecked(not self.params.get('disable_brain_activations', False))
        act_brain.toggled.connect(self._on_toggle_brain_activations)
        view_menu.addAction(act_brain)
        self._add_bool_menu_action(view_menu, "Grafico de metricas", 'show_metrics_chart', callback=self._set_metrics_chart_visible)
        self._add_bool_menu_action(view_menu, "Mostrar visao dos organismos", 'bacteria_show_vision')
        self._add_bool_menu_action(view_menu, "Mostrar visao de organismos legados", 'predator_show_vision')
        self._add_bool_menu_action(view_menu, "Ver spatial hash", 'show_spatial_hash')
        self._add_bool_menu_action(view_menu, "Rastrear agente selecionado", 'camera_follow_selected_agent')
        neural_layout_menu = view_menu.addMenu("Layout da rede neural")
        self._build_neural_layout_menu(neural_layout_menu)

        pref_menu = bar.addMenu("Preferencias")
        self._add_bool_menu_action(pref_menu, "Renderizar", 'render_enabled')
        pref_menu.addSeparator()
        act_sim_options = QAction("Opcoes de simulacao", self)
        act_sim_options.triggered.connect(self.open_simulation_options_window)
        pref_menu.addAction(act_sim_options)
        act_autosave = QAction("Autosave", self)
        act_autosave.triggered.connect(self.open_autosave_window)
        pref_menu.addAction(act_autosave)
        pref_menu.addSeparator()
        act_appearance = QAction("Aparencia do ambiente", self)
        act_appearance.triggered.connect(self.open_environment_appearance_window)
        pref_menu.addAction(act_appearance)
        act_vision_system = QAction("Sistema de Visao", self)
        act_vision_system.triggered.connect(self.open_vision_system_window)
        pref_menu.addAction(act_vision_system)
        act_neural_networks = QAction("Redes neurais", self)
        act_neural_networks.triggered.connect(self.open_neural_network_window)
        pref_menu.addAction(act_neural_networks)
        chart_menu = pref_menu.addMenu("Grafico")
        self._build_chart_sampling_menu(chart_menu)
        render_menu = pref_menu.addMenu("Resolucao da renderizacao")
        self._build_render_resolution_menu(render_menu)
        new_changes_menu = pref_menu.addMenu("Novas mudancas")
        act_perf_changes = QAction("Percepcao, cerebro e escala", self)
        act_perf_changes.triggered.connect(self.open_new_changes_window)
        new_changes_menu.addAction(act_perf_changes)

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

    def _build_chart_sampling_menu(self, menu):
        group = QActionGroup(self)
        group.setExclusive(True)
        self._chart_sample_action_group = group
        self._chart_sample_actions = {}
        current = int(self.params.get('metrics_chart_sample_seconds', 5) or 5)
        for label, seconds in (("1s", 1), ("5s", 5), ("30s", 30), ("1min", 60), ("10min", 600)):
            action = QAction(label, self)
            action.setCheckable(True)
            action.setChecked(int(seconds) == current)
            action.triggered.connect(lambda _checked=False, s=seconds: self._set_chart_sample_seconds(s))
            group.addAction(action)
            menu.addAction(action)
            self._chart_sample_actions[int(seconds)] = action

    def _build_neural_layout_menu(self, menu):
        group = QActionGroup(self)
        group.setExclusive(True)
        current = str(self.params.get('neural_view_dense_layout', 'fixed'))
        self._neural_layout_actions = {}
        for key, label in (('fixed', 'Espacamento fixo'), ('spread', 'Preencher painel')):
            action = QAction(label, self)
            action.setCheckable(True)
            action.setChecked(key == current)
            action.triggered.connect(lambda _checked=False, mode=key: self._set_neural_layout_mode(mode))
            group.addAction(action)
            menu.addAction(action)
            self._neural_layout_actions[key] = action
        self._neural_layout_group = group

    def _set_neural_layout_mode(self, mode: str):
        mode = 'spread' if str(mode) == 'spread' else 'fixed'
        self.params.set('neural_view_dense_layout', mode, validate=False)
        for key, action in getattr(self, '_neural_layout_actions', {}).items():
            action.blockSignals(True)
            action.setChecked(key == mode)
            action.blockSignals(False)
        viewer = getattr(self, '_agent_neural_view', None)
        if viewer is not None:
            viewer.set_dense_layout(mode)
            viewer.set_agent(getattr(self.engine, 'selected_agent', None))
        self._schedule_ui_params_save()

    def _build_render_resolution_menu(self, menu):
        group = QActionGroup(self)
        group.setExclusive(True)
        self._render_resolution_action_group = group
        self._render_resolution_actions = {}
        current = float(self.params.get('render_resolution_scale', 1.0) or 1.0)
        options = (
            ("Normal 1x", 1.0, "Renderizacao nativa, mais leve."),
            ("Alta 1.5x", 1.5, "Desenha em 1.5x e reduz para suavizar bordas."),
            ("Muito alta 2x", 2.0, "Mais suave, com custo maior de renderizacao."),
            ("Ultra 3x", 3.0, "Maxima nitidez visual; pode pesar bastante."),
        )
        for label, scale, tip in options:
            action = QAction(label, self)
            action.setCheckable(True)
            action.setChecked(abs(float(scale) - current) < 1e-6)
            action.setToolTip(tip)
            action.triggered.connect(lambda _checked=False, s=scale: self._set_render_resolution_scale(s))
            group.addAction(action)
            menu.addAction(action)
            self._render_resolution_actions[float(scale)] = action

    def _set_render_resolution_scale(self, scale: float):
        try:
            scale = max(1.0, min(3.0, float(scale)))
        except (TypeError, ValueError):
            scale = 1.0
        self.params.set('render_resolution_scale', scale, validate=True)
        if hasattr(self.pygame_view, 'set_render_scale'):
            self.pygame_view.set_render_scale(scale)
        for action_scale, action in self.__dict__.get('_render_resolution_actions', {}).items():
            action.blockSignals(True)
            action.setChecked(abs(float(action_scale) - scale) < 1e-6)
            action.blockSignals(False)
        self._schedule_ui_params_save()

    def _color_from_param(self, name: str, fallback=(10, 10, 20)) -> QColor:
        value = self.params.get(name, fallback)
        try:
            r, g, b = [int(max(0, min(255, float(c)))) for c in list(value)[:3]]
            return QColor(r, g, b)
        except Exception:
            return QColor(*fallback)

    def _set_color_swatch(self, swatch: QLabel, color: QColor):
        swatch.setStyleSheet(
            f"background: rgb({color.red()},{color.green()},{color.blue()}); "
            "border:1px solid #66717f; border-radius:4px;"
        )

    def _make_color_picker_row(self, label: str, param_name: str, fallback=(10, 10, 20)):
        row = QHBoxLayout()
        row.addWidget(QLabel(label))
        swatch = QLabel()
        swatch.setFixedSize(34, 24)
        color = self._color_from_param(param_name, fallback)
        self._set_color_swatch(swatch, color)
        btn = QPushButton("Cor")
        btn.setFixedWidth(70)

        def pick():
            current = self._color_from_param(param_name, fallback)
            chosen = QColorDialog.getColor(current, self, label)
            if not chosen.isValid():
                return
            value = (chosen.red(), chosen.green(), chosen.blue())
            self.params.set(param_name, value, validate=False)
            if param_name == 'background_color_top':
                self.params.set('substrate_bg_color', value, validate=False)
            self._set_color_swatch(swatch, chosen)
            self._schedule_ui_params_save()

        btn.clicked.connect(pick)
        row.addWidget(swatch)
        row.addWidget(btn)
        row.addStretch(1)
        return row

    def open_environment_appearance_window(self):
        existing = getattr(self, '_appearance_dialog', None)
        if existing is not None and existing.isVisible():
            existing.raise_()
            existing.activateWindow()
            return

        dlg = QDialog(self)
        self._appearance_dialog = dlg
        dlg.setWindowTitle("Aparencia do ambiente")
        dlg.setMinimumWidth(420)
        dlg.setStyleSheet(
            "QDialog { background:#171a1f; color:#dce7f3; } "
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:24px; padding:8px; } "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; "
            "margin-left:10px; padding:2px 8px; background:#262b31; color:#cfe1f5; } "
            "QLabel, QCheckBox { color:#dce7f3; }"
        )
        layout = QVBoxLayout(dlg)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(8)

        bg_box = QGroupBox("Background")
        bg_layout = QVBoxLayout(bg_box)
        cb_bg_grad = QCheckBox("Usar gradiente vertical")
        cb_bg_grad.setChecked(bool(self.params.get('background_gradient_enabled', False)))
        cb_bg_grad.toggled.connect(lambda checked: self._set_param_and_schedule('background_gradient_enabled', bool(checked), validate=False))
        bg_layout.addWidget(cb_bg_grad)
        bg_layout.addLayout(self._make_color_picker_row("Cor superior / solida:", 'background_color_top', (10, 10, 20)))
        bg_layout.addLayout(self._make_color_picker_row("Cor inferior:", 'background_color_bottom', (10, 10, 20)))
        layout.addWidget(bg_box)

        sub_box = QGroupBox("Substrato")
        sub_layout = QVBoxLayout(sub_box)
        cb_sub_grad = QCheckBox("Usar gradiente vertical")
        cb_sub_grad.setChecked(bool(self.params.get('substrate_gradient_enabled', False)))
        cb_sub_grad.toggled.connect(lambda checked: self._set_param_and_schedule('substrate_gradient_enabled', bool(checked), validate=False))
        sub_layout.addWidget(cb_sub_grad)
        sub_layout.addLayout(self._make_color_picker_row("Cor superior / solida:", 'substrate_color_top', (10, 10, 20)))
        sub_layout.addLayout(self._make_color_picker_row("Cor inferior:", 'substrate_color_bottom', (10, 10, 20)))
        layout.addWidget(sub_box)

        border_box = QGroupBox("Borda do substrato")
        border_layout = QVBoxLayout(border_box)
        cb_border = QCheckBox("Mostrar borda")
        cb_border.setChecked(bool(self.params.get('substrate_border_enabled', True)))
        cb_border.toggled.connect(lambda checked: self._set_param_and_schedule('substrate_border_enabled', bool(checked), validate=False))
        border_layout.addWidget(cb_border)
        border_layout.addLayout(self._make_color_picker_row("Cor da borda:", 'substrate_border_color', (40, 200, 40)))
        layout.addWidget(border_box)

        hint = QLabel("As mudancas sao aplicadas enquanto esta janela fica aberta. Desligue os gradientes para o modo mais leve.")
        hint.setWordWrap(True)
        hint.setStyleSheet("color:#9fb1c4;")
        layout.addWidget(hint)

        close_btn = QPushButton("Fechar")
        close_btn.clicked.connect(dlg.close)
        layout.addWidget(close_btn, alignment=Qt.AlignmentFlag.AlignRight)
        dlg.show()

    def _preferences_dialog_style(self) -> str:
        return (
            "QDialog { background:#171a1f; color:#dce7f3; } "
            "QGroupBox { border:1px solid #4a4f58; border-radius:8px; margin-top:24px; padding:8px; } "
            "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; "
            "margin-left:10px; padding:2px 8px; background:#262b31; color:#cfe1f5; } "
            "QLabel, QCheckBox { color:#dce7f3; }"
        )

    def _make_preferences_dialog(self, attr_name: str, title: str, minimum_width: int = 520) -> tuple[QDialog, QVBoxLayout] | None:
        existing = getattr(self, attr_name, None)
        if existing is not None and existing.isVisible():
            existing.raise_()
            existing.activateWindow()
            return None

        dlg = QDialog(self)
        setattr(self, attr_name, dlg)
        dlg.setWindowTitle(title)
        dlg.setMinimumWidth(minimum_width)
        dlg.setStyleSheet(self._preferences_dialog_style())

        root = QVBoxLayout(dlg)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(8)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        root.addWidget(scroll)
        content = QWidget()
        scroll.setWidget(content)
        layout = QVBoxLayout(content)
        layout.setContentsMargins(2, 2, 2, 12)
        layout.setSpacing(14)
        return dlg, layout

    def open_simulation_options_window(self):
        created = self._make_preferences_dialog('_simulation_options_dialog', "Opcoes de simulacao", 560)
        if created is None:
            return
        dlg, layout = created
        card_style = self._card_style()

        g_time = QGroupBox("Tempo & Execucao")
        g_time.setStyleSheet(card_style)
        grid = QGridLayout(g_time)
        row = 0
        w = _spin_int(1, 240); w.setValue(_param_int(self.params, 'fps', 60)); row = self._add_grid_param(grid, row, "FPS:", 'fps', w)
        w = _spin_int(5, 1000); w.setValue(_param_int(self.params, 'physics_steps_per_second', 30)); row = self._add_grid_param(grid, row, "Fisica fixa (Hz):", 'physics_steps_per_second', w)
        w = _spin_int(1, 1000); w.setValue(_param_int(self.params, 'max_physics_steps_per_frame', 8)); row = self._add_grid_param(grid, row, "Substeps max/frame:", 'max_physics_steps_per_frame', w)
        w = _spin_double(0.0, 60.0, 0.25, 2); w.setValue(self.params.get('max_physics_backlog_seconds', 0.25)); row = self._add_grid_param(grid, row, "Atraso max fisico (s):", 'max_physics_backlog_seconds', w)
        layout.addWidget(g_time)

        g_perf = QGroupBox("Performance & Determinismo")
        g_perf.setStyleSheet(card_style)
        grid = QGridLayout(g_perf)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('render_enabled', True)); row = self._add_grid_param(grid, row, "Renderizar:", 'render_enabled', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get('use_spatial', True)); row = self._add_grid_param(grid, row, "Spatial Hash:", 'use_spatial', cb)
        w = _spin_int(0, 10); w.setValue(_param_int(self.params, 'retina_skip', 0)); row = self._add_grid_param(grid, row, "Retina skip:", 'retina_skip', w)
        w = _spin_int(-1, 2147483647); w.setValue(_param_int(self.params, 'random_seed', -1)); row = self._add_grid_param(grid, row, "Seed RNG (-1 aleatoria):", 'random_seed', w)
        cb = QCheckBox(); cb.setChecked(self.params.get('reuse_spatial_grid', True)); row = self._add_grid_param(grid, row, "Reutilizar grid espacial:", 'reuse_spatial_grid', cb)
        layout.addWidget(g_perf)

        g_motion = QGroupBox("Fisica de movimento")
        g_motion.setStyleSheet(card_style)
        grid = QGridLayout(g_motion)
        row = 0
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('smooth_locomotion_enabled', False))); row = self._add_grid_param(grid, row, "Locomocao suave:", 'smooth_locomotion_enabled', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('smooth_linear_inertia_enabled', True))); row = self._add_grid_param(grid, row, "Inercia linear:", 'smooth_linear_inertia_enabled', cb)
        w = _spin_double(0.0, 50000.0, 25.0, 1); w.setValue(self.params.get('smooth_max_linear_accel', 900.0)); row = self._add_grid_param(grid, row, "Aceleracao linear max:", 'smooth_max_linear_accel', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('smooth_linear_drag_enabled', True))); row = self._add_grid_param(grid, row, "Arrasto linear:", 'smooth_linear_drag_enabled', cb)
        w = _spin_double(0.0, 50.0, 0.05, 3); w.setValue(self.params.get('smooth_linear_drag', 0.75)); row = self._add_grid_param(grid, row, "Fator arrasto linear:", 'smooth_linear_drag', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('smooth_angular_inertia_enabled', True))); row = self._add_grid_param(grid, row, "Inercia angular:", 'smooth_angular_inertia_enabled', cb)
        w = _spin_double(0.0, 500.0, 0.25, 3); w.setValue(self.params.get('smooth_max_angular_accel', math.pi * 4.0)); row = self._add_grid_param(grid, row, "Acel. angular max (rad/s2):", 'smooth_max_angular_accel', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('smooth_angular_drag_enabled', True))); row = self._add_grid_param(grid, row, "Arrasto angular:", 'smooth_angular_drag_enabled', cb)
        w = _spin_double(0.0, 50.0, 0.05, 3); w.setValue(self.params.get('smooth_angular_drag', 1.5)); row = self._add_grid_param(grid, row, "Fator arrasto angular:", 'smooth_angular_drag', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('render_interpolation_enabled', False))); row = self._add_grid_param(grid, row, "Interpolar render:", 'render_interpolation_enabled', cb)
        layout.addWidget(g_motion)

        g_actions = QGroupBox("Acoes")
        g_actions.setStyleSheet(card_style)
        actions = QHBoxLayout(g_actions)
        btn_apply = QPushButton("Aplicar opcoes")
        btn_apply.clicked.connect(self.apply_simulation_params)
        actions.addWidget(btn_apply)
        layout.addWidget(g_actions)
        layout.addStretch(1)

        dlg.show()

    def open_vision_system_window(self):
        created = self._make_preferences_dialog('_vision_system_dialog', "Sistema de Visao", 620)
        if created is None:
            return
        dlg, layout = created
        card_style = self._card_style()

        g_mode = QGroupBox("Modo principal")
        g_mode.setStyleSheet(card_style)
        grid = QGridLayout(g_mode)
        row = 0
        mode = QComboBox()
        mode.addItem("Raycast rapido - centro do objeto", "single")
        mode.addItem("Raycast atual - corpo completo", "fullbody")
        mode.addItem("Bins angulares", "sector")
        current_mode = str(self.params.get('retina_vision_mode', 'single'))
        idx = mode.findData(current_mode)
        if idx < 0:
            idx = 0
        mode.setCurrentIndex(idx)
        row = self._add_grid_param(grid, row, "Modo de visao:", 'retina_vision_mode', mode)
        layout.addWidget(g_mode)

        g_bins = QGroupBox("Controles dos bins angulares")
        g_bins.setStyleSheet(card_style)
        bins_grid = QGridLayout(g_bins)
        bins_row = 0

        combo = QComboBox()
        combo.addItem("Mais proximo", "nearest")
        combo.addItem("Mais forte", "strongest")
        combo.addItem("Soma saturada", "sum_saturating")
        combo.addItem("Media ponderada", "weighted_average")
        idx = combo.findData(str(self.params.get('retina_bins_mode', 'nearest')))
        combo.setCurrentIndex(max(0, idx))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Modo dos bins:", 'retina_bins_mode', combo)

        w = _spin_int(1, 99)
        w.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
        w.setValue(_param_int(self.params, 'retina_bins_distance_subdivisions', 5))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Subdivisoes de distancia:", 'retina_bins_distance_subdivisions', w)

        combo = QComboBox()
        combo.addItem("Linear", "linear")
        combo.addItem("Mais detalhe perto", "near_detail")
        idx = combo.findData(str(self.params.get('retina_bins_distance_distribution', 'near_detail')))
        combo.setCurrentIndex(max(0, idx))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Distribuicao das faixas:", 'retina_bins_distance_distribution', combo)

        combo = QComboBox()
        combo.addItem("Linear", "linear")
        combo.addItem("Quadratica", "quadratic")
        combo.addItem("Degrau", "step")
        combo.addItem("Sem queda", "none")
        idx = combo.findData(str(self.params.get('retina_bins_distance_falloff', 'linear')))
        combo.setCurrentIndex(max(0, idx))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Queda por distancia:", 'retina_bins_distance_falloff', combo)

        combo = QComboBox()
        combo.addItem("Centro apenas", "center")
        combo.addItem("Centro + bordas", "center_edges")
        combo.addItem("Espalhar por tamanho aparente", "apparent_size")
        idx = combo.findData(str(self.params.get('retina_bins_projection', 'center')))
        combo.setCurrentIndex(max(0, idx))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Projecao do objeto:", 'retina_bins_projection', combo)

        w = _spin_int(0, 10000)
        w.setValue(_param_int(self.params, 'retina_bins_candidate_limit', 128))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Limite de candidatos:", 'retina_bins_candidate_limit', w)

        cb = QCheckBox()
        cb.setChecked(bool(self.params.get('retina_bins_obstacles_block_vision', False)))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Obstaculos bloqueiam visao:", 'retina_bins_obstacles_block_vision', cb)

        cb = QCheckBox()
        cb.setChecked(bool(self.params.get('retina_high_scale_auto_sector', False)))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Usar bins automaticamente:", 'retina_high_scale_auto_sector', cb)

        w = _spin_int(1, 200000)
        w.setValue(_param_int(self.params, 'retina_high_scale_sector_min_agents', 800))
        bins_row = self._add_grid_param(bins_grid, bins_row, "Acima de X organismos:", 'retina_high_scale_sector_min_agents', w)
        layout.addWidget(g_bins)

        hint = QLabel("O raio de visao, FOV, numero de retinas, canais e olhos continuam no editor genetico. Aqui voce escolhe apenas o algoritmo que calcula o sinal dessas retinas.")
        hint.setWordWrap(True)
        hint.setStyleSheet("color:#9fb1c4;")
        layout.addWidget(hint)

        def update_enabled():
            enabled = self._get_widget_value('retina_vision_mode') == 'sector'
            for name in [
                'retina_bins_mode',
                'retina_bins_distance_subdivisions',
                'retina_bins_distance_distribution',
                'retina_bins_distance_falloff',
                'retina_bins_projection',
                'retina_bins_candidate_limit',
                'retina_bins_obstacles_block_vision',
            ]:
                widget = self.widgets.get(name)
                if widget is not None:
                    widget.setEnabled(enabled)
        mode.currentIndexChanged.connect(lambda _idx: update_enabled())
        update_enabled()

        actions = QHBoxLayout()
        btn_apply = QPushButton("Aplicar sistema de visao")
        btn_apply.clicked.connect(self.apply_simulation_params)
        actions.addWidget(btn_apply)
        actions.addStretch(1)
        layout.addLayout(actions)
        layout.addStretch(1)
        dlg.show()

    def open_neural_network_window(self):
        created = self._make_preferences_dialog('_neural_network_dialog', "Redes neurais", 640)
        if created is None:
            return
        dlg, layout = created
        card_style = self._card_style()

        g_mode = QGroupBox("Tipo de cerebro")
        g_mode.setStyleSheet(card_style)
        grid = QGridLayout(g_mode)
        row = 0
        combo = QComboBox()
        options = [
            ("MLP padrao", "mlp"),
            ("MLP com gates", "gated_mlp"),
            ("MLP com atalho entrada -> saida", "shortcut_mlp"),
            ("MLP modulada (gates + atalho)", "modulated_mlp"),
            ("RNN simples com memoria curta", "simple_rnn"),
        ]
        for label, value in options:
            combo.addItem(label, value)
        idx = combo.findData(str(self.params.get('neural_network_type', 'mlp')))
        combo.setCurrentIndex(max(0, idx))
        row = self._add_grid_param(grid, row, "Rede neural:", 'neural_network_type', combo)
        layout.addWidget(g_mode)

        stack = QStackedWidget()
        self._neural_network_stack = stack

        def add_panel(title: str, rows: list[tuple[str, str, QWidget]], note: str):
            box = QGroupBox(title)
            box.setStyleSheet(card_style)
            inner = QVBoxLayout(box)
            form = QGridLayout()
            r = 0
            for label, name, widget in rows:
                r = self._add_grid_param(form, r, label, name, widget)
            inner.addLayout(form)
            if note:
                hint = QLabel(note)
                hint.setWordWrap(True)
                hint.setStyleSheet("color:#9fb1c4;")
                inner.addWidget(hint)
            stack.addWidget(box)

        add_panel(
            "MLP padrao",
            [],
            "Rede feedforward atual. Usa as camadas e neuronios definidos no editor genetico e serve como linha de base para comparar os outros cerebros.",
        )

        rows = []
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_init', 1.0)); rows.append(("Gate inicial:", 'neural_gate_init', w))
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_min', 0.0)); rows.append(("Gate minimo:", 'neural_gate_min', w))
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_max', 2.0)); rows.append(("Gate maximo:", 'neural_gate_max', w))
        w = _spin_double(-1.0, 1.0, 0.001, 3); w.setValue(self.params.get('neural_gate_mutation_rate', -1.0)); rows.append(("Taxa mutacao gates (-1 usa geral):", 'neural_gate_mutation_rate', w))
        w = _spin_double(-1.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_gate_mutation_strength', -1.0)); rows.append(("Forca mutacao gates (-1 usa geral):", 'neural_gate_mutation_strength', w))
        add_panel(
            "MLP com gates",
            rows,
            "Cada neuronio oculto ganha um modulador multiplicativo. Isso permite vias quase desligadas ou amplificadas sem mudar a arquitetura retangular.",
        )

        rows = []
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_shortcut_init_std', 0.05)); rows.append(("Inicializacao atalho:", 'neural_shortcut_init_std', w))
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_shortcut_scale', 0.25)); rows.append(("Escala do atalho:", 'neural_shortcut_scale', w))
        w = _spin_double(-1.0, 1.0, 0.001, 3); w.setValue(self.params.get('neural_shortcut_mutation_rate', -1.0)); rows.append(("Taxa mutacao atalho (-1 usa geral):", 'neural_shortcut_mutation_rate', w))
        w = _spin_double(-1.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_shortcut_mutation_strength', -1.0)); rows.append(("Forca mutacao atalho (-1 usa geral):", 'neural_shortcut_mutation_strength', w))
        add_panel(
            "MLP com atalho",
            rows,
            "Adiciona conexoes diretas da entrada para a saida. Funciona como reflexo rapido sem remover a rede profunda.",
        )

        rows = []
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_init', 1.0)); rows.append(("Gate inicial:", 'neural_gate_init', w))
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_min', 0.0)); rows.append(("Gate minimo:", 'neural_gate_min', w))
        w = _spin_double(0.0, 10.0, 0.05, 3); w.setValue(self.params.get('neural_gate_max', 2.0)); rows.append(("Gate maximo:", 'neural_gate_max', w))
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_shortcut_init_std', 0.05)); rows.append(("Inicializacao atalho:", 'neural_shortcut_init_std', w))
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_shortcut_scale', 0.25)); rows.append(("Escala do atalho:", 'neural_shortcut_scale', w))
        add_panel(
            "MLP modulada",
            rows,
            "Combina gates e atalho entrada->saida. E o melhor candidato inicial para mais expressividade mantendo batch e custo controlado.",
        )

        rows = []
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_rnn_recurrent_init_std', 0.08)); rows.append(("Inicializacao recorrente:", 'neural_rnn_recurrent_init_std', w))
        w = _spin_double(0.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_rnn_recurrent_scale', 0.35)); rows.append(("Escala recorrente:", 'neural_rnn_recurrent_scale', w))
        w = _spin_double(0.0, 0.999, 0.01, 3); w.setValue(self.params.get('neural_rnn_memory_decay', 0.6)); rows.append(("Decaimento da memoria:", 'neural_rnn_memory_decay', w))
        w = _spin_double(0.01, 10.0, 0.05, 3); w.setValue(self.params.get('neural_rnn_state_clip', 1.0)); rows.append(("Limite do estado:", 'neural_rnn_state_clip', w))
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('neural_rnn_reset_state_on_copy', True))); rows.append(("Resetar memoria no filho:", 'neural_rnn_reset_state_on_copy', cb))
        w = _spin_double(-1.0, 1.0, 0.001, 3); w.setValue(self.params.get('neural_rnn_mutation_rate', -1.0)); rows.append(("Taxa mutacao recorrente (-1 usa geral):", 'neural_rnn_mutation_rate', w))
        w = _spin_double(-1.0, 10.0, 0.01, 3); w.setValue(self.params.get('neural_rnn_mutation_strength', -1.0)); rows.append(("Forca mutacao recorrente (-1 usa geral):", 'neural_rnn_mutation_strength', w))
        add_panel(
            "RNN simples",
            rows,
            "Usa a primeira camada oculta como memoria curta. O visualizador mostra essa camada de forma simplificada como camada recorrente.",
        )

        def sync_stack():
            index = max(0, combo.currentIndex())
            stack.setCurrentIndex(index)
        combo.currentIndexChanged.connect(lambda _idx: sync_stack())
        sync_stack()
        layout.addWidget(stack)

        actions = QHBoxLayout()
        btn_apply = QPushButton("Aplicar rede neural")
        btn_apply.clicked.connect(self.apply_simulation_params)
        actions.addWidget(btn_apply)
        actions.addStretch(1)
        layout.addLayout(actions)
        layout.addStretch(1)
        dlg.show()

    def open_new_changes_window(self):
        created = self._make_preferences_dialog('_new_changes_dialog', "Novas mudancas", 580)
        if created is None:
            return
        dlg, layout = created
        card_style = self._card_style()

        g_perception = QGroupBox("Percepcao e visao em lote")
        g_perception.setStyleSheet(card_style)
        grid = QGridLayout(g_perception)
        row = 0
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('use_persistent_perception_arrays', False))); row = self._add_grid_param(grid, row, "Buffers persistentes da cena:", 'use_persistent_perception_arrays', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('use_grouped_vision_batches', True))); row = self._add_grid_param(grid, row, "Visao por grupos:", 'use_grouped_vision_batches', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('use_numba_batch_retina', False))); row = self._add_grid_param(grid, row, "Retina Numba em lote:", 'use_numba_batch_retina', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('retina_high_scale_auto_sector', False))); row = self._add_grid_param(grid, row, "Auto visao setorial em escala:", 'retina_high_scale_auto_sector', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('retina_high_scale_global_sector', False))); row = self._add_grid_param(grid, row, "Setor global experimental:", 'retina_high_scale_global_sector', cb)
        w = _spin_int(1, 200000); w.setValue(_param_int(self.params, 'retina_high_scale_sector_min_agents', 800)); row = self._add_grid_param(grid, row, "Min. agentes por grupo:", 'retina_high_scale_sector_min_agents', w)
        layout.addWidget(g_perception)

        g_compiled = QGroupBox("Cerebro e movimento compilados")
        g_compiled.setStyleSheet(card_style)
        grid = QGridLayout(g_compiled)
        row = 0
        cb = QCheckBox(); cb.setChecked(not bool(self.params.get('brain_cache_disable', False))); cb.toggled.connect(lambda checked: self.params.set('brain_cache_disable', not bool(checked), validate=False)); row = self._add_grid_param(grid, row, "Cache de pesos por grupo:", 'enable_brain_group_cache', cb)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('use_numba_brain_forward', False))); row = self._add_grid_param(grid, row, "Forward do cerebro em Numba:", 'use_numba_brain_forward', cb)
        w = _spin_int(1, 200000); w.setValue(_param_int(self.params, 'numba_brain_forward_min_batch', 256)); row = self._add_grid_param(grid, row, "Min. lote Numba cerebro:", 'numba_brain_forward_min_batch', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get('use_numba_locomotion_energy', False))); row = self._add_grid_param(grid, row, "Numba locomocao/energia:", 'use_numba_locomotion_energy', cb)
        layout.addWidget(g_compiled)

        hint = QLabel("Visao setorial automatica muda a aproximacao sensorial apenas quando habilitada. Os demais controles mantem o contrato atual e atacam preparacao de arrays, lotes e cache numerico.")
        hint.setWordWrap(True)
        hint.setStyleSheet("color:#9fb1c4;")
        layout.addWidget(hint)

        actions = QHBoxLayout()
        btn_apply = QPushButton("Aplicar novas mudancas")
        btn_apply.clicked.connect(self.apply_simulation_params)
        actions.addWidget(btn_apply)
        actions.addStretch(1)
        layout.addLayout(actions)
        layout.addStretch(1)
        dlg.show()

    def open_autosave_window(self):
        created = self._make_preferences_dialog('_autosave_dialog', "Autosave", 520)
        if created is None:
            return
        dlg, layout = created
        card_style = self._card_style()

        g_auto = QGroupBox("Autosave de simulacao")
        g_auto.setStyleSheet(card_style)
        grid = QGridLayout(g_auto)
        row = 0
        cb = QCheckBox()
        cb.setChecked(bool(self.params.get('auto_export_substrate', False)))
        cb.toggled.connect(self._on_toggle_auto_export)
        row = self._add_grid_param(grid, row, "Ativar autosave:", 'auto_export_substrate', cb)
        w = _spin_double(0.1, 1440.0, 0.5, 2)
        w.setValue(self.params.get('auto_export_interval_minutes', 10.0))
        row = self._add_grid_param(grid, row, "Intervalo autosave (min):", 'auto_export_interval_minutes', w)
        cb = QCheckBox(); cb.setChecked(self.params.get('export_substrate_include_brain_activations', False)); row = self._add_grid_param(grid, row, "Salvar ativacoes neurais:", 'export_substrate_include_brain_activations', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get('export_substrate_pretty_json', False)); row = self._add_grid_param(grid, row, "JSON legivel manual:", 'export_substrate_pretty_json', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get('debug_tracebacks', False)); row = self._add_grid_param(grid, row, "Tracebacks no debug:", 'debug_tracebacks', cb)
        def _on_interval_changed(_):
            self.params.set('auto_export_interval_minutes', self._get_widget_value('auto_export_interval_minutes'), validate=False)
            self._schedule_ui_params_save()
            if self._is_autosave_enabled():
                self._reschedule_auto_export()
        w.valueChanged.connect(_on_interval_changed)
        layout.addWidget(g_auto)

        hint = QLabel("O autosave salva um projeto .biosim completo: agentes, comidas, obstaculos, camera, parametros e estado da simulacao.")
        hint.setWordWrap(True)
        hint.setStyleSheet("color:#9fb1c4;")
        layout.addWidget(hint)

        actions = QHBoxLayout()
        btn_apply = QPushButton("Aplicar autosave")
        btn_apply.clicked.connect(self.apply_autosave_params)
        actions.addWidget(btn_apply)
        layout.addLayout(actions)
        layout.addStretch(1)

        dlg.show()

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
        self._schedule_ui_params_save()

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
        self._schedule_ui_params_save()

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
            idx = -1
            for i in range(w.count()):
                if w.itemData(i) == value:
                    idx = i
                    break
            if idx < 0:
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
            data = w.currentData()
            if data is not None:
                return data
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
        log_exception(prefix, type(exc), exc, exc.__traceback__)
        print(f"{prefix}: {self._format_exception(exc)}")

    def _warn_exception(self, title: str, exc: BaseException):
        QMessageBox.warning(self, title, self._format_exception(exc))

    def _help_label(self, label: str, name: str) -> ClickHelpLabel:
        return ClickHelpLabel(label, self._param_help_text(name, label))

    def _param_help_text(self, name: str, label: str) -> str:
        help_by_name = {
            'time_scale': 'Multiplica a velocidade do tempo simulado. Se o computador nao acompanhar, a velocidade efetiva cai, mas o passo fisico continua fixo.',
            'fps': 'Limite de quadros por segundo da janela. Afeta fluidez visual e quanto tempo de CPU a interface tenta usar.',
            'show_metrics_chart': 'Mostra um grafico leve com populacao, comida, tempo efetivo e inteligencia media dos grupos com label.',
            'physics_steps_per_second': 'Resolucao fixa da fisica em substeps por segundo simulado. Valores maiores aumentam precisao temporal, mas custam CPU proporcionalmente.',
            'max_physics_steps_per_frame': 'Quantidade maxima de substeps fisicos antes de renderizar outro frame. Se bater no limite, o excedente e descartado para evitar travamento e manter dt fixo.',
            'max_physics_backlog_seconds': 'Atraso simulado maximo acumulado apos travamentos. Excesso e descartado para evitar congelamento longo; isso reduz velocidade efetiva, sem aumentar dt fisico.',
            'paused': 'Pausa ou retoma o avanco da simulacao sem apagar agentes, comida ou obstaculos.',
            'population_min_rescue_enabled': 'Quando ativo, impede que a simulacao mate individuos abaixo do minimo configurado para aquela populacao.',
            'use_spatial': 'Usa uma grade espacial para acelerar buscas de proximidade, colisao, alimentacao e visao em populacoes grandes.',
            'retina_skip': 'Quantidade de frames que cada retina pode reutilizar a leitura anterior. Aumentar melhora desempenho, mas reduz precisao temporal da percepcao.',
            'random_seed': 'Seed do gerador aleatorio. Use -1 para aleatorio; use um numero fixo para repetir experimentos com o mesmo ponto de partida.',
            'retina_vision_mode': 'Modo de mapeamento da retina. single usa centro do objeto; fullbody considera o corpo inteiro por raycast; sector usa bins angulares configuraveis.',
            'retina_bins_mode': 'Define como um setor visual resume varios objetos ao mesmo tempo. Mais proximo usa o objeto mais perto; Mais forte usa o sinal dominante; Soma saturada soma ate 1; Media ponderada suaviza os sinais do setor.',
            'retina_bins_distance_subdivisions': 'Divide cada setor angular em faixas internas de distancia para calcular melhor a intensidade final. Nao muda o numero de entradas da rede neural.',
            'retina_bins_distance_distribution': 'Controla o tamanho das faixas internas. Linear usa faixas iguais; Mais detalhe perto usa faixas menores perto do organismo e maiores longe.',
            'retina_bins_distance_falloff': 'Define como a intensidade visual cai com a distancia. Linear preserva o comportamento simples; Quadratica prioriza perto; Degrau discretiza; Sem queda usa presenca/cor.',
            'retina_bins_projection': 'Define quantos setores um objeto ativa. Centro apenas usa o centro; Centro + bordas considera as bordas angulares; Tamanho aparente espalha objetos grandes ou proximos.',
            'retina_bins_candidate_limit': 'Limita quantos objetos proximos cada organismo avalia no modo bins. Zero significa ilimitado. Valores baixos evitam travamentos, mas podem ignorar objetos relevantes.',
            'retina_bins_obstacles_block_vision': 'No modo bins, obstaculos podem bloquear objetos atras deles. E mais realista, mas custa mais processamento.',
            'neural_network_type': 'Escolhe o tipo de cerebro usado ao criar ou recriar redes neurais. A arquitetura basica continua vindo do editor genetico.',
            'neural_gate_init': 'Valor inicial dos gates. 1.0 preserva a intensidade normal; perto de 0 silencia neuronios; acima de 1 amplifica.',
            'neural_gate_min': 'Limite inferior dos gates apos mutacoes.',
            'neural_gate_max': 'Limite superior dos gates apos mutacoes.',
            'neural_gate_mutation_rate': 'Taxa de mutacao dos gates. Use -1 para reutilizar a taxa geral do editor genetico.',
            'neural_gate_mutation_strength': 'Forca da mutacao dos gates. Use -1 para reutilizar a forca geral do editor genetico.',
            'neural_shortcut_init_std': 'Escala inicial dos pesos do atalho direto entrada->saida.',
            'neural_shortcut_scale': 'Multiplicador global do atalho entrada->saida. Valores baixos criam reflexos sem dominar a rede profunda.',
            'neural_shortcut_mutation_rate': 'Taxa de mutacao dos pesos do atalho. Use -1 para reutilizar a taxa geral.',
            'neural_shortcut_mutation_strength': 'Forca de mutacao dos pesos do atalho. Use -1 para reutilizar a forca geral.',
            'neural_rnn_recurrent_init_std': 'Escala inicial dos pesos recorrentes da RNN simples.',
            'neural_rnn_recurrent_scale': 'Quanto o estado anterior influencia a primeira camada oculta.',
            'neural_rnn_memory_decay': 'Quanto da memoria anterior permanece a cada passo. Maior = memoria mais lenta.',
            'neural_rnn_state_clip': 'Limite numerico do estado interno da RNN para evitar explosao.',
            'neural_rnn_reset_state_on_copy': 'Quando um filho nasce, zera a memoria momentanea em vez de herdar o estado instantaneo do pai.',
            'neural_rnn_mutation_rate': 'Taxa de mutacao dos pesos recorrentes. Use -1 para reutilizar a taxa geral.',
            'neural_rnn_mutation_strength': 'Forca de mutacao dos pesos recorrentes. Use -1 para reutilizar a forca geral.',
            'render_enabled': 'Liga ou desliga o desenho da simulacao no Pygame. Desligado, a simulacao continua evoluindo, mas a tela nao e redesenhada.',
            'simple_render': 'Troca para renderizacao mais simples e rapida. Use para populacoes grandes ou benchmarks visuais.',
            'show_spatial_hash': 'Desenha a grade de celulas do spatial hash sobre o substrato. Desligado nao adiciona custo de renderizacao.',
            'use_numba_kernels': 'Ativa kernels numericos por arrays/Numba quando disponiveis. Mantem fallback seguro para o caminho antigo.',
            'use_numba_batch_retina': 'Processa retinas compativeis no mesmo kernel Numba para reduzir chamadas por agente.',
            'use_grouped_vision_batches': 'Agrupa organismos com a mesma configuracao de retina antes de preparar candidatos e executar kernels.',
            'use_persistent_perception_arrays': 'Reusa buffers numericos da cena gerados junto com o spatial hash para reduzir leitura repetida de objetos Python na visao.',
            'retina_high_scale_auto_sector': 'Troca automaticamente grupos grandes para visao setorial quando habilitado. Custa menos, mas usa aproximacao sensorial diferente.',
            'retina_high_scale_global_sector': 'No modo setorial, usa um snapshot global compacto do grupo visivel em vez de montar vizinhanca por olho. E experimental.',
            'retina_high_scale_sector_min_agents': 'Tamanho minimo do grupo visual para ativar a visao setorial automatica.',
            'use_numba_brain_forward': 'Usa kernels Numba no forward de grupos grandes de cerebros com arquitetura compativel.',
            'numba_brain_forward_min_batch': 'Quantidade minima de cerebros no grupo antes de usar o forward Numba.',
            'enable_brain_group_cache': 'Reusa pilhas de pesos de grupos estaveis entre passos. Economiza empilhamento repetido e consome memoria extra.',
            'use_numba_locomotion_energy': 'Experimental: aplica Numba tambem na locomocao e energia. Desligado por padrao porque pode ser mais lento em alguns perfis.',
            'reuse_spatial_grid': 'Reutiliza a estrutura da grade espacial entre frames quando possivel, reduzindo alocacoes.',
            'agents_inertia': 'Controla suavizacao da velocidade. 1 aplica o comando neural imediatamente; valores maiores deixam movimento mais inercial.',
            'smooth_locomotion_enabled': 'Liga o atuador fisico suave. Desligado preserva a locomocao direta usada em experimentos antigos.',
            'smooth_linear_inertia_enabled': 'Limita a mudanca de velocidade linear para o organismo acelerar e frear gradualmente.',
            'smooth_max_linear_accel': 'Aceleracao linear maxima usada quando a inercia linear esta ativa.',
            'smooth_linear_drag_enabled': 'Aplica arrasto viscoso na velocidade linear do organismo.',
            'smooth_linear_drag': 'Intensidade do arrasto linear por segundo. Valores maiores freiam mais rapido.',
            'smooth_angular_inertia_enabled': 'Usa velocidade angular para a rotacao entrar e sair gradualmente.',
            'smooth_max_angular_accel': 'Aceleracao angular maxima em radianos por segundo ao quadrado.',
            'smooth_angular_drag_enabled': 'Aplica arrasto viscoso na velocidade de rotacao.',
            'smooth_angular_drag': 'Intensidade do arrasto angular por segundo. Valores maiores reduzem giros persistentes.',
            'render_interpolation_enabled': 'Interpola a pose visual entre substeps fisicos. Melhora fluidez visual e nao muda a dinamica simulada.',
            'allow_reverse_locomotion': 'Permite que a saida neural gere movimento para tras. Desligado preserva a locomocao historica apenas para frente.',
            'reproduction_min_age': 'Idade minima para um agente poder reproduzir. Ajuda a evitar reproducao imediata de recem-nascidos.',
            'reproduction_cooldown': 'Tempo minimo entre duas reproducoes do mesmo agente.',
            'show_selected_details': 'Mostra o painel lateral do agente selecionado: energia, idade, velocidade, retinas e rede neural.',
            'camera_follow_selected_agent': 'Quando existe um unico agente selecionado, a camera acompanha esse organismo mantendo o zoom e a posicao de tela escolhida por pan.',
            'enable_brain_activations': 'Habilita calculo e exibicao das ativacoes neurais do agente selecionado. E util para diagnostico, mas tem custo extra.',
            'debug_tracebacks': 'Mostra tracebacks completos em erros da UI. Use para depurar; desligado deixa mensagens mais curtas.',
            'auto_export_substrate': 'Ativa autosave de projeto .biosim completo em intervalos regulares.',
            'auto_export_interval_minutes': 'Intervalo, em minutos, entre autosaves da simulacao completa.',
            'export_substrate_include_brain_activations': 'Inclui ativacoes neurais no save. Aumenta o arquivo e o custo de salvamento.',
            'export_substrate_pretty_json': 'Salva JSON com indentacao legivel. Facilita inspecao humana, mas gera arquivos maiores.',
            'food_target': 'Quantidade alvo de comida. O controlador tenta repor comida ate aproximar esse valor.',
            'food_mode': 'Define como a comida entrega energia: instantanea ou em pedacos solidos consumidos aos poucos.',
            'food_bite_seconds': 'No modo pedacos, define quanto tempo de contato leva para consumir uma particula.',
            'food_piece_particle_radius': 'Raio de cada particula solida que forma um aglomerado de comida.',
            'food_piece_cluster_radius': 'Raio aproximado do aglomerado que nasce no modo pedacos.',
            'food_piece_particle_spacing': 'Distancia entre os centros das particulas do aglomerado. Maior distancia reduz empilhamento e custo.',
            'food_piece_replenish_mode': 'Define se a comida em pedacos nasce em novos aglomerados, cresce em aglomerados, ou cresce uma particula por vez.',
            'food_min_r': 'Raio minimo da comida nova. Afeta tamanho visual e energia disponivel por item.',
            'food_max_r': 'Raio maximo da comida nova. Tambem influencia energia e espaco ocupado.',
            'food_replenish_interval': 'Intervalo base de reposicao de comida. Valores menores repoe comida mais rapidamente.',
            'food_color': 'Cor usada nas novas comidas e aplicada as comidas existentes quando alterada.',
            'world_w': 'Largura do substrato retangular base.',
            'world_h': 'Altura do substrato retangular base.',
            'substrate_shape': 'Formato fisico do substrato: retangular ou circular.',
            'substrate_radius': 'Raio usado quando o substrato esta no modo circular.',
            'bacteria_count': 'Quantidade de organismos criada ao resetar ou iniciar uma populacao nova.',
            'bacteria_min_limit': 'Limite legado por tipo. Agora o minimo vivo deve ser definido por label.',
            'bacteria_max_limit': 'Limite legado por tipo. Agora o maximo vivo deve ser definido por label.',
            'bacteria_initial_energy': 'Energia inicial de organismos novos criados por reset ou spawn padrao.',
            'bacteria_death_energy': 'Energia abaixo da qual o organismo vira candidato a morrer.',
            'bacteria_age_death_enabled': 'Quando ativo, o organismo morre ao atingir a idade configurada. Essa regra faz parte do genoma/metabolismo.',
            'bacteria_death_age': 'Idade maxima em segundos simulados para morte por idade, quando a opcao esta habilitada.',
            'bacteria_corpse_to_food': 'Quando ativo, um organismo morto por idade ou energia deixa um ponto de comida no substrato.',
            'bacteria_split_energy': 'Energia minima para o organismo poder se dividir.',
            'bacteria_metab_v0_cost': 'Custo energetico por segundo quando o organismo esta parado.',
            'bacteria_metab_vmax_cost': 'Custo energetico por segundo quando o organismo se move perto da velocidade maxima.',
            'bacteria_energy_cap': 'Energia maxima que um organismo consegue armazenar.',
            'bacteria_body_size': 'Raio corporal do organismo. Afeta colisao, renderizacao, area ocupada e posicionamento.',
            'bacteria_vision_radius': 'Distancia maxima que a retina do organismo consegue perceber.',
            'bacteria_retina_count': 'Numero de raios/sensores da retina do organismo. Mais retinas aumentam resolucao e custo.',
            'bacteria_retina_fov_degrees': 'Campo angular total de visao do organismo, em graus.',
            'bacteria_max_speed': 'Velocidade maxima que a locomocao do organismo pode atingir.',
            'bacteria_max_turn_deg': 'Velocidade maxima de rotacao do organismo em graus por segundo.',
            'bacteria_hidden_layers': 'Quantidade de camadas ocultas no cerebro neural dos novos organismos. Alterar vivos pode recriar cerebros.',
            'bacteria_mutation_rate': 'Probabilidade de cada peso neural sofrer mutacao na reproducao.',
            'bacteria_mutation_strength': 'Intensidade/desvio das mutacoes numericas aplicadas aos pesos neurais.',
            'bacteria_show_vision': 'Desenha os raios de visao dos organismos quando habilitado.',
            'bacteria_retina_see_food': 'Define se a retina do organismo detecta comida.',
            'bacteria_retina_see_bacteria': 'Define se a retina do organismo detecta outros organismos.',
            'bacteria_retina_see_predators': 'Define se a retina detecta organismos legados criados como predadores.',
            'bacteria_retina_see_obstacles': 'Define se a retina do organismo detecta obstaculos desenhados.',
            'bacteria_retina_see_all': 'Detecta comida, organismos e obstaculos como coisas coloridas no substrato.',
            'bacteria_retina_see_through_walls': 'Quando desligado, obstaculos bloqueiam a visao do que esta atras.',
            'bacteria_retina_input_mode': 'Define se a retina usa apenas distancia, cor ponderada pela distancia, cor e distancia separadas, ou apenas cor.',
            'bacteria_retina_channel_d': 'Canal interno de distancia/proximidade. Agora e controlado pelo tipo de entrada da retina.',
            'bacteria_retina_channel_r': 'Canal vermelho disponivel para a retina quando o modo de visao usa cor.',
            'bacteria_retina_channel_g': 'Canal verde disponivel para a retina quando o modo de visao usa cor.',
            'bacteria_retina_channel_b': 'Canal azul disponivel para a retina quando o modo de visao usa cor.',
            'bacteria_diet_food': 'Permite que o organismo ganhe energia ao tocar comida.',
            'bacteria_diet_agents': 'Permite que o organismo ganhe energia ao tocar e consumir outros organismos.',
            'bacteria_diet_same_label': 'Quando ativo, organismos podem consumir outros da mesma label.',
            'bacteria_diet_food_efficiency': 'Multiplicador da energia recebida ao comer comida.',
            'bacteria_diet_agent_efficiency': 'Multiplicador da energia recebida ao consumir outro organismo.',
            'predators_enabled': 'Liga ou desliga criacao inicial e presenca configurada de predadores.',
            'predator_count': 'Quantidade de predadores criada ao resetar ou iniciar populacao nova.',
            'predator_min_limit': 'Numero minimo de predadores que o sistema tenta preservar.',
            'predator_max_limit': 'Limite maximo de predadores vivos permitido pela reproducao.',
            'predator_initial_energy': 'Energia inicial de predadores novos criados por reset ou spawn padrao.',
            'predator_death_energy': 'Energia abaixo da qual o predador vira candidato a morrer.',
            'predator_age_death_enabled': 'Quando ativo, o organismo legado morre ao atingir a idade configurada.',
            'predator_death_age': 'Idade maxima em segundos simulados para morte por idade de organismos legados.',
            'predator_corpse_to_food': 'Quando ativo, um organismo legado morto deixa comida no substrato.',
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
            'predator_retina_see_obstacles': 'Define se a retina do predador detecta obstaculos desenhados.',
            'predator_retina_see_all': 'Detecta comida, organismos e obstaculos como coisas coloridas no substrato.',
            'predator_retina_see_through_walls': 'Quando desligado, obstaculos bloqueiam a visao do que esta atras.',
            'obstacle_brush_width': 'Largura do pincel usado para desenhar ou apagar obstaculos solidos.',
            'obstacle_brush_erase': 'Quando ativo, o pincel apaga obstaculos em vez de desenhar novos.',
        }
        if name in help_by_name:
            return help_by_name[name]
        if name.startswith('bacteria_neurons_layer_'):
            layer = name.rsplit('_', 1)[-1]
            return f"Numero de neuronios na camada oculta {layer} dos organismos. Mudar isso em agentes vivos pode recriar o cerebro e apagar pesos atuais."
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
        if getattr(self, '_live_param_signals_ready', False):
            self._prepare_param_widget_runtime(name, widget)
        grid.addWidget(self._help_label(label, name), row, 0)
        grid.addWidget(widget, row, 1)
        return row + 1

    def _agent_group_apply_buttons(self, species: str, group: str) -> QWidget:
        wrap = QWidget()
        row = QHBoxLayout(wrap)
        row.setContentsMargins(0, 8, 0, 0)
        row.setSpacing(8)
        btn_all = QPushButton("Aplicar a todos")
        btn_selected = QPushButton("Aplicar aos selecionados")
        btn_all.clicked.connect(lambda _checked=False, s=species, g=group: self.apply_agent_param_group(s, g, 'all_alive'))
        btn_selected.clicked.connect(lambda _checked=False, s=species, g=group: self.apply_agent_param_group(s, g, 'selected'))
        row.addWidget(btn_all)
        row.addWidget(btn_selected)
        return wrap

    def _build_tab_genetic_editor(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Editor Genetico")
        outer = QVBoxLayout(tab)
        outer.setContentsMargins(4, 4, 4, 4)
        outer.setSpacing(6)

        header = ClickHelpLabel(
            "Template: Organismo",
            "Este editor define o genoma base dos novos organismos. O papel ecologico agora vem da dieta: comer comida, comer organismos ou ambos."
        )
        outer.addWidget(header)
        name_row = QHBoxLayout()
        name_row.addWidget(ClickHelpLabel("Nome do agente:", "Nome do template genetico usado para novos organismos e exports de agente. Pressione Enter para aplicar."))
        name_edit = QLineEdit(str(self.params.get('agent_template_name', 'organismo_1')))
        name_edit.setPlaceholderText("organismo_1")
        name_edit.returnPressed.connect(lambda: self._update_param_real_time('agent_template_name'))
        self.widgets['agent_template_name'] = name_edit
        name_row.addWidget(name_edit, stretch=1)
        outer.addLayout(name_row)
        outer.addWidget(self._build_genetic_page('bacteria'), stretch=1)

    def _retina_input_mode_options(self):
        from .sensors import (
            RETINA_INPUT_MODE_COLOR_DISTANCE,
            RETINA_INPUT_MODE_COLOR_ONLY,
            RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE,
            RETINA_INPUT_MODE_DISTANCE_ONLY,
        )
        return [
            ("Distancia apenas", RETINA_INPUT_MODE_DISTANCE_ONLY),
            ("Cor ponderada pela distancia", RETINA_INPUT_MODE_COLOR_DISTANCE),
            ("Cor + distancia separadas", RETINA_INPUT_MODE_COLOR_PLUS_DISTANCE),
            ("Cor apenas", RETINA_INPUT_MODE_COLOR_ONLY),
        ]

    @staticmethod
    def _set_combo_data(combo: QComboBox, value: Any):
        for index in range(combo.count()):
            if combo.itemData(index) == value:
                combo.setCurrentIndex(index)
                return
        idx = combo.findText(str(value))
        if idx >= 0:
            combo.setCurrentIndex(idx)

    def _make_retina_mode_combo(self, species: str) -> QComboBox:
        from .sensors import normalize_retina_input_mode
        combo = QComboBox()
        for label, value in self._retina_input_mode_options():
            combo.addItem(label, value)
        current = normalize_retina_input_mode(self.params.get(f'{species}_retina_input_mode', 'distance_only'))
        self._set_combo_data(combo, current)
        combo.currentIndexChanged.connect(lambda _idx, s=species: self._on_retina_channel_ui_changed(s))
        return combo

    def _make_movement_mode_combo(self, species: str) -> QComboBox:
        from .actuators import MOVEMENT_MODE_FORWARD, MOVEMENT_MODE_OMNI, normalize_movement_mode
        combo = QComboBox()
        combo.addItem("Frente + rotacao", MOVEMENT_MODE_FORWARD)
        combo.addItem("4 direcoes + rotacao", MOVEMENT_MODE_OMNI)
        self._set_combo_data(combo, normalize_movement_mode(self.params.get(f'{species}_movement_mode', MOVEMENT_MODE_FORWARD)))
        return combo

    def _make_body_shape_combo(self, species: str) -> QComboBox:
        from .actuators import BODY_SHAPE_CIRCLE, BODY_SHAPE_ELLIPSE, normalize_body_shape
        combo = QComboBox()
        combo.addItem("Elipse", BODY_SHAPE_ELLIPSE)
        combo.addItem("Circulo", BODY_SHAPE_CIRCLE)
        self._set_combo_data(combo, normalize_body_shape(self.params.get(f'{species}_body_shape', BODY_SHAPE_ELLIPSE)))
        return combo

    def _on_retina_channel_ui_changed(self, species: str):
        self._normalize_retina_channel_widgets(species)
        self._update_retina_input_summary(species)
        self._schedule_ui_params_save()

    def _normalize_retina_channel_widgets(self, species: str):
        from .sensors import RETINA_INPUT_MODE_DISTANCE_ONLY, normalize_retina_input_mode
        mode_widget = self.widgets.get(f'{species}_retina_input_mode')
        mode = normalize_retina_input_mode(
            self._get_widget_value(f'{species}_retina_input_mode')
            if mode_widget is not None else self.params.get(f'{species}_retina_input_mode', 'distance_only')
        )
        color_widgets = [self.widgets.get(f'{species}_retina_channel_{ch}') for ch in ('r', 'g', 'b')]
        color_widgets = [w for w in color_widgets if isinstance(w, QCheckBox)]
        uses_color = mode != RETINA_INPUT_MODE_DISTANCE_ONLY
        for widget in color_widgets:
            widget.setEnabled(uses_color)
        if uses_color and color_widgets and not any(widget.isChecked() for widget in color_widgets):
            color_widgets[0].blockSignals(True)
            color_widgets[0].setChecked(True)
            color_widgets[0].blockSignals(False)

    def _collect_retina_params_from_widgets(self, species: str):
        from .sensors import active_retina_channels, normalize_retina_input_mode
        mode_key = f'{species}_retina_input_mode'
        if mode_key in self.widgets:
            self.params.set(mode_key, normalize_retina_input_mode(self._get_widget_value(mode_key)), validate=False)
        for ch in ('r', 'g', 'b'):
            key = f'{species}_retina_channel_{ch}'
            if key in self.widgets:
                self.params.set(key, bool(self._get_widget_value(key)), validate=False)
        channels = active_retina_channels(self.params, species)
        self.params.set(f'{species}_retina_channel_d', 'd' in channels, validate=False)
        if not any(ch in channels for ch in ('r', 'g', 'b', 'rd', 'gd', 'bd')):
            for ch in ('r', 'g', 'b'):
                self.params.set(f'{species}_retina_channel_{ch}', False, validate=False)

    def _update_retina_input_summary(self, species: str):
        label = getattr(self, f'_{species}_retina_summary_label', None)
        if label is None:
            return
        try:
            from .sensors import active_retina_channels, retina_channels_description, retina_input_size
            self._collect_retina_params_from_widgets(species)
            channels = active_retina_channels(self.params, species)
            retina_count = int(self.params.get(f'{species}_retina_count', 18))
            eyes = int(self.params.get(f'{species}_eye_count', 1) or 1)
            per_retina = len(channels)
            total = retina_input_size(self.params, species, retina_count)
            label.setText(
                f"Entradas por retina: {per_retina} ({retina_channels_description(channels)}) | "
                f"total: {eyes} olho(s) x {retina_count} retinas x {per_retina} = {total}"
            )
        except Exception:
            label.setText("Entradas neurais: -")

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
            'reproduction_min_age': 0.0,
            'reproduction_cooldown': 0.0,
            'body_size': 9.0 if is_bacteria else 14.0,
            'vision_radius': 120.0,
            'retina_count': 18,
            'retina_fov_degrees': 180.0,
            'max_speed': 300.0,
            'max_turn': math.pi,
            'hidden_layers': 4 if is_bacteria else 2,
            'mutation_rate': 0.05,
            'mutation_strength': 0.08,
            'eye_count': 1,
            'eye_angle_degrees': 60.0,
            'eye_separation_degrees': 45.0,
        }

        g_energy = QGroupBox("Metabolismo & Energia")
        g_energy.setStyleSheet(card_style)
        grid = QGridLayout(g_energy)
        row = 0
        w = _spin_double(0.0, 200000.0, 1.0, 1); w.setValue(self.params.get(f'{species}_initial_energy', defaults['initial_energy'])); row = self._add_grid_param(grid, row, "Energia inicial:", f'{species}_initial_energy', w)
        w = _spin_double(0.0, 10000.0, 1.0, 1); w.setValue(self.params.get(f'{species}_death_energy', defaults['death_energy'])); row = self._add_grid_param(grid, row, "Energia morte:", f'{species}_death_energy', w)
        w = _spin_double(0.0, 400000.0, 5.0, 1); w.setValue(self.params.get(f'{species}_split_energy', defaults['split_energy'])); row = self._add_grid_param(grid, row, "Energia dividir:", f'{species}_split_energy', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get(f'{species}_age_death_enabled', False))); row = self._add_grid_param(grid, row, "Morte por idade:", f'{species}_age_death_enabled', cb)
        w = _spin_double(0.0, 1_000_000.0, 10.0, 1); w.setValue(self.params.get(f'{species}_death_age', 3600.0)); row = self._add_grid_param(grid, row, "Idade da morte (s):", f'{species}_death_age', w)
        cb = QCheckBox(); cb.setChecked(bool(self.params.get(f'{species}_corpse_to_food', False))); row = self._add_grid_param(grid, row, "Virar comida ao morrer:", f'{species}_corpse_to_food', cb)
        w = _spin_double(0.0, 1_000_000.0, 0.1, 2); w.setValue(self.params.get(f'{species}_reproduction_min_age', defaults['reproduction_min_age'])); row = self._add_grid_param(grid, row, "Idade min. reproducao:", f'{species}_reproduction_min_age', w)
        w = _spin_double(0.0, 1_000_000.0, 0.1, 2); w.setValue(self.params.get(f'{species}_reproduction_cooldown', defaults['reproduction_cooldown'])); row = self._add_grid_param(grid, row, "Cooldown reproducao:", f'{species}_reproduction_cooldown', w)
        w = _spin_double(0.0, 5000.0, 0.01, 2); w.setValue(self.params.get(f'{species}_metab_v0_cost', defaults['metab_v0_cost'])); row = self._add_grid_param(grid, row, "Custo v=0 (s):", f'{species}_metab_v0_cost', w)
        w = _spin_double(0.0, 20000.0, 0.01, 2); w.setValue(self.params.get(f'{species}_metab_vmax_cost', defaults['metab_vmax_cost'])); row = self._add_grid_param(grid, row, "Custo v=vmax (s):", f'{species}_metab_vmax_cost', w)
        w = _spin_double(10.0, 1000000.0, 10.0, 1); w.setValue(self.params.get(f'{species}_energy_cap', defaults['energy_cap'])); row = self._add_grid_param(grid, row, "Cap energia:", f'{species}_energy_cap', w)
        grid.addWidget(self._agent_group_apply_buttons(species, 'energy'), row, 0, 1, 2)
        v.addWidget(g_energy)

        g_body = QGroupBox("Corpo & Movimento")
        g_body.setStyleSheet(card_style)
        grid = QGridLayout(g_body)
        row = 0
        w = _spin_double(1.0, 1000.0, 0.5, 1); w.setValue(self.params.get(f'{species}_body_size', defaults['body_size'])); row = self._add_grid_param(grid, row, "Tamanho corpo (raio):", f'{species}_body_size', w)
        row = self._add_grid_param(grid, row, "Formato corpo:", f'{species}_body_shape', self._make_body_shape_combo(species))
        row = self._add_grid_param(grid, row, "Modo movimento:", f'{species}_movement_mode', self._make_movement_mode_combo(species))
        cb = QCheckBox(); cb.setChecked(bool(self.params.get(f'{species}_allow_reverse_locomotion', False))); row = self._add_grid_param(grid, row, "Permitir marcha re:", f'{species}_allow_reverse_locomotion', cb)
        w = _spin_double(0.0, 10000.0, 10.0, 1); w.setValue(self.params.get(f'{species}_max_speed', defaults['max_speed'])); row = self._add_grid_param(grid, row, "Velocidade max:", f'{species}_max_speed', w)
        w = _spin_double(1.0, 5000.0, 1.0, 1); w.setValue(math.degrees(self.params.get(f'{species}_max_turn', defaults['max_turn']))); row = self._add_grid_param(grid, row, "Rotacao max (graus/s):", f'{species}_max_turn_deg', w)
        grid.addWidget(self._agent_group_apply_buttons(species, 'body_motion'), row, 0, 1, 2)
        v.addWidget(g_body)

        g_vision = QGroupBox("Visao")
        g_vision.setStyleSheet(card_style)
        grid = QGridLayout(g_vision)
        row = 0
        w = _spin_double(1.0, 5000.0, 5.0, 1); w.setValue(self.params.get(f'{species}_vision_radius', defaults['vision_radius'])); row = self._add_grid_param(grid, row, "Raio visao:", f'{species}_vision_radius', w)
        w = _spin_int(1, 128); w.setValue(self.params.get(f'{species}_retina_count', defaults['retina_count'])); w.valueChanged.connect(lambda _v, s=species: self._update_retina_input_summary(s)); row = self._add_grid_param(grid, row, "Numero de retinas:", f'{species}_retina_count', w)
        w = _spin_double(1.0, 360.0, 1.0, 1); w.setValue(self.params.get(f'{species}_retina_fov_degrees', defaults['retina_fov_degrees'])); row = self._add_grid_param(grid, row, "Campo de visao (graus):", f'{species}_retina_fov_degrees', w)
        w = _spin_int(1, 2); w.setValue(self.params.get(f'{species}_eye_count', defaults['eye_count'])); w.valueChanged.connect(lambda _v, s=species: self._update_retina_input_summary(s)); row = self._add_grid_param(grid, row, "Olhos:", f'{species}_eye_count', w)
        w = _spin_double(0.0, 360.0, 1.0, 1); w.setValue(self.params.get(f'{species}_eye_angle_degrees', defaults['eye_angle_degrees'])); row = self._add_grid_param(grid, row, "Abertura entre olhos:", f'{species}_eye_angle_degrees', w)
        w = _spin_double(0.0, 360.0, 1.0, 1); w.setValue(self.params.get(f'{species}_eye_separation_degrees', defaults['eye_separation_degrees'])); row = self._add_grid_param(grid, row, "Separacao no corpo:", f'{species}_eye_separation_degrees', w)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_food', True)); row = self._add_grid_param(grid, row, "Ver comida:", f'{species}_retina_see_food', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_bacteria', False if is_bacteria else True)); row = self._add_grid_param(grid, row, "Ver organismos:", f'{species}_retina_see_bacteria', cb)
        self.params.set(f'{species}_retina_see_predators', bool(self.params.get(f'{species}_retina_see_bacteria', False if is_bacteria else True)), validate=False)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_obstacles', False)); row = self._add_grid_param(grid, row, "Ver obstaculos:", f'{species}_retina_see_obstacles', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_all', False)); row = self._add_grid_param(grid, row, "Ver tudo colorido:", f'{species}_retina_see_all', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_retina_see_through_walls', True)); row = self._add_grid_param(grid, row, "Ver atraves paredes:", f'{species}_retina_see_through_walls', cb)
        mode_combo = self._make_retina_mode_combo(species)
        row = self._add_grid_param(grid, row, "Tipo de entrada:", f'{species}_retina_input_mode', mode_combo)
        grid.addWidget(QLabel("Canais de cor:"), row, 0)
        channel_wrap = QWidget()
        channel_row = QHBoxLayout(channel_wrap)
        channel_row.setContentsMargins(0, 0, 0, 0)
        channel_row.setSpacing(10)
        for suffix, label in (('r', 'R'), ('g', 'G'), ('b', 'B')):
            cbox = QCheckBox(label)
            cbox.setChecked(self.params.get(f'{species}_retina_channel_{suffix}', False))
            cbox.toggled.connect(lambda _checked=False, s=species: self._on_retina_channel_ui_changed(s))
            self.widgets[f'{species}_retina_channel_{suffix}'] = cbox
            channel_row.addWidget(cbox)
        channel_row.addStretch(1)
        grid.addWidget(channel_wrap, row, 1)
        row += 1
        summary = QLabel()
        summary.setWordWrap(True)
        summary.setStyleSheet("color:#9fb1c4;")
        setattr(self, f'_{species}_retina_summary_label', summary)
        grid.addWidget(summary, row, 0, 1, 2)
        row += 1
        grid.addWidget(self._agent_group_apply_buttons(species, 'vision'), row, 0, 1, 2)
        v.addWidget(g_vision)
        self._normalize_retina_channel_widgets(species)
        self._update_retina_input_summary(species)

        g_diet = QGroupBox("Dieta")
        g_diet.setStyleSheet(card_style)
        grid = QGridLayout(g_diet)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_diet_food', True)); row = self._add_grid_param(grid, row, "Come comida:", f'{species}_diet_food', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_diet_agents', False)); row = self._add_grid_param(grid, row, "Come organismos:", f'{species}_diet_agents', cb)
        cb = QCheckBox(); cb.setChecked(self.params.get(f'{species}_diet_same_label', False)); row = self._add_grid_param(grid, row, "Pode comer mesma label:", f'{species}_diet_same_label', cb)
        w = _spin_double(0.0, 10.0, 0.05, 2); w.setValue(self.params.get(f'{species}_diet_food_efficiency', 1.0)); row = self._add_grid_param(grid, row, "Eficiencia comida:", f'{species}_diet_food_efficiency', w)
        w = _spin_double(0.0, 10.0, 0.05, 2); w.setValue(self.params.get(f'{species}_diet_agent_efficiency', 0.7)); row = self._add_grid_param(grid, row, "Eficiencia organismo:", f'{species}_diet_agent_efficiency', w)
        grid.addWidget(self._agent_group_apply_buttons(species, 'diet'), row, 0, 1, 2)
        v.addWidget(g_diet)

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
        grid.addWidget(self._agent_group_apply_buttons(species, 'brain'), row, 0, 1, 2)
        v.addWidget(g_brain)

        hidden_spin = self.widgets[f'{species}_hidden_layers']
        def _update_neuron_enabled():
            layers = int(hidden_spin.value())
            for idx, spin in enumerate(neuron_widgets):
                spin.setEnabled(idx < layers)
        hidden_spin.valueChanged.connect(lambda _v: _update_neuron_enabled())
        _update_neuron_enabled()

        v.addStretch(1)
        return page

    def _add_color_picker(self, layout: QVBoxLayout, species: str, card_style: str):
        title = "Cor dos organismos" if species == 'bacteria' else "Cor dos predadores legados"
        key = f'{species}_color'
        default = (220, 220, 220) if species == 'bacteria' else (80, 120, 220)
        box = QGroupBox(title)
        box.setStyleSheet(card_style)
        box_layout = QVBoxLayout(box)
        row = QHBoxLayout()
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
            self._schedule_ui_params_save()
        button.clicked.connect(_pick)
        row.addWidget(swatch)
        row.addWidget(button)
        box_layout.addLayout(row)
        box_layout.addWidget(self._agent_group_apply_buttons(species, 'color'))
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

        g_b = QGroupBox("Organismos")
        g_b.setStyleSheet(card_style)
        grid = QGridLayout(g_b)
        row = 0
        w = _spin_int(0, 20000); w.setValue(self.params.get('bacteria_count', 150)); row = self._add_grid_param(grid, row, "Quantidade inicial:", 'bacteria_count', w)
        v.addWidget(g_b)

        g_rules = QGroupBox("Regras Populacionais")
        g_rules.setStyleSheet(card_style)
        grid = QGridLayout(g_rules)
        row = 0
        cb = QCheckBox(); cb.setChecked(self.params.get('population_min_rescue_enabled', True)); row = self._add_grid_param(grid, row, "Respeitar minimo das labels:", 'population_min_rescue_enabled', cb)
        note = QLabel("Minimo e maximo agora sao definidos por grupo na aba Labels.")
        note.setWordWrap(True)
        grid.addWidget(note, row, 0, 1, 2)
        row += 1
        v.addWidget(g_rules)
        btn_apply = QPushButton("Aplicar populacao")
        btn_apply.setToolTip("Aplica a quantidade inicial para proximos resets. Limites vivos ficam nas labels.")
        btn_apply.clicked.connect(self.apply_population_params)
        v.addWidget(btn_apply)
        v.addStretch(1)

    def _build_tab_environment(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Substrato")
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
        mode = QComboBox()
        mode.addItem("Instantanea", "instant")
        mode.addItem("Pedacos", "chunk")
        self._set_combo_data(mode, self.params.get('food_mode', 'instant'))
        mode.currentIndexChanged.connect(lambda _idx: self._update_food_mode_controls())
        row = self._add_grid_param(grid, row, "Tipo de comida:", 'food_mode', mode)
        w = _spin_double(0.05, 120.0, 0.1, 2)
        w.setValue(self.params.get('food_bite_seconds', 6.0))
        row = self._add_grid_param(grid, row, "Tempo consumir particula (s):", 'food_bite_seconds', w)
        self._piece_food_widgets = [w]
        w = _spin_double(0.5, 200.0, 0.5, 1)
        w.setValue(self.params.get('food_piece_particle_radius', self.params.get('food_max_r', 5.0)))
        row = self._add_grid_param(grid, row, "Raio da particula:", 'food_piece_particle_radius', w)
        self._piece_food_widgets.append(w)
        w = _spin_double(1.0, 1000.0, 1.0, 1)
        w.setValue(self.params.get('food_piece_cluster_radius', 36.0))
        row = self._add_grid_param(grid, row, "Raio do aglomerado:", 'food_piece_cluster_radius', w)
        self._piece_food_widgets.append(w)
        w = _spin_double(0.5, 300.0, 0.5, 1)
        w.setValue(self.params.get('food_piece_particle_spacing', 11.0))
        row = self._add_grid_param(grid, row, "Distancia particulas:", 'food_piece_particle_spacing', w)
        self._piece_food_widgets.append(w)
        rep = QComboBox()
        rep.addItem("Surgir em pedacos", "spawn_cluster")
        rep.addItem("Crescer em pedacos", "grow_existing")
        rep.addItem("Crescer em particulas", "grow_particles")
        self._set_combo_data(rep, self.params.get('food_piece_replenish_mode', 'spawn_cluster'))
        row = self._add_grid_param(grid, row, "Reposicao pedacos:", 'food_piece_replenish_mode', rep)
        self._piece_food_widgets.append(rep)
        w = _spin_int(0, 10000); w.setValue(self.params.get('food_target', 50)); row = self._add_grid_param(grid, row, "Target comida:", 'food_target', w)
        w = _spin_double(0.1, 100.0, 0.1, 2); w.setValue(self.params.get('food_min_r', 4.5)); row = self._add_grid_param(grid, row, "Comida raio min:", 'food_min_r', w)
        w = _spin_double(0.1, 100.0, 0.1, 2); w.setValue(self.params.get('food_max_r', 5.0)); row = self._add_grid_param(grid, row, "Comida raio max:", 'food_max_r', w)
        w = _spin_double(0.01, 60.0, 0.01, 2); w.setValue(self.params.get('food_replenish_interval', 0.1)); row = self._add_grid_param(grid, row, "Intervalo reposicao (s):", 'food_replenish_interval', w)
        color_wrap = QWidget()
        color_row = QHBoxLayout(color_wrap)
        color_row.setContentsMargins(0, 0, 0, 0)
        self._food_color_swatch = QLabel()
        self._food_color_swatch.setFixedSize(36, 24)
        self._refresh_food_color_swatch()
        color_btn = QPushButton("Escolher cor")
        color_btn.clicked.connect(self._pick_food_color)
        color_row.addWidget(self._food_color_swatch)
        color_row.addWidget(color_btn)
        color_row.addStretch(1)
        grid.addWidget(self._help_label("Cor da comida:", 'food_color'), row, 0)
        grid.addWidget(color_wrap, row, 1)
        row += 1
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

        self._update_food_mode_controls()

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

    def _refresh_food_color_swatch(self):
        swatch = getattr(self, '_food_color_swatch', None)
        if swatch is None:
            return
        color = self.params.get('food_color', (220, 30, 30))
        swatch.setStyleSheet(
            f"background: rgb({color[0]},{color[1]},{color[2]}); border:1px solid #333; border-radius:4px;"
        )

    def _pick_food_color(self):
        current = self.params.get('food_color', (220, 30, 30))
        col = QColorDialog.getColor(QColor(*current), self, "Cor da comida")
        if not col.isValid():
            return
        rgb = (col.red(), col.green(), col.blue())
        self.params.set('food_color', rgb, validate=False)
        for food in self.engine.entities.get('foods', []):
            try:
                food.color = rgb
            except Exception:
                pass
        self._refresh_food_color_swatch()
        self._schedule_ui_params_save()

    def _update_food_mode_controls(self):
        mode = self._get_widget_value('food_mode') if 'food_mode' in self.widgets else self.params.get('food_mode', 'instant')
        for widget in getattr(self, '_piece_food_widgets', []) or []:
            widget.setEnabled(mode == 'chunk')

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
                self._schedule_ui_params_save()
            button.clicked.connect(_pick)
            row.addWidget(swatch)
            row.addWidget(button)
            layout.addWidget(box)

        add_picker("Cor da comida", 'food_color', (220, 30, 30),
                   lambda rgb: [setattr(food, 'color', rgb) for food in self.engine.entities.get('foods', [])])
        # Cores de fundo/substrato ficam na janela Preferencias > Aparencia do ambiente.

    def _build_tab_labels(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Labels")
        layout = QVBoxLayout(tab)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)

        info = QLabel("Use S, selecao quadrada ou lasso para selecionar organismos; depois atribua uma label ao grupo.")
        info.setWordWrap(True)
        layout.addWidget(info)

        self._updating_labels_table = False
        self.labels_table = QTableWidget(0, 5)
        self.labels_table.setHorizontalHeaderLabels(["Grupo", "Individuos", "Min", "Max", "Grafico"])
        self.labels_table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.labels_table.setSelectionMode(QAbstractItemView.SelectionMode.SingleSelection)
        self.labels_table.setEditTriggers(
            QAbstractItemView.EditTrigger.DoubleClicked |
            QAbstractItemView.EditTrigger.SelectedClicked |
            QAbstractItemView.EditTrigger.EditKeyPressed
        )
        self.labels_table.verticalHeader().setVisible(False)
        self.labels_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeMode.Stretch)
        self.labels_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeMode.ResizeToContents)
        self.labels_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeMode.ResizeToContents)
        self.labels_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.ResizeToContents)
        self.labels_table.horizontalHeader().setSectionResizeMode(4, QHeaderView.ResizeMode.ResizeToContents)
        self.labels_table.itemChanged.connect(self._on_label_table_item_changed)
        self.labels_table.cellDoubleClicked.connect(self._on_label_table_double_clicked)
        self.labels_table.currentCellChanged.connect(lambda *_args: self._sync_label_editor_from_selection())
        layout.addWidget(self.labels_table, stretch=1)

        color_row = QHBoxLayout()
        self.label_color_btn = QPushButton("Cor")
        self.label_color_btn.clicked.connect(self._choose_current_label_color)
        color_row.addWidget(self.label_color_btn)
        color_row.addStretch(1)
        layout.addLayout(color_row)

        buttons = QGridLayout()
        actions = [
            ("Atribuir label aos selecionados", self._create_label_from_selection),
            ("Adicionar selecionados a label", self._assign_selection_to_current_label),
            ("Selecionar grupo", self._select_current_label_group),
            ("Remover selecionados da label", self._remove_selection_from_current_label),
            ("Excluir label", self._delete_current_label),
            ("Atualizar lista", self._refresh_labels_list),
        ]
        for idx, (text, slot) in enumerate(actions):
            btn = QPushButton(text)
            btn.clicked.connect(slot)
            buttons.addWidget(btn, idx // 2, idx % 2)
        layout.addLayout(buttons)
        self._refresh_labels_list()

    def _current_label_id(self):
        table = getattr(self, 'labels_table', None)
        if table is None:
            return None
        row = table.currentRow()
        if row < 0:
            return None
        item = table.item(row, 0)
        if item is None:
            return None
        value = item.data(Qt.ItemDataRole.UserRole)
        try:
            return int(value)
        except Exception:
            return None

    def _select_label_row(self, label_id: int):
        table = getattr(self, 'labels_table', None)
        if table is None:
            return False
        for row in range(table.rowCount()):
            item = table.item(row, 0)
            if item is not None and item.data(Qt.ItemDataRole.UserRole) == label_id:
                table.setCurrentCell(row, 0)
                table.selectRow(row)
                return True
        return False

    def _refresh_labels_list(self):
        if 'labels_table' not in self.__dict__:
            return
        current_id = self._current_label_id()
        table = self.labels_table
        self._updating_labels_table = True
        table.blockSignals(True)
        table.setRowCount(0)
        for row, (label_id, meta) in enumerate(sorted(getattr(self.engine, 'agent_labels', {}).items())):
            count = len(self.engine.get_agents_by_label(label_id))
            color = QColor(*meta.get('color', (220, 220, 220)))
            table.insertRow(row)

            name_item = QTableWidgetItem(str(meta.get('name', f'Label {label_id}')))
            name_item.setData(Qt.ItemDataRole.UserRole, label_id)
            name_item.setForeground(QBrush(color))
            name_item.setFlags(name_item.flags() | Qt.ItemFlag.ItemIsEditable)

            count_item = QTableWidgetItem(str(count))
            count_item.setData(Qt.ItemDataRole.UserRole, label_id)
            count_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            count_item.setFlags(count_item.flags() & ~Qt.ItemFlag.ItemIsEditable)

            min_item = QTableWidgetItem(str(int(meta.get('min_limit', 0) or 0)))
            min_item.setData(Qt.ItemDataRole.UserRole, label_id)
            min_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            min_item.setFlags(min_item.flags() | Qt.ItemFlag.ItemIsEditable)

            max_item = QTableWidgetItem(str(int(meta.get('max_limit', 0) or 0)))
            max_item.setData(Qt.ItemDataRole.UserRole, label_id)
            max_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            max_item.setFlags(max_item.flags() | Qt.ItemFlag.ItemIsEditable)

            graph_item = QTableWidgetItem("")
            graph_item.setData(Qt.ItemDataRole.UserRole, label_id)
            graph_item.setTextAlignment(Qt.AlignmentFlag.AlignCenter)
            graph_item.setFlags(
                Qt.ItemFlag.ItemIsEnabled |
                Qt.ItemFlag.ItemIsSelectable |
                Qt.ItemFlag.ItemIsUserCheckable
            )
            graph_item.setCheckState(
                Qt.CheckState.Checked if bool(meta.get('show_chart', True)) else Qt.CheckState.Unchecked
            )

            table.setItem(row, 0, name_item)
            table.setItem(row, 1, count_item)
            table.setItem(row, 2, min_item)
            table.setItem(row, 3, max_item)
            table.setItem(row, 4, graph_item)

        table.blockSignals(False)
        self._updating_labels_table = False
        if table.rowCount() > 0:
            if current_id is None or not self._select_label_row(current_id):
                table.setCurrentCell(0, 0)
                table.selectRow(0)
        self._refresh_chart_metric_checkboxes()
        self._sync_label_editor_from_selection()

    def _sync_label_editor_from_selection(self):
        label_id = self._current_label_id()
        meta = self.engine.agent_labels.get(label_id) if label_id is not None else None
        enabled = meta is not None
        widget = getattr(self, 'label_color_btn', None)
        if widget is not None:
            widget.setEnabled(enabled)
        if meta:
            color = QColor(*meta.get('color', (220, 220, 220)))
            self.label_color_btn.setStyleSheet(f"background: rgb({color.red()},{color.green()},{color.blue()});")
        else:
            if widget is not None:
                widget.setStyleSheet("")

    def _on_label_table_item_changed(self, item: QTableWidgetItem):
        if getattr(self, '_updating_labels_table', False) or item is None:
            return
        label_id = item.data(Qt.ItemDataRole.UserRole)
        try:
            label_id = int(label_id)
        except Exception:
            return
        meta = self.engine.agent_labels.get(label_id)
        if meta is None:
            return
        if item.column() == 0:
            text = item.text().strip()
            if text:
                meta['name'] = text
                self._refresh_chart_metric_checkboxes()
            else:
                item.setText(str(meta.get('name', f'Label {label_id}')))
        elif item.column() in (2, 3):
            try:
                value = max(0, int(float(item.text().strip() or 0)))
            except Exception:
                value = int(meta.get('min_limit' if item.column() == 2 else 'max_limit', 0) or 0)
            key = 'min_limit' if item.column() == 2 else 'max_limit'
            meta[key] = value
            if item.text() != str(value):
                item.setText(str(value))
        elif item.column() == 4:
            meta['show_chart'] = item.checkState() == Qt.CheckState.Checked
            self._refresh_chart_metric_checkboxes()

    def _on_label_table_double_clicked(self, row: int, column: int):
        item = self.labels_table.item(row, 0) if 'labels_table' in self.__dict__ else None
        if item is None:
            return
        try:
            label_id = int(item.data(Qt.ItemDataRole.UserRole))
        except Exception:
            return
        self.engine.select_label(label_id)

    def _create_label_from_selection(self):
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        if not agents:
            QMessageBox.information(self, "Labels", "Selecione organismos antes de atribuir uma label.")
            return
        label_id = self.engine.create_agent_label()
        self.engine.assign_label_to_agents(label_id, agents)
        self._refresh_labels_list()
        self._select_label_row(label_id)

    def _assign_selection_to_current_label(self):
        label_id = self._current_label_id()
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        if label_id is None or not agents:
            return
        self.engine.assign_label_to_agents(label_id, agents)
        self._refresh_labels_list()

    def _remove_selection_from_current_label(self):
        label_id = self._current_label_id()
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        if label_id is None or not agents:
            return
        self.engine.remove_label_from_agents(label_id, agents)
        self._refresh_labels_list()

    def _select_current_label_group(self):
        label_id = self._current_label_id()
        if label_id is None:
            return
        self.engine.select_label(label_id)
        self._refresh_labels_list()

    def _delete_current_label(self):
        label_id = self._current_label_id()
        if label_id is None:
            return
        self.engine.delete_agent_label(label_id)
        self._refresh_labels_list()

    def _choose_current_label_color(self):
        label_id = self._current_label_id()
        if label_id is None:
            return
        meta = self.engine.agent_labels[label_id]
        current = QColor(*meta.get('color', (220, 220, 220)))
        color = QColorDialog.getColor(current, self, "Cor da label")
        if not color.isValid():
            return
        new_color = (color.red(), color.green(), color.blue())
        meta['color'] = new_color
        for agent in self.engine.get_agents_by_label(label_id):
            agent.color = new_color
        self._refresh_labels_list()

    # Nova UI de labels: paineis compactos em vez de tabela.
    def _build_tab_labels(self):
        tab = QWidget()
        self.tabs.addTab(tab, "Labels")
        root = QVBoxLayout(tab)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(8)
        self.engine.ensure_default_agent_label()
        self.labels_table = None  # mantem compatibilidade com chamadas antigas que testam a existencia do atributo
        self._current_label_panel_id = self._current_label_panel_id if hasattr(self, '_current_label_panel_id') else None
        info = QLabel("Cada label e uma linhagem/grupo. Os organismos novos sempre recebem uma label; Min e Max controlam a populacao desse grupo.")
        info.setWordWrap(True)
        root.addWidget(info)

        self.labels_scroll = QScrollArea()
        self.labels_scroll.setWidgetResizable(True)
        self.labels_scroll.setFrameShape(QFrame.Shape.NoFrame)
        self.labels_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        self.labels_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.labels_list_widget = QWidget()
        self.labels_list_layout = QVBoxLayout(self.labels_list_widget)
        self.labels_list_layout.setContentsMargins(0, 0, 0, 0)
        self.labels_list_layout.setSpacing(10)
        self.labels_scroll.setWidget(self.labels_list_widget)
        root.addWidget(self.labels_scroll, stretch=1)
        if not hasattr(self, '_labels_refresh_timer'):
            self._labels_refresh_timer = QTimer(self)
            self._labels_refresh_timer.timeout.connect(self._refresh_labels_list_light)
            self._labels_refresh_timer.start(1000)
        self._refresh_labels_list()

    def _current_label_id(self):
        label_id = getattr(self, '_current_label_panel_id', None)
        if label_id in getattr(self.engine, 'agent_labels', {}):
            return int(label_id)
        if getattr(self.engine, 'agent_labels', None):
            label_id = int(sorted(self.engine.agent_labels.keys())[0])
            self._current_label_panel_id = label_id
            return label_id
        return self.engine.ensure_default_agent_label()

    def _select_label_row(self, label_id: int):
        if label_id not in getattr(self.engine, 'agent_labels', {}):
            return False
        self._current_label_panel_id = int(label_id)
        self._refresh_label_panel_styles()
        return True

    def _dark_label_style(self, color: QColor, selected: bool = False) -> str:
        bg = QColor(max(18, int(color.red() * 0.20)), max(18, int(color.green() * 0.20)), max(22, int(color.blue() * 0.20)))
        border = QColor(min(255, int(color.red() * 0.80 + 40)), min(255, int(color.green() * 0.80 + 40)), min(255, int(color.blue() * 0.80 + 40)))
        width = 2 if selected else 1
        return (
            f"QFrame {{ background: rgb({bg.red()},{bg.green()},{bg.blue()}); "
            f"border:{width}px solid rgb({border.red()},{border.green()},{border.blue()}); border-radius:8px; }} "
            "QLabel { color:#edf4ff; border:none; background:transparent; } "
            "QLineEdit, QSpinBox { background:#101318; color:#edf4ff; border:1px solid #405060; border-radius:4px; padding:3px; } "
            "QPushButton { background:#232a32; color:#edf4ff; border:1px solid #4a5664; border-radius:5px; padding:4px 6px; } "
            "QCheckBox { color:#edf4ff; border:none; background:transparent; }"
        )

    def _refresh_label_panel_styles(self):
        for label_id, frame in getattr(self, '_label_panel_frames', {}).items():
            meta = self.engine.agent_labels.get(label_id, {})
            color = QColor(*meta.get('color', (220, 220, 220)))
            frame.setStyleSheet(self._dark_label_style(color, selected=(label_id == self._current_label_id())))

    def _apply_label_name(self, label_id: int, edit: QLineEdit):
        meta = self.engine.agent_labels.get(label_id)
        if not meta:
            return
        text = edit.text().strip() or f'organismo_{label_id}'
        meta['name'] = text
        edit.setText(text)
        self._refresh_chart_metric_checkboxes()

    def _apply_label_limit(self, label_id: int, key: str, spin: QSpinBox):
        meta = self.engine.agent_labels.get(label_id)
        if not meta:
            return
        meta[key] = max(0, int(spin.value()))
        self._refresh_chart_metric_checkboxes()

    def _make_label_panel(self, label_id: int, meta: dict) -> QFrame:
        frame = QFrame()
        frame.setObjectName(f"label_panel_{label_id}")
        def select_panel(event, lid=label_id):
            self._current_label_panel_id = lid
            self._refresh_label_panel_styles()
            event.accept()
        frame.mousePressEvent = select_panel
        color = QColor(*meta.get('color', (220, 220, 220)))
        frame.setStyleSheet(self._dark_label_style(color, selected=(label_id == self._current_label_id())))
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(10, 9, 10, 9)
        layout.setSpacing(8)

        top = QHBoxLayout()
        name = QLineEdit(str(meta.get('name', f'organismo_{label_id}')))
        name.returnPressed.connect(lambda lid=label_id, edit=name: self._apply_label_name(lid, edit))
        name.installEventFilter(self)
        top.addWidget(name, stretch=1)
        count = len(self.engine.get_agents_by_label(label_id))
        count_label = QLabel(f"{count} individuos")
        count_label.setMinimumWidth(86)
        count_label.setObjectName(f"label_count_{label_id}")
        top.addWidget(count_label)
        layout.addLayout(top)

        meta_row = QHBoxLayout()
        graph = QCheckBox("Grafico")
        graph.setChecked(bool(meta.get('show_chart', True)))
        graph.toggled.connect(lambda checked, lid=label_id: self._set_label_graph_visible(lid, checked))
        meta_row.addWidget(graph)
        meta_row.addWidget(QLabel("Min"))
        min_spin = _spin_int(0, 100000)
        min_spin.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
        min_spin.setFixedWidth(76)
        min_spin.setKeyboardTracking(False)
        min_spin.setValue(int(meta.get('min_limit', 0) or 0))
        min_spin.lineEdit().returnPressed.connect(lambda lid=label_id, spin=min_spin: self._apply_label_limit(lid, 'min_limit', spin))
        min_spin.installEventFilter(self)
        meta_row.addWidget(min_spin)
        meta_row.addWidget(QLabel("Max"))
        max_spin = _spin_int(0, 100000)
        max_spin.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
        max_spin.setFixedWidth(76)
        max_spin.setKeyboardTracking(False)
        max_spin.setValue(int(meta.get('max_limit', 0) or 0))
        max_spin.lineEdit().returnPressed.connect(lambda lid=label_id, spin=max_spin: self._apply_label_limit(lid, 'max_limit', spin))
        max_spin.installEventFilter(self)
        meta_row.addWidget(max_spin)
        delete_btn = QPushButton("Excluir")
        delete_btn.clicked.connect(lambda _checked=False, lid=label_id: self._delete_label(lid))
        meta_row.addWidget(delete_btn)
        meta_row.addStretch(1)
        layout.addLayout(meta_row)

        actions = QGridLayout()
        actions.setHorizontalSpacing(6)
        actions.setVerticalSpacing(6)
        color_btn = QPushButton("Cor")
        color_btn.clicked.connect(lambda _checked=False, lid=label_id: self._choose_label_color(lid))
        actions.addWidget(color_btn, 0, 0)
        select_btn = QPushButton("Selecionar")
        select_btn.clicked.connect(lambda _checked=False, lid=label_id: self._select_label_group(lid))
        actions.addWidget(select_btn, 0, 1)
        add_btn = QPushButton("Atribuir selecionados")
        add_btn.clicked.connect(lambda _checked=False, lid=label_id: self._assign_selection_to_label(lid))
        actions.addWidget(add_btn, 1, 0)
        remove_btn = QPushButton("Remover selecionados")
        remove_btn.clicked.connect(lambda _checked=False, lid=label_id: self._remove_selection_from_label(lid))
        actions.addWidget(remove_btn, 1, 1)
        reset_brain_btn = QPushButton("Resetar rede neural")
        reset_brain_btn.clicked.connect(lambda _checked=False, lid=label_id: self._reset_label_brains(lid))
        actions.addWidget(reset_brain_btn, 2, 0, 1, 2)
        layout.addLayout(actions)
        return frame

    def _set_label_graph_visible(self, label_id: int, checked: bool):
        meta = self.engine.agent_labels.get(label_id)
        if not meta:
            return
        meta['show_chart'] = bool(checked)
        self._refresh_chart_metric_checkboxes()

    def _select_label_group(self, label_id: int):
        if label_id not in self.engine.agent_labels:
            return
        self._current_label_panel_id = int(label_id)
        self.engine.select_label(label_id)
        self._refresh_label_panel_styles()

    def _refresh_labels_list(self):
        if 'labels_list_layout' not in self.__dict__:
            return
        self.engine.ensure_default_agent_label()
        if self._current_label_id() not in self.engine.agent_labels:
            self._current_label_panel_id = int(sorted(self.engine.agent_labels.keys())[0])
        layout = self.labels_list_layout
        while layout.count():
            item = layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
        self._label_panel_frames = {}
        self._label_count_widgets = {}
        for label_id, meta in sorted(self.engine.agent_labels.items()):
            panel = self._make_label_panel(int(label_id), meta)
            self._label_panel_frames[int(label_id)] = panel
            count_widget = panel.findChild(QLabel, f"label_count_{int(label_id)}")
            if count_widget is not None:
                self._label_count_widgets[int(label_id)] = count_widget
            layout.addWidget(panel)
        plus_container = QWidget()
        plus_row = QHBoxLayout(plus_container)
        plus_row.setContentsMargins(0, 2, 0, 2)
        plus_row.addStretch(1)
        plus_btn = QPushButton("+ Nova label com selecionados")
        plus_btn.setMinimumWidth(210)
        plus_btn.clicked.connect(self._create_label_from_selection)
        plus_row.addWidget(plus_btn)
        plus_row.addStretch(1)
        layout.addWidget(plus_container)
        layout.addStretch(1)
        self._labels_last_signature = self._labels_panel_signature()
        self._refresh_chart_metric_checkboxes()

    def _labels_panel_signature(self):
        return tuple(
            (
                int(label_id),
                str(meta.get('name', '')),
                tuple(meta.get('color', (220, 220, 220))),
                bool(meta.get('show_chart', True)),
                int(meta.get('min_limit', 0) or 0),
                int(meta.get('max_limit', 0) or 0),
            )
            for label_id, meta in sorted(getattr(self.engine, 'agent_labels', {}).items())
        )

    def _refresh_labels_list_light(self):
        if 'labels_list_layout' not in self.__dict__:
            return
        self.engine.ensure_default_agent_label()
        signature = self._labels_panel_signature()
        if signature != getattr(self, '_labels_last_signature', None):
            self._refresh_labels_list()
            return
        for label_id, widget in getattr(self, '_label_count_widgets', {}).items():
            count = len(self.engine.get_agents_by_label(label_id))
            widget.setText(f"{count} individuos")

    def _sync_label_editor_from_selection(self):
        self._refresh_label_panel_styles()

    def _create_label_from_selection(self):
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        label_id = self.engine.create_agent_label()
        if agents:
            self.engine.assign_label_to_agents(label_id, agents)
        self._current_label_panel_id = label_id
        self._refresh_labels_list()

    def _assign_selection_to_label(self, label_id: int):
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        if agents:
            self.engine.assign_label_to_agents(label_id, agents)
        self._current_label_panel_id = label_id
        self._refresh_labels_list()

    def _remove_selection_from_label(self, label_id: int):
        agents = list(getattr(self.engine, 'selected_agents', set()) or [])
        if agents:
            self.engine.remove_label_from_agents(label_id, agents)
        self._refresh_labels_list()

    def _reset_label_brains(self, label_id: int):
        agents = self.engine.get_agents_by_label(label_id)
        if not agents:
            QMessageBox.information(self, "Resetar rede neural", "Essa label nao tem organismos vivos.")
            return
        meta = self.engine.agent_labels.get(label_id, {})
        name = str(meta.get('name', f'Label {label_id}'))
        answer = QMessageBox.question(
            self,
            "Resetar rede neural",
            f"Reinicializar a rede neural dos {len(agents)} organismos da label '{name}'?",
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        reset_count = self.engine.reset_label_brains(label_id)
        self._refresh_labels_list_light()
        QMessageBox.information(self, "Resetar rede neural", f"{reset_count} redes reinicializadas.")

    def _delete_label(self, label_id: int):
        self.engine.delete_agent_label(label_id)
        self._current_label_panel_id = self.engine.ensure_default_agent_label()
        self._refresh_labels_list()

    def _assign_selection_to_current_label(self):
        label_id = self._current_label_id()
        if label_id is not None:
            self._assign_selection_to_label(label_id)

    def _remove_selection_from_current_label(self):
        label_id = self._current_label_id()
        if label_id is not None:
            self._remove_selection_from_label(label_id)

    def _select_current_label_group(self):
        label_id = self._current_label_id()
        if label_id is not None:
            self.engine.select_label(label_id)
            self._refresh_label_panel_styles()

    def _delete_current_label(self):
        label_id = self._current_label_id()
        if label_id is not None:
            self._delete_label(label_id)

    def _choose_label_color(self, label_id: int):
        meta = self.engine.agent_labels.get(label_id)
        if not meta:
            return
        current = QColor(*meta.get('color', (220, 220, 220)))
        color = QColorDialog.getColor(current, self, "Cor da label")
        if not color.isValid():
            return
        new_color = (color.red(), color.green(), color.blue())
        meta['color'] = new_color
        for agent in self.engine.get_agents_by_label(label_id):
            agent.color = new_color
        self._refresh_labels_list()

    def _choose_current_label_color(self):
        label_id = self._current_label_id()
        if label_id is not None:
            self._choose_label_color(label_id)

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
        w=_spin_int(1,240); w.setValue(_param_int(self.params,'fps',60)); add_exec("FPS:",'fps',w)
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
        w.setValue(_param_int(self.params, 'retina_skip', 0))
        add_perf("Retina skip:", 'retina_skip', w)

        w = _spin_int(-1, 2147483647)
        w.setValue(_param_int(self.params, 'random_seed', -1))
        add_perf("Seed RNG (-1 aleatoria):", 'random_seed', w)

        # Retina vision mode selector (single = centroid, fullbody = raycast, sector = fast sectors)
        mode_cb = QComboBox()
        mode_cb.addItems(['single', 'fullbody', 'sector'])
        mode_cb.setCurrentText(self.params.get('retina_vision_mode', 'single'))
        add_perf("Visão retinas:", 'retina_vision_mode', mode_cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('render_enabled', True))
        add_perf("Renderizar:", 'render_enabled', cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('simple_render', False))
        add_perf("Renderização simples:", 'simple_render', cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('use_numba_kernels', True))
        add_perf("Aceleracao arrays/Numba:", 'use_numba_kernels', cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('use_numba_batch_retina', False))
        add_perf("Retina Numba em lote:", 'use_numba_batch_retina', cb)

        cb = QCheckBox()
        cb.setChecked(self.params.get('use_numba_locomotion_energy', False))
        add_perf("Numba locomocao/energia:", 'use_numba_locomotion_energy', cb)

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
            self.widgets[name]=w; grid3.addWidget(self._help_label(label, name), r_vis,0); grid3.addWidget(w,r_vis,1); r_vis+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('show_selected_details',True)); add_vis("Detalhes agente selecionado:",'show_selected_details',cb)
        cb=QCheckBox(); cb.setChecked(not self.params.get('disable_brain_activations',False)); cb.toggled.connect(self._on_toggle_brain_activations); add_vis("Mostrar ativações neurais:",'enable_brain_activations',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('debug_tracebacks',False)); add_vis("Tracebacks no debug:",'debug_tracebacks',cb)
        v.addWidget(g_vis)
        # Grupo: Autosave
        g_auto = QGroupBox("Autosave"); g_auto.setStyleSheet(card_style); grid4=QGridLayout(g_auto); r_auto=0
        def add_auto(label,name,w):
            nonlocal r_auto
            self.widgets[name]=w; grid4.addWidget(self._help_label(label, name), r_auto,0); grid4.addWidget(w,r_auto,1); r_auto+=1
        cb=QCheckBox(); cb.setChecked(self.params.get('auto_export_substrate',False)); cb.toggled.connect(self._on_toggle_auto_export); add_auto("Ativar autosave:",'auto_export_substrate',cb)
        w=_spin_double(0.1,1440.0,0.5,2); w.setValue(self.params.get('auto_export_interval_minutes',10.0)); add_auto("Intervalo autosave (min):",'auto_export_interval_minutes',w)
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
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_obstacles',False)); add_vis("Ver obstaculos:",'bacteria_retina_see_obstacles',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_all',False)); add_vis("Ver tudo:",'bacteria_retina_see_all',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('bacteria_retina_see_through_walls',True)); add_vis("Ver atraves paredes:",'bacteria_retina_see_through_walls',cb)
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
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_obstacles',False)); add_vis("Ver obstaculos:",'predator_retina_see_obstacles',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_all',False)); add_vis("Ver tudo:",'predator_retina_see_all',cb)
        cb=QCheckBox(); cb.setChecked(self.params.get('predator_retina_see_through_walls',True)); add_vis("Ver atraves paredes:",'predator_retina_see_through_walls',cb)
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
    def _live_param_names(self) -> set[str]:
        return {
            'time_scale', 'fps', 'paused', 'physics_steps_per_second',
            'max_physics_steps_per_frame', 'max_physics_backlog_seconds',
            'render_enabled', 'simple_render', 'use_numba_kernels', 'use_numba_batch_retina',
            'use_numba_locomotion_energy', 'bacteria_show_vision',
            'predator_show_vision', 'show_selected_details',
            'show_spatial_hash', 'retina_vision_mode',
            'retina_bins_mode', 'retina_bins_distance_subdivisions',
            'retina_bins_distance_distribution', 'retina_bins_distance_falloff',
            'retina_bins_projection', 'retina_bins_candidate_limit',
            'retina_bins_obstacles_block_vision',
            'neural_network_type', 'neural_gate_init', 'neural_gate_min',
            'neural_gate_max', 'neural_gate_mutation_rate',
            'neural_gate_mutation_strength', 'neural_shortcut_init_std',
            'neural_shortcut_scale', 'neural_shortcut_mutation_rate',
            'neural_shortcut_mutation_strength', 'neural_rnn_recurrent_init_std',
            'neural_rnn_recurrent_scale', 'neural_rnn_memory_decay',
            'neural_rnn_state_clip', 'neural_rnn_reset_state_on_copy',
            'neural_rnn_mutation_rate', 'neural_rnn_mutation_strength',
            'camera_follow_selected_agent',
        }

    def _prepare_param_widget_runtime(self, name: str, widget: QWidget):
        if getattr(widget, '_agentbiosim_runtime_prepared', False):
            return
        if isinstance(widget, (QSpinBox, QDoubleSpinBox, QComboBox, QLineEdit)):
            widget.installEventFilter(self)
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                try:
                    widget.lineEdit().installEventFilter(self)
                except Exception:
                    pass
        if name in self._live_param_names():
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                widget.valueChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(widget, QCheckBox):
                widget.toggled.connect(lambda _v, n=name: self._update_param_real_time(n))
            elif isinstance(widget, QComboBox):
                widget.currentTextChanged.connect(lambda _v, n=name: self._update_param_real_time(n))
        widget._agentbiosim_runtime_prepared = True

    def _setup_live_param_signals(self):
        for name, widget in list(self.widgets.items()):
            self._prepare_param_widget_runtime(name, widget)
        # brain activation toggle handled separately

    def eventFilter(self, obj, event):
        if event.type() == QEvent.Type.Wheel and isinstance(obj, (QSpinBox, QDoubleSpinBox, QComboBox)):
            event.ignore()
            return True
        if event.type() == QEvent.Type.KeyPress and event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            name = self._widget_name_for_object(obj)
            if name:
                self._apply_widget_enter(name)
                return False
        return super().eventFilter(obj, event)

    def _widget_name_for_object(self, obj) -> str | None:
        for name, widget in self.widgets.items():
            if obj is widget:
                return name
            if isinstance(widget, (QSpinBox, QDoubleSpinBox)):
                try:
                    if obj is widget.lineEdit():
                        return name
                except Exception:
                    pass
        return None

    def _apply_widget_enter(self, name: str):
        genetic_names = set(self._agent_param_names('bacteria')) | {f'bacteria_neurons_layer_{i}' for i in range(1, 6)}
        environment_names = {
            'food_mode', 'food_bite_seconds', 'food_piece_particle_radius',
            'food_piece_cluster_radius', 'food_piece_particle_spacing', 'food_piece_replenish_mode',
            'food_target', 'food_min_r', 'food_max_r', 'food_replenish_interval',
            'world_w', 'world_h', 'substrate_shape', 'substrate_radius',
        }
        simulation_names = {'time_scale', 'fps', 'paused', 'physics_steps_per_second', 'max_physics_steps_per_frame', 'max_physics_backlog_seconds', 'use_spatial', 'retina_skip', 'random_seed', 'retina_vision_mode', 'retina_bins_mode', 'retina_bins_distance_subdivisions', 'retina_bins_distance_distribution', 'retina_bins_distance_falloff', 'retina_bins_projection', 'retina_bins_candidate_limit', 'retina_bins_obstacles_block_vision', 'neural_network_type', 'neural_gate_init', 'neural_gate_min', 'neural_gate_max', 'neural_gate_mutation_rate', 'neural_gate_mutation_strength', 'neural_shortcut_init_std', 'neural_shortcut_scale', 'neural_shortcut_mutation_rate', 'neural_shortcut_mutation_strength', 'neural_rnn_recurrent_init_std', 'neural_rnn_recurrent_scale', 'neural_rnn_memory_decay', 'neural_rnn_state_clip', 'neural_rnn_reset_state_on_copy', 'neural_rnn_mutation_rate', 'neural_rnn_mutation_strength', 'render_enabled', 'simple_render', 'show_spatial_hash', 'camera_follow_selected_agent', 'use_numba_kernels', 'use_numba_batch_retina', 'use_grouped_vision_batches', 'use_persistent_perception_arrays', 'retina_high_scale_auto_sector', 'retina_high_scale_global_sector', 'retina_high_scale_sector_min_agents', 'use_numba_brain_forward', 'numba_brain_forward_min_batch', 'use_numba_locomotion_energy', 'reuse_spatial_grid', 'agents_inertia', 'smooth_locomotion_enabled', 'smooth_linear_inertia_enabled', 'smooth_max_linear_accel', 'smooth_linear_drag_enabled', 'smooth_linear_drag', 'smooth_angular_inertia_enabled', 'smooth_max_angular_accel', 'smooth_angular_drag_enabled', 'smooth_angular_drag', 'render_interpolation_enabled', 'show_selected_details', 'debug_tracebacks'}
        if name == 'agent_template_name':
            self.params.set(name, self._get_widget_value(name), validate=False)
        elif name in genetic_names:
            self._collect_agent_params_from_widgets('bacteria', [name])
        elif name in environment_names:
            self.apply_substrate_params()
        elif name in simulation_names:
            self.apply_simulation_params()
        elif name in {'bacteria_count', 'population_min_rescue_enabled'}:
            self.apply_population_params()
        elif name in {'auto_export_substrate', 'auto_export_interval_minutes', 'export_substrate_include_brain_activations', 'export_substrate_pretty_json'}:
            self.params.set(name, self._get_widget_value(name), validate=False)
            if name in {'auto_export_substrate', 'auto_export_interval_minutes'} and bool(self.params.get('auto_export_substrate', False)):
                self._reschedule_auto_export()
        else:
            self.params.set(name, self._get_widget_value(name), validate=False)
        self._schedule_ui_params_save()

    def _on_toggle_brain_activations(self, checked: bool):
        profiler.enabled = checked
        self.params.set('disable_brain_activations', not checked)
        self._schedule_ui_params_save()

    def _start_diagnostic_heartbeat(self):
        minutes = float(self.params.get('diagnostic_heartbeat_minutes', 1.0) or 1.0)
        interval_ms = max(10_000, int(minutes * 60_000))
        self._diagnostic_timer = QTimer(self)
        self._diagnostic_timer.timeout.connect(self._diagnostic_heartbeat)
        self._diagnostic_timer.start(interval_ms)
        self._diagnostic_heartbeat(reason='startup')

    def _diagnostic_heartbeat(self, reason: str = 'timer'):
        try:
            engine = self.engine
            log_event(
                "HEARTBEAT",
                reason=reason,
                running=bool(getattr(engine, 'running', False)),
                paused=bool(self.params.get('paused', False)),
                sim_time=round(float(getattr(engine, 'total_simulation_time', 0.0)), 3),
                frame_count=int(getattr(engine, 'frame_count', 0)),
                bacteria=len(engine.entities.get('bacteria', [])),
                predators=len(engine.entities.get('predators', [])),
                foods=len(engine.entities.get('foods', [])),
                all_agents=len(getattr(engine, 'all_agents', [])),
                fps=round(float(getattr(engine, 'current_fps', 0.0)), 3),
                cpu_proc_percent=round(float(getattr(engine, 'cpu_proc_percent', 0.0)), 3),
                mem_used_mb=round(float(getattr(engine, 'mem_used_mb', 0.0)), 3),
                spatial_rebuilds=int(getattr(engine, 'spatial_hash_rebuilds', 0)),
                spatial_skips=int(getattr(engine, 'spatial_hash_skips', 0)),
            )
        except Exception as exc:
            self._log_exception("DIAGNOSTIC_HEARTBEAT_ERROR", exc)

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
            if name.startswith('neural_'):
                try:
                    from .brain import clear_multi_brain_cache
                    clear_multi_brain_cache()
                except Exception:
                    pass
                viewer = getattr(self, '_agent_neural_view', None)
                if viewer is not None:
                    viewer.set_agent(getattr(self.engine, 'selected_agent', None))
            self._schedule_ui_params_save()
        except Exception as e:
            self._log_exception(f"Erro callback {name}", e)

    def _init_pygame_view(self):
        try:
            log_event("PYGAME_INIT_START")
            win_id = int(self.pygame_host.winId())  # native window id
            self.pygame_view.initialize(win_id)
            def runner():
                try:
                    log_event("PYGAME_THREAD_START")
                    self.pygame_view.run()
                    log_event("PYGAME_THREAD_STOP")
                except Exception as e:
                    self._log_exception("Erro thread sim", e)
                    try:
                        self.engine.stop()
                    except Exception:
                        pass
            self._sim_thread = threading.Thread(target=runner, daemon=True)
            self._sim_thread.start()
            log_event("PYGAME_INIT_DONE")
        except Exception as e:
            self._log_exception("Falha ao inicializar pygame embutido", e)

    # ------------------------------------------------------------------
    # Apply parameter groups
    # ------------------------------------------------------------------
    def apply_population_params(self):
        for name in [
            'bacteria_count',
            'population_min_rescue_enabled',
        ]:
            if name in self.widgets:
                self.params.set(name, self._get_widget_value(name))
        # Neutraliza os limites antigos por tipo. A partir desta versao os
        # limites vivos sao por label, para permitir competicao de linhagens.
        for name, value in {
            'bacteria_min_limit': 0,
            'bacteria_max_limit': 0,
            'predators_enabled': False,
            'predator_count': 0,
            'predator_min_limit': 0,
            'predator_max_limit': 0,
        }.items():
            self.params.set(name, value, validate=False)
        print("Parametros de populacao aplicados; limites vivos ficam na aba Labels")
        self._schedule_ui_params_save()

    def apply_simulation_params(self):
        for name in ['time_scale','fps','paused','physics_steps_per_second','max_physics_steps_per_frame','max_physics_backlog_seconds','use_spatial','retina_skip','random_seed','retina_vision_mode','retina_bins_mode','retina_bins_distance_subdivisions','retina_bins_distance_distribution','retina_bins_distance_falloff','retina_bins_projection','retina_bins_candidate_limit','retina_bins_obstacles_block_vision','neural_network_type','neural_gate_init','neural_gate_min','neural_gate_max','neural_gate_mutation_rate','neural_gate_mutation_strength','neural_shortcut_init_std','neural_shortcut_scale','neural_shortcut_mutation_rate','neural_shortcut_mutation_strength','neural_rnn_recurrent_init_std','neural_rnn_recurrent_scale','neural_rnn_memory_decay','neural_rnn_state_clip','neural_rnn_reset_state_on_copy','neural_rnn_mutation_rate','neural_rnn_mutation_strength','render_enabled','simple_render','show_spatial_hash','camera_follow_selected_agent','use_numba_kernels','use_numba_batch_retina','use_grouped_vision_batches','use_persistent_perception_arrays','retina_high_scale_auto_sector','retina_high_scale_global_sector','retina_high_scale_sector_min_agents','use_numba_brain_forward','numba_brain_forward_min_batch','use_numba_locomotion_energy','reuse_spatial_grid','agents_inertia','smooth_locomotion_enabled','smooth_linear_inertia_enabled','smooth_max_linear_accel','smooth_linear_drag_enabled','smooth_linear_drag','smooth_angular_inertia_enabled','smooth_max_angular_accel','smooth_angular_drag_enabled','smooth_angular_drag','render_interpolation_enabled','show_selected_details','debug_tracebacks']:
            if name in self.widgets:
                val = self._get_widget_value(name)
                if name == 'show_selected_details':
                    val = bool(val)
                self.params.set(name, val)
        if 'enable_brain_activations' in self.widgets:
            enabled = bool(self._get_widget_value('enable_brain_activations'))
            profiler.enabled = enabled
            self.params.set('disable_brain_activations', not enabled)
        try:
            from .brain import clear_multi_brain_cache, configure_multi_brain_cache
            configure_multi_brain_cache(
                max_entries=self.params.get('brain_cache_max_entries', 32),
                max_mb=self.params.get('brain_cache_max_mb', 512),
                disable=self.params.get('brain_cache_disable', True),
                log=self.params.get('brain_cache_log', False),
                numba_forward=self.params.get('use_numba_brain_forward', False),
                numba_min_batch=self.params.get('numba_brain_forward_min_batch', 256),
            )
            clear_multi_brain_cache()
        except Exception:
            pass
        print("Parâmetros de simulação aplicados")
        self._schedule_ui_params_save()

    def apply_substrate_params(self):
        old_food_mode = str(self.params.get('food_mode', 'instant'))
        for name in ['food_mode','food_bite_seconds','food_piece_particle_radius','food_piece_cluster_radius','food_piece_particle_spacing','food_piece_replenish_mode','food_target','food_min_r','food_max_r','food_replenish_interval','world_w','world_h','substrate_shape','substrate_radius']:
            if name in self.widgets:
                self.params.set(name, self._get_widget_value(name))
        self._update_food_mode_controls()
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
        food_mode = str(self.params.get('food_mode', 'instant'))
        if old_food_mode != 'chunk' and food_mode == 'chunk':
            self.engine.entities['foods'].clear()
            try:
                self.engine.food_controller.food_debt = 0.0
                self.engine.food_controller.food_energy_debt = 0.0
                new_foods = self.engine.food_controller.update(
                    self.engine.entities['foods'],
                    int(self.params.get('food_target', 0) or 0),
                    self.engine.world.width,
                    self.engine.world.height,
                    self.params,
                    float(self.params.get('food_replenish_interval', 0.1) or 0.1),
                    obstacle_map=self.engine.obstacles,
                    agents=self.engine.all_agents,
                )
                self.engine.entities['foods'].extend(new_foods)
            except Exception:
                pass
        else:
            for food in self.engine.entities.get('foods', []):
                try:
                    food.kind = food_mode
                    if not getattr(food, 'initial_energy', None):
                        food.initial_energy = max(1e-9, float(getattr(food, 'energy', food.r * food.r)))
                    if not getattr(food, 'base_radius', None):
                        food.base_radius = float(getattr(food, 'r', 1.0))
                except Exception:
                    pass
        self.engine._spatial_hash_dirty = True
        # color pickers already update params and propagate; nothing else to do here
        print("Parâmetros de substrato aplicados")
        self._schedule_ui_params_save()

    def _agent_param_names(self, species: str) -> list[str]:
        if species == 'bacteria':
            return [
                'bacteria_initial_energy','bacteria_death_energy','bacteria_split_energy',
                'bacteria_age_death_enabled','bacteria_death_age','bacteria_corpse_to_food',
                'bacteria_reproduction_min_age','bacteria_reproduction_cooldown',
                'bacteria_metab_v0_cost','bacteria_metab_vmax_cost','bacteria_energy_cap',
                'bacteria_show_vision','bacteria_body_size','bacteria_vision_radius','bacteria_retina_count',
                'bacteria_body_shape','bacteria_movement_mode','bacteria_allow_reverse_locomotion',
                'bacteria_retina_fov_degrees','bacteria_eye_count','bacteria_eye_angle_degrees','bacteria_eye_separation_degrees',
                'bacteria_retina_see_food','bacteria_retina_see_bacteria','bacteria_retina_see_predators',
                'bacteria_retina_see_obstacles','bacteria_retina_see_all','bacteria_retina_see_through_walls',
                'bacteria_retina_input_mode',
                'bacteria_retina_channel_r','bacteria_retina_channel_g','bacteria_retina_channel_b','bacteria_retina_channel_d',
                'bacteria_diet_food','bacteria_diet_agents','bacteria_diet_same_label',
                'bacteria_diet_food_efficiency','bacteria_diet_agent_efficiency',
                'bacteria_max_speed','bacteria_hidden_layers','bacteria_mutation_rate',
                'bacteria_mutation_strength','bacteria_max_turn_deg'
            ]
        return [
                'predator_initial_energy','predator_death_energy','predator_split_energy',
                'predator_age_death_enabled','predator_death_age','predator_corpse_to_food',
                'predator_reproduction_min_age','predator_reproduction_cooldown',
                'predator_metab_v0_cost','predator_metab_vmax_cost','predator_energy_cap',
                'predator_body_size','predator_body_shape','predator_movement_mode','predator_allow_reverse_locomotion',
                'predator_show_vision','predator_vision_radius','predator_retina_see_food','predator_retina_count',
                'predator_retina_fov_degrees','predator_eye_count','predator_eye_angle_degrees','predator_eye_separation_degrees',
                'predator_retina_see_bacteria','predator_retina_see_predators','predator_max_speed',
            'predator_retina_see_obstacles','predator_retina_see_all','predator_retina_see_through_walls',
            'predator_retina_input_mode',
            'predator_retina_channel_r','predator_retina_channel_g','predator_retina_channel_b','predator_retina_channel_d',
            'predator_diet_food','predator_diet_agents','predator_diet_same_label',
            'predator_diet_food_efficiency','predator_diet_agent_efficiency',
            'predator_hidden_layers','predator_mutation_rate','predator_mutation_strength','predator_max_turn_deg'
        ]

    def _agent_param_group_names(self, species: str, group: str) -> list[str]:
        groups = {
            'energy': [
                f'{species}_initial_energy',
                f'{species}_death_energy',
                f'{species}_split_energy',
                f'{species}_age_death_enabled',
                f'{species}_death_age',
                f'{species}_corpse_to_food',
                f'{species}_reproduction_min_age',
                f'{species}_reproduction_cooldown',
                f'{species}_metab_v0_cost',
                f'{species}_metab_vmax_cost',
                f'{species}_energy_cap',
            ],
            'body_sensor_motion': [
                f'{species}_show_vision',
                f'{species}_body_size',
                f'{species}_body_shape',
                f'{species}_vision_radius',
                f'{species}_retina_count',
                f'{species}_retina_fov_degrees',
                f'{species}_eye_count',
                f'{species}_eye_angle_degrees',
                f'{species}_eye_separation_degrees',
                f'{species}_retina_see_food',
                f'{species}_retina_see_bacteria',
                f'{species}_retina_see_predators',
                f'{species}_retina_see_obstacles',
                f'{species}_retina_see_all',
                f'{species}_retina_see_through_walls',
                f'{species}_retina_input_mode',
                f'{species}_retina_channel_r',
                f'{species}_retina_channel_g',
                f'{species}_retina_channel_b',
                f'{species}_retina_channel_d',
                f'{species}_max_speed',
                f'{species}_max_turn_deg',
            ],
            'body_motion': [
                f'{species}_body_size',
                f'{species}_body_shape',
                f'{species}_movement_mode',
                f'{species}_allow_reverse_locomotion',
                f'{species}_max_speed',
                f'{species}_max_turn_deg',
            ],
            'vision': [
                f'{species}_show_vision',
                f'{species}_vision_radius',
                f'{species}_retina_count',
                f'{species}_retina_fov_degrees',
                f'{species}_eye_count',
                f'{species}_eye_angle_degrees',
                f'{species}_eye_separation_degrees',
                f'{species}_retina_see_food',
                f'{species}_retina_see_bacteria',
                f'{species}_retina_see_predators',
                f'{species}_retina_see_obstacles',
                f'{species}_retina_see_all',
                f'{species}_retina_see_through_walls',
                f'{species}_retina_input_mode',
                f'{species}_retina_channel_r',
                f'{species}_retina_channel_g',
                f'{species}_retina_channel_b',
                f'{species}_retina_channel_d',
            ],
            'diet': [
                f'{species}_diet_food',
                f'{species}_diet_agents',
                f'{species}_diet_same_label',
                f'{species}_diet_food_efficiency',
                f'{species}_diet_agent_efficiency',
            ],
            'color': [f'{species}_color'],
            'brain': [
                f'{species}_hidden_layers',
                f'{species}_mutation_rate',
                f'{species}_mutation_strength',
                *[f'{species}_neurons_layer_{i}' for i in range(1, 6)],
            ],
        }
        return groups.get(group, self._agent_param_names(species))

    def _collect_agent_params_from_widgets(self, species: str, names: list[str] | None = None):
        names_set = set(names) if names is not None else None
        turn_deg_key = f'{species}_max_turn_deg'
        turn_key = f'{species}_max_turn'
        for name in self._agent_param_names(species):
            if names_set is not None and name not in names_set:
                continue
            if name in self.widgets:
                value = self._get_widget_value(name)
                if name == turn_deg_key:
                    self.params.set(turn_key, math.radians(value))
                else:
                    self.params.set(name, value)
        for i in range(1, 6):
            name = f'{species}_neurons_layer_{i}'
            if names_set is not None and name not in names_set:
                continue
            if name in self.widgets:
                self.params.set(name, self._get_widget_value(name))
        see_org_key = f'{species}_retina_see_bacteria'
        see_legacy_key = f'{species}_retina_see_predators'
        if names_set is None or see_org_key in names_set or see_legacy_key in names_set:
            self.params.set(see_legacy_key, bool(self.params.get(see_org_key, False)), validate=False)
        channel_keys = [f'{species}_retina_channel_{ch}' for ch in ('r', 'g', 'b', 'd')] + [f'{species}_retina_input_mode']
        if names_set is None or any(key in names_set for key in channel_keys):
            self._normalize_retina_channel_widgets(species)
            self._collect_retina_params_from_widgets(species)

    def _desired_agent_brain_sizes(self, species: str) -> tuple[int, ...]:
        if species == 'bacteria':
            default_hidden_layers = 4
            default_neurons = lambda i: 20
        else:
            default_hidden_layers = 2
            default_neurons = lambda i: 16 if i == 1 else 8

        from .sensors import retina_input_size
        input_size = retina_input_size(self.params, species, int(self.params.get(f'{species}_retina_count', 18)))
        hidden_layers = int(self.params.get(f'{species}_hidden_layers', default_hidden_layers))
        sizes = [input_size]
        for i in range(1, 6):
            if i > hidden_layers:
                break
            neurons = int(self.params.get(f'{species}_neurons_layer_{i}', default_neurons(i)))
            if neurons > 0:
                sizes.append(neurons)
        from .actuators import locomotion_output_size
        sizes.append(locomotion_output_size(self.params.get(f'{species}_movement_mode', 'forward')))
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
        if species == 'bacteria':
            if mode == 'all_alive':
                return list(getattr(self.engine, 'all_agents', []))
            selected_group = [
                agent for agent in (getattr(self.engine, 'selected_agents', set()) or set())
                if agent in getattr(self.engine, 'all_agents', [])
            ]
            if selected_group:
                return selected_group
            selected = getattr(self.engine, 'selected_agent', None)
            if selected is None or selected not in getattr(self.engine, 'all_agents', []):
                try:
                    QMessageBox.information(self, "Aplicar ao selecionado", "Selecione um organismo.")
                except Exception:
                    pass
                return []
            return [selected]

        is_predator = species == 'predator'
        if mode == 'all_alive':
            key = 'predators' if is_predator else 'bacteria'
            return list(self.engine.entities.get(key, []))

        selected_group = [
            agent for agent in (getattr(self.engine, 'selected_agents', set()) or set())
            if agent in getattr(self.engine, 'all_agents', [])
            and bool(getattr(agent, 'is_predator', False)) == is_predator
        ]
        if selected_group:
            return selected_group

        selected = getattr(self.engine, 'selected_agent', None)
        if selected is None or bool(getattr(selected, 'is_predator', False)) != is_predator:
            try:
                label = "predador" if is_predator else "bacteria"
                QMessageBox.information(self, "Aplicar ao selecionado", f"Selecione um agente do tipo {label}.")
            except Exception:
                pass
            return []
        return [selected]

    def _agents_requiring_brain_rebuild(self, species: str, agents: list[Any], param_names: list[str] | None = None) -> list[Any]:
        if param_names is not None:
            structural_names = {f'{species}_hidden_layers', f'{species}_movement_mode'} | {f'{species}_neurons_layer_{i}' for i in range(1, 6)}
            if not (set(param_names) & structural_names):
                return []
        desired = self._desired_agent_brain_sizes(species)
        desired_brain_type = str(self.params.get('neural_network_type', 'mlp'))
        changed = []
        for agent in agents:
            brain = getattr(agent, 'brain', None)
            current = tuple(getattr(brain, 'sizes', ()) or ())
            current_type = str(getattr(brain, 'brain_type', 'mlp') or 'mlp')
            if current_type != desired_brain_type:
                changed.append(agent)
                continue
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
        label = "predadores legados" if species == 'predator' else "organismos"
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

    def _apply_agent_template_to_agents(
        self,
        species: str,
        agents: list[Any],
        rebuild_brain: bool,
        param_names: list[str] | None = None,
    ) -> dict[str, int]:
        helpers = self._agent_factory_helpers(species)
        desired_sizes = self._desired_agent_brain_sizes(species)
        body_key = f'{species}_body_size'
        color_key = f'{species}_color'
        stats = {'agents': 0, 'brains_rebuilt': 0, 'brains_resized': 0, 'brains_kept': 0}
        names = set(param_names) if param_names is not None else None
        full_apply = names is None
        energy_names = {
            f'{species}_initial_energy', f'{species}_death_energy', f'{species}_split_energy',
            f'{species}_age_death_enabled', f'{species}_death_age', f'{species}_corpse_to_food',
            f'{species}_reproduction_min_age', f'{species}_reproduction_cooldown',
            f'{species}_metab_v0_cost', f'{species}_metab_vmax_cost', f'{species}_energy_cap',
        }
        sensor_names = {
            f'{species}_vision_radius', f'{species}_retina_count', f'{species}_retina_fov_degrees',
            f'{species}_eye_count', f'{species}_eye_angle_degrees', f'{species}_eye_separation_degrees',
            f'{species}_retina_see_food', f'{species}_retina_see_bacteria', f'{species}_retina_see_predators',
            f'{species}_retina_see_obstacles', f'{species}_retina_see_all', f'{species}_retina_see_through_walls',
            f'{species}_retina_input_mode',
            f'{species}_retina_channel_r', f'{species}_retina_channel_g', f'{species}_retina_channel_b',
            f'{species}_retina_channel_d',
            f'{species}_show_vision',
        }
        locomotion_names = {
            f'{species}_max_speed', f'{species}_max_turn_deg',
            f'{species}_movement_mode', f'{species}_allow_reverse_locomotion', f'{species}_body_shape',
        }
        diet_names = {
            f'{species}_diet_food', f'{species}_diet_agents', f'{species}_diet_same_label',
            f'{species}_diet_food_efficiency', f'{species}_diet_agent_efficiency',
        }
        brain_names = {f'{species}_hidden_layers', f'{species}_mutation_rate', f'{species}_mutation_strength'} | {
            f'{species}_neurons_layer_{i}' for i in range(1, 6)
        }
        apply_body = full_apply or body_key in names or f'{species}_body_shape' in names
        apply_color = full_apply or color_key in names
        apply_energy = full_apply or bool(names & energy_names)
        apply_sensor = full_apply or bool(names & sensor_names)
        apply_locomotion = full_apply or bool(names & locomotion_names)
        apply_diet = full_apply or bool(names & diet_names)
        apply_brain = full_apply or bool(names & brain_names)

        for agent in agents:
            stats['agents'] += 1
            if apply_body:
                try:
                    radius = float(self.params.get(body_key, getattr(agent, 'r', 1.0)))
                    agent.r = max(0.1, radius)
                    agent.m = agent.r * agent.r
                except Exception:
                    pass

            if apply_sensor:
                agent.sensor = helpers['sensor'](self.params)
            if apply_locomotion:
                agent.locomotion = helpers['locomotion'](self.params)
                try:
                    agent.body_shape = getattr(agent.locomotion, 'body_shape', self.params.get(f'{species}_body_shape', getattr(agent, 'body_shape', 'ellipse')))
                except Exception:
                    pass
            if apply_energy:
                agent.energy_model = helpers['energy'](self.params)
                cap = getattr(agent.energy_model, 'energy_cap', None)
                if cap is not None and getattr(agent, 'energy', 0.0) > cap:
                    agent.energy = float(cap)

            if apply_color:
                try:
                    color = self.params.get(color_key, None)
                    if color is not None:
                        agent.color = tuple(color)
                except Exception:
                    pass

            if apply_diet:
                try:
                    agent.diet_food = bool(self.params.get(f'{species}_diet_food', getattr(agent, 'diet_food', True)))
                    agent.diet_agents = bool(self.params.get(f'{species}_diet_agents', getattr(agent, 'diet_agents', False)))
                    agent.diet_same_label = bool(self.params.get(f'{species}_diet_same_label', getattr(agent, 'diet_same_label', False)))
                    agent.diet_food_efficiency = float(self.params.get(f'{species}_diet_food_efficiency', getattr(agent, 'diet_food_efficiency', 1.0)))
                    agent.diet_agent_efficiency = float(self.params.get(f'{species}_diet_agent_efficiency', getattr(agent, 'diet_agent_efficiency', 0.7)))
                except Exception:
                    pass

            brain = getattr(agent, 'brain', None)
            current_sizes = tuple(getattr(brain, 'sizes', ()) or ())
            current_type = str(getattr(brain, 'brain_type', 'mlp') or 'mlp')
            desired_type = str(self.params.get('neural_network_type', 'mlp'))
            type_changed = current_type != desired_type
            if apply_brain and (current_sizes != desired_sizes or type_changed):
                if brain is not None and len(current_sizes) == len(desired_sizes) and current_sizes[1:] == desired_sizes[1:] and hasattr(brain, 'resize_input'):
                    if type_changed:
                        if rebuild_brain:
                            agent.brain = helpers['brain'](self.params)
                            stats['brains_rebuilt'] += 1
                        else:
                            stats['brains_kept'] += 1
                    else:
                        brain.resize_input(desired_sizes[0])
                        brain.version = int(getattr(brain, 'version', 0)) + 1
                        stats['brains_resized'] += 1
                elif rebuild_brain:
                    agent.brain = helpers['brain'](self.params)
                    stats['brains_rebuilt'] += 1
                else:
                    stats['brains_kept'] += 1
            elif apply_sensor and not apply_brain and brain is not None and hasattr(brain, 'sizes'):
                from .sensors import retina_input_size
                desired_input = retina_input_size(self.params, species, current_sizes[0] if current_sizes else 0)
                if current_sizes and current_sizes[0] != desired_input and hasattr(brain, 'resize_input'):
                    brain.resize_input(desired_input)
                    brain.version = int(getattr(brain, 'version', 0)) + 1
                    stats['brains_resized'] += 1

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

    def _apply_agent_params(
        self,
        species: str,
        mode: str = 'template',
        confirm_structural: bool = True,
        param_names: list[str] | None = None,
        group_label: str | None = None,
    ):
        if mode not in {'template', 'all_alive', 'selected'}:
            mode = 'template'

        self._collect_agent_params_from_widgets(species, param_names)
        label = "predadores legados" if species == 'predator' else "organismos"
        if mode == 'template':
            print(f"Parametros de {label} aplicados ao template de novos individuos")
            self._schedule_ui_params_save()
            return

        lock = getattr(self.engine, 'state_lock', None)
        if lock is not None:
            with lock:
                agents = self._target_live_agents(species, mode)
                structural = self._agents_requiring_brain_rebuild(species, agents, param_names)
        else:
            agents = self._target_live_agents(species, mode)
            structural = self._agents_requiring_brain_rebuild(species, agents, param_names)

        if not agents:
            print(f"Nenhum agente vivo de {label} recebeu parametros")
            self._schedule_ui_params_save()
            return

        rebuild_brain = False
        if structural:
            rebuild_brain = True if not confirm_structural else self._confirm_agent_brain_rebuild(species, len(structural))

        if lock is not None:
            with lock:
                stats = self._apply_agent_template_to_agents(species, agents, rebuild_brain, param_names)
        else:
            stats = self._apply_agent_template_to_agents(species, agents, rebuild_brain, param_names)

        scope = "selecionado" if mode == 'selected' else "todos vivos"
        group = f" ({group_label})" if group_label else ""
        print(
            f"Parametros de {label}{group} aplicados a {scope}: "
            f"{stats['agents']} agentes, {stats['brains_rebuilt']} cerebros recriados, "
            f"{stats['brains_resized']} entradas redimensionadas, {stats['brains_kept']} cerebros preservados"
        )
        self._schedule_ui_params_save()

    def apply_bacteria_params(self, mode: str = 'template', confirm_structural: bool = True):
        self._apply_agent_params('bacteria', mode, confirm_structural)

    def apply_predator_params(self, mode: str = 'template', confirm_structural: bool = True):
        self._apply_agent_params('predator', mode, confirm_structural)

    def apply_agent_param_group(self, species: str, group: str, mode: str):
        names = self._agent_param_group_names(species, group)
        labels = {
            'energy': 'metabolismo/energia',
            'body_sensor_motion': 'corpo/sensores/movimento',
            'body_motion': 'corpo/movimento',
            'vision': 'visao',
            'diet': 'dieta',
            'color': 'cor',
            'brain': 'cerebro/mutacao',
        }
        self._apply_agent_params(
            species,
            mode,
            confirm_structural=True,
            param_names=names,
            group_label=labels.get(group, group),
        )

    def apply_all_params(self):
        self.apply_population_params(); self.apply_simulation_params(); self.apply_substrate_params(); self.apply_bacteria_params()
        self.apply_autosave_params(silent=True)
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

    def apply_autosave_params(self, silent: bool = False):
        for name in [
            'auto_export_substrate',
            'export_substrate_include_brain_activations',
            'export_substrate_pretty_json',
            'debug_tracebacks',
        ]:
            if name in self.widgets:
                self.params.set(name, bool(self._get_widget_value(name)), validate=False)
        if 'auto_export_interval_minutes' in self.widgets:
            self.params.set('auto_export_interval_minutes', self._get_widget_value('auto_export_interval_minutes'), validate=False)
        if self._is_autosave_enabled():
            self._reschedule_auto_export()
        elif self._auto_export_timer:
            self._auto_export_timer.stop()
            self._auto_export_timer = None
        self._schedule_ui_params_save()
        if not silent:
            print("Autosave aplicado")

    # ------------------------------------------------------------------
    # Engine actions
    # ------------------------------------------------------------------
    def _set_paused_state(self, paused: bool):
        if 'paused' in self.widgets:
            self._set_widget_value('paused', bool(paused))
        self.params.set('paused', bool(paused), validate=False)

    def delete_selected_agents(self):
        state_lock = getattr(self.engine, 'state_lock', None)
        if state_lock is None:
            removed = self.engine.remove_selected_agents()
        else:
            with state_lock:
                removed = self.engine.remove_selected_agents()
        if removed and 'labels_table' in self.__dict__:
            self._refresh_labels_list()
        if removed:
            print(f"{removed} organismo(s) selecionado(s) removido(s)")

    def _reset_population_now(self):
        state_lock = getattr(self.engine, 'state_lock', None)
        if state_lock is not None:
            state_lock.acquire()
        try:
            self.engine._initialize_population()
            self.engine.total_simulation_time = 0.0
            self.engine.frame_count = 0
            self.engine._spatial_hash_dirty = True
            self._reset_metrics_history()
        finally:
            if state_lock is not None:
                state_lock.release()
        if 'labels_table' in self.__dict__:
            self._refresh_labels_list()

    def reset_population(self):
        self.apply_all_params()
        self._reset_population_now()
        print("População resetada")

    def start_simulation(self):
        self.apply_all_params()
        self._set_paused_state(False)
        if not self.engine.running:
            self.engine.start()
            print("Simulacao iniciada")
        else:
            print("Simulacao ja em execucao")
        self._update_metrics_chart(force=True)

    def play_simulation(self):
        if self.engine.running:
            self._set_paused_state(False)
            print("Simulacao retomada")
            self._update_metrics_chart(force=True)
        else:
            self.start_simulation()

    def pause_simulation(self):
        self._set_paused_state(True)
        print("Simulacao pausada")

    def stop_simulation(self):
        self.apply_all_params()
        self._set_paused_state(True)
        self.engine.stop()
        self._reset_population_now()
        print("Simulacao parada e populacao resetada")

    def save_biosim_window(self):
        if not getattr(self, '_current_biosim_path', None):
            self.save_biosim_as_window()
            return
        try:
            self._export_substrate(path_override=self._current_biosim_path, file_type='biosim')
            self._update_window_title()
            QMessageBox.information(self, "Salvar Simulacao", f"Projeto salvo em {self._current_biosim_path}")
        except Exception as e:
            self._warn_exception("Erro ao salvar simulacao", e)

    def save_biosim_as_window(self):
        default_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'substrates'))
        os.makedirs(default_dir, exist_ok=True)
        initial = getattr(self, '_current_biosim_path', None) or os.path.join(default_dir, 'projeto.biosim')
        path, _ = QFileDialog.getSaveFileName(self, "Salvar Simulacao Como", initial, "BioSim (*.biosim)")
        if not path:
            return
        if not path.endswith('.biosim'):
            path += '.biosim'
        try:
            self._export_substrate(path_override=path, file_type='biosim')
            self._current_biosim_path = path
            self._update_window_title()
            QMessageBox.information(self, "Salvar Simulacao", f"Projeto salvo em {path}")
        except Exception as e:
            self._warn_exception("Erro ao salvar simulacao", e)

    def open_biosim_window(self):
        default_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'substrates'))
        os.makedirs(default_dir, exist_ok=True)
        path, _ = QFileDialog.getOpenFileName(self, "Abrir Simulacao", default_dir, "BioSim (*.biosim);;JSON (*.json)")
        if not path:
            return
        try:
            self._import_substrate(path)
            if path.lower().endswith('.biosim'):
                self._current_biosim_path = path
            else:
                self._current_biosim_path = None
            self._update_window_title()
            QMessageBox.information(self, "Abrir Simulacao", "Projeto carregado.")
        except Exception as e:
            self._warn_exception("Erro ao abrir simulacao", e)

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
                if hasattr(self.engine, 'selected_agents'):
                    self.engine.selected_agents.clear()
                self.engine.agent_labels.clear()
                self.engine._next_agent_label_id = 1
                self.engine.dragged_object = None
                self.engine.total_simulation_time = 0.0
                self.engine.frame_count = 0
                self.engine._initialize_population()
                self._reset_metrics_history()
                self._current_biosim_path = None
                self._update_window_title()
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
            "Mouse: botao direito move a camera. Na barra superior use Play/Pause/Stop, selecao unitaria/quadrada/lasso, F para comida, A para agente importado, pincel para obstaculos, M para mover e D para remover.\n\n"
            "Menus superiores: Arquivo salva/abre projetos .biosim; View controla visualizacao; Preferencias abre opcoes de simulacao, autosave e aparencia; Agente exporta, carrega e cria linhagens."
        )

    # ------------------------------------------------------------------
    # Persistence CSV
    # ------------------------------------------------------------------
    def _schedule_ui_params_save(self, delay_ms: int = 900):
        try:
            state = object.__getattribute__(self, '__dict__')
        except Exception:
            return
        if not bool(state.get('_ui_params_autosave_ready', False)):
            return
        timer = state.get('_ui_params_save_timer')
        if timer is not None:
            timer.start(max(50, int(delay_ms)))
            return
        self.save_ui_params(silent=True)

    def _set_param_and_schedule(self, name: str, value: Any, validate: bool = False):
        self.params.set(name, value, validate=validate)
        self._schedule_ui_params_save()

    def save_ui_params(self, silent: bool = False):
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
                color_params = {
                    'substrate_bg_color': (10, 10, 20),
                    'background_color_top': (10, 10, 20),
                    'background_color_bottom': (10, 10, 20),
                    'substrate_color_top': (10, 10, 20),
                    'substrate_color_bottom': (10, 10, 20),
                    'substrate_border_color': (40, 200, 40),
                    'food_color': (220, 30, 30),
                    'bacteria_color': (220, 220, 220),
                    'predator_color': (80, 120, 220),
                }
                for param_name, default in color_params.items():
                    rows_by_name[param_name] = {
                        'name': param_name,
                        'value': _json.dumps(list(self.params.get(param_name, default))),
                    }
                bool_params = {
                    'background_gradient_enabled': False,
                    'substrate_gradient_enabled': False,
                    'substrate_border_enabled': True,
                }
                for param_name, default in bool_params.items():
                    rows_by_name[param_name] = {'name': param_name, 'value': bool(self.params.get(param_name, default))}
                for menu_param in ['render_enabled', 'simple_render', 'show_spatial_hash', 'show_selected_details', 'show_metrics_chart', 'bacteria_show_vision', 'predator_show_vision', 'camera_follow_selected_agent', 'neural_view_dense_layout']:
                    rows_by_name[menu_param] = {'name': menu_param, 'value': self.params.get(menu_param, True if menu_param == 'render_enabled' else False)}
                rows_by_name['render_resolution_scale'] = {
                    'name': 'render_resolution_scale',
                    'value': self.params.get('render_resolution_scale', 1.0),
                }
                for param_name, default in {
                    'auto_export_substrate': False,
                    'auto_export_interval_minutes': 10.0,
                    'export_substrate_include_brain_activations': False,
                    'export_substrate_pretty_json': False,
                    'debug_tracebacks': False,
                    'fps': 60,
                    'physics_steps_per_second': 30,
                    'max_physics_steps_per_frame': 8,
                    'max_physics_backlog_seconds': 0.25,
                    'use_spatial': True,
                    'retina_skip': 0,
                    'random_seed': -1,
                    'retina_vision_mode': 'single',
                    'retina_bins_mode': 'nearest',
                    'retina_bins_distance_subdivisions': 5,
                    'retina_bins_distance_distribution': 'near_detail',
                    'retina_bins_distance_falloff': 'linear',
                    'retina_bins_projection': 'center',
                    'retina_bins_candidate_limit': 128,
                    'retina_bins_obstacles_block_vision': False,
                    'reuse_spatial_grid': True,
                    'use_numba_batch_retina': False,
                    'use_grouped_vision_batches': True,
                    'use_persistent_perception_arrays': False,
                    'retina_high_scale_auto_sector': False,
                    'retina_high_scale_global_sector': False,
                    'retina_high_scale_sector_min_agents': 800,
                    'brain_cache_disable': False,
                    'use_numba_brain_forward': False,
                    'numba_brain_forward_min_batch': 256,
                    'neural_network_type': 'mlp',
                    'neural_gate_init': 1.0,
                    'neural_gate_min': 0.0,
                    'neural_gate_max': 2.0,
                    'neural_gate_mutation_rate': -1.0,
                    'neural_gate_mutation_strength': -1.0,
                    'neural_shortcut_init_std': 0.05,
                    'neural_shortcut_scale': 0.25,
                    'neural_shortcut_mutation_rate': -1.0,
                    'neural_shortcut_mutation_strength': -1.0,
                    'neural_rnn_recurrent_init_std': 0.08,
                    'neural_rnn_recurrent_scale': 0.35,
                    'neural_rnn_memory_decay': 0.6,
                    'neural_rnn_state_clip': 1.0,
                    'neural_rnn_reset_state_on_copy': True,
                    'neural_rnn_mutation_rate': -1.0,
                    'neural_rnn_mutation_strength': -1.0,
                    'use_numba_locomotion_energy': False,
                    'agents_inertia': 1.0,
                    'smooth_locomotion_enabled': False,
                    'smooth_linear_inertia_enabled': True,
                    'smooth_max_linear_accel': 900.0,
                    'smooth_linear_drag_enabled': True,
                    'smooth_linear_drag': 0.75,
                    'smooth_angular_inertia_enabled': True,
                    'smooth_max_angular_accel': math.pi * 4.0,
                    'smooth_angular_drag_enabled': True,
                    'smooth_angular_drag': 1.5,
                    'render_interpolation_enabled': False,
                }.items():
                    rows_by_name[param_name] = {
                        'name': param_name,
                        'value': self.params.get(param_name, default),
                    }
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
            if silent:
                return
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
                rows = list(csv.DictReader(f))
                saved_names = {row.get('name') for row in rows}
                for row in rows:
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
                    if hasattr(self.params, '_data') and name in self.params._data:
                        try:
                            current = self.params.get(name)
                            if isinstance(current, bool):
                                parsed = value in ('1', 'True', 'true', 'yes', 'YES')
                            elif isinstance(current, int) and not isinstance(current, bool):
                                parsed = int(float(value))
                            elif isinstance(current, float):
                                parsed = float(value)
                            elif isinstance(current, str):
                                parsed = str(value)
                            else:
                                parsed = current
                            if isinstance(current, (bool, int, float, str)):
                                self.params.set(name, parsed, validate=False)
                        except Exception:
                            pass
                    # Additional: load saved color params or substrate shape even if not in widgets
                    try:
                        color_param_defaults = {
                            'substrate_bg_color': (10, 10, 20),
                            'background_color_top': (10, 10, 20),
                            'background_color_bottom': (10, 10, 20),
                            'substrate_color_top': (10, 10, 20),
                            'substrate_color_bottom': (10, 10, 20),
                            'substrate_border_color': (40, 200, 40),
                        }
                        if name in color_param_defaults:
                            import json as _json
                            col = tuple(int(max(0, min(255, float(c)))) for c in _json.loads(value)[:3])
                            self.params.set(name, col, validate=False)
                            if name == 'substrate_bg_color' and 'background_color_top' not in saved_names:
                                self.params.set('background_color_top', col, validate=False)
                            if name == 'substrate_bg_color' and 'background_color_bottom' not in saved_names:
                                self.params.set('background_color_bottom', col, validate=False)
                            if name == 'substrate_bg_color' and 'substrate_color_top' not in saved_names:
                                self.params.set('substrate_color_top', col, validate=False)
                            if name == 'substrate_bg_color' and 'substrate_color_bottom' not in saved_names:
                                self.params.set('substrate_color_bottom', col, validate=False)
                        bool_param_defaults = {
                            'background_gradient_enabled': False,
                            'substrate_gradient_enabled': False,
                            'substrate_border_enabled': True,
                        }
                        if name in bool_param_defaults:
                            self.params.set(name, value in ('1', 'True', 'true', 'yes', 'YES'), validate=False)
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
                        if name in ['render_enabled', 'simple_render', 'show_spatial_hash', 'show_selected_details', 'show_metrics_chart', 'bacteria_show_vision', 'predator_show_vision']:
                            checked = value in ('1', 'True', 'true', 'yes', 'YES')
                            self.params.set(name, checked, validate=False)
                            if name == 'show_metrics_chart':
                                panel = getattr(self, 'metrics_panel', None)
                                if panel is not None:
                                    panel.setVisible(checked)
                        if name == 'render_resolution_scale':
                            scale = max(1.0, min(3.0, float(value)))
                            self.params.set('render_resolution_scale', scale, validate=False)
                            if hasattr(getattr(self, 'pygame_view', None), 'set_render_scale'):
                                self.pygame_view.set_render_scale(scale)
                        if name == 'enable_brain_activations':
                            enabled = value in ('1', 'True', 'true', 'yes', 'YES')
                            profiler.enabled = enabled
                            self.params.set('disable_brain_activations', not enabled, validate=False)
                    except Exception:
                        pass
            # schedule autosave if active
            if self._is_autosave_enabled():
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
        add('agent_name', getattr(agent, 'agent_name', None) or self.params.get('agent_template_name', 'organismo_1'))
        add('type', 'predator' if getattr(agent,'is_predator', False) else 'organism')
        for attr in ['x','y','r','angle','vx','vy','angular_velocity','energy','age']:
            add(attr, getattr(agent, attr, 0.0))
        for attr in ['food_eaten_count','food_energy_eaten_total','prey_eaten_count','prey_energy_eaten_total']:
            add(attr, getattr(agent, attr, 0.0))
        add('label_ids', json.dumps(sorted(int(v) for v in (getattr(agent, 'label_ids', set()) or set()))))
        add('last_reproduction_age', getattr(agent, 'last_reproduction_age', ''))
        # Cor do agente (RGB tuple) - exportada em JSON para compatibilidade
        try:
            col = getattr(agent, 'color', None)
            if col is not None:
                add('color', json.dumps(list(col)))
        except Exception:
            pass
        if brain is not None and hasattr(brain,'sizes'):
            try:
                from .brain import brain_to_data
                brain_data = brain_to_data(brain)
                add('brain_type', brain_data.get('brain_type', 'mlp'))
                add('brain_type_label', brain_data.get('brain_type_label', 'MLP padrao'))
                for extra_key, extra_value in brain_data.items():
                    if extra_key in {'brain_type', 'brain_type_label', 'brain_sizes', 'brain_weights', 'brain_biases', 'brain_version'}:
                        continue
                    add(extra_key, json.dumps(extra_value) if isinstance(extra_value, (list, dict)) else extra_value)
            except Exception:
                pass
            add('brain_sizes', json.dumps(brain.sizes)); add('brain_version', getattr(brain,'version',0))
            for idx,(W,B) in enumerate(zip(brain.weights, brain.biases)):
                try:
                    w_list = W.tolist() if hasattr(W,'tolist') else list(W)
                    b_list = B.tolist() if hasattr(B,'tolist') else list(B)
                except Exception:
                    w_list = list(W); b_list = list(B)
                add(f'brain_weight_{idx}', json.dumps(w_list)); add(f'brain_bias_{idx}', json.dumps(b_list))
        if sensor is not None:
            for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators','see_obstacles','see_all','see_through_walls','eye_count','eye_angle_degrees','eye_separation_degrees']:
                if hasattr(sensor, attr): add(f'sensor_{attr}', getattr(sensor, attr))
            if hasattr(sensor, 'channels'):
                channels = tuple(getattr(sensor, 'channels', ('d',)))
                add('sensor_channels', json.dumps(list(channels)))
                try:
                    from .sensors import retina_input_mode_from_channels
                    add('sensor_input_mode', retina_input_mode_from_channels(channels))
                    color_channels = sorted({ch[0] if len(ch) == 2 and ch.endswith('d') else ch for ch in channels if ch in ('r', 'g', 'b') or (len(ch) == 2 and ch.endswith('d'))})
                    add('sensor_color_channels', json.dumps(color_channels))
                except Exception:
                    pass
        for attr in ['diet_food', 'diet_agents', 'diet_same_label', 'diet_food_efficiency', 'diet_agent_efficiency']:
            if hasattr(agent, attr):
                add(attr, getattr(agent, attr))
        if locomotion is not None:
            for attr in ['max_speed','max_turn','allow_reverse','movement_mode','body_shape']:
                if hasattr(locomotion, attr): add(f'locomotion_{attr}', getattr(locomotion, attr))
        if hasattr(agent, 'body_shape'):
            add('body_shape', getattr(agent, 'body_shape'))
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
        for attr in ['mutation_rate', 'mutation_strength']:
            key = f"bacteria_{attr}"
            add(key, self.params.get(key, ''))
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
        self.engine._prototype_revision = int(getattr(self.engine, '_prototype_revision', 0) or 0) + 1
        self._apply_agent_data_to_genetic_editor(data, name)
        self._last_applied_prototype_revision = int(getattr(self.engine, '_prototype_revision', 0) or 0)
        print(f"Prototipo '{name}' carregado. Clique no icone A para inserir instancias.")

    def _apply_agent_data_to_genetic_editor(self, data: dict, fallback_name: str):
        """Carrega a genetica do agente no editor sem aplicar nos organismos vivos."""
        def pick(*keys, default=None):
            for key in keys:
                if key in data and data.get(key) not in (None, ''):
                    return data.get(key)
            return default

        def as_float(value, default=0.0):
            try:
                return float(value)
            except Exception:
                return default

        def as_bool(value, default=False):
            if isinstance(value, bool):
                return value
            if value is None:
                return default
            return str(value).strip().lower() in {'1', 'true', 'yes', 'y', 'sim'}

        def set_param(name, value):
            self.params.set(name, value, validate=False)
            if name in self.widgets:
                self._set_widget_value(name, value)

        set_param('agent_template_name', str(pick('agent_name', default=fallback_name) or fallback_name))
        if pick('brain_type', default=None):
            set_param('neural_network_type', str(pick('brain_type', default='mlp') or 'mlp'))
        set_param('bacteria_body_size', as_float(pick('r', default=self.params.get('bacteria_body_size', 9.0)), 9.0))
        set_param('bacteria_initial_energy', as_float(pick('energy', default=self.params.get('bacteria_initial_energy', 100.0)), 100.0))
        set_param('bacteria_death_energy', as_float(pick('energy_death_energy', 'death_energy', default=self.params.get('bacteria_death_energy', 50.0)), 50.0))
        set_param('bacteria_split_energy', as_float(pick('energy_split_energy', 'split_energy', default=self.params.get('bacteria_split_energy', 150.0)), 150.0))
        set_param('bacteria_age_death_enabled', as_bool(pick('energy_age_death_enabled', 'age_death_enabled', default=self.params.get('bacteria_age_death_enabled', False)), False))
        set_param('bacteria_death_age', as_float(pick('energy_death_age', 'death_age', default=self.params.get('bacteria_death_age', 3600.0)), 3600.0))
        set_param('bacteria_corpse_to_food', as_bool(pick('energy_corpse_to_food', 'corpse_to_food', default=self.params.get('bacteria_corpse_to_food', False)), False))
        set_param('bacteria_reproduction_min_age', as_float(pick('energy_reproduction_min_age', 'reproduction_min_age', default=self.params.get('bacteria_reproduction_min_age', 0.0)), 0.0))
        set_param('bacteria_reproduction_cooldown', as_float(pick('energy_reproduction_cooldown', 'reproduction_cooldown', default=self.params.get('bacteria_reproduction_cooldown', 0.0)), 0.0))
        set_param('bacteria_metab_v0_cost', as_float(pick('energy_v0_cost', 'metab_v0_cost', 'energy_loss_idle', default=self.params.get('bacteria_metab_v0_cost', 0.5)), 0.5))
        set_param('bacteria_metab_vmax_cost', as_float(pick('energy_vmax_cost', 'metab_vmax_cost', 'energy_loss_move', default=self.params.get('bacteria_metab_vmax_cost', 8.0)), 8.0))
        set_param('bacteria_energy_cap', as_float(pick('energy_energy_cap', 'energy_cap', default=self.params.get('bacteria_energy_cap', 400.0)), 400.0))
        set_param('bacteria_vision_radius', as_float(pick('sensor_vision_radius', default=self.params.get('bacteria_vision_radius', 120.0)), 120.0))
        set_param('bacteria_retina_count', int(as_float(pick('sensor_retina_count', default=self.params.get('bacteria_retina_count', 18)), 18)))
        set_param('bacteria_retina_fov_degrees', as_float(pick('sensor_fov_degrees', default=self.params.get('bacteria_retina_fov_degrees', 180.0)), 180.0))
        set_param('bacteria_eye_count', int(as_float(pick('sensor_eye_count', default=self.params.get('bacteria_eye_count', 1)), 1)))
        set_param('bacteria_eye_angle_degrees', as_float(pick('sensor_eye_angle_degrees', default=self.params.get('bacteria_eye_angle_degrees', 60.0)), 60.0))
        set_param('bacteria_eye_separation_degrees', as_float(pick('sensor_eye_separation_degrees', default=self.params.get('bacteria_eye_separation_degrees', 45.0)), 45.0))
        set_param('bacteria_retina_see_food', as_bool(pick('sensor_see_food', default=True), True))
        set_param('bacteria_retina_see_bacteria', as_bool(pick('sensor_see_bacteria', default=False), False))
        set_param('bacteria_retina_see_predators', as_bool(pick('sensor_see_predators', default=False), False))
        set_param('bacteria_retina_see_obstacles', as_bool(pick('sensor_see_obstacles', default=False), False))
        set_param('bacteria_retina_see_all', as_bool(pick('sensor_see_all', default=False), False))
        set_param('bacteria_retina_see_through_walls', as_bool(pick('sensor_see_through_walls', default=True), True))

        channels = pick('sensor_channels', default='["d"]')
        if isinstance(channels, str):
            try:
                channels = json.loads(channels)
            except Exception:
                channels = [channels]
        channels = {str(ch).lower() for ch in (channels or ['d'])}
        if not channels:
            channels = {'d'}
        try:
            from .sensors import retina_input_mode_from_channels, normalize_retina_input_mode
            input_mode = normalize_retina_input_mode(pick('sensor_input_mode', default=''))
            if not input_mode or input_mode == 'distance_only' and channels != {'d'}:
                input_mode = retina_input_mode_from_channels(tuple(channels))
            set_param('bacteria_retina_input_mode', input_mode)
        except Exception:
            set_param('bacteria_retina_input_mode', 'distance_only')
        color_channels = {
            ch[0] if len(ch) == 2 and ch.endswith('d') else ch
            for ch in channels
            if ch in {'r', 'g', 'b'} or (len(ch) == 2 and ch.endswith('d'))
        }
        try:
            raw_color_channels = pick('sensor_color_channels', default=None)
            if raw_color_channels:
                color_channels = {str(ch).lower() for ch in json.loads(raw_color_channels)}
        except Exception:
            pass
        for ch in ('r', 'g', 'b'):
            set_param(f'bacteria_retina_channel_{ch}', ch in color_channels)
        self._collect_retina_params_from_widgets('bacteria')
        self._update_retina_input_summary('bacteria')

        set_param('bacteria_max_speed', as_float(pick('locomotion_max_speed', default=self.params.get('bacteria_max_speed', 300.0)), 300.0))
        set_param('bacteria_body_shape', str(pick('locomotion_body_shape', 'body_shape', default=self.params.get('bacteria_body_shape', 'ellipse')) or 'ellipse'))
        set_param('bacteria_movement_mode', str(pick('locomotion_movement_mode', default=self.params.get('bacteria_movement_mode', 'forward')) or 'forward'))
        set_param('bacteria_allow_reverse_locomotion', as_bool(pick('locomotion_allow_reverse', 'locomotion_allow_reverse_locomotion', default=self.params.get('bacteria_allow_reverse_locomotion', False)), False))
        max_turn = as_float(pick('locomotion_max_turn', default=self.params.get('bacteria_max_turn', math.pi)), math.pi)
        self.params.set('bacteria_max_turn', max_turn, validate=False)
        if 'bacteria_max_turn_deg' in self.widgets:
            self._set_widget_value('bacteria_max_turn_deg', math.degrees(max_turn))

        set_param('bacteria_diet_food', as_bool(pick('diet_food', default=True), True))
        set_param('bacteria_diet_agents', as_bool(pick('diet_agents', default=False), False))
        set_param('bacteria_diet_same_label', as_bool(pick('diet_same_label', default=False), False))
        set_param('bacteria_diet_food_efficiency', as_float(pick('diet_food_efficiency', default=1.0), 1.0))
        set_param('bacteria_diet_agent_efficiency', as_float(pick('diet_agent_efficiency', default=0.7), 0.7))

        for key in ('bacteria_mutation_rate', 'bacteria_mutation_strength'):
            value = pick(key, default=None)
            if value is not None:
                set_param(key, as_float(value, self.params.get(key, 0.0)))

        try:
            color = json.loads(data.get('color', 'null'))
            if isinstance(color, (list, tuple)) and len(color) >= 3:
                rgb = (int(color[0]), int(color[1]), int(color[2]))
                set_param('bacteria_color', rgb)
                swatch = getattr(self, '_swatch_bacteria', None)
                if swatch is not None:
                    swatch.setStyleSheet(f"background: rgb({rgb[0]},{rgb[1]},{rgb[2]}); border:1px solid #333; border-radius:4px;")
        except Exception:
            pass

        try:
            sizes = json.loads(data.get('brain_sizes', '[]'))
        except Exception:
            sizes = []
        if isinstance(sizes, list) and len(sizes) >= 2:
            hidden = max(1, min(5, len(sizes) - 2))
            set_param('bacteria_hidden_layers', hidden)
            for idx in range(1, 6):
                neurons = int(sizes[idx]) if idx <= hidden and idx < len(sizes) - 1 else 0
                set_param(f'bacteria_neurons_layer_{idx}', neurons)
            if hasattr(self, '_bacteria_neuron_widgets'):
                for idx, spin in enumerate(self._bacteria_neuron_widgets):
                    spin.setEnabled(idx < hidden)

    # ------------------------------------------------------------------
    # Substrate export/import
    # ------------------------------------------------------------------
    def _get_substrate_dirs(self) -> Tuple[str,str]:
            root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
            base = os.path.join(root_dir, 'substrates')
            manual = os.path.join(base, 'manual_exports')
            auto_root = os.path.join(base, 'autosaves')
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
                          path_override: str | None = None, file_type: str = 'substrate',
                          apply_current_params: bool = True) -> str:
        import time
        prev_paused = bool(self._get_widget_value('paused')) if 'paused' in self.widgets else bool(self.params.get('paused', False))
        self._set_paused_state(True)
        state_lock = None
        try:
            engine = self.engine
            state_lock = getattr(engine, 'state_lock', None)
            if state_lock is not None:
                state_lock.acquire()
            # Manual saves can apply pending widgets; autosaves/recovery stay read-only.
            if apply_current_params:
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
                ext = '.biosim' if file_type == 'biosim' else '.json'
                filename = f"{prefix}_{ts_full}{ext}"
                out_dir = manual_dir
            else:
                date_br = time.strftime('%d.%m.%Y')  # formato para nome da subpasta
                # garante subpasta da data
                auto_dir = os.path.join(auto_root, date_br)
                os.makedirs(auto_dir, exist_ok=True)
                ext = '.biosim' if file_type == 'biosim' else '.json'
                kind = 'simulacao' if file_type == 'biosim' else 'substrato'
                base_pref = f"autosave_{kind}_{date_br}_"
                try:
                    existing = [f for f in os.listdir(auto_dir) if f.startswith(base_pref) and f.endswith(ext)]
                except Exception:
                    existing = []
                seq = 1
                if existing:
                    import re
                    pat = re.compile(rf"^autosave_{kind}_{date_br}_(\d+){re.escape(ext)}$")
                    nums = []
                    for fname in existing:
                        m = pat.match(fname)
                        if m:
                            try: nums.append(int(m.group(1)))
                            except: pass
                    if nums:
                        seq = max(nums) + 1
                filename = f"{base_pref}{seq:02d}{ext}"
                out_dir = auto_dir
            if not path_override:
                path = os.path.join(out_dir, filename)
            params_snapshot = dict(self.params._data)
            ui_snapshot = {k:self._get_widget_value(k) for k in self.widgets.keys()}
            from .random_utils import capture_rng_state
            rng_state = capture_rng_state()
            include_brain_activations = bool(self.params.get('export_substrate_include_brain_activations', False))
            pretty_json = bool(self.params.get('export_substrate_pretty_json', False))
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
                    'energy': getattr(food, 'energy', food.r * food.r),
                    'initial_energy': getattr(food, 'initial_energy', getattr(food, 'energy', food.r * food.r)),
                    'base_radius': getattr(food, 'base_radius', food.r),
                    'kind': getattr(food, 'kind', self.params.get('food_mode', 'instant')),
                    'bite_holes': [list(hole) for hole in (getattr(food, 'bite_holes', []) or [])],
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
                    'type': 'predator' if getattr(agent,'is_predator', False) else 'organism',
                    'agent_name': getattr(agent, 'agent_name', None) or self.params.get('agent_template_name', 'organismo_1'),
                    'x': agent.x,'y': agent.y,'r': agent.r,'angle': agent.angle,'vx': agent.vx,'vy': agent.vy,
                    'angular_velocity': getattr(agent, 'angular_velocity', 0.0),
                    'energy': getattr(agent,'energy',0.0),'age': getattr(agent,'age',0.0),
                    'last_reproduction_age': getattr(agent, 'last_reproduction_age', None),
                    'food_eaten_count': getattr(agent, 'food_eaten_count', 0),
                    'food_energy_eaten_total': getattr(agent, 'food_energy_eaten_total', 0.0),
                    'prey_eaten_count': getattr(agent, 'prey_eaten_count', 0),
                    'prey_energy_eaten_total': getattr(agent, 'prey_energy_eaten_total', 0.0),
                    'label_ids': sorted(int(v) for v in (getattr(agent, 'label_ids', set()) or set())),
                }
                try:
                    ad['color'] = list(getattr(agent, 'color', (220, 220, 220)))
                except Exception:
                    pass
                if brain and hasattr(brain,'sizes'):
                    try:
                        from .brain import brain_to_data
                        brain_data = brain_to_data(brain)
                        for extra_key, extra_value in brain_data.items():
                            if extra_key in {'brain_sizes', 'brain_weights', 'brain_biases', 'brain_version'}:
                                continue
                            ad[extra_key] = extra_value
                    except Exception:
                        pass
                    ad['brain_sizes'] = list(brain.sizes); ad['brain_version'] = getattr(brain,'version',0)
                    try:
                        ad['brain_weights'] = [w.tolist() if hasattr(w,'tolist') else list(w) for w in brain.weights]
                        ad['brain_biases'] = [b.tolist() if hasattr(b,'tolist') else list(b) for b in brain.biases]
                    except Exception:
                        ad['brain_weights'] = [list(w) for w in brain.weights]; ad['brain_biases'] = [list(b) for b in brain.biases]
                if sensor:
                    for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators','see_obstacles','see_all','see_through_walls','eye_count','eye_angle_degrees','eye_separation_degrees']:
                        if hasattr(sensor, attr): ad[f'sensor_{attr}'] = getattr(sensor, attr)
                    if hasattr(sensor, 'channels'):
                        ad['sensor_channels'] = list(getattr(sensor, 'channels', ('d',)))
                for attr in ['diet_food', 'diet_agents', 'diet_same_label', 'diet_food_efficiency', 'diet_agent_efficiency']:
                    if hasattr(agent, attr):
                        ad[attr] = getattr(agent, attr)
                if locomotion:
                    for attr in ['max_speed','max_turn','allow_reverse','movement_mode','body_shape']:
                        if hasattr(locomotion, attr): ad[f'locomotion_{attr}'] = getattr(locomotion, attr)
                if hasattr(agent, 'body_shape'):
                    ad['body_shape'] = getattr(agent, 'body_shape')
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
            selected_agent_indices = []
            for agent in getattr(engine, 'selected_agents', set()) or set():
                try:
                    selected_agent_indices.append(engine.all_agents.index(agent))
                except ValueError:
                    pass
            snapshot = {
                'version':2,'file_type': file_type,'timestamp': ts_full,'params': params_snapshot,'ui_params': ui_snapshot,
                'world': {'width': world.width,'height': world.height,'shape': world.shape,'radius': world.radius},
                'camera': {'x': engine.camera.x,'y': engine.camera.y,'zoom': engine.camera.zoom},
                'simulation': {'total_simulation_time': engine.total_simulation_time},
                'rng_state': rng_state,
                'loaded_agent_prototypes': dict(getattr(engine, 'loaded_agent_prototypes', {})),
                'current_agent_prototype': getattr(engine, 'current_agent_prototype', None),
                'agent_labels': {
                    str(label_id): dict(meta)
                    for label_id, meta in getattr(engine, 'agent_labels', {}).items()
                },
                'next_agent_label_id': getattr(engine, '_next_agent_label_id', 1),
                'selected_agent_index': selected_agent_index,
                'selected_agent_indices': selected_agent_indices,
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
            self._set_paused_state(prev_paused)

    def _import_substrate(self, path: str):
        import math as _m
        def _as_bool(value, default=False):
            if isinstance(value, bool):
                return value
            if value is None:
                return default
            if isinstance(value, str):
                return value.strip().lower() in {'1', 'true', 'yes', 'y', 'sim'}
            return bool(value)
        prev_paused = bool(self._get_widget_value('paused')) if 'paused' in self.widgets else bool(self.params.get('paused', False))
        self._set_paused_state(True)
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
            if hasattr(self.engine, '_sim_time_accumulator'):
                self.engine._sim_time_accumulator = 0.0
                self.engine.simulation_backlog = 0.0
            self.engine.loaded_agent_prototypes = dict(data.get('loaded_agent_prototypes', {}))
            self.engine.current_agent_prototype = data.get('current_agent_prototype')
            raw_labels = data.get('agent_labels', {}) or {}
            self.engine.agent_labels = {}
            for raw_id, meta in raw_labels.items():
                try:
                    label_id = int(raw_id)
                except Exception:
                    continue
                color = meta.get('color', (220, 220, 220))
                self.engine.agent_labels[label_id] = {
                    'id': label_id,
                    'name': meta.get('name', f'Label {label_id}'),
                    'color': tuple(int(c) for c in color[:3]),
                    'show_chart': bool(meta.get('show_chart', True)),
                    'min_limit': int(meta.get('min_limit', 0) or 0),
                    'max_limit': int(meta.get('max_limit', 0) or 0),
                }
            self.engine._next_agent_label_id = max(
                int(data.get('next_agent_label_id', 1) or 1),
                (max(self.engine.agent_labels.keys()) + 1) if self.engine.agent_labels else 1,
            )
            fallback_label_id = self.engine.ensure_default_agent_label()
            tool_state = data.get('tool_state', {})
            if tool_state:
                if hasattr(self.pygame_view, 'active_tool'):
                    self._set_canvas_tool(tool_state.get('active_tool', self.pygame_view.active_tool))
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
            if hasattr(self.engine, 'selected_agents'):
                self.engine.selected_agents.clear()
            if hasattr(self.engine, 'obstacles'):
                self.engine.obstacles.load_dicts(data.get('obstacles', []))
            from .entities import create_random_food, Food
            food_items = data.get('foods') or data.get('food_items')
            if food_items:
                for fd in food_items:
                    food = Food(fd.get('x', 0.0), fd.get('y', 0.0), fd.get('r', 4.5),
                                kind=fd.get('kind', self.params.get('food_mode', 'instant')))
                    food.energy = fd.get('energy', getattr(food, 'energy', food.r * food.r))
                    food.initial_energy = fd.get('initial_energy', max(1e-9, getattr(food, 'energy', food.r * food.r)))
                    food.base_radius = fd.get('base_radius', food.r)
                    food.bite_holes = [
                        (float(h[0]), float(h[1]), float(h[2]))
                        for h in fd.get('bite_holes', []) or []
                        if isinstance(h, (list, tuple)) and len(h) >= 3
                    ]
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
            from .brain import brain_from_data
            from .sensors import RetinaSensor
            from .actuators import Locomotion, EnergyModel
            from .entities import Bacteria, Predator
            for ad in data.get('agents', []):
                sizes = ad.get('brain_sizes') or []
                brain = None
                if sizes:
                    brain = brain_from_data(ad, params=self.params, init_std=0.01)
                raw_channels = ad.get('sensor_channels', ('d',))
                if isinstance(raw_channels, str):
                    try:
                        raw_channels = json.loads(raw_channels)
                    except Exception:
                        raw_channels = [raw_channels]
                sensor = RetinaSensor(
                    retina_count=ad.get('sensor_retina_count',18), vision_radius=ad.get('sensor_vision_radius',120.0),
                    fov_degrees=ad.get('sensor_fov_degrees',180.0), skip=ad.get('sensor_skip',0), see_food=ad.get('sensor_see_food',True),
                    see_bacteria=ad.get('sensor_see_bacteria',False), see_predators=ad.get('sensor_see_predators',False),
                    see_obstacles=ad.get('sensor_see_obstacles', False),
                    see_all=ad.get('sensor_see_all', False),
                    see_through_walls=ad.get('sensor_see_through_walls', True),
                    channels=raw_channels,
                    eye_count=ad.get('sensor_eye_count', 1),
                    eye_angle_degrees=ad.get('sensor_eye_angle_degrees', 60.0),
                    eye_separation_degrees=ad.get('sensor_eye_separation_degrees', 45.0))
                locomotion = Locomotion(
                    max_speed=ad.get('locomotion_max_speed',300.0),
                    max_turn=ad.get('locomotion_max_turn', _m.pi),
                    allow_reverse=_as_bool(ad.get('locomotion_allow_reverse', ad.get('locomotion_allow_reverse_locomotion', False)), False),
                    movement_mode=ad.get('locomotion_movement_mode', 'forward'),
                    body_shape=ad.get('locomotion_body_shape', ad.get('body_shape', 'ellipse')),
                )
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
                    energy_cap=_pick('energy_energy_cap','energy_cap', default=(600.0 if ad.get('type')=='predator' else 400.0)),
                    age_death_enabled=_as_bool(_pick('energy_age_death_enabled','age_death_enabled', default=False), False),
                    death_age=_pick('energy_death_age','death_age', default=3600.0),
                    corpse_to_food=_as_bool(_pick('energy_corpse_to_food','corpse_to_food', default=False), False),
                    reproduction_min_age=_pick('energy_reproduction_min_age','reproduction_min_age', default=0.0),
                    reproduction_cooldown=_pick('energy_reproduction_cooldown','reproduction_cooldown', default=0.0),
                )
                cls = Predator if ad.get('type')=='predator' else Bacteria
                agent = cls(ad.get('x',0.0), ad.get('y',0.0), ad.get('r',9.0), brain, sensor, locomotion, energy_model, ad.get('angle',0.0))
                agent.vx = ad.get('vx',0.0); agent.vy = ad.get('vy',0.0); agent.angular_velocity = ad.get('angular_velocity',0.0); agent.energy = ad.get('energy',0.0); agent.age = ad.get('age',0.0)
                agent.prev_x = agent.x; agent.prev_y = agent.y; agent.prev_angle = agent.angle
                agent.last_reproduction_age = ad.get('last_reproduction_age', getattr(agent, 'last_reproduction_age', None))
                agent.food_eaten_count = int(ad.get('food_eaten_count', 0) or 0)
                agent.food_energy_eaten_total = float(ad.get('food_energy_eaten_total', 0.0) or 0.0)
                agent.prey_eaten_count = int(ad.get('prey_eaten_count', 0) or 0)
                agent.prey_energy_eaten_total = float(ad.get('prey_energy_eaten_total', 0.0) or 0.0)
                agent.diet_food = _as_bool(ad.get('diet_food', None), getattr(agent, 'diet_food', not getattr(agent, 'is_predator', False)))
                agent.diet_agents = _as_bool(ad.get('diet_agents', None), getattr(agent, 'diet_agents', getattr(agent, 'is_predator', False)))
                agent.diet_same_label = _as_bool(ad.get('diet_same_label', None), getattr(agent, 'diet_same_label', False))
                agent.diet_food_efficiency = float(ad.get('diet_food_efficiency', getattr(agent, 'diet_food_efficiency', 1.0)) or 1.0)
                agent.diet_agent_efficiency = float(ad.get('diet_agent_efficiency', getattr(agent, 'diet_agent_efficiency', 0.7)) or 0.7)
                agent.body_shape = getattr(locomotion, 'body_shape', ad.get('body_shape', 'ellipse'))
                agent.agent_name = str(ad.get('agent_name') or self.params.get('agent_template_name', 'organismo_1') or 'organismo_1')
                agent.label_ids = set()
                for value in (ad.get('label_ids', []) or []):
                    try:
                        label_id = int(value)
                    except Exception:
                        continue
                    if label_id in self.engine.agent_labels:
                        agent.label_ids.add(label_id)
                if not agent.label_ids:
                    agent.label_ids = {fallback_label_id}
                    fallback_color = self.engine.agent_labels[fallback_label_id].get('color', agent.color)
                    agent.color = tuple(fallback_color)
                agent.last_brain_output = ad.get('last_brain_output', []); agent.last_brain_activations = ad.get('last_brain_activations', [])
                try:
                    color = ad.get('color')
                    if isinstance(color, (list, tuple)) and len(color) >= 3:
                        agent.color = (int(color[0]), int(color[1]), int(color[2]))
                except Exception:
                    pass
                if agent.label_ids:
                    primary_label = next(iter(agent.label_ids))
                    if primary_label in self.engine.agent_labels:
                        agent.color = tuple(self.engine.agent_labels[primary_label].get('color', agent.color))
                if agent.is_predator: self.engine.entities['predators'].append(agent)
                else: self.engine.entities['bacteria'].append(agent)
                self.engine.all_agents.append(agent)
            selected_idx = data.get('selected_agent_index', None)
            selected_indices = data.get('selected_agent_indices', [])
            if selected_indices:
                selected = []
                for idx in selected_indices:
                    try:
                        selected.append(self.engine.all_agents[int(idx)])
                    except Exception:
                        pass
                self.engine.selected_agents = set(selected)
            if selected_idx is not None:
                try:
                    self.engine.selected_agent = self.engine.all_agents[int(selected_idx)]
                    if not self.engine.selected_agents:
                        self.engine.selected_agents = {self.engine.selected_agent}
                except Exception:
                    self.engine.selected_agent = None
                    self.engine.selected_agents.clear()
            elif self.engine.selected_agents:
                self.engine.selected_agent = next(iter(self.engine.selected_agents), None)
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
            if path.lower().endswith('.biosim'):
                self._current_biosim_path = path
            else:
                self._current_biosim_path = None
            self._update_window_title()
            self._reset_metrics_history()
            if 'labels_table' in self.__dict__:
                self._refresh_labels_list()
            print(f"Substrato importado de {path}")
        finally:
            if state_lock is not None:
                state_lock.release()
            self._set_paused_state(prev_paused)

    # ------------------------------------------------------------------
    # Runtime recovery / shutdown diagnostics
    # ------------------------------------------------------------------
    def _save_recovery_snapshot(self, reason: str) -> str | None:
        if self._recovery_snapshot_saved:
            return None
        if not bool(self.params.get('save_recovery_on_close', True)):
            log_event("RECOVERY_SNAPSHOT_SKIPPED", reason=reason, disabled=True)
            return None
        try:
            root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
            out_dir = os.path.join(root_dir, 'substrates', 'recovery')
            os.makedirs(out_dir, exist_ok=True)
            import time
            path = os.path.join(out_dir, f"recovery_{reason}_{time.strftime('%Y%m%d_%H%M%S')}.biosim")
            saved = self._export_substrate(
                path_override=path,
                file_type='biosim',
                apply_current_params=False,
            )
            self._recovery_snapshot_saved = True
            log_event("RECOVERY_SNAPSHOT_SAVED", reason=reason, path=saved)
            return saved
        except Exception as exc:
            self._log_exception("RECOVERY_SNAPSHOT_ERROR", exc)
            return None

    def closeEvent(self, event):
        log_event(
            "UI_CLOSE_EVENT",
            running=bool(getattr(self.engine, 'running', False)),
            bacteria=len(self.engine.entities.get('bacteria', [])),
            predators=len(self.engine.entities.get('predators', [])),
            foods=len(self.engine.entities.get('foods', [])),
            all_agents=len(getattr(self.engine, 'all_agents', [])),
        )
        try:
            timer = getattr(self, '_ui_params_save_timer', None)
            if timer is not None:
                timer.stop()
            self.save_ui_params(silent=True)
        except Exception as exc:
            self._log_exception("UI_PREFS_SAVE_ON_CLOSE_ERROR", exc)
        self._save_recovery_snapshot('normal_close')
        try:
            if self._auto_export_timer:
                self._auto_export_timer.stop()
            if self._diagnostic_timer:
                self._diagnostic_timer.stop()
            if getattr(self, '_status_timer', None):
                self._status_timer.stop()
            if getattr(self, '_agent_panel_timer', None):
                self._agent_panel_timer.stop()
            if getattr(self, '_metrics_chart_timer', None):
                self._metrics_chart_timer.stop()
        except Exception:
            pass
        try:
            self.pygame_view.stop()
            thread = getattr(self, '_sim_thread', None)
            if thread is not None and thread.is_alive():
                thread.join(timeout=2.0)
            self.pygame_view.cleanup()
        except Exception as exc:
            self._log_exception("UI_CLOSE_CLEANUP_ERROR", exc)
        super().closeEvent(event)

    # ------------------------------------------------------------------
    # Autosave scheduling
    # ------------------------------------------------------------------
    def _is_autosave_enabled(self) -> bool:
        if 'auto_export_substrate' in self.widgets:
            return bool(self._get_widget_value('auto_export_substrate'))
        return bool(self.params.get('auto_export_substrate', False))

    def _autosave_interval_minutes(self) -> float:
        if 'auto_export_interval_minutes' in self.widgets:
            return float(self._get_widget_value('auto_export_interval_minutes') or 10.0)
        return float(self.params.get('auto_export_interval_minutes', 10.0) or 10.0)

    def _on_toggle_auto_export(self):
        active = self._is_autosave_enabled()
        self.params.set('auto_export_substrate', active, validate=False)
        if active:
            self._reschedule_auto_export()
        else:
            if self._auto_export_timer:
                self._auto_export_timer.stop(); self._auto_export_timer = None

    def _reschedule_auto_export(self):
        if not self._is_autosave_enabled():
            return
        if self._auto_export_timer:
            self._auto_export_timer.stop()
        self._schedule_next_auto_export()

    def _schedule_next_auto_export(self, initial: bool=False):
        if not self._is_autosave_enabled():
            return
        minutes = self._autosave_interval_minutes()
        delay_ms = max(1, int(minutes * 60_000))
        if self._auto_export_timer is None:
            self._auto_export_timer = QTimer(self)
            self._auto_export_timer.timeout.connect(self._perform_auto_export)
        self._auto_export_timer.start(delay_ms)
        import datetime as _dt
        next_at = _dt.datetime.now() + _dt.timedelta(milliseconds=delay_ms)
        print(f"[AUTOSAVE] Agendado em {minutes} min (por volta de {next_at.strftime('%H:%M:%S')}).")

    def _perform_auto_export(self):
        if not self._is_autosave_enabled():
            return
        try:
            print("[AUTOSAVE] Iniciando save...")
            path = self._export_substrate(manual=False, file_type='biosim', apply_current_params=False)
            print(f"[AUTOSAVE] Salvo: {path}")
        except Exception as e:
            self._log_exception("[AUTOSAVE] Erro", e)
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
