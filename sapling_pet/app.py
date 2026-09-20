from __future__ import annotations

import math
import time
import tkinter as tk
from datetime import date
from pathlib import Path

from PIL import Image, ImageEnhance, ImageTk

from .activity import idle_seconds
from .codex_bridge import bring_codex_to_front, notification_for_thread, unread_thread_ids
from .growth import earned_points, stage_for, stage_index_for
from .storage import load_state, save_state
from .work_state import WorkMode, WorkStateTracker


WINDOW_SIZE = 300
SAVE_INTERVAL = 30


def harden_alpha(image: Image.Image, threshold: int = 112) -> Image.Image:
    """避免 Tk 色键透明窗口把半透明边缘与洋红底色混合成红边。"""
    image = image.convert("RGBA")
    alpha = image.getchannel("A").point(lambda value: 255 if value >= threshold else 0)
    image.putalpha(alpha)
    return image


class SaplingPet:
    def __init__(self, root: tk.Tk, assets_dir: Path) -> None:
        self.root = root
        self.state = load_state()
        self.paused = False
        self.topmost = True
        self.drag_origin: tuple[int, int] | None = None
        self.last_tick = time.monotonic()
        self.last_save = self.last_tick
        self.break_notified = False
        self.bubble_window: tk.Toplevel | None = None
        self.click_job: str | None = None
        self.suppress_click_release = False
        self.press_position: tuple[int, int] | None = None
        self.drag_moved = False
        self.codex_unread_ids = unread_thread_ids()
        self.is_active = False
        self.work_tracker = WorkStateTracker()
        self.work_mode = WorkMode.CALM
        self.animation_phase = 0.0
        self.sprite_image: Image.Image | None = None
        self.base_image: Image.Image | None = None
        self.sprite_item: int | None = None
        self.base_item: int | None = None
        self.rendered_stage: tuple[str, float] | None = None
        self.source_trees = [
            Image.open(assets_dir / f"stage_{index}_tree.png").convert("RGBA")
            for index in range(2, 6)
        ]
        self.source_bases = [
            Image.open(assets_dir / f"stage_{index}_base.png").convert("RGBA")
            for index in range(2, 6)
        ]
        self.photo: ImageTk.PhotoImage | None = None
        self.base_photo: ImageTk.PhotoImage | None = None

        self._ensure_today()
        self._configure_window()
        self._build_ui()
        self._restore_position()
        self._render()
        self.root.after(80, self._animate)
        self.root.after(1000, self._tick)
        self.root.after(5000, self._poll_codex)

    @property
    def today_key(self) -> str:
        return date.today().isoformat()

    @property
    def today(self) -> dict:
        return self.state["days"][self.today_key]

    def _ensure_today(self) -> None:
        today = self.today_key
        days = self.state["days"]
        for day, record in days.items():
            if day != today and not record.get("settled", False):
                points = earned_points(float(record.get("active_seconds", 0)))
                self.state["total_points"] += points
                record["awarded_points"] = points
                record["settled"] = True
        days.setdefault(today, {"active_seconds": 0.0, "awarded_points": 0, "settled": False})

    def _configure_window(self) -> None:
        self.root.title("小树苗桌宠")
        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)
        self.root.configure(bg="#ff00ff")
        self.root.wm_attributes("-transparentcolor", "#ff00ff")
        self.root.geometry(f"{WINDOW_SIZE}x{WINDOW_SIZE}")
        self.root.protocol("WM_DELETE_WINDOW", self.close)

    def _build_ui(self) -> None:
        self.canvas = tk.Canvas(
            self.root,
            width=WINDOW_SIZE,
            height=WINDOW_SIZE,
            bg="#ff00ff",
            highlightthickness=0,
            cursor="hand2",
        )
        self.canvas.pack()
        self.canvas.bind("<ButtonPress-1>", self._drag_start)
        self.canvas.bind("<B1-Motion>", self._drag_move)
        self.canvas.bind("<ButtonRelease-1>", self._click_release)
        self.canvas.bind("<Button-3>", self._show_menu)
        self.canvas.bind("<Double-Button-1>", self._double_click)

        self.menu = tk.Menu(self.root, tearoff=False)
        self.menu.add_command(label="查看今日成长", command=self.show_stats)
        self.menu.add_command(label="暂停监测", command=self.toggle_pause)
        self.menu.add_command(label="取消置顶", command=self.toggle_topmost)
        self.menu.add_command(label="打开 Codex", command=self.open_codex)
        self.menu.add_separator()
        self.menu.add_command(label="退出并保存", command=self.close)

    def _restore_position(self) -> None:
        window = self.state.get("window", {})
        x = window.get("x")
        y = window.get("y")
        if isinstance(x, int) and isinstance(y, int):
            self.root.geometry(f"+{x}+{y}")
        else:
            x = max(0, self.root.winfo_screenwidth() - WINDOW_SIZE - 30)
            y = max(0, self.root.winfo_screenheight() - WINDOW_SIZE - 90)
            self.root.geometry(f"+{x}+{y}")

    def _preview_points(self) -> int:
        return self.state["total_points"] + earned_points(float(self.today["active_seconds"]))

    def _render(self) -> None:
        stage = stage_for(self._preview_points())
        _name, scale = stage
        stage_index = stage_index_for(self._preview_points())
        source_tree = self.source_trees[stage_index]
        source_base = self.source_bases[stage_index]
        size = int(275 * scale)
        width, height = source_tree.size
        factor = size / max(width, height)
        target = (max(1, int(width * factor)), max(1, int(height * factor)))
        self.sprite_image = harden_alpha(source_tree.resize(target, Image.Resampling.LANCZOS))
        self.base_image = harden_alpha(source_base.resize(target, Image.Resampling.LANCZOS))
        self.rendered_stage = stage
        if self.sprite_item is None:
            self.photo = ImageTk.PhotoImage(self.sprite_image)
            self.sprite_item = self.canvas.create_image(
                WINDOW_SIZE // 2, WINDOW_SIZE - 7, image=self.photo, anchor="s"
            )
            self.base_photo = ImageTk.PhotoImage(self.base_image)
            self.base_item = self.canvas.create_image(
                WINDOW_SIZE // 2, WINDOW_SIZE - 7, image=self.base_photo, anchor="s"
            )
        elif self.base_item is not None:
            self.base_photo = ImageTk.PhotoImage(self.base_image)
            self.canvas.itemconfigure(self.base_item, image=self.base_photo)

    def _animate(self) -> None:
        if self.sprite_image is not None and self.sprite_item is not None:
            if self.work_mode == WorkMode.ACTIVE:
                self.animation_phase += 0.11 + self.work_tracker.intensity * 0.05
                sway = 0.7 + self.work_tracker.intensity * 0.9
                brightness = 1.0
            elif self.work_mode == WorkMode.FATIGUED:
                self.animation_phase += 0.045
                sway = 0.35
                brightness = 0.84
            elif self.work_mode == WorkMode.SLEEPING:
                self.animation_phase += 0.025
                sway = 0.12
                brightness = 0.72
            else:
                sway = 0.0
                brightness = 0.96
            angle = math.sin(self.animation_phase) * sway
            pivot_y = int(self.sprite_image.height * 0.77)
            frame = ImageEnhance.Brightness(self.sprite_image).enhance(brightness).rotate(
                angle,
                resample=Image.Resampling.BICUBIC,
                expand=False,
                center=(self.sprite_image.width // 2, pivot_y),
            )
            frame = harden_alpha(frame)
            self.photo = ImageTk.PhotoImage(frame)
            self.canvas.itemconfigure(self.sprite_item, image=self.photo)
        self.root.after(80, self._animate)

    def _tick(self) -> None:
        now = time.monotonic()
        elapsed = min(now - self.last_tick, 5.0)
        self.last_tick = now
        self._ensure_today()

        idle = idle_seconds()
        previous_mode = self.work_mode
        self.work_mode = self.work_tracker.update(idle, elapsed, self.paused)
        self.is_active = self.work_mode in (WorkMode.ACTIVE, WorkMode.FATIGUED)
        if self.is_active:
            before = earned_points(float(self.today["active_seconds"]))
            self.today["active_seconds"] += elapsed
            after = earned_points(float(self.today["active_seconds"]))
            if after != before:
                self._render()
        if self.work_mode != WorkMode.FATIGUED:
            self.break_notified = False

        if self.work_mode == WorkMode.FATIGUED and not self.break_notified:
            self.break_notified = True
            self.show_bubble("我们已经专注很久啦！\n起来喝口水、活动一下，再继续长大吧。", 15000)

        if previous_mode != self.work_mode:
            self._render()

        if now - self.last_save >= SAVE_INTERVAL:
            self._save()
            self.last_save = now
        self.root.after(1000, self._tick)

    def _drag_start(self, event: tk.Event) -> None:
        self._close_bubble()
        self.press_position = (event.x_root, event.y_root)
        self.drag_moved = False
        self.drag_origin = (event.x_root - self.root.winfo_x(), event.y_root - self.root.winfo_y())

    def _drag_move(self, event: tk.Event) -> None:
        if self.drag_origin:
            if self.press_position:
                dx = abs(event.x_root - self.press_position[0])
                dy = abs(event.y_root - self.press_position[1])
                self.drag_moved = self.drag_moved or dx + dy > 5
            x = event.x_root - self.drag_origin[0]
            y = event.y_root - self.drag_origin[1]
            self.root.geometry(f"+{x}+{y}")

    def _click_release(self, _event: tk.Event) -> None:
        if self.suppress_click_release:
            self.suppress_click_release = False
            return
        if not self.drag_moved:
            if self.click_job is not None:
                self.root.after_cancel(self.click_job)
            self.click_job = self.root.after(280, self.show_stats)

    def _double_click(self, _event: tk.Event) -> None:
        self.suppress_click_release = True
        if self.click_job is not None:
            self.root.after_cancel(self.click_job)
            self.click_job = None
        self.open_codex()

    def _show_menu(self, event: tk.Event) -> None:
        self.menu.tk_popup(event.x_root, event.y_root)

    def toggle_pause(self) -> None:
        self.paused = not self.paused
        self.menu.entryconfigure(1, label="恢复监测" if self.paused else "暂停监测")
        self._render()

    def toggle_topmost(self) -> None:
        self.topmost = not self.topmost
        self.root.attributes("-topmost", self.topmost)
        self.menu.entryconfigure(2, label="取消置顶" if self.topmost else "保持置顶")

    def open_codex(self) -> None:
        self.click_job = None
        if not bring_codex_to_front():
            self.show_bubble("我没有找到正在运行的 Codex。\n先打开 Codex，我就能带你回到工作现场啦。")

    def _poll_codex(self) -> None:
        current = unread_thread_ids()
        new_updates = current - self.codex_unread_ids
        self.codex_unread_ids = current
        if new_updates:
            notifications = [notification_for_thread(thread_id) for thread_id in new_updates]
            latest = notifications[-1]
            if len(notifications) == 1:
                message = f"{latest.title}\n{latest.summary}"
            else:
                message = (
                    f"Codex 有 {len(notifications)} 个任务更新。\n"
                    f"最新：{latest.title}\n{latest.summary}"
                )
            self.show_bubble(message, 18000)
        self.root.after(5000, self._poll_codex)

    def show_stats(self) -> None:
        seconds = int(self.today["active_seconds"])
        hours, remainder = divmod(seconds, 3600)
        minutes = remainder // 60
        today_points = earned_points(seconds)
        stage, _ = stage_for(self._preview_points())
        self.show_bubble(
            f"今天我们一起专注了 {hours} 小时 {minutes} 分钟！\n"
            f"我获得了 {today_points} 点成长，现在是{stage}。\n"
            f"最近的办公强度是 {self.work_tracker.intensity:.0%}，"
            f"我正在{self.work_mode.value}。",
            15000,
        )

    def _rounded_box(
        self, canvas: tk.Canvas, x1: int, y1: int, x2: int, y2: int, radius: int
    ) -> None:
        points = [
            x1 + radius, y1, x2 - radius, y1, x2, y1, x2, y1 + radius,
            x2, y2 - radius, x2, y2, x2 - radius, y2, x1 + radius, y2,
            x1, y2, x1, y2 - radius, x1, y1 + radius, x1, y1,
        ]
        canvas.create_polygon(
            points,
            smooth=True,
            splinesteps=24,
            fill="#fffdf5",
            outline="#78a85f",
            width=2,
        )

    def show_bubble(self, message: str, duration_ms: int = 12000) -> None:
        self._close_bubble()
        width, height = 300, 154
        pet_center_x = self.root.winfo_x() + WINDOW_SIZE // 2
        show_left = pet_center_x >= width + 20
        x = pet_center_x - width - 8 if show_left else pet_center_x + 8
        y = self.root.winfo_y() + 72
        y = max(8, min(y, self.root.winfo_screenheight() - height - 48))

        bubble = tk.Toplevel(self.root)
        self.bubble_window = bubble
        bubble.overrideredirect(True)
        bubble.attributes("-topmost", True)
        bubble.configure(bg="#ff00ff")
        bubble.wm_attributes("-transparentcolor", "#ff00ff")
        bubble.geometry(f"{width}x{height}+{x}+{y}")

        canvas = tk.Canvas(
            bubble,
            width=width,
            height=height,
            bg="#ff00ff",
            highlightthickness=0,
            cursor="hand2",
        )
        canvas.pack()
        if show_left:
            self._rounded_box(canvas, 5, 5, width - 20, height - 10, 18)
            canvas.create_polygon(
                width - 21, 96, width - 1, 112, width - 21, 124,
                fill="#fffdf5", outline="#78a85f", width=2,
            )
            text_x = 20
            close_x = width - 38
        else:
            self._rounded_box(canvas, 20, 5, width - 5, height - 10, 18)
            canvas.create_polygon(
                21, 96, 1, 112, 21, 124,
                fill="#fffdf5", outline="#78a85f", width=2,
            )
            text_x = 36
            close_x = width - 22
        canvas.create_text(
            text_x,
            18,
            anchor="nw",
            text="小树苗对你说",
            fill="#527d42",
            font=("Microsoft YaHei UI", 10, "bold"),
        )
        canvas.create_text(
            text_x,
            47,
            anchor="nw",
            width=240,
            text=message,
            fill="#3f493c",
            font=("Microsoft YaHei UI", 9),
        )
        canvas.create_text(
            close_x,
            18,
            text="×",
            fill="#86a57c",
            font=("Microsoft YaHei UI", 12, "bold"),
        )
        canvas.bind("<Button-1>", lambda _event: self._close_bubble())
        bubble.after(duration_ms, self._close_bubble)

    def _close_bubble(self) -> None:
        if self.bubble_window is not None:
            try:
                self.bubble_window.destroy()
            except tk.TclError:
                pass
            self.bubble_window = None

    def _save(self) -> None:
        self.state["window"] = {"x": self.root.winfo_x(), "y": self.root.winfo_y()}
        save_state(self.state)

    def close(self) -> None:
        self._close_bubble()
        self._save()
        self.root.destroy()


def run(assets_dir: Path) -> None:
    root = tk.Tk()
    SaplingPet(root, assets_dir)
    root.mainloop()
