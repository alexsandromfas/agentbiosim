"""Standalone PyQt6 neural-network panel prototype.

Run with:
    python tests/neural_network_panel_demo.py

This file is intentionally isolated from the simulation engine. It uses
synthetic retina/activation/weight data so the visual design can be evaluated
before integrating a real selected-agent neural viewer into the main UI.
"""
from __future__ import annotations

import math
import random
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from PyQt6.QtCore import QPointF, QRectF, Qt, QTimer
from PyQt6.QtGui import QColor, QFont, QLinearGradient, QPainter, QPainterPath, QPen
from PyQt6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDoubleSpinBox,
    QFrame,
    QGraphicsEllipseItem,
    QGraphicsItem,
    QGraphicsPathItem,
    QGraphicsRectItem,
    QGraphicsScene,
    QGraphicsSimpleTextItem,
    QGraphicsView,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


BACKGROUND = QColor(11, 15, 20)
PANEL = QColor(20, 27, 36)
PANEL_2 = QColor(26, 36, 48)
INK = QColor(224, 234, 244)
MUTED = QColor(130, 147, 164)
CYAN = QColor(61, 202, 232)
GREEN = QColor(95, 211, 151)
RED = QColor(234, 105, 125)
AMBER = QColor(242, 181, 87)
BLUE = QColor(105, 151, 255)
MAGENTA = QColor(210, 115, 242)


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def mix(c1: QColor, c2: QColor, t: float) -> QColor:
    t = clamp(t, 0.0, 1.0)
    return QColor(
        int(lerp(c1.red(), c2.red(), t)),
        int(lerp(c1.green(), c2.green(), t)),
        int(lerp(c1.blue(), c2.blue(), t)),
        int(lerp(c1.alpha(), c2.alpha(), t)),
    )


def value_color(value: float, positive: QColor = CYAN) -> QColor:
    mag = clamp(abs(value), 0.0, 1.0)
    if value >= 0:
        return mix(QColor(45, 56, 70), positive, mag)
    return mix(QColor(45, 56, 70), RED, mag)


def channel_color(channel: str | None) -> QColor:
    if not channel:
        return CYAN
    key = channel.upper().replace(" ", "")
    if key == "D":
        return QColor(242, 247, 255)
    if "R" in key:
        return RED
    if "G" in key:
        return GREEN
    if "B" in key:
        return BLUE
    return CYAN


@dataclass
class VisualConfig:
    retina_count: int = 6
    hidden_layers: int = 3
    layer_size: int = 10
    hidden_sizes: tuple[int, ...] = ()
    output_size: int = 4
    channel_r: bool = True
    channel_g: bool = True
    channel_b: bool = True
    channel_d: bool = True
    input_mode: str = "color_plus_distance"
    show_values: bool = True
    show_weak_weights: bool = False
    dense_layout: str = "fixed"
    animation_speed: float = 1.0

    @property
    def channels(self) -> list[str]:
        colors = []
        if self.channel_r:
            colors.append("R")
        if self.channel_g:
            colors.append("G")
        if self.channel_b:
            colors.append("B")
        if self.input_mode == "distance_only":
            return ["D"] if self.channel_d else ["D"]
        if self.input_mode == "color_weighted":
            weighted = [f"D*{ch}" for ch in colors]
            return weighted or ["D"]
        if self.input_mode == "color_only":
            return colors or ["D"]
        channels = []
        if self.channel_d:
            channels.append("D")
        channels.extend(colors)
        return channels or ["D"]

    @property
    def input_size(self) -> int:
        return max(1, self.retina_count * len(self.channels))

    @property
    def resolved_hidden_sizes(self) -> list[int]:
        sizes = [max(1, int(size)) for size in self.hidden_sizes[: self.hidden_layers]]
        sizes.extend([self.layer_size] * max(0, self.hidden_layers - len(sizes)))
        return sizes

    @property
    def layers(self) -> list[int]:
        return [self.input_size, *self.resolved_hidden_sizes, self.output_size]


