"""Optional array/JIT kernels for hot simulation paths.

The simulation still keeps the object-oriented engine as the source of truth.
These helpers operate on temporary arrays extracted from agents and then copied
back by the caller. Numba is used when available; otherwise selected kernels can
fall back to NumPy or to the old Python object path.
"""
from __future__ import annotations

import math
from typing import Optional

import numpy as np

try:  # pragma: no cover - environment dependent
    from numba import njit, prange  # type: ignore
    NUMBA_AVAILABLE = True
except Exception:  # pragma: no cover - exercised when numba is absent
    njit = None  # type: ignore
    prange = range  # type: ignore
    NUMBA_AVAILABLE = False


_NUMBA_RUNTIME_FAILED = False


def has_numba() -> bool:
    return bool(NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED)


def _sigmoid_numpy(values: np.ndarray) -> np.ndarray:
    result = np.empty_like(values, dtype=np.float64)
    positive = values >= 0.0
    result[positive] = 1.0 / (1.0 + np.exp(-values[positive]))
    exp_x = np.exp(values[~positive])
    result[~positive] = exp_x / (1.0 + exp_x)
    return result


def _locomotion_energy_kernel_numpy(
    x: np.ndarray,
    y: np.ndarray,
    radius: np.ndarray,
    angle: np.ndarray,
    vx: np.ndarray,
    vy: np.ndarray,
    energy: np.ndarray,
    outputs: np.ndarray,
    max_speed: np.ndarray,
    max_turn: np.ndarray,
    dt: float,
    world_shape_code: int,
    world_width: float,
    world_height: float,
    world_cx: float,
    world_cy: float,
    world_radius: float,
    allow_reverse: bool,
    inertia: float,
    v0_cost: float,
    vmax_cost: float,
    vmax_ref: float,
    energy_cap: float,
):
    if outputs.shape[1] < 2:
        return
    speed_raw = outputs[:, 0]
    steer_raw = outputs[:, 1]
    if allow_reverse:
        speed_cmd = np.tanh(speed_raw)
    else:
        speed_cmd = _sigmoid_numpy(speed_raw)
    steer_cmd = np.tanh(steer_raw)

    angle[:] = (angle + steer_cmd * max_turn * dt + math.pi) % (2.0 * math.pi) - math.pi
    desired_speed = speed_cmd * max_speed
    desired_vx = np.cos(angle) * desired_speed
    desired_vy = np.sin(angle) * desired_speed

    if inertia <= 1.0:
        vx[:] = desired_vx
        vy[:] = desired_vy
    else:
        alpha = min(1.0, 1.0 / max(0.0, inertia))
        vx[:] = vx + (desired_vx - vx) * alpha
        vy[:] = vy + (desired_vy - vy) * alpha

    x[:] = x + vx * dt
    y[:] = y + vy * dt

    if world_shape_code == 1:
        dx = x - world_cx
        dy = y - world_cy
        dist = np.sqrt(dx * dx + dy * dy)
        max_dist = np.maximum(1e-6, world_radius - radius)
        outside = dist > max_dist
        valid = outside & (dist > 1e-12)
        if np.any(valid):
            nx = dx[valid] / dist[valid]
            ny = dy[valid] / dist[valid]
            x[valid] = world_cx + nx * max_dist[valid]
            y[valid] = world_cy + ny * max_dist[valid]
            vrad = vx[valid] * nx + vy[valid] * ny
            vx[valid] = vx[valid] - 1.5 * vrad * nx
            vy[valid] = vy[valid] - 1.5 * vrad * ny
    else:
        left = x - radius < 0.0
        if np.any(left):
            x[left] = radius[left]
            vx[left] = vx[left] * -0.5
        right = (~left) & (x + radius > world_width)
        if np.any(right):
            x[right] = world_width - radius[right]
            vx[right] = vx[right] * -0.5
        top = y - radius < 0.0
        if np.any(top):
            y[top] = radius[top]
            vy[top] = vy[top] * -0.5
        bottom = (~top) & (y + radius > world_height)
        if np.any(bottom):
            y[bottom] = world_height - radius[bottom]
            vy[bottom] = vy[bottom] * -0.5

    speed = np.sqrt(vx * vx + vy * vy)
    vmax_ref = max(1e-6, float(vmax_ref))
    speed_norm = np.clip(speed, 0.0, vmax_ref) / vmax_ref
    cost_sec = v0_cost + speed_norm * (vmax_cost - v0_cost)
    energy[:] = np.maximum(0.0, energy - cost_sec * dt)
    energy[:] = np.minimum(energy, energy_cap)


