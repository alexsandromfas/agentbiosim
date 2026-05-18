"""Metricas leves de eficiencia alimentar dos agentes.

O objetivo aqui nao e medir inteligencia geral. O fator compara quanto o agente
consegue converter oportunidade alimentar em energia obtida, ajustando pela
densidade atual do recurso disponivel.
"""
from __future__ import annotations

import math
from typing import Any, Dict, Iterable, List


MIN_AGE_SECONDS = 1.0
EPS = 1e-9


def substrate_area(world: Any) -> float:
    if getattr(world, "shape", "rectangular") == "circular":
        radius = max(0.0, float(getattr(world, "radius", 0.0)))
        return max(EPS, math.pi * radius * radius)
    return max(
        EPS,
        float(getattr(world, "width", 0.0)) * float(getattr(world, "height", 0.0)),
    )


def resource_type_code(agent: Any) -> int:
    # Para bacterias, o recurso e comida. Para predadores, o recurso e presa.
    return 1 if getattr(agent, "is_predator", False) else 0


def resource_pool(engine: Any, agent: Any) -> Iterable[Any]:
    if getattr(agent, "is_predator", False):
        return engine.entities.get("bacteria", [])
    return engine.entities.get("foods", [])


def resource_energy(obj: Any, predator_resource: bool = False) -> float:
    if predator_resource:
        return max(0.0, float(getattr(obj, "energy", 0.0))) * 0.7
    return max(0.0, float(getattr(obj, "energy", getattr(obj, "r", 0.0) ** 2)))


def agent_intake_energy(agent: Any) -> float:
    if getattr(agent, "is_predator", False):
        return max(0.0, float(getattr(agent, "prey_energy_eaten_total", 0.0)))
    return max(0.0, float(getattr(agent, "food_energy_eaten_total", 0.0)))


def agent_intake_events(agent: Any) -> int:
    if getattr(agent, "is_predator", False):
        return int(getattr(agent, "prey_eaten_count", 0))
    return int(getattr(agent, "food_eaten_count", 0))


def intake_rate(agent: Any) -> float:
    age = max(MIN_AGE_SECONDS, float(getattr(agent, "age", 0.0)))
    return agent_intake_energy(agent) / age


def global_resource_density(engine: Any, agent: Any) -> float:
    predator_resource = bool(getattr(agent, "is_predator", False))
    total = sum(resource_energy(obj, predator_resource) for obj in resource_pool(engine, agent))
    return total / substrate_area(engine.world)


def local_resource_density(engine: Any, agent: Any, radius: float | None = None) -> float:
    sensor = getattr(agent, "sensor", None)
    if radius is None:
        radius = float(getattr(sensor, "vision_radius", 120.0) or 120.0)
    radius = max(1.0, float(radius))
    radius2 = radius * radius
    target_code = resource_type_code(agent)
    predator_resource = bool(getattr(agent, "is_predator", False))

    spatial_hash = getattr(engine, "spatial_hash", None)
    if spatial_hash is not None:
        candidates = spatial_hash.query_ball(float(agent.x), float(agent.y), radius)
    else:
        candidates = resource_pool(engine, agent)

    total = 0.0
    ax = float(getattr(agent, "x", 0.0))
    ay = float(getattr(agent, "y", 0.0))
    for obj in candidates:
        if obj is agent:
            continue
        if getattr(obj, "type_code", -1) != target_code:
            continue
        dx = float(getattr(obj, "x", 0.0)) - ax
        dy = float(getattr(obj, "y", 0.0)) - ay
        if dx * dx + dy * dy <= radius2:
            total += resource_energy(obj, predator_resource)
    return total / max(EPS, math.pi * radius2)


def local_intelligence_for_agent(engine: Any, agent: Any) -> Dict[str, float]:
    rate = intake_rate(agent)
    local_density = local_resource_density(engine, agent)
    local_factor = rate / local_density if local_density > EPS else 0.0
    age = max(0.0, float(getattr(agent, "age", 0.0)))
    confidence = min(1.0, age / 30.0)
    return {
        "local_factor": local_factor,
        "local_density": local_density,
        "intake_rate": rate,
        "intake_energy": agent_intake_energy(agent),
        "intake_events": float(agent_intake_events(agent)),
        "age": age,
        "confidence": confidence,
    }


def intelligence_for_agent(engine: Any, agent: Any) -> Dict[str, float]:
    metrics = local_intelligence_for_agent(engine, agent)
    global_density = global_resource_density(engine, agent)
    global_factor = metrics["intake_rate"] / global_density if global_density > EPS else 0.0
    metrics["global_factor"] = global_factor
    metrics["global_density"] = global_density
    return metrics


def species_sample(engine: Any, selected_agent: Any, sample_size: int = 10) -> List[Any]:
    pool = (
        engine.entities.get("predators", [])
        if getattr(selected_agent, "is_predator", False)
        else engine.entities.get("bacteria", [])
    )
    if not pool:
        return []
    if len(pool) <= sample_size:
        return list(pool)
    try:
        start = pool.index(selected_agent)
    except ValueError:
        start = 0
    sample = [selected_agent] if selected_agent in pool else []
    stride = max(1, len(pool) // sample_size)
    cursor = start
    while len(sample) < sample_size:
        cursor = (cursor + stride) % len(pool)
        candidate = pool[cursor]
        if candidate not in sample:
            sample.append(candidate)
        elif len(sample) >= len(pool):
            break
    return sample


def intelligence_snapshot(engine: Any, selected_agent: Any, sample_size: int = 10) -> Dict[str, float]:
    if selected_agent is None:
        return {
            "local_factor": 0.0,
            "global_factor": 0.0,
            "species_local_factor": 0.0,
            "local_density": 0.0,
            "global_density": 0.0,
            "intake_rate": 0.0,
            "intake_energy": 0.0,
            "intake_events": 0.0,
            "sample_size": 0.0,
            "confidence": 0.0,
        }
    selected = intelligence_for_agent(engine, selected_agent)
    sample = species_sample(engine, selected_agent, sample_size=sample_size)
    local_values = [
        intelligence_for_agent(engine, agent)["local_factor"]
        for agent in sample
    ]
    species_local = sum(local_values) / len(local_values) if local_values else 0.0
    selected["species_local_factor"] = species_local
    selected["sample_size"] = float(len(sample))
    return selected


def group_intelligence_snapshot(
    engine: Any,
    agents: Iterable[Any],
    sample_size: int = 10,
    include_global: bool = False,
) -> Dict[str, float]:
    pool = [agent for agent in agents if agent in getattr(engine, "all_agents", [])]
    if not pool:
        return {"local_factor": 0.0, "global_factor": 0.0, "sample_size": 0.0}
    if len(pool) > sample_size:
        stride = max(1, len(pool) // sample_size)
        sample = pool[::stride][:sample_size]
    else:
        sample = pool
    if include_global:
        values = [intelligence_for_agent(engine, agent) for agent in sample]
    else:
        values = [local_intelligence_for_agent(engine, agent) for agent in sample]
    result = {
        "local_factor": sum(v["local_factor"] for v in values) / len(values),
        "intake_rate": sum(v["intake_rate"] for v in values) / len(values),
        "sample_size": float(len(values)),
        "member_count": float(len(pool)),
    }
    if include_global:
        result["global_factor"] = sum(v["global_factor"] for v in values) / len(values)
    else:
        result["global_factor"] = 0.0
    return result
