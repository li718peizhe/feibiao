"""生成 MJCF：刚体树、联动约束、皮筋、执行器、飞镖。

所有刚体姿态都和 CAD 坐标轴一致（只平移），网格顶点写成刚体局部坐标，关节轴直接用 CAD 方向。
"""
import numpy as np
import mujoco

from . import geometry
from .params import MOTORS

SOLREF, SOLIMP = "0.001 1", "0.95 0.99 0.001"
# 联动约束用接近硬的阻抗：MuJoCo 估算关节等式约束的等效惯量时直接把两个关节的逆惯量相加、不乘传动比，
# 丝杆这种 1:1500 的联动会被算软几百倍（实测默认阻抗下 232 N 时打滑 3 mm）
SOLIMP_HARD = "0.9999 0.9999 0.001"
LIMIT = 'solreflimit="0.002 1" solimplimit="0.99 0.999 0.0005"'


def _v(a, scale=1e-3):
    return " ".join(f"{x * scale:.7g}" for x in a)


def _quat(R):
    q = np.zeros(4)
    mujoco.mju_mat2Quat(q, np.asarray(R, float).reshape(9))
    return q


def joint_settings(p):
    arm = lambda name: MOTORS[name].rotor_inertia * MOTORS[name].ratio ** 2   # 转子惯量折算到输出轴
    r = np.radians
    return {
        "yaw_screw": dict(armature=arm(p.yaw_motor), damping=2e-4, frictionloss=0.005),
        "yaw_slide": dict(range=p.yaw_slide_range, damping=5, frictionloss=2),
        "frame": dict(damping=0.5, frictionloss=0.1),
        "motor_r": dict(armature=arm("M3508"), damping=1e-3, frictionloss=0.02),
        "motor_l": dict(armature=arm("M3508"), damping=1e-3, frictionloss=0.02),
        "idler_r": dict(armature=1e-6), "idler_l": dict(armature=1e-6),
        "slider": dict(range=(-0.33, p.slider_top), damping=1, frictionloss=2),
        "shuttle": dict(range=(-0.008, p.shuttle_top), damping=0.3, frictionloss=1),
        "pitch_screw": dict(armature=arm(p.pitch_motor), damping=2e-4, frictionloss=0.005),
        "pitch_slider": dict(range=p.pitch_range, damping=5, frictionloss=2),
        "lock": dict(range=(0, r(p.lock_open_deg)), armature=1e-4, damping=0.01),
        "claw_r": dict(range=(0, r(p.claw_open_deg)), armature=1e-6),
        "claw_l": dict(range=(0, r(p.claw_open_deg)), armature=1e-6),
        "arm": dict(range=tuple(r(a) for a in p.arm_range_deg), armature=2e-3, damping=0.05),
        "crank": dict(range=(r(-10), r(190)), armature=1e-4, damping=5e-3),
        "rod": dict(armature=1e-6),
        "plunger": dict(range=(-0.005, 0.125), damping=0.5, frictionloss=0.3),
    }


def yaw_polycoef(radius_m, s_range):
    """机架转角 ψ = -asin(s / R)（销轴在机架上、滑台横移 s），在行程内拟合四次多项式。"""
    s = np.linspace(s_range[0] - 0.005, s_range[1] + 0.005, 200)
    c = np.polyfit(s, -np.arcsin(s / radius_m), 4)[::-1]
    return c


