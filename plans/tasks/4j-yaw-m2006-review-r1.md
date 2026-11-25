# 审查报告 · 4j-yaw-m2006（第 1 轮）

- VERDICT: CHANGES_NEEDED
- MAX_SEVERITY: P1
- REVIEW_ROUND: 1
- BRANCH: auto/4j-yaw-m2006
- COMMIT_SHA: 未提交工作区改动（基线 HEAD `e17b490e8ffbefedc08c96d8d74396ba257487de`，未提交 diff SHA256 `4308cd4673d5ca6a23bf4c245ca66fd195acdaae4b883a24f9cca53de4002d49`）

## 一、范围与门禁核验

- **授权文件一致性**：通过。`git status -s` 与 `git diff --stat` 仅包含定稿 v2 授权的 8 个文件（`README.md`、`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`）；`src/launcher_io.hpp` 仅修改 `SetYaw` 单位注释。未修改 `MotorPort`/`RMMotor`、`Safety`/`LaunchSequence`/`MatchFlow`/`ModeLogic`、`YawGeometry`、CAN BSP、4g 拉力、Referee 或 `dart-fw` YAML。`artisans_dart.yaml` 核对保持 `yaw_motor` 在 `can2`、`MOTOR_M2006`、`reverse: false`、`feedback_id: 514`（`0x202`）；`kYawInstalled=true` 与 4i（`kArmInstalled=false`、`kCrankInstalled=false`、`kReloadInstalled=false`）、4g、pitch/滑块配置相容；无小于 1000 s 的外层回零超时。
- **编译与测试证据**：部分通过（证据真实可复核，但存在 P1 逻辑缺陷及测试覆盖缺口）。
  - 独立复核 `build-host/primary-freeze.json` 中 8 个授权文件 SHA256 与当前工作区一致；`git diff` 字节 SHA256 为 `4308cd4673d5ca6a23bf4c245ca66fd195acdaae4b883a24f9cca53de4002d49`（45,044 B），与 `build-host/primary-final.diff` 一致。
  - 只读运行既有验证二进制：`.\build-host\host_primary_4j_verify_r2.exe` 输出 `PASS: 2335266 checks`（退出 0）；`.\build-host\hx711_primary_4j_verify_r2.exe` 输出 `PASS: 2193 HX711 checks`（退出 0）。
  - 独立核验 `build-host/arm_primary_evidence.json`（SHA256 `5d58decd57b4db65c7f0ebb3057f68231693b7e8765a79b7dcff466551d41c50`）：25 个 DartLauncher ARM 编译单元全部退出 0、无 stderr 警告；输入路径及 `-I` 前缀全部指向隔离工作区 `auto-4j-yaw-m2006`，无残留 `Modules/local/DartLauncher` 引用；25 份 `input_sha256` 与当前源码一致，25 份 `object_sha256` 与 `build-host/arm_primary_*.o` 一致。
  - 独立扫描顶层 `src/*.{hpp,cpp}`：无新增 HAL/LibXR/FreeRTOS 依赖。
- **分支与工作区卫生**：通过。当前分支 `auto/4j-yaw-m2006`，HEAD 为基线 `e17b490e8ffbefedc08c96d8d74396ba257487de`（未提交、未改写历史），`git diff --check` 无异常，`build-host/` 已被 `.gitignore` 忽略且未混入源码改动。

## 二、问题清单

