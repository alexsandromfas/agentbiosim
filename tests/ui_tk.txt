"""
Interface Tkinter para controle da simulação.
Apenas orquestra parâmetros e envia comandos - NÃO contém lógica de simulação.
"""
import os
import sys
import threading
import tkinter as tk
from tkinter import ttk, messagebox
import math
import csv
import json

from .controllers import Params
from .engine import Engine
from .game import PygameView


class SimulationUI:
    """
    Interface Tkinter para controle da simulação.
    
    - Separada da lógica de simulação (Engine é headless)
    - Thread-safe através de commands queue
    - Foco em parâmetros e controles de alto nível
    """
    
    def __init__(self, params: Params, engine: Engine, pygame_view: PygameView):
        self.params = params
        self.engine = engine
        self.pygame_view = pygame_view
        self._ui_params_csv = os.path.join(os.path.dirname(__file__), 'ui_params.csv')
        # Modo de agente único carregado via importação
    # Protótipo atual carregado (para inserção com botão direito)
    # Agora suportamos múltiplos protótipos dentro de engine.loaded_agent_prototypes

        # Tkinter root
        self.root = tk.Tk()
        self.root.title("Simulação - Bactérias e Predadores")

        # Estilo
        self.style = ttk.Style(self.root)
        try:
            self.style.theme_use('clam')
        except Exception:
            pass

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
    
    def setup_layout(self):
        """Configura layout principal da janela."""
        # Frame esquerdo para controles
        self.control_frame = ttk.Frame(self.root, width=380, padding=8)
        self.control_frame.grid(row=0, column=0, sticky='ns')
        
        # Frame direito para Pygame
        self.pygame_frame = ttk.Frame(self.root, width=800, height=600)
        self.pygame_frame.grid(row=0, column=1, sticky='nsew')
        
        # Configurações de grid
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)
    
    def setup_control_variables(self):
        """Cria variáveis Tkinter para controles."""
        # Simulação geral
        self.control_vars.update({
            'time_scale': tk.DoubleVar(value=self.params.get('time_scale', 1.0)),
            'fps': tk.IntVar(value=self.params.get('fps', 60)),
            'paused': tk.BooleanVar(value=self.params.get('paused', False)),
            'use_spatial': tk.BooleanVar(value=self.params.get('use_spatial', True)),
            'retina_skip': tk.IntVar(value=self.params.get('retina_skip', 0)),
            'simple_render': tk.BooleanVar(value=self.params.get('simple_render', False)),
            'reuse_spatial_grid': tk.BooleanVar(value=self.params.get('reuse_spatial_grid', True)),
            
            # Substrato
            'substrate_shape': tk.StringVar(value=self.params.get('substrate_shape', 'rectangular')),
            'food_target': tk.IntVar(value=self.params.get('food_target', 50)),
            'food_min_r': tk.DoubleVar(value=self.params.get('food_min_r', 4.5)),
            'food_max_r': tk.DoubleVar(value=self.params.get('food_max_r', 5.0)),
            'food_replenish_interval': tk.DoubleVar(value=self.params.get('food_replenish_interval', 0.1)),
            'world_w': tk.DoubleVar(value=self.params.get('world_w', 1000.0)),
            'world_h': tk.DoubleVar(value=self.params.get('world_h', 700.0)),
            'substrate_radius': tk.DoubleVar(value=self.params.get('substrate_radius', 400.0)),
            
            # Bactérias (energia)
            'bacteria_count': tk.IntVar(value=self.params.get('bacteria_count', 150)),
            'bacteria_initial_energy': tk.DoubleVar(value=self.params.get('bacteria_initial_energy', 100.0)),
            'bacteria_energy_loss_idle': tk.DoubleVar(value=self.params.get('bacteria_energy_loss_idle', 0.01)),
            'bacteria_energy_loss_move': tk.DoubleVar(value=self.params.get('bacteria_energy_loss_move', 5.0)),
            'bacteria_death_energy': tk.DoubleVar(value=self.params.get('bacteria_death_energy', 0.0)),
            'bacteria_split_energy': tk.DoubleVar(value=self.params.get('bacteria_split_energy', 150.0)),
            'bacteria_body_size': tk.DoubleVar(value=self.params.get('bacteria_body_size', 9.0)),
            'bacteria_show_vision': tk.BooleanVar(value=self.params.get('bacteria_show_vision', False)),
            'bacteria_vision_radius': tk.DoubleVar(value=self.params.get('bacteria_vision_radius', 120.0)),
            'bacteria_retina_count': tk.IntVar(value=self.params.get('bacteria_retina_count', 18)),
            'bacteria_retina_fov_degrees': tk.DoubleVar(value=self.params.get('bacteria_retina_fov_degrees', 180.0)),
            'bacteria_retina_see_food': tk.BooleanVar(value=self.params.get('bacteria_retina_see_food', True)),
            'bacteria_retina_see_bacteria': tk.BooleanVar(value=self.params.get('bacteria_retina_see_bacteria', False)),
            'bacteria_retina_see_predators': tk.BooleanVar(value=self.params.get('bacteria_retina_see_predators', False)),
            'bacteria_max_speed': tk.DoubleVar(value=self.params.get('bacteria_max_speed', 300.0)),
            'bacteria_max_turn_deg': tk.DoubleVar(value=math.degrees(self.params.get('bacteria_max_turn', math.pi))),
            'bacteria_min_limit': tk.IntVar(value=self.params.get('bacteria_min_limit', 10)),
            'bacteria_max_limit': tk.IntVar(value=self.params.get('bacteria_max_limit', 300)),
            'bacteria_hidden_layers': tk.IntVar(value=self.params.get('bacteria_hidden_layers', 4)),
            'bacteria_mutation_rate': tk.DoubleVar(value=self.params.get('bacteria_mutation_rate', 0.05)),
            'bacteria_mutation_strength': tk.DoubleVar(value=self.params.get('bacteria_mutation_strength', 0.08)),
            
            # Predadores (energia)
            'predators_enabled': tk.BooleanVar(value=self.params.get('predators_enabled', False)),
            'predator_count': tk.IntVar(value=self.params.get('predator_count', 0)),
            'predator_initial_energy': tk.DoubleVar(value=self.params.get('predator_initial_energy', 100.0)),
            'predator_energy_loss_idle': tk.DoubleVar(value=self.params.get('predator_energy_loss_idle', 0.01)),
            'predator_energy_loss_move': tk.DoubleVar(value=self.params.get('predator_energy_loss_move', 5.0)),
            'predator_death_energy': tk.DoubleVar(value=self.params.get('predator_death_energy', 0.0)),
            'predator_split_energy': tk.DoubleVar(value=self.params.get('predator_split_energy', 150.0)),
            'predator_body_size': tk.DoubleVar(value=self.params.get('predator_body_size', 14.0)),
            'predator_show_vision': tk.BooleanVar(value=self.params.get('predator_show_vision', False)),
            'predator_vision_radius': tk.DoubleVar(value=self.params.get('predator_vision_radius', 120.0)),
            'predator_retina_count': tk.IntVar(value=self.params.get('predator_retina_count', 18)),
            'predator_retina_fov_degrees': tk.DoubleVar(value=self.params.get('predator_retina_fov_degrees', 180.0)),
            'predator_retina_see_food': tk.BooleanVar(value=self.params.get('predator_retina_see_food', True)),
            'predator_retina_see_bacteria': tk.BooleanVar(value=self.params.get('predator_retina_see_bacteria', True)),
            'predator_retina_see_predators': tk.BooleanVar(value=self.params.get('predator_retina_see_predators', False)),
            'predator_max_speed': tk.DoubleVar(value=self.params.get('predator_max_speed', 300.0)),
            'predator_max_turn_deg': tk.DoubleVar(value=math.degrees(self.params.get('predator_max_turn', math.pi))),
            'predator_min_limit': tk.IntVar(value=self.params.get('predator_min_limit', 0)),
            'predator_max_limit': tk.IntVar(value=self.params.get('predator_max_limit', 100)),
            'predator_hidden_layers': tk.IntVar(value=self.params.get('predator_hidden_layers', 2)),
            'predator_mutation_rate': tk.DoubleVar(value=self.params.get('predator_mutation_rate', 0.05)),
            'predator_mutation_strength': tk.DoubleVar(value=self.params.get('predator_mutation_strength', 0.08)),
            # Física global
            'agents_inertia': tk.DoubleVar(value=self.params.get('agents_inertia', 1.0)),
            
            # UI
            'show_selected_details': tk.BooleanVar(value=self.params.get('show_selected_details', True)),
        })

        # Auto export de substrato (adicionado depois para minimizar risco de conflito com bloco grande acima)
        if 'auto_export_substrate' not in self.control_vars:
            self.control_vars['auto_export_substrate'] = tk.BooleanVar(value=self.params.get('auto_export_substrate', False))
        if 'auto_export_interval_minutes' not in self.control_vars:
            self.control_vars['auto_export_interval_minutes'] = tk.DoubleVar(value=self.params.get('auto_export_interval_minutes', 10.0))
        
        # Variáveis para neurônios por camada (bactérias)
        for i in range(1, 6):
            key = f'bacteria_neurons_layer_{i}'
            self.control_vars[key] = tk.IntVar(value=self.params.get(key, 20 if i <= 4 else 0))
        
        # Variáveis para neurônios por camada (predadores)
        for i in range(1, 6):
            key = f'predator_neurons_layer_{i}'
            default = 16 if i == 1 else (8 if i == 2 else 0)
            self.control_vars[key] = tk.IntVar(value=self.params.get(key, default))
    
    def build_interface(self):
        """Constrói interface com abas."""
        # Notebook principal
        self.notebook = ttk.Notebook(self.control_frame)
        self.notebook.grid(row=0, column=0, sticky='nsew')
        self.control_frame.rowconfigure(0, weight=1)
        
        # Cria abas
        self.build_simulation_tab()
        self.build_substrate_tab()
        self.build_bacteria_tab()
        self.build_predator_tab()
        self.build_help_tab()
    
    def build_simulation_tab(self):
        """Aba de controles gerais da simulação."""
        tab = ttk.Frame(self.notebook, padding=6)
        self.notebook.add(tab, text="Simulação")
        row = 0
        # Controles de tempo
        ttk.Label(tab, text="Escala de tempo (x):").grid(row=row, column=0, sticky='w')
        ttk.Entry(tab, textvariable=self.control_vars['time_scale'], width=12).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="FPS:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=1, to=240, textvariable=self.control_vars['fps'], width=8).grid(row=row, column=1, sticky='w')
        row += 1
        # Estados da simulação
        ttk.Checkbutton(tab, text="Pausado", variable=self.control_vars['paused']).grid(row=row, column=0, sticky='w')
        ttk.Checkbutton(tab, text="Spatial Hash", variable=self.control_vars['use_spatial']).grid(row=row, column=1, sticky='w')
        row += 1
        # Performance
        ttk.Label(tab, text="Retina skip:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0, to=10, textvariable=self.control_vars['retina_skip'], width=6).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Checkbutton(tab, text="Renderização simples", variable=self.control_vars['simple_render']).grid(row=row, column=0, sticky='w')
        ttk.Checkbutton(tab, text="Reutilizar grid espacial", variable=self.control_vars['reuse_spatial_grid']).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Inércia global:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0.1, to=10.0, increment=0.1, textvariable=self.control_vars['agents_inertia'], width=8).grid(row=row, column=1, sticky='w')
        row += 1
        # Botões de controle
        ttk.Button(tab, text="Aplicar Parâmetros", command=self.apply_simulation_params).grid(row=row, column=0, pady=6)
        ttk.Button(tab, text="Resetar População", command=self.reset_population).grid(row=row, column=1, pady=6)
        row += 1
        ttk.Button(tab, text="Iniciar Simulação", command=self.start_simulation).grid(row=row, column=0, columnspan=2, pady=6)
        row += 1
        ttk.Button(tab, text="Aplicar TODOS os Parâmetros", command=self.apply_all_params).grid(row=row, column=0, columnspan=2, pady=6)
        row += 1
        ttk.Button(tab, text="Salvar Todos os Parâmetros", command=self.save_ui_params).grid(row=row, column=0, columnspan=2, pady=6)
        row += 1
        # Exportar / Carregar agente
        ttk.Button(tab, text="Exportar Agente", command=self.open_export_agent_window).grid(row=row, column=0, pady=6)
        ttk.Button(tab, text="Carregar Agente", command=self.open_load_agent_window).grid(row=row, column=1, pady=6)
        row += 1
        # Auto export substrato
        try:
            ttk.Checkbutton(tab, text="Auto Export Substrato", variable=self.control_vars['auto_export_substrate'], command=self._on_toggle_auto_export).grid(row=row, column=0, sticky='w')
            ttk.Label(tab, text="Intervalo (min):").grid(row=row, column=1, sticky='e')
            ttk.Spinbox(tab, from_=0.1, to=1440.0, increment=0.5, textvariable=self.control_vars['auto_export_interval_minutes'], width=8).grid(row=row, column=1, sticky='w', padx=(100,0))
        except Exception:
            pass


    def build_substrate_tab(self):
        """Aba de controles do substrato (comida/mundo)."""
        tab = ttk.Frame(self.notebook, padding=6)
        self.notebook.add(tab, text="Substrato")
        row = 0
        # Comida
        ttk.Label(tab, text="Target comida:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0, to=1000, textvariable=self.control_vars['food_target'], width=10).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Comida raio mín:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0.5, to=20.0, increment=0.1, textvariable=self.control_vars['food_min_r'], width=10).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Comida raio máx:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0.5, to=20.0, increment=0.1, textvariable=self.control_vars['food_max_r'], width=10).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Intervalo reposição (s):").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=0.01, to=10.0, increment=0.01, textvariable=self.control_vars['food_replenish_interval'], width=10).grid(row=row, column=1, sticky='w')
        row += 1
        # Mundo
        ttk.Label(tab, text="Largura do mundo:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=100.0, to=10000.0, increment=50.0, textvariable=self.control_vars['world_w'], width=12).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Altura do mundo:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=100.0, to=10000.0, increment=50.0, textvariable=self.control_vars['world_h'], width=12).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Formato do substrato:").grid(row=row, column=0, sticky='w')
        substrate_combo = ttk.Combobox(tab, textvariable=self.control_vars['substrate_shape'], values=["rectangular", "circular"], width=10)
        substrate_combo.grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Label(tab, text="Raio do substrato:").grid(row=row, column=0, sticky='w')
        ttk.Spinbox(tab, from_=50.0, to=1000.0, increment=10.0, textvariable=self.control_vars['substrate_radius'], width=12).grid(row=row, column=1, sticky='w')
        row += 1
        ttk.Button(tab, text="Aplicar Parâmetros", command=self.apply_substrate_params).grid(row=row, column=0, columnspan=2, pady=6)
        row += 1
        ttk.Button(tab, text="Exportar Substrato", command=self.open_export_substrate_window).grid(row=row, column=0, pady=6)
        ttk.Button(tab, text="Importar Substrato", command=self.open_import_substrate_window).grid(row=row, column=1, pady=6)
    
    def build_bacteria_tab(self):
        """Aba de controles das bactérias (energia)."""
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="Bactérias")
        canvas = tk.Canvas(tab)
        scrollbar = ttk.Scrollbar(tab, orient="vertical", command=canvas.yview)
        scrollable = ttk.Frame(canvas)
        scrollable.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=scrollable, anchor='nw')
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.grid(row=0, column=0, sticky='nsew')
        scrollbar.grid(row=0, column=1, sticky='ns')
        tab.rowconfigure(0, weight=1)
        tab.columnconfigure(0, weight=1)
        r = 0
        # População
        ttk.Label(scrollable, text="Quantidade inicial:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0, to=2000, textvariable=self.control_vars['bacteria_count'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Mínimo:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0, to=1000, textvariable=self.control_vars['bacteria_min_limit'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Máximo:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1, to=20000, textvariable=self.control_vars['bacteria_max_limit'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="ENERGIA:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Perda (idle):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=10.0, increment=0.001, textvariable=self.control_vars['bacteria_energy_loss_idle'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Perda (movimento):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=100.0, increment=0.1, textvariable=self.control_vars['bacteria_energy_loss_move'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia inicial:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=10000.0, increment=1.0, textvariable=self.control_vars['bacteria_initial_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia morte:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=1000.0, increment=1.0, textvariable=self.control_vars['bacteria_death_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia dividir:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=20000.0, increment=5.0, textvariable=self.control_vars['bacteria_split_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Tamanho corpo (raio):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1.0, to=100.0, increment=0.5, textvariable=self.control_vars['bacteria_body_size'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="VISÃO:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Raio visão:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=10.0, to=1000.0, increment=5.0, textvariable=self.control_vars['bacteria_vision_radius'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Número de retinas:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=3, to=64, increment=1, textvariable=self.control_vars['bacteria_retina_count'], width=8).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Campo de visão (°):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=10.0, to=360.0, increment=10.0, textvariable=self.control_vars['bacteria_retina_fov_degrees'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver comida:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['bacteria_retina_see_food']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver bactérias:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['bacteria_retina_see_bacteria']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver predadores:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['bacteria_retina_see_predators']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="MOVIMENTO:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Velocidade máx:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=5000.0, increment=10.0, textvariable=self.control_vars['bacteria_max_speed'], width=12).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Rotação máx (°/s):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1.0, to=1080.0, increment=5.0, textvariable=self.control_vars['bacteria_max_turn_deg'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="REDE NEURAL:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Camadas ocultas:").grid(row=r, column=0, sticky='w'); layers_spin = ttk.Spinbox(scrollable, from_=1, to=5, textvariable=self.control_vars['bacteria_hidden_layers'], width=6); layers_spin.grid(row=r, column=1, sticky='w'); r += 1
        self.bacteria_neuron_entries = []
        for i in range(1, 6):
            ttk.Label(scrollable, text=f"Neurônios camada {i}:").grid(row=r, column=0, sticky='w')
            ent = ttk.Entry(scrollable, textvariable=self.control_vars[f'bacteria_neurons_layer_{i}'], width=8)
            ent.grid(row=r, column=1, sticky='w')
            self.bacteria_neuron_entries.append(ent)
            r += 1
        def _upd():
            layers = self.control_vars['bacteria_hidden_layers'].get()
            for idx, ent in enumerate(self.bacteria_neuron_entries):
                ent.configure(state='normal' if idx < layers else 'disabled')
        self.control_vars['bacteria_hidden_layers'].trace_add('write', lambda *a: _upd())
        _upd()
        ttk.Label(scrollable, text="Taxa de mutação:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=1.0, increment=0.001, textvariable=self.control_vars['bacteria_mutation_rate'], width=8).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Força de mutação:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=5.0, increment=0.01, textvariable=self.control_vars['bacteria_mutation_strength'], width=8).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Button(scrollable, text="Aplicar Parâmetros", command=self.apply_bacteria_params).grid(row=r, column=0, columnspan=2, pady=6)
    
    def build_predator_tab(self):
        """Aba de controles dos predadores (energia)."""
        tab = ttk.Frame(self.notebook)
        self.notebook.add(tab, text="Predadores")
        canvas = tk.Canvas(tab)
        scrollbar = ttk.Scrollbar(tab, orient='vertical', command=canvas.yview)
        scrollable = ttk.Frame(canvas)
        scrollable.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        canvas.create_window((0, 0), window=scrollable, anchor='nw')
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.grid(row=0, column=0, sticky='nsew')
        scrollbar.grid(row=0, column=1, sticky='ns')
        tab.rowconfigure(0, weight=1)
        tab.columnconfigure(0, weight=1)
        r = 0
        ttk.Checkbutton(scrollable, text="Habilitar predadores", variable=self.control_vars['predators_enabled']).grid(row=r, column=0, columnspan=2, sticky='w'); r += 1
        ttk.Label(scrollable, text="Quantidade inicial:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0, to=500, textvariable=self.control_vars['predator_count'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Mínimo:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0, to=500, textvariable=self.control_vars['predator_min_limit'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Máximo:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1, to=2000, textvariable=self.control_vars['predator_max_limit'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="ENERGIA:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Perda (idle):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=10.0, increment=0.001, textvariable=self.control_vars['predator_energy_loss_idle'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Perda (movimento):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=100.0, increment=0.1, textvariable=self.control_vars['predator_energy_loss_move'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia inicial:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=20000.0, increment=1.0, textvariable=self.control_vars['predator_initial_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia morte:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=2000.0, increment=1.0, textvariable=self.control_vars['predator_death_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Energia dividir:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=40000.0, increment=10.0, textvariable=self.control_vars['predator_split_energy'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Tamanho corpo (raio):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1.0, to=300.0, increment=0.5, textvariable=self.control_vars['predator_body_size'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="VISÃO:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Raio visão:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=10.0, to=1500.0, increment=10.0, textvariable=self.control_vars['predator_vision_radius'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver comida:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['predator_retina_see_food']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver bactérias:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['predator_retina_see_bacteria']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Ver predadores:").grid(row=r, column=0, sticky='w'); ttk.Checkbutton(scrollable, variable=self.control_vars['predator_retina_see_predators']).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Qtd retinas:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=3, to=64, increment=1, textvariable=self.control_vars['predator_retina_count'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="FOV (graus):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=30, to=360, increment=10, textvariable=self.control_vars['predator_retina_fov_degrees'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="MOVIMENTO:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Velocidade máx:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=5000.0, increment=10.0, textvariable=self.control_vars['predator_max_speed'], width=12).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Rotação máx (°/s):").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=1.0, to=1080.0, increment=5.0, textvariable=self.control_vars['predator_max_turn_deg'], width=10).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Separator(scrollable, orient='horizontal').grid(row=r, column=0, columnspan=2, sticky='ew', pady=4); r += 1
        ttk.Label(scrollable, text="REDE NEURAL:").grid(row=r, column=0, sticky='w'); r += 1
        ttk.Label(scrollable, text="Camadas ocultas:").grid(row=r, column=0, sticky='w'); layers_spin = ttk.Spinbox(scrollable, from_=1, to=5, textvariable=self.control_vars['predator_hidden_layers'], width=6); layers_spin.grid(row=r, column=1, sticky='w'); r += 1
        self.predator_neuron_entries = []
        for i in range(1, 6):
            ttk.Label(scrollable, text=f"Neurônios camada {i}:").grid(row=r, column=0, sticky='w')
            ent = ttk.Entry(scrollable, textvariable=self.control_vars[f'predator_neurons_layer_{i}'], width=8)
            ent.grid(row=r, column=1, sticky='w')
            self.predator_neuron_entries.append(ent)
            r += 1
        def _upd_p():
            layers = self.control_vars['predator_hidden_layers'].get()
            for idx, ent in enumerate(self.predator_neuron_entries):
                ent.configure(state='normal' if idx < layers else 'disabled')
        self.control_vars['predator_hidden_layers'].trace_add('write', lambda *a: _upd_p())
        _upd_p()
        ttk.Label(scrollable, text="Taxa de mutação:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=1.0, increment=0.001, textvariable=self.control_vars['predator_mutation_rate'], width=8).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Label(scrollable, text="Força de mutação:").grid(row=r, column=0, sticky='w'); ttk.Spinbox(scrollable, from_=0.0, to=5.0, increment=0.01, textvariable=self.control_vars['predator_mutation_strength'], width=8).grid(row=r, column=1, sticky='w'); r += 1
        ttk.Button(scrollable, text="Aplicar Parâmetros", command=self.apply_predator_params).grid(row=r, column=0, columnspan=2, pady=6)
    
    def build_help_tab(self):
        """Aba de ajuda/instruções."""
        tab = ttk.Frame(self.notebook, padding=6)
        self.notebook.add(tab, text="Ajuda")
        
        help_text = tk.Text(tab, wrap='word', width=50, height=25)
        help_text.insert('1.0', """
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
        help_text.config(state='disabled')
        help_text.grid(row=0, column=0, sticky='nsew')
        tab.rowconfigure(0, weight=1)
        tab.columnconfigure(0, weight=1)
    
    def setup_param_callbacks(self):
        """Configura callbacks para sincronizar mudanças de parâmetros."""
        # Callbacks automáticos para parâmetros que mudança em tempo real
        real_time_params = [
            'time_scale', 'fps', 'paused', 'simple_render',
            'bacteria_show_vision', 'predator_show_vision', 'show_selected_details'
        ]
        
        for param in real_time_params:
            if param in self.control_vars:
                self.control_vars[param].trace_add('write', 
                    lambda name, index, mode, p=param: self.update_param_real_time(p))
        # Reagendar auto export quando intervalo mudar
        if 'auto_export_interval_minutes' in self.control_vars:
            try:
                self.control_vars['auto_export_interval_minutes'].trace_add('write', lambda *a: self._reschedule_auto_export())
            except Exception:
                pass
    
    def update_param_real_time(self, param_name: str):
        """Atualiza parâmetro em tempo real."""
        try:
            var = self.control_vars[param_name]
            value = var.get()
            
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
    
    def apply_simulation_params(self):
        """Aplica parâmetros de simulação."""
        try:
            params_to_update = [
                'time_scale', 'fps', 'paused', 'use_spatial',
                'retina_skip', 'simple_render', 'reuse_spatial_grid'
            ]
            
            for param in params_to_update:
                if param in self.control_vars:
                    value = self.control_vars[param].get()
                    self.params.set(param, value)
            
            print("Parâmetros de simulação aplicados")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de simulação: {e}")
    
    def apply_substrate_params(self):
        """Aplica parâmetros de substrato."""
        try:
            params_to_update = [
                'food_target', 'food_min_r', 'food_max_r', 'food_replenish_interval',
                'world_w', 'world_h', 'substrate_shape', 'substrate_radius'
            ]
            
            for param in params_to_update:
                if param in self.control_vars:
                    value = self.control_vars[param].get()
                    self.params.set(param, value)
            
            # Reconfigura mundo se shape ou radius mudou
            shape = self.params.get('substrate_shape', 'rectangular')
            radius = self.params.get('substrate_radius', 350.0)
            world = self.engine.world
            world_w = self.params.get('world_w', world.width)
            world_h = self.params.get('world_h', world.height)
            world.configure(shape, radius, world_w, world_h)
            
            # Ajusta posições de entidades para dentro do novo limite se circular
            if shape == 'circular':
                for entity in list(self.engine.agents) + list(self.engine.foods):
                    if hasattr(entity, 'x') and hasattr(entity, 'y') and hasattr(entity, 'r'):
                        entity.x, entity.y = world.clamp_position(entity.x, entity.y, getattr(entity, 'r', 0.0))
            
            print("Parâmetros de substrato aplicados (mundo atualizado)")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de substrato: {e}")
    
    def apply_bacteria_params(self):
        """Aplica parâmetros de bactérias."""
        try:
            # Lista de parâmetros de bactéria (exceto neurônios que precisam lógica especial)
            bacteria_params = [
                'bacteria_count', 'bacteria_initial_energy', 'bacteria_energy_loss_idle', 'bacteria_energy_loss_move',
                'bacteria_death_energy', 'bacteria_split_energy', 'bacteria_show_vision', 'bacteria_body_size',
                'bacteria_vision_radius', 'bacteria_retina_count', 'bacteria_retina_fov_degrees',
                'bacteria_retina_see_food', 'bacteria_retina_see_bacteria',
                'bacteria_retina_see_predators', 'bacteria_max_speed', 'bacteria_min_limit',
                'bacteria_max_limit', 'bacteria_hidden_layers', 'bacteria_mutation_rate',
                'bacteria_mutation_strength', 'bacteria_max_turn_deg'
            ]
            
            for param in bacteria_params:
                if param in self.control_vars:
                    value = self.control_vars[param].get()
                    # Converte graus para radianos se necessário
                    if param == 'bacteria_max_turn_deg':
                        param = 'bacteria_max_turn'
                        value = math.radians(value)
                    self.params.set(param, value)
            
            # Neurônios por camada
            for i in range(1, 6):
                param = f'bacteria_neurons_layer_{i}'
                if param in self.control_vars:
                    value = self.control_vars[param].get()
                    self.params.set(param, value)
            
            print("Parâmetros de bactérias aplicados")
            
        except Exception as e:
            print(f"Erro ao aplicar parâmetros de bactérias: {e}")
    
    def apply_predator_params(self):
        """Aplica parâmetros de predadores."""
        try:
            # Lista de parâmetros de predador
            predator_params = [
                'predators_enabled', 'predator_count', 'predator_initial_energy', 'predator_energy_loss_idle',
                'predator_energy_loss_move', 'predator_death_energy', 'predator_split_energy', 'predator_body_size',
                'predator_show_vision', 'predator_vision_radius', 'predator_retina_see_food',
                'predator_retina_count', 'predator_retina_fov_degrees',
                'predator_retina_see_bacteria', 'predator_retina_see_predators',
                'predator_max_speed', 'predator_min_limit', 'predator_max_limit',
                'predator_hidden_layers', 'predator_mutation_rate', 'predator_mutation_strength',
                'predator_max_turn_deg'
            ]
            
            for param in predator_params:
                if param in self.control_vars:
                    value = self.control_vars[param].get()
                    # Converte graus para radianos se necessário
                    if param == 'predator_max_turn_deg':
                        param = 'predator_max_turn'
                        value = math.radians(value)
                    self.params.set(param, value)
            
            # Neurônios por camada dos predadores
            for i in range(1, 6):
                param = f'predator_neurons_layer_{i}'
                if param in self.control_vars:
                    value = self.control_vars[param].get()
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
            # Persiste flags de auto export também
            self.params.set('auto_export_substrate', self.control_vars['auto_export_substrate'].get(), validate=False)
            self.params.set('auto_export_interval_minutes', self.control_vars['auto_export_interval_minutes'].get(), validate=False)
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
    
    def run(self):
        """Executa interface Tkinter.""" 
        # Inicializa Pygame embedded
        self.root.update_idletasks()
        self.pygame_frame.update_idletasks()
        
        # Setup para embedding
        window_id = self.pygame_frame.winfo_id()
        self.pygame_view.initialize(window_id)
        
        # Thread da simulação
        def simulation_thread():
            try:
                self.pygame_view.run()
            except Exception as e:
                print(f"Erro na thread da simulação: {e}")
        
        self.sim_thread = threading.Thread(target=simulation_thread, daemon=True)
        self.sim_thread.start()
        
        # Protocolo de fechamento
        def on_closing():
            try:
                self.pygame_view.stop()
                self.pygame_view.cleanup()
            except Exception as e:
                print(f"Erro ao fechar: {e}")
            finally:
                self.root.destroy()
        
        self.root.protocol("WM_DELETE_WINDOW", on_closing)
        
        # Mainloop Tkinter
        self.root.mainloop()

    # --------------------- Persistência UI ---------------------
    def save_ui_params(self):
        """Salva todos os valores das variáveis de controle em CSV."""
        try:
            fieldnames = sorted(self.control_vars.keys())
            rows = []
            for name in fieldnames:
                var = self.control_vars[name]
                try:
                    value = var.get()
                except Exception:
                    value = ''
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
                        var = self.control_vars[name]
                        # Converte tipo conforme a classe da variável
                        try:
                            if isinstance(var, tk.BooleanVar):
                                # Aceita 'True'/'False' ou '1'/'0'
                                var.set(value in ('1', 'True', 'true'))
                            elif isinstance(var, tk.IntVar):
                                var.set(int(float(value)))
                            elif isinstance(var, tk.DoubleVar):
                                var.set(float(value))
                            elif isinstance(var, tk.StringVar):
                                var.set(value)
                        except Exception:
                            pass
            print(f"Parâmetros UI carregados de {self._ui_params_csv}")
            # Agenda auto export se estava ativo
            try:
                if self.control_vars['auto_export_substrate'].get():
                    self._schedule_next_auto_export(initial=True)
            except Exception:
                pass
        except Exception as e:
            print(f"Erro ao carregar parâmetros UI: {e}")

    # --------------------- Exportar / Carregar Agente ---------------------
    def open_export_agent_window(self):
        """Abre janela para exportar agente selecionado."""
        agent = self.engine.selected_agent
        if agent is None:
            messagebox.showinfo("Exportar Agente", "Selecione um agente na área da simulação primeiro (clique sobre ele)."); return
        win = tk.Toplevel(self.root)
        win.title("Exportar Agente")
        ttk.Label(win, text="Nome do agente:").grid(row=0, column=0, padx=6, pady=6)
        name_var = tk.StringVar(value="agente")
        entry = ttk.Entry(win, textvariable=name_var, width=28)
        entry.grid(row=0, column=1, padx=6, pady=6)
        status_var = tk.StringVar(value="")
        ttk.Label(win, textvariable=status_var, foreground="gray").grid(row=2, column=0, columnspan=2, padx=6, pady=(0,6))

        def do_export():
            name = name_var.get().strip()
            if not name:
                status_var.set("Informe um nome."); return
            try:
                path = self._export_selected_agent(agent, name)
                status_var.set(f"Exportado: {os.path.basename(path)}")
            except Exception as e:
                status_var.set(f"Erro: {e}")
        ttk.Button(win, text="Exportar", command=do_export).grid(row=1, column=0, columnspan=2, pady=6)
        entry.focus_set()

    def _export_selected_agent(self, agent, name: str) -> str:
        """Exporta agente para CSV (key,value). Retorna caminho."""
        # Pasta 'agents' no root do projeto (um nível acima de sim/)
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'agents'))
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
        add('x', agent.x); add('y', agent.y); add('r', agent.r)
        add('angle', agent.angle); add('vx', agent.vx); add('vy', agent.vy)
        add('energy', getattr(agent, 'energy', 0.0)); add('age', getattr(agent, 'age', 0.0))
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
                    w_list = list(W); b_list = list(B)
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
            writer.writeheader(); writer.writerows(rows)
        print(f"Agente exportado para {path}")
        return path

    def open_load_agent_window(self):
        """Abre janela listando agentes exportados para carregar."""
        agents_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'agents'))
        os.makedirs(agents_dir, exist_ok=True)
        files = [f for f in os.listdir(agents_dir) if f.endswith('.agent.csv')]
        if not files:
            messagebox.showinfo("Carregar Agente", "Nenhum arquivo .agent.csv encontrado na pasta 'agents'."); return
        win = tk.Toplevel(self.root)
        win.title("Carregar Agente")
        ttk.Label(win, text="Escolha o agente exportado:").pack(padx=6, pady=6)
        listbox = tk.Listbox(win, height=min(12, len(files)), width=40)
        for f in files: listbox.insert(tk.END, f)
        listbox.pack(padx=6, pady=6, fill='both')
        status_var = tk.StringVar(value="")
        ttk.Label(win, textvariable=status_var, foreground='gray').pack(padx=6, pady=(0,6))

        def do_load():
            sel = listbox.curselection()
            if not sel:
                status_var.set("Selecione um arquivo."); return
            filename = files[sel[0]]
            path = os.path.join(agents_dir, filename)
            try:
                self._load_agent_from_csv(path)
                status_var.set("Carregado com sucesso.")
            except Exception as e:
                status_var.set(f"Erro: {e}")
        ttk.Button(win, text="Carregar", command=do_load).pack(padx=6, pady=6)
        def on_double(event): do_load()
        listbox.bind('<Double-1>', on_double)

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

    # --------------------- Exportar / Importar Substrato ---------------------
    def _get_substrate_dirs(self):
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
        base = os.path.join(root_dir, 'substrates')
        manual = os.path.join(base, 'manual_exports')
        auto = os.path.join(base, 'auto_exports')
        for d in (base, manual, auto):
            os.makedirs(d, exist_ok=True)
        return manual, auto

    def open_export_substrate_window(self):
        win = tk.Toplevel(self.root)
        win.title("Exportar Substrato")
        ttk.Label(win, text="Nome (prefixo):").grid(row=0, column=0, padx=6, pady=6)
        name_var = tk.StringVar(value="substrato")
        ttk.Entry(win, textvariable=name_var, width=28).grid(row=0, column=1, padx=6, pady=6)
        status = tk.StringVar(value="")
        ttk.Label(win, textvariable=status, foreground='gray').grid(row=2, column=0, columnspan=2, padx=6, pady=(0,6))
        def do_export():
            prefix = name_var.get().strip() or 'substrato'
            try:
                path = self._export_substrate(prefix=prefix, manual=True)
                status.set(f"Exportado: {os.path.basename(path)}")
            except Exception as e:
                status.set(f"Erro: {e}")
        ttk.Button(win, text="Exportar", command=do_export).grid(row=1, column=0, columnspan=2, pady=6)

    def open_import_substrate_window(self):
        manual_dir, auto_dir = self._get_substrate_dirs()
        files = []
        for d in (manual_dir, auto_dir):
            try:
                for f in os.listdir(d):
                    if f.endswith('.json'):
                        files.append((os.path.join(d, f), f))
            except Exception:
                pass
        if not files:
            messagebox.showinfo("Importar Substrato", "Nenhum snapshot encontrado.")
            return
        files.sort(key=lambda x: x[1], reverse=True)
        win = tk.Toplevel(self.root)
        win.title("Importar Substrato")
        ttk.Label(win, text="Escolha snapshot:").pack(padx=6, pady=6)
        listbox = tk.Listbox(win, height=min(15, len(files)), width=60)
        for _, name in files:
            listbox.insert(tk.END, name)
        listbox.pack(padx=6, pady=6, fill='both')
        status = tk.StringVar(value="")
        ttk.Label(win, textvariable=status, foreground='gray').pack(padx=6, pady=(0,6))
        def do_import():
            sel = listbox.curselection()
            if not sel:
                status.set("Selecione um arquivo."); return
            path = files[sel[0]][0]
            try:
                self._import_substrate(path)
                status.set("Importado.")
            except Exception as e:
                status.set(f"Erro: {e}")
        ttk.Button(win, text="Importar", command=do_import).pack(padx=6, pady=6)
        listbox.bind('<Double-1>', lambda e: do_import())

    def _export_substrate(self, prefix: str = 'substrato', manual: bool = True) -> str:
        import json, time
        prev_paused = self.control_vars['paused'].get()
        self.control_vars['paused'].set(True)
        self.params.set('paused', True, validate=False)
        self.root.update_idletasks()
        try:
            engine = self.engine
            manual_dir, auto_dir = self._get_substrate_dirs()
            ts = time.strftime('%Y%m%d_%H%M%S')  # timestamp completo (para manual)
            out_dir = manual_dir if manual else auto_dir
            if manual:
                filename = f"{prefix}_{ts}.json"
            else:
                # Formato requerido: autosave_substrate_DD.MM.YYYY_XX.json
                date_br = time.strftime('%d.%m.%Y')
                base_pref = f"autosave_substrate_{date_br}_"
                # Descobre próximo índice sequencial do dia
                try:
                    existing = [f for f in os.listdir(out_dir) if f.startswith(base_pref) and f.endswith('.json')]
                except Exception:
                    existing = []
                seq = 1
                if existing:
                    import re
                    pat = re.compile(rf"^autosave_substrate_{re.escape(date_br)}_(\d+)\.json$")
                    nums = []
                    for fname in existing:
                        m = pat.match(fname)
                        if m:
                            try: nums.append(int(m.group(1)))
                            except: pass
                    if nums:
                        seq = max(nums) + 1
                filename = f"{base_pref}{seq:02d}.json"
            path = os.path.join(out_dir, filename)
            # Snapshot params & ui
            params_snapshot = dict(self.params._data)
            ui_snapshot = {}
            for k, var in self.control_vars.items():
                try: ui_snapshot[k] = var.get()
                except Exception: pass
            world = engine.world
            camera = engine.camera
            agents_data = []
            for agent in engine.all_agents:
                brain = getattr(agent, 'brain', None)
                sensor = getattr(agent, 'sensor', None)
                locomotion = getattr(agent, 'locomotion', None)
                energy_model = getattr(agent, 'energy_model', None)
                ad = {
                    'type': 'predator' if getattr(agent, 'is_predator', False) else 'bacteria',
                    'x': agent.x, 'y': agent.y, 'r': agent.r,
                    'angle': agent.angle, 'vx': agent.vx, 'vy': agent.vy,
                    'energy': getattr(agent, 'energy', 0.0), 'age': getattr(agent, 'age', 0.0)
                }
                if brain and hasattr(brain, 'sizes'):
                    ad['brain_sizes'] = list(brain.sizes)
                    ad['brain_version'] = getattr(brain, 'version', 0)
                    try:
                        ad['brain_weights'] = [w.tolist() if hasattr(w, 'tolist') else list(w) for w in brain.weights]
                        ad['brain_biases'] = [b.tolist() if hasattr(b, 'tolist') else list(b) for b in brain.biases]
                    except Exception:
                        ad['brain_weights'] = [list(w) for w in brain.weights]
                        ad['brain_biases'] = [list(b) for b in brain.biases]
                if sensor:
                    for attr in ['retina_count','vision_radius','fov_degrees','skip','see_food','see_bacteria','see_predators']:
                        if hasattr(sensor, attr):
                            ad[f'sensor_{attr}'] = getattr(sensor, attr)
                if locomotion:
                    for attr in ['max_speed','max_turn']:
                        if hasattr(locomotion, attr):
                            ad[f'locomotion_{attr}'] = getattr(locomotion, attr)
                if energy_model:
                    for attr in ['loss_idle','loss_move','death_energy','split_energy']:
                        if hasattr(energy_model, attr):
                            ad[f'energy_{attr}'] = getattr(energy_model, attr)
                ad['last_brain_output'] = getattr(agent, 'last_brain_output', [])
                ad['last_brain_activations'] = getattr(agent, 'last_brain_activations', [])
                agents_data.append(ad)
            snapshot = {
                'version': 1,
                'timestamp': ts,
                'params': params_snapshot,
                'ui_params': ui_snapshot,
                'world': {'width': world.width, 'height': world.height, 'shape': world.shape, 'radius': world.radius},
                'camera': {'x': camera.x, 'y': camera.y, 'zoom': camera.zoom},
                'simulation': {'total_simulation_time': engine.total_simulation_time},
                'food': {'count': len(engine.entities['foods']), 'target': self.params.get('food_target', 0)},
                'agents': agents_data
            }
            with open(path, 'w', encoding='utf-8') as f:
                json.dump(snapshot, f)
            print(f"Substrato exportado para {path}")
            return path
        finally:
            self.control_vars['paused'].set(prev_paused)
            self.params.set('paused', prev_paused, validate=False)

    def _import_substrate(self, path: str):
        import json, math
        prev_paused = self.control_vars['paused'].get()
        self.control_vars['paused'].set(True)
        self.params.set('paused', True, validate=False)
        try:
            with open(path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            # Params
            for k, v in data.get('params', {}).items():
                self.params.set(k, v, validate=False)
                if k in self.control_vars:
                    var = self.control_vars[k]
                    try:
                        if isinstance(var, tk.BooleanVar): var.set(bool(v))
                        elif isinstance(var, tk.IntVar): var.set(int(v))
                        elif isinstance(var, tk.DoubleVar): var.set(float(v))
                        elif isinstance(var, tk.StringVar): var.set(str(v))
                    except Exception:
                        pass
            # Mundo
            wdata = data.get('world', {})
            world = self.engine.world
            world.configure(wdata.get('shape', world.shape), wdata.get('radius', world.radius), wdata.get('width', world.width), wdata.get('height', world.height))
            # Camera
            cdata = data.get('camera', {})
            cam = self.engine.camera
            cam.x = cdata.get('x', cam.x); cam.y = cdata.get('y', cam.y); cam.zoom = cdata.get('zoom', cam.zoom)
            # Limpa
            for lst in self.engine.entities.values(): lst.clear()
            self.engine.all_agents.clear()
            self.engine.selected_agent = None
            # Comida (recria aleatória)
            from .entities import create_random_food
            food_count = int(data.get('food', {}).get('count', 0))
            for _ in range(food_count):
                food = create_random_food(self.engine.entities['foods'], self.params, world.width, world.height)
                self.engine.entities['foods'].append(food)
            # Agentes
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
                        bw = ad.get('brain_weights', [])
                        bb = ad.get('brain_biases', [])
                        if bw and bb and len(bw) == len(bb):
                            brain.weights = bw
                            brain.biases = bb
                    except Exception:
                        pass
                sensor = RetinaSensor(
                    retina_count=ad.get('sensor_retina_count', 18),
                    vision_radius=ad.get('sensor_vision_radius', 120.0),
                    fov_degrees=ad.get('sensor_fov_degrees', 180.0),
                    skip=ad.get('sensor_skip', 0),
                    see_food=ad.get('sensor_see_food', True),
                    see_bacteria=ad.get('sensor_see_bacteria', False),
                    see_predators=ad.get('sensor_see_predators', False)
                )
                locomotion = Locomotion(max_speed=ad.get('locomotion_max_speed', 300.0), max_turn=ad.get('locomotion_max_turn', math.pi))
                energy_model = EnergyModel(
                    loss_idle=ad.get('energy_loss_idle', 0.01),
                    loss_move=ad.get('energy_loss_move', 5.0),
                    death_energy=ad.get('energy_death_energy', 0.0),
                    split_energy=ad.get('energy_split_energy', 150.0)
                )
                cls = Predator if ad.get('type') == 'predator' else Bacteria
                agent = cls(ad.get('x', 0.0), ad.get('y', 0.0), ad.get('r', 9.0), brain, sensor, locomotion, energy_model, ad.get('angle', 0.0))
                agent.vx = ad.get('vx', 0.0); agent.vy = ad.get('vy', 0.0)
                agent.energy = ad.get('energy', 0.0); agent.age = ad.get('age', 0.0)
                agent.last_brain_output = ad.get('last_brain_output', [])
                agent.last_brain_activations = ad.get('last_brain_activations', [])
                if agent.is_predator:
                    self.engine.entities['predators'].append(agent)
                else:
                    self.engine.entities['bacteria'].append(agent)
                self.engine.all_agents.append(agent)
            print(f"Substrato importado de {path}")
        finally:
            self.control_vars['paused'].set(prev_paused)
            self.params.set('paused', prev_paused, validate=False)

    # --------------------- Auto Export ---------------------
    def _on_toggle_auto_export(self):
        active = self.control_vars['auto_export_substrate'].get()
        self.params.set('auto_export_substrate', active, validate=False)
        if active:
            self._reschedule_auto_export()
        else:
            if hasattr(self, '_auto_export_after_id') and self._auto_export_after_id:
                try: self.root.after_cancel(self._auto_export_after_id)
                except Exception: pass
                self._auto_export_after_id = None

    def _reschedule_auto_export(self):
        if not self.control_vars['auto_export_substrate'].get():
            return
        if hasattr(self, '_auto_export_after_id') and self._auto_export_after_id:
            try: self.root.after_cancel(self._auto_export_after_id)
            except Exception: pass
            self._auto_export_after_id = None
        self._schedule_next_auto_export()

    def _schedule_next_auto_export(self, initial: bool = False):
        if not self.control_vars['auto_export_substrate'].get():
            return
        try:
            minutes = float(self.control_vars['auto_export_interval_minutes'].get())
        except Exception:
            minutes = 10.0
        delay_ms = max(1, int(minutes * 60_000))
        self._auto_export_after_id = self.root.after(delay_ms, self._perform_auto_export)
        print(f"Auto export substrato agendado em {minutes} min.")

    def _perform_auto_export(self):
        if not self.control_vars['auto_export_substrate'].get():
            return
        try:
            self._export_substrate(manual=False)
        except Exception as e:
            print(f"Erro em auto export substrato: {e}")
        finally:
            self._schedule_next_auto_export()
