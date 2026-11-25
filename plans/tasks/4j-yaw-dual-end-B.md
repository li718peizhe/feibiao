# 任务 4j：yaw M2006 双端寻端回中与自动回零集成实施方案（独立方案 B）

- **任务号**：4j（归属主对话 Orca item 5；用户已批准范围修订：直接自动找左右终点并回中，合并到自动回零功能）
- **日期**：2026-11-24
- **状态**：已规划（独立 B 版草稿，供主对话与 A 版合并）
- **角色**：planner (B版)
- **基线提交**：
  - `xrobot-local/DartLauncher`：分支 `codex/apply-load-position-calibration`，HEAD `e17b490e8ffbefedc08c96d8d74396ba257487de`（4i 临时关闭摆臂/推杆已合入；主机基线 `PASS: 31569 checks` + `PASS: 2193 HX711 checks`）
  - `dart-fw`：分支 `dart-launcher`，HEAD `fa0838b6b405db715d3be8eb552784da49f3cc35`；`Modules/local/DartLauncher` 与 `xrobot.lock` 锁定 `e17b490e8ffbefedc08c96d8d74396ba257487de`；当前 `dart-fw/build/debug/DevC.elf` SHA-256 `C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`
  - `dart-fw/Modules/QDU-Robomaster/Referee`：3 处本地补丁原样保留（`Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp`）
- **独立与只读边界**：未读取主对话本轮 A 方案及任何其他新草稿；不沿用冻结旧定稿的单边 12° 模型；未修改源码/测试/配置/`handoff.md`，未构建/提交/烧录，未另建 item。

---

## 1. 需求理解与边界

### 1.1 做什么
1. 启用已安装的 yaw M2006（**CAN2 PB5/PB6，C610 电调编号 2，反馈 ID `0x202` / `514`，控制帧 `0x200` 槽位 1**）。
2. 实现**双端自动寻端 → 退出接触 → 校验实测行程 → 限速回行程中点 → 居中稳定确认**的完整回零流程，并动态建立以中点为 0 的对称软限位。
3. 将 yaw 双端寻端回中统一合入全部现有回零入口：手动瞄准回零（`StartHome(ManualGroup::Aim)`）、4f 普通自动回零（`MatchFlow::AutoHomeStart`）及 3c 比赛自动回零（`MatchFlow::NeedsHoming`），确保自动流程严格等待 yaw 真正回中稳定后才进入滑块回零与后续步骤。
4. 保持严格物理语义诚实：**不以电机/联轴器角度冒充发射架偏航角**；**不将转子电流堵转无条件视为真实机械端点**；**绝不自动升级电流**。

### 1.2 不做什么
- 不修改滑块托持（`SliderMustHold`）、锁止（`LockOpenAllowed`）、比赛安全门控（`OutOfMatch` / `FireBlocker`）规则，不改动 pitch 参数、4g RS485 读数及 4i 临时关闭摆臂/推杆配置（`kArmInstalled=false`、`kCrankInstalled=false`、`kReloadInstalled=false`）。
- 不编造发射架偏航角几何换算（实车导程与销轴几何留待实测后另立任务标定）。
- 源码提交尚未获明确用户授权：本计划区分“本地实施与验证”和“授权后提交/同步”，默认只做本地交付。

---

## 2. 现状核对与关键代码依据

1. **硬件与 CAN2 分组现状**：
   - `dart-fw/User/RobotConfig/artisans_dart.yaml` L38–45、L77、L82 及 `dart-fw/User/xrobot_main.hpp` L68–76、L108–114：`yaw_motor` 已原生配置为 `can_bus: can2`、`MOTOR_M2006`、`reverse: false`、`feedback_id: 514`（`0x202`），`dart_launcher` 已绑定 `yaw_can: can2`、`yaw_id: 514`。**无需改 YAML 或运行 `xrobot gen`**（仅实车确认左右反向时才改 `yaw_motor.reverse: true`）。
   - `dart-fw/Core/Src/main.c` L339–361 与 `stm32f4xx_hal_msp.c` L230–260：CAN2（PB5 RX / PB6 TX）已 1 Mbps 初始化并使能中断。
   - `dart-fw/Modules/QDU-Robomaster/RMMotor/RMMotor.hpp` L116–231、L456–487：`yaw_motor`（M2006 `0x202`）独占 CAN2 控制组 `0x200`（`group_mask = 0x02`），与 `arm_motor`（GM6020 `0x205` → `0x1FE`）分组隔离，与 4i 拆除 6020 完全相容。
