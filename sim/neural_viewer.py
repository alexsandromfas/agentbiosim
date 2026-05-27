"""Qt neural-network viewer for the currently selected AgentBioSim organism."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Iterable

from PyQt6.QtCore import QPointF, QRectF, Qt
from PyQt6.QtGui import QColor, QFont, QPainter, QPainterPath, QPen
from PyQt6.QtWidgets import (
    QFrame,
    QGraphicsEllipseItem,
    QGraphicsPathItem,
    QGraphicsRectItem,
    QGraphicsScene,
    QGraphicsSimpleTextItem,
    QGraphicsView,
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


def _clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def _mix(c1: QColor, c2: QColor, t: float) -> QColor:
    t = _clamp(t, 0.0, 1.0)
    return QColor(
        int(c1.red() + (c2.red() - c1.red()) * t),
        int(c1.green() + (c2.green() - c1.green()) * t),
        int(c1.blue() + (c2.blue() - c1.blue()) * t),
        int(c1.alpha() + (c2.alpha() - c1.alpha()) * t),
    )


def _flatten(values: Any) -> list[float]:
    if hasattr(values, "tolist"):
        values = values.tolist()
    if isinstance(values, (int, float)):
        return [float(values)]
    if not isinstance(values, (list, tuple)):
        return []
    out: list[float] = []
    for value in values:
        out.extend(_flatten(value))
    return out


def _channel_label(channel: Any) -> str:
    mapping = {
        "d": "D",
        "r": "R",
        "g": "G",
        "b": "B",
        "rd": "D*R",
        "gd": "D*G",
        "bd": "D*B",
        "dr": "D*R",
        "dg": "D*G",
        "db": "D*B",
    }
    return mapping.get(str(channel).lower().replace("*", ""), str(channel).upper())


def _channel_sort_key(raw_channel: str) -> tuple[int, str]:
    raw = str(raw_channel).lower().replace("*", "")
    order = {"d": 0, "r": 1, "rd": 1, "dr": 1, "g": 2, "gd": 2, "dg": 2, "b": 3, "bd": 3, "db": 3}
    return order.get(raw, 99), raw


def _channel_color(channel: str | None) -> QColor:
    key = str(channel or "").upper().replace(" ", "")
    if key == "D":
        return QColor(242, 247, 255)
    if "R" in key:
        return RED
    if "G" in key:
        return GREEN
    if "B" in key:
        return BLUE
    return CYAN


@dataclass(frozen=True)
class ChannelSlot:
    raw: str
    label: str
    source_offset: int


@dataclass(frozen=True)
class NeuralViewConfig:
    layer_sizes: tuple[int, ...]
    retina_groups: int
    channel_slots: tuple[ChannelSlot, ...]
    dense_layout: str = "fixed"
    brain_type: str = "mlp"
    brain_label: str = "MLP padrao"
    neat_layer_node_ids: tuple[tuple[int, ...], ...] = ()

    @classmethod
    def from_agent(cls, agent: Any, dense_layout: str = "fixed") -> "NeuralViewConfig":
        brain = getattr(agent, "brain", None)
        brain_type = str(getattr(brain, "brain_type", "mlp") or "mlp")
        brain_label = str(getattr(brain, "display_name", brain_type) or brain_type)
        sizes = tuple(max(1, int(size)) for size in (getattr(brain, "sizes", []) or []))
        if len(sizes) < 2:
            sizes = (1, 1)
        sensor = getattr(agent, "sensor", None)
        raw_channels = tuple(str(channel).lower() for channel in (getattr(sensor, "channels", ("d",)) or ("d",)))
        indexed = list(enumerate(raw_channels))
        indexed.sort(key=lambda item: _channel_sort_key(item[1]))
        slots = tuple(ChannelSlot(raw=raw, label=_channel_label(raw), source_offset=index) for index, raw in indexed)
        if not slots:
            slots = (ChannelSlot(raw="d", label="D", source_offset=0),)

        eye_count = max(1, int(getattr(sensor, "eye_count", 1) or 1)) if sensor is not None else 1
        ray_count = max(1, int(getattr(sensor, "retina_count", 1) or 1)) * eye_count if sensor is not None else sizes[0]
        if ray_count * len(slots) != sizes[0]:
            if len(slots) > 0 and sizes[0] % len(slots) == 0:
                ray_count = max(1, sizes[0] // len(slots))
            else:
                ray_count = sizes[0]
                slots = (ChannelSlot(raw="input", label="I", source_offset=0),)
        neat_layer_node_ids: tuple[tuple[int, ...], ...] = ()
        if brain_type.startswith("neat") and hasattr(brain, "nodes"):
            try:
                nodes = [dict(node) for node in getattr(brain, "nodes", [])]
                inputs = sorted([n for n in nodes if n.get("kind") == "input"], key=lambda n: int(n.get("id", 0)))
                outputs = sorted([n for n in nodes if n.get("kind") == "output"], key=lambda n: int(n.get("id", 0)))
                hidden_by_layer: dict[float, list[dict]] = {}
                for node in nodes:
                    if node.get("kind") == "hidden":
                        hidden_by_layer.setdefault(float(node.get("layer", 0.5)), []).append(node)
                layers = [tuple(int(n.get("id", 0)) for n in inputs)]
                for layer in sorted(hidden_by_layer):
                    layers.append(tuple(int(n.get("id", 0)) for n in sorted(hidden_by_layer[layer], key=lambda n: int(n.get("id", 0)))))
                layers.append(tuple(int(n.get("id", 0)) for n in outputs))
                neat_layer_node_ids = tuple(layer for layer in layers if layer)
                if len(neat_layer_node_ids) >= 2:
                    sizes = tuple(len(layer) for layer in neat_layer_node_ids)
            except Exception:
                neat_layer_node_ids = ()
        return cls(
            layer_sizes=sizes,
            retina_groups=ray_count,
            channel_slots=slots,
            dense_layout=_dense_layout(dense_layout),
            brain_type=brain_type,
            brain_label=brain_label,
            neat_layer_node_ids=neat_layer_node_ids,
        )

    @property
    def input_size(self) -> int:
        return self.layer_sizes[0]


def _dense_layout(value: Any) -> str:
    return "spread" if str(value).lower() == "spread" else "fixed"


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
        value_index: int,
        display_label: str | None = None,
        channel: str | None = None,
    ):
        super().__init__(-radius, -radius, radius * 2, radius * 2)
        self.setPos(x, y)
        self.radius = radius
        self.label = label
        self.role = role
        self.value_index = int(value_index)
        self.channel = channel
        self.activation = 0.0
        self.gate_value: float | None = None
        self.accent_color = _channel_color(channel) if role == "retina" else CYAN
        self.setZValue(5)
        self.setAcceptHoverEvents(True)

        self.text = QGraphicsSimpleTextItem(display_label or label, self)
        self.text.setFont(QFont("Segoe UI", 7))
        self.text.setBrush(MUTED)
        self.value_text = QGraphicsSimpleTextItem("", self)
        self.value_text.setFont(QFont("Consolas", 6 if role == "retina" else 7))
        self.value_text.setBrush(QColor(190, 212, 226))
        self._position_texts()

    def _position_texts(self):
        text_rect = self.text.boundingRect()
        self.text.setPos(-text_rect.width() * 0.5, self.radius + (5 if self.role == "retina" else 3))
        value_rect = self.value_text.boundingRect()
        self.value_text.setPos(-value_rect.width() * 0.5, -self.radius - (22 if self.role == "retina" else 17))

    def set_activation(self, value: float):
        self.activation = _clamp(float(value), -1.0, 1.0)
        self.setToolTip(f"{self.label}\nativacao: {self.activation:+.4f}")
        if self.role == "retina":
            text = f"{self.activation:.2f}"
            if text.startswith("0"):
                text = text[1:]
            elif text.startswith("-0"):
                text = "-" + text[2:]
        else:
            text = f"{self.activation:+.2f}"
        self.value_text.setText(text)
        self._position_texts()
        self.update()

    def set_gate(self, value: float | None):
        self.gate_value = None if value is None else float(value)
        self.update()

    def paint(self, painter: QPainter, option, widget=None):
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        mag = abs(self.activation)
        accent = self.accent_color if self.role == "retina" else CYAN
        if self.role == "output":
            accent = AMBER
        if self.activation < 0 and self.role != "retina":
            accent = RED
        glow = QColor(accent)
        glow.setAlpha(int(40 + 150 * mag))
        painter.setPen(QPen(glow, 4.0 + 5.0 * mag))
        painter.setBrush(_mix(QColor(40, 52, 67), accent, mag))
        painter.drawEllipse(self.rect())
        painter.setPen(QPen(QColor(220, 236, 247, int(120 + 110 * mag)), 1.4))
        painter.setBrush(Qt.BrushStyle.NoBrush)
        painter.drawEllipse(self.rect().adjusted(2, 2, -2, -2))
        if self.gate_value is not None and self.role == "hidden":
            gate_mag = _clamp(float(self.gate_value) / 2.0, 0.0, 1.0)
            gate_color = _mix(RED, GREEN, gate_mag)
            gate_color.setAlpha(170)
            painter.setPen(QPen(gate_color, 2.2))
            painter.drawEllipse(self.rect().adjusted(-4, -4, 4, 4))


class WeightItem(QGraphicsPathItem):
    def __init__(self, source: NeuronItem, target: NeuronItem, weight: float):
        super().__init__()
        self.source = source
        self.target = target
        self.weight = float(weight)
        self.setZValue(1)
        self.setAcceptHoverEvents(True)
        self.setToolTip(f"peso: {self.weight:+.4f}")
        self._update_path()
        self.update_activity()

    def _update_path(self):
        s = self.source.scenePos()
        t = self.target.scenePos()
        path = QPainterPath(QPointF(s.x(), s.y() + self.source.radius))
        mid_y = (s.y() + t.y()) * 0.5
        path.cubicTo(QPointF(s.x(), mid_y), QPointF(t.x(), mid_y), QPointF(t.x(), t.y() - self.target.radius))
        self.setPath(path)

    def update_activity(self):
        active = _clamp(abs(self.source.activation), 0.0, 1.0)
        mag = _clamp(abs(self.weight), 0.0, 1.0)
        color = GREEN if self.weight >= 0 else RED
        color = QColor(color.red(), color.green(), color.blue(), int(28 + 170 * active * mag))
        self.setPen(QPen(color, 0.35 + 2.6 * mag * max(0.35, active), Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))


class AgentNeuralNetworkView(QGraphicsView):
    """Visualize the selected agent brain without owning any simulation state."""

    def __init__(self, dense_layout: str = "fixed"):
        super().__init__()
        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)
        self.setRenderHint(QPainter.RenderHint.Antialiasing, True)
        self.setRenderHint(QPainter.RenderHint.TextAntialiasing, True)
        self.setFrameShape(QFrame.Shape.NoFrame)
        self.setBackgroundBrush(BACKGROUND)
        self.setDragMode(QGraphicsView.DragMode.ScrollHandDrag)
        self.setTransformationAnchor(QGraphicsView.ViewportAnchor.AnchorUnderMouse)
        self.dense_layout = _dense_layout(dense_layout)
        self.config = NeuralViewConfig(layer_sizes=(1, 1), retina_groups=1, channel_slots=(ChannelSlot("d", "D", 0),))
        self.neuron_layers: list[list[NeuronItem]] = []
        self.weight_items: list[WeightItem] = []
        self._signature: tuple[Any, ...] | None = None
        self._fit_pending = True
        self._show_placeholder("Selecione um organismo para inspecionar a rede.")

    def wheelEvent(self, event):
        factor = 1.12 if event.angleDelta().y() > 0 else 1 / 1.12
        self.scale(factor, factor)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self._fit_pending and not self.scene.sceneRect().isEmpty():
            self.fitInView(self.scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio)
            self._fit_pending = False

    def set_dense_layout(self, layout: str):
        layout = _dense_layout(layout)
        if layout != self.dense_layout:
            self.dense_layout = layout
            self._signature = None

    def set_agent(self, agent: Any):
        if agent is None:
            if self._signature is not None:
                self._signature = None
                self._show_placeholder("Selecione um organismo para inspecionar a rede.")
            return
        config = NeuralViewConfig.from_agent(agent, self.dense_layout)
        brain = getattr(agent, "brain", None)
        signature = (
            id(agent),
            id(brain),
            tuple(config.layer_sizes),
            config.retina_groups,
            tuple((slot.raw, slot.source_offset) for slot in config.channel_slots),
            config.brain_type,
            int(getattr(brain, "version", 0) or 0),
            config.dense_layout,
        )
        if signature != self._signature:
            self.config = config
            self._build_scene(agent)
            self._signature = signature
        self._update_activations(agent)

    def _show_placeholder(self, text: str):
        self.scene.clear()
        self.scene.setSceneRect(0, 0, 760, 780)
        self.scene.addItem(RoundedPanelItem(QRectF(18, 18, 724, 744), QColor(14, 20, 28), QColor(46, 62, 78)))
        title = QGraphicsSimpleTextItem("Visualizador Neural")
        title.setFont(QFont("Segoe UI Semibold", 18))
        title.setBrush(INK)
        title.setPos((760 - title.boundingRect().width()) * 0.5, 36)
        self.scene.addItem(title)
        hint = QGraphicsSimpleTextItem(text)
        hint.setFont(QFont("Segoe UI", 11))
        hint.setBrush(MUTED)
        hint.setPos((760 - hint.boundingRect().width()) * 0.5, 370)
        self.scene.addItem(hint)
        self.neuron_layers = []
        self.weight_items = []
        self._fit_pending = True

    @staticmethod
    def _retina_gaps() -> tuple[float, float]:
        return 20.0, 35.0

    @staticmethod
    def _dense_gap() -> float:
        return 60.0

    def _retina_span(self) -> float:
        channel_gap, group_gap = self._retina_gaps()
        group_width = max(32.0, (max(1, len(self.config.channel_slots)) - 1) * channel_gap)
        return self.config.retina_groups * group_width + max(0, self.config.retina_groups - 1) * group_gap

    def _dense_span(self) -> float:
        return max(0, max(self.config.layer_sizes[1:], default=1) - 1) * self._dense_gap()

    def _inner_width(self) -> float:
        return max(620.0, self._retina_span() + 196.0, self._dense_span() + 196.0)

    def _scene_width(self) -> float:
        return self._inner_width() + 96.0

    @staticmethod
    def _layer_y(index: int, total: int) -> float:
        top, bottom = 155.0, 830.0
        return (top + bottom) * 0.5 if total <= 1 else top + (bottom - top) * index / (total - 1)

    def _retina_positions(self, left: float, right: float) -> tuple[list[tuple[int, float, float]], list[tuple[float, ChannelSlot, int, int]]]:
        slots = self.config.channel_slots
        channel_gap, group_gap = self._retina_gaps()
        channel_span = max(0, len(slots) - 1) * channel_gap
        group_width = max(32.0, channel_span)
        total_width = self.config.retina_groups * group_width + max(0, self.config.retina_groups - 1) * group_gap
        available = max(1.0, right - left)
        if total_width > available:
            scale = available / total_width
            channel_gap = max(16.0, channel_gap * scale)
            group_gap = max(22.0, group_gap * scale)
            channel_span = max(0, len(slots) - 1) * channel_gap
            group_width = max(24.0, channel_span)
            total_width = self.config.retina_groups * group_width + max(0, self.config.retina_groups - 1) * group_gap
        start = (left + right - total_width) * 0.5
        groups: list[tuple[int, float, float]] = []
        positions: list[tuple[float, ChannelSlot, int, int]] = []
        input_stride = max(1, len(slots))
        for group_index in range(self.config.retina_groups):
            group_left = start + group_index * (group_width + group_gap)
            first_x = group_left + group_width * 0.5 - channel_span * 0.5
            groups.append((group_index, group_left, group_width))
            for display_index, slot in enumerate(slots):
                source_index = group_index * input_stride + slot.source_offset
                positions.append((first_x + display_index * channel_gap, slot, group_index, source_index))
        return groups, positions

    def _dense_positions(self, count: int, left: float, right: float) -> list[float]:
        if count <= 1:
            return [(left + right) * 0.5]
        available = max(1.0, right - left)
        if self.config.dense_layout == "spread":
            return [left + available * i / (count - 1) for i in range(count)]
        gap = min(self._dense_gap(), available / (count - 1))
        start = (left + right - gap * (count - 1)) * 0.5
        return [start + gap * i for i in range(count)]

    def _build_scene(self, agent: Any):
        self.scene.clear()
        self.neuron_layers = []
        self.weight_items = []
        width, height = self._scene_width(), 910.0
        self.scene.setSceneRect(0, 0, width, height)
        inner_width = self._inner_width()
        inner_left = (width - inner_width) * 0.5
        inner_right = inner_left + inner_width
        content_left = inner_left + 140.0
        content_right = inner_right - 42.0
        self.scene.addItem(RoundedPanelItem(QRectF(inner_left - 30, 18, inner_width + 60, height - 36), QColor(14, 20, 28), QColor(46, 62, 78)))

        title = QGraphicsSimpleTextItem("Visualizador Neural")
        title.setFont(QFont("Segoe UI Semibold", 18))
        title.setBrush(INK)
        title.setPos((width - title.boundingRect().width()) * 0.5, 34)
        self.scene.addItem(title)
        legend = QGraphicsSimpleTextItem("H2.4 = camada oculta 2 / neuronio 4. Pesos sao as linhas entre neuronios.")
        legend.setFont(QFont("Segoe UI", 8))
        legend.setBrush(QColor(111, 131, 150))
        legend.setPos(inner_left + 4, 72)
        self.scene.addItem(legend)
        channel_text = " / ".join(slot.label for slot in self.config.channel_slots)
        meta = QGraphicsSimpleTextItem(f"{self.config.brain_label} | {self.config.retina_groups} retinas x {len(self.config.channel_slots)} canais ({channel_text}) = {self.config.input_size} entradas")
        meta.setFont(QFont("Segoe UI", 9))
        meta.setBrush(QColor(154, 184, 204))
        meta.setPos(width - meta.boundingRect().width() - 50, 52)
        self.scene.addItem(meta)

        for layer_index, count in enumerate(self.config.layer_sizes):
            role, prefix, radius = "hidden", f"H{layer_index}", 12.0
            if layer_index == 0:
                role, prefix, radius = "retina", "I", 8.0
            elif layer_index == len(self.config.layer_sizes) - 1:
                role, prefix, radius = "output", "O", 14.0
            y = self._layer_y(layer_index, len(self.config.layer_sizes))
            height_px = 104 if role == "retina" else 58
            self.scene.addItem(RoundedPanelItem(QRectF(inner_left, y - height_px * 0.5, inner_width, height_px), PANEL if layer_index % 2 == 0 else PANEL_2, QColor(40, 55, 70)))
            if role == "retina":
                row_title = "RETINAS"
            elif role == "output":
                row_title = "SAIDA"
            elif self.config.brain_type == "simple_rnn" and layer_index == 1:
                row_title = "CAMADA RECORRENTE"
            elif self.config.brain_type.startswith("neat") and role == "hidden":
                row_title = f"NEAT NIVEL {layer_index}"
            else:
                row_title = f"CAMADA {layer_index}"
            label = QGraphicsSimpleTextItem(row_title)
            label.setFont(QFont("Segoe UI Semibold", 8))
            label.setBrush(QColor(140, 160, 178))
            label.setPos(inner_left + 16, y - height_px * 0.5 + 7)
            self.scene.addItem(label)
            layer: list[NeuronItem] = []
            if role == "retina":
                groups, positions = self._retina_positions(content_left, content_right)
                for group_index, group_left, group_width in groups:
                    pane = RoundedPanelItem(QRectF(group_left - 16, y - 23, group_width + 32, 58), QColor(18, 28, 38, 210), QColor(54, 73, 91))
                    pane.setZValue(-8)
                    self.scene.addItem(pane)
                    retina_label = QGraphicsSimpleTextItem(f"R{group_index + 1}")
                    retina_label.setFont(QFont("Segoe UI Semibold", 7))
                    retina_label.setBrush(QColor(154, 177, 195))
                    retina_label.setPos(group_left + group_width * 0.5 - retina_label.boundingRect().width() * 0.5, y - 43)
                    self.scene.addItem(retina_label)
                for x, slot, group_index, source_index in positions:
                    layer.append(NeuronItem(x, y, radius, f"R{group_index + 1}:{slot.label}", role, source_index, slot.label, slot.label))
            else:
                neat_ids = self.config.neat_layer_node_ids[layer_index] if layer_index < len(self.config.neat_layer_node_ids) else ()
                for index, x in enumerate(self._dense_positions(count, content_left, content_right)):
                    item = NeuronItem(x, y, radius, f"{prefix}.{index + 1}", role, index)
                    if neat_ids and index < len(neat_ids):
                        node_id = int(neat_ids[index])
                        item.node_id = node_id
                        item.setToolTip(f"NEAT node {node_id}")
                        if role == "hidden":
                            item.label = f"N{node_id}"
                        elif role == "output":
                            item.label = f"O{index + 1}"
                    layer.append(item)
                if self.config.brain_type == "simple_rnn" and layer_index == 1:
                    note = QGraphicsSimpleTextItem("memoria curta")
                    note.setFont(QFont("Segoe UI", 7))
                    note.setBrush(QColor(111, 171, 210))
                    note.setPos(inner_left + 16, y + height_px * 0.5 - 20)
                    self.scene.addItem(note)
            for neuron in layer:
                self.scene.addItem(neuron)
            self.neuron_layers.append(layer)
        self._build_weights(getattr(agent, "brain", None))
        self._apply_static_brain_marks(getattr(agent, "brain", None))
        self._fit_pending = True
        self.fitInView(self.scene.sceneRect(), Qt.AspectRatioMode.KeepAspectRatio)

    def _iter_weight_edges(self, brain: Any, layer_index: int) -> Iterable[tuple[float, NeuronItem, NeuronItem, float]]:
        if self.config.brain_type.startswith("neat") and hasattr(brain, "connections"):
            if layer_index != 0:
                return []
            node_to_item = {}
            for layer in self.neuron_layers:
                for neuron in layer:
                    node_id = getattr(neuron, "node_id", None)
                    if node_id is not None:
                        node_to_item[int(node_id)] = neuron
            for neuron in self.neuron_layers[0] if self.neuron_layers else []:
                if 0 <= neuron.value_index < len(self.config.neat_layer_node_ids[0] if self.config.neat_layer_node_ids else ()):
                    node_to_item[int(self.config.neat_layer_node_ids[0][neuron.value_index])] = neuron
            edges = []
            for conn in getattr(brain, "connections", []) or []:
                if not bool(conn.get("enabled", True)):
                    continue
                source = node_to_item.get(int(conn.get("src", -1)))
                target = node_to_item.get(int(conn.get("dst", -1)))
                if source is None or target is None:
                    continue
                weight = float(conn.get("weight", 0.0))
                edges.append((abs(weight), source, target, weight))
            return edges
        weights = getattr(brain, "weights", []) or []
        if layer_index >= len(weights) or layer_index + 1 >= len(self.neuron_layers):
            return []
        matrix = weights[layer_index]
        source_layer = self.neuron_layers[layer_index]
        target_layer = self.neuron_layers[layer_index + 1]
        edges = []
        for source in source_layer:
            for target in target_layer:
                try:
                    weight = float(matrix[target.value_index][source.value_index])
                except Exception:
                    continue
                edges.append((abs(weight), source, target, weight))
        return edges

    def _build_weights(self, brain: Any):
        for layer_index in range(max(0, len(self.neuron_layers) - 1)):
            edges = list(self._iter_weight_edges(brain, layer_index))
            if len(edges) > 220:
                edges.sort(reverse=True, key=lambda item: item[0])
                edges = edges[:220]
            for _mag, source, target, weight in edges:
                edge = WeightItem(source, target, weight)
                self.scene.addItem(edge)
                self.weight_items.append(edge)
        self._build_shortcut_weights(brain)

    def _build_shortcut_weights(self, brain: Any):
        if not hasattr(brain, "shortcut_weights") or len(self.neuron_layers) < 2:
            return
        source_layer = self.neuron_layers[0]
        target_layer = self.neuron_layers[-1]
        matrix = getattr(brain, "shortcut_weights", None)
        edges = []
        for source in source_layer:
            for target in target_layer:
                try:
                    weight = float(matrix[target.value_index][source.value_index])
                except Exception:
                    continue
                edges.append((abs(weight), source, target, weight))
        if len(edges) > 90:
            edges.sort(reverse=True, key=lambda item: item[0])
            edges = edges[:90]
        for _mag, source, target, weight in edges:
            edge = WeightItem(source, target, weight)
            edge.setToolTip(f"atalho entrada->saida: {weight:+.4f}")
            self.scene.addItem(edge)
            self.weight_items.append(edge)

    def _apply_static_brain_marks(self, brain: Any):
        if brain is None:
            return
        gates = getattr(brain, "gates", None)
        if gates:
            for gate_layer_idx, values in enumerate(gates, start=1):
                if gate_layer_idx >= len(self.neuron_layers):
                    break
                layer = self.neuron_layers[gate_layer_idx]
                flat = _flatten(values)
                for neuron in layer:
                    if 0 <= neuron.value_index < len(flat):
                        neuron.set_gate(flat[neuron.value_index])

    def _update_activations(self, agent: Any):
        if not self.neuron_layers:
            return
        sensor = getattr(agent, "sensor", None)
        input_values = _flatten(getattr(sensor, "last_inputs", []))
        activation_layers = [_flatten(layer) for layer in (getattr(agent, "last_brain_activations", []) or [])]
        all_values = [input_values, *activation_layers]
        for layer_index, layer in enumerate(self.neuron_layers):
            values = all_values[layer_index] if layer_index < len(all_values) else []
            for neuron in layer:
                neuron.set_activation(values[neuron.value_index] if 0 <= neuron.value_index < len(values) else 0.0)
        for edge in self.weight_items:
            edge.update_activity()
