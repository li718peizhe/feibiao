"""三个 DJI 电机轴的级联 PID（1 kHz）。按新机械设计：目标给机构量，换算成转子计数后闭环。

- 3508 滑块：两台电机各自闭环，目标镜像（左右电机装反）
- yaw：目标是机架转角 ψ，经销轴几何 s = -R·sin ψ 和丝杆导程换算到电机
- pitch：目标是 pitch 滑块位移（拉力调节）
增益是在仿真里调的起步值，上实车要重新整定。
"""
import math

from .motors import Cascade, clamp

# (位置环 kp, ki, kd, 最大输出 rpm, 积分限幅), (速度环 kp, ki, kd, 最大电流指令, 积分限幅)
GAINS = {
    "slider": ((0.08, 0.0, 0.0, 6000, 0), (15.0, 0.5, 0.0, 16384, 16000)),
    "yaw": ((0.2, 0.0, 0.0, 15000, 0), (3.0, 0.05, 0.0, 10000, 5000)),
    "pitch": ((0.2, 0.0, 0.0, 15000, 0), (3.0, 0.05, 0.0, 10000, 6000)),
}


class Axis:
    def __init__(self, plant, motor, joint, to_motor, gains):
        self.pl, self.mot, self.joint, self.to_motor = plant, plant.motors[motor], joint, to_motor
        self.loop = Cascade(self.mot, *gains)
        self.target = None

    def update(self):
        if self.target is None:
            self.mot.cmd = 0
            return
        q, w = self.pl.q(self.joint), self.pl.qd(self.joint)
        self.loop.update(self.mot.counts(self.to_motor(self.target)), self.mot.counts(q), self.mot.rpm(w))


class Controller:
    def __init__(self, plant):
        self.pl = pl = plant
        p = pl.p
        r = pl.coef("c_belt_r")[1]
        k_yaw, k_pitch = pl.coef("c_yaw_screw")[1], pl.coef("c_pitch_screw")[1]
        self.pin_radius = pl.meta["yaw_pin_radius_mm"] * 1e-3
        lo, hi = p.yaw_slide_range
        self.yaw_range = tuple(sorted((-math.asin(lo / self.pin_radius), -math.asin(hi / self.pin_radius))))
        self.slider_r = Axis(pl, "motor_r", "motor_r", lambda s: s / r, GAINS["slider"])
        self.slider_l = Axis(pl, "motor_l", "motor_l", lambda s: -s / r, GAINS["slider"])
        self.yaw = Axis(pl, "yaw", "yaw_screw",
                        lambda psi: clamp(-self.pin_radius * math.sin(psi), lo, hi) / k_yaw, GAINS["yaw"])
        self.pitch = Axis(pl, "pitch", "pitch_screw", lambda s: clamp(s, *p.pitch_range) / k_pitch, GAINS["pitch"])
        self.axes = (self.slider_r, self.slider_l, self.yaw, self.pitch)
        self._ramp = None                    # (终点, 速度 m/s)：滑块目标按限定速度走

    def reset(self, slider, yaw, pitch):
        for a in self.axes:
            a.loop.reset()
        self.set_slider(slider)
        self.set_yaw(yaw)
        self.set_pitch(pitch)

    def set_slider(self, s):
        self._ramp = None
        s = None if s is None else clamp(s, -0.33, self.pl.p.slider_top)
        self.slider_r.target = self.slider_l.target = s

    def ramp_slider(self, s, speed):
        """滑块目标从当前位置按 speed 匀速走到 s（装填时带着发射机构慢慢抬升）。"""
        start = self.pl.q("slider")
        self.set_slider(start)
        self._ramp = (clamp(s, -0.33, self.pl.p.slider_top), speed)

    def set_yaw(self, psi):
        self.yaw.target = None if psi is None else clamp(psi, *self.yaw_range)

    def set_pitch(self, s):
        self.pitch.target = None if s is None else clamp(s, *self.pl.p.pitch_range)

    def stop(self):
        self._ramp = None
        for a in self.axes:
            a.target = None
            a.loop.reset()

    def update(self):
        if self._ramp is not None and self.slider_r.target is not None:
            end, v = self._ramp
            cur = self.slider_r.target
            step = v * self.pl.p.control_dt
            nxt = end if abs(end - cur) <= step else cur + math.copysign(step, end - cur)
            self.slider_r.target = self.slider_l.target = nxt
            if nxt == end:
                self._ramp = None
        for a in self.axes:
            a.update()

    def settled(self, axis, tol, vtol):
        """axis: 'slider' / 'yaw' / 'pitch'；误差和速度都小于阈值。"""
        joint, target = {"slider": ("slider", self.slider_r.target), "yaw": ("frame", self.yaw.target),
                         "pitch": ("pitch_slider", self.pitch.target)}[axis]
        return target is not None and abs(self.pl.q(joint) - target) < tol and abs(self.pl.qd(joint)) < vtol
