from __future__ import annotations

from dataclasses import dataclass
from datetime import date


SECONDS_PER_POINT = 25 * 60
DAILY_POINT_CAP = 12
STAGES = (
    (0, "破土嫩芽", 0.31),
    (10, "小树苗", 0.36),
    (25, "青年树", 0.44),
    (50, "成熟树", 0.53),
)


def earned_points(active_seconds: float) -> int:
    return min(DAILY_POINT_CAP, max(0, int(active_seconds // SECONDS_PER_POINT)))


def stage_for(points: int) -> tuple[str, float]:
    result = STAGES[0][1:]
    for threshold, name, scale in STAGES:
        if points < threshold:
            break
        result = (name, scale)
    return result


def stage_index_for(points: int) -> int:
    result = 0
    for index, (threshold, _name, _scale) in enumerate(STAGES):
        if points < threshold:
            break
        result = index
    return result


@dataclass
class DailyRecord:
    day: str
    active_seconds: float = 0.0
    awarded_points: int = 0

    @classmethod
    def today(cls) -> "DailyRecord":
        return cls(day=date.today().isoformat())
