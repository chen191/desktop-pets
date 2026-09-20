from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any


def state_path() -> Path:
    base = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "SaplingDeskPet"
    base.mkdir(parents=True, exist_ok=True)
    return base / "state.json"


def load_state() -> dict[str, Any]:
    path = state_path()
    if not path.exists():
        return {"total_points": 0, "days": {}, "window": {}}
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        data.setdefault("total_points", 0)
        data.setdefault("days", {})
        data.setdefault("window", {})
        return data
    except (OSError, ValueError, TypeError):
        return {"total_points": 0, "days": {}, "window": {}}


def save_state(state: dict[str, Any]) -> None:
    path = state_path()
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")
    temp.replace(path)

