"""DJI 电机 + 电调模型、位置式 PID、舵机限速。

电机接口和 CAN 一致：写电流指令（int16），读转子编码器 0..8191、转子转速 rpm。
"""
import math

ECD_RANGE = 8192


def clamp(x, lo, hi):
    return lo if x < lo else hi if x > hi else x


class DjiMotor:
    """电流指令 -> 输出轴转矩。电流限幅之外还有电压限制的转矩-转速线（接近空载转速时转矩掉到 0）。"""

    def __init__(self, spec):
        self.spec = spec
        self.cmd = 0
        self.tau = 0.0
        self._w0 = spec.noload_rpm * 2 * math.pi / 60
        self._tsv = spec.voltage_stall_torque
        self._tau_i = spec.kt * spec.i_max

    @property
    def current(self):
        return clamp(int(self.cmd), -self.spec.cmd_max, self.spec.cmd_max) * self.spec.i_max / self.spec.cmd_max

    def torque(self, w_out):
        hi = min(self._tau_i, self._tsv * (1 - w_out / self._w0))
        lo = max(-self._tau_i, -self._tsv * (1 + w_out / self._w0))
        self.tau = clamp(self.spec.kt * self.current, lo, hi)
        return self.tau

    def counts(self, q_out):
        """多圈转子计数（连续值），相当于固件里累加的 total_angle。"""
        return q_out * self.spec.ratio * ECD_RANGE / (2 * math.pi)

    def ecd(self, q_out):
        return int(math.floor(self.counts(q_out))) % ECD_RANGE

    def rpm(self, w_out):
        return w_out * self.spec.ratio * 60 / (2 * math.pi)


class Pid:
    """位置式 PID，和 RM 固件常见写法一致：D 是相邻两次误差差分，积分先限幅再求和。"""

    def __init__(self, kp, ki, kd, max_out, max_iout):
        self.kp, self.ki, self.kd = kp, ki, kd
        self.max_out, self.max_iout = max_out, max_iout
        self.reset()

    def reset(self):
        self.iout = 0.0
        self.last_err = 0.0
        self.out = 0.0

    def calc(self, ref, fdb):
        err = ref - fdb
        self.iout = clamp(self.iout + self.ki * err, -self.max_iout, self.max_iout)
        self.out = clamp(self.kp * err + self.iout + self.kd * (err - self.last_err), -self.max_out, self.max_out)
        self.last_err = err
        return self.out


class Cascade:
    """位置环（转子计数 -> rpm）+ 速度环（rpm -> 电流指令）。"""

    def __init__(self, motor, pos, spd):
        self.motor = motor
        self.pos = Pid(*pos)
        self.spd = Pid(*spd)

    def reset(self):
        self.pos.reset()
        self.spd.reset()

    def update(self, target_counts, counts, rpm, feedforward=0.0):
        w_ref = self.pos.calc(target_counts, counts)
        self.motor.cmd = int(clamp(self.spd.calc(w_ref, rpm) + feedforward, -self.motor.spec.cmd_max, self.motor.spec.cmd_max))
        return self.motor.cmd


class RateLimiter:
    """舵机：目标角按最大角速度逼近。"""

    def __init__(self, speed, value=0.0):
        self.speed = speed
        self.value = value
        self.target = value

    def step(self, dt):
        self.value += clamp(self.target - self.value, -self.speed * dt, self.speed * dt)
        return self.value

    def done(self, tol=1e-3):
        return abs(self.target - self.value) < tol
