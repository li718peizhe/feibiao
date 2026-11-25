# 任务 3y：同步用户修改的装填 5° 目标

## 目标与范围

用户将源码装填目标改为 5.0°，现场串口仍约 0.66°。检查发现源码工作区目标为 5.0°，固件锁定的模块副本 `11cbad2` 仍为 0.5°，现有 ELF 哈希与 3x 一致；固件编译直接使用模块副本。摆臂 MoveTo 到位范围 0.3°，串口 0.66° 与 0.5° 目标相符。

成功条件：保留用户 5.0° 目标，更新步骤标签、测试与说明，提交源码后通过 xrobot 同步，模块副本及新固件使用 5.0°；独立装填下放停止与断电吸持保持。

文件：`src/launch_sequence.cpp`、`test/host_test.cpp`、`README.md`、固件模块请求与自动生成锁文件、本记录及交接；既有 `src/dart_params.hpp` 右取镖角修改保留。不开新分支、不 push、不烧录、不运行 graphify、不调用子代理、不读 `plans/dart-fw-libxr.md`。

## 计划

1. 已完成：检查源码、固件模块、ELF 和到位判断，定位目标未同步。
2. 已完成：复现旧模块目标、同步测试与说明、主机及 ARM 验证、源码提交。
3. 已完成：模块同步复测、固件构建和提交、更新交接。

## 验证记录

- 首次新增断言放在上膛轨迹测试，未经过装填目标，运行仍 `PASS: 32496 checks`；移至完整装填轨迹后，用源码测试编译旧模块的纯逻辑文件，失败 `FAIL: loading commands the requested 5 degree arm target`。
- 使用用户源码 5.0° 目标并同步测试和步骤标签后，源码主机 `PASS: 32499 checks`，ARM C++20 / `-Werror` 单文件编译通过；暂存差异检查通过。
- 源码提交 `c978a0d5ea9fde83b3ab5348226fa5c3dfbaf919`，`同步用户装填摆臂5度目标及验证`；保留用户 5.0° 修改并纳入本任务提交，既有右取镖角修改留在工作区。
- `xrobot setup --update local/DartLauncher` 退出码 0，模块副本 `c978a0d` 且干净；同步模块主机 `PASS: 32499 checks`，输出 `dart-fw/build/3y-module-host-test.exe`。
- `cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3y-load-arm-five-degree-build.log`，模块无新警告，仅已有 `enable_language`、Eigen `CMP0146` 配置警告；RAM 32552 B、CCMRAM 64 KB、FLASH 218916 B。
- 旧版 `.eide` GDB 离线反汇编失败：`No type "LaunchSequence" within class or namespace "dart"`；改用标准库脚本 `dart-fw/build/3y-check-load-target.py` 解析 ELF 装载段中的步骤名指针、动作、完成条件与 double 目标，`python build/3y-check-load-target.py` 退出码 0；旧 ELF 和新 ELF 在步骤表 `0x080321e8` 的目标分别核对为 0.500000000°、5.000000000°。
- 新 ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 17:46:26，8317072 字节，SHA-256 `AE58CAC286147FC4E4BD17E4E0B08D159D9A9F2C548BE0D3D1E4B89560FE37E7`；旧版备份 `build/DevC-11cbad2-before-load-arm-five-degree.elf`，SHA-256 `31E682123B50F8E4A5B6B7FD1555F58E8350A1B712757A9A861431EA4AFEE165`。
- 固件提交 `67c8882cbd61ebd11d2b33527981faffe6cb97e8`，`锁定用户装填摆臂5度目标`；原分支不变，固件及模块副本干净，源码仅余既有右取镖角修改；Referee 原有三个修改保留。未烧录、未 push；根目录交接与本记录直接保存。

## 现场验证

新固件需现场验证 5° 实际位置与对准。独立装填停止时滑块托着未锁住发射机构，保持供电；断电或烧录前按已有脱困流程回顶部。
