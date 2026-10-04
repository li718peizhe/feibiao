"""MJCF 其余部分：资源、地面、飞镖、联动约束、皮筋、执行器。"""
import math

import numpy as np

from .mjcf import LIMIT, SOLIMP, SOLIMP_HARD, SOLREF, _v, build_xml, yaw_polycoef
from .params import MOTORS


def dart_body(i, p, dart):
    """飞镖：网格和质量属性来自 STEP（dart_model.py）。x 轴指向镖头，z 轴指向磁吸铁片。
    碰撞只用一根胶囊体（落地、出膛后的导轨接触），网格只用于显示。"""
    I = np.array(dart["inertia"])
    lo, hi = dart["extent_min"], dart["extent_max"]
    r = 0.9 * dart["body_radius"] * 1e-3
    fi = " ".join(f"{x:.6g}" for x in (I[0, 0], I[1, 1], I[2, 2], I[0, 1], I[0, 2], I[1, 2]))
    lines = [f'<body name="dart{i}" pos="{0.3 + 0.1 * i:.3f} -0.6 0.05">',
             f'  <freejoint name="dart{i}"/>',
             f'  <inertial pos="0 0 0" mass="{dart["mass"]:.6g}" fullinertia="{fi}"/>',
             f'  <geom name="dart{i}" type="capsule" fromto="{lo[0] * 1e-3 + r:.5f} 0 0 {hi[0] * 1e-3 - r:.5f} 0 0" size="{r:.5f}" '
             f'group="3" rgba="1 1 1 0.3" contype="2" conaffinity="1" friction="{p.dart_rail_friction} 0.005 0.0001"/>']
    lines += [f'  <geom type="mesh" mesh="dart_{k}" rgba="{_v(dart["colors"][k], 1)}" group="1"/>' for k in dart["groups"]]
    lines.append("</body>")
    return lines


