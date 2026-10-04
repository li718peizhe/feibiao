"""装弹几何（mm，导轨坐标 x, u, n），由 CAD 网格 + 飞镖网格算出。

- 磁吸：推杆末端磁铁座（part_433）下表面贴住飞镖铁片顶面，摆臂 0°、曲柄上止点（CAD 构型）
- 发射机构：镖体下面的长条凸起卡进推板（part_347）中间的凹槽，镖腹贴推板顶面，更低的凸块顶在推板前沿
- 装填：发射机构被滑块带到“磁铁正下方”（竖直方向）的位置 q_load，磁铁断电，飞镖竖直落下卡进推板
- 两侧镖座：凸起卡进镖座顶板（part_490 / part_482）的凹槽，飞镖沿导轨方向，镖尾架在后面的 ∧ 形支撑上；
  沿槽方向取不碰机架、又离磁铁最近的位置（CAD 里顶板没给凸块让位，凸块会压进顶板）
"""
import math

import numpy as np

MAGNET_PART = "part_433"                           # 推杆末端磁铁座，下表面是磁吸面
PUSH_PLATE = "part_347"                            # 发射机构推板
HOLDER_PARTS = {1: "part_490", -1: "part_482"}     # 右侧（+x）/ 左侧（-x）镖座顶板
VOX = 2.0


def _sample(tris, spacing=1.5, seed=0):
    """三角面上均匀撒点（含顶点），给体素占用检测用。"""
    rng = np.random.default_rng(seed)
    a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
    k = np.maximum(1, np.ceil(0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1) / spacing ** 2)).astype(int)
    i = np.repeat(np.arange(len(tris)), k)
    s, t = np.sqrt(rng.random(len(i))), rng.random(len(i))
    pts = (1 - s)[:, None] * a[i] + (s * (1 - t))[:, None] * b[i] + (s * t)[:, None] * c[i]
    return np.concatenate([pts, tris.reshape(-1, 3)])


def _keys(pts, vox=VOX):
    k = np.floor(pts / vox).astype(np.int64) + 4096          # 三个 13 位整数拼成一个键（±8 m / 2 mm 体素以内）
    return (k[:, 0] << 26) | (k[:, 1] << 13) | k[:, 2]


def _occ(pts, vox=VOX):
    return np.unique(_keys(pts, vox))


def _hits(pts, occ, vox=VOX):
    return int(np.isin(_keys(pts, vox), occ).sum())


def crank_for_drop(drop, r, l):
    """推杆下探 drop（mm）对应的曲柄角（0..π，单调，二分）。"""
    f = lambda th: (r - l) - r * math.cos(th) + math.sqrt(l * l - (r * math.sin(th)) ** 2)
    lo, hi = 0.0, math.pi
    assert 0 <= drop <= f(hi), f"下探 {drop:.1f} mm 超出推杆行程 {f(hi):.1f} mm"
    for _ in range(60):
        mid = (lo + hi) / 2
        lo, hi = (mid, hi) if f(mid) < drop else (lo, mid)
    return (lo + hi) / 2


def dart_features(body):
    """镖体网格（飞镖坐标 mm，x 指向镖头、z 指向铁片）-> 镖腹、凸起、凸块的位置。"""
    mid = (body[:, 0] > -12) & (body[:, 0] < 26) & (np.abs(body[:, 1]) > 3) & (np.abs(body[:, 1]) < 9)
    belly = -body[mid, 2].min()                              # 镖腹最低点（凸起两侧）
    lug = body[(body[:, 2] < -belly - 3) & (np.abs(body[:, 1]) < 9)]
    ridge = body[(body[:, 2] < -belly - 0.8) & (body[:, 2] > -belly - 3) & (np.abs(body[:, 1]) < 2)]
    return {"belly": float(belly), "lug_rear_x": float(lug[:, 0].min()), "lug_front_x": float(lug[:, 0].max()),
            "lug_bottom": float(-lug[:, 2].min()), "ridge_x": [float(ridge[:, 0].min()), float(ridge[:, 0].max())]}


