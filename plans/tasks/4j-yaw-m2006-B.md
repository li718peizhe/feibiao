# 任务 4j：yaw M2006 接入与首次上车实施方案（B 版）

- **任务号**：4j（对应主对话 Orca item 5）
- **日期**：2026-11-24
- **状态**：已规划（独立 B 版草稿，供主对话与 A 版交叉合并）
- **角色**：planner (B版)
- **基线提交**：
  - `xrobot-local/DartLauncher`：分支 `codex/apply-load-position-calibration`，HEAD `e17b490e8ffbefedc08c96d8d74396ba257487de`（4i 临时关闭摆臂/推杆已合入；主机基线 `PASS: 31569 checks` + `PASS: 2193 HX711 checks`）
  - `dart-fw`：分支 `dart-launcher`，HEAD `fa0838b6b405db715d3be8eb552784da49f3cc35`；`Modules/local/DartLauncher` 与 `xrobot.lock` 锁定 `e17b490e8ffbefedc08c96d8d74396ba257487de`；当前 `dart-fw/build/debug/DevC.elf` SHA-256 `C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`
  - `dart-fw/Modules/QDU-Robomaster/Referee`：3 处本地补丁原样保留（`Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp`）

---

## 1. 需求理解与边界

### 1.1 做什么
1. 明确已安装的 yaw M2006（C610 电调）应接入 **CAN1 还是 CAN2**、电调 ID 编号及接线核对要点。
2. 基于当前 4i 基线（摆臂 6020 与推杆舵机临时关闭、滑块与 pitch 开启、4g RS485 开启），制定最小、安全且可直接上车验证的 yaw 接入方案。
3. 解决 yaw 启用后在**松手保持、切模式停机与 `Aimed()` 判定**上的既有缺陷（避免复现 pitch 在 3i 前“手动瞄好后切自动卡 20 s 超时”的问题），并补齐首次上车排障所需的串口原始位移/电流/转速诊断信息。
4. 明确硬件事实与待现场验证参数，给出可执行验证命令、分阶段首次上车步骤、停止条件与回滚方案。

### 1.2 不做什么
- 本任务**不修改**发射/装填/比赛安全状态机逻辑（`Safety`、`LaunchSequence`、`MatchFlow`、`ModeLogic`），不削弱 `SliderMustHold`、`LockOpenAllowed`、`FireBlocker` 任何规则。
- **不重新开启** 4i 已关闭的 `kArmInstalled`、`kCrankInstalled`（`kReloadInstalled` 保持 `false`），不影响 4g RS485 拉力读数。
- **不杜撰**实车丝杠新 CAD 参数或行程；实测前沿用现有命名参数，首次上车仅验证在线、方向、低速点动、回零、软限位与松手保持，不直接做带镖实打、视觉接入或 Flash 预置位。
- 本计划文件仅落盘草稿，不修改源码/配置/测试/`handoff.md`，不执行构建/烧录/提交/推送。

### 1.3 隐含假设
- 现场 yaw 机构仍为 M2006（内置 36:1 行星减速）配 C610 电调驱动滚珠丝杠滑台；具体导程、滑台实际行程、销轴几何与零点方向须在首次上车第 1–3 阶段实测核对。

---

## 2. 现状调研结论（含代码与配置依据）

### 2.1 CAN1 vs CAN2 接线选口与 `RMMotor` 分组发送机制核查

**结论：推荐接入 CAN2（C 板丝印 CAN2，STM32 PB5=RX / PB6=TX），C610 电调设 ID = 2（反馈 ID `0x202` = `514`，控制帧 ID `0x200` 槽位 1）。**

逐项核实依据：
1. **BSP 与现有 YAML 配置已原生配置为 CAN2 + ID 2（`0x202`）**：
   - `dart-fw/Core/Src/main.c` L138–139、L302–361：`MX_CAN1_Init()` 与 `MX_CAN2_Init()` 参数完全一致（`Prescaler=3, BS1=6TQ, BS2=7TQ`，APB1 42 MHz → 1 Mbps，`AutoBusOff/AutoWakeUp/AutoRetransmission = ENABLE`）。
   - `dart-fw/Core/Src/stm32f4xx_hal_msp.c` L208–260：CAN1 在 `PD0/PD1`（AF9），CAN2 在 `PB5(RX)/PB6(TX)`（AF9），两者 TX/RX0/RX1 NVIC 中断均已使能。且 10-28 实车已通过 GM6020 在 CAN2 收到 ~1000 Hz 反馈（`.backup/handoff.md.20261028.bak` L11），证实 CAN2 硬件与 LibXR 驱动工作正常。
   - `dart-fw/User/RobotConfig/artisans_dart.yaml` L38–45、L77、L82 与 `dart-fw/User/xrobot_main.hpp` L68–76、L108–114：`yaw_motor` 已绑定 `can_bus: can2`、`model: RMMotor::Model::MOTOR_M2006`、`reverse: false`、`feedback_id: 514`（`0x202`），`dart_launcher` 已绑定 `yaw_can: can2`、`yaw_id: 514`。**接 CAN2 + ID 2 无需修改 `artisans_dart.yaml` 或重新运行 `xrobot gen`**（仅当实车首测方向相反时再改 `yaw_motor.reverse: true`）。