2. **既有 yaw 轴与 4f/3c/Safety 链路缺陷核查**：
   - `src/hw/launcher_hw.cpp` L42–45：`YawConfig()` 仍按旧单边 `0 ~ Slide(12°)`（0~125.7 mm）单向 `home_direction = -1` 堵转回零，无双端寻端、无 `hold_on_release`、无 `home_seek_limit`。
   - `src/hw/launcher_hw.cpp` L532–533：`OnMonitor()` 第 5 行经 `YawGeometry::Angle(feedback.position[3])`（`src/yaw_geometry.cpp` L11–14）把负位移夹死为 `0.00 deg` 并以旧 CAD 角度冒充发射架角，且缺少 yaw 电流/转速诊断行。
   - `src/hw/launcher_hw.cpp` L922：`StopMotors()` 直接调用 `yaw_.Axis().Stop()`，离开手动或停机时把 `active_` 清零，导致 `Aimed()`（L859–863）返回 `false`、发射流程卡 `NotAimed` / `kAimTimeout = 20 s` 超时。
   - `src/motor_axis.cpp` L325–336：`Update()` 在 `enabled == false`（无力模式）时仅清 `loop_` 与 `stall_time_`，**未将 `Seeking/Retreating` 退回 `Idle`**；若无力后未触发 `StopMotors()`，重新上力可能延续半截回零状态。
   - `src/hw/launcher_hw.cpp` L217–223 & `src/match_flow.cpp` L51–60、L156–210：4f 自动回零与 3c 自动回零统一依赖 `mechanism.aim_homed = (!kPitchInstalled || pitch_.Axis().Homed()) && (!kYawInstalled || yaw_.Axis().Homed())` 与 `mechanism.homing = ... || (kYawInstalled && Homing(yaw_.Axis()))`（`Homing()` 定义于 `launcher_hw.cpp` L22–24：`Seeking || Retreating`）。
   - `src/dart_params.hpp` L151：`kHomeTimeout = 90 s`（硬编码在 `motor_axis.cpp` L343）。对于双端寻端（最坏 `2.5 × 全行程`）若全程长达 77.5 圈（旧参数 310 mm / 4 mm 导程），温和速度下 90 s 余量不足，须支持轴级独立 `home_timeout`。
   - `RMMotor.hpp` L437–438、L560–561、L582–583：M2006 反馈力矩按 `raw_current * 0.005f * 10.0f / 16384.0f` 解码，反算电调原始反馈电流须用 `torque / 0.005 / 10.0 * 16384`。

---

## 3. 推荐设计方案

### 3.1 架构选型：在 `MotorAxis` 内做正交双端扩展（优于另起专用轴/Port）
- **对比权衡**：
  - *方案 1（新建 `YawAxis` + `YawPort`）*：需拆改 `launcher_hw.cpp` 6 处 5 电机统一数组循环（L121、L166、L251、L378、L408、L913）或把 `MotorPort` 改为虚基类/模板，侵入面大、重复维护多圈计数与在线门控。
  - *方案 2（推荐：`MotorAxis` 正交扩展 `dual_end_home` 模式）*：`MotorPort`、5 电机循环、`RotorCounter`、`MatchFlow` 均 100% 复用。仅当 `config_.dual_end_home == true` 时走双端回零状态机；对 `dual_end_home == false` 的滑块/pitch/arm **零行为变化**。
- **关键外部契约不变**：
  - 双端寻端全程（找第一端、第一端退离、找第二端、第二端退离）对外呈 `HomeState::Seeking`；
  - 校验行程通过、建立居中零点并向中点限速移动及稳态确认期间对外呈 `HomeState::Retreating`；
  - 居中稳态确认通过后才切入 `HomeState::Homed`（同时 `active_ = true, target_ = end_ = 0.0`，直接进入位置保持且 `Settled() == true`）。
  - 由此，手动瞄准回零、4f 普通自动回零、3c 比赛自动回零**天然一致**，无需修改 `MatchFlow` 一行代码，且自动流程必然等待 yaw 真正回中稳定后才启动滑块回零。

