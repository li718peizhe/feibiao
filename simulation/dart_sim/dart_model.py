"""把飞镖 STEP（SolidWorks 导出，6 个零件）转成仿真用的网格和质量属性：assets/dart_*.stl + dart.json。

只有转换时需要 gmsh（pip install gmsh==4.15.2）；生成的资源放在 assets/，仿真运行时不再读 STEP。
飞镖坐标系：原点在质心，x 指向镖头，z 指向磁吸铁片一侧，y = z × x。
"""
import json
import os

import numpy as np

from .cad import write_stl

ASSET_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets")
# 零件名 -> (网格组, 密度 kg/m³)。打印件按 PLA 估，铁片按钢
PARTS = {"镖体": ("body", 1240), "内嵌": ("body", 1240), "飞镖头": ("nose", 1240),
         "尾翼1改": ("fins", 1240), "尾翼2改": ("fins", 1240), "磁吸铁片": ("plate", 7870)}
COLORS = {"body": (0.95, 0.95, 0.95, 1), "nose": (0.85, 0.12, 0.12, 1),
          "fins": (0.2, 0.45, 0.85, 1), "plate": (0.35, 0.35, 0.38, 1)}


def _to_dart(p, com):
    """STEP 坐标 (mm) -> 飞镖坐标 (mm)：STEP 里镖头朝 -y、铁片朝 +z。"""
    q = np.asarray(p) - com
    return np.stack([-q[..., 1], q[..., 0], q[..., 2]], -1)


def convert(step_path, out_dir=ASSET_DIR, log=print):
    import gmsh
    gmsh.initialize()
    try:
        gmsh.option.setNumber("General.Terminal", 0)
        gmsh.option.setNumber("Geometry.OCCImportLabels", 1)
        gmsh.model.add("dart")
        gmsh.model.occ.importShapes(step_path)
        gmsh.model.occ.synchronize()
        solids = []
        for dim, tag in gmsh.model.getEntities(3):
            short = gmsh.model.getEntityName(dim, tag).split("/")[-1]
            group, rho = PARTS[short]
            vol = gmsh.model.occ.getMass(dim, tag) * 1e-9
            com = np.array(gmsh.model.occ.getCenterOfMass(dim, tag))
            I = np.array(gmsh.model.occ.getMatrixOfInertia(dim, tag)).reshape(3, 3) * 1e-15 * rho   # 质心处，单位密度 -> kg·m²
            solids.append(dict(tag=tag, name=short, group=group, mass=vol * rho, com=com, I=I))
        assert sorted(s["name"] for s in solids) == sorted(PARTS), "STEP 零件和预期不一致"
        mass = sum(s["mass"] for s in solids)
        com = sum(s["mass"] * s["com"] for s in solids) / mass
        R = np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1.0]])            # STEP -> 飞镖坐标的旋转
        I = np.zeros((3, 3))
        for s in solids:
            d = (s["com"] - com) * 1e-3
            I += s["I"] + s["mass"] * (d @ d * np.eye(3) - np.outer(d, d))
        I = R @ I @ R.T

        gmsh.option.setNumber("Mesh.MeshSizeFromCurvature", 12)
        gmsh.option.setNumber("Mesh.MeshSizeMax", 5.0)
        gmsh.option.setNumber("Mesh.MeshSizeMin", 0.8)
        gmsh.model.mesh.generate(2)
        groups = {}
        for s in solids:
            for _, surf in gmsh.model.getBoundary([(3, s["tag"])], oriented=False, recursive=False):
                nodes, coords, _ = gmsh.model.mesh.getNodes(2, abs(surf), includeBoundary=True)
                idx = {n: k for k, n in enumerate(nodes)}
                P = coords.reshape(-1, 3)
                for etype, enodes in zip(*gmsh.model.mesh.getElements(2, abs(surf))[::2]):
                    if etype == 2:
                        groups.setdefault(s["group"], []).append(P[[idx[n] for n in enodes]].reshape(-1, 3, 3))
    finally:
        gmsh.finalize()

    os.makedirs(out_dir, exist_ok=True)
    tris = {g: _to_dart(np.concatenate(t), com) for g, t in groups.items()}
    for g, t in tris.items():
        write_stl(os.path.join(out_dir, f"dart_{g}.stl"), t * 1e-3)
    allv = np.concatenate([t.reshape(-1, 3) for t in tris.values()])
    plate = tris["plate"].reshape(-1, 3)
    body = tris["body"].reshape(-1, 3)
    info = dict(step=step_path, mass=mass, inertia=I.tolist(), groups=sorted(tris), colors=COLORS,
                extent_min=allv.min(0).tolist(), extent_max=allv.max(0).tolist(),
                plate_centre=((plate.min(0) + plate.max(0)) / 2).tolist(), plate_top=float(plate[:, 2].max()),
                body_radius=float(np.percentile(np.hypot(body[:, 1], body[:, 2]), 90)))
    with open(os.path.join(out_dir, "dart.json"), "w", encoding="utf-8") as f:
        json.dump(info, f, ensure_ascii=False, indent=1)
    log(f"飞镖模型：{mass * 1e3:.0f} g，长 {np.ptp(allv[:, 0]):.1f} mm，{sum(len(t) for t in tris.values())} 个三角面 -> {out_dir}")
    return info


def load(out_dir=ASSET_DIR, step_path=None, log=print):
    path = os.path.join(out_dir, "dart.json")
    if not os.path.exists(path):
        if not step_path or not os.path.exists(step_path):
            raise FileNotFoundError(f"没有飞镖资源 {path}，也找不到飞镖 STEP：{step_path}")
        return convert(step_path, out_dir, log)
    with open(path, encoding="utf-8") as f:
        return json.load(f)
