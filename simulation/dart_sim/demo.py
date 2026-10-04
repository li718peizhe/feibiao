"""无界面演示：从初始状态连发四发，打印每一步和每发的结果。"""
import time

from .sim import Sim


def run_demo(p=None, shots=4, log=print):
    t0 = time.time()
    sim = Sim(p)
    sim.seq.cmd_salvo(shots)
    last_label = None
    while sim.seq.busy() and sim.t < 120:
        sim.run(0.01)
        if sim.seq.label != last_label:
            last_label = sim.seq.label
            log(f"{sim.t:7.2f} s  {last_label}")
    if sim.seq.fault:
        log(f"流程停止：{sim.seq.fault}")
    for k, s in enumerate(sim.darts.shots):
        if not sim.darts.enabled:
            log(f"第 {k + 1} 次发射：发射机构峰值速度 {s['v_release']:.2f} m/s（未放飞镖）")
            continue
        dist = "未落地" if s["distance"] is None else f"{s['distance']:.2f} m"
        log(f"第 {k + 1} 发：出膛 {s['v_release']:.2f} m/s，落点 {dist}，飞行 {s['flight_time'] or 0:.2f} s")
    log(f"仿真 {sim.t:.1f} s，用时 {time.time() - t0:.1f} s")
    return sim