### 3.2 计数单位与坐标定义（严禁冒充发射架角度）
- **轴坐标单位**：**M2006 减速箱输出轴（联轴器）弧度 `rad`**（中点为 `0 rad`）。
  - `counts_per_unit = kM2006Ratio * kEncoderRange / (2.0 * kPi)`（`36 * 8192 / 2π ≈ 46936.698 counts/rad`，1 联轴器圈 = `2π rad` = 294,912 转子 counts）。
  - **优点**：仅依赖 M2006 固有减速比 36:1 与 8192 线编码器，零几何猜测；严格遵守项目单位规范（m/rad/s/N/A）；绝不把联轴器/电机角度冒充为发射架偏航角。
  - **动态软限位**：双端实测跨度 `span = |end_second - end_first|` 校验通过后，将中点设为 `0 rad`（`offset_ = -0.5 * (end_first + end_second)`），并将轴运行时软限位 `soft_min_` / `soft_max_` 更新为 `[-0.5 * span + config_.end_margin, +0.5 * span - config_.end_margin]`；之后 `SetTarget`、`RampTo`、`Jog`、`HoldPosition` 全部按实测对称软限位约束。
  - **PID 增益兼容**：`MotorAxis` 内部级联环输入恒为转子 `counts` 与转子 `rpm`（`motor_axis.cpp` L405），与轴坐标单位无关，既有 `kYawPos`、`kYawSpeed` 初值可无缝沿用。

### 3.3 双端寻端回中状态机（`DualHomeStage`）与防假堵转策略

| 子状态 | 外部 `HomeState` | 控制方式与限流 | 成功迁移条件 | 故障 / 拒绝条件（**不自动加流**） |
|---|---|---|---|---|
| `SeekFirst` | `Seeking` | 速度环 `d1 * home_speed`（`d1 = -1`），固定限流 `low_limit` | `|cmd| >= low_limit && |rpm| < kHomeStallRpm` 持续 `home_stall_time` → 记 `end_first_`（允许初始已在端面位移≈0） | 单段位移 `> home_seek_limit` → `HomeFault::Overrun`；总耗时 `> home_timeout` → `HomeFault::Timeout` |
| `BackoffFirst` | `Seeking` | 位置环退向 `end_first_ - d1 * retreat`，限流 `low_limit` | 离 `end_first_` 达到 `retreat - home_tolerance` 且 `|rpm| < kHomeStallRpm` | 退离耗时 `> backoff_timeout` 或反向堵转满 `home_stall_time` → `HomeFault::BackoffBlocked`（识别双向死卡） |
| `SeekSecond` | `Seeking` | 速度环 `-d1 * home_speed`，固定限流 `low_limit` | 堵转满 `home_stall_time` → 记 `end_second_`，计算 `span = |end_second_ - end_first_|` 并通过 `min_span <= span <= max_span` 校验 | `span < min_span` → `HomeFault::SpanTooShort`（识别中途卡住/假堵转）；`span > max_span` 或单段超 `home_seek_limit` → `HomeFault::Overrun`；总超时 → `Timeout` |
| `BackoffSecond` | `Seeking` | 位置环退向 `end_second_ + d1 * retreat`，限流 `low_limit` | 离 `end_second_` 达到 `retreat - home_tolerance` 且 `|rpm| < kHomeStallRpm` → 建中点零位 `offset_`、写对称软限位、切 `Centering` | 退离超时或反向堵转 → `HomeFault::BackoffBlocked` |
| `Centering` | `Retreating` | 斜坡 `rate_ = center_speed` 走回 `end_ = 0.0`，位置-速度环仍限流 `low_limit` | 斜坡走完（`target_ == 0`）、`|Position()| < home_tolerance` 且 `|rpm| < kHomeStallRpm` **连续保持 `center_settle_time`** → 置 `HomeState::Homed`、`active_ = true` | 距中点 `>= home_tolerance` 时持续堵转满 `home_stall_time` → `HomeFault::CenterBlocked`；总超时 → `Timeout` |