- [P1] `src/motor_axis.cpp:340` | 现象：在 `MotorAxis::Update(double dt, bool enabled, bool online)` 的 `if (!enabled)` 分支中，对 `config_.dual_end_home` 无条件调用 `Stop()`，未区分“正在回零/未回零”与“已完成回零（`Homed()`）”。 | 根因/风险：`Stop()` 会将 `active_` 清零为 `false`。一旦已回零的 yaw 经历任意一次 `enabled=false`（例如“手动→无力→自动”、“自动→无力→自动”，或自动/比赛流程中触发任意短暂可恢复故障如 `RemoteOffline`/`RefereeOffline`/`GateClosed` 并复位后重新使能，期间不会再触发 `leave_manual` 的 `StopMotors()`/`HoldPosition()`），`yaw_` 的 `Homed()` 仍为 `true` 但 `active_` 永久为 `false`。因为生产代码无 `SetYaw()` 调用，恢复使能后 `yaw_` 输出恒为 0 不再保持位置，且 `MotorAxis::Settled()` 因要求 `active_` 恒返回 `false`，导致 `LauncherHardware::Aimed()` 永久为 `false`，后续自动发射在 `"瞄准：等 yaw / pitch 到位"` 步必超时失败（违反定稿“回零完成后切自动仿照 pitch 保留当前位置目标；无力/故障下 Gate 照常零电流，仅未完成阶段遇 enabled=false 清状态”的契约）。 | 建议：仅在 `!Homed()` 时于 `!enabled` 中调用 `Stop()`；若 `Homed()` 且当时处于 `jogging_`，调用 `HoldPosition()` 转为原地保持（仍返回 0），保持与 `pitch_` 一致的断力/复使能语义。
- [P1] `test/host_test.cpp:5435-5486` | 现象：新增 `TestDualYaw()` 存在两处逐字重复的测试块（`5435-5438` 与 `5439-5442` 重复，`5451-5461` 与 `5462-5472` 重复），且关键定稿场景存在覆盖缺口或断言掩盖。 | 根因/风险：1）未测试 `Homed()` 状态下 `enabled=false -> enabled=true` 后的 `Settled()` 与位置保持，导致上述 P1 缺陷未被检出；2）`5435-5438` 用 `high=91*2*kPi` 测试超长跨度，由于退离 0.5 圈后 `SeekSecond` 走满 90 圈即先触发 `Overrun`，并用 `Overrun || SpanLong` 放过，导致 `DualHomeFault::SpanLong` 分支从未被真实触发；3）未测试未回零状态下双端轴允许双向 `Jog(+/-)`、限流 848 且不置 `Homed`，以及 `BackoffFirst`/`BackoffSecond` 的 30 s 超时分支；4）`5474-5486` 的 4f 联动测试在调用 `MatchFlow::Update` 前先调用了 `p.Start()`，使 `MatchFlow` 首拍看到 `m.homing=true` 而停在 `Pending`，未真正经过 `auto_home_aim -> WaitAim -> WaitSlider` 完整转移。 | 建议：删除两处重复代码块，补齐上述 4 项真实动态测试用例并精确断言 `DualHomeFault::SpanLong`。
- [P2] `src/motor_axis.cpp:554` | 现象：在 `StepDualHome` 的退离与回中阶段，`if (std::fabs(error) >= config_.home_tolerance && Contact(dt, command, fresh))` 仅在误差大于等于容差时调用 `Contact()`，误差小于容差时未重置接触窗口。 | 根因/风险：若误差在 `home_tolerance` 边界附近小幅往复且位移落在 0.05 转子圈内，前一次累积的 `contact_elapsed_` 不会清零，可能在非连续时段被累加导致误判阻塞。 | 建议：在 `std::fabs(error) < config_.home_tolerance` 时补 `else { ResetContact(); }`。
- [P2] `src/motor_axis.cpp:524-529` | 现象：`BackoffFirst`/`BackoffSecond` 阶段 `target_` 从接触瞬间位置按 `speed * dt` 线性推进；若断流瞬间存在机械弹性回弹使实际 `Position()` 沿退离方向超前于 `target_`（尚未到达 `end_`），位置环误差反向，会短暂输出朝向硬限位的反向电流直至 `target_` 追上。 | 根因/风险：丝杠传动回弹通常很小，但严格按定稿“下一阶段仅施加退离方向控制”要求，退离阶段不应把已沿退离方向弹开的轴再拉回端点。 | 建议：仅在非回中退离阶段（`!centering`），若 `Position()` 已位于 `target_` 与 `end_` 之间，先将 `target_` 单调推进至 `Position()` 再计算 `loop_.Position`。
- [P2] `src/hw/launcher_hw.cpp:435-440` | 现象：控制线程向 `monitor_feedback_` 拷贝 `yaw_` 快照字段时未按 `kYawInstalled ? ... : 0` 保护，与相邻 `pitch_`、`arm_` 字段风格不一致。 | 根因/风险：当前 `kYawInstalled=true` 且 `OnMonitor()` 内已有 `if constexpr (kYawInstalled)` 保护，无功能风险；仅在未来临时关回 `kYawInstalled=false` 时会多拷未安装端口的默认快照值。 | 建议：与 `pitch_`/`arm_` 保持一致，加上 `kYawInstalled ? ... : ...` 条件保护。

## 三、下一步动作

本轮结论为 `CHANGES_NEEDED`（`MAX_SEVERITY: P1`，第 1 轮），未命中任何 P0 熔断条件，请按以下精确修订任务单返修（仅修改授权文件，不提交/不推送/不烧录）：

1. **修复已回零双端轴断力后 `active_` 丢失导致 `Aimed()` 永久失效（P1）**
   - **目标文件与位置**：`src/motor_axis.cpp:339-341`（`MotorAxis::Update` 的 `if (!enabled)` 分支）。
   - **改成什么**：将 `if (config_.dual_end_home) { Stop(); }` 改为：
     ```cpp
     if (config_.dual_end_home) {
         if (!Homed()) { Stop(); }
         else if (jogging_) { HoldPosition(); }
     }
     ```
     确保 5 个未完成回零阶段与未回零手动点动遇 `enabled=false` 仍立即 `Stop()` 清测量并禁止自启，而 `Homed()` 后遇 `enabled=false` 输出 0 且保留 `active_`、目标与动态边界（若断力瞬间在 `jogging_` 则切入当前位置 `HoldPosition()`），复使能后立即可保持且 `Settled()`/`Aimed()` 为真。
   - **修复后验证**：在 `test/host_test.cpp` 的 `TestDualYaw()` 中新增用例：完成回零后（以及 `Homed()` 下 `Jog` 期间）执行 `Update(0.001, false, true)` 确认输出 0，随后恢复 `Update(0.001, true, true)`，断言 `Homed()`、`BoundariesValid()`、`Settled(kYawTolerance, kYawVelocityTolerance)` 均为真，且外力偏移后可输出非零保持电流；同时确认 5 个未完成阶段遇 `enabled=false` 的既有“清测量且不自启”断言继续通过。

