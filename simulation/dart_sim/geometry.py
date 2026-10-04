"""从 CAD 网格推出建模要用的几何量（mm，CAD 坐标）。

关节锚点来自导出的关节原点；接触面、皮筋挂点、飞镖落位从零件网格量出来。
"""
import numpy as np

# 刚体 -> (父刚体, 关节类型, 导出关节名)。两处改挂：yaw_slide 骑在底座丝杆上，plunger 在摆臂导轨上滑
TREE = [
    ("yaw_screw", "base", "hinge", "yaw2006_joint"),
    ("yaw_slide", "base", "slide", "yaw滑台_joint"),
    ("frame", "base", "hinge", "yaw_joint"),
    ("motor_r", "frame", "hinge", "右3508_joint"),
    ("motor_l", "frame", "hinge", "左3508_joint"),
    ("idler_r", "frame", "hinge", "右齿轮_joint"),
    ("idler_l", "frame", "hinge", "左齿轮_joint"),
    ("slider", "frame", "slide", "滑块_joint"),
    ("shuttle", "frame", "slide", "发射机构_joint"),
    ("pitch_screw", "frame", "hinge", "pitch2006_joint"),
    ("pitch_slider", "frame", "slide", "pitch滑块_joint"),
    ("lock", "frame", "hinge", "锁止机构_joint"),
    ("claw_r", "frame", "hinge", "右夹爪_joint"),
    ("claw_l", "frame", "hinge", "左夹爪_joint"),
    ("arm", "frame", "hinge", "旋转_joint"),
    ("crank", "arm", "hinge", "连杆_joint"),
    ("rod", "crank", "hinge", "连杆2_joint"),
    ("plunger", "arm", "slide", "换弹机构_joint"),
]

# 从零件形状量出来的挂点（导轨坐标 x, u, n，mm）
BAND_ANCHOR = (74.0, 151.8, -161.7)    # pitch 滑块两侧皮筋夹板（part_313/317）中间
BAND_HOOK = (14.0, -49.15, -162.3)     # 发射机构前排立柱（part_327..337）
ROLLER_PARTS = ("part_355", "part_357")  # 上方机架两侧的皮筋滚轮（+x 侧、-x 侧），皮筋绕过滚轮靠上的一侧
RAIL_TOP_PART = "part_204"             # 飞镖下面那根 2020 型材
YAW_PIN_PART = "part_070"              # yaw 滑台上的转盘，机架的销轴在它中心


def snap(axis, basis):
    """把导出轴吸附到最近的基向量（导出的滑块轴有 0.5° 偏差）。"""
    a = np.asarray(axis, float) / np.linalg.norm(axis)
    best = max(((abs(a @ b), np.sign(a @ b) * b) for b in basis), key=lambda t: t[0])
    assert best[0] > np.cos(np.radians(1.0)), f"关节轴 {axis} 偏离坐标轴超过 1°"
    return best[1]


def derive(ex, tris):
    """ex: cad.Export；tris: 网格名 -> 三角形 (mm)。返回几何量 dict（mm / 单位向量）。"""
    X, U, N = ex.X, ex.U, ex.N
    basis = [X, U, N, np.array([0.0, 0.0, 1.0])]
    to_rail = np.stack([X, U, N])
    g = {"X": X, "U": U, "N": N, "floor_z": ex.floor_z, "bodies": {}}
    for body, parent, jtype, jname in TREE:
        origin = ex.joint_origin(jname)
        axis = snap(ex.joints[jname]["axis"], basis)
        if body == "yaw_slide":       # 改挂到底座：原点放在丝杆轴线 x=0 处，轴取 +X
            o = ex.joint_origin("yaw2006_joint")
            origin, axis = np.array([0.0, o[1], o[2]]), X
        g["bodies"][body] = {"parent": parent, "type": jtype, "origin": origin, "axis": axis}
    g["bodies"]["base"] = {"parent": None, "type": None, "origin": np.zeros(3), "axis": None}

    def rail_pts(name):
        return tris[name].reshape(-1, 3) @ to_rail.T

    # 3508 滑块下表面碰发射机构：两者沿 U 的初始间隙
    sl, sh = rail_pts("slider"), rail_pts("shuttle")
    n_lo, n_hi, x_hi = sl[:, 2].min(), sl[:, 2].max(), np.abs(sl[:, 0]).max()
    m = (sh[:, 2] > n_lo + 0.1) & (sh[:, 2] < n_hi) & (np.abs(sh[:, 0]) < x_hi)
    g["push_gap"] = sl[:, 1].min() - sh[m, 1].max()

    # 飞镖：躺在 2020 型材顶面，尾部顶住发射机构推板
    rail = ex.triangles([RAIL_TOP_PART]).reshape(-1, 3) @ to_rail.T
    g["rail_box"] = (rail.min(0), rail.max(0))
    g["dart_n"] = rail[:, 2].max()            # 型材顶面，加飞镖半径就是轴线高度

    pin = ex.triangles([YAW_PIN_PART]).reshape(-1, 3)
    piv = ex.joint_origin("yaw_joint")
    g["yaw_pin_radius"] = float(np.linalg.norm(((pin.min(0) + pin.max(0)) / 2 - piv)[:2]))

    # 曲柄滑块：曲柄半径 r、连杆长 l（垂直于曲柄轴 U 的平面内）
    c0, c1, c2 = (ex.joint_origin(j) for j in ("连杆_joint", "连杆2_joint", "换弹机构_joint"))
    perp = lambda v: v - (v @ U) * U
    g["crank_r"], g["rod_l"] = float(np.linalg.norm(perp(c1 - c0))), float(np.linalg.norm(perp(c2 - c1)))
    g["rod_pin"] = c2
    g["band_anchor"] = [ex.rail(sx * BAND_ANCHOR[0], *BAND_ANCHOR[1:]) for sx in (1, -1)]
    g["band_hook"] = [ex.rail(sx * BAND_HOOK[0], *BAND_HOOK[1:]) for sx in (1, -1)]
    g["band_roller"] = []
    for pid in ROLLER_PARTS:
        v = ex.triangles([pid]).reshape(-1, 3) @ to_rail.T
        lo, hi = v.min(0), v.max(0)
        c = (lo + hi) / 2
        g["band_roller"].append(ex.rail(c[0], hi[1], c[2]))     # 滚轮朝导轨上方的切点
    return g


def crank_drop(theta, r, l):
    """推杆从上止点下降的距离（mm），theta 为曲柄角（0 = 上止点）。"""
    return (r - l) - r * np.cos(theta) + np.sqrt(l * l - (r * np.sin(theta)) ** 2)