### 3.4 关键常量首轮初值（基于项目既有命名初值，不发问卷）

| 常量（`src/dart_params.hpp`） | 建议首轮初值 | 换算参考与依据 |
|---|---|---|
| `kYawInstalled` | `true` | 开启 CAN2 ID 2 yaw 轴 |
| `kYawCurrentCap` | 沿用 `2829`（±10000 对应 ±10 A，约 2.83 A） | 沿用现有命名初值（`dart_params.hpp` L30） |
| `kYawHomeCurrentFraction` | `kHomeCurrentFraction`（`0.3` → `low_limit = 848` ≈ 0.85 A） | 沿用既有保守限流，绝不自动升级电流 |
| `kYawHomeSpeed` | `2.0 * 2.0 * kPi`（2 联轴器圈/s ≈ 12.57 rad/s） | 转子 4320 rpm（若 4 mm 导程对应 8 mm/s），比旧 10 mm/s 更温和 |
| `kYawCenterSpeed` | `3.0 * 2.0 * kPi`（3 联轴器圈/s ≈ 18.85 rad/s） | 转子 6480 rpm（若 4 mm 导程对应 12 mm/s），已知无障回中且斜坡减速 |
| `kYawEndRetreat` | `0.5 * 2.0 * kPi`（0.5 圈 ≈ 3.14 rad） | 若 4 mm 导程约 2 mm，退出端面接触并验证可反向脱离 |
| `kYawEndMargin` | `1.0 * 2.0 * kPi`（1.0 圈 ≈ 6.28 rad） | 若 4 mm 导程约 4 mm，两端软限位各留 1 圈安全余量 |
| `kYawMinSpan` | `4.0 * 2.0 * kPi`（4.0 圈 ≈ 25.13 rad） | 必须远大于 `2 * kYawEndMargin`（2 圈），拦截中途假堵转 |
| `kYawMaxSpan` | `90.0 * 2.0 * kPi`（90.0 圈 ≈ 565.49 rad） | 若 4 mm 导程对应 360 mm，覆盖旧 `kYawTravel = 310 mm`（77.5 圈）并留余量；超过判打滑 `Overrun` |
| `kYawHomeTimeout` | `150.0` s | 覆盖 90 圈最坏 `2.5 × 行程`（`90/2 + 90/2 + 45/3 = 105 s`）加退离/堵转裕量 |
| `kYawBackoffTimeout` | `3.0` s | 退离 0.5 圈充裕时间，防退离死锁 |
| `kYawCenterSettleTime` | `0.2` s | 与 `kHomeStallTime` 同量级，确保居中真正稳定再报 `Homed` |
| `kYawHomeTolerance` | `0.05 * 2.0 * kPi`（0.05 圈 ≈ 0.314 rad） | 若 4 mm 导程约 0.2 mm |
| `kYawSettledTolerance` / `kYawSettledVelocityTolerance` | `0.05 * 2.0 * kPi` rad / `0.1 * 2.0 * kPi` rad/s | 用于 `Aimed()` 到位判定，与联轴器单位一致 |
| `kYawManualSpeed` | `2.0 * 2.0 * kPi` rad/s（满杆 2 圈/s） | 手动左摇杆精细微调速度 |