class RoundedPanelItem(QGraphicsRectItem):
    def __init__(self, rect: QRectF, color: QColor, border: QColor):
        super().__init__(rect)
        self.setBrush(color)
        self.setPen(QPen(border, 1.2))
        self.setZValue(-10)

    def paint(self, painter: QPainter, option, widget=None):
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        painter.setPen(self.pen())
        painter.setBrush(self.brush())
        painter.drawRoundedRect(self.rect(), 12, 12)


class NeuronItem(QGraphicsEllipseItem):
    def __init__(
        self,
        x: float,
        y: float,
        radius: float,
        label: str,
        role: str,
        channel: str | None = None,
        display_label: str | None = None,
    ):
        super().__init__(-radius, -radius, radius * 2, radius * 2)
        self.setPos(x, y)
        self.radius = radius
        self.label = label
        self.role = role
        self.channel = channel
        self.accent_color = channel_color(channel) if role == "retina" else CYAN
        self.activation = 0.0
        self.setZValue(5)
        self.setAcceptHoverEvents(True)
        self.text = QGraphicsSimpleTextItem(display_label or label, self)
        self.text.setFont(QFont("Segoe UI", 7 if role == "retina" else 7))
        self.text.setBrush(MUTED)
        self.value_text = QGraphicsSimpleTextItem("", self)
        self.value_text.setFont(QFont("Consolas", 6 if role == "retina" else 7))
        self.value_text.setBrush(QColor(190, 212, 226))
        self.value_text.setVisible(True)
        self._position_texts()

    def _position_texts(self):
        label_rect = self.text.boundingRect()
        label_y = self.radius + (5 if self.role == "retina" else 3)
        self.text.setPos(-label_rect.width() / 2, label_y)
        value_rect = self.value_text.boundingRect()
        value_y = -self.radius - (13 if self.role == "retina" else 17)
        self.value_text.setPos(-value_rect.width() / 2, value_y)

    def set_activation(self, value: float):
        self.activation = clamp(value, -1.0, 1.0)
        self.setToolTip(f"{self.role}\n{self.label}\nativacao: {self.activation:+.3f}")
        if self.role == "retina":
            text = f"{self.activation:.2f}"
            if text.startswith("0"):
                text = text[1:]
            elif text.startswith("-0"):
                text = "-" + text[2:]
            self.value_text.setText(text)
        else:
            self.value_text.setText(f"{self.activation:+.2f}")
        self._position_texts()
        self.update()

    def set_show_value(self, enabled: bool):
        self.value_text.setVisible(bool(enabled))

    def paint(self, painter: QPainter, option, widget=None):
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        mag = abs(self.activation)
        base = QColor(40, 52, 67)
        accent = self.accent_color if self.role == "retina" else CYAN
        if self.role == "output":
            accent = AMBER
        if self.activation < 0 and self.role != "retina":
            accent = RED
        fill = mix(base, accent, clamp(mag, 0.0, 1.0))
        glow = QColor(accent)
        glow.setAlpha(int(40 + 150 * mag))
        painter.setPen(QPen(glow, 4.0 + 5.0 * mag))
        painter.setBrush(fill)
        painter.drawEllipse(self.rect())
        painter.setPen(QPen(QColor(220, 236, 247, int(120 + 110 * mag)), 1.4))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(self.rect().adjusted(2, 2, -2, -2))


