# 任务 3z：2° 完整独立装填与下推后通电

## 目标、范围及验证

用户确认装填角 2.0° 刚好，要求启用自动装填，推杆推到底后继电器才通电。将独立装填恢复为释放、收杆、找底锁止确认和回位的完整流程，继续不发射、不预取，自动连发换弹开关沿用关闭。推杆下放步骤完成后额外停留 `kLoadCrankSettleTime=0.6 s` 再通电；现有下放到位条件及至少 0.6 s 步骤等待保留。舵机没有物理位置反馈，额外等待及实际推到底时序待现场验证。

文件：`src/launch_sequence.hpp/.cpp`、`src/dart_params.hpp` 的新等待参数、`test/host_test.cpp`、`README.md`；固件模块请求、自动生成锁文件、本记录及交接。源码与模块中的用户 2.0° 修改均保留并同步；源码既有右取镖 46.35° 修改不提交。不开新分支、不 push、不烧录、不运行 graphify、不调用子代理、不读取 `plans/dart-fw-libxr.md`。

## 计划

1. 已完成：核对两仓库、用户双份 2.0° 修改、推杆到位及继电器逻辑，确定完整装填和额外等待。
2. 已完成：修改并验证两侧完整装填、推杆到位后等待、释放中断与库存、ARM 编译及源码提交。
3. 已完成：同步模块、复测、完整 debug 编译、固件提交、交接更新。

## 基线

源码 `codex/apply-load-position-calibration` / `c978a0d`，用户修改 `launch_sequence.cpp` 为 2°，`dart_params.hpp` 既有右取镖角修改；固件 `dart-launcher` / `67c8882` 干净，模块副本 `c978a0d` 带相同 2° 修改。现有独立装填曾在下放后直接停住，需恢复完整队列。

## 验证记录

- 先同步用户 2° 标签与测试、增加完整装填目标检查，原截断流程失败：`FAIL: both sides finish loading at 2 degrees with the dart seated, locked and parked`；恢复完整队列后、增加等待前失败：`FAIL: standalone relay energizes only after the lowered crank settles`。
- 增加命名等待步骤后主机 `PASS: 30359 checks`；补充推杆延迟 0.8 s 到位模拟、到位后完整等待、再通电且仍保持推杆目标检查，最终 `PASS: 30504 checks`。左右及三种锁止初态完成 +2° 装填、释放收杆、锁止回位；释放前取消重做与释放窗口无力中断均通过，无发射和预取。
- `launch_sequence.cpp` 和 `launcher_hw.cpp` ARM C++20 / `-Werror` 单文件编译通过，无警告；纯逻辑平台依赖检查无输出，暂存差异检查通过。
- 源码提交 `dd79323a0c947e6b57e09037093188a3fe955dac`，`恢复2度独立装填并等待推杆到位后通电`；仅暂存本任务五个文件及参数等待 hunk，既有右取镖角未暂存。
- 模块中的同一 2° 修改已纳入上述源码提交，为通过工具同步，先只用补丁清除模块内重复的 2° 工作区修改，再由 `xrobot setup` 切至包含 2° 的新提交，未使用 reset/restore/clean。
- `xrobot setup --update local/DartLauncher` 退出码 0，模块副本 HEAD `dd79323` 且干净；同步模块主机 `PASS: 30504 checks`，输出 `dart-fw/build/3z-module-host-test.exe`。
- `cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3z-load-complete-build.log`，模块无新警告，仅已有 `enable_language`、Eigen `CMP0146` 配置警告；RAM 32552 B、CCMRAM 64 KB、FLASH 218948 B。
- 新 ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 19:20:33，8317516 字节，SHA-256 `6055C8FF67FD9CF87A07A9B38BF3E735BCF23C8953C7C13FB730E324413C0E15`。`python build/3z-check-load-steps.py` 核对装填步骤表为摆臂 2.0°、推杆下放 1.9438604544 rad（2150 µs）、等待 0.6 s、再继电器通电。
- 固件提交 `15656d06e63a35da78ff59edf4866fdedc838e10`，`锁定2度完整装填和下推等待`；原分支不变，固件及模块副本干净，Referee 原有三个修改保留。未烧录、未 push；根目录交接与本记录直接保存。