2. **`RMMotor` 分组拼包发送实证（`dart-fw/Modules/QDU-Robomaster/RMMotor/RMMotor.hpp`）**：
   - 组网结构（L116–137、L194–231、L456–487）：`RMMotor` 按 `LibXR::CAN*` 总线指针建 `BusState`，每条总线分 4 个独立控制组：`index=0`（M3508/M2006 `0x201–0x204` → 控制帧 `0x200`）、`index=1`（`0x205–0x208` → `0x1FF`）、`index=2`（GM6020 `0x205–0x208` → `0x1FE`）、`index=3`（`0x209–0x20B` → `0x2FE`）。同组所有已构造电机都在本拍调用 `Control()`（`pending_mask == group_mask`）时立刻发出 1 帧 8 B 标准 CAN 帧。
   - **CAN2 分组行为**：`yaw_motor`（M2006，`0x202`）独占 CAN2 `index=0`（`0x200`，`group_mask = 0x02`）；`arm_motor`（GM6020，`0x205`）独占 CAN2 `index=2`（`0x1FE`，`group_mask = 0x01`）。两者分属不同控制组，**`kArmInstalled = false`（4i 拆除 6020）或未来装回 6020 均完全互不阻塞、无 ID 冲突**。
   - **CAN1 分组行为**：CAN1 现有 `slider_right`（`0x201`）、`slider_left`（`0x202`）、`pitch_motor`（`0x203`）三台 M3508 共用 `0x200` 控制帧（1 帧发送 + 3 帧反馈 = 4 帧/ms，约占一半带宽，见 `handoff.md` L239）。
   - **为何不选 CAN1（仅作硬件故障兜底备选）**：
     1. CAN1 `0x202` 已被 `slider_left` 占用（`artisans_dart.yaml` L15–21），若改接 CAN1 必须将 C610 改设 ID = 4（`0x204` / `516`）并同步修改 `artisans_dart.yaml` 四处字段及重跑 `xrobot gen`；
     2. CAN1 增至 4 台电机（5 帧/ms，且新并接线束直接触碰已稳定的滑块/pitch 安全主干总线）；
     3. 接 CAN2 后，CAN1 维持 4 帧/ms、CAN2 当前 3 帧/ms（未来装回 6020 也仅 4 帧/ms），两条总线负载均衡且架构与 `handoff.md` L239 及 `plans/rules-2027-dart.md` L158 一致。

### 2.2 现有 yaw 代码全链路核对

