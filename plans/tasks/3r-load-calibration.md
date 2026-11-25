# 任务 3r：装填位置集中读数

## 2026-11-14 三项参数回填计划

目标：将现场确认的滑块 −0.19720 m、放镖推杆 1830 µs、统一待命角 36.16° 写入参数，并保持自动换弹关闭。
范围：源码 `src/dart_params.hpp`、`README.md`，固件模块请求及自动生成的 `xrobot.lock`，本文件与 `handoff.md`；既有右取镖 46.35° 修改保留未提交，不读 `plans/dart-fw-libxr.md`，不运行 graphify，不调用子代理，不烧录。
验收：主机完整回归通过，模块副本与本次源码提交一致，debug 固件构建通过，两个仓库各一次本地提交，交接记录实测证据及待现场项目。

1. 已完成：核对两仓库与既有右取镖角改动，主机基线 PASS: 22115 checks，回填三项参数及说明。
2. 已完成：源码与同步模块主机回归均 PASS: 22151 checks，ELF 参数和脉宽回算核对通过，完整 debug 编译通过。
3. 已完成：审查并完成两仓库本地提交，交接与现场待验项已更新。

## 目标与验收

用户报告十次取镖成功率为 80%，右侧角度和抬起速度未评价；要求用代码协助测量 `kCrankSeat`、`kLoadPosition`、`kStandby`。
复用现有手动操作和控制线程快照，每组状态增加一行 `Dart cal`，集中提供推杆输出脉宽/估计机构角、滑块平均高度、带方向的摆臂角以及滑块/摆臂回零状态。
现场对准后读取数值；贴合和避让间隙由操作手观察。推杆没有位置反馈。取镖参数和 `kLoadCalibrated=false` 保持当前设置。

## 范围与最小实现

- 模块：`src/hw/launcher_hw.cpp`、`README.md`。
- 固件：模块同步生成的 `xrobot.lock`；保留同步生成文件和已有 Referee 补丁。
- 文档：本任务文件及 `handoff.md` 的当前状态、下一步、提交记录。
- 不读取 `plans/dart-fw-libxr.md`，不运行 graphify，不调用子代理。
- 在现有分支上各做一次本地提交，不 push，不烧录。

## 进度

1. 已完成：核对两仓库分支、状态、HEAD；确认手动点动与现有快照可以提供全部读数。
2. 已完成：增加集中读数和现场标定说明；保持现有控制逻辑。
3. 已完成：主机回归、单文件 ARM 编译、读数格式检查、完整固件编译全部通过；两仓库各一次本地提交，交接已更新；现场测量见下文。

## 基线

- DartLauncher：`main`，`bc27634cc8f452ba1a8f57322b3d5e0423560a04`，干净。
- dart-fw：`dart-launcher`，`bec71d5`，干净；模块副本为 `bc27634`。
- Referee 独立仓库的 `Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp` 有既存修改，保留。
- 板上版本本次尚未核实，不从成功率推断已经烧录 3q4。

## 验证与现场回填

- 主机：按 README 的 g++ C++17 命令编译并运行完整回归；检查纯逻辑没有引入平台头文件或宏。
- 固件：用现有 debug 预设构建；检查 `launcher_hw.cpp` 无新警告，记录 ELF 哈希。
- 集中读数：按实际格式检查整行小于 USB 128 字节缓冲，核对推杆、滑块、摆臂的单位及符号。
- 现场：新固件烧录后，核对 `Dart cal` 与原有状态行读数，再分阶段确定装填贴合位置和左右避让角；数值确认前不启用自动换弹。

## 执行记录