def layout(ex, dart_tris, dart_info, body_tris, frame_tris):
    """ex: cad.Export；dart_tris: 飞镖全部网格；body_tris: 镖体网格（飞镖坐标 mm）；frame_tris: 机架网格（CAD mm）。"""
    R = np.stack([ex.X, ex.U, ex.N])
    rail = lambda p: p.reshape(-1, 3) @ R.T
    feat = dart_features(body_tris.reshape(-1, 3))
    mag = rail(ex.triangles([MAGNET_PART]))
    face, mu = mag[:, 2].min(), (mag[:, 1].min() + mag[:, 1].max()) / 2
    plate_x, plate_top = dart_info["plate_centre"][0], dart_info["plate_top"]
    held = np.array([0.0, mu - plate_x, face - plate_top])
    push = rail(ex.triangles([PUSH_PLATE]))
    seat = np.array([0.0, push[:, 1].max() - feat["lug_rear_x"], push[:, 2].max() + feat["belly"]])
    up = np.array([0.0, 0.0, 1.0]) @ R.T                     # 竖直向上在导轨坐标里的分量
    fall_n = held[2] - seat[2]
    land_u = held[1] - fall_n * up[1] / up[2]                # 竖直落下，沿导轨也会往下走一段
    piv = rail(ex.joint_origin("旋转_joint"))[0]
    radius = piv[1] - mu
    out = {"magnet_face_n": float(face), "magnet_u": float(mu), "held_com": held.tolist(), "features": feat,
           "seat_com_q0": seat.tolist(), "q_load": float(land_u - seat[1]), "fall_height": float(fall_n / up[2]),
           "holders": {}}
    d = np.concatenate([_sample(t) for t in dart_tris])
    d_rail = np.stack([-d[:, 1], d[:, 0], d[:, 2]], -1)
    in_lug = ((d[:, 0] > feat["lug_rear_x"] - 0.5) & (d[:, 0] < feat["lug_front_x"] + 2.5) & (d[:, 2] < -feat["belly"] + 0.3))
    no_lug = d_rail[~in_lug]                                     # 凸块另算（镖座顶板上没有给它让位）
    found = {}
    for side, pid in HOLDER_PARTS.items():
        h = rail(_sample(ex.triangles([pid]), spacing=0.5))
        top = h[:, 2].max()
        groove = h[(h[:, 2] < top - 1.0) & (h[:, 2] > top - 3.0)]
        x = float(np.median(groove[:, 0]))                   # 凹槽中线
        fine = 0.6
        occ = {}
        for q in ex.links["yaw"]["part_ids"]:
            raw = rail(ex.triangles([q]))
            lo_, hi_ = raw.min(0), raw.max(0)
            if hi_[0] < x - 60 or lo_[0] > x + 60 or hi_[2] < top - 60 or lo_[2] > top + 90:
                continue
            pts = rail(_sample(ex.triangles([q]), spacing=0.5))
            keep = (np.abs(pts[:, 0] - x) < 60) & (pts[:, 2] > top - 60) & (pts[:, 2] < top + 90)
            if keep.any():
                occ[q] = _occ(pts[keep], fine)
        plate_dist = lambda u, x=x: abs(math.hypot(x, u + plate_x - piv[1]) - radius)   # 铁片离磁铁圆的距离
        blockers = lambda u, x=x, top=top, occ=occ: [q for q, o in occ.items()
                                                     if _hits(no_lug + (x, u, top + feat["belly"] + 1.2), o, fine)]
        merged = np.unique(np.concatenate(list(occ.values())))
        free = lambda u, x=x, top=top: _hits(no_lug + (x, u, top + feat["belly"] + 1.2), merged, fine) == 0
        # 凸起卡进凹槽（镖腹贴顶板、镖尾搭在后面的 ∧ 形支撑上），沿槽找不碰机架、又离磁铁最近的位置。抬高 1.2 mm 检查，贴合面不算
        u = next((u for u in sorted(np.arange(40.0, 200.0, 0.5), key=plate_dist) if free(u)), None)
        found[side] = dict(pid=pid, x=x, top=top, u=u, h=h, blockers=blockers)
    for side, f in found.items():
        other = found[-side]["u"]
        u = f["u"] if f["u"] is not None else other          # 这一侧放不下就按另一侧镜像放，并记下挡住的零件
        if u is None:
            raise RuntimeError("两侧镖座都找不到不和机架干涉的放镖位置")
        com = np.array([f["x"], u, f["top"] + feat["belly"]])
        v = com[:2] + (0, plate_x) - piv[:2]
        th = math.atan2(v[0], -v[1])
        lug = d_rail[in_lug] + com
        out["holders"][side] = {"arm_deg": math.degrees(th), "drop": float(face - (com[2] + plate_top)), "com": com.tolist(),
                                "magnet_gap_mm": float(abs(math.hypot(com[0], com[1] + plate_x - piv[1]) - radius)),
                                "lug_in_plate_mm": float(max(0.0, f["top"] - lug[:, 2].min()) if (lug[:, 1] < f["h"][:, 1].max()).any() else 0.0),
                                "blocked_by": [] if f["u"] is not None else f["blockers"](u)}

    fp = rail(_sample(frame_tris, spacing=2.0))
    keep = (fp[:, 2] > -150) & (fp[:, 2] < 90) & (fp[:, 1] > -200) & (fp[:, 1] < 430) & (np.abs(fp[:, 0]) < 280)
    frame = _occ(fp[keep])
    launch = _occ(np.concatenate([d_rail + seat + (0, du, 0) for du in range(0, 420, 4)]))
    holders = _occ(np.concatenate([d_rail + h["com"] for h in out["holders"].values()]))
    mc0 = np.array([0.0, mu, face])

    def held_at(th):     # 摆臂转 th、推杆上止点时吸着那发的质心（飞镖保持和导轨平行）
        q = mc0 - piv
        c, s_ = math.cos(th), math.sin(th)
        return np.array([q[0] * c - q[1] * s_, q[0] * s_ + q[1] * c, q[2]]) + piv + (held - mc0)

    out["launch_clear"] = _hits(d_rail + held, launch) == 0
    out["standby_deg"] = {}              # 发射前吸着那发要摆开：每侧找最小的、不挡发射路径也不碰机架/镖座的摆角
    for sg in (1, -1):
        out["standby_deg"][sg] = 0.0
        if out["launch_clear"]:
            continue
        for deg in np.arange(3.0, 46.0, 1.0):
            if all(_hits(d_rail + held_at(math.radians(sg * deg)), o) == 0 for o in (launch, frame, holders)):
                out["standby_deg"][sg] = float(sg * (deg + 3.0))
                break
    for side, h in out["holders"].items():
        th = math.radians(h["arm_deg"])
        h["swing_frame_hits"] = max(_hits(d_rail + held_at(th * k / 12), frame) for k in range(13))
        h.pop("h", None)
    return out