| 环节 | 关键文件与行号 | 现状核实 | 存在的问题 / 约束 |
|---|---|---|---|
| **装配开关** | `src/dart_params.hpp` L243、`src/hw/launcher_hw.cpp` L14–15 | `kYawInstalled = false`；`kMotorInstalled[3] = kYawInstalled` | 改为 `true` 后，槽位 3 不再由 `Tick()` L168–171 自动喂反馈，yaw 离线超 100 ms 立即报 `SafetyFault::MotorOffline` |
| **轴配置与传动换算** | `src/dart_params.hpp` L18, L21, L24, L26, L30, L81–82, L112–114, L140, L142, L170–171, L238；`src/hw/launcher_hw.cpp` L42–45 | `counts_per_unit = 36 * 8192 / 0.004 = 73,728,000 counts/m`；`min=0, max=Slide(12°)≈0.12568 m`；`home=0, dir=-1`；`kYawCurrentCap=2829`（±10000 对应 ±10 A，约 2.83 A / 800 N）；回零限流 `0.3 * 2829 = 848`（约 0.85 A）；`kHomeSpeed=0.01 m/s`（转子 90 rpm）；`kHomeRetreat=0.003 m`；`kHomeStallTime=0.2 s`；`home_seek_limit=+inf` | 1. **`hold_on_release` 默认 `false`**；<br>2. **`home_seek_limit` 未设置**（联轴器打滑会空转至 90 s 超时）；<br>3. `kYawRadius=0.6045` 与 `kYawMax=12°` 为旧 CAD 几何，算得滑台范围 125.7 mm，远小于 `kYawTravel=310 mm` |
| **几何映射** | `src/yaw_geometry.hpp` L1–13、`src/yaw_geometry.cpp` L7–14 | `Slide(angle) = kYawRadius * sin(Clamp(angle, 0, 12°))`；`Angle(slide) = asin(Clamp(slide, 0, Slide(12°)) / kYawRadius)` | 仅单边 `0~12°`；**负位移会被 `Angle()` 夹死为 `0.00 deg`**，导致未回零时往负向点动在第 5 行看不到数值变化 |
| **手动点动与松手** | `src/mode_logic.cpp` L50–53；`src/hw/launcher_hw.cpp` L748–753；`src/motor_axis.cpp` L40–87, L384–398 | 手动瞄准组（右中、左上）左摇杆横向 `left_x` → `yaw_rate = Stick(left_x) * Radians(2)`；未回零时按 `yaw_rate * kYawRadius`（受 `kHomeSpeed=0.01 m/s` 夹紧且**只允许往 `home_direction=-1` 即负方向点动**）；回零后松手仅速度环归零不清 `end_` | 松手不更新 `end_`，手动瞄好后 `Settled()` 恒为 `false`（除非正好停在回零退让位 3 mm） |
| **停机与发射到位判定** | `src/hw/launcher_hw.cpp` L859–863, L911–924；`src/motor_axis.cpp` L191–194；`src/launch_sequence.cpp` L55, L307, L679–680；`src/safety.cpp` L78 | `StopMotors()` L922 直接调用 `yaw_.Axis().Stop()`（清 `active_=false`）；`Aimed()` 要求 `yaw_.Axis().Settled(Slide(0.05°), R*0.01)` ≈ `(0.528 mm, 6.05 mm/s)` | **离开手动切自动会触发 `StopMotors()`，使 `active_=false`，单发/自动发射必然卡死在“瞄准：等 yaw/pitch 到位”20 s 超时或 `NotAimed`**（与 3i 前 pitch 的缺陷完全一致） |
| **回零与 4f 自动回零联动** | `src/hw/launcher_hw.cpp` L217–223, L266–287, L714–730；`src/match_flow.cpp` L156–210 | `StartHome(ManualGroup::Aim)` 同时调用 `yaw_.Axis().StartHome()` 与 `pitch_.Axis().StartHome()`；`mechanism.aim_homed` 要求 pitch 与 yaw **均已回零** | 1. 瞄准组手动回零或 4f 自动回零会**同时驱动 pitch 和 yaw**；<br>2. 在 4i（`kReloadInstalled=false`）下，4f 自动回零会在 pitch+yaw 均完成退让后再回滑块，全回零后提示 `换弹机构未安装` 停住，不触发换弹或发射 |
| **监控与诊断输出** | `src/hw/launcher_hw.cpp` L407–424, L519–594；`src/hw/launcher_hw.hpp` L126–142 | 第 5 行打印 `online[3]`、`homed[3]` 和 `yaw=%.2fdeg`；**缺少类似 `Dart pitch cmd=... act=... rpm=...` 的 yaw 诊断行** | 未回零时负向点动 `yaw=` 被夹在 `0.00deg`，缺少 `sl(mm)/cmd/act/rpm` 将难以现场判别方向、堵转电流与滑台行程 |
| **RMMotor M2006 反馈换算细节** | `dart-fw/Modules/QDU-Robomaster/RMMotor/RMMotor.hpp` L437–438, L560–561, L582–583 | `Decode()` 中 `feedback_.torque = raw_current * 0.005f * 10.0f / 16384.0f`（注意分母写死 `MOTOR_CUR_RES=16384`，而 `Control` 发送量程 `GetLSB()=10000`） | 反算 C610 原始反馈电流必须用 `torque / 0.005 / 10.0 * 16384`（不能除以 10000，否则示值偏小 39%） |
| **主机测试依赖** | `test/host_test.cpp` L223, L309, L348–349 | L223 硬断言 `Check(!kYawInstalled, "yaw is not installed for stage 2e")` | 启用 `kYawInstalled = true` 必须同步更新此断言，并补齐 yaw 保持/限位/寻零上限单测 |