- 主机回归：`PASS: 22036 checks`；纯逻辑平台依赖检查无输出，`git diff --check` 通过。
- 从已有 `compile_commands.json` 复用 ARM C++20、`-Werror` 命令，编译本次修改的 `launcher_hw.cpp` 到 `build-host/3r-launcher_hw.o`，退出码 0，无警告。
- 核对实际格式串：装配范围内最长一行 74 字节（含 CRLF），小于 USB 128 字节缓冲；README 格式示例匹配。
- DartLauncher 本地提交：`0323ca800b8e2e8b18f0d6c31224032d14cd6cea`，`增加装填标定集中读数和现场测量说明`；`main` 干净。
- `xrobot setup --update local/DartLauncher` 退出码 0，模块副本已核对为 `0323ca8`；保留 Referee 三个既存修改。
- 构建技能脚本命令 `python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --source . --list-presets --no-save-profile` 失败：`ModuleNotFoundError: No module named 'tool_config'`。未安装依赖，改用现有 `cmake --list-presets`（通过）和 `cmake --build --preset debug`。
- 原 `bc27634` 构建产物已备份为 `dart-fw/build/DevC-bc27634-before-3r.elf`，SHA-256 为 `C41DEDA89037BA88C17EDEA46E23B0A14C77BB16485557D1DFF0ADD7B380E729`。
- 完整固件：`cmake --build --preset debug` 退出码 0；日志 `dart-fw/build/3r-build.log`。`launcher_hw.cpp` 无新警告；只有已有的 `enable_language`、Eigen `CMP0146` 配置警告。
- 占用：RAM 32472 B / 128 KB（24.77%），CCMRAM 64 KB / 64 KB，FLASH 217636 B / 1 MB（20.76%）。
- `dart-fw/build/debug/DevC.elf`：2026-11-13 12:53:48，8250024 字节，SHA-256 `25BA30E3967C5CC0406248A4ED8B6224892BF696616ECA269057269579BEEF25`。
- dart-fw 本地提交：`31397c09e3064f458641385b1ae02f6caf2e02d3`，`更新 DartLauncher：装填标定集中读数`；只提交自动更新的 `xrobot.lock`。Git 对该锁文件提示既有 `LF will be replaced by CRLF the next time Git touches it`，暂存差异仅为模块提交号。
- 本次未执行烧录和实车测量；待用户烧录后验证实际 USB 输出、装填贴合姿态及左右持镖避让角。参数尚未回填，自动换弹保持关闭。

## 2026-11-14 放镖续测记录

### 已完成与现场证据

- 首批 04:00:57–04:03:36 记录保存在根目录 `DartLauncher_calib_0401-0403.md`：完成俯仰/滑块回零、上膛与退膛、空载对准、滑块调位及左侧取镖；早期目视高度为 −0.20250 m。模块提交 `114fad6` 回填了该高度，固件提交 `128f0de` 将模块请求固定为 `114fad6`。此前主机测试 `PASS: 22115 checks`、完整 Debug 构建通过；ELF SHA-256 `D6797D340B59A807BC03588F545FE78128DBE11CC4E402F5BEE68B2FEA38C7C1`，本会话未烧录。
- 04:53 日志中遥控器掉线随后恢复；滑块平均 −0.19752 m，摆臂从约 26° 移向中间。05:05 日志前用户说明，在约 −0.1999 m 时继电器通电，飞镖刚好架到发射机构上。
- 05:05:28–37 稳定姿态：`sl=-200.25/-199.33mm`、`slider=-0.19979m`、`arm=2.24deg`、`raw=1993`、`crank=1835us/1.20116rad`、`home=1/1`、`latch=2`、`fault=无故障`。
- 05:05:32 日志出现“电磁铁通电消磁命令”；05:05:33–34 为 `mag=1`，05:05:35 出现“电磁铁通电满 3.0 s，自动断电吸持”，随后为 `mag=0`，姿态读数保持一致。用户确认断电吸持后飞镖“仍在”发射机构座内。
- 随后用户确认收起推杆“没有带起”飞镖；该段未收集日志。用户说明，放镖时留出间隙，通电释放后飞镖下落并卡在发射机构上。放镖验收记录为释放落座、收杆不带起；保留间隙，取镖下探使用左右镖座各自参数。

### 最新现场参数与代码状态

| 项目 | 最新现场读数 | 当前代码与验证状态 |
|---|---|---|
| `kLoadPosition` | −0.19720 m，左 −197.52 / 右 −196.88 mm | 近 0° 释放落座、收杆不带起；已回填源码提交 `2aa57ac` |
| `kCrankSeat` | 独立装填续测用户反馈需 2150 µs 才够；此前手动日志 1830 µs / 1.19059 rad | 已在 `35ece11` 修正为 `CrankAngleFromPulse(2150)`（约 1.94386 rad）；2150 µs 释放落座及收杆待验，修正证据见 `3v-load-only-test.md` |
| 放镖摆臂角 | +0.30°～+0.34°，raw 1949～1950 | 用户确认近 0° 手动释放落座、断电留座、收杆不带起；自动目标仍为 0°，自动到位待验 |
| `kStandby` | 左 −36.16°～−36.11°，右 +36.04° | 用户确认两侧为刚好的安全距离；统一取 36.16°，已回填源码提交 `2aa57ac` |
| 推杆取镖/收起 | 左 2288 µs、右 2395 µs、收起 1325 µs | 沿用已有参数 |

下一现场任务是新参数固件的自动到位、左右持镖避让与整套换弹联调；自动换弹保持 `kLoadCalibrated=false`。现场操作只在聊天输出。

05:05 轮仅更新 `handoff.md` 和本记录，未重新编译、未烧录；当时源码为 `114fad6`、固件为 `128f0de`。本轮续测与回填见下文，根目录没有 Git 仓库，根目录文档直接保存。

