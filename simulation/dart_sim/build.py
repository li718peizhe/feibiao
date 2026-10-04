"""从 URDF 导出生成仿真模型（model/dart_launcher.xml + meshes/ + meta.json）。"""
import json
import math
import os
import time

import shutil

import numpy as np

from . import cad, dart_model, geometry, loader_geometry, mjcf_parts
from .params import MODEL_DIR, Params

BUILD_VERSION = 2
XML_NAME = "dart_launcher.xml"
MAX_FACES = 190000


def build(p=None, out_dir=MODEL_DIR, log=print):
    p = p or Params()
    t0 = time.time()
    ex = cad.Export(p.export_dir)
    parts = ex.group_parts()
    mesh_dir = os.path.join(out_dir, "meshes")
    os.makedirs(mesh_dir, exist_ok=True)
    tris = {name: ex.triangles(ids) for name, ids in parts.items()}
    g = geometry.derive(ex, tris)
    dart = dart_model.load(step_path=p.dart_step, log=log)
    dart_tris = {k: cad.read_stl(os.path.join(dart_model.ASSET_DIR, f"dart_{k}.stl")) * 1e3 for k in dart["groups"]}
    g["loader"] = lay = loader_geometry.layout(ex, list(dart_tris.values()), dart, dart_tris["body"], tris["frame"])
    if not lay["launch_clear"]:
        log(f"磁铁上吸着的那发在 0° 会挡住发射路径，发射前摆到待命角 {lay['standby_deg'][1]:+.0f}° / {lay['standby_deg'][-1]:+.0f}°")
    for side, h in lay["holders"].items():
        if h["magnet_gap_mm"] > 3:
            log(f"注意：{'右' if int(side) > 0 else '左'}侧镖座上的飞镖铁片离磁铁能到的最近点还差 {h['magnet_gap_mm']:.1f} mm")
        if h["blocked_by"]:
            log(f"注意：{'右' if int(side) > 0 else '左'}侧镖座按另一侧镜像放镖，和 {', '.join(h['blocked_by'])} 干涉")
        if h["lug_in_plate_mm"] > 0.5:
            log(f"注意：{'右' if int(side) > 0 else '左'}侧镖座顶板没有给凸块让位，凸块压进顶板 {h['lug_in_plate_mm']:.1f} mm")

    for old in os.listdir(mesh_dir):
        os.remove(os.path.join(mesh_dir, old))
    groups = []
    for name, body, rgba, _ in cad.GROUPS:
        local = (tris[name] - g["bodies"][body]["origin"]) * 1e-3
        chunks = max(1, -(-len(local) // MAX_FACES))      # MuJoCo 的 STL 解码器每个文件最多 20 万面
        for k, part in enumerate(np.array_split(local, chunks)):
            mesh = name if chunks == 1 else f"{name}_{k}"
            cad.write_stl(os.path.join(mesh_dir, f"{mesh}.stl"), part)
            groups.append((mesh, body, rgba))
    for k in dart["groups"]:
        shutil.copy(os.path.join(dart_model.ASSET_DIR, f"dart_{k}.stl"), mesh_dir)

    in_links = {pid for l in ex.links.values() for pid in l["part_ids"]}
    inertials = {}
    for body in g["bodies"]:
        names = [gr for gr in cad.GROUPS if gr[1] == body]
        links = [l for gr in names for l in gr[3]]
        extra = [pid for gr in names for pid in parts[gr[0]] if pid not in in_links]
        m, com, I = ex.mass_properties(links, extra)
        if body in p.mass_override:
            k = p.mass_override[body] / m
            m, I = m * k, I * k
        inertials[body] = (m, com, I)

    xml = mjcf_parts.document(g, p, groups, inertials, dart)
    with open(os.path.join(out_dir, XML_NAME), "w", encoding="utf-8") as f:
        f.write(xml)
    meta = {
        "version": BUILD_VERSION, "export_dir": p.export_dir, "parts": len(ex.parts),
        "rail_angle_deg": math.degrees(math.atan2(g["U"][2], g["U"][1])),
        "push_gap_mm": g["push_gap"], "yaw_pin_radius_mm": g["yaw_pin_radius"],
        "crank_r_mm": g["crank_r"], "rod_l_mm": g["rod_l"], "loader": g["loader"],
        "dart": {"step": dart["step"], "mass": dart["mass"], "length_mm": dart["extent_max"][0] - dart["extent_min"][0]},
        "masses": {b: v[0] for b, v in inertials.items()},
        "orphans": {pid: grp for grp, ids in parts.items() for pid in ids if pid not in in_links},
    }
    with open(os.path.join(out_dir, "meta.json"), "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=1)
    log(f"模型已生成：{os.path.join(out_dir, XML_NAME)}（{len(ex.parts)} 个零件 -> {len(groups)} 块网格，"
        f"用时 {time.time() - t0:.1f} s）")
    return os.path.join(out_dir, XML_NAME)