def _locomotion_energy_kernel_python(
    x: np.ndarray,
    y: np.ndarray,
    radius: np.ndarray,
    angle: np.ndarray,
    vx: np.ndarray,
    vy: np.ndarray,
    energy: np.ndarray,
    outputs: np.ndarray,
    max_speed: np.ndarray,
    max_turn: np.ndarray,
    dt: float,
    world_shape_code: int,
    world_width: float,
    world_height: float,
    world_cx: float,
    world_cy: float,
    world_radius: float,
    allow_reverse: bool,
    inertia: float,
    v0_cost: float,
    vmax_cost: float,
    vmax_ref: float,
    energy_cap: float,
):
    for i in range(x.shape[0]):
        speed_raw = float(outputs[i, 0])
        steer_raw = float(outputs[i, 1])
        if allow_reverse:
            speed_cmd = math.tanh(speed_raw)
        else:
            try:
                speed_cmd = 1.0 / (1.0 + math.exp(-speed_raw))
            except OverflowError:
                speed_cmd = 0.0 if speed_raw < 0 else 1.0
        steer_cmd = math.tanh(steer_raw)
        desired_speed = speed_cmd * float(max_speed[i])
        a = float(angle[i]) + steer_cmd * float(max_turn[i]) * dt
        a = (a + math.pi) % (2.0 * math.pi) - math.pi
        angle[i] = a
        desired_vx = math.cos(a) * desired_speed
        desired_vy = math.sin(a) * desired_speed
        if inertia <= 1.0:
            vx[i] = desired_vx
            vy[i] = desired_vy
        else:
            alpha = min(1.0, 1.0 / max(0.0, inertia))
            vx[i] = float(vx[i]) + (desired_vx - float(vx[i])) * alpha
            vy[i] = float(vy[i]) + (desired_vy - float(vy[i])) * alpha
        x[i] = float(x[i]) + float(vx[i]) * dt
        y[i] = float(y[i]) + float(vy[i]) * dt

        if world_shape_code == 1:
            dx = float(x[i]) - world_cx
            dy = float(y[i]) - world_cy
            dist = math.hypot(dx, dy)
            max_dist = max(1e-6, world_radius - float(radius[i]))
            if dist > max_dist:
                nx = dx / dist
                ny = dy / dist
                x[i] = world_cx + nx * max_dist
                y[i] = world_cy + ny * max_dist
                vrad = float(vx[i]) * nx + float(vy[i]) * ny
                vx[i] = float(vx[i]) - 1.5 * vrad * nx
                vy[i] = float(vy[i]) - 1.5 * vrad * ny
        else:
            if float(x[i]) - float(radius[i]) < 0.0:
                x[i] = radius[i]
                vx[i] = float(vx[i]) * -0.5
            elif float(x[i]) + float(radius[i]) > world_width:
                x[i] = world_width - float(radius[i])
                vx[i] = float(vx[i]) * -0.5
            if float(y[i]) - float(radius[i]) < 0.0:
                y[i] = radius[i]
                vy[i] = float(vy[i]) * -0.5
            elif float(y[i]) + float(radius[i]) > world_height:
                y[i] = world_height - float(radius[i])
                vy[i] = float(vy[i]) * -0.5

        speed = math.hypot(float(vx[i]), float(vy[i]))
        vmax_ref_safe = max(1e-6, vmax_ref)
        s = max(0.0, min(speed, vmax_ref_safe)) / vmax_ref_safe
        cost_sec = v0_cost + s * (vmax_cost - v0_cost)
        energy[i] = max(0.0, float(energy[i]) - cost_sec * dt)
        if float(energy[i]) > energy_cap:
            energy[i] = energy_cap


