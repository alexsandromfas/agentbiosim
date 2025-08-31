"""
Interface PyQt6 completa para controle da simulação (baseada em ui_tk.py).
Inclui simulação Pygame integrada e todos os controles de parâmetros.

- Lado esquerdo: Interface de controle com abas (Simulação, Substrato, Bactérias, Predadores, Ajuda)
- Lado direito: Simulação Pygame (bola quicando como demo)

Instalação mínima:
    pip install PyQt6 pygame

Execute:
    python test_pyqt6_pygame.py
"""

import sys
import time
import os
import math
import csv
import json
import pygame

from PyQt6.QtWidgets import (
    QApplication,
    QMainWindow,
    QWidget,
    QLabel,
    QHBoxLayout,
    QVBoxLayout,
    QTabWidget,
    QTextEdit,
    QLineEdit,
    QSpinBox,
    QDoubleSpinBox,
    QCheckBox,
    QPushButton,
    QComboBox,
    QScrollArea,
    QFrame,
    QGridLayout,
    QFormLayout,
    QMessageBox,
    QDialog,
    QListWidget,
)
from PyQt6.QtGui import QImage, QPixmap
from PyQt6.QtCore import QTimer, Qt, pyqtSignal


# ====================== PLACEHOLDERS PARA DEPENDÊNCIAS EXTERNAS ======================

class MockParams:
    """Placeholder para controllers.Params"""
    def __init__(self):
        self._params = {
            'time_scale': 1.0, 'fps': 60, 'paused': False, 'use_spatial': True,
            'retina_skip': 0, 'simple_render': False, 'reuse_spatial_grid': True,
            'substrate_shape': 'rectangular', 'food_target': 50, 'food_min_r': 4.5,
            'food_max_r': 5.0, 'food_replenish_interval': 0.1, 'world_w': 1000.0,
            'world_h': 700.0, 'substrate_radius': 400.0, 'bacteria_count': 150,
            'bacteria_initial_energy': 100.0, 'bacteria_energy_loss_idle': 0.01,
            'bacteria_energy_loss_move': 5.0, 'bacteria_death_energy': 0.0,
            'bacteria_split_energy': 150.0, 'bacteria_body_size': 9.0,
            'bacteria_show_vision': False, 'bacteria_vision_radius': 120.0,
            'bacteria_retina_count': 18, 'bacteria_retina_fov_degrees': 180.0,
            'bacteria_retina_see_food': True, 'bacteria_retina_see_bacteria': False,
            'bacteria_retina_see_predators': False, 'bacteria_max_speed': 300.0,
            'bacteria_max_turn': math.pi, 'bacteria_min_limit': 10,
            'bacteria_max_limit': 300, 'bacteria_hidden_layers': 4,
            'bacteria_mutation_rate': 0.05, 'bacteria_mutation_strength': 0.08,
            'predators_enabled': False, 'predator_count': 0,
            'predator_initial_energy': 100.0, 'predator_energy_loss_idle': 0.01,
            'predator_energy_loss_move': 5.0, 'predator_death_energy': 0.0,
            'predator_split_energy': 150.0, 'predator_body_size': 14.0,
            'predator_show_vision': False, 'predator_vision_radius': 120.0,
            'predator_retina_count': 18, 'predator_retina_fov_degrees': 180.0,
            'predator_retina_see_food': True, 'predator_retina_see_bacteria': True,
            'predator_retina_see_predators': False, 'predator_max_speed': 300.0,
            'predator_max_turn': math.pi, 'predator_min_limit': 0,
            'predator_max_limit': 100, 'predator_hidden_layers': 2,
            'predator_mutation_rate': 0.05, 'predator_mutation_strength': 0.08,
            'agents_inertia': 1.0, 'show_selected_details': True,
        }
        # Neurônios por camada
        for i in range(1, 6):
            self._params[f'bacteria_neurons_layer_{i}'] = 20 if i <= 4 else 0
            self._params[f'predator_neurons_layer_{i}'] = 16 if i == 1 else (8 if i == 2 else 0)
    
    def get(self, key, default=None):
        return self._params.get(key, default)
    
    def set(self, key, value):
        self._params[key] = value
        print(f"[MockParams] {key} = {value}")

class MockEngine:
    """Placeholder para engine.Engine"""
    def __init__(self):
        self.running = False
        self.selected_agent = None
        self.agents = []
        self.foods = []
        self.loaded_agent_prototypes = {}
        self.current_agent_prototype = None
        self.world = MockWorld()
    
    def send_command(self, command, **kwargs):
        print(f"[MockEngine] Command: {command}, kwargs: {kwargs}")
    
    def start(self):
        self.running = True
        print("[MockEngine] Started")

class MockWorld:
    """Placeholder para world"""
    def __init__(self):
        self.width = 1000.0
        self.height = 700.0
    
    def configure(self, shape, radius, width, height):
        self.width, self.height = width, height
        print(f"[MockWorld] Configured: {shape}, radius={radius}, size={width}x{height}")
    
    def clamp_position(self, x, y, r=0.0):
        return x, y

class MockPygameView:
    """Placeholder para game.PygameView"""
    def __init__(self):
        pass
    
    def initialize(self, window_id):
        print(f"[MockPygameView] Initialized with window_id: {window_id}")
    
    def run(self):
        print("[MockPygameView] Running...")
    
    def stop(self):
        print("[MockPygameView] Stopped")
    
    def cleanup(self):
        print("[MockPygameView] Cleaned up")


# ====================== SIMULAÇÃO PYGAME INTEGRADA ======================

