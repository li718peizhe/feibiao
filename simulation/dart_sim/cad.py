"""读取 CAD 的 URDF 导出：零件网格、刚体归属（含孤立零件修正）、导轨坐标系、质量属性。

导出的 STL 顶点是 CAD 绝对坐标（mm），所以按刚体合并只需拼接，再平移到刚体原点。
"""
import json
import os
import struct

import numpy as np

DENSITY = 1200.0   # 导出给大部分零件用的密度 kg/m³，孤立零件按它补质量

# 网格分组：(网格名, 所属刚体, 颜色, 导出连杆)。一个刚体可以有几块不同颜色的网格。
GROUPS = [
    ("base", "base", (0.78, 0.79, 0.81, 1), ["base_link"]),
    ("yaw_screw", "yaw_screw", (0.30, 0.30, 0.33, 1), ["yaw2006"]),
    ("yaw_slide", "yaw_slide", (0.20, 0.62, 0.35, 1), ["yaw滑台"]),
    ("frame", "frame", (0.42, 0.55, 0.78, 1), ["yaw"]),
    ("belts", "frame", (0.08, 0.08, 0.08, 1), ["右履带", "左履带"]),
    ("motor_r", "motor_r", (0.22, 0.22, 0.24, 1), ["右3508"]),
    ("motor_l", "motor_l", (0.22, 0.22, 0.24, 1), ["左3508"]),
    ("idler_r", "idler_r", (0.85, 0.85, 0.88, 1), ["右齿轮"]),
    ("idler_l", "idler_l", (0.85, 0.85, 0.88, 1), ["左齿轮"]),
    ("slider", "slider", (0.10, 0.70, 0.40, 1), ["滑块"]),
    ("shuttle", "shuttle", (0.85, 0.15, 0.45, 1), ["发射机构"]),
    ("pitch_screw", "pitch_screw", (0.30, 0.30, 0.33, 1), ["pitch2006"]),
    ("pitch_slider", "pitch_slider", (0.16, 0.30, 0.66, 1), ["pitch滑块"]),
    ("lock", "lock", (0.95, 0.55, 0.10, 1), ["锁止机构"]),
    ("claw_r", "claw_r", (0.15, 0.85, 0.90, 1), ["右夹爪"]),
    ("claw_l", "claw_l", (0.15, 0.85, 0.90, 1), ["左夹爪"]),
    ("arm", "arm", (0.95, 0.60, 0.15, 1), ["旋转"]),
    ("crank", "crank", (0.60, 0.25, 0.80, 1), ["连杆"]),
    ("rod", "rod", (0.15, 0.75, 0.80, 1), ["连杆2"]),
    ("plunger", "plunger", (0.92, 0.85, 0.15, 1), ["换弹机构"]),
]

# 导出时没归到任何连杆的零件：按表面接触判断归属，没列出的都并入底座（底框、电控盒、电路板等）
ORPHANS = {
    "part_005": "base",                                   # yaw 丝杆轴承
    "part_071": "yaw_screw", "part_072": "yaw_screw",     # 联轴器
    "part_078": "yaw_screw", "part_080": "yaw_screw",
    "part_070": "yaw_slide",                              # 滑台上的转盘
    "part_231": "frame", "part_450": "frame",             # 带轮垫片、摆臂轴卡环
    "part_452": "frame", "part_459": "frame",             # 摆臂限位开关零件
    "part_240": "slider", "part_241": "slider",           # 导轨上第二个 MGN9 滑块，随 3508 滑块走
    "part_242": "slider", "part_243": "slider",
    "part_326": "pitch_slider",
    "part_428": "arm", "part_430": "arm",                 # 推杆直线轴承
}

_STL = np.dtype([("n", "<3f4"), ("v", "<9f4"), ("a", "<u2")])


