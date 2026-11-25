"""摇臂（arm）左右摆动需要多大力矩。

摆臂绕一根倾斜 37° 的轴转（CAD 的旋转_joint，轴 = (0, -sin37, cos37)），
所以它相当于一个偏心单摆：0°（装填位）是重力最低点，两侧 ±50° 要克服重力矩。

用法：
    python -m dart_sim.arm_force            # 在 simulation 目录下
输出两张表：
1. 各摆角下的保持力矩（准静态：跑物理跑到停住，取执行器力）——空载 / 吸一发飞镖
2. 从 0° 摆到 ±48° 的动态峰值力矩、峰值角速度，两档摆速
"""
import math
import sys

import numpy as np

if __package__ in (None, ""):
    sys.path.insert(0, str(__import__("pathlib").Path(__file__).resolve().parents[1]))
    from dart_sim.params import Params
    from dart_sim.sim import Sim
else:
    from .params import Params
    from .sim import Sim

ARM_MAX = 48.4          # 取镖角（meta.json: 右 +48.36°，左 -48.35°）


def _sim(darts_on):
    p = Params()
    p.darts_enabled = darts_on
    return Sim(p)


def hold_torque(darts_on, deg, settle=1.2):
    """把摆臂放到 deg 并保持，返回末段平均执行器力矩（= 需要的保持力矩），以及稳态角误差。"""
    sim = _sim(darts_on)
    pl = sim.plant
    pl.d.qpos[pl.qadr["arm"]] = math.radians(deg)
    pl.hold_servos_here()
    if darts_on:
        sim.darts.attach(1, "plunger", sim.darts.held_rel(), snap=True)
    sim.run(0.05)
    pl.servos["arm"].target = math.radians(deg)
    n = int(settle / pl.m.opt.timestep)
    tail = []
    for k in range(n):
        sim.step()
        if k > n - 200:
            tail.append(pl.d.actuator_force[pl.act["arm"]])
    return float(np.mean(tail)), abs(float(pl.q("arm")) - math.radians(deg))


def swing(darts_on, target_deg, speed_deg_s, seconds=2.5):
    """摆臂从 0° 摆到 target_deg（限速 speed_deg_s），返回 (峰值力矩, 峰值角速度, 峰值角加速度)。"""
    sim = _sim(darts_on)
    pl = sim.plant
    pl.servos["arm"].speed = math.radians(speed_deg_s)      # RateLimiter 的限速
    pl.d.qpos[pl.qadr["arm"]] = 0.0
    pl.hold_servos_here()
    if darts_on:
        sim.darts.attach(1, "plunger", sim.darts.held_rel(), snap=True)
    sim.run(0.3)
    pl.servos["arm"].target = math.radians(target_deg)
    tau, w = [], []
    for _ in range(int(seconds / pl.m.opt.timestep)):
        sim.step()
        tau.append(pl.d.actuator_force[pl.act["arm"]])
        w.append(pl.qd("arm"))
    tau, w = np.array(tau), np.array(w)
    alpha = np.gradient(w, pl.m.opt.timestep)
    return float(np.abs(tau).max()), float(np.abs(w).max()), float(np.abs(alpha).max())


def main():
    print("摆臂重力矩（准静态实测）：")
    print(f"{'摆角°':>6} {'空载 N·m':>11} {'吸一发 N·m':>12}")
    rows = []
    for deg in range(-50, 51, 10):
        a, _ = hold_torque(False, deg)
        b, _ = hold_torque(True, deg)
        rows.append((deg, a, b))
        print(f"{deg:6d} {a:+11.4f} {b:+12.4f}")
    s = np.sin(np.radians([r[0] for r in rows]))
    k0 = float(np.polyfit(s, [r[1] for r in rows], 1)[0])
    k1 = float(np.polyfit(s, [r[2] for r in rows], 1)[0])
    print(f"拟合：空载 {k0:.3f}·sinθ，吸一发 {k1:.3f}·sinθ N·m（θ 以装填位 0° 算）")

    print("\n摆动峰值（0° → ±48.4°）：")
    print(f"{'工况':>22} {'峰值力矩':>10} {'峰值角速度':>11} {'峰值角加速度':>13}")
    for on, tag in ((False, "空载"), (True, "吸一发飞镖")):
        for spd in (90, 170):
            t, w, a = swing(on, ARM_MAX, spd)
            print(f"{tag + f'，限速 {spd}°/s':>22} {t:8.3f} N·m {w:9.2f} rad/s {a:10.1f} rad/s²")

    # 固件里的摆速 kArmSpeed = 2 rad/s（约 115°/s），在 90~170°/s 两档之间；带镖取保守值
    peak = swing(True, ARM_MAX, 115)[0]
    for margin in (1.5, 2.0):
        need = peak * margin
        print(f"带镖、2 rad/s 摆速，峰值 {peak:.2f} N·m；{margin:.1f} 倍余量 → "
              f"{need:.2f} N·m，GM6020（0.741 N·m/A）{need / 0.741:.2f} A")


if __name__ == "__main__":
    main()
