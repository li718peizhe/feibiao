# 4j-yaw-m2006 第 1 轮返修任务（恢复执行）

请在隔离工作区 `C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`（分支 `auto/4j-yaw-m2006`）继续完成第 1 轮返修。

## 参考依据
- 冻结执行定稿：`C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-dual-end-final-v2.md`
- 第 1 轮审查报告：`C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-m2006-review-r1.md`

## 边界与禁止事项
- 仅允许修改定稿 v2 授权的 8 个文件（`README.md`、`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`）。
- 严禁修改 `MotorPort`/`RMMotor`、`Safety`/`LaunchSequence`/`MatchFlow`/`ModeLogic`、`YawGeometry`、CAN BSP、4g 拉力、Referee 或 `dart-fw` YAML。
- 严禁 `git commit`、`git push`、开 PR 或烧录固件；保留未提交状态供独立审查。

## 必须修复的问题（来自 review-r1）
1. **[P1] `src/motor_axis.cpp:340` 已回零双端轴断力复使能失效**：
   - 在 `MotorAxis::Update(double dt, bool enabled, bool online)` 的 `if (!enabled)` 分支中，仅在 `!Homed()` 时调用 `Stop()`；若 `Homed()` 且当时处于 `jogging_`，调用 `HoldPosition()` 转为原地保持（仍返回 0），保持与 `pitch_` 一致的断力/复使能语义。
2. **[P1] `test/host_test.cpp:5429-5487` 清理重复测试并补齐覆盖**：
   - 删除 `5439-5442` 与 `5462-5472` 两处重复代码块。
   - 新增 `Homed()` 状态下（以及 `Homed()` 下 `Jog` 期间）`enabled=false -> enabled=true` 恢复使能验证：断言 `Homed()`、`BoundariesValid()`、`Settled(kYawTolerance, kYawVelocityTolerance)` 为真，且外力偏移后可输出非零保持电流；同时确认 5 个未完成阶段遇 `enabled=false` 的清测量/不自启断言仍通过。
   - 将超长跨度测试改为 `p.low = 0; p.high = 90.3 * 2 * kPi; p.x = 0;`，精确断言 `p.axis.DualFault() == DualHomeFault::SpanLong`（保留 `low = -1000` 单独覆盖 `Overrun`）。
   - 新增未回零双向手动点动测试：`Jog(+2 * kYawHomeSpeed)` 与 `Jog(-2 * kYawHomeSpeed)` 均返回 `true`、电流绝对值 `<= kYawHomeCurrent`（848）、`!Homed()`；释放 `Jog(0)` 后输出归零。
   - 新增 `BackoffFirst` 30 s 超时测试：持续反馈缓慢移动/未达 `home_tolerance` 且不满足 `Contact()`，推进 `> 30 s` 断言 `DualHomeFault::BackoffBlocked`。
   - 调整 4f 联动测试：初始不预先调用 `p.Start()`，由 `cmd.salvo = true` 驱动 `MatchFlow` 发出 `cmd.auto_home_aim` 后再调用 `p.Start()`，断言 `WaitAim` 期间不发出 `cmd.auto_home_slider`，直至 `p.axis.Homed()` 后才发出 `cmd.auto_home_slider`。
3. **[P2] 3 项顺带修复**：
   - `src/motor_axis.cpp:554-556`：在 `std::fabs(error) < config_.home_tolerance` 时补 `else { ResetContact(); }`。
   - `src/motor_axis.cpp:524-529`：在 `!centering` 退离阶段，若 `Position()` 已在 `target_` 与 `end_` 之间（正向退离 `Position() > target_ && Position() < end_`，反向退离 `Position() < target_ && Position() > end_`），将 `target_` 更新为 `Position()` 后再做斜坡步进，避免回弹产生朝向硬限位的反向电流；并补充/复测退离弹性回弹场景。
   - `src/hw/launcher_hw.cpp:435-440`：对 `monitor_feedback_.yaw_*` 补 `kYawInstalled ? ... : ...` 保护，与 `pitch_`/`arm_` 一致。

## 门禁与交付
- 重新编译并运行主机测试与 HX711 测试（`-Wall -Wextra -Werror` 零告警且全部 PASS）。
- 重跑隔离工作区 25 个 DartLauncher 源码单元的 ARM 交叉编译预检，更新 `build-host/primary-freeze.json`、`build-host/primary-final.diff`、`build-host/arm_primary_evidence.json`（及各自 SHA-256）。
- 将返修结果、测试输出与 SHA-256 写入 `C:/Users/Lenovo/Desktop/xrobot/plans/tasks/4j-yaw-dual-worker-repair-r1.md`，完成后通过 `orca orchestration send --type worker_done --outcome succeeded` 汇报协调器。
