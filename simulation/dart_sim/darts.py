"""飞镖：两侧镖座、装弹机构磁吸、装到发射机构、出膛分离、飞行气动和落点。

四发的初始位置（按实物）：发射机构上 1 发、装弹机构磁吸 1 发（摆臂待命角）、左右镖座各 1 发。
飞镖都沿导轨方向放：镖体下面的长条凸起卡进推板/镖座的凹槽，更低的凸块顶在推板前沿。
摆臂转动时磁吸的飞镖保持和导轨平行（假设磁铁头能随动转动，否则从 ±45° 镖座取来的镖装不进推板）。
装填和取镖相反：发射机构停在磁铁正下方，推杆下放把飞镖送进推板凹槽，磁铁断电，推杆收起。
启用飞镖时：夹持/装填用 weld 约束表示，出膛后飞镖是自由刚体，受重力、简化气动和导轨/地面接触。
不启用飞镖（默认）时：飞镖隐藏、不参与动力学，只记“哪一发在哪”，装弹机构和发射流程照常动作，
每次发射记录发射机构的峰值速度。
"""
import math

import mujoco
import numpy as np

G, RHO = 9.81, 1.2
ALIGN, SPIN_DAMP = 3e-4, 8e-3        # 风标稳定力矩系数、角速度阻尼（简化气动）
N_DARTS = 4


def _mat(q):
    R = np.zeros(9)
    mujoco.mju_quat2Mat(R, q)
    return R.reshape(3, 3)


def rel_pose(p1, q1, p2, q2):
    qi, qr = np.zeros(4), np.zeros(4)
    mujoco.mju_negQuat(qi, q1)
    mujoco.mju_mulQuat(qr, qi, q2)
    return _mat(q1).T @ (p2 - p1), qr


def compose(p1, q1, pr, qr):
    q = np.zeros(4)
    mujoco.mju_mulQuat(q, q1, qr)
    return p1 + _mat(q1) @ pr, q


def near(a, b, tol_pos=0.006, tol_deg=4.0):
    dq = rel_pose(a[0], a[1], b[0], b[1])[1]
    return np.linalg.norm(a[0] - b[0]) < tol_pos and 2 * math.degrees(math.acos(min(1.0, abs(dq[0])))) < tol_deg


