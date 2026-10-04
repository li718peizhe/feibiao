"""飞镖发射架仿真的无界面测试。

    python -m unittest discover -s simulation -p "test_*.py"
"""
import math
import os
import unittest

import numpy as np

from dart_sim import cad
from dart_sim.build import XML_NAME, build
from dart_sim.motors import DjiMotor
from dart_sim.panel import render
from dart_sim.params import MODEL_DIR, MOTORS, Params
from dart_sim.sim import Sim

EXPORT_OK = os.path.isdir(Params().export_dir)


def setUpModule():
    if not os.path.exists(os.path.join(MODEL_DIR, XML_NAME)):
        build(Params(), log=lambda *_: None)


@unittest.skipUnless(EXPORT_OK, "没有 CAD 导出目录")
class TestExport(unittest.TestCase):
    def test_every_part_used_once(self):
        parts = cad.Export(Params().export_dir).group_parts()
        flat = [p for ps in parts.values() for p in ps]
        self.assertEqual(len(flat), 498)
        self.assertEqual(len(set(flat)), 498)
        self.assertIn("part_326", parts["pitch_slider"])     # 孤立零件跟着运动件走
        self.assertIn("part_070", parts["yaw_slide"])

    def test_rail_frame(self):
        ex = cad.Export(Params().export_dir)
        self.assertAlmostEqual(math.degrees(math.atan2(ex.U[2], ex.U[1])), 37.0, places=3)


