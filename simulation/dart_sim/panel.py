"""控制台面板（Tk）。滑条拖动过程中实时下发；自动流程改了目标时，滑条也跟着显示当前目标。

所有操作都通过 runner.cmd 投递到仿真线程执行。
"""
import math
import tkinter as tk
from tkinter import ttk

from .panel_text import render

FONT = ("Microsoft YaHei", 10)
MONO = ("NSimSun", 11)     # 新宋体：中文正好两个西文字符宽，表格能对齐


class Panel:
    def __init__(self, root, runner, set_view):
        self.root, self.r, self.set_view = root, runner, set_view
        self.scales, self._echo, self._drag = {}, {}, set()
        sim, p = runner.sim, runner.sim.plant.p
        root.title("飞镖发射架仿真控制台")
        root.option_add("*Font", FONT)
        left, right = ttk.Frame(root, padding=8), ttk.Frame(root, padding=8)
        left.grid(row=0, column=0, sticky="n")
        right.grid(row=0, column=1, sticky="n")
        cmd = lambda f: (lambda: self.r.cmd(f))

        box = self._box(left, "自动流程")
        buttons = [("上膛", lambda s: s.seq.cmd_cock()), ("装填", lambda s: s.seq.cmd_load()),
                   ("发射", lambda s: s.seq.cmd_fire()), ("单发全流程", lambda s: s.seq.cmd_salvo(1)),
                   ("四发连发", lambda s: s.seq.cmd_salvo(4)), ("急停", lambda s: s.stop()), ("复位", lambda s: s.reset())]
        for k, (text, fn) in enumerate(buttons):
            ttk.Button(box, text=text, command=cmd(fn), width=11).grid(row=k // 4, column=k % 4, padx=2, pady=2)

        box = self._box(left, "瞄准（拖动即生效，自动流程中也能调）")
        lo, hi = (math.degrees(a) for a in sim.ctrl.yaw_range)
        self._scale(box, 0, "yaw", "yaw 偏航角 (°)", lo, hi, lambda s, v: s.ctrl.set_yaw(math.radians(v)), manual=False)
        plo, phi = (x * 1e3 for x in p.pitch_range)
        self._scale(box, 1, "pitch", "pitch 滑块 (mm)", plo, phi, lambda s, v: s.ctrl.set_pitch(v * 1e-3), manual=False)
        ttk.Label(box, text="pitch 往下（负值）= 皮筋拉得更长 = 射得更远", foreground="#666").grid(row=2, column=0, columnspan=2, sticky="w")

        box = self._box(left, "手动（拖动即生效，会打断自动流程）")
        self._scale(box, 0, "slider", "3508 滑块 (mm)", -330, p.slider_top * 1e3, lambda s, v: s.ctrl.set_slider(v * 1e-3))
        self._scale(box, 1, "lock", "锁止摇臂 (°)", 0, p.lock_open_deg, self._servo("lock"))
        self._scale(box, 2, "arm", "摆臂 (°)", *p.arm_range_deg, self._servo("arm"))
        self._scale(box, 3, "crank", "曲柄 (°)", 0, 180, self._servo("crank"))
        row = ttk.Frame(box)
        row.grid(row=4, column=0, columnspan=2, sticky="w")
        ttk.Button(row, text="磁铁吸合", command=cmd(lambda s: s.darts.grip())).pack(side="left", padx=2)
        ttk.Button(row, text="磁铁释放", command=cmd(lambda s: s.darts.release())).pack(side="left", padx=2)
        self.darts_on = tk.BooleanVar(value=p.darts_enabled)
        ttk.Checkbutton(row, text="放飞镖（会复位）", variable=self.darts_on,
                        command=lambda: self.r.cmd(lambda s, on=self.darts_on.get(): s.set_darts(on))).pack(side="left", padx=10)

        box = self._box(left, "皮筋（整根）")
        self.band = {}
        for k, (key, text, val) in enumerate([("k", "刚度 N/m", p.band_stiffness), ("l0", "自然长度 mm", p.band_rest_length * 1e3),
                                              ("c", "阻尼 N·s/m", p.band_damping)]):
            ttk.Label(box, text=text).grid(row=k, column=0, sticky="w")
            self.band[key] = tk.StringVar(value=f"{val:g}")
            ttk.Entry(box, textvariable=self.band[key], width=10).grid(row=k, column=1, sticky="w")
        ttk.Button(box, text="应用", command=self._apply_band).grid(row=3, column=1, sticky="w", pady=2)

        box = self._box(left, "仿真")
        self.pause_text = tk.StringVar(value="暂停")
        ttk.Button(box, textvariable=self.pause_text, command=self._toggle_pause, width=8).grid(row=0, column=0, padx=2)
        self.speed = tk.StringVar(value="1x")
        ttk.Combobox(box, textvariable=self.speed, values=["0.05x", "0.1x", "0.25x", "0.5x", "1x"], width=6,
                     state="readonly").grid(row=0, column=1, padx=2)
        self.speed.trace_add("write", lambda *_: setattr(self.r, "speed", float(self.speed.get()[:-1])))
        self.follow = tk.BooleanVar(value=False)
        ttk.Checkbutton(box, text="镜头跟随飞镖", variable=self.follow,
                        command=lambda: setattr(self.r, "follow", self.follow.get())).grid(row=0, column=2)
        for k, name in enumerate(["近景", "全景", "侧面"]):
            ttk.Button(box, text=name, width=6, command=lambda n=name: self.r.cmd(lambda s: self.set_view(self.r.viewer.cam, n))
                       ).grid(row=1, column=k, padx=2, pady=2)

        self.text = tk.Text(right, width=64, height=40, font=MONO, relief="flat", background="#f6f7f9")
        self.text.pack()
        self._tick()

    def _box(self, parent, title):
        f = ttk.LabelFrame(parent, text=title, padding=6)
        f.pack(fill="x", pady=4)
        return f

    def _scale(self, parent, row, key, text, lo, hi, apply, manual=True):
        """apply(sim, 值) 在仿真线程里执行；拖动过程中每变一次就下发一次。"""
        ttk.Label(parent, text=text).grid(row=row, column=0, sticky="w")
        init = self.r.snap["targets"].get(key)
        init = round(lo if init is None else min(max(init, lo), hi), 1)
        self._echo[key] = init               # 建控件时 Tk 可能回调一次初值，不能当成用户操作下发
        var = tk.DoubleVar(value=init)
        sc = tk.Scale(parent, from_=lo, to=hi, resolution=0.1, orient="horizontal", length=280, variable=var,
                      command=lambda v: self._on_scale(key, float(v)))
        sc.grid(row=row, column=1)
        sc.bind("<ButtonPress-1>", lambda e: self._drag.add(key), add="+")
        sc.bind("<ButtonRelease-1>", lambda e: self._drag.discard(key), add="+")
        self.scales[key] = (var, apply, manual)

    def _on_scale(self, key, v):
        if abs(v - self._echo.get(key, math.nan)) < 0.05:     # 是程序同步过来的值，不是人拖的
            return
        self._echo[key] = v
        _, apply, manual = self.scales[key]

        def run(s):
            if manual:
                s.seq.cancel()
            apply(s, v)
        self.r.cmd(run)

    def _servo(self, name):
        return lambda s, v: setattr(s.plant.servos[name], "target", math.radians(v))

    def _sync_scales(self, targets):
        for key, (var, _, _) in self.scales.items():
            t = targets.get(key)
            if t is not None and key not in self._drag and abs(var.get() - t) > 0.05:
                self._echo[key] = round(t, 1)
                var.set(round(t, 1))

    def _apply_band(self):
        try:
            k, l0, c = (float(self.band[x].get()) for x in ("k", "l0", "c"))
        except ValueError:
            return

        def apply(s):
            s.plant.p.band_stiffness, s.plant.p.band_rest_length, s.plant.p.band_damping = k, l0 * 1e-3, c
            s.plant.apply_params()
        self.r.cmd(apply)

    def _toggle_pause(self):
        self.r.paused = not self.r.paused
        self.pause_text.set("继续" if self.r.paused else "暂停")

    def _tick(self):
        if not self.r.alive:
            self.root.destroy()
            return
        snap = self.r.snap
        self._sync_scales(snap["targets"])
        self.text.delete("1.0", "end")
        self.text.insert("end", render(snap))
        self.root.after(100, self._tick)