### 3.5 异常、取消、门控与故障复位行为规范
- **无力模式（`enabled == false`）**：在 `MotorAxis::Update()` L325–336 增补：若 `home_state_ == Seeking || home_state_ == Retreating`，断力立即将 `home_state_ = Idle`、清零 `dual_stage_` 与 `active_ = false`、输出 0，确保重新上力不会延续半截回零突然动作。
- **取消与 `StopMotors()`**：`StopMotors()`（`launcher_hw.cpp` L922）改为 `if (!yaw_.Axis().HoldPosition()) { yaw_.Axis().Stop(); }`；回零中取消走 `Stop()` 退回 `Idle` 并清双端阶段；已回零后切模式/停机走 `HoldPosition()` 保持原地且 `Aimed()` 保持成立，无力/故障时 `Gate()`/`Update()` 照常输出 0 电流。
- **掉线与重连（`online == false`）**：`MotorAxis::Update()` L306–320 清 `feedback_valid_`、`active_`、双端阶段并置 `Idle`（`Failed` 保留），同时 `Safety` 报 `MotorOffline` 停机（滑块托持照旧）；重连复位后须重新执行完整双端回零。
- **重复回零**：已 `Homed()` 状态下再次 `StartHome()`，立即切离 `Homed()`、清除旧 `measured_span_`、恢复名义软限位并从 `SeekFirst` 完整重跑。
- **未回零手动点动**：`MotorAxis::Jog()` L46 仅对 `config_.dual_end_home == true` 允许未回零时双向限速（`kYawHomeSpeed`）限流（`low_limit`）低速点动，方便首次核对方向与脱困；滑块/pitch 保持原有单向规则不变。
- **故障映射与诊断**：`MotorAxis` 新增只读 `HomeFault()` 枚举（`None / Overrun / Timeout / SpanTooShort / BackoffBlocked / CenterBlocked / InvalidInput`），`launcher_hw.cpp` L126–128 将 `Overrun` 映射 `SafetyFault::HomeOverrun`、`Timeout` 映射 `SafetyFault::Timeout`、其余映射 `SafetyFault::InvalidInput`（或对应既有故障码），并在故障首次锁存时用 `XR_LOG_WARN` 输出 ≤64 B 精确中文原因（如“yaw 回零行程过短或卡阻”“yaw 退离端点受阻”），故障复位（`ClearFault()`）清空故障与双端状态。
- **串口监控输出**：
  - `OnMonitor()` 第 5 行（L520/532）改为打印输出轴圈数 `yaw=%.2f%s`（单位 `"rev"` / `"rev[X]"`），彻底去掉对旧 `YawGeometry::Angle()` 的虚假 `deg` 调用；
  - 新增 `if constexpr (kYawInstalled)` 诊断行（前置 `Sleep(20)`）：`Dart yaw pos=%.2frev span=%.2frev cmd=%d act=%d rpm=%.0f\r\n`（行长 ≤ 65 B），`act` 按 `torque / 0.005 / 10.0 * 16384` 反算。

---

## 4. 精确授权改动文件清单

| 文件路径 | 改动内容 | 公共头文件 | 安全/控制状态机 | 外设/参数 |
|---|---|---|---|---|
| `xrobot-local/DartLauncher/src/dart_params.hpp` | `kYawInstalled = true`；新增双端寻端与输出轴单位命名常量（见 3.4 节） | 是 | 否 | 是 |
| `xrobot-local/DartLauncher/src/motor_axis.hpp` | `AxisConfig` 增加 `dual_end_home`、`home_timeout`、`center_speed`、`end_margin`、`min_span`、`max_span`、`backoff_timeout`、`center_settle_time`（均有向后兼容默认值）；新增 `HomeFault` 枚举与只读 getter（`HomeFault()`、`MeasuredSpan()`、`SoftMin()`、`SoftMax()`）及内部 `DualHomeStage` 状态成员 | 是 | 是（轴内回零状态机，默认关闭不影响其他轴） | 否 |
| `xrobot-local/DartLauncher/src/motor_axis.cpp` | 实现双端寻端回中状态机、动态软限位 `soft_min_`/`soft_max_`、断力清回零态、双端轴未回零双向低速点动 | 否 | 是 | 否 |
| `xrobot-local/DartLauncher/src/hw/launcher_hw.hpp` | `MonitorFeedback` 增加 `yaw_current`、`yaw_feedback_torque`、`yaw_feedback_rpm`、`yaw_span` | 是 | 否 | 否 |
| `xrobot-local/DartLauncher/src/hw/launcher_hw.cpp` | 重写 `YawConfig()` 为输出轴 `rad` + 双端配置 + `hold_on_release=true`；`Manual()` 去掉 `YawGeometry` 换算直接按 `kYawManualSpeed` 点动；`Aimed()` 改用联轴器阈值；`StopMotors()` 改为 `HoldPosition()`；`RefreshState()` 完善 `HomeFault` 映射与限频日志；`OnMonitor()` 第 5 行改 `rev` 并新增 `Dart yaw` 诊断行 | 否 | 轻度（沿用既有 `Homed`/`Homing`/`StopMotors` 契约，不碰 `Safety`/`MatchFlow`/`LaunchSequence`） | 是 |
| `xrobot-local/DartLauncher/test/host_test.cpp` | 更新 L223 `kYawInstalled` 断言；新增 `TestYawDualEndHoming()` 完整动态闭环测试集与 4f 联动断言 | 否 | 否 | 否 |
| `xrobot-local/DartLauncher/README.md` | 更新 `kYawInstalled=true`、yaw 双端自动寻端回中说明、输出轴 `rev` 单位说明及 `Dart yaw` 诊断行格式 | 否 | 否 | 否 |
| `dart-fw/Modules/modules.yaml` & `dart-fw/xrobot.lock` | 仅在用户授权本地提交与同步后，经 `xrobot setup --update local/DartLauncher` 更新（不手改） | 否 | 否 | 否 |
| `dart-fw/User/RobotConfig/artisans_dart.yaml` | **默认不改**；仅当首次点动确认方向相反时修改 `yaw_motor.reverse: true` 并跑 `xrobot gen` | 否 | 否 | 条件触发 |