---

## 3. 推荐设计方案

### 3.1 硬件接线与电调配置（方案 B 主推）
- **总线选择**：**CAN2**（C 板丝印 CAN2 口，STM32 PB5=RX / PB6=TX）。
- **电调编号**：C610 设为 **ID = 2**（反馈帧 `0x202` = `514`，控制帧 `0x200` 字节 2–3）。
  - *C610 设 ID 操作提示*：上电后按 C610 机身 SET 键进入调参，短按至绿灯闪 2 下（对应 ID 2），长按保存并重启复核绿灯每周期闪 2 下。
- **终端电阻核对**：因当前 4i 已拆 6020，CAN2 仅挂 C610 一台节点，确认 CANH/CANL 无反接且总线 120 Ω 终端匹配（若接分线板则检查终端跳帽）。
- **与 `artisans_dart.yaml` 关系**：默认**无需修改** `artisans_dart.yaml`（第 38–45、77、82 行已是 `can2` + `514` + `reverse: false`）；仅当首次低速点动确认方向反了，再按项目铁律（`handoff.md` L118）只改 `yaw_motor.reverse: true` 并跑 `xrobot gen`，不改 `MotorAxis` 正负号。
- **备选方案（仅 CAN2 硬件故障时启用）**：改接 CAN1 并将 C610 设为 **ID = 4**（反馈 `0x204` = `516`，与右滑块 `0x201`、左滑块 `0x202`、pitch `0x203` 共用 CAN1 `0x200` 帧最后一个槽位），同步修改 `artisans_dart.yaml` L41/45/77/82 与 `README.md` 并执行 `xrobot gen`。**权衡**：增加 CAN1 负载（4→5 帧/ms）且动了已稳定主干线束，除非 CAN2 物理损坏否则不采用。

### 3.2 固件最小改动设计（沿用 3e + 3h + 3i 成熟范式）
1. **开启装配开关并补齐安全寻零上限（`src/dart_params.hpp`）**：
   - L243：`kYawInstalled` 由 `false` 改为 `true`；
   - 在 `kYawTravel` 附近新增 `constexpr double kYawHomeSeekLimit = kYawTravel + 0.010;`（320 mm，参照 `kPitchHomeSeekLimit`，防止联轴器打滑或未碰挡块时持续空转 90 s；若实车有效行程确认更小，现场标定后收紧）。
   - 保持 `kYawCurrentCap = 2829`、`kHomeSpeed = 0.01 m/s`、`kHomeCurrentFraction = 0.3`（回零与未回零点动限流 848 ≈ 0.85 A）、`kHomeStallTime = 0.2 s`、`kYawPos`、`kYawSpeed`、`kYawRadius`、`kYawMax` 不变，等待实车测量后再专项标定。
2. **补齐 yaw 松手保持与停机保持（`src/hw/launcher_hw.cpp`，复用 `MotorAxis::HoldPosition()`）**：
   - `YawConfig()`（L42–45）：增加 `config.hold_on_release = true;` 与 `config.home_seek_limit = kYawHomeSeekLimit;`；
   - `StopMotors()`（L922）：将 `yaw_.Axis().Stop();` 改为与 `pitch_` 相同的：
     ```cpp
     if (!yaw_.Axis().HoldPosition()) {
         yaw_.Axis().Stop();
     }
     ```
   - **效果**：回零后手动瞄准松手自动切位置环保持并令 `Settled()` 成立；离开手动切自动/取消/故障停机时保留位置目标（不使能时照常输出 0 电流），彻底消除切自动后卡死 `NotAimed` / 20 s 超时的问题；未回零时照常 `Stop()`。
