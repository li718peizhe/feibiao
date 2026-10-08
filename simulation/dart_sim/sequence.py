"""发射流程（新机械）：上膛 → 装填 → 瞄准 → 发射，支持单发和连发。

每一步 = (中文名, 进入动作, 完成条件, 超时 s)。“下一发”在执行时按当时状态展开，
所以中途手动上膛/装填过也能接着跑。流程是按 CAD 机构设计的，不来自旧固件。
"""
import math


class Step:
    __slots__ = ("label", "enter", "done", "timeout")

    def __init__(self, label, enter=None, done=None, timeout=5.0):
        self.label, self.enter, self.done, self.timeout = label, enter, done, timeout


class Sequencer:
    def __init__(self, sim):
        self.sim = sim
        self.clear()

    def clear(self):
        self.queue, self.cur, self.fault = [], None, None
        self.label, self.t, self.t0, self.n_shots = "空闲", 0.0, 0.0, 0

    def busy(self):
        return self.cur is not None or bool(self.queue)

    def _fail(self, msg):
        self.fault, self.label = msg, "故障：" + msg
        self.queue, self.cur = [], None

    def _elapsed(self):
        return self.t - self.t0

    # ---- 原语 ----
    def _servo(self, name, deg):
        self.sim.plant.servos[name].target = math.radians(deg)

    def _servo_at(self, name, tol_deg=1.5):
        srv = self.sim.plant.servos[name]
        return srv.done() and abs(self.sim.plant.q(name) - srv.target) < math.radians(tol_deg)

    def _grip(self):
        if not self.sim.darts.grip():
            self._fail("磁铁下面没有飞镖")

    def _release(self):
        if self.sim.darts.release() != "seated":
            self._fail("磁铁断电时飞镖没有放到推板上，掉了")

    # ---- 动作组 ----
    def cock_steps(self):
        pl, c, p = self.sim.plant, self.sim.ctrl, self.sim.plant.p
        bottom = -0.003 - pl.meta["push_gap_mm"] * 1e-3      # 把发射机构压到锁止位下方 3 mm
        return [Step("上膛：张开夹爪", lambda: self._servo("lock", p.lock_open_deg), lambda: self._servo_at("lock"), 2),
                Step("上膛：滑块下压", lambda: c.set_slider(bottom), lambda: pl.q("shuttle") < -0.002, 8),
                Step("上膛：锁止", lambda: self._servo("lock", 0), lambda: pl.latched and self._servo_at("lock"), 2),
                Step("上膛：滑块回位", lambda: c.set_slider(p.slider_park),
                     lambda: pl.latched and c.settled("slider", 0.003, 0.05), 6)]

    def _servo_rad(self, name, rad):
        self.sim.plant.servos[name].target = rad

    def _move(self, label, name, rad, t=3):
        return Step(label, lambda: self._servo_rad(name, rad), lambda: self._servo_at(name), t)

    def _standby(self, side):
        sb = self.sim.plant.meta["loader"]["standby_deg"]
        return math.radians(sb[str(side)] if str(side) in sb else 0.0)

    def load_steps(self):
        """装填（按实物，和取镖相反）：滑块把发射机构带下去，再带着它慢慢抬升到磁铁正下方；
        推杆下放把飞镖送进推板凹槽，磁铁断电，推杆收起；然后压下锁止、滑块回位。要求磁铁上已经吸着一发。"""
        pl, c, d, p = self.sim.plant, self.sim.ctrl, self.sim.darts, self.sim.plant.p
        gap = pl.meta["push_gap_mm"] * 1e-3
        bottom = -0.003 - gap
        return [self._move("装填：推杆收起", "crank", 0.0, 2),
                self._move("装填：摆臂回 0°", "arm", 0.0),
                Step("装填：滑块把发射机构带下去", lambda: c.set_slider(bottom), lambda: pl.q("shuttle") < -0.002, 8),
                Step("装填：张开夹爪", lambda: self._servo("lock", p.lock_open_deg),
                     lambda: self._servo_at("lock") and not pl.latched, 2),
                Step("装填：慢慢抬升到磁铁正下方", lambda: c.ramp_slider(d.q_load - gap, p.load_rise_speed), d.shuttle_at_load, 8),
                self._move("装填：推杆下放", "crank", d.seat_crank, 2),
                Step("装填：磁铁断电", self._release, lambda: self._elapsed() > 0.15, 1),
                self._move("装填：推杆收起", "crank", 0.0, 2),
                Step("装填：压下锁止", lambda: c.set_slider(bottom), lambda: pl.q("shuttle") < -0.002, 6),
                Step("装填：锁止", lambda: self._servo("lock", 0), lambda: pl.latched and self._servo_at("lock"), 2),
                Step("装填：滑块回位", lambda: c.set_slider(p.slider_park),
                     lambda: pl.latched and c.settled("slider", 0.003, 0.05), 6)]

    def fetch_steps(self, side, park):
        """从一侧镖座吸一发，抬起后摆到 park 角。"""
        h = self.sim.darts.holder[side]
        name = "右" if side > 0 else "左"
        return [self._move("取镖：推杆收起", "crank", 0.0, 2),
                self._move(f"取镖：摆到{name}侧镖座", "arm", h["arm"]),
                self._move("取镖：下探", "crank", h["crank"], 2),
                Step("取镖：磁铁通电", self._grip, lambda: self._elapsed() > 0.15, 1),
                self._move("取镖：抬起", "crank", 0.0, 2),
                self._move("取镖：摆到待命位" if park else "取镖：摆回 0°", "arm", park)]

    def _expand_load(self):
        """执行到这一步时再决定：发射机构上已有镖就跳过；吸着一发就装；否则先去镖座取。"""
        pl, d = self.sim.plant, self.sim.darts
        if d.find("seated") is not None and pl.latched:
            return
        if d.find("held") is not None:
            self.queue[0:0] = self.load_steps()
            return
        nxt = d.next_holder()
        if nxt is not None:
            self.queue[0:0] = self.fetch_steps(nxt[0], 0.0) + [Step("装填", self._expand_load, None, 1)]
        elif d.enabled and d.find("seated") is None:
            self._fail("没有飞镖了")
        elif not pl.latched:
            self.queue[0:0] = self.cock_steps()

    def _expand_prefetch(self):
        """发射前先把下一发吸上来，停在不挡发射路径的待命角。"""
        d = self.sim.darts
        nxt = d.next_holder()
        if d.find("held") is None and nxt is not None:
            self.queue[0:0] = self.fetch_steps(nxt[0], self._standby(nxt[0]))

    def _expand_standby(self):
        """吸着的那发在 0° 附近会挡住发射路径，先摆开。"""
        pl, d = self.sim.plant, self.sim.darts
        steps = [] if pl.q("crank") < math.radians(2) else [self._move("发射：推杆收起", "crank", 0.0, 2)]
        if d.find("held") is not None:
            target = self._standby(d.held_side or 1)
            if abs(pl.q("arm")) < abs(target) - math.radians(1):
                steps.append(self._move("发射：吸着的镖摆开", "arm", target))
        self.queue[0:0] = steps

    def fire_steps(self):
        c, d, p = self.sim.ctrl, self.sim.darts, self.sim.plant.p

        def open_lock():
            self.n_shots = len(d.shots)
            self._servo("lock", p.lock_open_deg)
        steps = [Step("发射准备", self._expand_standby, None, 1),
                 Step("瞄准：等 yaw / pitch 到位", None,
                      lambda: c.settled("yaw", math.radians(0.05), 0.01) and c.settled("pitch", 5e-4, 2e-3), 20),
                 Step("发射：解锁", open_lock, lambda: len(d.shots) > self.n_shots, 1.5)]
        if d.enabled:
            steps.append(Step("飞行", None, lambda: d.shots[-1]["distance"] is not None, 6))
        return steps

    def fire_blocker(self):
        """不能发射的原因；可以发射时返回 None。"""
        pl, d = self.sim.plant, self.sim.darts
        if not pl.latched:
            return "发射机构没有锁止"
        if d.enabled and d.find("seated") is None:
            return "发射机构上没有飞镖"
        if pl.q("slider") < pl.p.slider_park - 0.01:
            return "3508 滑块没有回到顶部"
        return None

    def _expand_one(self):
        d = self.sim.darts
        if d.enabled and d.remaining() == 0:
            return
        self.queue[0:0] = [Step("装填", self._expand_load, None, 1), Step("预取下一发", self._expand_prefetch, None, 1)] + self.fire_steps()

    def cancel(self, label="手动控制"):
        """手动操作时打断自动流程，免得两边抢同一个执行器。"""
        if self.busy():
            self.queue, self.cur = [], None
            self.label = label

    # ---- 命令 ----
    def cmd_cock(self):
        if not self.sim.plant.latched:
            self.queue += self.cock_steps()

    def cmd_load(self):
        self.queue += [Step("装填", self._expand_load, None, 1)]

    def cmd_fire(self):
        why = self.fire_blocker()
        if why:
            return self._fail(why)
        self.queue += self.fire_steps()

    def cmd_salvo(self, n):
        self.fault = None
        self.queue += [Step(f"第 {k + 1} 发", self._expand_one, None, 1) for k in range(n)]

    def tick(self, t):
        self.t = t
        if self.cur is None:
            if not self.queue:
                if self.fault is None:
                    self.label = "空闲"
                return
            self.cur = self.queue.pop(0)
            self.t0, self.label = t, self.cur.label
            if self.cur.enter:
                self.cur.enter()
            if self.cur is None:
                return
        if self.cur.done is None or self.cur.done():
            self.cur = None
        elif t - self.t0 > self.cur.timeout:
            self._fail(f"{self.cur.label} 超时")
