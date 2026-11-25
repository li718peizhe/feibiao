# 任务 3v：独立自动装填试运行

## 目标与边界

用户选择“自动装填、不发射”。新固件两侧取镖均由用户确认吸牢、无碰撞，手动移动至两侧待命位均确认间隙合适、保持稳定；未提供本轮新日志或读数。
复用手动装弹组拨轮正向到底保持 2 s：磁铁无镖时沿用单发取镖；取镖后再次触发时执行装填，到落座、收杆、找底锁止确认、滑块回位即结束。接镖座为空由操作手在触发装填前确认；此入口只接收取镖试运行持镖或已转交库存的中止装填。
保持 `kLoadCalibrated=false`，不调用发射、连发或预取下一发；沿用现有回零、锁止、找底、遥控取消、滑块托持、磁铁 3 s 限时与释放中断规则。

## 范围与验收

源码：`src/dart_inventory.hpp/.cpp`、`src/launch_sequence.hpp/.cpp`、`src/hw/launcher_hw.cpp`、`src/mode_logic.hpp` 的请求注释、`test/host_test.cpp`、`README.md`。
固件：`Modules/modules.yaml` 模块请求及由 `xrobot setup` 自动生成的 `xrobot.lock`。
文档：本文件、`handoff.md`；既有右取镖 46.35° 改动保留未提交。
不读取 `plans/dart-fw-libxr.md`，不运行 graphify，不调用子代理，不烧录，不 push，不新建分支。
验收：取镖库存能转交装填，左右及未知/已锁/已放开状态均完成装填且发射计数为零；中止及释放中断沿用原规则；主机与 ARM、完整 debug 编译通过；两仓库各一次本地提交。

## 计划

1. 已完成：读取现有入口与库存，主机基线 PASS: 22151 checks，补充左右及三种锁止状态、无发射和中止测试。
2. 已完成：最小实现持镖转交与遥控分派，主机 PASS: 28565 checks、ARM 单文件编译通过，源码暂存审查及提交完成。
3. 已完成：模块同步及复测、完整 debug 编译和固件提交完成，交接已更新。

## 基线

- 源码：`codex/apply-load-position-calibration` / `2aa57ac707c55182e6a70cf8cd728493d64d3b0e`，仅 `src/dart_params.hpp` 右取镖 46.35° 既有改动。
- 固件：`dart-launcher` / `ca477afe97663af5662bae39829e09a886661047`，干净；模块副本 `2aa57ac`。
- 最近 ELF：`E5AA020E4F1A2A31C32CE18419F52AD067C593B621B029D41B3C8F2B69D54C8C`；用户反馈已编译烧录，左右取镖和手动待命检查通过。

## 验证命令

在源码目录用现有 g++ C++17 命令编译全部 `src/*.cpp` 和 `test/host_test.cpp`，运行主机回归；检查纯逻辑无平台依赖、`git diff --check`。
复用 debug `compile_commands.json` 对源码 `launcher_hw.cpp` 进行 ARM C++20 / `-Werror` 编译；源码提交只暂存本任务文件。
更新固件模块请求至新源码提交，`xrobot setup --update local/DartLauncher`；副本提交不一致就停止，不手改锁文件。
对同步模块重新跑主机回归，`cmake --build --preset debug`，记录 ELF 哈希、占用与模块警告；固件只提交模块请求和锁文件。
构建失败时保留日志并修复本任务原因；现场烧录与机构验收分别记录，现场操作只在聊天给出。

## 执行记录

- 新增 `DartInventory::ResetHeld()`，把操作手确认的单发持镖状态导入库存，禁用手动锁止时自动补一发的推断；未知锁止探测成已锁住时也会继续真正的落座装填。
- `CommandLoadFromFetch()` 复用 `CommandLoad()`，只在首次从取镖试运行交接时重建单发库存；释放前取消保留 Held，可从装填起点重做。持镖分派由现有遥控入口完成，内部不重复检查调用方已保证的持镖前提。
- 手动装弹组正向 2 s 手势按持镖状态分派：空载取一发、持镖装填；不新增遥控组合或开关。`kLoadCalibrated=false`、发射与连发触发逻辑不变。
- 基线 `PASS: 22151 checks`。先加测试编译因新接口尚未实现报 `LaunchSequence has no member named CommandLoadFromFetch`；实现后首轮测试误将“装填：滑块把发射机构带下去”中的“发射机构”判为发射步骤，报 `FAIL: standalone loading never enters firing, flight or prefetch`，改为核对发射步骤前缀后通过；最终仅保留有效流程与安全需求测试，`PASS: 28565 checks`。
- 新测试覆盖左右取镖、已放开/已锁住/未知三种锁止状态的持镖转交，核对标定滑块与推杆目标、回 0°、落座锁止回位，逐拍确认无发射步骤、无飞行或预取、发射计数及记录为 0；覆盖释放前取消后重做与释放期间无力后的 `ReleaseInterrupted` 和滑块托持。
- 纯逻辑平台依赖检查无输出，`git diff --check` 通过。复用 debug `compile_commands.json` 的 ARM C++20 / `-Werror` 命令，源码 `launcher_hw.cpp` 编译到 `build-host/3v-launcher_hw.o`，退出码 0、无警告。
- 构建技能脚本在本会话前轮已报 `ModuleNotFoundError: No module named 'tool_config'`，本轮不重复运行或安装依赖，使用已验证的现有 CMake debug 命令。
- 源码提交 `8b94f387dfedf5b4cdcb7324e8a68396b165540e`，`增加持镖后的独立自动装填`；本任务八个文件已提交，既有 `src/dart_params.hpp` 右取镖 46.35° 修改未暂存、未提交。
- 旧 ELF 备份 `dart-fw/build/DevC-2aa57ac-before-3v.elf`，SHA-256 `E5AA020E4F1A2A31C32CE18419F52AD067C593B621B029D41B3C8F2B69D54C8C`；`xrobot setup --update local/DartLauncher` 退出码 0，副本核对为 `8b94f38` 且干净，Referee 原有三个修改保留。
- 同步模块主机回归 `PASS: 28565 checks`；`cmake --build --preset debug` 退出码 0，日志 `dart-fw/build/3v-build.log`。模块无新警告，只有既有 `enable_language` 与 Eigen `CMP0146` 配置警告；RAM 32552 B / 128 KB、CCMRAM 64 KB / 64 KB、FLASH 218868 B / 1 MB。
- 新 ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 16:38:08，8316980 字节，SHA-256 `65E5B6D65AE15BFEB9C3AF2FE79BC11EB3BAC231B3BF9EF0473864F9D0B6B033`；`arm-none-eabi-nm -C` 核对 `CommandLoadFromFetch` 和 `ResetHeld` 已链接进镜像。
- 未烧录；现场待验为新入口的遥控分派、自动近 0° 放镖释放及收杆、找底锁止确认和回位，完成应有镖、已上膛且无发射。用户最后反馈旧参数固件右侧待命合适、稳定，磁铁可能仍持镖；烧录复位前先把镖取下或放回镖座并释放，确保磁铁空载、滑块已到顶，上着膛先退膛；新固件复位后需重新回零并重新取镖。
- 固件提交 `eaf88d1d97f814c97dd74444b8b28ad75c2587c8`，`锁定独立自动装填试运行模块`；仅模块请求和自动生成的锁文件。源码分支 `codex/apply-load-position-calibration` 保留既有右取镖角修改，固件分支 `dart-launcher` 及模块副本干净，Referee 原有三个修改保留；根目录无 Git，任务记录和交接直接保存，未 push。