2. **清理重复测试并补齐缺失动态覆盖（P1）**
   - **目标文件与位置**：`test/host_test.cpp:5429-5487`（`TestDualYaw()`）。
   - **改成什么**：
     1. 删除 `5439-5442` 与 `5462-5472` 两处重复代码块。
     2. 将超长跨度测试改为 `p.low = 0; p.high = 90.3 * 2 * kPi; p.x = 0;`（退离 0.5 圈后 `SeekSecond` 行程 89.8 圈 <= 90 圈，准确命中两端跨度 > 90 圈），精确断言 `p.axis.DualFault() == DualHomeFault::SpanLong`（保留 `low = -1000` 单独覆盖 `Overrun`）。
     3. 新增未回零双向手动点动测试：未回零时分别 `Jog(+2 * kYawHomeSpeed)` 与 `Jog(-2 * kYawHomeSpeed)` 均返回 `true`、电流绝对值 `<= kYawHomeCurrent`（848）、`!Homed()`；释放 `Jog(0)` 后输出归零。
     4. 新增退离 30 s 超时测试：在 `BackoffFirst` 持续反馈缓慢移动/未达 `home_tolerance` 且不满足 `Contact()`，推进 `> 30 s` 断言 `DualHomeFault::BackoffBlocked`。
     5. 调整 4f 联动测试：初始不预先调用 `p.Start()`，由 `cmd.salvo = true` 驱动 `MatchFlow` 发出 `cmd.auto_home_aim` 后再调用 `p.Start()`（模拟 pitch 已完成而 yaw 经历全部 5 阶段含 `Centering`），断言全过程 `WaitAim` 期间不发出 `cmd.auto_home_slider`，直至 `p.axis.Homed()` 后才发出 `cmd.auto_home_slider`。
   - **修复后验证**：重新编译并运行主机测试与 HX711 测试（`-Wall -Wextra -Werror` 零告警且全部 PASS），并重跑隔离工作区 ARM 25 单元预检更新证据与冻结哈希。

3. **顺带并入的 P2 修复（同次修订单）**
   - **目标文件与位置**：`src/motor_axis.cpp:524-529` 与 `554-556`、`src/hw/launcher_hw.cpp:435-440`。
   - **改成什么**：
     - `src/motor_axis.cpp:554-556` 在 `std::fabs(error) < config_.home_tolerance` 时补 `else { ResetContact(); }`。
     - `src/motor_axis.cpp:524-526` 在 `!centering` 退离阶段，若 `Position()` 已在 `target_` 与 `end_` 之间（正向退离 `Position() > target_ && Position() < end_`，反向退离 `Position() < target_ && Position() > end_`），将 `target_` 更新为 `Position()` 后再做斜坡步进，避免回弹时产生朝向硬限位的反向电流。
     - `src/hw/launcher_hw.cpp:435-440` 对 `monitor_feedback_.yaw_*` 补 `kYawInstalled ? ... : ...` 保护，与 `pitch_`/`arm_` 一致。
   - **修复后验证**：主机测试与 ARM 25 单元预检全部通过，且新增/复测退离阶段弹性回弹场景无反向电流脉冲。

### 未确认项与后续集成/现场门禁（不构成本轮代码失败，须在后续门禁闭环）

- **完整固件 ELF 未构建**：当前仅完成 25 个 DartLauncher 源码单元的 ARM 交叉编译预检（`-c` 生成 `.o`），尚未在 `dart-fw` 中同步模块并执行完整链接。完整链接时需确认：1）`Modules/local/DartLauncher` 同步后无 ODR/符号冲突；2）`OnMonitor()` 新增的两条 `LibXR::STDIO::Printf` 模板实例化后 `.text`/`.rodata` 与控制/监控线程栈水位正常。
- **未烧录与现场机械/电控待确认项**：
  1. CAN2 / C610 ID2（`0x202`）实车在线与方向极性（负向寻端对应现场左还是右）；
  2. 首轮 `kYawHomeCurrent = 848`（0.848 A）在实车静摩擦与负载下能否稳定带动且不在中途误触发接触；
  3. 实测两端跨度是否落在 `4~90` 输出轴圈内，以及算术中点相对实际发射架正前方的偏置；
  4. USB CDC 128 字节发送缓冲下 `OnMonitor()` 第 5 行及新增两行 yaw 日志是否完整不截断；
  5. 4f 自动回零实车时序（pitch 先完成后等待 yaw 回中再启动滑块，且 4i 换弹关闭时自动停住不发射）。

VERDICT: CHANGES_NEEDED
