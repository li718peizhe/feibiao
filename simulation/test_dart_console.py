"""控制台冒烟测试：隐藏的 Tk 根窗口 + 假 viewer，不会弹出任何窗口。"""
import contextlib
import math
import unittest

import mujoco

from dart_sim.console import Runner, set_view
from dart_sim.sim import Sim


class FakeViewer:
    def __init__(self):
        self.cam = mujoco.MjvCamera()
        self.syncs = 0

    def lock(self):
        return contextlib.nullcontext()

    def sync(self):
        self.syncs += 1

    def is_running(self):
        return True


class TestConsole(unittest.TestCase):
    def test_runner_executes_commands(self):
        runner = Runner(Sim(), FakeViewer())
        runner.cmd(lambda s: s.seq.cmd_fire())
        for _ in range(30):
            runner.frame()
        self.assertEqual(len(runner.sim.darts.shots), 1)
        self.assertEqual(runner.viewer.syncs, 30)
        self.assertGreater(runner.snap["t"], 0.45)

    def test_panel_builds(self):
        import tkinter as tk
        from dart_sim.panel import Panel
        try:
            root = tk.Tk()
        except tk.TclError as e:
            self.skipTest(f"没有可用的 Tk：{e}")
        root.withdraw()
        try:
            runner = Runner(Sim(), FakeViewer())
            panel = Panel(root, runner, set_view)
            panel.band["k"].set("120")
            panel._apply_band()
            runner.frame()
            self.assertAlmostEqual(runner.sim.plant.p.band_stiffness, 120.0)
            self.assertIn("状态", panel.text.get("1.0", "end"))
            # 拖动滑条：值一变就下发，并打断正在跑的自动流程
            runner.sim.seq.cmd_salvo(1)
            runner.frame()
            panel._on_scale("arm", 20.0)
            panel._on_scale("slider", 120.0)
            runner.frame()
            self.assertAlmostEqual(runner.sim.plant.servos["arm"].target, math.radians(20.0))
            self.assertAlmostEqual(runner.sim.ctrl.slider_r.target, 0.12)
            self.assertFalse(runner.sim.seq.busy())
            # 自动流程改了目标后，滑条跟着显示
            panel._sync_scales(runner.sim.snapshot()["targets"])
            self.assertAlmostEqual(panel.scales["arm"][0].get(), 20.0, places=1)
        finally:
            root.destroy()


if __name__ == "__main__":
    unittest.main()
