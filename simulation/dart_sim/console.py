"""中文控制台：Tk 面板 + MuJoCo 三维视图。仿真在后台线程里按实时推进，面板的操作投递到该线程执行。"""
import queue
import threading
import time
import traceback

import mujoco

from .sim import Sim


class Runner(threading.Thread):
    FRAME = 1 / 60

    def __init__(self, sim, viewer):
        super().__init__(daemon=True)
        self.sim, self.viewer = sim, viewer
        self.inbox = queue.Queue()
        self.alive, self.paused, self.speed, self.follow = True, False, 1.0, False
        self.snap = sim.snapshot()
        self.error = None
        self.rtf, self._rtf_mark = 1.0, None

    def cmd(self, fn):
        """fn(sim) 在仿真线程里执行。"""
        self.inbox.put(fn)

    def frame(self):
        with self.viewer.lock():
            while not self.inbox.empty():
                self.inbox.get_nowait()(self.sim)
            if not self.paused:
                self.sim.run(self.FRAME * self.speed)
            if self.follow:
                self._follow()
            self.snap = self.sim.snapshot()
        self._measure()
        self.snap["rtf"] = self.rtf
        self.viewer.sync()

    def _measure(self):
        """实时倍率 = 仿真时间 / 墙钟时间（每秒更新）。"""
        now = time.perf_counter()
        if self._rtf_mark is None or self.paused:
            self._rtf_mark = (now, self.sim.t)
            return
        t0, s0 = self._rtf_mark
        if now - t0 >= 1.0:
            self.rtf = (self.sim.t - s0) / (now - t0)
            self._rtf_mark = (now, self.sim.t)

    def run(self):
        next_t = time.perf_counter()
        try:
            while self.alive and self.viewer.is_running():
                self.frame()
                next_t += self.FRAME
                delay = next_t - time.perf_counter()
                if delay > 0:
                    time.sleep(delay)
                else:
                    next_t = time.perf_counter()
        except Exception:
            self.error = traceback.format_exc()
        self.alive = False

    def _follow(self):
        d = self.sim.darts
        i = next((k for k in range(d.n) if d.state[k] in ("flying", "free")), None)
        target = self.sim.plant.d.xpos[d.body[i]] if i is not None else self.sim.plant.d.xpos[self.sim.plant.m.body("frame").id]
        self.viewer.cam.lookat[:] = target


def set_view(cam, name):
    views = {"近景": ((0.02, 0.25, 0.45), 1.3, -135, -20), "全景": ((0.0, 6.0, 1.0), 13.0, -150, -14),
             "侧面": ((0.0, 0.3, 0.4), 1.9, 180, -8)}
    look, dist, az, el = views[name]
    cam.type = mujoco.mjtCamera.mjCAMERA_FREE
    cam.lookat[:], cam.distance, cam.azimuth, cam.elevation = look, dist, az, el


def run_console(p=None):
    import tkinter as tk
    import mujoco.viewer
    from .panel import Panel

    sim = Sim(p)
    viewer = mujoco.viewer.launch_passive(sim.plant.m, sim.plant.d, show_left_ui=False, show_right_ui=False)
    with viewer.lock():
        set_view(viewer.cam, "近景")
    runner = Runner(sim, viewer)
    runner.start()
    root = tk.Tk()
    Panel(root, runner, set_view)

    def on_close():
        runner.alive = False
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    root.mainloop()
    runner.alive = False
    runner.join(timeout=2)
    viewer.close()
    if runner.error:
        print(runner.error)
