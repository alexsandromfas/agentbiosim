"""Pequeno utilitário de profiling por seção para a simulação.

Uso:
    from .profiler import profiler, profile_section
    with profile_section('nome'):
        ... codigo ...

Ativar/desativar:
    profiler.enabled = True / False

Relatório:
    print(profiler.report())
"""
from __future__ import annotations
import time
from contextlib import contextmanager
from dataclasses import dataclass, field
from typing import Dict, List, Tuple


@dataclass
class SectionStats:
    total_time: float = 0.0
    calls: int = 0
    max_time: float = 0.0
    min_time: float = float('inf')

    def add(self, dt: float):
        self.total_time += dt
        self.calls += 1
        if dt > self.max_time:
            self.max_time = dt
        if dt < self.min_time:
            self.min_time = dt

    def as_tuple(self) -> Tuple[float, int, float, float]:
        return self.total_time, self.calls, self.min_time if self.min_time != float('inf') else 0.0, self.max_time


class Profiler:
    def __init__(self):
        self.enabled: bool = True
        self.sections: Dict[str, SectionStats] = {}
        self._stack: List[Tuple[str, float]] = []  # (name, start_time)
        self.total_wall: float = 0.0  # acumulado (externo pode somar)

    @contextmanager
    def section(self, name: str):
        if not self.enabled:
            yield
            return
        start = time.perf_counter()
        self._stack.append((name, start))
        try:
            yield
        finally:
            end = time.perf_counter()
            _, s = self._stack.pop()
            dt = end - s
            stats = self.sections.get(name)
            if stats is None:
                stats = SectionStats()
                self.sections[name] = stats
            stats.add(dt)

    def merge_child(self, other: 'Profiler'):
        for k, v in other.sections.items():
            target = self.sections.get(k)
            if target is None:
                self.sections[k] = v
            else:
                target.total_time += v.total_time
                target.calls += v.calls
                target.max_time = max(target.max_time, v.max_time)
                target.min_time = min(target.min_time, v.min_time)

    def reset(self):
        self.sections.clear()
        self.total_wall = 0.0

    def snapshot(self) -> Dict[str, dict]:
        return {k: { 'total': st.total_time, 'calls': st.calls, 'min': st.min_time if st.min_time != float('inf') else 0.0, 'max': st.max_time } for k, st in self.sections.items()}

    def report(self) -> str:
        if not self.sections:
            return "[Profiler] Sem dados."
        total = sum(st.total_time for st in self.sections.values())
        # Na ausência de hierarquia explícita, exclusive ~ total (mantemos campo para futura extensão)
        lines = []
        lines.append("=== PROFILING SECTIONS ===")
        lines.append(f"Total tempo seccoes (somadas): {total:.6f}s | Wall: {self.total_wall:.6f}s | OverlapRatio: {(total/self.total_wall if self.total_wall>0 else 0):.2f}")
        lines.append(f"{'Seção':28s} {'Total(s)':>9s} {'Calls':>7s} {'Avg(ms)':>9s} {'%Wall':>7s} {'Min(ms)':>9s} {'Max(ms)':>9s}")
        for name, st in sorted(self.sections.items(), key=lambda kv: kv[1].total_time, reverse=True):
            avg = (st.total_time / st.calls * 1000.0) if st.calls else 0.0
            pct_wall = (st.total_time / self.total_wall * 100.0) if self.total_wall > 0 else 0.0
            lines.append(f"{name:28s} {st.total_time:9.4f} {st.calls:7d} {avg:9.3f} {pct_wall:7.2f} {st.min_time*1000.0:9.3f} {st.max_time*1000.0:9.3f}")
        return "\n".join(lines)


profiler = Profiler()


def set_enabled(flag: bool):
    profiler.enabled = flag


def profile_section(name: str):
    return profiler.section(name)