class Darts:
    def __init__(self, plant):
        self.pl = pl = plant
        m = pl.m
        self.n = N_DARTS
        self.body = [m.body(f"dart{i}").id for i in range(self.n)]
        self.geom = {m.geom(f"dart{i}").id: i for i in range(self.n)}
        self.qadr = [m.jnt_qposadr[m.joint(f"dart{i}").id] for i in range(self.n)]
        self.vadr = [m.jnt_dofadr[m.joint(f"dart{i}").id] for i in range(self.n)]
        self.carrier = {c: m.body(c).id for c in ("shuttle", "plunger", "frame")}
        self.floor = m.geom("floor").id
        self.exit_site = m.site("rail_exit").id
        lay = pl.meta["loader"]
        site = lambda nm: (m.site_pos[m.site(nm).id].copy(), m.site_quat[m.site(nm).id].copy())
        self.held_rel0 = site("magnet")                   # 摆臂 0° 时飞镖相对推杆的位姿
        self.mag_c = m.site_pos[m.site("magnet_centre").id].copy()
        self.mag_axis = m.jnt_axis[m.joint("arm").id].copy()
        self.seat_rel = site("dart_seat")                 # 相对发射机构
        self.q_load = lay["q_load"] * 1e-3                # 装填时发射机构的位置
        self.seat_crank = pl.crank_for_drop(lay["seat_drop"] * 1e-3)   # 放镖时曲柄下放的角度
        self.holder = {}
        for k, v in lay["holders"].items():
            side = int(k)
            self.holder[side] = dict(arm=math.radians(v["arm_deg"]), crank=pl.crank_for_drop(v["drop"] * 1e-3),
                                     rel=site(f"holder_{'r' if side > 0 else 'l'}"))
        self._geoms = [g for b in self.body for g in range(m.body_geomadr[b], m.body_geomadr[b] + m.body_geomnum[b])]
        self._look = {g: (float(m.geom_rgba[g, 3]), int(m.geom_contype[g]), int(m.geom_conaffinity[g])) for g in self._geoms}
        self.apply_enabled()
        self.shots, self._armed = [], False

    def apply_enabled(self):
        """按参数 darts_enabled 显示/隐藏飞镖，并开关它们的碰撞。改完要 reset。"""
        m = self.pl.m
        self.enabled = bool(self.pl.p.darts_enabled)
        for g, (alpha, ct, ca) in self._look.items():
            m.geom_rgba[g, 3] = alpha if self.enabled else 0.0
            m.geom_contype[g] = ct if self.enabled else 0
            m.geom_conaffinity[g] = ca if self.enabled else 0

    def pose(self, body):
        return self.pl.d.xpos[body].copy(), self.pl.d.xquat[body].copy()

    def held_rel(self, arm=None):
        """磁吸住的飞镖相对推杆的位姿：摆臂转 arm 角时磁铁头反向转同样角度，飞镖始终和导轨平行。"""
        arm = self.pl.q("arm") if arm is None else arm
        qr, q = np.zeros(4), np.zeros(4)
        mujoco.mju_axisAngle2Quat(qr, self.mag_axis, -arm)
        mujoco.mju_mulQuat(q, qr, self.held_rel0[1])
        return self.mag_c + _mat(qr) @ (self.held_rel0[0] - self.mag_c), q

    def _held_world(self):
        return compose(*self.pose(self.carrier["plunger"]), *self.held_rel())

    # ---- weld 挂接 ----
    def _weld(self, i, carrier):
        return self.pl.eq[f"dart{i}_{carrier}"]

    def detach(self, i):
        for c in self.carrier:
            self.pl.d.eq_active[self._weld(i, c)] = 0

    def attach(self, i, carrier, rel, snap=False):
        self.detach(i)
        e = self._weld(i, carrier)
        self.pl.m.eq_data[e, 0:3] = 0
        self.pl.m.eq_data[e, 3:6], self.pl.m.eq_data[e, 6:10] = rel
        self.pl.d.eq_active[e] = 1
        if snap:
            pos, quat = compose(*self.pose(self.carrier[carrier]), *rel)
            self.pl.d.qpos[self.qadr[i]:self.qadr[i] + 7] = np.concatenate([pos, quat])
            self.pl.d.qvel[self.vadr[i]:self.vadr[i] + 6] = 0

    def reset(self):
        """调用前机架、发射机构、摆臂已经摆到初始构型。"""
        self.state = ["seated", "held", "holder", "holder"]
        self.side = {2: 1, 3: -1}                   # 镖座上的飞镖 -> 哪一侧
        self.flight, self.shots = {}, []
        self.held_side = None                       # 吸着的那发是从哪一侧镖座取的（开机那发为 None）
        self.pl.d.xfrc_applied[:] = 0
        if not self.enabled:                         # 隐藏的飞镖统一挂在机架上，不影响发射机构和装弹机构
            for i in range(self.n):
                self.attach(i, "frame", self.holder[1]["rel"], snap=True)
            return
        self.attach(0, "shuttle", self.seat_rel, snap=True)
        self.attach(1, "plunger", self.held_rel(), snap=True)
        for i, side in self.side.items():
            self.attach(i, "frame", self.holder[side]["rel"], snap=True)
        self.pl.d.xfrc_applied[:] = 0

    # ---- 查询 ----
    def find(self, state):
        return next((i for i in range(self.n) if self.state[i] == state), None)

    def next_holder(self):
        """按取镖顺序找还有镖的镖座，返回 (side, 飞镖号)；都空了返回 None。"""
        for side in self.pl.p.holder_order:
            i = next((k for k, s in self.side.items() if s == side and self.state[k] == "holder"), None)
            if i is not None:
                return side, i
        return None

    def remaining(self):
        return sum(1 for s in self.state if s in ("seated", "held", "holder"))

    # ---- 磁吸 ----
    def _loader_at(self, arm, crank, tol_deg=2.0):
        tol = math.radians(tol_deg)
        return abs(self.pl.q("arm") - arm) < tol and abs(self.pl.q("crank") - crank) < tol

    def shuttle_at_load(self):
        """发射机构停在磁铁正下方。"""
        pl = self.pl
        return abs(pl.q("shuttle") - self.q_load) < 2e-3 and abs(pl.qd("shuttle")) < 0.01

    def at_load_position(self):
        """放镖条件：发射机构在磁铁正下方，摆臂 0°，推杆已经下放到推板上。"""
        return self.shuttle_at_load() and self._loader_at(0.0, self.seat_crank)

    def grip(self):
        """磁铁通电：磁吸面下方正好有一发（镖座上）就吸住。"""
        if self.find("held") is not None:
            return False
        for i, side in self.side.items():
            h = self.holder[side]
            if self.state[i] != "holder" or not self._loader_at(h["arm"], h["crank"]):
                continue
            if self.enabled:   # 电磁铁能把偏 1 cm 以内的铁片吸过来对正
                if not near(self.pose(self.body[i]), self._held_world(), tol_pos=0.012):
                    continue
                self.attach(i, "plunger", self.held_rel())
            self.state[i] = "held"
            self.held_side = side
            return True
        return False

    def release(self):
        """磁铁断电：推杆已经把飞镖送到推板上就装好，否则掉下去。"""
        i = self.find("held")
        if i is None:
            return None
        ok = self.find("seated") is None and self.at_load_position()
        self.held_side = None
        if not self.enabled:
            self.state[i] = "seated" if ok else "landed"
            return "seated" if ok else "dropped"
        if not ok:
            self.detach(i)
            self.state[i] = "free"
            return "dropped"
        self.attach(i, "shuttle", self.seat_rel)
        self.state[i] = "seated"
        return "seated"

    # ---- 每个物理步调用 ----
    def step(self, t):
        i = self.find("held")
        if self.enabled and i is not None:                 # 磁铁头随摆臂反转，飞镖保持和导轨平行
            e = self._weld(i, "plunger")
            self.pl.m.eq_data[e, 3:6], self.pl.m.eq_data[e, 6:10] = self.held_rel()
        self._separate(t)
        self._aero()
        self._land(t)

    def _separate(self, t):
        """发射机构的减速度超过飞镖自己（重力分量 + 导轨摩擦）时，飞镖脱离托槽。每次解锁只记一发。"""
        pl, i = self.pl, self.find("seated")
        if pl.latched:
            self._armed = True
            return
        if not self._armed or (self.enabled and i is None):
            return
        a, v = pl.d.qacc[pl.vadr["shuttle"]], pl.qd("shuttle")
        limit = -G * (math.sin(pl.rail_angle) + pl.p.dart_rail_friction * math.cos(pl.rail_angle)) - 0.5
        if v > 0.05 and a < limit:
            self._armed = False
            if not self.enabled:
                if i is not None:
                    self.state[i] = "landed"
                self.shots.append(dict(dart=i, t_release=t, v_release=v, shuttle_v=v,
                                       distance=None, flight_time=None, landing=None))
                return
            self.detach(i)
            self.state[i] = "flying"
            vel = pl.d.qvel[self.vadr[i]:self.vadr[i] + 3]
            self.flight[i] = pl.d.site_xpos[self.exit_site].copy()
            self.shots.append(dict(dart=i, t_release=t, v_release=float(np.linalg.norm(vel)), shuttle_v=v,
                                   distance=None, flight_time=None, landing=None))

    def _aero(self):
        if not self.enabled:
            return
        d, cda = self.pl.d, self.pl.p.dart_cda
        for i, st in enumerate(self.state):
            b = self.body[i]
            v = d.qvel[self.vadr[i]:self.vadr[i] + 3]
            speed = float(np.linalg.norm(v))
            if st not in ("flying", "free") or speed < 0.5:
                d.xfrc_applied[b] = 0
                continue
            R = d.xmat[b].reshape(3, 3)
            w = R @ d.qvel[self.vadr[i] + 3:self.vadr[i] + 6]
            d.xfrc_applied[b, :3] = -0.5 * RHO * cda * speed * v
            d.xfrc_applied[b, 3:] = ALIGN * speed * np.cross(R[:, 0], v) - SPIN_DAMP * w

    def _land(self, t):
        d = self.pl.d
        if not self.enabled or d.ncon == 0:
            return
        for g1, g2 in zip(d.contact.geom1, d.contact.geom2):
            if self.floor not in (g1, g2):
                continue
            i = self.geom.get(g2 if g1 == self.floor else g1)
            if i is None or self.state[i] not in ("flying", "free"):
                continue
            if self.state[i] == "flying":
                shot = next(s for s in reversed(self.shots) if s["dart"] == i)
                pos = d.xpos[self.body[i]].copy()
                shot.update(landing=pos, flight_time=t - shot["t_release"],
                            distance=float(np.linalg.norm((pos - self.flight.pop(i))[:2])))
            self.state[i] = "landed"