def read_stl(path):
    """二进制 STL -> (n, 3, 3) 三角形顶点（mm）。"""
    with open(path, "rb") as f:
        data = f.read()
    n = struct.unpack("<I", data[80:84])[0]
    return np.frombuffer(data[84:84 + 50 * n], dtype=_STL)["v"].reshape(-1, 3, 3).astype(np.float64)


def write_stl(path, tris):
    rec = np.zeros(len(tris), dtype=_STL)
    rec["v"] = tris.reshape(-1, 9).astype("<f4")
    with open(path, "wb") as f:
        f.write(b"dart_sim".ljust(80, b"\0"))
        f.write(struct.pack("<I", len(tris)))
        f.write(rec.tobytes())


class Export:
    """URDF 导出目录里的 parts.json / user_model.json / meshes。"""

    def __init__(self, export_dir):
        self.dir = export_dir
        with open(os.path.join(export_dir, "parts.json"), encoding="utf-8") as f:
            self.parts = {p["part_id"]: p for p in json.load(f)["parts"]}
        with open(os.path.join(export_dir, "user_model.json"), encoding="utf-8") as f:
            um = json.load(f)
        self.links = {l["name"]: l for l in um["links"]}
        self.joints = {j["name"]: j for j in um["joints"]}
        # 导轨坐标系：N 取摆臂转轴（导轨法向，朝上），U = N × X 沿导轨向上
        self.X = np.array([1.0, 0.0, 0.0])
        n = np.array(self.joints["旋转_joint"]["axis"], float)
        self.N = n / np.linalg.norm(n)
        self.U = np.cross(self.N, self.X)
        self.floor_z = min(p["bbox"]["min"][2] for p in self.parts.values())   # 底框最低点 mm

    def rail(self, x, u, n):
        """导轨坐标 (mm) -> CAD 坐标 (mm)。"""
        return x * self.X + u * self.U + n * self.N

    def joint_origin(self, name):
        return np.array(self.joints[name]["origin"]["xyz"], float)

    def group_parts(self):
        """网格名 -> 零件列表，检查每个零件只用一次。"""
        out = {g[0]: [p for link in g[3] for p in self.links[link]["part_ids"]] for g in GROUPS}
        used = {p for ps in out.values() for p in ps}
        for pid in sorted(self.parts):
            if pid not in used:
                out[ORPHANS.get(pid, "base")].append(pid)
        flat = [p for ps in out.values() for p in ps]
        assert len(flat) == len(set(flat)) == len(self.parts), "零件归属有重复或遗漏"
        return out

    def triangles(self, part_ids):
        return np.concatenate([read_stl(os.path.join(self.dir, self.parts[p]["mesh_file"])) for p in part_ids])

    def mass_properties(self, links, extra_parts):
        """合并若干导出连杆 + 孤立零件（按体积×密度当质点）-> (质量, 质心 mm, 质心处惯量 3x3)。"""
        items = []
        for name in links:
            mp = self.links[name]["mass_properties"]
            i = mp["inertia_com_cad"]
            I = np.array([[i["ixx"], i["ixy"], i["ixz"]], [i["ixy"], i["iyy"], i["iyz"]], [i["ixz"], i["iyz"], i["izz"]]])
            items.append((mp["mass"], np.array(mp["com_xyz_cad"], float), I))
        for pid in extra_parts:
            t = self.triangles([pid])
            vol6 = np.einsum("ij,ij->i", t[:, 0], np.cross(t[:, 1], t[:, 2]))
            vol = abs(vol6.sum()) / 6.0
            com = (vol6[:, None] * t.sum(1)).sum(0) / (4.0 * vol6.sum()) if vol > 1e-9 else t.reshape(-1, 3).mean(0)
            items.append((vol * 1e-9 * DENSITY, com, np.zeros((3, 3))))
        m = sum(it[0] for it in items)
        c = sum(it[0] * it[1] for it in items) / m
        I = np.zeros((3, 3))
        for mi, ci, Ii in items:
            d = (ci - c) * 1e-3
            I += Ii + mi * (np.dot(d, d) * np.eye(3) - np.outer(d, d))
        return m, c, I
