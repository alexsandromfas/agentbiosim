## MODIFICADO PARA GIT
"""Benchmark/regression runner for the fixed test substrate.

This script runs the real simulation engine in headless mode from a substrate
snapshot and writes rich aggregate metrics for future comparisons. Heavy
analysis is kept here, outside the normal UI/runtime path.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import json
import math
import os
import platform
import random
import subprocess
import sys
import time
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from sim.actuators import EnergyModel, Locomotion
from sim.brain import NeuralNet, get_multi_brain_cache_stats
from sim.controllers import Params
from sim.engine import Engine
from sim.entities import Bacteria, Food, Predator, create_random_food
from sim.profiler import profiler
from sim.sensors import RetinaSensor
from sim.spatial import SpatialHash
from sim.world import Camera, World


DEFAULT_SUBSTRATE = ROOT / "tests" / "substrato_de_teste" / "substrato_de_teste_20260515_210719.json"
DEFAULT_OUTPUT_DIR = ROOT / "tests" / "substrato_de_teste" / "resultados_benchmark"
DEFAULT_FOOD_LAYOUT_DIR = ROOT / "tests" / "substrato_de_teste" / "layouts"
FACTORS_LOG = ROOT / "tests" / "substrato_de_teste" / "fatores_comparativos.jsonl"

_ORIGINAL_QUERY_BALL = SpatialHash.query_ball


def _stable_entity_key(obj: Any) -> tuple[Any, ...]:
    def finite(value: Any) -> float:
        try:
            out = float(value)
        except Exception:
            return 0.0
        return out if math.isfinite(out) else 0.0

    color = getattr(obj, "color", ())
    if isinstance(color, (list, tuple)):
        color_key = tuple(int(c) for c in color[:3])
    else:
        color_key = ()
    return (
        int(getattr(obj, "type_code", -1)),
        finite(getattr(obj, "x", 0.0)),
        finite(getattr(obj, "y", 0.0)),
        finite(getattr(obj, "r", 0.0)),
        finite(getattr(obj, "energy", 0.0)),
        finite(getattr(obj, "age", 0.0)),
        color_key,
    )


def _install_benchmark_deterministic_queries() -> None:
    if getattr(SpatialHash.query_ball, "_benchmark_deterministic", False):
        return

    def query_ball_deterministic(self: SpatialHash, x: float, y: float, r: float):
        return tuple(sorted(_ORIGINAL_QUERY_BALL(self, x, y, r), key=_stable_entity_key))

    query_ball_deterministic._benchmark_deterministic = True
    SpatialHash.query_ball = query_ball_deterministic


def _slug(text: str) -> str:
    import re

    text = text.strip().lower()
    text = re.sub(r"[^a-z0-9_\-]+", "_", text)
    text = re.sub(r"_+", "_", text).strip("_")
    return text or "run"


def _git_commit() -> str | None:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=ROOT,
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except Exception:
        return None


def _safe_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except Exception:
        return default


def _safe_bool(value: Any, default: bool = False) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        return value.strip().lower() in {"1", "true", "yes", "y", "sim"}
    if value is None:
        return default
    return bool(value)


def _stats(values: Iterable[float]) -> dict[str, Any]:
    arr = np.asarray(list(values), dtype=np.float64)
    arr = arr[np.isfinite(arr)]
    if arr.size == 0:
        return {
            "count": 0,
            "mean": None,
            "std": None,
            "min": None,
            "p05": None,
            "p25": None,
            "p50": None,
            "p75": None,
            "p95": None,
            "max": None,
        }
    return {
        "count": int(arr.size),
        "mean": float(arr.mean()),
        "std": float(arr.std()),
        "min": float(arr.min()),
        "p05": float(np.percentile(arr, 5)),
        "p25": float(np.percentile(arr, 25)),
        "p50": float(np.percentile(arr, 50)),
        "p75": float(np.percentile(arr, 75)),
        "p95": float(np.percentile(arr, 95)),
        "max": float(arr.max()),
    }


def _top_counter(values: Iterable[Any], limit: int = 8) -> list[dict[str, Any]]:
    return [{"value": str(k), "count": int(v)} for k, v in Counter(values).most_common(limit)]


def _pick(mapping: dict[str, Any], *names: str, default: Any = None) -> Any:
    for name in names:
        if name in mapping:
            return mapping[name]
    return default


def _build_brain(agent_data: dict[str, Any]) -> NeuralNet:
    sizes = list(agent_data.get("brain_sizes") or [1, 2])
    brain = NeuralNet(sizes, init_std=0.01)
    weights = agent_data.get("brain_weights")
    biases = agent_data.get("brain_biases")
    if weights and biases and len(weights) == len(biases):
        brain.weights = [np.asarray(w, dtype=np.float32) for w in weights]
        brain.biases = [np.asarray(b, dtype=np.float32) for b in biases]
    brain.version = int(agent_data.get("brain_version", 0))
    return brain


def _build_agent(agent_data: dict[str, Any], params: Params):
    brain = _build_brain(agent_data)
    raw_channels = agent_data.get("sensor_channels", ("d",))
    if isinstance(raw_channels, str):
        try:
            raw_channels = json.loads(raw_channels)
        except Exception:
            raw_channels = [raw_channels]
    sensor = RetinaSensor(
        retina_count=int(agent_data.get("sensor_retina_count", 18)),
        vision_radius=_safe_float(agent_data.get("sensor_vision_radius", 120.0), 120.0),
        fov_degrees=_safe_float(agent_data.get("sensor_fov_degrees", 180.0), 180.0),
        skip=int(agent_data.get("sensor_skip", params.get("retina_skip", 0))),
        see_food=_safe_bool(agent_data.get("sensor_see_food", True), True),
        see_bacteria=_safe_bool(agent_data.get("sensor_see_bacteria", False), False),
        see_predators=_safe_bool(agent_data.get("sensor_see_predators", False), False),
        channels=raw_channels,
    )
    locomotion = Locomotion(
        max_speed=_safe_float(agent_data.get("locomotion_max_speed", 300.0), 300.0),
        max_turn=_safe_float(agent_data.get("locomotion_max_turn", math.pi), math.pi),
    )
    is_predator = agent_data.get("type") == "predator"
    energy_model = EnergyModel(
        death_energy=_safe_float(_pick(agent_data, "energy_death_energy", "death_energy", default=0.0)),
        split_energy=_safe_float(_pick(agent_data, "energy_split_energy", "split_energy", default=150.0)),
        v0_cost=_safe_float(_pick(agent_data, "energy_v0_cost", "metab_v0_cost", "energy_loss_idle", default=0.5)),
        vmax_cost=_safe_float(_pick(agent_data, "energy_vmax_cost", "metab_vmax_cost", "energy_loss_move", default=8.0)),
        vmax_ref=_safe_float(_pick(agent_data, "energy_vmax_ref", "locomotion_max_speed", default=300.0)),
        energy_cap=_safe_float(
            _pick(agent_data, "energy_energy_cap", "energy_cap", default=(600.0 if is_predator else 400.0))
        ),
    )
    cls = Predator if is_predator else Bacteria
    agent = cls(
        _safe_float(agent_data.get("x", 0.0)),
        _safe_float(agent_data.get("y", 0.0)),
        _safe_float(agent_data.get("r", 9.0)),
        brain,
        sensor,
        locomotion,
        energy_model,
        _safe_float(agent_data.get("angle", 0.0)),
    )
    agent.vx = _safe_float(agent_data.get("vx", 0.0))
    agent.vy = _safe_float(agent_data.get("vy", 0.0))
    agent.energy = _safe_float(agent_data.get("energy", 0.0))
    agent.age = _safe_float(agent_data.get("age", 0.0))
    if "last_reproduction_age" in agent_data:
        agent.last_reproduction_age = _safe_float(agent_data.get("last_reproduction_age"), agent.age)
    if "color" in agent_data:
        color = agent_data.get("color")
        if isinstance(color, list) and len(color) >= 3:
            agent.color = tuple(int(c) for c in color[:3])
    agent.diet_food = _safe_bool(
        agent_data.get("diet_food", getattr(agent, "diet_food", not agent.is_predator)),
        not agent.is_predator,
    )
    agent.diet_agents = _safe_bool(
        agent_data.get("diet_agents", getattr(agent, "diet_agents", agent.is_predator)),
        agent.is_predator,
    )
    agent.diet_same_label = _safe_bool(
        agent_data.get("diet_same_label", getattr(agent, "diet_same_label", False)),
        False,
    )
    agent.diet_food_efficiency = _safe_float(
        agent_data.get("diet_food_efficiency", getattr(agent, "diet_food_efficiency", 1.0)),
        1.0,
    )
    agent.diet_agent_efficiency = _safe_float(
        agent_data.get("diet_agent_efficiency", getattr(agent, "diet_agent_efficiency", 0.7)),
        0.7,
    )
    return agent


def _food_to_dict(food: Food) -> dict[str, Any]:
    return {
        "x": float(food.x),
        "y": float(food.y),
        "r": float(food.r),
        "energy": float(getattr(food, "energy", food.r * food.r)),
        "color": list(getattr(food, "color", (220, 30, 30))),
    }


def _add_food_from_dict(engine: Engine, data: dict[str, Any]):
    food = Food(_safe_float(data.get("x", 0.0)), _safe_float(data.get("y", 0.0)), _safe_float(data.get("r", 4.5)))
    food.energy = _safe_float(data.get("energy", food.energy), food.energy)
    color = data.get("color")
    if isinstance(color, list) and len(color) >= 3:
        food.color = tuple(int(c) for c in color[:3])
    engine.entities["foods"].append(food)


def _food_layout_path(substrate_path: Path, food_layout_dir: Path, seed: int) -> Path:
    return food_layout_dir / f"{substrate_path.stem}_food_layout_seed_{seed}.json"


def _restore_foods(
    snapshot: dict[str, Any],
    engine: Engine,
    params: Params,
    substrate_path: Path,
    food_layout_dir: Path,
    seed: int,
    rebuild_food_layout: bool,
) -> dict[str, Any]:
    foods_exact = snapshot.get("foods") or snapshot.get("food_items")
    if foods_exact:
        for fd in foods_exact:
            _add_food_from_dict(engine, fd)
        return {"mode": "exact", "count": len(engine.entities["foods"])}

    count = int(snapshot.get("food", {}).get("count", 0))
    layout_path = _food_layout_path(substrate_path, food_layout_dir, seed)
    if layout_path.exists() and not rebuild_food_layout:
        layout = json.loads(layout_path.read_text(encoding="utf-8"))
        foods = layout.get("foods", [])
        if len(foods) == count:
            for fd in foods:
                _add_food_from_dict(engine, fd)
            return {
                "mode": "bench_canonical_layout",
                "count": len(engine.entities["foods"]),
                "layout_path": str(layout_path),
                "note": "Snapshot v2 has only food count; benchmark uses a persisted canonical food layout.",
            }

    for _ in range(count):
        food = create_random_food(engine.entities["foods"], params, engine.world.width, engine.world.height)
        engine.entities["foods"].append(food)
    food_layout_dir.mkdir(parents=True, exist_ok=True)
    layout = {
        "kind": "agentbiosim_benchmark_food_layout",
        "substrate": str(substrate_path),
        "snapshot_timestamp": snapshot.get("timestamp"),
        "seed": seed,
        "count": count,
        "world": snapshot.get("world", {}),
        "created_at": _dt.datetime.now().isoformat(),
        "foods": [_food_to_dict(food) for food in engine.entities["foods"]],
    }
    _write_json(layout_path, layout)
    return {
        "mode": "bench_canonical_layout_created",
        "count": count,
        "layout_path": str(layout_path),
        "note": "Snapshot v2 has only food count; benchmark created and persisted a canonical food layout.",
    }


def load_substrate_engine(
    path: Path,
    time_scale_override: float | None = None,
    food_layout_dir: Path = DEFAULT_FOOD_LAYOUT_DIR,
    seed: int = 12345,
    rebuild_food_layout: bool = False,
) -> tuple[Engine, dict[str, Any]]:
    snapshot = json.loads(path.read_text(encoding="utf-8"))
    params = Params()
    for key, value in snapshot.get("params", {}).items():
        params.set(key, value, validate=False)
    params.set("paused", False, validate=False)
    if time_scale_override is not None:
        params.set("time_scale", float(time_scale_override), validate=False)

    world_data = snapshot.get("world", {})
    world = World(
        width=_safe_float(world_data.get("width", params.get("world_w", 1000.0)), 1000.0),
        height=_safe_float(world_data.get("height", params.get("world_h", 700.0)), 700.0),
        shape=world_data.get("shape", params.get("substrate_shape", "rectangular")),
        radius=_safe_float(world_data.get("radius", params.get("substrate_radius", 400.0)), 400.0),
    )
    camera_data = snapshot.get("camera", {})
    camera = Camera(
        x=_safe_float(camera_data.get("x", 0.0)),
        y=_safe_float(camera_data.get("y", 0.0)),
        zoom=max(0.01, _safe_float(camera_data.get("zoom", 1.0), 1.0)),
    )
    engine = Engine(world, camera, params, headless=True)
    raw_labels = snapshot.get("agent_labels", {}) or {}
    engine.agent_labels = {}
    for raw_id, meta in raw_labels.items():
        try:
            label_id = int(raw_id)
        except Exception:
            continue
        color = meta.get("color", (220, 220, 220))
        engine.agent_labels[label_id] = {
            "id": label_id,
            "name": meta.get("name", f"Label {label_id}"),
            "color": tuple(int(c) for c in color[:3]),
            "show_chart": bool(meta.get("show_chart", True)),
            "min_limit": int(meta.get("min_limit", 0) or 0),
            "max_limit": int(meta.get("max_limit", 0) or 0),
        }
    engine._next_agent_label_id = max(
        int(snapshot.get("next_agent_label_id", 1) or 1),
        (max(engine.agent_labels.keys()) + 1) if engine.agent_labels else 1,
    )

    food_restore = _restore_foods(snapshot, engine, params, path, food_layout_dir, seed, rebuild_food_layout)
    for agent_data in snapshot.get("agents", []):
        agent = _build_agent(agent_data, params)
        agent.label_ids = set()
        for raw_label_id in agent_data.get("label_ids", []) or []:
            try:
                label_id = int(raw_label_id)
            except Exception:
                continue
            if label_id in engine.agent_labels:
                agent.label_ids.add(label_id)
        if agent.is_predator:
            engine.entities["predators"].append(agent)
        else:
            engine.entities["bacteria"].append(agent)
        engine.all_agents.append(agent)

    engine.total_simulation_time = _safe_float(snapshot.get("simulation", {}).get("total_simulation_time", 0.0))
    engine.start(initialize=False)
    engine._update_spatial_hash()
    meta = {
        "snapshot_version": snapshot.get("version"),
        "snapshot_timestamp": snapshot.get("timestamp"),
        "food_restore": food_restore,
        "source_path": str(path),
    }
    return engine, meta


def _agent_group_stats(agents: list[Any], params: Params, type_name: str, neural_sample: int = 0) -> dict[str, Any]:
    if not agents:
        return {"count": 0}
    death_key = "predator_death_energy" if type_name == "predator" else "bacteria_death_energy"
    split_key = "predator_split_energy" if type_name == "predator" else "bacteria_split_energy"
    death_energy = params.get(death_key, 0.0)
    split_energy = params.get(split_key, 150.0)
    energies = [float(getattr(a, "energy", 0.0)) for a in agents]
    speeds = [float(a.speed()) for a in agents]
    ages = [float(getattr(a, "age", 0.0)) for a in agents]
    radii = [float(getattr(a, "r", 0.0)) for a in agents]
    retina_counts = [int(getattr(a.sensor, "retina_count", 0)) for a in agents if getattr(a, "sensor", None)]
    vision_radii = [float(getattr(a.sensor, "vision_radius", 0.0)) for a in agents if getattr(a, "sensor", None)]
    brain_arches = [tuple(getattr(a.brain, "sizes", [])) for a in agents if getattr(a, "brain", None)]
    brain_versions = [getattr(a.brain, "version", None) for a in agents if getattr(a, "brain", None)]

    retina_values = []
    brain_outputs = []
    layer_values: dict[int, list[float]] = {}
    for agent in agents[: max(0, neural_sample)]:
        sensor_values = list(getattr(agent.sensor, "last_inputs", []) or [])
        retina_values.extend(float(v) for v in sensor_values)
        output = list(getattr(agent, "last_brain_output", []) or [])
        brain_outputs.extend(float(v) for v in output)
        if sensor_values and getattr(agent, "brain", None):
            try:
                activations = agent.brain.activations(sensor_values)
                for idx, layer in enumerate(activations):
                    layer_values.setdefault(idx, []).extend(float(v) for v in layer)
            except Exception:
                pass

    return {
        "count": len(agents),
        "energy": _stats(energies),
        "speed": _stats(speeds),
        "age": _stats(ages),
        "radius": _stats(radii),
        "ready_to_reproduce_fraction": sum(e >= split_energy for e in energies) / len(energies),
        "near_death_fraction": sum(e <= death_energy + 1e-6 for e in energies) / len(energies),
        "retina_count": _stats(retina_counts),
        "vision_radius": _stats(vision_radii),
        "brain_architectures_top": _top_counter(brain_arches),
        "brain_versions_top": _top_counter(brain_versions),
        "sampled_retina_activation": {
            **_stats(retina_values),
            "nonzero_fraction": (sum(abs(v) > 1e-9 for v in retina_values) / len(retina_values)) if retina_values else None,
        },
        "sampled_brain_output": _stats(brain_outputs),
        "sampled_activation_layers": {
            str(idx): {
                **_stats(values),
                "saturation_fraction_abs_gt_095": (sum(abs(v) > 0.95 for v in values) / len(values)) if values else None,
            }
            for idx, values in layer_values.items()
        },
    }


def _integrity(engine: Engine, spatial_sample: int = 200) -> dict[str, Any]:
    listed = list(engine.entities["bacteria"]) + list(engine.entities["predators"])
    listed_ids = {id(a) for a in listed}
    all_ids = {id(a) for a in engine.all_agents}
    duplicate_all = len(engine.all_agents) - len(all_ids)
    bad_numbers = 0
    outside = 0
    negative_energy = 0
    for obj in listed + list(engine.entities["foods"]):
        values = [getattr(obj, "x", 0.0), getattr(obj, "y", 0.0), getattr(obj, "r", 0.0)]
        if any(not math.isfinite(float(v)) for v in values):
            bad_numbers += 1
        if not engine.world.is_inside(getattr(obj, "x", 0.0), getattr(obj, "y", 0.0), getattr(obj, "r", 0.0)):
            outside += 1
        if hasattr(obj, "energy") and getattr(obj, "energy", 0.0) < -1e-9:
            negative_energy += 1

    spatial_misses = 0
    if engine.spatial_hash is not None:
        for obj in (listed + list(engine.entities["foods"]))[:spatial_sample]:
            if obj not in engine.spatial_hash.query_ball(obj.x, obj.y, max(obj.r, 1.0)):
                spatial_misses += 1

    return {
        "all_agents_missing_from_entities": len(all_ids - listed_ids),
        "entities_missing_from_all_agents": len(listed_ids - all_ids),
        "duplicate_all_agents": duplicate_all,
        "bad_numeric_objects": bad_numbers,
        "outside_world_objects": outside,
        "negative_energy_objects": negative_energy,
        "spatial_hash_sample_misses": spatial_misses,
    }


def collect_sample(engine: Engine, step: int, sim_time: float, wall_elapsed: float, neural_sample: int) -> dict[str, Any]:
    bacteria = list(engine.entities["bacteria"])
    predators = list(engine.entities["predators"])
    foods = list(engine.entities["foods"])
    spatial_stats = engine.spatial_hash.get_stats() if engine.spatial_hash is not None else None
    return {
        "step": step,
        "sim_time_s": sim_time,
        "wall_elapsed_s": wall_elapsed,
        "counts": {"bacteria": len(bacteria), "predators": len(predators), "foods": len(foods), "all_agents": len(engine.all_agents)},
        "population_pressure": {
            "bacteria_fraction_of_max": len(bacteria) / max(1, engine.params.get("bacteria_max_limit", 1)),
            "predator_fraction_of_max": len(predators) / max(1, engine.params.get("predator_max_limit", 1)),
            "food_fraction_of_target": len(foods) / max(1, engine.params.get("food_target", 1)),
        },
        "bacteria": _agent_group_stats(bacteria, engine.params, "bacteria", neural_sample),
        "predators": _agent_group_stats(predators, engine.params, "predator", neural_sample),
        "food_energy": _stats(getattr(f, "energy", 0.0) for f in foods),
        "integrity": _integrity(engine),
        "spatial_hash": spatial_stats,
        "brain_cache": get_multi_brain_cache_stats(),
        "resources": {
            "cpu_percent": engine.cpu_percent,
            "cpu_proc_percent": engine.cpu_proc_percent,
            "mem_used_mb": engine.mem_used_mb,
            "mem_percent": engine.mem_percent,
        },
    }


def _write_json(path: Path, data: Any):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def _write_report(path: Path, summary: dict[str, Any]):
    final = summary["final_sample"]
    initial = summary["initial_sample"]
    lines = [
        "RELATORIO DO BENCHMARK DO SUBSTRATO DE TESTE",
        f"Run id: {summary['run_id']}",
        f"Commit: {summary.get('git_commit')}",
        f"Substrato: {summary['substrate']['source_path']}",
        f"Duracao solicitada: {summary['config']['seconds']}s | FPS logico: {summary['config']['fps']}",
        f"Wall time: {summary['wall_s']:.3f}s | Steps: {summary['steps']} | ms/agente/step: {summary['ms_per_agent_step']:.6f}",
        "",
        "Contagens:",
        f"- Inicial: {initial['counts']}",
        f"- Final:   {final['counts']}",
        "",
        "Eventos acumulados:",
        f"- Comidas consumidas: {summary['events']['foods_eaten']}",
        f"- Predacoes: {summary['events']['predations']}",
        f"- Nascimentos: {summary['events']['births']}",
        f"- Mortes por energia: {summary['events']['deaths']}",
        "",
        "Energia final:",
        f"- Bacterias: {final['bacteria'].get('energy')}",
        f"- Predadores: {final['predators'].get('energy')}",
        "",
        "Integridade final:",
        json.dumps(final["integrity"], ensure_ascii=False),
        "",
        "Top secoes do profiler:",
    ]
    sections = summary.get("profiler_sections", {})
    for name, data in sorted(sections.items(), key=lambda kv: kv[1].get("total", 0.0), reverse=True)[:12]:
        calls = data.get("calls", 0)
        avg_ms = (data.get("total", 0.0) / calls * 1000.0) if calls else 0.0
        lines.append(f"- {name}: total={data.get('total', 0.0):.4f}s calls={calls} avg={avg_ms:.3f}ms")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def run_benchmark(
    substrate: Path,
    seconds: float,
    fps: int,
    sample_interval: float,
    label: str,
    output_dir: Path,
    neural_sample: int,
    time_scale_override: float | None,
    seed: int,
    food_layout_dir: Path,
    rebuild_food_layout: bool,
) -> dict[str, Any]:
    run_id = f"{_dt.datetime.now().strftime('%Y%m%d_%H%M%S')}_{_slug(label)}"
    run_dir = output_dir / run_id
    run_dir.mkdir(parents=True, exist_ok=True)

    _install_benchmark_deterministic_queries()
    random.seed(seed)
    np.random.seed(seed)
    engine, substrate_meta = load_substrate_engine(
        substrate,
        time_scale_override=time_scale_override,
        food_layout_dir=food_layout_dir,
        seed=seed,
        rebuild_food_layout=rebuild_food_layout,
    )
    # Loading legacy snapshots may create a canonical food layout and instantiate
    # NeuralNet objects; reset RNGs so simulation dynamics start from the same
    # random stream whether the layout was generated or read from disk.
    random.seed(seed)
    np.random.seed(seed)
    profiler.reset()
    prev_profiler_enabled = profiler.enabled
    profiler.enabled = True

    dt = 1.0 / max(1, fps)
    steps = int(math.ceil(seconds * fps))
    sample_every = max(1, int(round(sample_interval * fps)))
    samples: list[dict[str, Any]] = []
    step_times: list[float] = []
    events = {
        "foods_eaten": 0,
        "predations": 0,
        "births": 0,
        "deaths": 0,
        "foods_added_controller": 0,
        "foods_removed_controller": 0,
        "births_blocked_age": 0,
        "births_blocked_cooldown": 0,
        "collisions_resolved": 0,
        "spatial_hash_rebuilds": 0,
        "spatial_hash_skips": 0,
    }

    wall_start = time.perf_counter()
    samples.append(collect_sample(engine, 0, engine.total_simulation_time, 0.0, neural_sample))
    for step in range(1, steps + 1):
        t0 = time.perf_counter()
        engine.step(dt)
        step_times.append(time.perf_counter() - t0)
        events["foods_eaten"] += int(getattr(engine.interaction_system, "last_foods_eaten", 0))
        events["predations"] += int(getattr(engine.interaction_system, "last_agents_predated", 0))
        events["births"] += len(getattr(engine.reproduction_system, "last_births", []) or [])
        events["deaths"] += len(getattr(engine.death_system, "last_deaths", []) or [])
        events["foods_added_controller"] += int(getattr(engine.food_controller, "last_foods_added", 0))
        events["foods_removed_controller"] += int(getattr(engine.food_controller, "last_foods_removed", 0))
        events["births_blocked_age"] += int(getattr(engine.reproduction_system, "last_blocked_by_age", 0))
        events["births_blocked_cooldown"] += int(getattr(engine.reproduction_system, "last_blocked_by_cooldown", 0))
        events["collisions_resolved"] += int(getattr(engine.collision_system, "last_collisions_resolved", 0))
        if step % sample_every == 0 or step == steps:
            samples.append(collect_sample(engine, step, engine.total_simulation_time, time.perf_counter() - wall_start, neural_sample))

    wall_s = time.perf_counter() - wall_start
    events["spatial_hash_rebuilds"] = int(getattr(engine, "spatial_hash_rebuilds", 0))
    events["spatial_hash_skips"] = int(getattr(engine, "spatial_hash_skips", 0))
    profiler.total_wall = wall_s
    profiler_sections = profiler.snapshot()
    profiler.enabled = prev_profiler_enabled

    agent_steps = sum(s["counts"]["all_agents"] for s in samples) / max(1, len(samples)) * max(1, steps)
    summary = {
        "run_id": run_id,
        "label": label,
        "git_commit": _git_commit(),
        "created_at": _dt.datetime.now().isoformat(),
        "python": sys.version,
        "platform": platform.platform(),
        "config": {
            "seconds": seconds,
            "fps": fps,
            "sample_interval": sample_interval,
            "neural_sample": neural_sample,
            "time_scale": engine.params.get("time_scale"),
            "params_random_seed": engine.params.get("random_seed"),
            "retina_vision_mode": engine.params.get("retina_vision_mode"),
            "seed": seed,
            "food_layout_dir": str(food_layout_dir),
            "rebuild_food_layout": rebuild_food_layout,
            "deterministic_spatial_queries": True,
        },
        "substrate": substrate_meta,
        "steps": steps,
        "wall_s": wall_s,
        "step_wall_s": _stats(step_times),
        "avg_agents": agent_steps / max(1, steps),
        "ms_per_agent_step": (wall_s * 1000.0 / agent_steps) if agent_steps else None,
        "events": events,
        "initial_sample": samples[0],
        "final_sample": samples[-1],
        "profiler_sections": profiler_sections,
        "profiler_report": profiler.report(),
        "outputs": {
            "run_dir": str(run_dir),
            "summary_json": str(run_dir / "summary.json"),
            "timeseries_jsonl": str(run_dir / "timeseries.jsonl"),
            "report_txt": str(run_dir / "report.txt"),
        },
    }

    _write_json(run_dir / "summary.json", summary)
    with (run_dir / "timeseries.jsonl").open("w", encoding="utf-8") as f:
        for sample in samples:
            f.write(json.dumps(sample, ensure_ascii=False) + "\n")
    _write_report(run_dir / "report.txt", summary)

    factor = {
        "run_id": run_id,
        "label": label,
        "git_commit": summary["git_commit"],
        "created_at": summary["created_at"],
        "seconds": seconds,
        "fps": fps,
        "time_scale": engine.params.get("time_scale"),
        "params_random_seed": engine.params.get("random_seed"),
        "retina_vision_mode": engine.params.get("retina_vision_mode"),
        "seed": seed,
        "food_restore": summary["substrate"].get("food_restore"),
        "wall_s": wall_s,
        "steps": steps,
        "avg_agents": summary["avg_agents"],
        "ms_per_agent_step": summary["ms_per_agent_step"],
        "initial_counts": samples[0]["counts"],
        "final_counts": samples[-1]["counts"],
        "events": events,
        "final_integrity": samples[-1]["integrity"],
        "final_bacteria_energy_mean": samples[-1]["bacteria"].get("energy", {}).get("mean"),
        "final_predator_energy_mean": samples[-1]["predators"].get("energy", {}).get("mean"),
        "summary_json": str(run_dir / "summary.json"),
    }
    FACTORS_LOG.parent.mkdir(parents=True, exist_ok=True)
    with FACTORS_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(factor, ensure_ascii=False) + "\n")
    _write_json(run_dir / "fator_comparativo.json", factor)
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Roda benchmark headless do substrato de teste com metricas ricas.")
    parser.add_argument("--substrate", type=Path, default=DEFAULT_SUBSTRATE)
    parser.add_argument("--seconds", type=float, default=10.0)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--sample-interval", type=float, default=1.0)
    parser.add_argument("--label", default="substrato_teste")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--neural-sample", type=int, default=32, help="Quantidade de agentes por tipo usados para metricas neurais.")
    parser.add_argument("--time-scale", type=float, default=None, help="Override opcional do time_scale do snapshot.")
    parser.add_argument("--seed", type=int, default=12345, help="Seed fixa para restauracao legada de comida e eventos aleatorios.")
    parser.add_argument("--food-layout-dir", type=Path, default=DEFAULT_FOOD_LAYOUT_DIR, help="Pasta para layout canonico de comidas do benchmark.")
    parser.add_argument("--rebuild-food-layout", action="store_true", help="Recria o layout canonico de comidas mesmo se ja existir.")
    args = parser.parse_args(argv)

    summary = run_benchmark(
        substrate=args.substrate,
        seconds=args.seconds,
        fps=args.fps,
        sample_interval=args.sample_interval,
        label=args.label,
        output_dir=args.output_dir,
        neural_sample=args.neural_sample,
        time_scale_override=args.time_scale,
        seed=args.seed,
        food_layout_dir=args.food_layout_dir,
        rebuild_food_layout=args.rebuild_food_layout,
    )
    print("Benchmark concluido.")
    print(f"Run id: {summary['run_id']}")
    print(f"Resumo: {summary['outputs']['summary_json']}")
    print(f"Relatorio: {summary['outputs']['report_txt']}")
    print(f"ms/agente/step: {summary['ms_per_agent_step']:.6f}")
    print(f"Eventos: {summary['events']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
