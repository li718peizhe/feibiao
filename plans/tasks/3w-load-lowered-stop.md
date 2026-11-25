# 任务 3w：独立装填下放后停住

## 目标与范围

用户确认：推杆下放到 2150 µs 后停住，磁铁保持断电吸持。成功条件为独立装填在下放步骤完成后结束，推杆保持目标、滑块保持 −0.19720 m、库存仍持镖；不进入释放、收杆、后续锁止回位或发射。

最小实现：使用独立装填的运行状态选择已有装填队列的前七步，结束、取消、故障时清掉该运行状态。普通装填及连发继续使用完整队列，既有安全判断保留。

文件：`src/launch_sequence.hpp/.cpp`、`test/host_test.cpp`、`README.md`；固件模块请求及自动生成锁文件；本记录与 `handoff.md`。保留源码既有右取镖 46.35° 修改，不读取 `plans/dart-fw-libxr.md`，不运行 graphify，不调用子代理，不烧录，不 push。

## 计划

1. 已完成：读取交接、代码、Git 状态，确认动作含义；主机基线 28565 项，通过新增断电断言复现旧流程通电。
2. 已完成：最小修改、两侧及三种锁止初态的主机验证、ARM 编译、暂存审查和源码提交。
3. 已完成：同步固件模块、模块回归、完整 debug 构建、固件提交及交接更新。

## 验证记录

- 基线：源码 `codex/apply-load-position-calibration` / `35ece11`，仅 `src/dart_params.hpp` 既有右取镖角修改；固件 `dart-launcher` / `0282413`，工作区干净。
- 新断言运行旧流程失败：`FAIL: standalone loading keeps the magnet de-energized throughout`。
- 最终主机 `PASS: 32496 checks`：覆盖左右、已放开/已锁住/未知初态、全程断电吸持、2150 µs 停止、5 s 后保持、持镖禁止再次取镖、停止后取消仍托持、中途取消后重做、独立装填后普通装填仍完整执行；普通装填和释放中断测试仍通过。
- ARM 首次命令把连续空格拆成空参数，编译器报 `linker input file not found: No such file or directory`；按连续空白拆参后，`launch_sequence.cpp` 和 `launcher_hw.cpp` 的 ARM C++20 / `-Werror` 单文件编译通过，无警告。纯逻辑平台依赖检查无输出，暂存差异检查通过。
- 源码提交 `84eafc2cad0909f0054a8053794ced6983f89f3b`，`独立装填下放后停住并保持磁铁断电`；只提交本任务四个文件，右取镖角 46.35° 修改仍未提交。
- `xrobot setup --update local/DartLauncher` 退出码 0；模块副本 HEAD 为 `84eafc2` 且干净，Referee 原有三个修改保留。
- 模块主机首次 `g++ ... -o build-host\host_test.exe` 因副本没有 `build-host` 目录失败：`cannot open output file build-host\host_test.exe: No such file or directory`；输出改至现有 `dart-fw/build/3w-module-host-test.exe` 后编译运行通过，`PASS: 32496 checks`。
- `cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3w-load-stop-build.log`，模块无新警告，仅已有 `enable_language` 和 Eigen `CMP0146` 配置警告。RAM 32552 B / 128 KB、CCMRAM 64 KB / 64 KB、FLASH 218908 B / 1 MB。
- ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 17:11:44，8317072 字节，SHA-256 `D906161BF697AF89F552A2F9073DAB10B84280864FF54B4A3859F0B91E9ED815`。上一版备份 `build/DevC-35ece11-before-load-stop.elf`，SHA-256 `6279E9DB1991C0EF9B5219F28B459B43FF46E0230F4EF8190AB08877692B7D33`。
- 固件提交 `6c50b1800ca5378fc807f416894505e0b1a83f32`，`锁定独立装填下放停住模块`；仅模块请求和自动生成锁文件。原分支不变，源码保留既有右取镖角修改，固件及模块副本干净，Referee 原有三个修改保留；根目录无 Git，记录与交接直接保存，未 push、未烧录。
- 构建沿用交接已有 CMake debug 命令；包装脚本此前缺 `tool_config`，本任务不安装依赖。
- 现场待验：实际下推停住位置、磁铁保持断电吸持、推杆保持和滑块托持。停止时滑块托着未锁住发射机构，保持供电；烧录和断电前按既有脱困流程回顶部。
