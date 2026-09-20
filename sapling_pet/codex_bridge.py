from __future__ import annotations

import ctypes
import json
import re
from ctypes import wintypes
from dataclasses import dataclass
from pathlib import Path


GLOBAL_STATE_PATH = Path.home() / ".codex" / ".codex-global-state.json"
SESSION_INDEX_PATH = Path.home() / ".codex" / "session_index.jsonl"
SESSIONS_PATH = Path.home() / ".codex" / "sessions"
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
SW_RESTORE = 9


@dataclass(frozen=True)
class CodexNotification:
    thread_id: str
    title: str
    summary: str


def unread_thread_ids() -> set[str]:
    """读取 Codex 本地未读任务标识；文件暂时被写入时返回空集合。"""
    try:
        data = json.loads(GLOBAL_STATE_PATH.read_text(encoding="utf-8-sig"))
        atom_state = data.get("electron-persisted-atom-state", {})
        by_host = atom_state.get("unread-thread-ids-by-host-v1", {})
        result: set[str] = set()
        if isinstance(by_host, dict):
            for ids in by_host.values():
                if isinstance(ids, list):
                    result.update(str(item) for item in ids)
        return result
    except (OSError, ValueError, TypeError):
        return set()


def _thread_title(thread_id: str) -> str:
    title = "Codex 任务"
    try:
        for line in SESSION_INDEX_PATH.read_text(encoding="utf-8-sig").splitlines():
            item = json.loads(line)
            if item.get("id") == thread_id and item.get("thread_name"):
                title = str(item["thread_name"])
    except (OSError, ValueError, TypeError):
        pass
    return title


def _plain_summary(text: str, limit: int = 150) -> str:
    text = re.sub(r"[`*_#]", "", text)
    text = re.sub(r"(?m)^\s*[-•]\s*", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text if len(text) <= limit else text[: limit - 1].rstrip() + "…"


def _latest_final_answer(thread_id: str) -> str:
    try:
        candidates = list(SESSIONS_PATH.rglob(f"*{thread_id}.jsonl"))
        if not candidates:
            return "任务有新的进展，点击我返回 Codex 查看。"
        path = max(candidates, key=lambda item: item.stat().st_mtime)
        lines = path.read_text(encoding="utf-8-sig", errors="ignore").splitlines()
        for line in reversed(lines):
            item = json.loads(line)
            payload = item.get("payload", {})
            if (
                item.get("type") == "response_item"
                and payload.get("type") == "message"
                and payload.get("role") == "assistant"
                and payload.get("phase") == "final_answer"
            ):
                content = payload.get("content", [])
                text = " ".join(
                    str(part.get("text", ""))
                    for part in content
                    if isinstance(part, dict) and part.get("text")
                )
                if text:
                    return _plain_summary(text)
    except (OSError, ValueError, TypeError):
        pass
    return "任务有新的进展，点击我返回 Codex 查看。"


def notification_for_thread(thread_id: str) -> CodexNotification:
    return CodexNotification(
        thread_id=thread_id,
        title=_thread_title(thread_id),
        summary=_latest_final_answer(thread_id),
    )


def _process_path(process_id: int) -> str:
    kernel32 = ctypes.windll.kernel32
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, process_id)
    if not handle:
        return ""
    try:
        size = wintypes.DWORD(1024)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            return ""
        return buffer.value.lower()
    finally:
        kernel32.CloseHandle(handle)


def bring_codex_to_front() -> bool:
    """找到最大的 Codex 主窗口并将其恢复到前台。"""
    user32 = ctypes.windll.user32
    candidates: list[tuple[int, int]] = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def enum_callback(hwnd: int, _lparam: int) -> bool:
        if not user32.IsWindowVisible(hwnd):
            return True
        process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(process_id))
        process_path = _process_path(process_id.value)
        process_name = Path(process_path).name
        is_codex_window = process_name == "codex.exe" or (
            process_name == "chatgpt.exe" and "openai.codex_" in process_path
        )
        if not is_codex_window:
            return True
        rect = wintypes.RECT()
        if user32.GetWindowRect(hwnd, ctypes.byref(rect)):
            area = max(0, rect.right - rect.left) * max(0, rect.bottom - rect.top)
            if area >= 200_000:
                candidates.append((area, hwnd))
        return True

    user32.EnumWindows(enum_callback, 0)
    if not candidates:
        return False
    _area, hwnd = max(candidates)
    user32.ShowWindow(hwnd, SW_RESTORE)
    user32.SetForegroundWindow(hwnd)
    return True
