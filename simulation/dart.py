"""飞镖发射架仿真入口。

    python simulation/dart.py build      从 CAD 导出生成模型（第一次运行或改了导出/几何参数后）
    python simulation/dart.py console    中文控制台 + 三维视图
    python simulation/dart.py demo       无界面跑一遍四发流程，打印每发结果
    加 --darts 放上飞镖（默认不放，只看机构动作）
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser(description="飞镖发射架 MuJoCo 仿真")
    ap.add_argument("cmd", choices=["build", "console", "demo"])
    ap.add_argument("--export", help="URDF 导出目录（默认见 dart_sim/params.py）")
    ap.add_argument("--darts", action="store_true", help="放上飞镖（出膛、飞行、落点）")
    args = ap.parse_args()
    from dart_sim.params import Params
    p = Params()
    if args.export:
        p.export_dir = args.export
    p.darts_enabled = args.darts
    if args.cmd == "build":
        from dart_sim.build import build
        build(p)
    elif args.cmd == "console":
        from dart_sim.console import run_console
        run_console(p)
    else:
        from dart_sim.demo import run_demo
        run_demo(p)


if __name__ == "__main__":
    main()
