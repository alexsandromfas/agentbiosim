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
    def _retina_batch_sector_kernel_numba(
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
        self_flags,
        cand_color_r,
        cand_color_g,
        cand_color_b,
        channel_codes,
        retina_count,
        out,
        distance_out,
    ):
        channel_count = channel_codes.shape[0]
        for agent_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                distance_out[agent_idx, r_idx] = 0.0
                for ch_idx in range(channel_count):
                    out[agent_idx, r_idx, ch_idx] = 0.0
            hf = half_fov[agent_idx]
            vr = vision_radius[agent_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue

            ray_best = np.empty(retina_count, dtype=np.float64)
            ray_r = np.empty(retina_count, dtype=np.float64)
            ray_g = np.empty(retina_count, dtype=np.float64)
            ray_b = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
                ray_r[r_idx] = 0.0
                ray_g[r_idx] = 0.0
                ray_b[r_idx] = 0.0

            start = cand_start[agent_idx]
            end = start + cand_count[agent_idx]
            ex = eye_x[agent_idx]
            ey = eye_y[agent_idx]
            aa = agent_angle[agent_idx]
            for i in range(start, end):
                if self_flags[i]:
                    continue
                dx = cand_x[i] - ex
                dy = cand_y[i] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[i]
                if dist - cr > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if abs(ang) > hf:
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
                    ray_r[ray_idx] = cand_color_r[i]
                    ray_g[ray_idx] = cand_color_g[i]
                    ray_b[ray_idx] = cand_color_b[i]

            for r_idx in range(retina_count):
                best = ray_best[r_idx]
                if not math.isfinite(best):
                    continue
                act = (vr - best) / vr
                if act < 0.0:
                    act = 0.0
                elif act > 1.0:
                    act = 1.0
                distance_out[agent_idx, r_idx] = act
                for ch_idx in range(channel_count):
                    code = channel_codes[ch_idx]
                    if code == 1:
                        out[agent_idx, r_idx, ch_idx] = ray_r[r_idx]
                    elif code == 2:
                        out[agent_idx, r_idx, ch_idx] = ray_g[r_idx]
                    elif code == 3:
                        out[agent_idx, r_idx, ch_idx] = ray_b[r_idx]
                    elif code == 4:
                        out[agent_idx, r_idx, ch_idx] = act * ray_r[r_idx]
                    elif code == 5:
                        out[agent_idx, r_idx, ch_idx] = act * ray_g[r_idx]
                    elif code == 6:
                        out[agent_idx, r_idx, ch_idx] = act * ray_b[r_idx]
                    else:
                        out[agent_idx, r_idx, ch_idx] = act

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_batch_sector_distance_kernel_numba(
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
        self_flags,
        retina_count,
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
                dx = cand_x[i] - ex
                dy = cand_y[i] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[i]
                if dist - cr > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if abs(ang) > hf:
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

    @njit(cache=True, fastmath=True)
    def _bins_activation_numba(distance, vision_radius, subdivisions, distribution_code, falloff_code):
        if vision_radius <= 1e-12:
            return 0.0
        norm = distance / vision_radius
        if norm < 0.0:
            norm = 0.0
        elif norm > 1.0:
            norm = 1.0
        if falloff_code == 3:  # none
            return 1.0
        if subdivisions <= 1:
            representative = norm
            band = 0
        else:
            if distribution_code == 1:  # near_detail
                band = int(math.floor(math.sqrt(norm) * subdivisions))
                if band < 0:
                    band = 0
                elif band >= subdivisions:
                    band = subdivisions - 1
                left = (band / subdivisions) * (band / subdivisions)
                right_ratio = (band + 1) / subdivisions
                right = right_ratio * right_ratio
                representative = (left + right) * 0.5
            else:
                band = int(math.floor(norm * subdivisions))
                if band < 0:
                    band = 0
                elif band >= subdivisions:
                    band = subdivisions - 1
                representative = (band + 0.5) / subdivisions
        if falloff_code == 2:  # step
            value = (subdivisions - band) / subdivisions
        else:
            value = 1.0 - representative
            if falloff_code == 1:  # quadratic
                value *= value
        if value < 0.0:
            return 0.0
        if value > 1.0:
            return 1.0
        return value

    @njit(cache=True, fastmath=True)
    def _bins_sector_index_numba(angle, half_fov, retina_count):
        if retina_count <= 1:
            return 0
        rel = (angle + half_fov) / (2.0 * half_fov) * retina_count
        idx = int(math.floor(rel))
        if idx < 0:
            idx = 0
        elif idx >= retina_count:
            idx = retina_count - 1
        return idx

    @njit(cache=True, fastmath=True)
    def _bins_channel_value_numba(code, activation, color_r, color_g, color_b):
        if code == 1:
            return color_r
        if code == 2:
            return color_g
        if code == 3:
            return color_b
        if code == 4:
            return activation * color_r
        if code == 5:
            return activation * color_g
        if code == 6:
            return activation * color_b
        return activation

    @njit(cache=True, fastmath=True)
    def _bins_update_sector_numba(
        sector_idx,
        eff_dist,
        activation,
        color_r,
        color_g,
        color_b,
        mode_code,
        channel_codes,
        best_dist,
        best_score,
        best_r,
        best_g,
        best_b,
        accum,
        weights,
        distance_out_row,
    ):
        if sector_idx < 0 or sector_idx >= best_dist.shape[0]:
            return
        channel_count = channel_codes.shape[0]
        if mode_code == 2:  # sum_saturating
            for ch_idx in range(channel_count):
                value = _bins_channel_value_numba(channel_codes[ch_idx], activation, color_r, color_g, color_b)
                new_value = accum[sector_idx, ch_idx] + value
                accum[sector_idx, ch_idx] = 1.0 if new_value > 1.0 else new_value
            if activation > distance_out_row[sector_idx]:
                distance_out_row[sector_idx] = activation
            return
        if mode_code == 3:  # weighted_average
            weight = activation
            if weight < 1e-9:
                weight = 1e-9
            for ch_idx in range(channel_count):
                value = _bins_channel_value_numba(channel_codes[ch_idx], activation, color_r, color_g, color_b)
                accum[sector_idx, ch_idx] += value * weight
            weights[sector_idx] += weight
            if activation > distance_out_row[sector_idx]:
                distance_out_row[sector_idx] = activation
            return
        if mode_code == 1:  # strongest
            score = activation
            strongest_color = color_r
            if color_g > strongest_color:
                strongest_color = color_g
            if color_b > strongest_color:
                strongest_color = color_b
            score *= strongest_color
            if score <= best_score[sector_idx]:
                return
            best_score[sector_idx] = score
        else:  # nearest
            if eff_dist >= best_dist[sector_idx]:
                return
        best_dist[sector_idx] = eff_dist
        best_r[sector_idx] = color_r
        best_g[sector_idx] = color_g
        best_b[sector_idx] = color_b

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_batch_bins_kernel_numba(
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
        self_flags,
        cand_color_r,
        cand_color_g,
        cand_color_b,
        channel_codes,
        retina_count,
        mode_code,
        subdivisions,
        distribution_code,
        falloff_code,
        projection_code,
        candidate_limit,
        out,
        distance_out,
    ):
        channel_count = channel_codes.shape[0]
        for eye_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                distance_out[eye_idx, r_idx] = 0.0
                for ch_idx in range(channel_count):
                    out[eye_idx, r_idx, ch_idx] = 0.0
            hf = half_fov[eye_idx]
            vr = vision_radius[eye_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue

            best_dist = np.empty(retina_count, dtype=np.float64)
            best_score = np.empty(retina_count, dtype=np.float64)
            best_r = np.empty(retina_count, dtype=np.float64)
            best_g = np.empty(retina_count, dtype=np.float64)
            best_b = np.empty(retina_count, dtype=np.float64)
            accum = np.empty((retina_count, channel_count), dtype=np.float64)
            weights = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                best_dist[r_idx] = np.inf
                best_score[r_idx] = -np.inf
                best_r[r_idx] = 0.0
                best_g[r_idx] = 0.0
                best_b[r_idx] = 0.0
                weights[r_idx] = 0.0
                for ch_idx in range(channel_count):
                    accum[r_idx, ch_idx] = 0.0

            start = cand_start[eye_idx]
            end = start + cand_count[eye_idx]
            if candidate_limit > 0 and start + candidate_limit < end:
                end = start + candidate_limit
            ex = eye_x[eye_idx]
            ey = eye_y[eye_idx]
            aa = agent_angle[eye_idx]
            has_color = cand_color_r.shape[0] > 0

            for cand_idx in range(start, end):
                if self_flags[cand_idx]:
                    continue
                dx = cand_x[cand_idx] - ex
                dy = cand_y[cand_idx] - ey
                center_dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[cand_idx]
                eff_dist = center_dist - cr
                if eff_dist > vr:
                    continue
                if eff_dist < 0.0:
                    eff_dist = 0.0
                elif eff_dist > vr:
                    eff_dist = vr
                angle = math.atan2(dy, dx)
                rel_angle = (angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                span = 0.0
                if projection_code != 0 and center_dist > 1e-9 and cr > 0.0:
                    ratio = cr / center_dist
                    if ratio > 1.0:
                        ratio = 1.0
                    elif ratio < 0.0:
                        ratio = 0.0
                    span = math.asin(ratio)
                if abs(rel_angle) > hf + span:
                    continue
                activation = _bins_activation_numba(eff_dist, vr, subdivisions, distribution_code, falloff_code)
                color_r = cand_color_r[cand_idx] if has_color else 0.0
                color_g = cand_color_g[cand_idx] if has_color else 0.0
                color_b = cand_color_b[cand_idx] if has_color else 0.0

                if projection_code == 2 and span > 1e-9:  # apparent_size
                    left = rel_angle - span
                    right = rel_angle + span
                    if left < -hf:
                        left = -hf
                    if right > hf:
                        right = hf
                    if left <= right:
                        start_sector = _bins_sector_index_numba(left, hf, retina_count)
                        end_sector = _bins_sector_index_numba(right, hf, retina_count)
                        if end_sector < start_sector:
                            tmp = start_sector
                            start_sector = end_sector
                            end_sector = tmp
                        for sector_idx in range(start_sector, end_sector + 1):
                            _bins_update_sector_numba(
                                sector_idx, eff_dist, activation, color_r, color_g, color_b,
                                mode_code, channel_codes, best_dist, best_score, best_r, best_g, best_b,
                                accum, weights, distance_out[eye_idx],
                            )
                    continue

                center_sector = _bins_sector_index_numba(rel_angle, hf, retina_count)
                _bins_update_sector_numba(
                    center_sector, eff_dist, activation, color_r, color_g, color_b,
                    mode_code, channel_codes, best_dist, best_score, best_r, best_g, best_b,
                    accum, weights, distance_out[eye_idx],
                )
                if projection_code == 1 and span > 1e-9:  # center + edges
                    left_sector = _bins_sector_index_numba(rel_angle - span, hf, retina_count)
                    right_sector = _bins_sector_index_numba(rel_angle + span, hf, retina_count)
                    if left_sector != center_sector:
                        _bins_update_sector_numba(
                            left_sector, eff_dist, activation, color_r, color_g, color_b,
                            mode_code, channel_codes, best_dist, best_score, best_r, best_g, best_b,
                            accum, weights, distance_out[eye_idx],
                        )
                    if right_sector != center_sector and right_sector != left_sector:
                        _bins_update_sector_numba(
                            right_sector, eff_dist, activation, color_r, color_g, color_b,
                            mode_code, channel_codes, best_dist, best_score, best_r, best_g, best_b,
                            accum, weights, distance_out[eye_idx],
                        )

            for r_idx in range(retina_count):
                if mode_code == 2:
                    for ch_idx in range(channel_count):
                        out[eye_idx, r_idx, ch_idx] = accum[r_idx, ch_idx]
                    continue
                if mode_code == 3:
                    weight = weights[r_idx]
                    if weight > 0.0:
                        for ch_idx in range(channel_count):
                            out[eye_idx, r_idx, ch_idx] = accum[r_idx, ch_idx] / weight
                    continue
                if math.isfinite(best_dist[r_idx]):
                    activation = _bins_activation_numba(best_dist[r_idx], vr, subdivisions, distribution_code, falloff_code)
                    distance_out[eye_idx, r_idx] = activation
                    for ch_idx in range(channel_count):
                        out[eye_idx, r_idx, ch_idx] = _bins_channel_value_numba(
                            channel_codes[ch_idx], activation, best_r[r_idx], best_g[r_idx], best_b[r_idx]
                        )

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_batch_bins_nearest_center_kernel_numba(
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
        self_flags,
        cand_color_r,
        cand_color_g,
        cand_color_b,
        channel_codes,
        retina_count,
        subdivisions,
        distribution_code,
        falloff_code,
        candidate_limit,
        out,
        distance_out,
    ):
        channel_count = channel_codes.shape[0]
        for eye_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                distance_out[eye_idx, r_idx] = 0.0
                for ch_idx in range(channel_count):
                    out[eye_idx, r_idx, ch_idx] = 0.0
            hf = half_fov[eye_idx]
            vr = vision_radius[eye_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue

            ray_best = np.empty(retina_count, dtype=np.float64)
            ray_r = np.empty(retina_count, dtype=np.float64)
            ray_g = np.empty(retina_count, dtype=np.float64)
            ray_b = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
                ray_r[r_idx] = 0.0
                ray_g[r_idx] = 0.0
                ray_b[r_idx] = 0.0

            start = cand_start[eye_idx]
            end = start + cand_count[eye_idx]
            if candidate_limit > 0 and start + candidate_limit < end:
                end = start + candidate_limit
            ex = eye_x[eye_idx]
            ey = eye_y[eye_idx]
            aa = agent_angle[eye_idx]
            has_color = cand_color_r.shape[0] > 0
            for i in range(start, end):
                if self_flags[i]:
                    continue
                dx = cand_x[i] - ex
                dy = cand_y[i] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[i]
                eff_dist = dist - cr
                if eff_dist > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if abs(ang) > hf:
                    continue
                if eff_dist < 0.0:
                    eff_dist = 0.0
                elif eff_dist > vr:
                    eff_dist = vr
                ray_idx = _bins_sector_index_numba(ang, hf, retina_count)
                if eff_dist < ray_best[ray_idx]:
                    ray_best[ray_idx] = eff_dist
                    if has_color:
                        ray_r[ray_idx] = cand_color_r[i]
                        ray_g[ray_idx] = cand_color_g[i]
                        ray_b[ray_idx] = cand_color_b[i]

            for r_idx in range(retina_count):
                best = ray_best[r_idx]
                if not math.isfinite(best):
                    continue
                act = _bins_activation_numba(best, vr, subdivisions, distribution_code, falloff_code)
                distance_out[eye_idx, r_idx] = act
                for ch_idx in range(channel_count):
                    out[eye_idx, r_idx, ch_idx] = _bins_channel_value_numba(
                        channel_codes[ch_idx], act, ray_r[r_idx], ray_g[r_idx], ray_b[r_idx]
                    )

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_global_sector_distance_kernel_numba(
        eye_x,
        eye_y,
        owner_ids,
        agent_angle,
        vision_radius,
        half_fov,
        cand_x,
        cand_y,
        cand_r,
        cand_ids,
        retina_count,
        out,
    ):
        for eye_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                out[eye_idx, r_idx] = 0.0
            hf = half_fov[eye_idx]
            vr = vision_radius[eye_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue
            ray_best = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
            ex = eye_x[eye_idx]
            ey = eye_y[eye_idx]
            aa = agent_angle[eye_idx]
            owner_id = owner_ids[eye_idx]
            for cand_idx in range(cand_x.shape[0]):
                if cand_ids[cand_idx] == owner_id:
                    continue
                dx = cand_x[cand_idx] - ex
                dy = cand_y[cand_idx] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[cand_idx]
                if dist - cr > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if abs(ang) > hf:
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
                    out[eye_idx, r_idx] = act

    @njit(cache=True, fastmath=True, parallel=True)
    def _retina_global_sector_kernel_numba(
        eye_x,
        eye_y,
        owner_ids,
        agent_angle,
        vision_radius,
        half_fov,
        cand_x,
        cand_y,
        cand_r,
        cand_ids,
        cand_color_r,
        cand_color_g,
        cand_color_b,
        channel_codes,
        retina_count,
        out,
        distance_out,
    ):
        channel_count = channel_codes.shape[0]
        for eye_idx in prange(eye_x.shape[0]):
            for r_idx in range(retina_count):
                distance_out[eye_idx, r_idx] = 0.0
                for ch_idx in range(channel_count):
                    out[eye_idx, r_idx, ch_idx] = 0.0
            hf = half_fov[eye_idx]
            vr = vision_radius[eye_idx]
            if retina_count <= 0 or hf <= 0.0 or vr <= 0.0:
                continue
            ray_best = np.empty(retina_count, dtype=np.float64)
            ray_r = np.empty(retina_count, dtype=np.float64)
            ray_g = np.empty(retina_count, dtype=np.float64)
            ray_b = np.empty(retina_count, dtype=np.float64)
            for r_idx in range(retina_count):
                ray_best[r_idx] = np.inf
                ray_r[r_idx] = 0.0
                ray_g[r_idx] = 0.0
                ray_b[r_idx] = 0.0
            ex = eye_x[eye_idx]
            ey = eye_y[eye_idx]
            aa = agent_angle[eye_idx]
            owner_id = owner_ids[eye_idx]
            for cand_idx in range(cand_x.shape[0]):
                if cand_ids[cand_idx] == owner_id:
                    continue
                dx = cand_x[cand_idx] - ex
                dy = cand_y[cand_idx] - ey
                dist = math.sqrt(dx * dx + dy * dy)
                cr = cand_r[cand_idx]
                if dist - cr > vr:
                    continue
                obj_angle = math.atan2(dy, dx)
                ang = (obj_angle - aa + math.pi) % (2.0 * math.pi) - math.pi
                if abs(ang) > hf:
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
                    ray_r[ray_idx] = cand_color_r[cand_idx]
                    ray_g[ray_idx] = cand_color_g[cand_idx]
                    ray_b[ray_idx] = cand_color_b[cand_idx]
            for r_idx in range(retina_count):
                best = ray_best[r_idx]
                if not math.isfinite(best):
                    continue
                act = (vr - best) / vr
                if act < 0.0:
                    act = 0.0
                elif act > 1.0:
                    act = 1.0
                distance_out[eye_idx, r_idx] = act
                for ch_idx in range(channel_count):
                    code = channel_codes[ch_idx]
                    if code == 1:
                        out[eye_idx, r_idx, ch_idx] = ray_r[r_idx]
                    elif code == 2:
                        out[eye_idx, r_idx, ch_idx] = ray_g[r_idx]
                    elif code == 3:
                        out[eye_idx, r_idx, ch_idx] = ray_b[r_idx]
                    elif code == 4:
                        out[eye_idx, r_idx, ch_idx] = act * ray_r[r_idx]
                    elif code == 5:
                        out[eye_idx, r_idx, ch_idx] = act * ray_g[r_idx]
                    elif code == 6:
                        out[eye_idx, r_idx, ch_idx] = act * ray_b[r_idx]
                    else:
                        out[eye_idx, r_idx, ch_idx] = act

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


def retina_batch_sector_kernel(
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
    self_flags: np.ndarray,
    cand_color_r: np.ndarray,
    cand_color_g: np.ndarray,
    cand_color_b: np.ndarray,
    channel_codes: np.ndarray,
    retina_count: int,
    out: np.ndarray,
    distance_out: np.ndarray,
) -> bool:
    """Fill ``out`` for a batch of eyes using fast angular sector mapping."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_sector_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, self_flags,
            cand_color_r, cand_color_g, cand_color_b, channel_codes,
            int(retina_count), out, distance_out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_batch_sector_distance_kernel(
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
    self_flags: np.ndarray,
    retina_count: int,
    out: np.ndarray,
) -> bool:
    """Fill ``out`` for fast angular sectors with distance-only inputs."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_sector_distance_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, self_flags,
            int(retina_count), out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_batch_bins_kernel(
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
    self_flags: np.ndarray,
    cand_color_r: np.ndarray,
    cand_color_g: np.ndarray,
    cand_color_b: np.ndarray,
    channel_codes: np.ndarray,
    retina_count: int,
    mode_code: int,
    subdivisions: int,
    distribution_code: int,
    falloff_code: int,
    projection_code: int,
    candidate_limit: int,
    out: np.ndarray,
    distance_out: np.ndarray,
) -> bool:
    """Fill configurable angular-bin retina outputs for a batch of eyes."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_bins_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, self_flags,
            cand_color_r, cand_color_g, cand_color_b, channel_codes,
            int(retina_count), int(mode_code), int(subdivisions),
            int(distribution_code), int(falloff_code), int(projection_code),
            int(candidate_limit), out, distance_out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_batch_bins_nearest_center_kernel(
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
    self_flags: np.ndarray,
    cand_color_r: np.ndarray,
    cand_color_g: np.ndarray,
    cand_color_b: np.ndarray,
    channel_codes: np.ndarray,
    retina_count: int,
    subdivisions: int,
    distribution_code: int,
    falloff_code: int,
    candidate_limit: int,
    out: np.ndarray,
    distance_out: np.ndarray,
) -> bool:
    """Fast path for the common angular-bin mode: nearest object, center sector."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - depends on optional dep
        return False
    try:  # pragma: no cover - depends on optional dep
        _retina_batch_bins_nearest_center_kernel_numba(
            eye_x, eye_y, agent_angle, vision_radius, half_fov,
            cand_start, cand_count, cand_x, cand_y, cand_r, self_flags,
            cand_color_r, cand_color_g, cand_color_b, channel_codes,
            int(retina_count), int(subdivisions), int(distribution_code),
            int(falloff_code), int(candidate_limit), out, distance_out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_global_sector_kernel(
    eye_x: np.ndarray,
    eye_y: np.ndarray,
    owner_ids: np.ndarray,
    agent_angle: np.ndarray,
    vision_radius: np.ndarray,
    half_fov: np.ndarray,
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_ids: np.ndarray,
    cand_color_r: np.ndarray,
    cand_color_g: np.ndarray,
    cand_color_b: np.ndarray,
    channel_codes: np.ndarray,
    retina_count: int,
    out: np.ndarray,
    distance_out: np.ndarray,
) -> bool:
    """Fill sector retina outputs against one shared candidate snapshot."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - optional dependency
        return False
    try:  # pragma: no cover - optional dependency
        _retina_global_sector_kernel_numba(
            eye_x, eye_y, owner_ids, agent_angle, vision_radius, half_fov,
            cand_x, cand_y, cand_r, cand_ids, cand_color_r, cand_color_g,
            cand_color_b, channel_codes, int(retina_count), out, distance_out,
        )
        return True
    except Exception:
        _NUMBA_RUNTIME_FAILED = True
        return False


def retina_global_sector_distance_kernel(
    eye_x: np.ndarray,
    eye_y: np.ndarray,
    owner_ids: np.ndarray,
    agent_angle: np.ndarray,
    vision_radius: np.ndarray,
    half_fov: np.ndarray,
    cand_x: np.ndarray,
    cand_y: np.ndarray,
    cand_r: np.ndarray,
    cand_ids: np.ndarray,
    retina_count: int,
    out: np.ndarray,
) -> bool:
    """Fill distance-only sector outputs against one candidate snapshot."""
    global _NUMBA_RUNTIME_FAILED
    if not (NUMBA_AVAILABLE and not _NUMBA_RUNTIME_FAILED):  # pragma: no cover - optional dependency
        return False
    try:  # pragma: no cover - optional dependency
        _retina_global_sector_distance_kernel_numba(
            eye_x, eye_y, owner_ids, agent_angle, vision_radius, half_fov,
            cand_x, cand_y, cand_r, cand_ids, int(retina_count), out,
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