### 15:14–15:57 续测与回填

- 左侧 15:14:29–32：`arm=-36.16/-36.16/-36.16/-36.11deg`，raw 1119～1120；右侧 15:15:21–23：`arm=36.04deg`，raw 2762。两侧均为 `crank=1325us/0rad`、`slider=-0.19693m`、`mag=0`，摇杆回中、转速 0、无故障、`stall=0`；用户确认两侧有间隙，并说明是其认为刚好的安全距离。
- 15:57:18–21：`slider=-0.19720m`（左 −197.52 / 右 −196.88 mm）、`crank=1830us/1.19059rad`、`arm=0.34/0.30/0.30/0.30deg`、`home=1/1`、`latch=2`、`mag=0`，无故障。随后用户确认“是落座，断电后仍在，没有带起”；释放及收杆段没有新增串口日志。
- 放镖参数采用最新近 0° 验收姿态，替换 05:05 的 −0.19979 m / 1835 µs。`Dart cal` 脉宽格式为 `%.0f`、角度为 `%.5f`；以显示的整数 1830 µs 经现有换算函数得到约 1.18988 rad，记录中的 1.19059 rad 保留为原始现场读数。
- 主机基线 `PASS: 22115 checks`；参数回填后原有完整回归 `PASS: 22151 checks`，未新增测试，检查数随流程运行拍数变化。纯逻辑平台依赖检查无输出、`git diff --check` 通过；复用 debug `compile_commands.json` 的 ARM C++20 / `-Werror` 参数，编译源码 `launcher_hw.cpp` 到 `build-host/3r-calibration-launcher_hw.o`，退出码 0、无警告。
- 本次构建技能预设命令 `python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --source . --list-presets --no-save-profile` 失败：`ModuleNotFoundError: No module named 'tool_config'`。未安装依赖；`cmake --list-presets` 通过，继续使用现有 debug 预设。
- 源码提交 `2aa57ac707c55182e6a70cf8cd728493d64d3b0e`，`回填放镖与持镖待命标定参数`；仅提交 `src/dart_params.hpp` 本任务的参数及注释、`README.md`。既有右取镖 46.35° 改动留在工作区，固件同步提交中的右取镖值为 46.18°。
- 旧 ELF 备份为 `dart-fw/build/DevC-114fad6-before-load-calibration.elf`，SHA-256 `D6797D340B59A807BC03588F545FE78128DBE11CC4E402F5BEE68B2FEA38C7C1`。
- `xrobot setup --update local/DartLauncher` 退出码 0；模块副本为 `2aa57ac707c55182e6a70cf8cd728493d64d3b0e` 且干净，Referee 三项既有修改保留。同步模块主机复测首次命令 `g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_test.exe` 失败：`cannot open output file build-host\host_test.exe: No such file or directory`；将输出指定到源码已有 `build-host/host_test-synced.exe` 后完整回归通过 `PASS: 22151 checks`。
- `cmake --build --preset debug` 退出码 0，日志 `dart-fw/build/3r-calibration-build.log`；模块无新警告，仅既有 `enable_language` 和 Eigen `CMP0146` 警告。RAM 32552 B / 128 KB，CCMRAM 64 KB / 64 KB，FLASH 217932 B / 1 MB。
- ELF 为 `dart-fw/build/debug/DevC.elf`，2026-11-14 16:12:04，8255156 字节，SHA-256 `E5AA020E4F1A2A31C32CE18419F52AD067C593B621B029D41B3C8F2B69D54C8C`。离线 GDB 核对滑块 −0.19720 m、待命角 36.16°、推杆 1.1898782175471341 rad（回算 1830 µs）、右取镖角 46.18°，`kLoadCalibrated=false`、`kReloadInstalled=false`。
- 离线 GDB 有 CP1252 → UTF-32 编码警告，数值读取完成；尝试 `-iex 'set host-charset UTF-8'` 报 `Undefined item: "UTF-8"`，该字符集设置未生效。Git 对模块请求和锁文件提示既有 LF → CRLF 转换警告，暂存审查仅含模块提交号变化。
- 未烧录；现场待验为新固件自动到位、左右持镖避让与整套换弹联调。只读 SWD 追踪仍绑定旧 3u ELF，未更新，当前新镜像不可直接使用旧追踪绑定。
- 固件提交 `ca477afe97663af5662bae39829e09a886661047`，`锁定放镖与持镖待命标定参数`；仅提交 `Modules/modules.yaml` 的模块请求与自动生成的 `xrobot.lock`。源码分支保持 `codex/apply-load-position-calibration`，仅剩既有右取镖角改动；固件保持 `dart-launcher` 且干净，模块副本干净；未 push。