def document(g, p, groups, inertials, dart):
    m3, my, mp = MOTORS["M3508"], MOTORS[p.yaw_motor], MOTORS[p.pitch_motor]
    tmax = lambda m: m.kt * m.i_max
    lines = ['<mujoco model="dart_launcher">',
             '  <compiler angle="radian" meshdir="meshes" inertiafromgeom="false" autolimits="true"/>',
             f'  <option timestep="{p.timestep}" integrator="implicitfast"/>',
             '  <visual>',
             '    <global offwidth="1600" offheight="1200" azimuth="-125" elevation="-18"/>',
             '    <quality shadowsize="4096"/>',
             '    <headlight ambient="0.42 0.42 0.42" diffuse="0.45 0.45 0.45" specular="0.05 0.05 0.05"/>',
             '    <map znear="0.005" zfar="80"/>',
             '  </visual>',
             f'  <default><geom contype="0" conaffinity="0"/><joint {LIMIT}/></default>',
             '  <asset>',
             '    <texture name="grid" type="2d" builtin="checker" width="512" height="512" rgb1="0.80 0.82 0.84" '
             'rgb2="0.73 0.75 0.78" mark="edge" markrgb="0.58 0.6 0.63"/>',
             '    <material name="floor" texture="grid" texrepeat="36 40" reflectance="0.03"/>',
             '    <texture name="sky" type="skybox" builtin="gradient" rgb1="0.72 0.82 0.94" rgb2="0.96 0.97 1" width="256" height="256"/>']
    lines += [f'    <mesh name="{m}" file="{m}.stl" inertia="shell"/>' for m, _, _ in groups]
    lines += [f'    <mesh name="dart_{k}" file="dart_{k}.stl" inertia="shell"/>' for k in dart["groups"]]
    lines += ['  </asset>', '  <worldbody>',
              '    <light name="sun" directional="true" pos="2 -3 6" dir="-0.3 0.5 -1" diffuse="0.55 0.55 0.55" castshadow="false"/>',
              '    <geom name="floor" type="plane" pos="0 14 0" size="16 18 0.1" material="floor" contype="1" conaffinity="2" '
              'friction="0.8 0.005 0.0001"/>',
              '    <camera name="overview" pos="1.5 -0.9 1.1" mode="targetbody" target="frame"/>',
              '    <camera name="side" pos="2.2 0.35 0.45" mode="targetbody" target="frame"/>']
    lines += [f'    <geom type="box" pos="0 {d} 0.0005" size="4 0.012 0.0005" rgba="0.85 0.3 0.2 0.9"/>' for d in range(5, 31, 5)]
    lines += build_xml(g, p, groups, inertials)
    for i in range(4):
        lines += ["    " + s for s in dart_body(i, p, dart)]
    lines.append('  </worldbody>')

    # 联动：传动比约束（右旋丝杆：绕轴正转螺母沿轴反向走）
    eq = ['  <equality>']
    jeq = lambda name, j1, j2, c: eq.append(
        f'    <joint name="{name}" joint1="{j1}" joint2="{j2}" polycoef="{" ".join(f"{x:.10g}" for x in c)}" '
        f'solref="{SOLREF}" solimp="{SOLIMP_HARD}"/>')
    ky, kp, r = p.yaw_lead / (2 * math.pi), p.pitch_lead / (2 * math.pi), p.belt_radius
    # 大传动比的联动（丝杆、同步带）用固定腱长度 = 0 表示：腱的等效惯量按雅可比算，约束不会被算软
    fixed = {"c_yaw_screw": ("yaw_slide", "yaw_screw", ky), "c_pitch_screw": ("pitch_slider", "pitch_screw", -kp),
             "c_belt_r": ("slider", "motor_r", -r), "c_belt_l": ("slider", "motor_l", r)}
    for name, (j1, j2, c) in fixed.items():
        eq.append(f'    <tendon name="{name}" tendon1="t_{name[2:]}" solref="{SOLREF}" solimp="{SOLIMP}"/>')
    jeq("c_yaw_pin", "frame", "yaw_slide", yaw_polycoef(g["yaw_pin_radius"] * 1e-3, p.yaw_slide_range))
    jeq("c_idler_r", "idler_r", "slider", [0, 1 / r, 0, 0, 0])
    jeq("c_idler_l", "idler_l", "slider", [0, -1 / r, 0, 0, 0])
    k = p.claw_open_deg / p.lock_open_deg
    jeq("c_claw_r", "claw_r", "lock", [0, k, 0, 0, 0])
    jeq("c_claw_l", "claw_l", "lock", [0, k, 0, 0, 0])
    rod_anchor = g["rod_pin"] - g["bodies"]["rod"]["origin"]
    eq.append(f'    <connect name="c_rod" body1="rod" body2="plunger" anchor="{_v(rod_anchor)}" solref="{SOLREF}" solimp="{SOLIMP_HARD}"/>')
    for i in range(4):
        for carrier in ("shuttle", "plunger", "frame"):
            eq.append(f'    <weld name="dart{i}_{carrier}" body1="{carrier}" body2="dart{i}" active="false" '
                      f'solref="0.002 1" solimp="{SOLIMP}"/>')
    eq.append('  </equality>')

    ten = ['  <tendon>']
    for name, (j1, j2, c) in fixed.items():
        ten += [f'    <fixed name="t_{name[2:]}">', f'      <joint joint="{j1}" coef="1"/>',
                f'      <joint joint="{j2}" coef="{c:.10g}"/>', '    </fixed>']
    # 一整根皮筋：pitch 滑块一端 → 上方滚轮 → 发射机构 → 另一侧滚轮 → pitch 滑块另一端（滚轮按无摩擦处理）
    ten += [f'    <spatial name="band" stiffness="{p.band_stiffness}" damping="{p.band_damping}" '
            f'springlength="0 {p.band_rest_length}" width="0.0025" rgba="0.95 0.72 0.15 1">']
    ten += [f'      <site site="{s}"/>' for s in ("band_anchor_l", "band_roller_l", "band_hook_l",
                                                 "band_hook_r", "band_roller_r", "band_anchor_r")]
    ten += ['    </spatial>']
    ten += [f'    <fixed name="push" range="{-g["push_gap"] * 1e-3:.6f} 10" {LIMIT}>',
            '      <joint joint="slider" coef="1"/>', '      <joint joint="shuttle" coef="-1"/>', '    </fixed>', '  </tendon>']

    lo, hi = (math.radians(a) for a in p.arm_range_deg)
    act = ['  <actuator>',
           f'    <motor name="motor_r" joint="motor_r" ctrlrange="{-tmax(m3):.4g} {tmax(m3):.4g}"/>',
           f'    <motor name="motor_l" joint="motor_l" ctrlrange="{-tmax(m3):.4g} {tmax(m3):.4g}"/>',
           f'    <motor name="yaw" joint="yaw_screw" ctrlrange="{-tmax(my):.4g} {tmax(my):.4g}"/>',
           f'    <motor name="pitch" joint="pitch_screw" ctrlrange="{-tmax(mp):.4g} {tmax(mp):.4g}"/>',
           f'    <position name="lock" joint="lock" kp="2" kv="0.03" ctrlrange="0 {math.radians(p.lock_open_deg):.5f}" forcerange="-0.6 0.6"/>',
           f'    <position name="arm" joint="arm" kp="40" kv="2" ctrlrange="{lo:.5f} {hi:.5f}" forcerange="-5 5"/>',
           f'    <position name="crank" joint="crank" kp="6" kv="0.12" ctrlrange="0 {math.pi:.5f}" forcerange="-1.2 1.2"/>',
           '  </actuator>']
    return "\n".join(lines + eq + ten + act + ['</mujoco>', ''])
