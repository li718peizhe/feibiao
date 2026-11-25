# 4j-yaw-m2006 第 1 轮返修报告

- 工作区：`C:/Users/Lenovo/orca/workspaces/DartLauncher/auto-4j-yaw-m2006`
- 分支：`auto/4j-yaw-m2006`（基线 HEAD：`e17b490e8ffbefedc08c96d8d74396ba257487de`，未提交供独立审查）
- 授权改动文件（8 个）：`README.md`、`src/dart_params.hpp`、`src/motor_axis.hpp`、`src/motor_axis.cpp`、`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`、`src/launcher_io.hpp`、`test/host_test.cpp`

## 一、返修项目完成情况

1. **[P1] `src/motor_axis.cpp:339-343` 已回零双端轴断力复使能**
   - 在 `MotorAxis::Update(double dt, bool enabled, bool online)` 的 `if (!enabled)` 分支中，仅在 `!Homed()` 时调用 `Stop()`；若 `Homed()` 且当时处于 `jogging_`，调用 `HoldPosition()` 转为原地保持（仍返回 `0`），保持与 `pitch_` 一致的断力/复使能语义。

2. **[P1] `test/host_test.cpp:5429-5506` 清理重复测试并补齐覆盖**
   - 已删除两处重复代码块，当前 `TestDualYaw()` 无重复断言。
   - 新增 `Homed()`（含 `jogging_ == false` 与 `jogging_ == true`）下 `enabled=false -> enabled=true` 恢复使能验证：断力返回 `0`，复使能后 `Homed()`、`BoundariesValid()`、`Settled(kYawTolerance, kYawVelocityTolerance)` 均为真，且位置偏移后输出非零保持电流；同时 5 个未完成阶段遇 `enabled=false` 的清测量/不自启断言继续通过。
   - 超长跨度测试更新为 `p.low = 0; p.high = 90.3 * 2 * kPi; p.x = 0;`，精确断言 `p.axis.DualFault() == DualHomeFault::SpanLong`（保留 `low = -1000` 单独覆盖 `Overrun`）。
   - 新增未回零双向手动点动测试：`Jog(-2 * kYawHomeSpeed)` 与 `Jog(+2 * kYawHomeSpeed)` 均返回 `true`、电流绝对值 `<= kYawHomeCurrent`（848）、`!Homed()`；释放 `Jog(0)` 后输出归零。
   - 新增 `BackoffFirst` 与 `BackoffSecond` 30 s 超时测试：持续反馈未达 `home_tolerance` 且不满足 `Contact()`，推进 `> 30 s` 断言 `DualHomeFault::BackoffBlocked`。
   - 调整 4f 联动测试：初始不预先调用 `p.Start()`，由 `cmd.salvo = true` 驱动 `MatchFlow` 发出 `cmd.auto_home_aim` 后再调用 `p.Start()`，断言 `WaitAim` 期间不发出 `cmd.auto_home_slider`，直至 `p.axis.Homed()` 后才发出 `cmd.auto_home_slider`。

3. **[P2] 3 项顺带修复**
   - `src/motor_axis.cpp:561-565`：在 `std::fabs(error) < config_.home_tolerance` 时补 `else { ResetContact(); }`。
   - `src/motor_axis.cpp:528-531`：在 `!centering` 退离阶段，若 `Position()` 已在 `target_` 与 `end_` 之间，将 `target_` 更新为 `Position()` 后再做斜坡步进；并在 `test/host_test.cpp:5500-5505` 补充退离弹性回弹场景断言无朝向硬限位的反向电流脉冲。
   - `src/hw/launcher_hw.cpp:435-440`：对 `monitor_feedback_.yaw_*` 补 `kYawInstalled ? ... : ...` 保护，与 `pitch_`/`arm_` 一致。

## 二、门禁验证结果

### 1. 主机测试（`-std=c++17 -Wall -Wextra -Werror -O1`，零告警）
- 主回归（`build-host/host_repair_r1_verify.exe`）：`PASS: 2125939 checks`（退出码 0）
- HX711 驱动主机测试（`build-host/hx711_repair_r1_verify.exe`）：`PASS: 2193 HX711 checks`（退出码 0）

### 2. ARM 交叉编译预检（25 个 DartLauncher 源码单元）
- 运行 `python build-host/arm_primary_precheck.py`：25 个编译单元全部退出码 `0`、stderr 无告警。
- `src/*.{hpp,cpp}` 依赖检查：`dependency_violations: []`（无新增 HAL/LibXR/FreeRTOS 依赖）。
- `git diff --check`：无空白/格式错误。

## 三、产物与文件 SHA-256

### 构建与冻结证据
- `build-host/primary-freeze.json`：`0cf844377e4003709fc43bf71c59205200318e06ad9f89759e28ac989948f2c7`
- `build-host/primary-final.diff`（46,357 B）：`4ca0e3a8cdd7269b1dec9944f592a643df2d0cca46d889f4e543d8079ca6a884`
- `build-host/arm_primary_evidence.json`：`9563ff7042c27db279caa321407a6d686692d07d469ade8971f4e2c65aa0a000`

### 8 个授权文件 SHA-256
- `src/dart_params.hpp`：`3120d5862bc69318b26986069a80a633617f3e4e6fa25cf2dae853a15d4d9b20`
- `src/motor_axis.hpp`：`52f062a0e386a8eaae47687d1e5014aedbabc88207db74dcc117343603f99f1c`
- `src/motor_axis.cpp`：`d906c5190fa67b6d642ee62aaf6fdb3cfa15d2ea4dbe9c97edbdc661e8fac7c5`
- `src/hw/launcher_hw.hpp`：`95c99fa49d2ec1d6ee22b956ad11393b52aa9250ab841ab9ef9af47781c48cb9`
- `src/hw/launcher_hw.cpp`：`e6bcd4100b3ed8335213077bbc2091272342ea343ea05c557fb8360baf28ffc5`
- `src/launcher_io.hpp`：`3d93f36be1241768442c76bd74ca1fa99888fb88ae34beeb67a5cc483a04e52a`
- `test/host_test.cpp`：`e652853187ada9976c5317f8c451bb14cf1fc244ad965a39b550a60d0a92f967`
- `README.md`：`2673477e3f8d81fcbf78dc4ac810cf53e80a0c6a09d8178b23b109f05e928c93`