> **严禁修改**：`src/safety.*`、`src/match_flow.*`、`src/mode_logic.*`、`src/launch_sequence.*`、`src/tension_modbus.*`、`src/hw/tension_port.*`、`dart-fw/Core/Src/main.c`、`dart-fw/Modules/QDU-Robomaster/Referee/*`、4i 的 `kArmInstalled=false`/`kCrankInstalled=false` 及所有 pitch/滑块参数。

---

## 5. 风险、阻塞点与回退点

### 5.1 明确不可在纯代码阶段落实的阻塞/未定项
1. **发射架偏航角真实标定（不可杜撰）**：本轮不输出发射架 `deg`，统一使用输出轴 `rad`/`rev`；真实发射架角度标定需等双端首测拿到 `span (rev)` 与量角器实测角度后再单独立项。
2. **源码提交授权边界**：当前未获用户明确 `git commit` 授权，worker 阶段只做本地修改、主机测试与 ARM 预检；获授权后再执行两仓本地提交、4i ELF 备份（`build/DevC-e17b490-before-4j.elf`）与 debug 构建，绝不 `push` 或自行烧录。

### 5.2 核心运行风险与防护
1. **CAN2 离线连带停机**：开启 `kYawInstalled=true` 后 C610 掉线超 100 ms 将全车报 `MotorOffline`；烧录前须保留 4i 备份 ELF 以便随时回退。
2. **首测限流 `848`（0.85 A）与静摩擦匹配**：若静摩擦大于 0.85 A 推力，双端回零会在 `BackoffFirst` 或 `SpanTooShort` 安全停机报故障，**绝不自动加流**；凭 `Dart yaw` 日志人工调整 `kYawHomeCurrentFraction` 后复测。
3. **瞄准组回零与 pitch 同启**：`StartHome(ManualGroup::Aim)` 会同时启动 pitch 与 yaw；现场触发前必须确认 pitch 位于上端 1–2 cm 以内。
4. **3c 比赛窗口时限**：双端寻端最坏耗时几十秒，开赛后 37 s 首开闸前必须按规则在准备阶段用遥控器完成回零，3c `NeedsHoming` 仅作兜底。

---

## 6. 验证命令与动态测试清单

