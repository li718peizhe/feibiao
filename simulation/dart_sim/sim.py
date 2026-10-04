"""仿真主循环：物理 2 kHz，控制和流程 1 kHz。界面和测试都只通过这个类操作。"""
import math

import mujoco

from .control import Controller
from .darts import Darts
from .params import Params
from .plant import Plant
from .sequence import Sequencer


class Sim:
    def __init__(self, p=None):
        self.plant = Plant(p or Params())
        self.darts = Darts(self.plant)
        self.ctrl = Controller(self.plant)
        self.seq = Sequencer(self)
        self.ctrl_every = max(1, round(self.plant.p.control_dt / self.plant.m.opt.timestep))
        self.reset()

    @property
    def t(self):
        return float(self.plant.d.time)

    def reset(self):
        """初始状态（按 CAD）：已上膛锁止，3508 滑块停在顶部；发射机构上一发，装弹机构在 0° 吸着一发，左右镖座各一发。"""
        pl, p = self.plant, self.plant.p
        mujoco.mj_resetData(pl.m, pl.d)
        pl.set_latch(True)
        pl.set_config(slider=p.slider_park, shuttle=0.0, yaw_slide=p.yaw_slide_range[0], pitch=0.0,
                      lock=0.0, arm=0.0, crank=0.0)
        self.darts.reset()
        mujoco.mj_forward(pl.m, pl.d)
        pl.hold_servos_here()
        for mot in pl.motors.values():
            mot.cmd = 0
        self.ctrl.reset(slider=p.slider_park, yaw=pl.q("frame"), pitch=0.0)
        self.seq.clear()
        self._k = 0

    def set_darts(self, on):
        """开关飞镖（会复位）。"""
        self.plant.p.darts_enabled = bool(on)
        self.darts.apply_enabled()
        self.reset()

    def step(self):
        pl = self.plant
        if self._k % self.ctrl_every == 0:
            self.seq.tick(self.t)
            self.ctrl.update()
        pl.apply_actuators()
        mujoco.mj_step(pl.m, pl.d)
        self._k += 1
        pl.update_latch()
        self.darts.step(self.t)

    def run(self, seconds, until=None):
        """跑 seconds 秒；until() 为真时提前返回 True。"""
        n = int(round(seconds / self.plant.m.opt.timestep))
        for _ in range(n):
            self.step()
            if until is not None and self._k % self.ctrl_every == 0 and until():
                return True
        return False

    def stop(self):
        """急停：清空流程，电机断电，舵机停在当前位置。"""
        self.seq.clear()
        self.seq.label = "急停"
        self.ctrl.stop()
        self.plant.hold_servos_here()

    # ---- 给界面的状态 ----
    def snapshot(self):
        pl, d = self.plant, self.darts
        deg = math.degrees
        motors = {}
        for name, mot in pl.motors.items():
            j = pl.motor_joint[name]
            motors[name] = dict(cmd=mot.cmd, amp=mot.current, torque=mot.tau, rpm=mot.rpm(pl.qd(j)), ecd=mot.ecd(pl.q(j)))
        last = d.shots[-1] if d.shots else None
        c, srv = self.ctrl, pl.servos
        mm = lambda x: None if x is None else x * 1e3
        targets = dict(slider=mm(c.slider_r.target), pitch=mm(c.pitch.target),
                       yaw=None if c.yaw.target is None else deg(c.yaw.target),
                       lock=deg(srv["lock"].target), arm=deg(srv["arm"].target), crank=deg(srv["crank"].target))
        return dict(
            t=self.t, state=self.seq.label, fault=self.seq.fault, motors=motors,
            slider_mm=pl.q("slider") * 1e3, shuttle_mm=pl.q("shuttle") * 1e3, shuttle_v=pl.qd("shuttle"),
            latched=pl.latched, band_n=pl.band_tension(), band_len_mm=pl.band_length() * 1e3,
            yaw_deg=deg(pl.q("frame")), yaw_slide_mm=pl.q("yaw_slide") * 1e3, pitch_mm=pl.q("pitch_slider") * 1e3,
            lock_deg=deg(pl.q("lock")), arm_deg=deg(pl.q("arm")), crank_deg=deg(pl.q("crank")),
            plunger_mm=pl.q("plunger") * 1e3, darts=list(d.state), holders=sum(1 for x in d.state if x == "holder"), shots=list(d.shots), last=last,
            blocker=self.seq.fire_blocker(), targets=targets, darts_enabled=d.enabled)