3. **增加 yaw 诊断行与原始位移观测（`src/hw/launcher_hw.hpp`、`src/hw/launcher_hw.cpp`）**：
   - `MonitorFeedback` 新增 3 个标量字段：`int yaw_current = 0; double yaw_feedback_torque = 0, yaw_feedback_rpm = 0;`（静态成员，无动态分配）。
   - `Tick()` 快照复制处填充 `yaw_.Current()`、`yaw_.FeedbackTorque()`、`yaw_.FeedbackRpm()`。
   - `OnMonitor()` 在 `if constexpr (kPitchInstalled)` 块后增加 `if constexpr (kYawInstalled)` 诊断行（前置 `LibXR::Thread::Sleep(20)`）：
     ```cpp
     constexpr double kM2006RotorTorqueConstant = 0.005; // N·m/A，转子端，和 RMMotor.hpp KGetTorque() 一致。
     const int yaw_actual = static_cast<int>(std::lround(
         feedback.yaw_feedback_torque / kM2006RotorTorqueConstant / 10.0 * 16384));
     LibXR::STDIO::Printf<"Dart yaw sl=%.2fmm cmd=%d act=%d rpm=%.0f\r\n">(
                 static_cast<float>(feedback.position[3] * 1000),
                 static_cast<int>(feedback.yaw_current), yaw_actual,
                 static_cast<float>(feedback.yaw_feedback_rpm));
     ```
   - **效果**：行长 ≤ 55 字节（远小于 USB CDC 128 B 缓冲）；既显示未经 `0~12°` 夹紧的真实滑台位置 `sl`（mm，未回零往负向点动也能直接观测位移与方向），又与 `Dart pitch` 对齐显示 `cmd/act/rpm`（C610 原始单位 ±10000 对应 ±10 A），可直接判断联轴器是否转动、静摩擦电流及是否接近回零限流 848。
4. **更新主机测试与文档（`test/host_test.cpp`、`README.md`）**：
   - `test/host_test.cpp` L223 更新为 `Check(kYawInstalled, "yaw M2006 is installed on CAN2 ID 2");`，并补充 `kYawHomeSeekLimit > kYawTravel && kYawHomeSeekLimit / kHomeSpeed < kHomeTimeout` 断言；
   - 在 `TestAxis()` 补充一条以 yaw 参数构造的 `MotorAxis` 用例，覆盖 `hold_on_release` 松手保持、`HoldPosition()`、`Settled(Slide(kYawTolerance), kYawRadius * kYawVelocityTolerance)` 及 `home_seek_limit` 超限触发 `HomeOverrun()`；
   - `README.md` 更新 `kYawInstalled` 当前值为 `true`（L42）、说明在线与全部回零当前包含两侧滑块、pitch 与 yaw（不含已临时关闭的摆臂/推杆），并补充 `Dart yaw sl=... cmd=... act=... rpm=...` 状态行说明。

### 3.3 硬件事实 vs 待现场验证参数清单

| 类别 | 项目 | 数值 / 状态 | 依据 |
|---|---|---|---|
| **硬件事实（已锁定）** | 电机与电调 | M2006（36:1 减速）+ C610（±10000 对应 ±10 A） | `dart_params.hpp` L18, L30；`RMMotor.hpp` L50, L54 |
| **硬件事实（已锁定）** | CAN2 引脚与波特率 | PB5(RX)/PB6(TX)，1 Mbps，`AutoBusOff=ENABLE`，中断已开 | `main.c` L339–361；`stm32f4xx_hal_msp.c` L230–260 |
| **硬件事实（已锁定）** | CAN2 配置与 ID | `can2`，反馈 ID `0x202`（514），控制帧 `0x200` 槽位 1 | `artisans_dart.yaml` L38–45, L77, L82 |
| **硬件事实（已锁定）** | 与 4i/4g 并存 | `kArmInstalled=false`, `kCrankInstalled=false`, `kReloadInstalled=false`, `USART1=RS485` | `dart_params.hpp` L247–259；`handoff.md` L15 |
| **待现场验证参数** | 电机正反方向 `reverse` | 当前 `false`；点动反了改 `artisans_dart.yaml` `yaw_motor.reverse: true` | `handoff.md` L118；`README.md` L29 |
| **待现场验证参数** | 丝杠导程与有效行程 | 当前 `kYawLead=4 mm`（SFUR1204）、`kYawTravel=310 mm` | `handoff.md` L239；`dart_params.hpp` L21, L112 |
| **待现场验证参数** | 销轴半径与最大转角 | 当前 `kYawRadius=0.6045 m`（旧 CAD）、`kYawMax=12°`（对应 125.7 mm，单边 `0~12°`） | `dart_params.hpp` L113–114；`yaw_geometry.cpp` L7–14 |
| **待现场验证参数** | 回零方向、堵转限流与时间 | 当前 `home_direction=-1`、`low_limit=848`（0.85 A）、`kHomeSpeed=10 mm/s`、`kHomeStallTime=0.2 s` | `launcher_hw.cpp` L43–44；`dart_params.hpp` L140, L142, L149 |
| **待现场验证参数** | 丝杠刚度/摩擦与 PID | 当前 `kYawPos={0.2,0,0,15000,0}`、`kYawSpeed={3,0.05,0,2829,2829}`、`kManualYawSpeed=2°/s` | `dart_params.hpp` L81–82, L238 |