def build_xml(g, p, groups, inertials):
    """g: geometry.derive 的结果；groups: [(网格名, 刚体, rgba)]；inertials: 刚体 -> (m, com mm, I)。"""
    B = g["bodies"]
    children = {}
    for name, b in B.items():
        children.setdefault(b["parent"], []).append(name)
    js = joint_settings(p)
    out = []

    def body(name, depth):
        b = B[name]
        pad = "  " * depth
        rel = b["origin"] - (B[b["parent"]]["origin"] if b["parent"] else np.array([0, 0, g["floor_z"]]))
        out.append(f'{pad}<body name="{name}" pos="{_v(rel)}">')
        m, com, I = inertials[name]
        fi = (I[0, 0], I[1, 1], I[2, 2], I[0, 1], I[0, 2], I[1, 2])
        out.append(f'{pad}  <inertial pos="{_v(com - b["origin"])}" mass="{m:.6g}" fullinertia="{_v(fi, 1)}"/>')
        if b["type"]:
            attrs = " ".join(f'{k}="{_v(v, 1) if isinstance(v, tuple) else f"{v:.6g}"}"' for k, v in js[name].items())
            out.append(f'{pad}  <joint name="{name}" type="{b["type"]}" axis="{_v(b["axis"], 1)}" {attrs}/>')
        for mesh, owner, rgba in groups:
            if owner == name:
                out.append(f'{pad}  <geom name="{mesh}" type="mesh" mesh="{mesh}" rgba="{_v(rgba, 1)}" group="1"/>')
        out.extend(f"{pad}  {line}" for line in extras(name, g, p, b["origin"]))
        for c in children.get(name, []):
            body(c, depth + 1)
        out.append(f"{pad}</body>")

    body("base", 2)
    return out


def extras(name, g, p, origin):
    """刚体上的额外几何：飞镖导轨接触盒、出口点、皮筋挂点、磁吸面。"""
    X, U, N = g["X"], g["U"], g["N"]
    lines = []
    if name == "frame":
        lo, hi = g["rail_box"]
        c = (lo + hi) / 2
        centre = c[0] * X + c[1] * U + c[2] * N
        q = _quat(np.stack([X, U, N], axis=1))
        lines.append(f'<geom name="rail_top" type="box" pos="{_v(centre - origin)}" quat="{_v(q, 1)}" '
                     f'size="{_v((hi - lo) / 2)}" group="3" rgba="0.2 0.8 0.2 0.3" contype="1" conaffinity="2" '
                     f'friction="{p.dart_rail_friction} 0.005 0.0001"/>')
        lines.append(f'<site name="rail_exit" pos="{_v(hi[1] * U + g["dart_n"] * N - origin)}" size="0.004"/>')
        for side, r in zip("rl", g["band_roller"]):
            lines.append(f'<site name="band_roller_{side}" pos="{_v(r - origin)}" size="0.004" rgba="1 0.8 0.2 1"/>')
    if name == "pitch_slider":
        for side, a in zip("rl", g["band_anchor"]):
            lines.append(f'<site name="band_anchor_{side}" pos="{_v(a - origin)}" size="0.004" rgba="1 0.8 0.2 1"/>')
    if name == "shuttle":
        for side, h in zip("rl", g["band_hook"]):
            lines.append(f'<site name="band_hook_{side}" pos="{_v(h - origin)}" size="0.004" rgba="1 0.8 0.2 1"/>')
    # 飞镖位姿统一用：x 沿 U（镖头朝导轨上方）、z 沿 N（铁片朝上，凸起朝下）
    lay, q = g.get("loader"), _quat(np.stack([U, -X, N], axis=1))
    rail_pt = lambda r: r[0] * X + r[1] * U + r[2] * N
    site = lambda nm, r, rgba="1 0 0 0.5": f'<site name="{nm}" pos="{_v(rail_pt(r) - origin)}" quat="{_v(q, 1)}" size="0.003" rgba="{rgba}"/>'
    if lay and name == "plunger":
        lines.append(site("magnet", lay["held_com"]))                                    # 磁铁吸住的飞镖质心
        lines.append(site("magnet_centre", (0.0, lay["magnet_u"], lay["magnet_face_n"]), "1 1 0 0.6"))
    if lay and name == "shuttle":
        lines.append(site("dart_seat", lay["seat_com_q0"]))                              # 凸起进推板凹槽、凸块顶推板前沿
    if lay and name == "frame":
        for side, h in lay["holders"].items():
            lines.append(site(f"holder_{'r' if int(side) > 0 else 'l'}", h["com"]))       # 两侧镖座上的飞镖
    return lines
