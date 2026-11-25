"""Summarize a 3p arm SWD trace: stick-driven moves, stick-slip stops, release coast, hold drift, crank pushes."""
import csv
import math
import sys
from pathlib import Path


def load(path):
    rows = []
    with open(path, encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            try:
                rows.append({key: float(value) for key, value in row.items()})
            except ValueError:
                continue
    rows.sort(key=lambda r: r["tick"])
    unique = []
    for row in rows:
        if not unique or row["tick"] > unique[-1]["tick"]:
            unique.append(row)
    t0 = unique[0]["tick"]
    for row in unique:
        row["t"] = (row["tick"] - t0) / 1000.0
    # 角速度用前后各约 40 ms 的角度差估算，单位 °/s。
    for i, row in enumerate(unique):
        j0, j1 = i, i
        while j0 > 0 and row["t"] - unique[j0 - 1]["t"] <= 0.04:
            j0 -= 1
        while j1 < len(unique) - 1 and unique[j1 + 1]["t"] - row["t"] <= 0.04:
            j1 += 1
        span = unique[j1]["t"] - unique[j0]["t"]
        row["dps"] = (unique[j1]["angle_deg"] - unique[j0]["angle_deg"]) / span if span > 0 else 0.0
    return unique


def segments(rows, predicate, min_len=0.15):
    out, start = [], None
    for i, row in enumerate(rows):
        if predicate(row):
            if start is None:
                start = i
        elif start is not None:
            if rows[i - 1]["t"] - rows[start]["t"] >= min_len:
                out.append((start, i - 1))
            start = None
    if start is not None and rows[-1]["t"] - rows[start]["t"] >= min_len:
        out.append((start, len(rows) - 1))
    return out


def main():
    path = Path(sys.argv[1])
    rows = load(path)
    duration = rows[-1]["t"]
    print(f"file {path.name}: {len(rows)} samples over {duration:.1f} s ({len(rows) / duration:.0f} Hz)")
    offline = segments(rows, lambda r: r["remote_online"] < 0.5 or r["mode"] != 1, 0)
    for a, b in offline:
        print(f"  not manual/online {rows[a]['t']:.2f}-{rows[b]['t']:.2f} s: mode={rows[a]['mode']:.0f} "
              f"online={rows[a]['remote_online']:.0f} active={rows[a]['active']:.0f} angle {rows[a]['angle_deg']:.2f}->{rows[b]['angle_deg']:.2f}")
    print(f"  temp {min(r['temp_c'] for r in rows):.0f}-{max(r['temp_c'] for r in rows):.0f} C, "
          f"angle {min(r['angle_deg'] for r in rows):.2f}..{max(r['angle_deg'] for r in rows):.2f}")
    for a, b in segments(rows, lambda r: r["stall"] > 0.5, 0):
        print(f"  stall {rows[a]['t']:.2f}-{rows[b]['t']:.2f} s at {rows[a]['angle_deg']:.2f} deg, limit {rows[a]['stall_limit']:.0f}")

    print("\nSTICK MOVES (|left_x| > 0.05):")
    print("  start-end s | angle from->to | stick avg | cmd deg/s | real avg deg/s | peak | stops | max lead | |cmd| max")
    moves = segments(rows, lambda r: abs(r["left_x"]) > 0.05)
    for a, b in moves:
        seg = rows[a:b + 1]
        span = seg[-1]["t"] - seg[0]["t"]
        stick = sum(r["left_x"] for r in seg) / len(seg)
        cmd_speed = sum(abs(r["jog_rad_s"]) for r in seg) / len(seg) * 180 / math.pi
        moved = seg[-1]["angle_deg"] - seg[0]["angle_deg"]
        peak = max(abs(r["dps"]) for r in seg)
        # 停顿：动起来（>3°/s）以后又在推着摇杆时停下（<0.5°/s 至少 60 ms）。
        stops, moving, still_since = 0, False, None
        for r in seg:
            if abs(r["dps"]) > 3:
                moving, still_since = True, None
            elif abs(r["dps"]) < 0.5 and moving:
                still_since = r["t"] if still_since is None else still_since
                if r["t"] - still_since >= 0.06:
                    stops += 1
                    moving, still_since = False, None
        lead = max(abs(r["target_deg"] - r["angle_deg"]) for r in seg)
        current = max(abs(r["cmd"]) for r in seg)
        print(f"  {seg[0]['t']:6.2f}-{seg[-1]['t']:6.2f} | {seg[0]['angle_deg']:7.2f}->{seg[-1]['angle_deg']:7.2f} | "
              f"{stick:+.2f} | {cmd_speed:6.1f} | {abs(moved) / span if span else 0:6.1f} | {peak:5.1f} | {stops:3d} | "
              f"{lead:5.2f} | {current:5.0f}")

    print("\nRELEASES (stick back to center, next 2 s or until next move):")
    print("  t s | angle at release | +0.3 s | +2 s | max drift after 0.3 s | hold cmd | crank moving")
    for index, (a, b) in enumerate(moves):
        end = moves[index + 1][0] if index + 1 < len(moves) else len(rows)
        release = rows[b]
        after = [r for r in rows[b:end] if r["t"] - release["t"] <= 2.0]
        if len(after) < 5:
            continue
        settle = [r for r in after if r["t"] - release["t"] >= 0.3]
        a03 = settle[0]["angle_deg"] if settle else after[-1]["angle_deg"]
        drift = max((abs(r["angle_deg"] - a03) for r in settle), default=0)
        crank = any(abs(r["right_y"]) > 0.05 for r in after)
        print(f"  {release['t']:6.2f} | {release['angle_deg']:7.2f} | {a03:7.2f} | {after[-1]['angle_deg']:7.2f} | "
              f"{drift:5.2f} | {after[-1]['cmd']:6.0f} | {'yes' if crank else 'no'}")

    print("\nCRANK WHILE ARM STICK CENTERED (|right_y| > 0.05, |left_x| <= 0.05):")
    for a, b in segments(rows, lambda r: abs(r["right_y"]) > 0.05 and abs(r["left_x"]) <= 0.05):
        seg = rows[a:b + 1]
        angles = [r["angle_deg"] for r in seg]
        print(f"  {seg[0]['t']:6.2f}-{seg[-1]['t']:6.2f} s right_y {seg[0]['right_y']:+.2f}: arm {min(angles):.2f}..{max(angles):.2f} "
              f"(start {angles[0]:.2f}, end {angles[-1]:.2f}), cmd {min(r['cmd'] for r in seg):.0f}..{max(r['cmd'] for r in seg):.0f}")


if __name__ == "__main__":
    main()
