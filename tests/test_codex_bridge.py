import json
import tempfile
import unittest
from pathlib import Path

import sapling_pet.codex_bridge as bridge


class CodexBridgeTests(unittest.TestCase):
    def test_unread_thread_ids_are_merged_across_hosts(self):
        original = bridge.GLOBAL_STATE_PATH
        try:
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "state.json"
                path.write_text(
                    json.dumps(
                        {
                            "electron-persisted-atom-state": {
                                "unread-thread-ids-by-host-v1": {
                                    "local": ["task-a"],
                                    "cloud": ["task-b", "task-a"],
                                }
                            }
                        }
                    ),
                    encoding="utf-8",
                )
                bridge.GLOBAL_STATE_PATH = path
                self.assertEqual(bridge.unread_thread_ids(), {"task-a", "task-b"})
        finally:
            bridge.GLOBAL_STATE_PATH = original

    def test_invalid_state_returns_empty_set(self):
        original = bridge.GLOBAL_STATE_PATH
        try:
            bridge.GLOBAL_STATE_PATH = Path("missing-codex-state.json")
            self.assertEqual(bridge.unread_thread_ids(), set())
        finally:
            bridge.GLOBAL_STATE_PATH = original

    def test_notification_reads_title_and_latest_final_answer(self):
        old_index = bridge.SESSION_INDEX_PATH
        old_sessions = bridge.SESSIONS_PATH
        try:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                bridge.SESSION_INDEX_PATH = root / "session_index.jsonl"
                bridge.SESSIONS_PATH = root / "sessions"
                bridge.SESSIONS_PATH.mkdir()
                bridge.SESSION_INDEX_PATH.write_text(
                    json.dumps({"id": "task-a", "thread_name": "测试项目_master"}, ensure_ascii=False),
                    encoding="utf-8",
                )
                session = bridge.SESSIONS_PATH / "rollout-task-a.jsonl"
                session.write_text(
                    json.dumps(
                        {
                            "type": "response_item",
                            "payload": {
                                "type": "message",
                                "role": "assistant",
                                "phase": "final_answer",
                                "content": [{"type": "output_text", "text": "已完成：\n- **功能一**\n- `功能二`"}],
                            },
                        },
                        ensure_ascii=False,
                    ),
                    encoding="utf-8",
                )
                notice = bridge.notification_for_thread("task-a")
                self.assertEqual(notice.title, "测试项目_master")
                self.assertEqual(notice.summary, "已完成： 功能一 功能二")
        finally:
            bridge.SESSION_INDEX_PATH = old_index
            bridge.SESSIONS_PATH = old_sessions


if __name__ == "__main__":
    unittest.main()