---

## 4. 建议改动文件清单

| 文件路径 | 改动意图 | 涉及公共头文件 | 涉及安全/控制状态机 | 涉及引脚/外设/参数 |
|---|---|---|---|---|
| `xrobot-local/DartLauncher/src/dart_params.hpp` | `kYawInstalled = true`；新增 `kYawHomeSeekLimit = kYawTravel + 0.010` | 是（模块内部公共参数头） | 否（仅配置常量） | 是（装配开关与寻零上限参数） |
| `xrobot-local/DartLauncher/src/hw/launcher_hw.hpp` | `MonitorFeedback` 新增 `yaw_current`、`yaw_feedback_torque`、`yaw_feedback_rpm` | 是（适配层头） | 否 | 否 |
| `xrobot-local/DartLauncher/src/hw/launcher_hw.cpp` | `YawConfig()` 开 `hold_on_release=true` 与 `home_seek_limit`；`StopMotors()` 回零后 `HoldPosition()`；`Tick()` 复制 yaw 反馈；`OnMonitor()` 增加 `Dart yaw sl=... cmd=... act=... rpm=...` 行 | 否 | 轻度（仅改停机时 yaw 的保持调用，与 pitch 3i 一致，不碰 Safety/Sequence/MatchFlow） | 是（监控诊断输出） |
| `xrobot-local/DartLauncher/test/host_test.cpp` | 更新 L223 `kYawInstalled` 断言；新增 `kYawHomeSeekLimit` 与 yaw `hold_on_release`/`HoldPosition`/`HomeOverrun` 用例 | 否 | 否 | 否 |
| `xrobot-local/DartLauncher/README.md` | 更新 `kYawInstalled=true`、在线/回零设备列表及新增 `Dart yaw` 状态行说明 | 否 | 否 | 否 |
| `dart-fw/Modules/modules.yaml` & `dart-fw/xrobot.lock` | 经 `xrobot setup --update local/DartLauncher` 锁定新提交（不手改） | 否 | 否 | 否 |
| `dart-fw/User/RobotConfig/artisans_dart.yaml`（**仅方向实测相反或改 CAN1 时才改**） | 默认不改；若首次点动方向相反，仅改 `yaw_motor.reverse: true` 并跑 `xrobot gen` | 否 | 否 | 是（仅条件触发） |

> **严禁修改**：`src/safety.*`、`src/match_flow.*`、`src/mode_logic.*`、`src/launch_sequence.*`、`src/tension_modbus.*`、`src/hw/tension_port.*`、`dart-fw/Core/Src/main.c`、`dart-fw/Modules/QDU-Robomaster/Referee/*`。

---

## 5. 风险与回退点

### 5.1 关键风险排查
1. **硬件安全 / 回零方向与联轴器风险**：
   - 若实际机构回零挡块在正方向（或 `reverse` 接反），盲目按拨轮回零会朝错误方向顶撞另一端！**缓解**：首次上车严禁直接回零，必须先“无力手盘确认位移方向 → 未回零低速点动核对正负号”。
   - `StartHome(ManualGroup::Aim)` 会同时启动 pitch 与 yaw 回零；若 pitch 不在离上端 1–2 cm 内，会连带触发 pitch 半路误判堵转或顶滑联轴器。**缓解**：按瞄准组回零前必须先确认 pitch 已靠近上端。
   - 未设 `home_seek_limit` 时联轴器打滑会空转 90 s；设 `kYawHomeSeekLimit = 320 mm` 后超程即锁存 `HomeOverrun` 停机。
2. **总线并发与离线连带停机风险**：
   - 开启 `kYawInstalled = true` 后，若 C610 未上电、ID 不是 2、或 CAN2 线断，超 100 ms 即触发 `fault=电机离线` 锁停滑块和 pitch。**缓解**：提前备份 4i ELF，上电先在无力模式下检查 `online=1/1/1/1/0`。
   - CAN2 单独挂载 1 台 C610 时注意 120 Ω 终端匹配；若无反馈可用既有只读 OpenOCD 命令查 `CAN2 ESR`（`0x40006818`，LEC=3 代表无应答，见 `handoff.md` L243）。
