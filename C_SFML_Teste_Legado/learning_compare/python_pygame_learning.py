"""
Isolated Python/Pygame learning benchmark.

This file does not import AgentBioSim. It intentionally mirrors the C++/SFML
prototype in cpp_sfml_learning: same simple world, same visual sector sensor,
same MLP layout and same evolutionary selection rule.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict

import numpy as np
import pygame


WORLD_W = 960
WORLD_H = 720
CENTER = np.array([WORLD_W * 0.5, WORLD_H * 0.5], dtype=np.float32)
WORLD_R = 330.0
DT = 1.0 / 30.0
VISION_RADIUS = 145.0
VISION_FOV = math.radians(220.0)
AGENT_R = 5.5
FOOD_R = 3.2
MAX_SPEED = 78.0
TURN_RATE = 3.4
HIDDEN = 12
GENERATION_STEPS = 240


@dataclass
class Profile:
    sections: Dict[str, float] = field(default_factory=lambda: {
        "vision": 0.0,
        "brain": 0.0,
        "physics": 0.0,
        "interaction": 0.0,
        "evolution": 0.0,
        "render": 0.0,
    })

    def add(self, name: str, seconds: float) -> None:
        self.sections[name] = self.sections.get(name, 0.0) + seconds


class Timer:
    def __init__(self, profile: Profile, name: str):
        self.profile = profile
        self.name = name
        self.t0 = 0.0

    def __enter__(self):
        self.t0 = time.perf_counter()
        return self

    def __exit__(self, *_exc):
        self.profile.add(self.name, time.perf_counter() - self.t0)


def sigmoid(x: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-np.clip(x, -16.0, 16.0)))


class LearningWorld:
    def __init__(self, agents: int, foods: int, bins: int, seed: int):
        self.n = int(agents)
        self.f = int(foods)
        self.bins = int(bins)
        self.rng = np.random.default_rng(seed)
        self.profile = Profile()
        self.steps = 0
        self.generation = 0
        self.total_eats = 0

        self.pos = self._random_points(self.n, margin=18.0)
        angles = self.rng.uniform(0.0, math.tau, size=self.n).astype(np.float32)
        self.angle = angles
        self.energy = np.full(self.n, 50.0, dtype=np.float32)
        self.eats = np.zeros(self.n, dtype=np.float32)
        self.lifetime_eats = np.zeros(self.n, dtype=np.float32)
        self.food = self._random_points(self.f, margin=12.0)

        scale = 0.7
        self.w1 = self.rng.normal(0.0, scale, size=(self.n, self.bins, HIDDEN)).astype(np.float32)
        self.b1 = self.rng.normal(0.0, 0.15, size=(self.n, HIDDEN)).astype(np.float32)
        self.w2 = self.rng.normal(0.0, scale, size=(self.n, HIDDEN, 2)).astype(np.float32)
        self.b2 = self.rng.normal(0.0, 0.15, size=(self.n, 2)).astype(np.float32)
        self.last_inputs = np.zeros((self.n, self.bins), dtype=np.float32)

    def _random_points(self, count: int, margin: float) -> np.ndarray:
        a = self.rng.uniform(0.0, math.tau, size=count).astype(np.float32)
        r = np.sqrt(self.rng.uniform(0.0, 1.0, size=count)).astype(np.float32) * (WORLD_R - margin)
        return np.column_stack((CENTER[0] + np.cos(a) * r, CENTER[1] + np.sin(a) * r)).astype(np.float32)

    def vision(self) -> np.ndarray:
        dx = self.food[:, 0][None, :] - self.pos[:, 0][:, None]
        dy = self.food[:, 1][None, :] - self.pos[:, 1][:, None]
        d2 = dx * dx + dy * dy
        within_r = d2 < (VISION_RADIUS * VISION_RADIUS)
        rel = np.arctan2(dy, dx).astype(np.float32) - self.angle[:, None]
        rel = (rel + math.pi) % math.tau - math.pi
        half = VISION_FOV * 0.5
        mask = within_r & (np.abs(rel) <= half)

        inputs = np.zeros((self.n, self.bins), dtype=np.float32)
        if np.any(mask):
            dist = np.sqrt(np.maximum(d2[mask], 1e-6)).astype(np.float32)
            rel_valid = rel[mask]
            intensity = 1.0 - np.clip(dist / VISION_RADIUS, 0.0, 1.0)
            bin_idx = np.floor(((rel_valid + half) / VISION_FOV) * self.bins).astype(np.int32)
            bin_idx = np.clip(bin_idx, 0, self.bins - 1)
            agent_idx = np.nonzero(mask)[0].astype(np.int32)
            flat_idx = agent_idx * self.bins + bin_idx
            np.maximum.at(inputs.ravel(), flat_idx, intensity)
        self.last_inputs = inputs
        return inputs

    def brain(self, inputs: np.ndarray) -> np.ndarray:
        hidden = np.tanh(np.einsum("ni,nih->nh", inputs, self.w1, optimize=True) + self.b1)
        out = np.einsum("nh,nho->no", hidden, self.w2, optimize=True) + self.b2
        return out.astype(np.float32)

    def physics(self, outputs: np.ndarray) -> None:
        turn = np.tanh(outputs[:, 0]) * TURN_RATE
        speed = sigmoid(outputs[:, 1]) * MAX_SPEED
        self.angle = (self.angle + turn * DT).astype(np.float32)
        self.pos[:, 0] += np.cos(self.angle) * speed * DT
        self.pos[:, 1] += np.sin(self.angle) * speed * DT
        self.energy -= (0.016 + 0.00035 * speed) * DT

        delta = self.pos - CENTER
        dist = np.sqrt(np.sum(delta * delta, axis=1))
        outside = dist > (WORLD_R - AGENT_R)
        if np.any(outside):
            n = delta[outside] / np.maximum(dist[outside, None], 1e-6)
            self.pos[outside] = CENTER + n * (WORLD_R - AGENT_R)
            self.angle[outside] += math.pi * 0.82

    def interaction(self) -> None:
        dx = self.food[:, 0][None, :] - self.pos[:, 0][:, None]
        dy = self.food[:, 1][None, :] - self.pos[:, 1][:, None]
        d2 = dx * dx + dy * dy
        eat_mask = d2 < ((AGENT_R + FOOD_R) ** 2)
        food_hit = np.any(eat_mask, axis=0)
        if not np.any(food_hit):
            return
        hit_food_idx = np.nonzero(food_hit)[0]
        first_agent = np.argmax(eat_mask[:, hit_food_idx], axis=0)
        np.add.at(self.energy, first_agent, 16.0)
        np.add.at(self.eats, first_agent, 1.0)
        np.add.at(self.lifetime_eats, first_agent, 1.0)
        self.energy = np.minimum(self.energy, 120.0)
        self.total_eats += int(len(hit_food_idx))
        self.food[hit_food_idx] = self._random_points(len(hit_food_idx), margin=12.0)

    def evolve(self) -> None:
        dead = self.energy <= 0.0
        if np.any(dead):
            parents = self._sample_parents(int(np.sum(dead)))
            self._replace(np.nonzero(dead)[0], parents, reset_score=True)

        if self.steps > 0 and self.steps % GENERATION_STEPS == 0:
            self.generation += 1
            score = self.eats * 8.0 + self.energy * 0.04
            order = np.argsort(score)
            losers = order[: max(1, self.n // 3)]
            parents = self.rng.choice(order[-max(4, self.n // 5):], size=len(losers), replace=True)
            self._replace(losers, parents, reset_score=True)
            self.eats *= 0.25

    def _sample_parents(self, count: int) -> np.ndarray:
        score = self.eats * 8.0 + self.energy * 0.04 + 0.001
        elite_count = max(4, self.n // 5)
        elite = np.argsort(score)[-elite_count:]
        return self.rng.choice(elite, size=count, replace=True)

    def _replace(self, children: np.ndarray, parents: np.ndarray, reset_score: bool) -> None:
        self.w1[children] = self.w1[parents]
        self.b1[children] = self.b1[parents]
        self.w2[children] = self.w2[parents]
        self.b2[children] = self.b2[parents]

        mut = 0.08
        strength = 0.18
        for arr in (self.w1, self.b1, self.w2, self.b2):
            shape = arr[children].shape
            mask = self.rng.random(shape) < mut
            noise = self.rng.normal(0.0, strength, size=shape).astype(np.float32)
            arr[children] += mask * noise

        self.pos[children] = self._random_points(len(children), margin=18.0)
        self.angle[children] = self.rng.uniform(0.0, math.tau, size=len(children)).astype(np.float32)
        self.energy[children] = 50.0
        if reset_score:
            self.eats[children] = 0.0

    def step(self) -> None:
        with Timer(self.profile, "vision"):
            inputs = self.vision()
        with Timer(self.profile, "brain"):
            outputs = self.brain(inputs)
        with Timer(self.profile, "physics"):
            self.physics(outputs)
        with Timer(self.profile, "interaction"):
            self.interaction()
        with Timer(self.profile, "evolution"):
            self.evolve()
        self.steps += 1


def draw(world: LearningWorld, surface: pygame.Surface, font: pygame.font.Font | None, show_vision: bool) -> None:
    surface.fill((5, 9, 14))
    pygame.draw.circle(surface, (12, 24, 31), (int(CENTER[0]), int(CENTER[1])), int(WORLD_R))
    pygame.draw.circle(surface, (90, 210, 235), (int(CENTER[0]), int(CENTER[1])), int(WORLD_R), width=3)

    if show_vision:
        overlay = pygame.Surface(surface.get_size(), pygame.SRCALPHA)
        half = VISION_FOV * 0.5
        for i in range(0, world.n, max(1, world.n // 80)):
            a = world.angle[i]
            pts = [(world.pos[i, 0], world.pos[i, 1])]
            for k in range(18):
                t = k / 17.0
                ang = a - half + VISION_FOV * t
                pts.append((world.pos[i, 0] + math.cos(ang) * VISION_RADIUS,
                            world.pos[i, 1] + math.sin(ang) * VISION_RADIUS))
            pygame.draw.polygon(overlay, (70, 180, 255, 18), pts)
        surface.blit(overlay, (0, 0))

    for x, y in world.food:
        pygame.draw.circle(surface, (245, 190, 70), (int(x), int(y)), int(FOOD_R))

    for i in range(world.n):
        x, y = world.pos[i]
        a = float(world.angle[i])
        color = (82, 230, 155)
        pygame.draw.circle(surface, color, (int(x), int(y)), int(AGENT_R))
        hx = x + math.cos(a) * AGENT_R
        hy = y + math.sin(a) * AGENT_R
        pygame.draw.circle(surface, (4, 15, 12), (int(hx), int(hy)), 2)

    if font is not None:
        text = f"Python/Pygame | agentes {world.n} | comida {world.f} | gen {world.generation} | eats {world.total_eats}"
        img = font.render(text, True, (220, 240, 250))
        surface.blit(img, (16, 14))


def result_dict(engine: str, world: LearningWorld, wall: float, render_seconds: float, args: argparse.Namespace) -> dict:
    sections = dict(world.profile.sections)
    sections["render"] = render_seconds
    return {
        "engine": engine,
        "agents": world.n,
        "foods": world.f,
        "bins": world.bins,
        "steps": world.steps,
        "simulated_seconds": world.steps * DT,
        "wall_seconds": wall,
        "steps_per_wall_second": world.steps / max(wall, 1e-9),
        "total_eats": int(world.total_eats),
        "avg_lifetime_eats_per_agent": float(np.mean(world.lifetime_eats)),
        "generation": int(world.generation),
        "profile_seconds": sections,
        "profile_percent_wall": {k: 100.0 * v / max(wall, 1e-9) for k, v in sections.items()},
        "config": vars(args),
    }


def run_benchmark(args: argparse.Namespace) -> dict:
    os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
    pygame.init()
    surface = pygame.Surface((WORLD_W, WORLD_H))
    font = pygame.font.SysFont("Segoe UI", 18)
    world = LearningWorld(args.agents, args.foods, args.bins, args.seed)
    render_seconds = 0.0
    t0 = time.perf_counter()
    for _ in range(args.steps):
        world.step()
        rt0 = time.perf_counter()
        draw(world, surface, font, show_vision=args.show_vision)
        render_seconds += time.perf_counter() - rt0
    wall = time.perf_counter() - t0
    result = result_dict("python_pygame", world, wall, render_seconds, args)
    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output).write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, indent=2))
    return result


def run_visual(args: argparse.Namespace) -> None:
    pygame.init()
    screen = pygame.display.set_mode((WORLD_W, WORLD_H))
    pygame.display.set_caption("Learning Compare - Python/Pygame")
    font = pygame.font.SysFont("Segoe UI", 18)
    clock = pygame.time.Clock()
    world = LearningWorld(args.agents, args.foods, args.bins, args.seed)
    show_vision = args.show_vision
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_v:
                    show_vision = not show_vision
        for _ in range(max(1, args.steps_per_frame)):
            world.step()
        rt0 = time.perf_counter()
        draw(world, screen, font, show_vision=show_vision)
        world.profile.add("render", time.perf_counter() - rt0)
        pygame.display.flip()
        clock.tick(60)
    pygame.quit()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--benchmark", action="store_true")
    parser.add_argument("--visual", action="store_true")
    parser.add_argument("--agents", type=int, default=600)
    parser.add_argument("--foods", type=int, default=300)
    parser.add_argument("--bins", type=int, default=16)
    parser.add_argument("--steps", type=int, default=1200)
    parser.add_argument("--steps-per-frame", type=int, default=1)
    parser.add_argument("--seed", type=int, default=123)
    parser.add_argument("--show-vision", action="store_true")
    parser.add_argument("--output", default="")
    args = parser.parse_args()
    if not args.visual and not args.benchmark and len(os.sys.argv) <= 1:
        args.visual = True
    if args.visual and not args.benchmark:
        run_visual(args)
    else:
        run_benchmark(args)


if __name__ == "__main__":
    main()
