"""Drawable solid obstacles for the simulation.

Obstacles are stored as circular brush stamps indexed in a small spatial grid.
When no stamps exist, every query returns immediately so normal simulations do
not pay the obstacle cost.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Iterable


@dataclass(slots=True)
class ObstacleStamp:
    x: float
    y: float
    r: float
    color: tuple[int, int, int]
    type_code: int = 3


class ObstacleMap:
    def __init__(self, cell_size: float = 32.0):
        self.cell_size = max(4.0, float(cell_size))
        self.stamps: list[ObstacleStamp] = []
        self._grid: dict[tuple[int, int], set[int]] = {}

    def __len__(self) -> int:
        return len(self.stamps)

    @property
    def has_obstacles(self) -> bool:
        return bool(self.stamps)

    def clear(self):
        self.stamps.clear()
        self._grid.clear()

    def configure_cell_size(self, max_radius: float):
        desired = max(8.0, float(max_radius) * 2.0)
        if abs(desired - self.cell_size) < 1e-6:
            return
        self.cell_size = desired
        self._rebuild_grid()

    def to_dicts(self) -> list[dict]:
        return [
            {"x": s.x, "y": s.y, "r": s.r, "color": list(s.color)}
            for s in self.stamps
        ]

    def load_dicts(self, rows: Iterable[dict]):
        self.clear()
        for row in rows or []:
            try:
                color = row.get("color", (95, 95, 105))
                if isinstance(color, list):
                    color = tuple(color[:3])
                self._add_stamp_raw(
                    float(row.get("x", 0.0)),
                    float(row.get("y", 0.0)),
                    max(0.5, float(row.get("r", 4.0))),
                    tuple(int(c) for c in color),
                )
            except Exception:
                continue
        self._rebuild_grid()

    def add_brush_line(self, x0: float, y0: float, x1: float, y1: float,
                       radius: float, color: tuple[int, int, int], world=None) -> int:
        radius = max(0.5, float(radius))
        dist = math.hypot(x1 - x0, y1 - y0)
        spacing = max(1.0, radius * 0.45)
        steps = max(1, int(math.ceil(dist / spacing)))
        added = 0
        for i in range(steps + 1):
            t = i / steps
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            x, y = self._brush_point_inside_world(x, y, world)
            if x is None:
                continue
            # Avoid stamp explosions when the mouse barely moved.
            if self._has_stamp_center_near(x, y, max(0.5, spacing * 0.5)):
                continue
            self._add_stamp_raw(x, y, radius, color)
            added += 1
        if added:
            self._index_new_stamps(len(self.stamps) - added)
        return added

    def erase_brush_line(self, x0: float, y0: float, x1: float, y1: float,
                         radius: float, world=None) -> int:
        if not self.stamps:
            return 0
        radius = max(0.5, float(radius))
        dist = math.hypot(x1 - x0, y1 - y0)
        spacing = max(1.0, radius * 0.45)
        steps = max(1, int(math.ceil(dist / spacing)))
        remove_ids: set[int] = set()
        for i in range(steps + 1):
            t = i / steps
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            x, y = self._brush_point_inside_world(x, y, world)
            if x is None:
                continue
            for idx in self.query_indices(x, y, radius):
                stamp = self.stamps[idx]
                total = radius + stamp.r
                dx = x - stamp.x
                dy = y - stamp.y
                if dx * dx + dy * dy <= total * total:
                    remove_ids.add(idx)
        if not remove_ids:
            return 0
        self.stamps = [s for idx, s in enumerate(self.stamps) if idx not in remove_ids]
        self._rebuild_grid()
        return len(remove_ids)

    def circle_overlaps(self, x: float, y: float, radius: float) -> bool:
        if not self.stamps:
            return False
        r = max(0.0, float(radius))
        for idx in self.query_indices(x, y, r):
            stamp = self.stamps[idx]
            total = r + stamp.r
            dx = x - stamp.x
            dy = y - stamp.y
            if dx * dx + dy * dy <= total * total:
                return True
        return False

    def _has_stamp_center_near(self, x: float, y: float, distance: float) -> bool:
        if not self.stamps:
            return False
        dist2_limit = distance * distance
        for idx in self.query_indices(x, y, distance):
            stamp = self.stamps[idx]
            dx = x - stamp.x
            dy = y - stamp.y
            if dx * dx + dy * dy <= dist2_limit:
                return True
        return False

    @staticmethod
    def _brush_point_inside_world(x: float, y: float, world):
        if world is None:
            return float(x), float(y)
        if world.is_inside(x, y, 0.0):
            return float(x), float(y)
        try:
            cx, cy = world.clamp_position(float(x), float(y), 0.0)
        except Exception:
            return None, None
        if world.is_inside(cx, cy, 0.0):
            return float(cx), float(cy)
        return None, None

    def query_indices(self, x: float, y: float, radius: float) -> set[int]:
        if not self.stamps:
            return set()
        min_cx, min_cy = self._cell_coords(x - radius, y - radius)
        max_cx, max_cy = self._cell_coords(x + radius, y + radius)
        found: set[int] = set()
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                found.update(self._grid.get((cx, cy), ()))
        return found

    def remove_food_overlaps(self, foods: list) -> int:
        if not self.stamps or not foods:
            return 0
        before = len(foods)
        foods[:] = [
            food for food in foods
            if not self.circle_overlaps(float(food.x), float(food.y), float(food.r))
        ]
        return before - len(foods)

    def resolve_agent(self, agent) -> int:
        if not self.stamps:
            return 0
        collisions = 0
        for idx in self.query_indices(agent.x, agent.y, agent.r):
            stamp = self.stamps[idx]
            dx = agent.x - stamp.x
            dy = agent.y - stamp.y
            min_dist = agent.r + stamp.r
            dist2 = dx * dx + dy * dy
            if dist2 >= min_dist * min_dist:
                continue
            if dist2 <= 1e-12:
                nx, ny = 1.0, 0.0
                dist = 0.0
            else:
                dist = math.sqrt(dist2)
                nx, ny = dx / dist, dy / dist
            overlap = min_dist - dist + 1e-6
            agent.x += nx * overlap
            agent.y += ny * overlap
            vdot = agent.vx * nx + agent.vy * ny
            if vdot < 0.0:
                agent.vx -= 1.5 * vdot * nx
                agent.vy -= 1.5 * vdot * ny
            collisions += 1
        return collisions

    def _add_stamp_raw(self, x: float, y: float, radius: float, color: tuple[int, int, int]):
        self.stamps.append(ObstacleStamp(float(x), float(y), float(radius), color))

    def _index_new_stamps(self, start_idx: int):
        for idx in range(max(0, start_idx), len(self.stamps)):
            self._index_stamp(idx, self.stamps[idx])

    def _index_stamp(self, idx: int, stamp: ObstacleStamp):
        min_cx, min_cy = self._cell_coords(stamp.x - stamp.r, stamp.y - stamp.r)
        max_cx, max_cy = self._cell_coords(stamp.x + stamp.r, stamp.y + stamp.r)
        for cx in range(min_cx, max_cx + 1):
            for cy in range(min_cy, max_cy + 1):
                self._grid.setdefault((cx, cy), set()).add(idx)

    def _rebuild_grid(self):
        self._grid.clear()
        for idx, stamp in enumerate(self.stamps):
            self._index_stamp(idx, stamp)

    def _cell_coords(self, x: float, y: float) -> tuple[int, int]:
        return int(math.floor(x / self.cell_size)), int(math.floor(y / self.cell_size))