3. **时序、内存与串口缓冲风险**：
   - `MonitorFeedback` 仅增 20 B 静态内存，零堆分配，不增加 1 kHz 控制线程耗时；`OnMonitor()` 新增 1 行 `Dart yaw`（约 45 B < 128 B 缓冲）加 20 ms 间隔，总输出 ~260 ms << 1000 ms。
   - ARM 构建开启 `-Wall -Werror`；`kM2006RotorTorqueConstant` 须定义在 `if constexpr (kYawInstalled)` 内部（或确保不被未启用态触发 `-Wunused-const-variable`）。
4. **向后兼容与安全规则保持**：
   - `SliderMustHold`、`LockOpenAllowed`、`OutOfMatch`、`FireBlocker` 逻辑零改动；4i 的 `kReloadInstalled = false` 保持不变，4f 自动回零结束后仍止步于 `换弹机构未安装`，不会意外触发换弹或发射。

### 5.2 回退点
- **现场即时回滚（回到 4i 无 yaw 固件）**：确认未上膛、非滑块托持态、手离机构后，刷回烧录前备份的 `dart-fw/build/DevC-e17b490-before-4j.elf`（SHA-256 `C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`）。
- **代码层回滚**：将 `kYawInstalled` 置回 `false` 并恢复 `host_test.cpp` L223（保留 `hold_on_release` 与诊断行无副作用），严禁 `git reset --hard`。

---

## 6. 验收标准与可执行验证方案

### 6.1 自动化验证命令（PowerShell，阶段化执行）

1. **源码仓主机回归与纯逻辑依赖扫描**：
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
验收判据：`host_test_4j.exe` PASS（在基线 `31569` 之上新增 yaw 用例全通过）、`hx711_port_test_4j.exe` PASS `2193` checks、纯逻辑依赖扫描输出为空。

2. **ARM 单文件预检（强制把 `compile_commands.json` 中副本路径替换为源码仓路径，避免假通过）**：
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
验收判据：退出码 0，零警告。