## 2150 µs 放镖行程修正

用户反馈已编译烧录并运行独立自动装填，滑块下压很远，推杆下放仍距发射机构很远；随后给出“要 2150 µs 才够”。本轮按该实测脉宽修正 `kCrankSeat`，滑块目标保持 −0.19720 m，独立装填流程及自动连发关闭设置不变；2150 µs 下的释放落座、收杆及整套装填仍待现场验收，本轮未收到对应日志。
范围追加 `src/dart_params.hpp` 的放镖脉宽参数及既有 PWM 换算测试、README；既有右取镖角修改继续保留未提交，不烧录、不运行 graphify、不调用子代理。

1. 已完成：核对两仓库及既有修改，PWM 断言复现 1830 µs 不满足 2150 µs 要求，修正参数与说明。
2. 已完成：主机回归 PASS: 28565 checks，ARM 编译及暂存审查通过，源码已提交并同步。
3. 已完成：同步模块复测、完整 debug 编译、固件提交及交接更新完成。

- 修正前基线 `PASS: 28565 checks`；在现有推杆标定 PWM 换算检查中加入放镖输出应为 2150 µs 的现场要求，旧参数运行失败：`FAIL: calibrated crank angles give the measured retract, fetch and 2150 us load pulses`。改为 `CrankAngleFromPulse(2150)` 后完整主机回归 `PASS: 28565 checks`。
- 源码 ARM C++20 / `-Werror` 编译 `launcher_hw.cpp` 到 `build-host/3v-crank-2150-launcher_hw.o`，退出码 0、无警告；纯逻辑平台依赖检查无输出，`git diff --check` 通过。
- 源码提交 `35ece1174f71d3ede951af158c97cf8752db1927`，`修正装填推杆下放至2150微秒`；仅本任务参数 hunk、PWM 检查及 README，右取镖 46.35° 既有 hunk 留在工作区。
- 旧 ELF 已备份 `dart-fw/build/DevC-8b94f38-before-crank-2150.elf`，SHA-256 `65E5B6D65AE15BFEB9C3AF2FE79BC11EB3BAC231B3BF9EF0473864F9D0B6B033`。
- `xrobot setup --update local/DartLauncher` 退出码 0，模块副本为 `35ece1174f71d3ede951af158c97cf8752db1927` 且干净，Referee 原有三个修改保留；同步副本主机回归 `PASS: 28565 checks`。
- `cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3v-crank-2150-build.log`，无模块新警告，仅已有 `enable_language`、Eigen `CMP0146` 配置警告。RAM 32552 B / 128 KB、CCMRAM 64 KB / 64 KB、FLASH 218868 B / 1 MB。
- ELF：`dart-fw/build/debug/DevC.elf`，2026-11-14 16:56:58，8316980 字节，SHA-256 `6279E9DB1991C0EF9B5219F28B459B43FF46E0230F4EF8190AB08877692B7D33`。
- 固件提交 `0282413c7dc26c53e2aaf436dca764a809d5216b`，`锁定2150微秒装填推杆参数`；仅模块请求与自动生成的锁文件。两仓库原分支不变，源码仅剩既有右取镖角修改，固件及模块副本干净，未 push、未烧录。
- 当前待现场验证：2150 µs 下的释放落座、收杆不带起、找底锁止回位；滑块“下压很远”仍未收到故障姿态的 `step`、`sl`、`Dart cal`、`latch` 和 `fault`，本次未改变滑块目标或下压步骤。滑块托着未锁住的发射机构时保持供电，安全退膛或脱困回顶部之后才烧录新镜像。
