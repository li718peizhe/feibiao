# 任务 3x：装填回中向右 0.5°

## 目标与范围

用户反馈放镖后镖整体在发射机构左侧约 2–3 mm，要求装填回中向右 0.5°。代码已有右侧正角、左侧负角的坐标定义，本任务将装填步骤的摆臂目标设为 +0.5°，步骤名称同步显示目标；编码器零点及取镖位置沿用。

成功条件：左右取镖后独立装填均在摆臂 +0.5°、推杆 2150 µs、滑块 −0.19720 m 处停止，磁铁保持断电吸持，现有主机回归、ARM 编译、固件构建通过。

文件：`src/launch_sequence.cpp`、`test/host_test.cpp`、`README.md`；固件模块请求和自动生成锁文件；本记录和 `handoff.md`。保留 `src/dart_params.hpp` 既有右取镖 46.35° 修改；不读取 `plans/dart-fw-libxr.md`，不运行 graphify、不调用子代理、不烧录、不 push。

## 计划

1. 已完成：核对现有分支及修改、装填步骤和左右方向，最小修改目标、步骤名称、现有测试及说明。
2. 已完成：主机回归、ARM 单文件编译、暂存审查及源码提交。
3. 已完成：模块同步、模块复测、完整 debug 编译、固件提交及交接更新。

## 验证记录

- 基线：源码 `codex/apply-load-position-calibration` / `84eafc2`，仅 `src/dart_params.hpp` 既有右取镖角修改；固件 `dart-launcher` / `6c50b18`，固件及模块副本干净。
- 源码主机 `PASS: 32496 checks`；现有左右与三种锁止初态测试核对 +0.5° 目标及停止时实测角度，完整顺序标签同步更新。`launch_sequence.cpp` 的 ARM C++20 / `-Werror` 编译通过，无警告；纯逻辑平台依赖检查无输出，暂存差异检查通过。
- 源码提交 `11cbad2ef54abd49585e0d2fd30f859e8cab6465`，`将装填摆臂回中目标右移0.5度`；只提交本任务三个文件，既有右取镖角修改仍未提交。
- `xrobot setup --update local/DartLauncher` 退出码 0；模块副本 HEAD `11cbad2` 且干净。同步模块主机 `PASS: 32496 checks`，输出 `dart-fw/build/3x-module-host-test.exe`；Referee 既有三个修改保留。
- `cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3x-load-arm-half-degree-build.log`。模块无新警告，只有既有 `enable_language` 和 Eigen `CMP0146` 配置警告；RAM 32552 B / 128 KB，CCMRAM 64 KB / 64 KB，FLASH 218916 B / 1 MB。
- 新 ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 17:23:56，8317072 字节，SHA-256 `31E682123B50F8E4A5B6B7FD1555F58E8350A1B712757A9A861431EA4AFEE165`。旧 ELF 备份 `build/DevC-84eafc2-before-load-arm-half-degree.elf`，SHA-256 `D906161BF697AF89F552A2F9073DAB10B84280864FF54B4A3859F0B91E9ED815`。
- 固件提交 `1a0dc4d38e49caa545cb0677e70ce88c04caa9bf`，`锁定装填摆臂右移0.5度参数`；仅模块请求和自动生成锁文件。两仓库原分支不变，源码保留原有右取镖角修改，固件及模块副本干净；根目录无 Git，记录与交接直接保存，未烧录、未 push。
- 构建继续按已读取的 build-cmake 技能复用现有 debug 目录，使用项目交接规定的 `cmake --build --preset debug`；包装脚本既有 `tool_config` 缺失，本轮跳过包装脚本，无安装。
- 现场待验：+0.5° 的实际横向对准效果、推杆下放停止与磁铁吸持。停止时滑块托着未锁住的发射机构，保持供电；断电和烧录前按既有脱困流程回顶部。
