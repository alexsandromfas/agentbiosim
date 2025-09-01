## MODIFICADO PARA GIT
"""Diagnóstico: compara forward_batch(agents[0].brain) com forward por agente.

Não altera código de produção. Imprime estatísticas por grupo de arquitetura.
"""
import os
import sys
import math
import numpy as np

# Garantir que o root do projeto esteja no sys.path ao rodar como módulo
ROOT = os.path.dirname(os.path.dirname(__file__))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from sim.controllers import Params
from sim.world import World, Camera
from sim.engine import Engine
from sim.sensors import batch_retina_sense


def analyze(engine: Engine, sample_limit_per_group: int = 32):
    # Executa um passo rápido para popular spatial hash/scene
    engine.step(1.0 / 60.0)

    # Agrupa como faz o engine
    agent_groups = {}
    for agent in engine.all_agents:
        key = (type(agent), tuple(agent.brain.sizes) if hasattr(agent.brain, 'sizes') else None)
        agent_groups.setdefault(key, []).append(agent)

    overall = []
    for key, group in agent_groups.items():
        if len(group) < 2:
            continue
        agents = group[: min(len(group), sample_limit_per_group)]

        # Coleta inputs via batch_retina_sense (mesmo usado pela engine)
        inputs_list = batch_retina_sense(agents, engine.scene_query, engine.params)
        if not inputs_list:
            print(f"[group {key}] sem inputs calculados (vazios)")
            continue

        inputs_np = np.array(inputs_list, dtype=np.float32)

        # Saída batched correta usando vários cérebros
        try:
            from sim.brain import forward_many_brains
            batched_out = forward_many_brains([a.brain for a in agents], inputs_np)
        except Exception as e:
            print(f"[group {key}] forward_many_brains falhou: {e}")
            continue

        # Saídas individuais usando cada brain
        per_outs = []
        for i, inp in enumerate(inputs_list):
            out = agents[i].brain.forward(inp)
            per_outs.append(out)
        per_outs_np = np.array(per_outs, dtype=np.float32)

        # Compare shapes
        if batched_out.shape != per_outs_np.shape:
            print(f"[group {key}] shapes differ: batched {batched_out.shape} vs per {per_outs_np.shape}")
            continue

    diffs = np.abs(batched_out - per_outs_np)
    # Para float32, diferenças de ordem 1e-7 são normais; usar tolerância mais realista
    tol = 1e-6
    mismatches = diffs > tol
    frac_mismatch = mismatches.sum() / mismatches.size
    mean_diff = float(diffs.mean())
    max_diff = float(diffs.max())

    status = "OK" if max_diff < 5e-6 else "DIF"
    print(f"[group {key}] agents={len(group)} sampled={len(agents)} frac_mismatch={frac_mismatch:.4f} mean_diff={mean_diff:.3e} max_diff={max_diff:.3e} tol={tol} status={status}")
    overall.append((key, len(group), len(agents), frac_mismatch, mean_diff, max_diff))

    if not overall:
        print("Nenhum grupo comparável encontrado (população pequena?)")
    else:
        avg_frac = sum(o[3] for o in overall) / len(overall)
        print(f"Resumo: grupos={len(overall)} avg_frac_mismatch={avg_frac:.4f}")


def main():
    params = Params()
    # usar população pequena, suficiente para formar grupos
    params.set('bacteria_count', 50)
    params.set('predators_enabled', False)

    world = World(width=params.get('world_w', 1000.0), height=params.get('world_h', 700.0),
                  shape=params.get('substrate_shape', 'rectangular'), radius=params.get('substrate_radius', 400.0))
    camera = Camera()
    engine = Engine(world, camera, params, headless=True)
    engine.start()

    print("Running diagnostic: comparing batched forward vs per-agent forward")
    analyze(engine)


if __name__ == '__main__':
    main()