class WeightItem(QGraphicsPathItem):
    def __init__(self, source: NeuronItem, target: NeuronItem, weight: float):
        super().__init__()
        self.source = source
        self.target = target
        self.weight = weight
        self.setZValue(1)
        self.setAcceptHoverEvents(True)
        self.setToolTip(f"peso: {weight:+.4f}")
        self.update_path()

    def update_path(self):
        s = self.source.scenePos()
        t = self.target.scenePos()
        path = QPainterPath(QPointF(s.x(), s.y() + self.source.radius))
        mid_y = (s.y() + t.y()) * 0.5
        path.cubicTo(QPointF(s.x(), mid_y), QPointF(t.x(), mid_y), QPointF(t.x(), t.y() - self.target.radius))
        self.setPath(path)

    def set_strength(self, source_activation: float):
        active = clamp(abs(source_activation), 0.0, 1.0)
        mag = clamp(abs(self.weight), 0.0, 1.0)
        alpha = int(28 + 170 * active * mag)
        color = GREEN if self.weight >= 0 else RED
        color = QColor(color.red(), color.green(), color.blue(), alpha)
        self.setPen(QPen(color, 0.35 + 2.6 * mag * max(0.35, active), Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))


class NeuralNetworkView(QGraphicsView):
    def __init__(self):
        super().__init__()
        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)
        self.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        self.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)
        self.setFrameShape(QFrame.Shape.NoFrame)
        self.setBackgroundBrush(BACKGROUND)
        self.setDragMode(QGraphicsView.DragMode.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.config = VisualConfig()
        self.neuron_layers: list[list[NeuronItem]] = []
        self.weights: list[WeightItem] = []
        self.rng = random.Random(42)
        self.t = 0.0
        self._build_scene()

    def wheelEvent(self, event):
        factor = 1.12 if event.angleDelta().y() > 0 else 1 / 1.12
        self.scale(factor, factor)

    def set_config(self, config: VisualConfig):
        self.config = config
        self._build_scene()

    @staticmethod
    def _retina_gaps() -> tuple[float, float]:
        return 20.0, 35.0

    @staticmethod
    def _dense_gap() -> float:
        return 60.0

    def _retina_group_span(self) -> float:
        channel_gap, group_gap = self._retina_gaps()
        channel_count = max(1, len(self.config.channels))
        retina_count = max(1, self.config.retina_count)
        group_width = max(32.0, (channel_count - 1) * channel_gap)
        return retina_count * group_width + max(0, retina_count - 1) * group_gap

    def _dense_span(self) -> float:
        layer_counts = [self.config.output_size, *self.config.resolved_hidden_sizes]
        return max(0, max(layer_counts, default=1) - 1) * self._dense_gap()

    def _inner_panel_width(self) -> float:
        # Leave a compact title pocket on the left and neuron padding on both sides.
        retina_width = self._retina_group_span() + 196.0
        dense_width = self._dense_span() + 196.0
        return max(620.0, retina_width, dense_width)

    def _scene_width(self) -> float:
        # 30 px between the main shell and the widest inner layer panel, plus shell inset.
        return self._inner_panel_width() + 96.0

    def _layer_y(self, index: int, total: int) -> float:
        top = 155.0
        bottom = 830.0
        if total <= 1:
            return (top + bottom) * 0.5
        return top + (bottom - top) * index / (total - 1)

    def _retina_layout(self, left: float, right: float) -> tuple[list[tuple[int, float, float]], list[tuple[float, str, int]]]:
        channels = self.config.channels
        channel_count = max(1, len(channels))
        retina_count = max(1, self.config.retina_count)
        channel_gap, group_gap = self._retina_gaps()

        channel_span = (channel_count - 1) * channel_gap
        group_width = max(32.0, channel_span)
        total_width = retina_count * group_width + (retina_count - 1) * group_gap
        available = max(1.0, right - left)
        if total_width > available:
            scale = available / total_width
            channel_gap = max(22.0, channel_gap * scale)
            group_gap = max(36.0, group_gap * scale)
            channel_span = (channel_count - 1) * channel_gap
            group_width = max(24.0, channel_span)
            total_width = retina_count * group_width + (retina_count - 1) * group_gap

        start = (left + right - total_width) * 0.5
        groups: list[tuple[int, float, float]] = []
        positions: list[tuple[float, str, int]] = []
        for retina_idx in range(retina_count):
            group_left = start + retina_idx * (group_width + group_gap)
            group_center = group_left + group_width * 0.5
            first_x = group_center - channel_span * 0.5
            groups.append((retina_idx, group_left, group_width))
            for channel_idx, channel in enumerate(channels):
                positions.append((first_x + channel_idx * channel_gap, channel, retina_idx))
        return groups, positions

    def _dense_layer_positions(self, count: int, left: float, right: float) -> list[float]:
        gap = self._dense_gap()
        available = max(1.0, right - left)
        if count <= 1:
            return [(left + right) * 0.5]
        if self.config.dense_layout == "spread":
            return [left + available * i / (count - 1) for i in range(count)]
        gap = min(gap, available / (count - 1))
        span = gap * (count - 1)
        start = (left + right - span) * 0.5
        return [start + gap * i for i in range(count)]

    def _build_scene(self):
        self.scene.clear()
        self.neuron_layers = []
        self.weights = []
        width = self._scene_width()
        height = 910.0
        self.scene.setSceneRect(0, 0, width, height)
        inner_panel_width = self._inner_panel_width()
        inner_panel_left = (width - inner_panel_width) * 0.5
        inner_panel_right = inner_panel_left + inner_panel_width
        content_left = inner_panel_left + 140.0
        content_right = inner_panel_right - 42.0

        gradient = QLinearGradient(0, 0, width, height)
        gradient.setColorAt(0.0, QColor(13, 19, 27))
        gradient.setColorAt(0.55, QColor(9, 14, 20))
        gradient.setColorAt(1.0, QColor(20, 25, 31))
        self.scene.addItem(RoundedPanelItem(QRectF(inner_panel_left - 30, 18, inner_panel_width + 60, height - 36), QColor(14, 20, 28), QColor(46, 62, 78)))

        title = QGraphicsSimpleTextItem("Visualizador Neural - prototipo isolado")
        title.setFont(QFont("Segoe UI Semibold", 18))
        title.setBrush(INK)
        title.setPos((width - title.boundingRect().width()) * 0.5, 34)
        self.scene.addItem(title)
        legend = QGraphicsSimpleTextItem("H2.4 = camada oculta 2 / neuronio 4. Pesos sao as linhas entre neuronios.")
        legend.setFont(QFont("Segoe UI", 8))
        legend.setBrush(QColor(111, 131, 150))
        legend.setPos(44, 72)
        self.scene.addItem(legend)

        channel_text = " / ".join(self.config.channels)
        meta = QGraphicsSimpleTextItem(f"{self.config.retina_count} retinas x {len(self.config.channels)} canais ({channel_text}) = {self.config.input_size} entradas")
        meta.setFont(QFont("Segoe UI", 9))
        meta.setBrush(QColor(154, 184, 204))
        meta.setPos(width - meta.boundingRect().width() - 50, 52)
        self.scene.addItem(meta)

        layers = self.config.layers
        for layer_idx, count in enumerate(layers):
            role = "hidden"
            label_prefix = f"H{layer_idx}"
            radius = 12.0
            if layer_idx == 0:
                role = "retina"
                label_prefix = "I"
                radius = 8.0
            elif layer_idx == len(layers) - 1:
                role = "output"
                label_prefix = "O"
                radius = 14.0
            y = self._layer_y(layer_idx, len(layers))
            panel_height = 58 if role != "retina" else 104
            layer_panel = RoundedPanelItem(QRectF(inner_panel_left, y - panel_height / 2, inner_panel_width, panel_height), PANEL if layer_idx % 2 == 0 else PANEL_2, QColor(40, 55, 70))
            self.scene.addItem(layer_panel)
            label = QGraphicsSimpleTextItem("RETINAS" if role == "retina" else ("SAIDA" if role == "output" else f"CAMADA {layer_idx}"))
            label.setFont(QFont("Segoe UI Semibold", 8))
            label.setBrush(QColor(140, 160, 178))
            label.setPos(58, y - panel_height / 2 + 7)
            self.scene.addItem(label)

            layer_items: list[NeuronItem] = []
            channels = self.config.channels
            if role == "retina":
                label.setPos(inner_panel_left + 16.0, y - panel_height / 2 + 7)
                groups, retina_positions = self._retina_layout(content_left, content_right)
                for retina_idx, group_left, group_width in groups:
                    group_panel = RoundedPanelItem(
                        QRectF(group_left - 16, y - 23, group_width + 32, 58),
                        QColor(18, 28, 38, 210),
                        QColor(54, 73, 91),
                    )
                    group_panel.setZValue(-8)
                    self.scene.addItem(group_panel)
                    group_label = QGraphicsSimpleTextItem(f"R{retina_idx + 1}")
                    group_label.setFont(QFont("Segoe UI Semibold", 7))
                    group_label.setBrush(QColor(154, 177, 195))
                    group_label.setPos(group_left + group_width * 0.5 - group_label.boundingRect().width() * 0.5, y - 43)
                    self.scene.addItem(group_label)
                for x, channel, retina_idx in retina_positions:
                    label_text = f"R{retina_idx + 1}:{channel}"
                    item = NeuronItem(x, y + 5, radius, label_text, role, channel=channel, display_label=channel)
                    item.set_show_value(self.config.show_values)
                    self.scene.addItem(item)
                    layer_items.append(item)
            else:
                label.setPos(inner_panel_left + 16.0, y - panel_height / 2 + 7)
                xs = self._dense_layer_positions(count, content_left, content_right)
                for i, x in enumerate(xs):
                    label_text = f"{label_prefix}.{i + 1}"
                    item = NeuronItem(x, y, radius, label_text, role)
                    item.set_show_value(self.config.show_values)
                    self.scene.addItem(item)
                    layer_items.append(item)
            self.neuron_layers.append(layer_items)

        self._build_weights()
        self.fitInView(self.scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio)

    def _build_weights(self):
        self.rng.seed(12)
        for layer_idx in range(len(self.neuron_layers) - 1):
            source_layer = self.neuron_layers[layer_idx]
            target_layer = self.neuron_layers[layer_idx + 1]
            all_edges = []
            for s in source_layer:
                for t in target_layer:
                    weight = self.rng.uniform(-1.0, 1.0)
                    all_edges.append((abs(weight), s, t, weight))
            if not self.config.show_weak_weights and len(all_edges) > 220:
                all_edges.sort(reverse=True, key=lambda item: item[0])
                all_edges = all_edges[:220]
            for _mag, s, t, weight in all_edges:
                edge = WeightItem(s, t, weight)
                self.scene.addItem(edge)
                self.weights.append(edge)

    def advance_animation(self):
        self.t += 0.075 * max(0.05, self.config.animation_speed)
        for layer_idx, layer in enumerate(self.neuron_layers):
            for neuron_idx, neuron in enumerate(layer):
                phase = self.t + layer_idx * 0.77 + neuron_idx * 0.31
                pulse = math.sin(phase) * 0.55 + math.sin(phase * 0.37 + neuron_idx) * 0.25
                if neuron.role == "retina":
                    pulse = max(0.0, pulse + 0.35 * math.sin(self.t * 2.0 + neuron_idx))
                neuron.set_activation(clamp(pulse, -1.0, 1.0))
        for edge in self.weights:
            edge.set_strength(edge.source.activation)


class ControlPanel(QWidget):
    def __init__(self, view: NeuralNetworkView):
        super().__init__()
        self.view = view
        self.setObjectName("ControlPanel")
        self.setStyleSheet(
            """
            QWidget#ControlPanel { background:#101720; color:#e0eaf4; }
            QGroupBox { border:1px solid #2c3d4e; border-radius:8px; margin-top:14px; padding:10px; font-weight:600; }
            QGroupBox::title { subcontrol-origin: margin; left:10px; padding:0 4px; color:#9fc8df; }
            QLabel { color:#c9d6e2; }
            QSpinBox, QDoubleSpinBox, QComboBox { background:#182331; color:#edf5fb; border:1px solid #31475b; border-radius:6px; padding:4px; }
            QCheckBox { spacing:8px; color:#d8e4ee; }
            QPushButton { background:#295f7f; color:white; border:0; border-radius:8px; padding:8px 10px; font-weight:600; }
            QPushButton:hover { background:#347da5; }
            """
        )
        self._build()

    def _build(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 14, 14, 14)
        layout.setSpacing(10)
        title = QLabel("Controle do Protótipo")
        title.setFont(QFont("Segoe UI Semibold", 15))
        title.setStyleSheet("color:#f0f7ff;")
        layout.addWidget(title)
        note = QLabel("Ajuste arquitetura, canais e modo de entrada para avaliar composição visual.")
        note.setWordWrap(True)
        note.setStyleSheet("color:#8fa6b8;")
        layout.addWidget(note)

        net = QGroupBox("Rede")
        grid = QGridLayout(net)
        self.retinas = self._spin(1, 32, 6)
        self.hidden_layers = self._spin(0, 6, 3)
        self.output_size = self._spin(1, 16, 4)
        self.hidden_size_spins: list[QSpinBox] = []
        self.hidden_sizes_widget = QWidget()
        self.hidden_sizes_layout = QVBoxLayout(self.hidden_sizes_widget)
        self.hidden_sizes_layout.setContentsMargins(0, 0, 0, 0)
        self.hidden_sizes_layout.setSpacing(4)
        self._row(grid, 0, "Retinas", self.retinas)
        self._row(grid, 1, "Camadas ocultas", self.hidden_layers)
        self._row(grid, 2, "Neuronios ocultos", self.hidden_sizes_widget)
        self._row(grid, 3, "Saidas", self.output_size)
        self._rebuild_hidden_size_controls()
        layout.addWidget(net)

        vision = QGroupBox("Entrada visual")
        vgrid = QGridLayout(vision)
        self.input_mode = QComboBox()
        self.input_mode.addItem("Distancia dedicada", "distance_only")
        self.input_mode.addItem("Cor + distancia dedicada", "color_plus_distance")
        self.input_mode.addItem("Distancia multiplicando RGB", "color_weighted")
        self.input_mode.addItem("Somente cor", "color_only")
        self.input_mode.setCurrentIndex(1)
        self._row(vgrid, 0, "Modo", self.input_mode)
        self.chk_r = QCheckBox("R")
        self.chk_g = QCheckBox("G")
        self.chk_b = QCheckBox("B")
        self.chk_d = QCheckBox("D dedicado")
        for chk, checked in ((self.chk_r, True), (self.chk_g, True), (self.chk_b, True), (self.chk_d, True)):
            chk.setChecked(checked)
        row = QHBoxLayout()
        row.addWidget(self.chk_r)
        row.addWidget(self.chk_g)
        row.addWidget(self.chk_b)
        row.addWidget(self.chk_d)
        wrap = QWidget()
        wrap.setLayout(row)
        self._row(vgrid, 1, "Canais", wrap)
        layout.addWidget(vision)

        display = QGroupBox("Desenho")
        dgrid = QGridLayout(display)
        self.show_values = QCheckBox("Valores")
        self.show_values.setChecked(True)
        self.show_weights = QCheckBox("Pesos fracos")
        self.dense_layout = QComboBox()
        self.dense_layout.addItem("Espacamento fixo", "fixed")
        self.dense_layout.addItem("Preencher painel", "spread")
        self.speed = QDoubleSpinBox()
        self.speed.setRange(0.1, 4.0)
        self.speed.setSingleStep(0.1)
        self.speed.setValue(1.0)
        self._row(dgrid, 0, "Mostrar", self.show_values)
        self._row(dgrid, 1, "Conexoes", self.show_weights)
        self._row(dgrid, 2, "Neuronios", self.dense_layout)
        self._row(dgrid, 3, "Blink", self.speed)
        layout.addWidget(display)

        apply_btn = QPushButton("Aplicar visual")
        apply_btn.clicked.connect(self.apply)
        layout.addWidget(apply_btn)
        layout.addStretch(1)

        for widget in (self.retinas, self.output_size):
            widget.valueChanged.connect(lambda _value=0: self.apply())
        self.hidden_layers.valueChanged.connect(self._hidden_layer_count_changed)
        self.speed.valueChanged.connect(lambda _value=0.0: self.apply())
        self.input_mode.currentIndexChanged.connect(lambda _index=0: self.apply())
        self.dense_layout.currentIndexChanged.connect(lambda _index=0: self.apply())
        for widget in (self.chk_r, self.chk_g, self.chk_b, self.chk_d, self.show_values, self.show_weights):
            widget.toggled.connect(lambda _checked=False: self.apply())

    @staticmethod
    def _spin(lo: int, hi: int, value: int) -> QSpinBox:
        spin = QSpinBox()
        spin.setRange(lo, hi)
        spin.setValue(value)
        return spin

    @staticmethod
    def _row(grid: QGridLayout, row: int, label: str, widget: QWidget):
        grid.addWidget(QLabel(label), row, 0)
        grid.addWidget(widget, row, 1)

    def _hidden_layer_count_changed(self, _value: int):
        self._rebuild_hidden_size_controls()
        self.apply()

    def _rebuild_hidden_size_controls(self):
        old_values = [spin.value() for spin in getattr(self, "hidden_size_spins", [])]
        while self.hidden_sizes_layout.count():
            item = self.hidden_sizes_layout.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()
        self.hidden_size_spins = []

        if self.hidden_layers.value() <= 0:
            empty = QLabel("Sem camadas")
            empty.setStyleSheet("color:#8fa6b8;")
            self.hidden_sizes_layout.addWidget(empty)
            return

        for index in range(self.hidden_layers.value()):
            row = QWidget()
            row_layout = QHBoxLayout(row)
            row_layout.setContentsMargins(0, 0, 0, 0)
            row_layout.setSpacing(5)
            row_layout.addWidget(QLabel(f"H{index + 1}"))
            spin = self._spin(1, 48, old_values[index] if index < len(old_values) else 10)
            spin.valueChanged.connect(lambda _value=0: self.apply())
            row_layout.addWidget(spin, 1)
            self.hidden_sizes_layout.addWidget(row)
            self.hidden_size_spins.append(spin)

    def apply(self):
        hidden_sizes = tuple(spin.value() for spin in self.hidden_size_spins)
        cfg = VisualConfig(
            retina_count=int(self.retinas.value()),
            hidden_layers=int(self.hidden_layers.value()),
            layer_size=int(hidden_sizes[0]) if hidden_sizes else 10,
            hidden_sizes=hidden_sizes,
            output_size=int(self.output_size.value()),
            channel_r=self.chk_r.isChecked(),
            channel_g=self.chk_g.isChecked(),
            channel_b=self.chk_b.isChecked(),
            channel_d=self.chk_d.isChecked(),
            input_mode=str(self.input_mode.currentData()),
            show_values=self.show_values.isChecked(),
            show_weak_weights=self.show_weights.isChecked(),
            dense_layout=str(self.dense_layout.currentData()),
            animation_speed=float(self.speed.value()),
        )
        self.chk_d.setEnabled(cfg.input_mode == "color_plus_distance" or cfg.input_mode == "distance_only")
        self.view.set_config(cfg)


class DemoWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AgentBioSim - prototipo visualizador neural")
        self.resize(1320, 900)
        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        self.view = NeuralNetworkView()
        self.controls = ControlPanel(self.view)
        self.controls.setFixedWidth(310)
        layout.addWidget(self.controls)
        layout.addWidget(self.view, 1)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.view.advance_animation)
        self.timer.start(80)


def main() -> int:
    app = QApplication.instance() or QApplication(sys.argv)
    app.setApplicationName("AgentBioSim Neural Viewer Demo")
    window = DemoWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
