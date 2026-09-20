from __future__ import annotations

from collections import deque
from enum import Enum


class WorkMode(str, Enum):
    ACTIVE = "持续办公"
    CALM = "短暂空闲"
    SLEEPING = "休眠"
    FATIGUED = "疲惫"


class WorkStateTracker:
    def __init__(
        self,
        active_idle_limit: float = 60,
        sleep_idle_limit: float = 15 * 60,
        fatigue_limit: float = 50 * 60,
        sample_window: int = 5 * 60,
    ) -> None:
        self.active_idle_limit = active_idle_limit
        self.sleep_idle_limit = sleep_idle_limit
        self.fatigue_limit = fatigue_limit
        self.samples: deque[bool] = deque(maxlen=sample_window)
        self.focus_streak = 0.0
        self.mode = WorkMode.CALM

    @property
    def intensity(self) -> float:
        if not self.samples:
            return 0.0
        return sum(self.samples) / len(self.samples)

    def update(self, idle: float, elapsed: float, paused: bool = False) -> WorkMode:
        active = not paused and idle < self.active_idle_limit
        for _ in range(max(1, round(elapsed))):
            self.samples.append(active)

        if active:
            self.focus_streak += elapsed
        elif idle >= self.active_idle_limit or paused:
            self.focus_streak = 0.0

        if paused:
            self.mode = WorkMode.CALM
        elif idle >= self.sleep_idle_limit:
            self.mode = WorkMode.SLEEPING
        elif self.focus_streak >= self.fatigue_limit:
            self.mode = WorkMode.FATIGUED
        elif active:
            self.mode = WorkMode.ACTIVE
        else:
            self.mode = WorkMode.CALM
        return self.mode