if NUMBA_AVAILABLE:  # pragma: no cover - requires optional dependency

    @njit(cache=True, fastmath=True, parallel=True)
    def _locomotion_energy_kernel_numba(
        x,
        y,
        radius,
        angle,
        vx,
        vy,
        energy,
        outputs,
        max_speed,
        max_turn,
        dt,
        world_shape_code,
        world_width,
        world_height,
        world_cx,
        world_cy,
        world_radius,
        allow_reverse,
        inertia,
        v0_cost,
        vmax_cost,
        vmax_ref,
        energy_cap,
    ):
        for i in prange(x.shape[0]):
            speed_raw = outputs[i, 0]
            steer_raw = outputs[i, 1]
            if allow_reverse:
                speed_cmd = math.tanh(speed_raw)
            else:
                if speed_raw >= 0.0:
                    speed_cmd = 1.0 / (1.0 + math.exp(-speed_raw))
                else:
                    exp_x = math.exp(speed_raw)
                    speed_cmd = exp_x / (1.0 + exp_x)
            steer_cmd = math.tanh(steer_raw)
            desired_speed = speed_cmd * max_speed[i]
            a = angle[i] + steer_cmd * max_turn[i] * dt
            a = (a + math.pi) % (2.0 * math.pi) - math.pi
            angle[i] = a
            desired_vx = math.cos(a) * desired_speed
            desired_vy = math.sin(a) * desired_speed
            if inertia <= 1.0:
                vx[i] = desired_vx
                vy[i] = desired_vy
            else:
                alpha = min(1.0, 1.0 / max(0.0, inertia))
                vx[i] = vx[i] + (desired_vx - vx[i]) * alpha
                vy[i] = vy[i] + (desired_vy - vy[i]) * alpha
            x[i] = x[i] + vx[i] * dt
            y[i] = y[i] + vy[i] * dt

            if world_shape_code == 1:
                dx = x[i] - world_cx
                dy = y[i] - world_cy
                dist = math.sqrt(dx * dx + dy * dy)
                max_dist = max(1e-6, world_radius - radius[i])
                if dist > max_dist:
                    nx = dx / dist
                    ny = dy / dist
                    x[i] = world_cx + nx * max_dist
                    y[i] = world_cy + ny * max_dist
                    vrad = vx[i] * nx + vy[i] * ny
                    vx[i] = vx[i] - 1.5 * vrad * nx
                    vy[i] = vy[i] - 1.5 * vrad * ny
            else:
                if x[i] - radius[i] < 0.0:
                    x[i] = radius[i]
                    vx[i] = vx[i] * -0.5
                elif x[i] + radius[i] > world_width:
                    x[i] = world_width - radius[i]
                    vx[i] = vx[i] * -0.5
                if y[i] - radius[i] < 0.0:
                    y[i] = radius[i]
                    vy[i] = vy[i] * -0.5
                elif y[i] + radius[i] > world_height:
                    y[i] = world_height - radius[i]
                    vy[i] = vy[i] * -0.5

            speed = math.sqrt(vx[i] * vx[i] + vy[i] * vy[i])
            vmax_ref_safe = max(1e-6, vmax_ref)
            s = max(0.0, min(speed, vmax_ref_safe)) / vmax_ref_safe
            cost_sec = v0_cost + s * (vmax_cost - v0_cost)
            energy[i] = max(0.0, energy[i] - cost_sec * dt)
            if energy[i] > energy_cap:
                energy[i] = energy_cap

    @njit(cache=True, fastmath=True)
    def _retina_single_kernel_numba(
        cand_x,
        cand_y,
        cand_r,
        cand_type,
        self_flags,
        eye_x,
        eye_y,
        agent_angle,
        vision_radius,
        half_fov,
        retina_count,
        see_food,
        see_bacteria,
        see_predators,
        spatial_filtered,
        out,
    ):
        for r_idx in range(retina_count):
            out[r_idx] = 0.0
        if retina_count <= 0 or half_fov <= 0.0 or vision_radius <= 0.0:
            return
        ray_best = np.empty(retina_count, dtype=np.float64)
        for r_idx in range(retina_count):
            ray_best[r_idx] = np.inf
        for i in range(cand_x.shape[0]):
            if self_flags[i]:
                continue
            if not spatial_filtered:
                tc = cand_type[i]
                if not ((tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators)):
                    continue
            dx = cand_x[i] - eye_x
            dy = cand_y[i] - eye_y
            dist = math.sqrt(dx * dx + dy * dy)
            cr = cand_r[i]
            if dist - cr > vision_radius:
                continue
            obj_angle = math.atan2(dy, dx)
            ang = (obj_angle - agent_angle + math.pi) % (2.0 * math.pi) - math.pi
            if dist <= cr:
                half_span = math.pi
            else:
                ratio = cr / dist
                if ratio > 1.0:
                    ratio = 1.0
                elif ratio < 0.0:
                    ratio = 0.0
                half_span = math.asin(ratio)
            if abs(ang) > half_fov + half_span:
                continue
            eff_dist = dist - cr
            if eff_dist < 0.0:
                eff_dist = 0.0
            elif eff_dist > vision_radius:
                eff_dist = vision_radius
            if retina_count > 1:
                rel = (ang + half_fov) / (2.0 * half_fov) * (retina_count - 1)
                ray_idx = int(math.floor(rel + 0.5))
                if ray_idx < 0:
                    ray_idx = 0
                elif ray_idx >= retina_count:
                    ray_idx = retina_count - 1
            else:
                ray_idx = 0
            if eff_dist < ray_best[ray_idx]:
                ray_best[ray_idx] = eff_dist
        for r_idx in range(retina_count):
            best = ray_best[r_idx]
            if math.isfinite(best):
                act = (vision_radius - best) / vision_radius
                if act < 0.0:
                    act = 0.0
                elif act > 1.0:
                    act = 1.0
                out[r_idx] = act

    @njit(cache=True, fastmath=True)
    def _retina_fullbody_kernel_numba(
        cand_x,
        cand_y,
        cand_r,
        cand_type,
        self_flags,
        eye_x,
        eye_y,
        agent_angle,
        vision_radius,
        half_fov,
        retina_count,
        see_food,
        see_bacteria,
        see_predators,
        spatial_filtered,
        out,
    ):
        for r_idx in range(retina_count):
            out[r_idx] = 0.0
        if retina_count <= 0 or half_fov <= 0.0 or vision_radius <= 0.0:
            return
        ray_best = np.empty(retina_count, dtype=np.float64)
        for r_idx in range(retina_count):
            ray_best[r_idx] = np.inf

        for i in range(cand_x.shape[0]):
            if self_flags[i]:
                continue
            if not spatial_filtered:
                tc = cand_type[i]
                if not ((tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators)):
                    continue

            dx = cand_x[i] - eye_x
            dy = cand_y[i] - eye_y
            dist = math.sqrt(dx * dx + dy * dy)
            cr = cand_r[i]
            if dist - cr > vision_radius:
                continue

            obj_angle = math.atan2(dy, dx)
            ang = (obj_angle - agent_angle + math.pi) % (2.0 * math.pi) - math.pi
            if dist <= cr:
                half_span = math.pi
            else:
                ratio = cr / dist
                if ratio > 1.0:
                    ratio = 1.0
                elif ratio < 0.0:
                    ratio = 0.0
                half_span = math.asin(ratio)
            if abs(ang) > half_fov + half_span:
                continue

            ox = -dx
            oy = -dy
            c = ox * ox + oy * oy - cr * cr
            for r_idx in range(retina_count):
                if retina_count > 1:
                    rel = -half_fov + (r_idx / (retina_count - 1)) * (2.0 * half_fov)
                else:
                    rel = 0.0
                ray_angle = agent_angle + rel
                dir_x = math.cos(ray_angle)
                dir_y = math.sin(ray_angle)
                b = dir_x * ox + dir_y * oy
                disc = b * b - c
                if disc < 0.0:
                    continue
                root = math.sqrt(disc)
                t1 = -b - root
                t2 = -b + root
                t = np.inf
                if t1 >= 0.0:
                    t = t1
                if t2 >= 0.0 and t2 < t:
                    t = t2
                if t >= 0.0 and t <= vision_radius and t < ray_best[r_idx]:
                    ray_best[r_idx] = t

        for r_idx in range(retina_count):
            best = ray_best[r_idx]
            if math.isfinite(best):
                act = (vision_radius - best) / vision_radius
                if act < 0.0:
                    act = 0.0
                elif act > 1.0:
                    act = 1.0
                out[r_idx] = act

    @njit(cache=True, fastmath=True)
    def _retina_fullbody_precomputed_kernel_numba(
        cand_x,
        cand_y,
        cand_r,
        cand_type,
        self_flags,
        eye_x,
        eye_y,
        agent_angle,
        vision_radius,
        half_fov,
        ray_cos_rel,
        ray_sin_rel,
        retina_count,
        see_food,
        see_bacteria,
        see_predators,
        spatial_filtered,
        out,
    ):
        for r_idx in range(retina_count):
            out[r_idx] = 0.0
        if retina_count <= 0 or half_fov <= 0.0 or vision_radius <= 0.0:
            return
        ray_best = np.empty(retina_count, dtype=np.float64)
        for r_idx in range(retina_count):
            ray_best[r_idx] = np.inf

        cos_a = math.cos(agent_angle)
        sin_a = math.sin(agent_angle)

        for i in range(cand_x.shape[0]):
            if self_flags[i]:
                continue
            if not spatial_filtered:
                tc = cand_type[i]
                if not ((tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators)):
                    continue

            dx = cand_x[i] - eye_x
            dy = cand_y[i] - eye_y
            dist = math.sqrt(dx * dx + dy * dy)
            cr = cand_r[i]
            if dist - cr > vision_radius:
                continue

            obj_angle = math.atan2(dy, dx)
            ang = (obj_angle - agent_angle + math.pi) % (2.0 * math.pi) - math.pi
            if dist <= cr:
                half_span = math.pi
            else:
                ratio = cr / dist
                if ratio > 1.0:
                    ratio = 1.0
                elif ratio < 0.0:
                    ratio = 0.0
                half_span = math.asin(ratio)
            if abs(ang) > half_fov + half_span:
                continue

            ox = -dx
            oy = -dy
            c = ox * ox + oy * oy - cr * cr
            for r_idx in range(retina_count):
                dir_x = cos_a * ray_cos_rel[r_idx] - sin_a * ray_sin_rel[r_idx]
                dir_y = sin_a * ray_cos_rel[r_idx] + cos_a * ray_sin_rel[r_idx]
                b = dir_x * ox + dir_y * oy
                disc = b * b - c
                if disc < 0.0:
                    continue
                root = math.sqrt(disc)
                t1 = -b - root
                t2 = -b + root
                t = np.inf
                if t1 >= 0.0:
                    t = t1
                if t2 >= 0.0 and t2 < t:
                    t = t2
                if t >= 0.0 and t <= vision_radius and t < ray_best[r_idx]:
                    ray_best[r_idx] = t

        for r_idx in range(retina_count):
            best = ray_best[r_idx]
            if math.isfinite(best):
                act = (vision_radius - best) / vision_radius
                if act < 0.0:
                    act = 0.0
                elif act > 1.0:
                    act = 1.0
                out[r_idx] = act

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_batch_single_kernel_numba(
        eye_x,
        eye_y,
        agent_angle,
        vision_radius,
        half_fov,
        cand_start,
        cand_count,
        cand_x,
        cand_y,
        cand_r,
        cand_type,
        self_flags,
        retina_count,
        see_food,
        see_bacteria,
        see_predators,
        spatial_filtered,
        out,
    ):
        for agent_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                out[agent_idx, r_idx] = 0.0
            hf = half_fov[agent_idx]
            vr = vision_radius[agent_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue
            ray_best = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
            start = cand_start[agent_idx]
            end = start + cand_count[agent_idx]
            ex = eye_x[agent_idx]
            ey = eye_y[agent_idx]
            aa = agent_angle[agent_idx]
            for i in range(start, end):
                if self_flags[i]:
                    continue
                if not spatial_filtered:
                    tc = cand_type[i]
                    if not ((tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators)):
                        continue
                dx = cand_x[i] - ex
                dy = cand_y[i] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[i]
                if dist - cr > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if dist <= cr:
                    half_span = math.pi
                else:
                    ratio = cr / dist
                    if ratio > 1.0:
                        ratio = 1.0
                    elif ratio < 0.0:
                        ratio = 0.0
                    half_span = math.asin(ratio)
                if abs(ang) > hf + half_span:
                    continue
                eff_dist = dist - cr
                if eff_dist < 0.0:
                    eff_dist = 0.0
                elif eff_dist > vr:
                    eff_dist = vr
                if retina_count > 1:
                    rel = (ang + hf) / (2.0 * hf) * (retina_count - 1)
                    ray_idx = int(math.floor(rel + 0.5))
                    if ray_idx < 0:
                        ray_idx = 0
                    elif ray_idx >= retina_count:
                        ray_idx = retina_count - 1
                else:
                    ray_idx = 0
                if eff_dist < ray_best[ray_idx]:
                    ray_best[ray_idx] = eff_dist
            for r_idx in range(retina_count):
                best = ray_best[r_idx]
                if math.isfinite(best):
                    act = (vr - best) / vr
                    if act < 0.0:
                        act = 0.0
                    elif act > 1.0:
                        act = 1.0
                    out[agent_idx, r_idx] = act

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_batch_fullbody_kernel_numba(
        eye_x,
        eye_y,
        agent_angle,
        vision_radius,
        half_fov,
        cand_start,
        cand_count,
        cand_x,
        cand_y,
        cand_r,
        cand_type,
        self_flags,
        retina_count,
        see_food,
        see_bacteria,
        see_predators,
        spatial_filtered,
        out,
    ):
        for agent_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                out[agent_idx, r_idx] = 0.0
            hf = half_fov[agent_idx]
            vr = vision_radius[agent_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue
            ray_best = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
            start = cand_start[agent_idx]
            end = start + cand_count[agent_idx]
            ex = eye_x[agent_idx]
            ey = eye_y[agent_idx]
            aa = agent_angle[agent_idx]

            for i in range(start, end):
                if self_flags[i]:
                    continue
                if not spatial_filtered:
                    tc = cand_type[i]
                    if not ((tc == 0 and see_food) or (tc == 1 and see_bacteria) or (tc == 2 and see_predators)):
                        continue

                dx = cand_x[i] - ex
                dy = cand_y[i] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[i]
                if dist - cr > vr:
                    continue

                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if dist <= cr:
                    half_span = math.pi
                else:
                    ratio = cr / dist
                    if ratio > 1.0:
                        ratio = 1.0
                    elif ratio < 0.0:
                        ratio = 0.0
                    half_span = math.asin(ratio)
                if abs(ang) > hf + half_span:
                    continue

                ox = -dx
                oy = -dy
                c = ox * ox + oy * oy - cr * cr
                for r_idx in range(retina_count):
                    if retina_count > 1:
                        rel = -hf + (r_idx / (retina_count - 1)) * (2.0 * hf)
                    else:
                        rel = 0.0
                    ray_angle = aa + rel
                    dir_x = math.cos(ray_angle)
                    dir_y = math.sin(ray_angle)
                    b = dir_x * ox + dir_y * oy
                    disc = b * b - c
                    if disc < 0.0:
                        continue
                    root = math.sqrt(disc)
                    t1 = -b - root
                    t2 = -b + root
                    t = np.inf
                    if t1 >= 0.0:
                        t = t1
                    if t2 >= 0.0 and t2 < t:
                        t = t2
                    if t >= 0.0 and t <= vr and t < ray_best[r_idx]:
                        ray_best[r_idx] = t

            for r_idx in range(retina_count):
                best = ray_best[r_idx]
                if math.isfinite(best):
                    act = (vr - best) / vr
                    if act < 0.0:
                        act = 0.0
                    elif act > 1.0:
                        act = 1.0
                    out[agent_idx, r_idx] = act

    @njit(cache=True, fastmath=True, parallel=True)
    def _brain_layer_forward_kernel_numba(inputs, weights, biases, apply_tanh, out):
        for agent_idx in prange(inputs.shape[0]):
            for out_idx in range(weights.shape[1]):
                total = biases[agent_idx, out_idx]
                for in_idx in range(inputs.shape[1]):
                    total += weights[agent_idx, out_idx, in_idx] * inputs[agent_idx, in_idx]
                if apply_tanh:
                    total = math.tanh(total)
                out[agent_idx, out_idx] = total


def apply_locomotion_energy_arrays(
    x: np.ndarray,
    y: np.ndarray,
    radius: np.ndarray,
    angle: np.ndarray,
    vx: np.ndarray,
    vy: np.ndarray,
    energy: np.ndarray,
    outputs: np.ndarray,
    max_speed: np.ndarray,
    max_turn: np.ndarray,
    dt: float,
    world_shape_code: int,
    world_width: float,
    world_height: float,
    world_cx: float,
    world_cy: float,
    world_radius: float,
    allow_reverse: bool,
    inertia: float,
    v0_cost: float,
    vmax_cost: float,
    vmax_ref: float,
    energy_cap: float,
    *,
    prefer_numba: bool = True,
    allow_numpy: bool = True,
    force_python: bool = False,
) -> Optional[str]:
    """Apply locomotion + energy to arrays.

    Returns the backend name used, or None if the caller should use the old
    object path.
    """
    global _NUMBA_RUNTIME_FAILED
    if outputs.ndim != 2 or outputs.shape[1] < 2:
        return None
    if force_python:
        _locomotion_energy_kernel_python(
            x, y, radius, angle, vx, vy, energy, outputs, max_speed, max_turn,
            dt, world_shape_code, world_width, world_height, world_cx, world_cy,
            world_radius, allow_reverse, inertia, v0_cost, vmax_cost, vmax_ref, energy_cap,
        )
        return "python-array"
    if prefer_numba and NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED:  # pragma: no cover - optional dependency
        try:
            _locomotion_energy_kernel_numba(
                x, y, radius, angle, vx, vy, energy, outputs, max_speed, max_turn,
                dt, world_shape_code, world_width, world_height, world_cx, world_cy,
                world_radius, allow_reverse, inertia, v0_cost, vmax_cost, vmax_ref, energy_cap,
            )
            return "numba"
        except Exception:
            _NUMBA_RUNTIME_FAILED = True
    if allow_numpy:
        _locomotion_energy_kernel_numpy(
            x, y, radius, angle, vx, vy, energy, outputs, max_speed, max_turn,
            dt, world_shape_code, world_width, world_height, world_cx, world_cy,
            world_radius, allow_reverse, inertia, v0_cost, vmax_cost, vmax_ref, energy_cap,
        )
        return "numpy-array"
    return None


def retina_single_kernel(
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_type: np.ndarray,
    self_flags: np.ndarray,
    eye_x: float,
    eye_y: float,
    agent_angle: float,
    vision_radius: float,
    half_fov: float,
    retina_count: int,
    see_food: bool,
    see_bacteria: bool,
    see_predators: bool,
    spatial_filtered: bool,
    out: np.ndarray,
) -> bool:
    """Fill ``out`` with single-mode retina activations using Numba if present."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_single_kernel_numba(
            cand_x, cand_y, cand_r, cand_type, self_flags,
            float(eye_x), float(eye_y), float(agent_angle), float(vision_radius),
            float(half_fov), int(retina_count), bool(see_food), bool(see_bacteria),
            bool(see_predators), bool(spatial_filtered), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_fullbody_kernel(
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_type: np.ndarray,
    self_flags: np.ndarray,
    eye_x: float,
    eye_y: float,
    agent_angle: float,
    vision_radius: float,
    half_fov: float,
    retina_count: int,
    see_food: bool,
    see_bacteria: bool,
    see_predators: bool,
    spatial_filtered: bool,
    out: np.ndarray,
) -> bool:
    """Fill ``out`` with fullbody retina activations using Numba if present."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_fullbody_kernel_numba(
            cand_x, cand_y, cand_r, cand_type, self_flags,
            float(eye_x), float(eye_y), float(agent_angle), float(vision_radius),
            float(half_fov), int(retina_count), bool(see_food), bool(see_bacteria),
            bool(see_predators), bool(spatial_filtered), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_fullbody_precomputed_kernel(
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_type: np.ndarray,
    self_flags: np.ndarray,
    eye_x: float,
    eye_y: float,
    agent_angle: float,
    vision_radius: float,
    half_fov: float,
    ray_cos_rel: np.ndarray,
    ray_sin_rel: np.ndarray,
    retina_count: int,
    see_food: bool,
    see_bacteria: bool,
    see_predators: bool,
    spatial_filtered: bool,
    out: np.ndarray,
) -> bool:
    """Fullbody retina using precomputed relative ray vectors."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_fullbody_precomputed_kernel_numba(
            cand_x, cand_y, cand_r, cand_type, self_flags,
            float(eye_x), float(eye_y), float(agent_angle), float(vision_radius),
            float(half_fov), ray_cos_rel, ray_sin_rel, int(retina_count),
            bool(see_food), bool(see_bacteria), bool(see_predators),
            bool(spatial_filtered), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_batch_single_kernel(
    eye_x: np.ndarray,
    eye_y: np.ndarray,
    agent_angle: np.ndarray,
    vision_radius: np.ndarray,
    half_fov: np.ndarray,
    cand_start: np.ndarray,
    cand_count: np.ndarray,
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_type: np.ndarray,
    self_flags: np.ndarray,
    retina_count: int,
    see_food: bool,
    see_bacteria: bool,
    see_predators: bool,
    spatial_filtered: bool,
    out: np.ndarray,
) -> bool:
    """Fill ``out`` for a batch of agents using the single retina mapping."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_single_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, cand_type,
            self_flags, int(retina_count), bool(see_food), bool(see_bacteria),
            bool(see_predators), bool(spatial_filtered), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_batch_fullbody_kernel(
    eye_x: np.ndarray,
    eye_y: np.ndarray,
    agent_angle: np.ndarray,
    vision_radius: np.ndarray,
    half_fov: np.ndarray,
    cand_start: np.ndarray,
    cand_count: np.ndarray,
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_type: np.ndarray,
    self_flags: np.ndarray,
    retina_count: int,
    see_food: bool,
    see_bacteria: bool,
    see_predators: bool,
    spatial_filtered: bool,
    out: np.ndarray,
) -> bool:
    """Fill ``out`` for a batch of agents using exact fullbody intersections."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_fullbody_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, cand_type,
            self_flags, int(retina_count), bool(see_food), bool(see_bacteria),
            bool(see_predators), bool(spatial_filtered), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def brain_layer_forward_kernel(
    inputs: np.ndarray,
    weights: np.ndarray,
    biases: np.ndarray,
    apply_tanh: bool,
    out: np.ndarray,
) -> bool:
    """Forward one neural layer for many agents with agent-specific weights."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _brain_layer_forward_kernel_numba(inputs, weights, biases, bool(apply_tanh), out)
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False