class TestMechanism(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sim = Sim()

    def setUp(self):
        self.sim.reset()

    def test_latch_holds_cocked_shuttle(self):
        self.sim.run(1.0)
        pl = self.sim.plant
        self.assertTrue(pl.latched)
        self.assertLess(abs(pl.q("shuttle")), 5e-4)
        self.assertGreater(pl.band_tension(), 50)
        # 皮筋拉着 pitch 滑块，丝杆联动打滑要小于 0.2 mm
        slip = pl.q("pitch_slider") - pl.coef("c_pitch_screw")[1] * pl.q("pitch_screw")
        self.assertLess(abs(slip), 2e-4)

    def test_yaw_follows_pin_geometry(self):
        s, pl = self.sim, self.sim.plant
        s.ctrl.set_yaw(math.radians(-8))
        self.assertTrue(s.run(6.0, until=lambda: abs(pl.q("frame") - math.radians(-8)) < math.radians(0.05)))
        s.run(1.0)
        self.assertLess(abs(math.degrees(pl.q("frame")) + 8), 0.01)
        # 销轴在机架上、滑台横移：s = -R·sin ψ（多项式拟合误差应小于 0.05 mm）
        r = pl.meta["yaw_pin_radius_mm"] * 1e-3
        self.assertAlmostEqual(pl.q("yaw_slide"), -r * math.sin(pl.q("frame")), delta=5e-5)

    def test_pitch_step(self):
        s, pl = self.sim, self.sim.plant
        s.ctrl.set_pitch(0.05)
        self.assertTrue(s.run(4.0, until=lambda: abs(pl.q("pitch_slider") - 0.05) < 5e-4))

    def test_crank_stroke_and_claws(self):
        s, pl = self.sim, self.sim.plant
        pl.servos["crank"].target = math.pi
        pl.servos["lock"].target = math.radians(pl.p.lock_open_deg)
        s.run(1.5)
        self.assertAlmostEqual(pl.q("plunger"), 0.120, delta=0.002)
        self.assertAlmostEqual(math.degrees(pl.q("claw_r")), pl.p.claw_open_deg, delta=0.5)
        self.assertFalse(pl.latched)

    def test_initial_dart_layout(self):
        s = Sim(Params(darts_enabled=True))
        d, pl = s.darts, s.plant
        self.assertEqual(d.state, ["seated", "held", "holder", "holder"])
        self.assertAlmostEqual(pl.q("arm"), 0.0)                 # 和 CAD 一样：摆臂 0° 吸着一发
        s.run(0.5)
        U = pl.d.xmat[pl.m.body("frame").id].reshape(3, 3) @ np.array([0, 0.7986355, 0.6018150])
        for i in range(4):                                       # 四发都和导轨平行
            axis = pl.d.xmat[d.body[i]].reshape(3, 3)[:, 0]
            self.assertGreater(axis @ U, 0.9999, f"第 {i + 1} 发没有沿导轨方向")
        lay = pl.meta["loader"]
        # 发射机构上那发：凸块后沿正好顶在推板前沿
        self.assertAlmostEqual(lay["seat_com_q0"][1] + lay["features"]["lug_rear_x"], -5.71, delta=0.05)
        self.assertLess(np.linalg.norm(d.pose(d.body[1])[0] - d._held_world()[0]), 1e-3)

    def test_fire_swings_held_dart_clear(self):
        s = Sim(Params(darts_enabled=True))
        pl = s.plant
        s.seq.cmd_fire()
        arm_at_release = []
        s.run(2.0, until=lambda: bool(s.darts.shots) and not arm_at_release.append(pl.q("arm")))
        standby = math.radians(abs(pl.meta["loader"]["standby_deg"]["1"]))
        self.assertGreaterEqual(abs(arm_at_release[0]), standby - math.radians(1))

    def test_loading_drops_dart_into_shuttle(self):
        s = Sim(Params(darts_enabled=True))
        pl, d = s.plant, s.darts
        s.seq.cmd_fire()
        s.run(2.0, until=lambda: not s.seq.busy())
        s.seq.cmd_load()
        q_at_release = []
        s.run(15.0, until=lambda: (d.falling is not None and not q_at_release) and not q_at_release.append(pl.q("shuttle")))
        self.assertAlmostEqual(q_at_release[0], d.q_load, delta=2e-3)    # 磁铁断电时发射机构在磁铁正下方
        s.run(15.0, until=lambda: not s.seq.busy())
        self.assertIsNone(s.seq.fault)
        self.assertTrue(pl.latched)
        self.assertEqual(d.state[1], "seated")
        rel = d.pose(d.body[1])[0] - d.pose(d.carrier["shuttle"])[0]
        seat = pl.d.xmat[d.carrier["shuttle"]].reshape(3, 3) @ d.seat_rel[0]
        self.assertLess(np.linalg.norm(rel - seat), 1e-3)                  # 落到凸起进槽的位置

    def test_fire_blocked_without_latch(self):
        s = self.sim
        s.seq.cmd_fire()
        s.run(3.0)
        s.seq.cmd_fire()
        self.assertEqual(s.seq.fault, "发射机构没有锁止")
        self.assertIn("不能发射", render(s.snapshot()))


class TestMotor(unittest.TestCase):
    def test_torque_limits(self):
        mot = DjiMotor(MOTORS["M3508"])
        mot.cmd = 16384
        self.assertAlmostEqual(mot.torque(0.0), 6.0, places=6)
        self.assertAlmostEqual(mot.torque(482 * 2 * math.pi / 60), 0.0, places=6)
        mot.cmd = 5000
        self.assertAlmostEqual(mot.torque(10.0), 0.3 * 5000 * 20 / 16384, places=6)


class TestLaunchCycle(unittest.TestCase):
    def test_salvo_of_four_with_darts(self):
        s = Sim(Params(darts_enabled=True))
        s.seq.cmd_salvo(4)
        s.run(60.0, until=lambda: not s.seq.busy())
        self.assertIsNone(s.seq.fault)
        self.assertEqual(len(s.darts.shots), 4)
        for shot in s.darts.shots:
            self.assertGreater(shot["v_release"], 8.0)
            self.assertGreater(shot["distance"], 8.0)
        self.assertEqual(s.darts.state, ["landed"] * 4)

    def test_salvo_without_darts_still_runs_loader(self):
        s = Sim()                                   # 默认不放飞镖
        s.seq.cmd_salvo(4)
        arm_max = 0.0
        for _ in range(6000):
            s.run(0.01)
            arm_max = max(arm_max, abs(s.plant.q("arm")))
            if not s.seq.busy():
                break
        self.assertIsNone(s.seq.fault)
        self.assertEqual(len(s.darts.shots), 4)
        self.assertGreater(math.degrees(arm_max), 44.0)     # 装弹机构照常去镖座取镖
        self.assertEqual(s.darts.state, ["landed"] * 4)

    def test_pitch_raises_launch_speed(self):
        speeds = []
        for pitch in (0.0, -0.06):                  # pitch 滑块往下 = 皮筋更长
            s = Sim()
            s.ctrl.set_pitch(pitch)
            s.seq.cmd_salvo(1)
            s.run(30.0, until=lambda: not s.seq.busy())
            self.assertIsNone(s.seq.fault)
            speeds.append(s.darts.shots[0]["v_release"])
        self.assertGreater(speeds[1], speeds[0] + 0.3)


if __name__ == "__main__":
    unittest.main()