class PygameSim:
    """Simulação simples de uma bolinha quicando dentro de um retângulo."""

    def __init__(self, size):
        pygame.init()
        self.width, self.height = size
        self.surface = pygame.Surface((self.width, self.height), flags=0)
        self.bg_color = (18, 18, 20)  # quase preto

        self.ball_radius = 12
        self.ball_color = (200, 120, 80)
        self.pos = [self.width // 2, self.height // 2]
        self.vel = [180.0, 140.0]  # pixels por segundo

        self.last_time = time.time()

    def resize(self, size):
        self.width, self.height = size
        self.surface = pygame.Surface((self.width, self.height), flags=0)

        # confine ball if outside
        x, y = self.pos
        x = max(self.ball_radius, min(self.width - self.ball_radius, x))
        y = max(self.ball_radius, min(self.height - self.ball_radius, y))
        self.pos = [x, y]

    def step(self):
        now = time.time()
        dt = now - self.last_time
        self.last_time = now

        # update position
        self.pos[0] += self.vel[0] * dt
        self.pos[1] += self.vel[1] * dt

        # bounce
        if self.pos[0] - self.ball_radius <= 0:
            self.pos[0] = self.ball_radius
            self.vel[0] *= -1
        if self.pos[0] + self.ball_radius >= self.width:
            self.pos[0] = self.width - self.ball_radius
            self.vel[0] *= -1
        if self.pos[1] - self.ball_radius <= 0:
            self.pos[1] = self.ball_radius
            self.vel[1] *= -1
        if self.pos[1] + self.ball_radius >= self.height:
            self.pos[1] = self.height - self.ball_radius
            self.vel[1] *= -1

    def draw(self):
        # draw onto surface
        self.surface.fill(self.bg_color)
        pygame.draw.circle(
            self.surface,
            self.ball_color,
            (int(self.pos[0]), int(self.pos[1])),
            self.ball_radius,
        )

    def get_qimage(self):
        """Converte a surface pygame para QImage (RGBA preferido)."""
        # tente RGBA primeiro
        try:
            raw = pygame.image.tostring(self.surface, "RGBA")
            fmt = QImage.Format.Format_RGBA8888
            bytes_per_line = 4 * self.width
            img = QImage(raw, self.width, self.height, bytes_per_line, fmt)
            return img
        except Exception:
            # fallback para RGB
            raw = pygame.image.tostring(self.surface, "RGB")
            fmt = QImage.Format.Format_RGB888
            bytes_per_line = 3 * self.width
            img = QImage(raw, self.width, self.height, bytes_per_line, fmt)
            return img


class PygameWidget(QLabel):
    """QLabel que exibe a simulação pygame atualizada por um timer."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(320, 240)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setStyleSheet("background: transparent;")

        # init simulation with current size
        self.sim = PygameSim((max(1, self.width()), max(1, self.height())))

    def resizeEvent(self, event):
        w = max(1, self.width())
        h = max(1, self.height())
        self.sim.resize((w, h))
        super().resizeEvent(event)

    def update_frame(self):
        self.sim.step()
        self.sim.draw()
        qimg = self.sim.get_qimage()
        pix = QPixmap.fromImage(qimg)
        # garantir preenchimento do widget
        pix = pix.scaled(self.width(), self.height(), Qt.AspectRatioMode.IgnoreAspectRatio)
        self.setPixmap(pix)


# ====================== INTERFACE PYQT6 COMPLETA ======================


class SimulationUI(QMainWindow):
    """
    Interface PyQt6 completa para controle da simulação.
    
    - Separada da lógica de simulação (Engine é headless)  
    - Foco em parâmetros e controles de alto nível
    - Baseada na versão Tkinter original
    """
    
    def __init__(self, params=None, engine=None, pygame_view=None):
        super().__init__()
        
        # Use placeholders se não foram fornecidos
        self.params = params or MockParams()
        self.engine = engine or MockEngine()
        self.pygame_view = pygame_view or MockPygameView()
        
        self._ui_params_csv = os.path.join(os.path.dirname(__file__), 'ui_params.csv')
        
        self.setWindowTitle("Simulação - Bactérias e Predadores (PyQt6)")
        self.resize(1200, 800)

        # Layout principal
        self.setup_layout()

        # Variáveis
        self.control_vars = {}
        self.setup_control_variables()
        self.load_ui_params()  # tenta carregar antes de montar interface

        # Interface
        self.build_interface()

        # Callbacks
        self.setup_param_callbacks()
        
        # Timer para simulação pygame
        self.pygame_timer = QTimer(self)
        self.pygame_timer.timeout.connect(self.pygame_widget.update_frame)
        self.pygame_timer.start(16)  # ~60 FPS
        
        # Aplicar estilo escuro minimalista
        self._apply_dark_theme()
    
    def setup_layout(self):
        """Configura layout principal da janela."""
        central = QWidget()
        self.setCentralWidget(central)
        layout = QHBoxLayout(central)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(8)

        # Frame esquerdo para controles
        self.control_frame = QWidget()
        self.control_frame.setFixedWidth(400)
        layout.addWidget(self.control_frame)
        
        # Frame direito para Pygame
        self.pygame_widget = PygameWidget()
        layout.addWidget(self.pygame_widget, stretch=1)
        
    def setup_control_variables(self):
        """Cria dicionário com todas as variáveis de controle."""
        # Simulação geral
        self.control_vars.update({
            'time_scale': self.params.get('time_scale', 1.0),
            'fps': self.params.get('fps', 60),
            'paused': self.params.get('paused', False),
            'use_spatial': self.params.get('use_spatial', True),
            'retina_skip': self.params.get('retina_skip', 0),
            'simple_render': self.params.get('simple_render', False),
            'reuse_spatial_grid': self.params.get('reuse_spatial_grid', True),
            
            # Substrato
            'substrate_shape': self.params.get('substrate_shape', 'rectangular'),
            'food_target': self.params.get('food_target', 50),
            'food_min_r': self.params.get('food_min_r', 4.5),
            'food_max_r': self.params.get('food_max_r', 5.0),
            'food_replenish_interval': self.params.get('food_replenish_interval', 0.1),
            'world_w': self.params.get('world_w', 1000.0),
            'world_h': self.params.get('world_h', 700.0),
            'substrate_radius': self.params.get('substrate_radius', 400.0),
            
            # Bactérias
            'bacteria_count': self.params.get('bacteria_count', 150),
            'bacteria_initial_energy': self.params.get('bacteria_initial_energy', 100.0),
            'bacteria_energy_loss_idle': self.params.get('bacteria_energy_loss_idle', 0.01),
            'bacteria_energy_loss_move': self.params.get('bacteria_energy_loss_move', 5.0),
            'bacteria_death_energy': self.params.get('bacteria_death_energy', 0.0),
            'bacteria_split_energy': self.params.get('bacteria_split_energy', 150.0),
            'bacteria_body_size': self.params.get('bacteria_body_size', 9.0),
            'bacteria_show_vision': self.params.get('bacteria_show_vision', False),
            'bacteria_vision_radius': self.params.get('bacteria_vision_radius', 120.0),
            'bacteria_retina_count': self.params.get('bacteria_retina_count', 18),
            'bacteria_retina_fov_degrees': self.params.get('bacteria_retina_fov_degrees', 180.0),
            'bacteria_retina_see_food': self.params.get('bacteria_retina_see_food', True),
            'bacteria_retina_see_bacteria': self.params.get('bacteria_retina_see_bacteria', False),
            'bacteria_retina_see_predators': self.params.get('bacteria_retina_see_predators', False),
            'bacteria_max_speed': self.params.get('bacteria_max_speed', 300.0),
            'bacteria_max_turn_deg': math.degrees(self.params.get('bacteria_max_turn', math.pi)),
            'bacteria_min_limit': self.params.get('bacteria_min_limit', 10),
            'bacteria_max_limit': self.params.get('bacteria_max_limit', 300),
            'bacteria_hidden_layers': self.params.get('bacteria_hidden_layers', 4),
            'bacteria_mutation_rate': self.params.get('bacteria_mutation_rate', 0.05),
            'bacteria_mutation_strength': self.params.get('bacteria_mutation_strength', 0.08),
            
            # Predadores
            'predators_enabled': self.params.get('predators_enabled', False),
            'predator_count': self.params.get('predator_count', 0),
            'predator_initial_energy': self.params.get('predator_initial_energy', 100.0),
            'predator_energy_loss_idle': self.params.get('predator_energy_loss_idle', 0.01),
            'predator_energy_loss_move': self.params.get('predator_energy_loss_move', 5.0),
            'predator_death_energy': self.params.get('predator_death_energy', 0.0),
            'predator_split_energy': self.params.get('predator_split_energy', 150.0),
            'predator_body_size': self.params.get('predator_body_size', 14.0),
            'predator_show_vision': self.params.get('predator_show_vision', False),
            'predator_vision_radius': self.params.get('predator_vision_radius', 120.0),
            'predator_retina_count': self.params.get('predator_retina_count', 18),
            'predator_retina_fov_degrees': self.params.get('predator_retina_fov_degrees', 180.0),
            'predator_retina_see_food': self.params.get('predator_retina_see_food', True),
            'predator_retina_see_bacteria': self.params.get('predator_retina_see_bacteria', True),
            'predator_retina_see_predators': self.params.get('predator_retina_see_predators', False),
            'predator_max_speed': self.params.get('predator_max_speed', 300.0),
            'predator_max_turn_deg': math.degrees(self.params.get('predator_max_turn', math.pi)),
            'predator_min_limit': self.params.get('predator_min_limit', 0),
            'predator_max_limit': self.params.get('predator_max_limit', 100),
            'predator_hidden_layers': self.params.get('predator_hidden_layers', 2),
            'predator_mutation_rate': self.params.get('predator_mutation_rate', 0.05),
            'predator_mutation_strength': self.params.get('predator_mutation_strength', 0.08),
            
            # Outros
            'agents_inertia': self.params.get('agents_inertia', 1.0),
            'show_selected_details': self.params.get('show_selected_details', True),
        })
        
        # Neurônios por camada (bactérias)
        for i in range(1, 6):
            key = f'bacteria_neurons_layer_{i}'
            self.control_vars[key] = self.params.get(key, 20 if i <= 4 else 0)
        
        # Neurônios por camada (predadores)
        for i in range(1, 6):
            key = f'predator_neurons_layer_{i}'
            default = 16 if i == 1 else (8 if i == 2 else 0)
            self.control_vars[key] = self.params.get(key, default)
    
    def _style_spinbox(self, spinbox, width=150):
        """Aplica estilo padrão para SpinBox e DoubleSpinBox."""
        spinbox.setFixedWidth(width)
        spinbox.setButtonSymbols(QSpinBox.ButtonSymbols.UpDownArrows)
        spinbox.setStyleSheet("QSpinBox, QDoubleSpinBox { padding-right: 15px; } QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 15px; }")
    
    def build_interface(self):
        """Constrói interface com abas."""
        # Layout no frame de controles
        layout = QVBoxLayout(self.control_frame)
        
        # Notebook principal
        self.notebook = QTabWidget()
        layout.addWidget(self.notebook)
        
        # Cria abas
        self.build_simulation_tab()
        self.build_substrate_tab()
        self.build_bacteria_tab()
        self.build_predator_tab()
        self.build_help_tab()
    
    def build_simulation_tab(self):
        """Aba de controles gerais da simulação."""
        tab = QWidget()
        self.notebook.addTab(tab, "Simulação")
        layout = QFormLayout(tab)
        
        # Controles de tempo
        self.time_scale_edit = QDoubleSpinBox()
        self.time_scale_edit.setRange(0.1, 10.0)
        self.time_scale_edit.setSingleStep(0.1)
        self.time_scale_edit.setValue(self.control_vars['time_scale'])
        self._style_spinbox(self.time_scale_edit)
        layout.addRow("Escala de tempo (x):", self.time_scale_edit)
        
        self.fps_edit = QSpinBox()
        self.fps_edit.setRange(1, 240)
        self.fps_edit.setValue(self.control_vars['fps'])
        self._style_spinbox(self.fps_edit)
        layout.addRow("FPS:", self.fps_edit)
        
        # Estados da simulação
        self.paused_cb = QCheckBox("Pausado")
        self.paused_cb.setChecked(self.control_vars['paused'])
        layout.addRow(self.paused_cb)
        
        self.use_spatial_cb = QCheckBox("Spatial Hash")
        self.use_spatial_cb.setChecked(self.control_vars['use_spatial'])
        layout.addRow(self.use_spatial_cb)
        
        # Performance
        self.retina_skip_edit = QSpinBox()
        self.retina_skip_edit.setRange(0, 10)
        self.retina_skip_edit.setValue(self.control_vars['retina_skip'])
        self._style_spinbox(self.retina_skip_edit)
        layout.addRow("Retina skip:", self.retina_skip_edit)
        
        self.simple_render_cb = QCheckBox("Renderização simples")
        self.simple_render_cb.setChecked(self.control_vars['simple_render'])
        layout.addRow(self.simple_render_cb)
        
        self.reuse_spatial_cb = QCheckBox("Reutilizar grid espacial")
        self.reuse_spatial_cb.setChecked(self.control_vars['reuse_spatial_grid'])
        layout.addRow(self.reuse_spatial_cb)
        
        self.inertia_edit = QDoubleSpinBox()
        self.inertia_edit.setRange(0.1, 10.0)
        self.inertia_edit.setSingleStep(0.1)
        self.inertia_edit.setValue(self.control_vars['agents_inertia'])
        self._style_spinbox(self.inertia_edit)
        layout.addRow("Inércia global:", self.inertia_edit)
        
        # Botões de controle
        apply_sim_btn = QPushButton("Aplicar Parâmetros")
        apply_sim_btn.clicked.connect(self.apply_simulation_params)
        layout.addRow(apply_sim_btn)
        
        reset_pop_btn = QPushButton("Resetar População")
        reset_pop_btn.clicked.connect(self.reset_population)
        layout.addRow(reset_pop_btn)
        
        start_sim_btn = QPushButton("Iniciar Simulação")
        start_sim_btn.clicked.connect(self.start_simulation)
        layout.addRow(start_sim_btn)
        
        apply_all_btn = QPushButton("Aplicar TODOS os Parâmetros")
        apply_all_btn.clicked.connect(self.apply_all_params)
        layout.addRow(apply_all_btn)
        
        save_btn = QPushButton("Salvar")
        save_btn.clicked.connect(self.save_ui_params)
        layout.addRow(save_btn)
        
        # Botão linha - exportar/carregar agente
        agent_layout = QHBoxLayout()
        export_btn = QPushButton("Exportar Agente")
        export_btn.clicked.connect(self.open_export_agent_window)
        agent_layout.addWidget(export_btn)
        
        load_btn = QPushButton("Carregar Agente")
        load_btn.clicked.connect(self.open_load_agent_window)
        agent_layout.addWidget(load_btn)
        
        layout.addRow(agent_layout)
    
    def build_substrate_tab(self):
        """Aba de controles do substrato (comida/mundo)."""
        tab = QWidget()
        self.notebook.addTab(tab, "Substrato")
        layout = QFormLayout(tab)
        
        # Comida
        self.food_target_edit = QSpinBox()
        self.food_target_edit.setRange(0, 1000)
        self.food_target_edit.setValue(self.control_vars['food_target'])
        self._style_spinbox(self.food_target_edit)
        layout.addRow("Target comida:", self.food_target_edit)
        
        self.food_min_r_edit = QDoubleSpinBox()
        self.food_min_r_edit.setRange(0.5, 20.0)
        self.food_min_r_edit.setSingleStep(0.1)
        self.food_min_r_edit.setValue(self.control_vars['food_min_r'])
        self._style_spinbox(self.food_min_r_edit)
        layout.addRow("Comida raio mín:", self.food_min_r_edit)
        
        self.food_max_r_edit = QDoubleSpinBox()
        self.food_max_r_edit.setRange(0.5, 20.0)
        self.food_max_r_edit.setSingleStep(0.1)
        self.food_max_r_edit.setValue(self.control_vars['food_max_r'])
        self._style_spinbox(self.food_max_r_edit)
        layout.addRow("Comida raio máx:", self.food_max_r_edit)
        
        self.food_replenish_edit = QDoubleSpinBox()
        self.food_replenish_edit.setRange(0.01, 10.0)
        self.food_replenish_edit.setSingleStep(0.01)
        self.food_replenish_edit.setValue(self.control_vars['food_replenish_interval'])
        self._style_spinbox(self.food_replenish_edit)
        layout.addRow("Intervalo reposição (s):", self.food_replenish_edit)
        
        # Mundo
        self.world_w_edit = QDoubleSpinBox()
        self.world_w_edit.setRange(100.0, 10000.0)
        self.world_w_edit.setSingleStep(50.0)
        self.world_w_edit.setValue(self.control_vars['world_w'])

        self._style_spinbox(self.world_w_edit)
        layout.addRow("Largura do mundo:", self.world_w_edit)
        
        self.world_h_edit = QDoubleSpinBox()
        self.world_h_edit.setRange(100.0, 10000.0)
        self.world_h_edit.setSingleStep(50.0)
        self.world_h_edit.setValue(self.control_vars['world_h'])

        self._style_spinbox(self.world_h_edit)
        layout.addRow("Altura do mundo:", self.world_h_edit)
        
        # Formato do substrato
        self.substrate_combo = QComboBox()
        self.substrate_combo.addItems(["rectangular", "circular"])
        self.substrate_combo.setCurrentText(self.control_vars['substrate_shape'])
        layout.addRow("Formato do substrato:", self.substrate_combo)
        
        self.substrate_radius_edit = QDoubleSpinBox()
        self.substrate_radius_edit.setRange(50.0, 1000.0)
        self.substrate_radius_edit.setSingleStep(10.0)
        self.substrate_radius_edit.setValue(self.control_vars['substrate_radius'])

        self._style_spinbox(self.substrate_radius_edit)
        layout.addRow("Raio do substrato:", self.substrate_radius_edit)
        
        apply_substrate_btn = QPushButton("Aplicar Parâmetros")
        apply_substrate_btn.clicked.connect(self.apply_substrate_params)
        layout.addRow(apply_substrate_btn)
    
    def build_bacteria_tab(self):
        """Aba de controles das bactérias."""
        tab = QWidget()
        self.notebook.addTab(tab, "Bactérias")
        
        # Scroll area para todos os controles
        scroll = QScrollArea()
        scroll_widget = QWidget()
        layout = QFormLayout(scroll_widget)
        
        # População
        self.bacteria_count_edit = QSpinBox()
        self.bacteria_count_edit.setRange(0, 2000)
        self.bacteria_count_edit.setValue(self.control_vars['bacteria_count'])

        self._style_spinbox(self.bacteria_count_edit)
        layout.addRow("Quantidade inicial:", self.bacteria_count_edit)
        
        self.bacteria_min_edit = QSpinBox()
        self.bacteria_min_edit.setRange(0, 1000)
        self.bacteria_min_edit.setValue(self.control_vars['bacteria_min_limit'])

        self._style_spinbox(self.bacteria_min_edit)
        layout.addRow("Mínimo:", self.bacteria_min_edit)
        
        self.bacteria_max_edit = QSpinBox()
        self.bacteria_max_edit.setRange(1, 20000)
        self.bacteria_max_edit.setValue(self.control_vars['bacteria_max_limit'])

        self._style_spinbox(self.bacteria_max_edit)
        layout.addRow("Máximo:", self.bacteria_max_edit)
        
        # Separador
        separator1 = QFrame()
        separator1.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator1)
        
        # ENERGIA
        energy_label = QLabel("ENERGIA:")
        energy_label.setStyleSheet("font-weight: bold;")
        layout.addRow(energy_label)
        
        self.bacteria_energy_idle_edit = QDoubleSpinBox()
        self.bacteria_energy_idle_edit.setRange(0.0, 10.0)
        self.bacteria_energy_idle_edit.setSingleStep(0.001)
        self.bacteria_energy_idle_edit.setDecimals(3)
        self.bacteria_energy_idle_edit.setValue(self.control_vars['bacteria_energy_loss_idle'])
        layout.addRow("Perda (idle):", self.bacteria_energy_idle_edit)
        
        self.bacteria_energy_move_edit = QDoubleSpinBox()
        self.bacteria_energy_move_edit.setRange(0.0, 100.0)
        self.bacteria_energy_move_edit.setSingleStep(0.1)
        self.bacteria_energy_move_edit.setValue(self.control_vars['bacteria_energy_loss_move'])
        layout.addRow("Perda (movimento):", self.bacteria_energy_move_edit)
        
        self.bacteria_initial_energy_edit = QDoubleSpinBox()
        self.bacteria_initial_energy_edit.setRange(0.0, 10000.0)
        self.bacteria_initial_energy_edit.setSingleStep(1.0)
        self.bacteria_initial_energy_edit.setValue(self.control_vars['bacteria_initial_energy'])

        self._style_spinbox(self.bacteria_initial_energy_edit)
        layout.addRow("Energia inicial:", self.bacteria_initial_energy_edit)
        
        self.bacteria_death_energy_edit = QDoubleSpinBox()
        self.bacteria_death_energy_edit.setRange(0.0, 1000.0)
        self.bacteria_death_energy_edit.setSingleStep(1.0)
        self.bacteria_death_energy_edit.setValue(self.control_vars['bacteria_death_energy'])

        self._style_spinbox(self.bacteria_death_energy_edit)
        layout.addRow("Energia morte:", self.bacteria_death_energy_edit)
        
        self.bacteria_split_energy_edit = QDoubleSpinBox()
        self.bacteria_split_energy_edit.setRange(0.0, 20000.0)
        self.bacteria_split_energy_edit.setSingleStep(5.0)
        self.bacteria_split_energy_edit.setValue(self.control_vars['bacteria_split_energy'])

        self._style_spinbox(self.bacteria_split_energy_edit)
        layout.addRow("Energia dividir:", self.bacteria_split_energy_edit)
        
        self.bacteria_body_size_edit = QDoubleSpinBox()
        self.bacteria_body_size_edit.setRange(1.0, 100.0)
        self.bacteria_body_size_edit.setSingleStep(0.5)
        self.bacteria_body_size_edit.setValue(self.control_vars['bacteria_body_size'])
        layout.addRow("Tamanho corpo (raio):", self.bacteria_body_size_edit)
        
        # Separador
        separator2 = QFrame()
        separator2.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator2)
        
        # VISÃO
        vision_label = QLabel("VISÃO:")
        vision_label.setStyleSheet("font-weight: bold;")
        layout.addRow(vision_label)
        
        self.bacteria_vision_radius_edit = QDoubleSpinBox()
        self.bacteria_vision_radius_edit.setRange(10.0, 1000.0)
        self.bacteria_vision_radius_edit.setSingleStep(5.0)
        self.bacteria_vision_radius_edit.setValue(self.control_vars['bacteria_vision_radius'])

        self._style_spinbox(self.bacteria_vision_radius_edit)
        layout.addRow("Raio visão:", self.bacteria_vision_radius_edit)
        
        self.bacteria_retina_count_edit = QSpinBox()
        self.bacteria_retina_count_edit.setRange(3, 64)
        self.bacteria_retina_count_edit.setValue(self.control_vars['bacteria_retina_count'])

        self._style_spinbox(self.bacteria_retina_count_edit)
        layout.addRow("Número de retinas:", self.bacteria_retina_count_edit)
        
        self.bacteria_fov_edit = QDoubleSpinBox()
        self.bacteria_fov_edit.setRange(10.0, 360.0)
        self.bacteria_fov_edit.setSingleStep(10.0)
        self.bacteria_fov_edit.setValue(self.control_vars['bacteria_retina_fov_degrees'])
        layout.addRow("Campo de visão (°):", self.bacteria_fov_edit)
        
        self.bacteria_see_food_cb = QCheckBox("Ver comida")
        self.bacteria_see_food_cb.setChecked(self.control_vars['bacteria_retina_see_food'])
        layout.addRow(self.bacteria_see_food_cb)
        
        self.bacteria_see_bacteria_cb = QCheckBox("Ver bactérias")
        self.bacteria_see_bacteria_cb.setChecked(self.control_vars['bacteria_retina_see_bacteria'])
        layout.addRow(self.bacteria_see_bacteria_cb)
        
        self.bacteria_see_predators_cb = QCheckBox("Ver predadores")
        self.bacteria_see_predators_cb.setChecked(self.control_vars['bacteria_retina_see_predators'])
        layout.addRow(self.bacteria_see_predators_cb)
        
        # Separador
        separator3 = QFrame()
        separator3.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator3)
        
        # MOVIMENTO
        movement_label = QLabel("MOVIMENTO:")
        movement_label.setStyleSheet("font-weight: bold;")
        layout.addRow(movement_label)
        
        self.bacteria_max_speed_edit = QDoubleSpinBox()
        self.bacteria_max_speed_edit.setRange(0.0, 5000.0)
        self.bacteria_max_speed_edit.setSingleStep(10.0)
        self.bacteria_max_speed_edit.setValue(self.control_vars['bacteria_max_speed'])

        self._style_spinbox(self.bacteria_max_speed_edit)
        layout.addRow("Velocidade máx:", self.bacteria_max_speed_edit)
        
        self.bacteria_max_turn_edit = QDoubleSpinBox()
        self.bacteria_max_turn_edit.setRange(1.0, 1080.0)
        self.bacteria_max_turn_edit.setSingleStep(5.0)
        self.bacteria_max_turn_edit.setValue(self.control_vars['bacteria_max_turn_deg'])
        layout.addRow("Rotação máx (°/s):", self.bacteria_max_turn_edit)
        
        # Separador
        separator4 = QFrame()
        separator4.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator4)
        
        # REDE NEURAL
        neural_label = QLabel("REDE NEURAL:")
        neural_label.setStyleSheet("font-weight: bold;")
        layout.addRow(neural_label)
        
        self.bacteria_hidden_layers_edit = QSpinBox()
        self.bacteria_hidden_layers_edit.setRange(1, 5)
        self.bacteria_hidden_layers_edit.setValue(self.control_vars['bacteria_hidden_layers'])

        self._style_spinbox(self.bacteria_hidden_layers_edit)
        layout.addRow("Camadas ocultas:", self.bacteria_hidden_layers_edit)
        
        # Neurônios por camada
        self.bacteria_neuron_edits = []
        for i in range(1, 6):
            edit = QSpinBox()
            edit.setRange(1, 100)
            edit.setValue(self.control_vars[f'bacteria_neurons_layer_{i}'])
            self.bacteria_neuron_edits.append(edit)
            layout.addRow(f"Neurônios camada {i}:", edit)
        
        def update_bacteria_layers():
            layers = self.bacteria_hidden_layers_edit.value()
            for i, edit in enumerate(self.bacteria_neuron_edits):
                edit.setEnabled(i < layers)
        
        self.bacteria_hidden_layers_edit.valueChanged.connect(update_bacteria_layers)
        update_bacteria_layers()  # Aplicar inicial
        
        self.bacteria_mutation_rate_edit = QDoubleSpinBox()
        self.bacteria_mutation_rate_edit.setRange(0.0, 1.0)
        self.bacteria_mutation_rate_edit.setSingleStep(0.001)
        self.bacteria_mutation_rate_edit.setDecimals(3)
        self.bacteria_mutation_rate_edit.setValue(self.control_vars['bacteria_mutation_rate'])

        self._style_spinbox(self.bacteria_mutation_rate_edit)
        layout.addRow("Taxa de mutação:", self.bacteria_mutation_rate_edit)
        
        self.bacteria_mutation_strength_edit = QDoubleSpinBox()
        self.bacteria_mutation_strength_edit.setRange(0.0, 5.0)
        self.bacteria_mutation_strength_edit.setSingleStep(0.01)
        self.bacteria_mutation_strength_edit.setValue(self.control_vars['bacteria_mutation_strength'])

        self._style_spinbox(self.bacteria_mutation_strength_edit)
        layout.addRow("Força de mutação:", self.bacteria_mutation_strength_edit)
        
        apply_bacteria_btn = QPushButton("Aplicar Parâmetros")
        apply_bacteria_btn.clicked.connect(self.apply_bacteria_params)
        layout.addRow(apply_bacteria_btn)
        
        scroll.setWidget(scroll_widget)
        tab_layout = QVBoxLayout(tab)
        tab_layout.addWidget(scroll)
    
    def build_predator_tab(self):
        """Aba de controles dos predadores."""
        tab = QWidget()
        self.notebook.addTab(tab, "Predadores")
        
        # Scroll area
        scroll = QScrollArea()
        scroll_widget = QWidget()
        layout = QFormLayout(scroll_widget)
        
        # Enable predadores
        self.predators_enabled_cb = QCheckBox("Habilitar predadores")
        self.predators_enabled_cb.setChecked(self.control_vars['predators_enabled'])
        layout.addRow(self.predators_enabled_cb)
        
        # População
        self.predator_count_edit = QSpinBox()
        self.predator_count_edit.setRange(0, 500)
        self.predator_count_edit.setValue(self.control_vars['predator_count'])

        self._style_spinbox(self.predator_count_edit)
        layout.addRow("Quantidade inicial:", self.predator_count_edit)
        
        self.predator_min_edit = QSpinBox()
        self.predator_min_edit.setRange(0, 500)
        self.predator_min_edit.setValue(self.control_vars['predator_min_limit'])

        self._style_spinbox(self.predator_min_edit)
        layout.addRow("Mínimo:", self.predator_min_edit)
        
        self.predator_max_edit = QSpinBox()
        self.predator_max_edit.setRange(1, 2000)
        self.predator_max_edit.setValue(self.control_vars['predator_max_limit'])

        self._style_spinbox(self.predator_max_edit)
        layout.addRow("Máximo:", self.predator_max_edit)
        
        # ENERGIA
        separator1 = QFrame()
        separator1.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator1)
        
        energy_label = QLabel("ENERGIA:")
        energy_label.setStyleSheet("font-weight: bold;")
        layout.addRow(energy_label)
        
        self.predator_energy_idle_edit = QDoubleSpinBox()
        self.predator_energy_idle_edit.setRange(0.0, 10.0)
        self.predator_energy_idle_edit.setSingleStep(0.001)
        self.predator_energy_idle_edit.setDecimals(3)
        self.predator_energy_idle_edit.setValue(self.control_vars['predator_energy_loss_idle'])
        layout.addRow("Perda (idle):", self.predator_energy_idle_edit)
        
        self.predator_energy_move_edit = QDoubleSpinBox()
        self.predator_energy_move_edit.setRange(0.0, 100.0)
        self.predator_energy_move_edit.setSingleStep(0.1)
        self.predator_energy_move_edit.setValue(self.control_vars['predator_energy_loss_move'])
        layout.addRow("Perda (movimento):", self.predator_energy_move_edit)
        
        self.predator_initial_energy_edit = QDoubleSpinBox()
        self.predator_initial_energy_edit.setRange(0.0, 20000.0)
        self.predator_initial_energy_edit.setSingleStep(1.0)
        self.predator_initial_energy_edit.setValue(self.control_vars['predator_initial_energy'])

        self._style_spinbox(self.predator_initial_energy_edit)
        layout.addRow("Energia inicial:", self.predator_initial_energy_edit)
        
        self.predator_death_energy_edit = QDoubleSpinBox()
        self.predator_death_energy_edit.setRange(0.0, 2000.0)
        self.predator_death_energy_edit.setSingleStep(1.0)
        self.predator_death_energy_edit.setValue(self.control_vars['predator_death_energy'])

        self._style_spinbox(self.predator_death_energy_edit)
        layout.addRow("Energia morte:", self.predator_death_energy_edit)
        
        self.predator_split_energy_edit = QDoubleSpinBox()
        self.predator_split_energy_edit.setRange(0.0, 40000.0)
        self.predator_split_energy_edit.setSingleStep(10.0)
        self.predator_split_energy_edit.setValue(self.control_vars['predator_split_energy'])

        self._style_spinbox(self.predator_split_energy_edit)
        layout.addRow("Energia dividir:", self.predator_split_energy_edit)
        
        self.predator_body_size_edit = QDoubleSpinBox()
        self.predator_body_size_edit.setRange(1.0, 300.0)
        self.predator_body_size_edit.setSingleStep(0.5)
        self.predator_body_size_edit.setValue(self.control_vars['predator_body_size'])
        layout.addRow("Tamanho corpo (raio):", self.predator_body_size_edit)
        
        # VISÃO
        separator2 = QFrame()
        separator2.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator2)
        
        vision_label = QLabel("VISÃO:")
        vision_label.setStyleSheet("font-weight: bold;")
        layout.addRow(vision_label)
        
        self.predator_vision_radius_edit = QDoubleSpinBox()
        self.predator_vision_radius_edit.setRange(10.0, 1500.0)
        self.predator_vision_radius_edit.setSingleStep(10.0)
        self.predator_vision_radius_edit.setValue(self.control_vars['predator_vision_radius'])

        self._style_spinbox(self.predator_vision_radius_edit)
        layout.addRow("Raio visão:", self.predator_vision_radius_edit)
        
        self.predator_see_food_cb = QCheckBox("Ver comida")
        self.predator_see_food_cb.setChecked(self.control_vars['predator_retina_see_food'])
        layout.addRow(self.predator_see_food_cb)
        
        self.predator_see_bacteria_cb = QCheckBox("Ver bactérias")
        self.predator_see_bacteria_cb.setChecked(self.control_vars['predator_retina_see_bacteria'])
        layout.addRow(self.predator_see_bacteria_cb)
        
        self.predator_see_predators_cb = QCheckBox("Ver predadores")
        self.predator_see_predators_cb.setChecked(self.control_vars['predator_retina_see_predators'])
        layout.addRow(self.predator_see_predators_cb)
        
        self.predator_retina_count_edit = QSpinBox()
        self.predator_retina_count_edit.setRange(3, 64)
        self.predator_retina_count_edit.setValue(self.control_vars['predator_retina_count'])

        self._style_spinbox(self.predator_retina_count_edit)
        layout.addRow("Qtd retinas:", self.predator_retina_count_edit)
        
        self.predator_fov_edit = QDoubleSpinBox()
        self.predator_fov_edit.setRange(30, 360)
        self.predator_fov_edit.setSingleStep(10)
        self.predator_fov_edit.setValue(self.control_vars['predator_retina_fov_degrees'])
        layout.addRow("FOV (graus):", self.predator_fov_edit)
        
        # MOVIMENTO
        separator3 = QFrame()
        separator3.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator3)
        
        movement_label = QLabel("MOVIMENTO:")
        movement_label.setStyleSheet("font-weight: bold;")
        layout.addRow(movement_label)
        
        self.predator_max_speed_edit = QDoubleSpinBox()
        self.predator_max_speed_edit.setRange(0.0, 5000.0)
        self.predator_max_speed_edit.setSingleStep(10.0)
        self.predator_max_speed_edit.setValue(self.control_vars['predator_max_speed'])

        self._style_spinbox(self.predator_max_speed_edit)
        layout.addRow("Velocidade máx:", self.predator_max_speed_edit)
        
        self.predator_max_turn_edit = QDoubleSpinBox()
        self.predator_max_turn_edit.setRange(1.0, 1080.0)
        self.predator_max_turn_edit.setSingleStep(5.0)
        self.predator_max_turn_edit.setValue(self.control_vars['predator_max_turn_deg'])
        layout.addRow("Rotação máx (°/s):", self.predator_max_turn_edit)
        
        # REDE NEURAL
        separator4 = QFrame()
        separator4.setFrameShape(QFrame.Shape.HLine)
        layout.addRow(separator4)
        
        neural_label = QLabel("REDE NEURAL:")
        neural_label.setStyleSheet("font-weight: bold;")
        layout.addRow(neural_label)
        
        self.predator_hidden_layers_edit = QSpinBox()
        self.predator_hidden_layers_edit.setRange(1, 5)
        self.predator_hidden_layers_edit.setValue(self.control_vars['predator_hidden_layers'])

        self._style_spinbox(self.predator_hidden_layers_edit)
        layout.addRow("Camadas ocultas:", self.predator_hidden_layers_edit)
        
        # Neurônios por camada
        self.predator_neuron_edits = []
        for i in range(1, 6):
            edit = QSpinBox()
            edit.setRange(1, 100)
            edit.setValue(self.control_vars[f'predator_neurons_layer_{i}'])
            self.predator_neuron_edits.append(edit)
            layout.addRow(f"Neurônios camada {i}:", edit)
        
        def update_predator_layers():
            layers = self.predator_hidden_layers_edit.value()
            for i, edit in enumerate(self.predator_neuron_edits):
                edit.setEnabled(i < layers)
        
        self.predator_hidden_layers_edit.valueChanged.connect(update_predator_layers)
        update_predator_layers()
        
        self.predator_mutation_rate_edit = QDoubleSpinBox()
        self.predator_mutation_rate_edit.setRange(0.0, 1.0)
        self.predator_mutation_rate_edit.setSingleStep(0.001)
        self.predator_mutation_rate_edit.setDecimals(3)
        self.predator_mutation_rate_edit.setValue(self.control_vars['predator_mutation_rate'])

        self._style_spinbox(self.predator_mutation_rate_edit)
        layout.addRow("Taxa de mutação:", self.predator_mutation_rate_edit)
        
        self.predator_mutation_strength_edit = QDoubleSpinBox()
        self.predator_mutation_strength_edit.setRange(0.0, 5.0)
        self.predator_mutation_strength_edit.setSingleStep(0.01)
        self.predator_mutation_strength_edit.setValue(self.control_vars['predator_mutation_strength'])

        self._style_spinbox(self.predator_mutation_strength_edit)
        layout.addRow("Força de mutação:", self.predator_mutation_strength_edit)
        
        apply_predator_btn = QPushButton("Aplicar Parâmetros")
        apply_predator_btn.clicked.connect(self.apply_predator_params)
        layout.addRow(apply_predator_btn)
        
        scroll.setWidget(scroll_widget)
        tab_layout = QVBoxLayout(tab)
        tab_layout.addWidget(scroll)
    
    def build_help_tab(self):
        """Aba de ajuda/instruções."""
        tab = QWidget()
        self.notebook.addTab(tab, "Ajuda")
        layout = QVBoxLayout(tab)
        
        help_text = QTextEdit()
        help_text.setReadOnly(True)
        help_text.setPlainText("""
CONTROLES:

Mouse (área de simulação):
• Scroll: Zoom focalizando no cursor
• Botão esquerdo: Selecionar agente / Adicionar comida
• Botão do meio: Adicionar bactéria
• Botão direito + arrastar: Mover câmera

Teclado:
• Espaço: Pausar/Continuar
• R: Resetar população
• F: Enquadrar mundo na tela
• T: Alternar renderização (simples/bonita)
• V: Mostrar/ocultar visão do agente selecionado
• +/-: Acelerar/desacelerar tempo
• WASD / Setas: Mover câmera

PARÂMETROS:

Simulação:
- Escala de tempo: Multiplica velocidade da simulação
- FPS: Frames por segundo de renderização
- Spatial Hash: Otimização para grandes populações
- Renderização simples: Modo rápido (círculos)

Substrato:
- Target comida: Quantidade ideal de comida
- Reposição: Velocidade de criação de nova comida

Bactérias/Predadores:
- Energia: Consumo por movimento/repouso
- Visão: Raio e filtros das retinas
- Rede Neural: Arquitetura e mutações
- População: Limites mínimo/máximo

DICAS:

• Use renderização simples para populações grandes
• Spatial Hash melhora performance significativamente  
• Time scale alto pode causar instabilidade
• Predadores comem bactérias (70% eficiência)
• Mutações estruturais são raras e controladas
• Sistema de morte é limitado (1 por frame)
""")
        layout.addWidget(help_text)
    
    def setup_param_callbacks(self):
        """Configura callbacks para sincronizar mudanças de parâmetros em tempo real."""
        # Conecta checkboxes que mudam em tempo real
        self.paused_cb.stateChanged.connect(lambda state: self.update_param_real_time('paused', state == 2))
        self.simple_render_cb.stateChanged.connect(lambda state: self.update_param_real_time('simple_render', state == 2))
        
        # Time scale
        self.time_scale_edit.valueChanged.connect(lambda val: self.update_param_real_time('time_scale', val))
        self.fps_edit.valueChanged.connect(lambda val: self.update_param_real_time('fps', val))
    
    def update_param_real_time(self, param_name, value):
        """Atualiza parâmetro em tempo real."""
        try:
            # Conversões especiais
            if param_name.endswith('_deg'):
                # Converte graus para radianos
                param_name = param_name[:-4]  # Remove '_deg'
                value = math.radians(value)
            
            self.params.set(param_name, value)
            
            # Comandos especiais
            if param_name == 'simple_render':
                self.engine.send_command('change_renderer', simple=value)
                
        except Exception as e:
            print(f"Erro ao atualizar parâmetro {param_name}: {e}")
    
    def get_values_from_ui(self):
        """Coleta todos os valores da interface e atualiza control_vars."""
        try:
            # Simulação
            self.control_vars['time_scale'] = self.time_scale_edit.value()
            self.control_vars['fps'] = self.fps_edit.value()
            self.control_vars['paused'] = self.paused_cb.isChecked()
            self.control_vars['use_spatial'] = self.use_spatial_cb.isChecked()
            self.control_vars['retina_skip'] = self.retina_skip_edit.value()
            self.control_vars['simple_render'] = self.simple_render_cb.isChecked()
            self.control_vars['reuse_spatial_grid'] = self.reuse_spatial_cb.isChecked()
            self.control_vars['agents_inertia'] = self.inertia_edit.value()
            
            # Substrato
            self.control_vars['food_target'] = self.food_target_edit.value()
            self.control_vars['food_min_r'] = self.food_min_r_edit.value()
            self.control_vars['food_max_r'] = self.food_max_r_edit.value()
            self.control_vars['food_replenish_interval'] = self.food_replenish_edit.value()
            self.control_vars['world_w'] = self.world_w_edit.value()
            self.control_vars['world_h'] = self.world_h_edit.value()
            self.control_vars['substrate_shape'] = self.substrate_combo.currentText()
            self.control_vars['substrate_radius'] = self.substrate_radius_edit.value()
            
            # Bactérias
            self.control_vars['bacteria_count'] = self.bacteria_count_edit.value()
            self.control_vars['bacteria_min_limit'] = self.bacteria_min_edit.value()
            self.control_vars['bacteria_max_limit'] = self.bacteria_max_edit.value()
            self.control_vars['bacteria_energy_loss_idle'] = self.bacteria_energy_idle_edit.value()
            self.control_vars['bacteria_energy_loss_move'] = self.bacteria_energy_move_edit.value()
            self.control_vars['bacteria_initial_energy'] = self.bacteria_initial_energy_edit.value()
            self.control_vars['bacteria_death_energy'] = self.bacteria_death_energy_edit.value()
            self.control_vars['bacteria_split_energy'] = self.bacteria_split_energy_edit.value()
            self.control_vars['bacteria_body_size'] = self.bacteria_body_size_edit.value()
            self.control_vars['bacteria_vision_radius'] = self.bacteria_vision_radius_edit.value()
            self.control_vars['bacteria_retina_count'] = self.bacteria_retina_count_edit.value()
            self.control_vars['bacteria_retina_fov_degrees'] = self.bacteria_fov_edit.value()
            self.control_vars['bacteria_retina_see_food'] = self.bacteria_see_food_cb.isChecked()
            self.control_vars['bacteria_retina_see_bacteria'] = self.bacteria_see_bacteria_cb.isChecked()
            self.control_vars['bacteria_retina_see_predators'] = self.bacteria_see_predators_cb.isChecked()
            self.control_vars['bacteria_max_speed'] = self.bacteria_max_speed_edit.value()
            self.control_vars['bacteria_max_turn_deg'] = self.bacteria_max_turn_edit.value()
            self.control_vars['bacteria_hidden_layers'] = self.bacteria_hidden_layers_edit.value()
            self.control_vars['bacteria_mutation_rate'] = self.bacteria_mutation_rate_edit.value()
            self.control_vars['bacteria_mutation_strength'] = self.bacteria_mutation_strength_edit.value()
            
            # Neurônios bactérias
            for i, edit in enumerate(self.bacteria_neuron_edits):
                self.control_vars[f'bacteria_neurons_layer_{i+1}'] = edit.value()
            
            # Predadores
            self.control_vars['predators_enabled'] = self.predators_enabled_cb.isChecked()
            self.control_vars['predator_count'] = self.predator_count_edit.value()
            self.control_vars['predator_min_limit'] = self.predator_min_edit.value()
            self.control_vars['predator_max_limit'] = self.predator_max_edit.value()
            self.control_vars['predator_energy_loss_idle'] = self.predator_energy_idle_edit.value()
            self.control_vars['predator_energy_loss_move'] = self.predator_energy_move_edit.value()
            self.control_vars['predator_initial_energy'] = self.predator_initial_energy_edit.value()
            self.control_vars['predator_death_energy'] = self.predator_death_energy_edit.value()
            self.control_vars['predator_split_energy'] = self.predator_split_energy_edit.value()
            self.control_vars['predator_body_size'] = self.predator_body_size_edit.value()
            self.control_vars['predator_vision_radius'] = self.predator_vision_radius_edit.value()
            self.control_vars['predator_retina_see_food'] = self.predator_see_food_cb.isChecked()
            self.control_vars['predator_retina_see_bacteria'] = self.predator_see_bacteria_cb.isChecked()
            self.control_vars['predator_retina_see_predators'] = self.predator_see_predators_cb.isChecked()
            self.control_vars['predator_retina_count'] = self.predator_retina_count_edit.value()
            self.control_vars['predator_retina_fov_degrees'] = self.predator_fov_edit.value()
            self.control_vars['predator_max_speed'] = self.predator_max_speed_edit.value()
            self.control_vars['predator_max_turn_deg'] = self.predator_max_turn_edit.value()
            self.control_vars['predator_hidden_layers'] = self.predator_hidden_layers_edit.value()
            self.control_vars['predator_mutation_rate'] = self.predator_mutation_rate_edit.value()
            self.control_vars['predator_mutation_strength'] = self.predator_mutation_strength_edit.value()
            
            # Neurônios predadores
            for i, edit in enumerate(self.predator_neuron_edits):
                self.control_vars[f'predator_neurons_layer_{i+1}'] = edit.value()
            
        except Exception as e:
            print(f"Erro ao coletar valores da UI: {e}")
    
    def apply_simulation_params(self):
        """Aplica parâmetros de simulação."""
        try:
            self.get_values_from_ui()
            
            params_to_update = [
                'time_scale', 'fps', 'paused', 'use_spatial',
                'retina_skip', 'simple_render', 'reuse_spatial_grid', 'agents_inertia'
            ]
            
            for param in params_to_update:
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            print("Parâmetros de simulação aplicados")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de simulação: {e}")
    
    def apply_substrate_params(self):
        """Aplica parâmetros de substrato."""
        try:
            self.get_values_from_ui()
            
            params_to_update = [
                'food_target', 'food_min_r', 'food_max_r', 'food_replenish_interval',
                'world_w', 'world_h', 'substrate_shape', 'substrate_radius'
            ]
            
            for param in params_to_update:
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            # Reconfigura mundo se shape ou radius mudou
            shape = self.control_vars['substrate_shape']
            radius = self.control_vars['substrate_radius']
            world_w = self.control_vars['world_w']
            world_h = self.control_vars['world_h']
            
            self.engine.world.configure(shape, radius, world_w, world_h)
            
            # Ajusta posições de entidades para dentro do novo limite se circular
            if shape == 'circular':
                for entity in list(self.engine.agents) + list(self.engine.foods):
                    if hasattr(entity, 'x') and hasattr(entity, 'y') and hasattr(entity, 'r'):
                        entity.x, entity.y = self.engine.world.clamp_position(entity.x, entity.y, getattr(entity, 'r', 0.0))
            
            print("Parâmetros de substrato aplicados (mundo atualizado)")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de substrato: {e}")
    
    def apply_bacteria_params(self):
        """Aplica parâmetros de bactérias."""
        try:
            self.get_values_from_ui()
            
            # Lista de parâmetros de bactéria (exceto neurônios que precisam lógica especial)
            bacteria_params = [
                'bacteria_count', 'bacteria_initial_energy', 'bacteria_energy_loss_idle', 'bacteria_energy_loss_move',
                'bacteria_death_energy', 'bacteria_split_energy', 'bacteria_show_vision', 'bacteria_body_size',
                'bacteria_vision_radius', 'bacteria_retina_count', 'bacteria_retina_fov_degrees',
                'bacteria_retina_see_food', 'bacteria_retina_see_bacteria',
                'bacteria_retina_see_predators', 'bacteria_max_speed', 'bacteria_min_limit',
                'bacteria_max_limit', 'bacteria_hidden_layers', 'bacteria_mutation_rate',
                'bacteria_mutation_strength'
            ]
            
            for param in bacteria_params:
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            # Converte graus para radianos para bacteria_max_turn
            if 'bacteria_max_turn_deg' in self.control_vars:
                value = math.radians(self.control_vars['bacteria_max_turn_deg'])
                self.params.set('bacteria_max_turn', value)
            
            # Neurônios por camada
            for i in range(1, 6):
                param = f'bacteria_neurons_layer_{i}'
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            print("Parâmetros de bactérias aplicados")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de bactérias: {e}")
    
    def apply_predator_params(self):
        """Aplica parâmetros de predadores."""
        try:
            self.get_values_from_ui()
            
            # Lista de parâmetros de predador
            predator_params = [
                'predators_enabled', 'predator_count', 'predator_initial_energy', 'predator_energy_loss_idle',
                'predator_energy_loss_move', 'predator_death_energy', 'predator_split_energy', 'predator_body_size',
                'predator_show_vision', 'predator_vision_radius', 'predator_retina_see_food',
                'predator_retina_count', 'predator_retina_fov_degrees',
                'predator_retina_see_bacteria', 'predator_retina_see_predators',
                'predator_max_speed', 'predator_min_limit', 'predator_max_limit',
                'predator_hidden_layers', 'predator_mutation_rate', 'predator_mutation_strength'
            ]
            
            for param in predator_params:
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            # Converte graus para radianos para predator_max_turn
            if 'predator_max_turn_deg' in self.control_vars:
                value = math.radians(self.control_vars['predator_max_turn_deg'])
                self.params.set('predator_max_turn', value)
            
            # Neurônios por camada dos predadores
            for i in range(1, 6):
                param = f'predator_neurons_layer_{i}'
                if param in self.control_vars:
                    value = self.control_vars[param]
                    self.params.set(param, value)
            
            print("Parâmetros de predadores aplicados")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de predadores: {e}")

    def apply_all_params(self):
        """Aplica todos os parâmetros (ordem: simulação, substrato, bactérias, predadores)."""
        try:
            self.apply_simulation_params()
            self.apply_substrate_params()
            self.apply_bacteria_params()
            self.apply_predator_params()
            print("Todos os parâmetros aplicados")
        except Exception as e:
            print(f"Erro ao aplicar todos os parâmetros: {e}")
    
    def reset_population(self):
        """Reseta população da simulação."""
        self.engine.send_command('reset_population')
        print("População resetada")
    
    def start_simulation(self):
        """Inicia simulação aplicando parâmetros e inicializando população se ainda não estiver rodando."""
        try:
            self.apply_all_params()
            if not self.engine.running:
                self.engine.start()
                print("Simulação iniciada")
            else:
                print("Simulação já em execução")
        except Exception as e:
            print(f"Erro ao iniciar simulação: {e}")
    
    def save_ui_params(self):
        """Salva todos os valores das variáveis de controle em CSV."""
        try:
            self.get_values_from_ui()
            fieldnames = sorted(self.control_vars.keys())
            rows = []
            for name in fieldnames:
                value = self.control_vars[name]
                rows.append({'name': name, 'value': value})
            
            with open(self._ui_params_csv, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=['name', 'value'])
                writer.writeheader()
                writer.writerows(rows)
            print(f"Parâmetros UI salvos em {self._ui_params_csv}")
        except Exception as e:
            print(f"Erro ao salvar parâmetros UI: {e}")

    def load_ui_params(self):
        """Carrega valores previamente salvos no CSV (se existir)."""
        if not os.path.exists(getattr(self, '_ui_params_csv', '')):
            return
        try:
            with open(self._ui_params_csv, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    name = row.get('name')
                    value = row.get('value')
                    if name in self.control_vars:
                        # Converte tipo conforme necessário
                        try:
                            if isinstance(self.control_vars[name], bool):
                                self.control_vars[name] = value in ('1', 'True', 'true')
                            elif isinstance(self.control_vars[name], int):
                                self.control_vars[name] = int(float(value))
                            elif isinstance(self.control_vars[name], float):
                                self.control_vars[name] = float(value)
                            else:
                                self.control_vars[name] = value
                        except Exception:
                            pass
            print(f"Parâmetros UI carregados de {self._ui_params_csv}")
        except Exception as e:
            print(f"Erro ao carregar parâmetros UI: {e}")

    def open_export_agent_window(self):
        """Abre janela para exportar agente selecionado."""
        agent = self.engine.selected_agent
        if agent is None:
            QMessageBox.information(self, "Exportar Agente", "Selecione um agente na área da simulação primeiro (clique sobre ele).")
            return
        
        dialog = QDialog(self)
        dialog.setWindowTitle("Exportar Agente")
        layout = QVBoxLayout(dialog)
        
        form_layout = QFormLayout()
        name_edit = QLineEdit("agente")
        form_layout.addRow("Nome do agente:", name_edit)
        layout.addLayout(form_layout)
        
        status_label = QLabel("")
        status_label.setStyleSheet("color: gray;")
        layout.addWidget(status_label)
        
        def do_export():
            name = name_edit.text().strip()
            if not name:
                status_label.setText("Informe um nome.")
                return
            try:
                path = self._export_selected_agent(agent, name)
                status_label.setText(f"Exportado: {os.path.basename(path)}")
            except Exception as e:
                status_label.setText(f"Erro: {e}")
        
        export_btn = QPushButton("Exportar")
        export_btn.clicked.connect(do_export)
        layout.addWidget(export_btn)
        
        dialog.exec()

    def _export_selected_agent(self, agent, name: str) -> str:
        """Exporta agente para CSV (key,value). Retorna caminho."""
        # Pasta 'agents' no root do projeto (um nível acima de tests/)
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'agents'))
        os.makedirs(agents_dir, exist_ok=True)
        
        # Evita sobrescrever código fonte .py; cria sufixo .agent.csv
        filename = f"{name}.agent.csv"
        path = os.path.join(agents_dir, filename)
        
        brain = getattr(agent, 'brain', None)
        sensor = getattr(agent, 'sensor', None)
        locomotion = getattr(agent, 'locomotion', None)
        energy_model = getattr(agent, 'energy_model', None)
        
        rows = []
        def add(k,v): rows.append({'key':k,'value':v})
        
        # Metadados básicos
        add('type', 'predator' if getattr(agent, 'is_predator', False) else 'bacteria')
        add('x', getattr(agent, 'x', 0.0))
        add('y', getattr(agent, 'y', 0.0))
        add('r', getattr(agent, 'r', 5.0))
        add('angle', getattr(agent, 'angle', 0.0))
        add('vx', getattr(agent, 'vx', 0.0))
        add('vy', getattr(agent, 'vy', 0.0))
        add('energy', getattr(agent, 'energy', 0.0))
        add('age', getattr(agent, 'age', 0.0))
        
        # Cérebro
        if brain is not None and hasattr(brain, 'sizes'):
            add('brain_sizes', json.dumps(brain.sizes))
            add('brain_version', getattr(brain, 'version', 0))
            for idx, (W,B) in enumerate(zip(brain.weights, brain.biases)):
                # Converte listas/arrays para lista nativa
                try:
                    import numpy as np
                    if hasattr(W, 'tolist'): w_list = W.tolist()
                    else: w_list = list(W)
                    if hasattr(B, 'tolist'): b_list = B.tolist()
                    else: b_list = list(B)
                except Exception:
                    w_list = list(W) if hasattr(W, '__iter__') else [W]
                    b_list = list(B) if hasattr(B, '__iter__') else [B]
                add(f'brain_weight_{idx}', json.dumps(w_list))
                add(f'brain_bias_{idx}', json.dumps(b_list))
        
        # Sensor
        if sensor is not None:
            for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators']:
                if hasattr(sensor, attr):
                    add(f'sensor_{attr}', getattr(sensor, attr))
        
        # Locomotion
        if locomotion is not None:
            for attr in ['max_speed','max_turn']:
                if hasattr(locomotion, attr):
                    add(f'locomotion_{attr}', getattr(locomotion, attr))
        
        # Energy model
        if energy_model is not None:
            for attr in ['loss_idle','loss_move','death_energy','split_energy']:
                if hasattr(energy_model, attr):
                    add(f'energy_{attr}', getattr(energy_model, attr))
        
        # Última saída e ativações (debug)
        add('last_brain_output', json.dumps(getattr(agent, 'last_brain_output', [])))
        add('last_brain_activations', json.dumps(getattr(agent, 'last_brain_activations', [])))
        
        # Escreve CSV
        with open(path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=['key','value'])
            writer.writeheader()
            writer.writerows(rows)
        print(f"Agente exportado para {path}")
        return path

    def open_load_agent_window(self):
        """Abre janela listando agentes exportados para carregar."""
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'agents'))
        os.makedirs(agents_dir, exist_ok=True)
        files = [f for f in os.listdir(agents_dir) if f.endswith('.agent.csv')]
        
        if not files:
            QMessageBox.information(self, "Carregar Agente", "Nenhum arquivo .agent.csv encontrado na pasta 'agents'.")
            return
        
        dialog = QDialog(self)
        dialog.setWindowTitle("Carregar Agente")
        layout = QVBoxLayout(dialog)
        
        layout.addWidget(QLabel("Escolha o agente exportado:"))
        
        listbox = QListWidget()
        for f in files:
            listbox.addItem(f)
        layout.addWidget(listbox)
        
        status_label = QLabel("")
        status_label.setStyleSheet("color: gray;")
        layout.addWidget(status_label)

        def do_load():
            current = listbox.currentItem()
            if not current:
                status_label.setText("Selecione um arquivo.")
                return
            filename = current.text()
            path = os.path.join(agents_dir, filename)
            try:
                self._load_agent_from_csv(path)
                status_label.setText("Carregado com sucesso.")
            except Exception as e:
                status_label.setText(f"Erro: {e}")
                
        load_btn = QPushButton("Carregar")
        load_btn.clicked.connect(do_load)
        layout.addWidget(load_btn)
        
        # Double click
        listbox.itemDoubleClicked.connect(lambda: do_load())
        
        dialog.exec()

    def _load_agent_from_csv(self, path: str):
        """Carrega CSV e registra protótipo para inserção múltipla via clique direito."""
        data = {}
        with open(path, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            for row in reader:
                data[row['key']] = row['value']
        
        # Nome do protótipo baseado no arquivo
        base = os.path.basename(path)
        name = base.replace('.agent.csv','').replace('.csv','')
        self.engine.loaded_agent_prototypes[name] = data
        self.engine.current_agent_prototype = name
        print(f"Protótipo '{name}' carregado. Clique direito no substrato para inserir instâncias.")

    def _apply_dark_theme(self):
        """Aplica tema escuro minimalista."""
        style = """
        QMainWindow { 
            background-color: #0f1113; 
            color: #e6e6e6; 
        }
        QWidget { 
            background-color: #0f1113; 
            color: #e6e6e6; 
        }
        QTabWidget::pane { 
            border: 1px solid #333; 
            background: #0f1113; 
        }
        QTabBar::tab { 
            background: #17181a; 
            color: #dcdcdc; 
            padding: 6px 12px; 
            margin-right: 2px;
        }
        QTabBar::tab:selected { 
            background: #1f2022; 
        }
        QTextEdit { 
            background: #151517; 
            color: #dcdcdc; 
            border: 1px solid #333;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { 
            background: #151517; 
            color: #dcdcdc; 
            border: 1px solid #333; 
            padding: 4px;
        }
        QPushButton { 
            background: #1a1c1e; 
            color: #dcdcdc; 
            border: 1px solid #444; 
            padding: 6px 12px; 
            border-radius: 3px;
        }
        QPushButton:hover { 
            background: #2a2c2e; 
        }
        QPushButton:pressed { 
            background: #0a0c0e; 
        }
        QCheckBox { 
            color: #dcdcdc; 
        }
        QLabel { 
            color: #e6e6e6; 
        }
        QScrollArea { 
            background: #0f1113; 
            border: none;
        }
        QFrame[frameShape="4"] { 
            color: #444; 
        }
        """
        self.setStyleSheet(style)



# ====================== FUNÇÃO PRINCIPAL ======================

def main():
    """Função principal para executar a aplicação."""
    app = QApplication(sys.argv)
    
    # Cria instância da interface com placeholders
    # Em uso real, passe os objetos reais: SimulationUI(params, engine, pygame_view)
    win = SimulationUI()
    win.show()
    
    sys.exit(app.exec())


if __name__ == '__main__':
    main()
