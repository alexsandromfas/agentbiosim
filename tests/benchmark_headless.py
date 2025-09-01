## MODIFICADO PARA GIT
"""Script headless para medir performance inicial.

Mede tempos agregados de várias seções com população fixa:
 - 100 bactérias
 - 100 predadores
 - 100 comidas (target)

Executa por um número de passos simulando ~5 segundos de tempo de mundo.
Imprime relatório no final.
"""
from __future__ import annotations
import os
import sys
import time

# When this script is executed directly (python tests/benchmark_headless.py)
# sys.path[0] is the tests/ directory which makes `import sim` fail. Ensure the x
# project root is on sys.path so imports like `from sim.*` work both when
# running as a module (python -m tests.benchmark_headless) and as a script.
ROOT = os.path.dirname(os.path.dirname(__file__))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from sim.controllers import Params
from sim.world import World, Camera
from sim.engine import Engine
from sim.profiler import profiler


def configure_params() -> Params:
    p = Params()
    p.set('bacteria_count', 100)
    p.set('predators_enabled', True)
    p.set('predator_count', 100)
    p.set('food_target', 100)
    p.set('time_scale', 1.0)
    p.set('paused', False)
    # Garantir retina moderada
    p.set('bacteria_retina_count', 18)
    p.set('predator_retina_count', 18)
    # Evitar visão desenhada
    p.set('bacteria_show_vision', False)
    p.set('predator_show_vision', False)
    # Evitar reprodução (massas de split muito altas)
    p.set('bacteria_split_mass', 1e9)
    p.set('predator_split_mass', 1e9)
    return p


def _slug(label: str) -> str:
    import re
    s = label.strip().lower()
    s = re.sub(r'[^a-z0-9_\-]+', '_', s)
    s = re.sub(r'_+', '_', s).strip('_')
    return s or 'sem_nome'


def run_benchmark(sim_seconds: float = 30.0, fps: int = 60, label: str | None = None, disable_activations: bool = False):
    params = configure_params()
    world = World(width=params.get('world_w', 1000.0), height=params.get('world_h', 700.0),
                  shape=params.get('substrate_shape', 'rectangular'), radius=params.get('substrate_radius', 400.0))
    camera = Camera()
    engine = Engine(world, camera, params, headless=True)
    engine.start()
    print('Benchmark iniciado', flush=True)

    # Habilita profiler apenas durante benchmark
    prev_prof_enabled = profiler.enabled
    profiler.enabled = True
    if disable_activations:
        params.set('disable_brain_activations', True, validate=False)

    dt_real = 1.0 / fps
    sim_time = 0.0
    initial_bact = len(engine.entities['bacteria'])
    initial_pred = len(engine.entities['predators'])
    sum_agents = 0
    wall_start = time.perf_counter()
    steps = 0
    next_progress = 0.0
    while sim_time < sim_seconds:
        engine.step(dt_real)
        sim_time += dt_real
        steps += 1
        sum_agents += (len(engine.entities['bacteria']) + len(engine.entities['predators']))

        if sim_time >= next_progress:
            pct = (sim_time / sim_seconds) * 100.0
            print(f".. {sim_time:6.2f}s sim ({pct:5.1f}%) steps={steps}", flush=True)
            next_progress += max(0.5, sim_seconds / 60.0)  # ~60 updates de progresso
    wall_end = time.perf_counter()
    profiler.total_wall = wall_end - wall_start

    print("==== BENCHMARK HEADLESS ====")
    print(f"Passos: {steps}  | Tempo simulado: {sim_time:.2f}s  | Wall: {profiler.total_wall:.3f}s")
    print(f"Bactérias finais: {len(engine.entities['bacteria'])}")
    print(f"Predadores finais: {len(engine.entities['predators'])}")
    print(f"Comidas finais: {len(engine.entities['foods'])}")
    print()
    print(profiler.report())
    # Restaura estado original do profiler
    profiler.enabled = prev_prof_enabled
    print('Benchmark concluido', flush=True)

    avg_agents = (sum_agents / steps) if steps else 0.0

    # Exportação
    if label:
        import json, os, datetime
        from sim.profiler import profiler as _prof
        export_dir = os.path.join(os.path.dirname(__file__), 'otimizacoes')
        os.makedirs(export_dir, exist_ok=True)
        slug = _slug(label)
        file_path = os.path.join(export_dir, f'{slug}.json')
        ms_per_agent = ( (_prof.total_wall*1000.0) / (avg_agents*steps) ) if (avg_agents>0 and steps>0) else None
        data = {
            'label': label,
            'slug': slug,
            'timestamp': datetime.datetime.utcnow().isoformat() + 'Z',
            'seconds_requested': sim_seconds,
            'fps_target': fps,
            'steps': steps,
            'sim_time_s': sim_time,
            'wall_s': _prof.total_wall,
            'initial': {'bacteria': initial_bact, 'predators': initial_pred},
            'final': {'bacteria': len(engine.entities['bacteria']), 'predators': len(engine.entities['predators']), 'foods': len(engine.entities['foods'])},
            'avg_agents': avg_agents,
            'ms_per_agent_step': ms_per_agent,
            'params_snapshot': params._data,  # interno mas útil
            'sections': _prof.snapshot(),
            'note_overlap': 'Somas de seções se sobrepõem (nested); usar wall_s para % real.'
        }
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        print(f"[export] Resultado salvo em {file_path}")



if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--seconds', type=float, default=30.0, help='Tempo de simulação (s)')
    ap.add_argument('--fps', type=int, default=60, help='FPS lógico (passos/seg)')
    ap.add_argument('--label', type=str, default=None, help='Rótulo da otimização / experimento para exportação')
    ap.add_argument('--no-activations', action='store_true', help='Desliga cálculo de activations do cérebro para reduzir overhead')
    args = ap.parse_args()

    label = args.label
    if label is None:
        try:
            label = input('Digite o título da otimização/relatório: ').strip()
        except EOFError:
            label = None
    run_benchmark(sim_seconds=args.seconds, fps=args.fps, label=label, disable_activations=args.no_activations)
