"""仿真本体：加载模型、联动关节的一致构型、电机/舵机执行、锁止。飞镖相关逻辑在 darts.py。"""
import json
import math
import os

import mujoco
import numpy as np

from . import build, loader_geometry
from .motors import DjiMotor, RateLimiter
from .params import MODEL_DIR, MOTORS, Params


class Plant:
    def __init__(self, p=None, model_dir=MODEL_DIR):
        self.p = p = p or Params()
        xml = os.path.join(model_dir, build.XML_NAME)
        if not os.path.exists(xml):
            build.build(p, model_dir)
        with open(os.path.join(model_dir, "meta.json"), encoding="utf-8") as f:
            self.meta = json.load(f)
        self.m = m = mujoco.MjModel.from_xml_path(xml)
        self.d = mujoco.MjData(m)
        m.opt.timestep = p.timestep
        self.qadr = {m.joint(i).name: m.jnt_qposadr[i] for i in range(m.njnt)}
        self.vadr = {m.joint(i).name: m.jnt_dofadr[i] for i in range(m.njnt)}
        self.act = {m.actuator(i).name: i for i in range(m.nu)}
        self.eq = {m.equality(i).name: i for i in range(m.neq)}
        self.shuttle_jnt = m.joint("shuttle").id
        self.rail_angle = math.radians(self.meta["rail_angle_deg"])
        self.motors = {"motor_r": DjiMotor(MOTORS["M3508"]), "motor_l": DjiMotor(MOTORS["M3508"]),
                       "yaw": DjiMotor(MOTORS[p.yaw_motor]), "pitch": DjiMotor(MOTORS[p.pitch_motor])}
        self.motor_joint = {"motor_r": "motor_r", "motor_l": "motor_l", "yaw": "yaw_screw", "pitch": "pitch_screw"}
        self.servos = {"lock": RateLimiter(p.servo_speed), "arm": RateLimiter(p.arm_speed), "crank": RateLimiter(p.servo_speed)}
        self.latched = False
        self.apply_params()
        mujoco.mj_setConst(m, self.d)     # 改过质量后重算常量；只在初始化时调用（会覆盖 data）

    # ---- 参数（加载后可改） ----
    def apply_params(self):
        m, p = self.m, self.p
        t = m.tendon("band").id
        m.tendon_stiffness[t] = p.band_stiffness
        m.tendon_damping[t] = p.band_damping
        m.tendon_lengthspring[t] = (0.0, p.band_rest_length)
        for i in range(4):
            b = m.body(f"dart{i}").id
            if p.dart_mass is not None:              # 默认用 STEP 估出来的质量
                k = p.dart_mass / m.body_mass[b]
                m.body_mass[b] *= k
                m.body_inertia[b] *= k
            m.geom_friction[m.geom(f"dart{i}").id, 0] = p.dart_rail_friction
        m.geom_friction[m.geom("rail_top").id, 0] = p.dart_rail_friction

    # ---- 联动关节一致构型 ----
    def coef(self, eq_name):
        """联动的多项式系数 [a0..a4]（q1 = Σ a_k q2^k）。固定腱联动（q1 + c·q2 = 0）换算成同样的形式。"""
        m, e = self.m, self.eq[eq_name]
        if m.eq_type[e] != mujoco.mjtEq.mjEQ_TENDON:
            return m.eq_data[e, :5]
        adr = m.tendon_adr[m.eq_obj1id[e]]
        c1, c2 = m.wrap_prm[adr], m.wrap_prm[adr + 1]
        return np.array([0.0, -c2 / c1, 0.0, 0.0, 0.0])

    def frame_angle(self, slide):
        c = self.coef("c_yaw_pin")
        return float(sum(c[k] * slide ** k for k in range(5)))

    def crank_for_drop(self, drop):
        """推杆下探 drop（m）对应的曲柄角。"""
        return loader_geometry.crank_for_drop(drop * 1e3, self.meta["crank_r_mm"], self.meta["rod_l_mm"])

    def loader_q(self, crank):
        r, l = self.meta["crank_r_mm"] * 1e-3, self.meta["rod_l_mm"] * 1e-3
        rod = crank - math.asin(r * math.sin(crank) / l)
        drop = (r - l) - r * math.cos(crank) + math.sqrt(l * l - (r * math.sin(crank)) ** 2)
        return rod, drop

    def set_config(self, slider=None, shuttle=None, yaw_slide=None, pitch=None, lock=None, arm=None, crank=None):
        q, a = self.d.qpos, self.qadr
        if slider is not None:
            r = self.coef("c_belt_r")[1]
            q[a["slider"]], q[a["motor_r"]], q[a["motor_l"]] = slider, slider / r, -slider / r
            q[a["idler_r"]], q[a["idler_l"]] = slider * self.coef("c_idler_r")[1], slider * self.coef("c_idler_l")[1]
        if shuttle is not None:
            q[a["shuttle"]] = shuttle
        if yaw_slide is not None:
            q[a["yaw_slide"]], q[a["frame"]] = yaw_slide, self.frame_angle(yaw_slide)
            q[a["yaw_screw"]] = yaw_slide / self.coef("c_yaw_screw")[1]
        if pitch is not None:
            q[a["pitch_slider"]], q[a["pitch_screw"]] = pitch, pitch / self.coef("c_pitch_screw")[1]
        if lock is not None:
            q[a["lock"]] = lock
            q[a["claw_r"]], q[a["claw_l"]] = lock * self.coef("c_claw_r")[1], lock * self.coef("c_claw_l")[1]
        if arm is not None:
            q[a["arm"]] = arm
        if crank is not None:
            q[a["crank"]] = crank
            q[a["rod"]], q[a["plunger"]] = self.loader_q(crank)
        mujoco.mj_forward(self.m, self.d)

    def q(self, joint):
        return float(self.d.qpos[self.qadr[joint]])

    def qd(self, joint):
        return float(self.d.qvel[self.vadr[joint]])

    # ---- 锁止：夹爪合拢且发射机构压到锁止位以下时，把发射机构行程上限改成 0 ----
    def set_latch(self, on):
        self.latched = on
        self.m.jnt_range[self.shuttle_jnt, 1] = 0.0 if on else self.p.shuttle_top

    def update_latch(self):
        lock = self.q("lock")
        if self.latched and lock > math.radians(10):
            self.set_latch(False)
        elif not self.latched and lock < math.radians(3) and self.q("shuttle") <= 5e-4:
            self.set_latch(True)

    # ---- 执行器 ----
    def apply_actuators(self):
        d, dt = self.d, self.m.opt.timestep
        for name, mot in self.motors.items():
            d.ctrl[self.act[name]] = mot.torque(d.qvel[self.vadr[self.motor_joint[name]]])
        for name, srv in self.servos.items():
            d.ctrl[self.act[name]] = srv.step(dt)

    def band_length(self):
        return float(self.d.ten_length[self.m.tendon("band").id])

    def band_tension(self):
        """皮筋张力（滚轮无摩擦，整根一样）。发射机构受两股，约 2 倍。"""
        return max(0.0, self.p.band_stiffness * (self.band_length() - self.p.band_rest_length))

    def hold_servos_here(self):
        for name, srv in self.servos.items():
            srv.value = srv.target = self.q(name)