3. **备份 4i ELF、同步模块、副本复测与完整固件构建**（源码仓按 LF 提交后执行）：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw'
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
Copy-Item -LiteralPath build/debug/DevC.elf -Destination build/DevC-e17b490-before-4j.elf
xrobot module remove local/DartLauncher
xrobot module add 'local/DartLauncher@<源码完整提交哈希>'
xrobot setup --update local/DartLauncher
git -C Modules/local/DartLauncher rev-parse HEAD
git -C Modules/local/DartLauncher status --short
# 若改动了 artisans_dart.yaml，再运行：xrobot gen -c User/RobotConfig/artisans_dart.yaml
xrobot format --check
cmake --build --preset debug
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```
并在 `dart-fw/Modules/local/DartLauncher` 中复跑 `host_test.cpp`（输出到源码仓 `build-host/host_test_4j_sync.exe`），核对 `Referee` 3 处本地补丁完好。

### 6.2 首次上车手工验证步骤（一次一阶段，由用户自行烧录后执行）

> **烧录前提**：确认未上膛（`latch=2`）、非滑块托持态、发射座无镖、手离开机构；确认 yaw 滑台停在机械行程中间区域（两侧留有余量）、pitch 停在离上端挡块 1–2 cm 内；C610 设为 ID 2 并接 CAN2。

- **阶段 1：无力上线与方向判别（右拨杆下）**
  - **动作**：上电保持无力，观察 USB 串口日志（约 10 s）；若丝杠可安全手盘，轻转观察位移符号变化。
  - **验收**：`fault=无故障`、`online=1/1/1/1/0`、`home=0/0/0/0/0`；出现 `Dart yaw sl=...mm cmd=0 act=... rpm=0`；`Dart rs485 L/R` 照常更新、`late` 正常。
  - **排障**：若 `online=1/1/1/0/0` 且报 `fault=电机离线`，依次查 C610 供电、绿灯闪 2 下（ID=2）、CAN2 H/L 极性与 120 Ω 终端，必要时用 OpenOCD 读 `0x40006818`（CAN2 ESR）。
- **阶段 2：未回零低速点动核对方向与最小驱动电流（右拨杆中、左拨杆上）**
  - **动作**：轻推左摇杆向左（负向，`left_x < -0.05`）短促点动 1–2 s（未回零只允许朝回零方向 `-1` 移动，限速 10 mm/s、限流 848 ≈ 0.85 A）；再试向右推确认被拦截不动。
  - **验收**：向左推时 `sl` 数值向负方向变化、`rpm` 稳定、机构朝**预期 0 端机械挡块**运动；向右推时 `cmd=0`、`rpm=0`。
  - **分叉处理**：若运动方向与预期 0 端相反，立即切无力停止，后续单独把 `artisans_dart.yaml` 中 `yaw_motor.reverse` 改为 `true` 并重编烧录；若 `cmd` 顶满 `±848` 但 `rpm=0` 丝杠不动，记录 `act` 与现场阻力，待评估提高回零限流。
- **阶段 3：瞄准组回零与软限位/松手保持验收（右拨杆中、左拨杆上）**
  - **动作**：再次确认 pitch 在上端 1–2 cm 内、yaw 距 0 端畅通无干涉；拨轮向上保持 2 s 触发 `ManualGroup::Aim` 回零。
  - **验收**：yaw 与 pitch 均顶到挡块并顺利退让（yaw 停在 `sl≈3.00 mm`，`home=0/0/1/1/0`）；随后左右轻推左摇杆在 `0~12°`（`0~125.7 mm`）内低速往返，观察：① 松手立即停稳且位置不飘；② 接近 0 mm 与 125.7 mm 时平滑减速不撞块；③ 同步用尺/量角器实测滑台行程与发射架实际转角，记录实车 `kYawLead`、`kYawRadius`、`kYawMax` 真实值。
- **阶段 4：与 4f 自动回零相容性验收（自动模式：右拨杆上）**
  - **动作**：重启或清回零后，先左拨杆中→右拨杆上静置 3 s（确认不自启），再左拨杆**中→下**触发 4f 自动回零；严禁拨中→上（单发）。
  - **验收**：先完成瞄准组（pitch + yaw）回零与退让，再启动滑块回零与退让，全回零（`home=1/1/1/1/0`）后仅打一次 `换弹机构未安装` 并停在 `step=空闲`。

### 6.3 停止条件（任一触发即停）
- 首次上电出现非预期运动、异响、联轴器打滑、限位撞击、电机异常发热或 `late` 持续升高；
- 未回零点动方向与预期回零端相反（立即停机改 `reverse`，禁止强行回零）；
- 瞄准组回零报 `HomeOverrun`、超 35 s 仍未碰端、或丝杠卡滞；
- `SliderMustHold` 滑块托持态出现异常（**严禁断电或烧录**，先按脱困流程回顶）。

---

## 7. 需人工确认项

以下 5 项涉及机械事实或行为变更，在合并 A/B 方案并派发 worker 前请人工确认：

1. **CAN2 接线与 C610 ID = 2 确认**：确认本轮直接沿用既有配置（**CAN2 PB5/PB6 + C610 ID 2 / `0x202`**），而非改接 CAN1 ID 4（`0x204`）。
2. **本轮是否一并补齐 yaw `hold_on_release = true` 与 `StopMotors()` 保持**：
   - *改变行为*：将 `YawConfig()` 与 `StopMotors()` 从“松手/停机即 `Stop()`”改为与 pitch（3i）一致的位置保持。
   - *理由*：否则启用 yaw 后任何切自动发射流程都会卡死 `NotAimed` / 20 s 超时，且 `plans/rules-2027-dart.md` L158 已规划如此；若仅想先测上线与点动，也可分两步做，但推荐本轮一并完成。
3. **本轮是否新增 `kYawHomeSeekLimit`（默认 `kYawTravel + 0.010 = 0.320 m`）与 `Dart yaw` 诊断行**：
   - *理由*：防止联轴器打滑空转 90 s，且解决未回零负位移被 `YawGeometry::Angle()` 夹死在 `0.00 deg` 看不见读数的问题。
4. **实车 yaw 机构几何与零点方向**：
   - 请确认当前已装车的 yaw 丝杠是**单边 `0~12°`（一侧挡块回零）**还是**中线对称 `±θ` 双向摆动**？导程是否确为 4 mm（SFUR1204）？若为双向对称或行程小于 125 mm，首次上车阶段 2/3 需避免跑满 12° 并在标定任务中重写 `YawGeometry` 与 `kYawMax`。
5. **瞄准组回零时 pitch 与 yaw 同启的安全前提**：
   - 现有 `StartHome(ManualGroup::Aim)`（`launcher_hw.cpp` L726–729）会同时回零 pitch 和 yaw；请确认接受“首次测 yaw 回零前将 pitch 置于上端 1–2 cm 内一同回零”，还是需要单独隔离测试策略。