### 6.1 动态主机测试清单（`test/host_test.cpp` 新增 `TestYawDualEndHoming()`）
基于带真实硬挡块 `[hard_min, hard_max]`、可注入假卡阻与打滑的一阶动力学仿真模型，覆盖 11 组动态用例：
1. **居中起步全闭环**：`[-20, +20] rev` 挡块，起点 `0`，验证依次经历 `Seeking`→退离→对侧 `Seeking`→退离→`Retreating`（回中）→稳定满 `0.2 s` 才 `Homed()==true`，期间 `Homing()==true`、`Homed()==false`，完成后 `Position()≈0`、`MeasuredSpan()≈40 rev`、动态软限位 `[-19, +19] rev`、`Settled()==true`、全程 `|cmd| <= low_limit`。
2. **非对称偏置起点**：挡块 `[-10, +30] rev`、起点 `+14 rev`，验证新坐标系中点精准落于物理 `+10 rev`（新 `Position()==0`）。
3. **开机已在第一端 / 已在第二端**：分别从 `-20 rev` 与 `+20 rev` 启动，验证首段 0 位移堵转不误判，顺利退离并找到对侧端点完成居中。
4. **中途假堵转 / 行程过短**：`[-1, +1] rev` 注入假障碍（跨度 2 rev < `kYawMinSpan = 4 rev`），验证必入 `Failed` + `HomeFault::SpanTooShort`，绝不置 `Homed()`，电流归 0 且不加流。
5. **双向死卡 / 退离受阻**：到第一端后锁死无法退离，验证必入 `Failed` + `HomeFault::BackoffBlocked`。
6. **回中途中受阻**：找完两端后在回中 `Centering` 阶段于 `+5 rev` 注入卡死，验证必入 `Failed` + `HomeFault::CenterBlocked`，不置 `Homed()`。
7. **联轴器打滑 / 单侧无挡块超程**：撤除硬挡块持续空转超过 `kYawMaxSpan`，验证必入 `Failed` + `HomeOverrun()==true`。
8. **总超时保护**：注入极低响应无法在 `kYawHomeTimeout` 内完成，验证必入 `Failed` + `HomeFault::Timeout`。
9. **取消、无力（`enabled=false`）与掉线重连（`online=false`）**：在 `SeekFirst`、`BackoffFirst`、`SeekSecond`、`BackoffSecond`、`Centering` 各阶段分别中断，验证立即输出 0、退回 `Idle`，重新上力/重连绝不自启；`Failed` 后 `ClearFault()` 可恢复再次完整回零。
10. **重复回零**：已 `Homed()` 并点动到 `+6 rev` 后再调 `StartHome()`，验证立刻退出 `Homed()`、完整重跑双端并重新回中；若中途取消则保持未回零。
11. **回零后软限位、松手保持、停机保持与 4f 联动**：点动至 `+8 rev` 松手保持并 `Settled()==true`；向 `+19 rev` 软限位平滑减速不越界；`HoldPosition()` 后保持 `Settled()==true`；联动 `MatchFlow::AutoHomeStart()`，验证 yaw 处于任一双端寻端/回中未稳阶段（即使 pitch 已回零）严格阻挡 `auto_home_slider`，直至 yaw 真正居中稳态后才释放滑块回零。

### 6.2 可执行验证命令（PowerShell）

1. **主机测试与纯逻辑依赖检查**：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher'
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$src = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $src -o build-host/host_test_4j.exe
.\build-host\host_test_4j.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4j.exe
.\build-host\hx711_port_test_4j.exe
Select-String -Path src\*.hpp,src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
```

2. **ARM 单文件预检（替换 `compile_commands.json` 副本路径为源码仓路径，避免假通过）**：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw\build\debug'
$entry = (Get-Content -Raw -LiteralPath 'compile_commands.json' | ConvertFrom-Json) |
    Where-Object { ($_.file -replace '\\', '/') -like '*Modules/local/DartLauncher/src/hw/launcher_hw.cpp' } |
    Select-Object -First 1
if (-not $entry) { throw '未找到 launcher_hw.cpp 编译条目' }
$oldPrefix = 'C:/Users/Lenovo/Desktop/xrobot/dart-fw/Modules/local/DartLauncher'
$newPrefix = 'C:/Users/Lenovo/Desktop/xrobot/xrobot-local/DartLauncher'
$tokens = [regex]::Matches($entry.command, '"([^"]+)"|(\S+)') | ForEach-Object {
    if ($_.Groups[1].Success) { $_.Groups[1].Value } else { $_.Groups[2].Value }
}
$compiler = $tokens[0]
$argsList = [System.Collections.Generic.List[string]]::new()
for ($i = 1; $i -lt $tokens.Count; $i++) {
    $t = $tokens[$i]
    if ($t -in @('-MD', '-MMD', '-c')) { continue }
    if ($t -in @('-MT', '-MF', '-o')) { $i++; continue }
    $t = $t -replace [regex]::Escape($oldPrefix), $newPrefix
    $t = $t -replace [regex]::Escape($oldPrefix.Replace('/', '\')), $newPrefix.Replace('/', '\')
    $argsList.Add($t)
}
$argsList.Add('-c'); $argsList.Add('-o')
$argsList.Add('C:/Users/Lenovo/Desktop/xrobot/xrobot-local/DartLauncher/build-host/launcher_hw_4j.obj')
if ($argsList | Where-Object { $_ -match 'Modules[/\\]local[/\\]DartLauncher' }) { throw '残留同步副本路径' }
& $compiler @argsList
if ($LASTEXITCODE -ne 0) { throw "ARM 单文件预检失败: $LASTEXITCODE" }
```

3. **（仅获提交/同步授权后）备份 4i ELF、同步模块、副本复测与 debug 构建**：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw'
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
Copy-Item -LiteralPath build/debug/DevC.elf -Destination build/DevC-e17b490-before-4j.elf
xrobot module remove local/DartLauncher
xrobot module add 'local/DartLauncher@<源码完整提交哈希>'
xrobot setup --update local/DartLauncher
git -C Modules/local/DartLauncher rev-parse HEAD
git -C Modules/local/DartLauncher status --short
xrobot format --check
cmake --build --preset debug
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```

### 6.3 首次上车验收步骤（用户自行烧录后，一次一阶段）
- **烧录前提**：未上膛（`latch=2`）、非滑块托持态、无镖、手离机构、pitch 置于上端 1–2 cm 内、C610 设 ID 2 接 CAN2（PB5/PB6）。
1. **无力上线检查（右拨杆下）**：核对 `fault=无故障`、`online=1/1/1/1/0`、`home=0/0/0/0/0`、`Dart yaw pos=0.00rev span=0.00rev cmd=0 act=... rpm=0`、`Dart rs485 L/R` 正常。
2. **未回零双向低速点动（右中、左上）**：左右短促轻推左摇杆（受限于 2 圈/s、0.85 A），核对左右运动方向是否符合操作手习惯（反了则停机仅改 `artisans_dart.yaml` `yaw_motor.reverse: true`），同时确认 0.85 A 能平滑推动丝杠。
3. **手动瞄准双端寻端回中（右中、左上，拨轮向上 2 s）**：观察 yaw 自动找第一端 → 退离 0.5 圈 → 找第二端 → 退离 0.5 圈 → 回中并稳定（同时 pitch 回零退至 95 mm）；记录 `Dart yaw` 打印的实测 `span` 圈数、目视确认确实停在物理中点、`home=0/0/1/1/0`；随后手动左右点动确认软限位前平滑减速、松手位置保持不飘。
4. **4f 自动回零联动与取消测试（右上、左中→下）**：重启未回零下触发 4f 自动启动，核对必须等 pitch 与 yaw **双端回中均完成**后滑块才开始回零，全回零（`home=1/1/1/1/0`）后提示一次 `换弹机构未安装` 停住；另测回零中途切无力立即停机且重新上力不自启。
- **停止条件**：任何异响、联轴器打滑、未退出接触即强顶、0.85 A 推不动反复报阻、或非预期动作，立即切无力（非滑块托持态）停止并视需要回退 `DevC-e17b490-before-4j.elf`。

---

## 7. 需人工确认项（最小化，不发硬件问卷）

1. **源码提交授权**：本轮代码与主机测试完成后，是否授权在 `xrobot-local/DartLauncher` 与 `dart-fw` 当前分支各做一次本地提交以执行 `xrobot setup --update` 与完整固件构建（仍不 push、不烧录）？
2. **首次实测 `reverse` 方向约定**：默认保持 `artisans_dart.yaml` `yaw_motor.reverse: false` 上车验证，若左摇杆向右推时实际向左走，再单独翻转 `reverse: true`（不改代码正负号）。
