# 任务 4j 旧工程独立核对与修订建议（B 版）

- **任务号**：4j（归属主对话 Orca item 5；原 4j 定稿已暂停，尚未批准新机械模型）
- **日期**：2026-11-24
- **状态**：旧工程只读核对完成（独立 B 版，供主对话对比合并）
- **角色**：planner (B版)
- **核对对象**：`C:\Users\Lenovo\Desktop\xrobot\四发完整流程 - 副本`
- **当前新工程基线**：`xrobot-local/DartLauncher` @ `e17b490e8ffbefedc08c96d8d74396ba257487de`（4i）；`dart-fw` @ `fa0838b6b405db715d3be8eb552784da49f3cc35`
- **边界声明**：严格只读，未读取主对话旧工程结论或 A 版草稿；未修改原冻结定稿、源码、测试、配置或 `handoff.md`，未构建、未提交、未烧录。

---

## 1. Keil 工程实际编译引用核实

检查 `四发完整流程 - 副本/MDK-ARM/PROJECT.uvprojx` 与链接表 `MDK-ARM/OBJ/PROJECT.map`、`Src/freertos.c`：

- **实际参与编译与运行的业务文件**：
  - `application/CAN_receive.c`（`PROJECT.uvprojx` L819）+ `application/CAN_receive.h`
  - `MDK-ARM/shoot_task.c`（`PROJECT.uvprojx` L959，**非 `Src/shoot_task.c`，非 `application/shoot_task.c`**）+ `Inc/shoot_task.h`
  - `MDK-ARM/usbv_task.c`（`PROJECT.uvprojx` L964）
  - `MDK-ARM/visionv_task.c`（`PROJECT.uvprojx` L974）
  - `Src/freertos.c`（`PROJECT.uvprojx` L410）：`MX_FREERTOS_Init()` 实际启动 4 个业务线程：`referee_usart_task`、`usbv_task`、`shoot_task`、`visionv_task`（L162–173）；`chassis_task`、`INS_task`、`led_RGB_flow_task` 均已注释（L150–158）。
- **未编入 Keil 工程的干扰文件（不可作为旧工程实运行依据）**：`application/ats_gimbal_task.c`、`application/ATS_chassis_task.c`、`Src/shoot_task.c`、`application/shoot_task.c`、`MDK-ARM/shoot.c` 均不在 `PROJECT.uvprojx` 文件列表中。

---

## 2. 旧工程两个 M2006 通道的严格区分

依据 `application/CAN_receive.c`（L13–35、L120、L142–163、L255–272、L388–390）与 `application/CAN_receive.h`（L27、L30–36），旧工程在 **CAN1 单帧 `0x200`** 上挂了两台 3508（`0x201`、`0x202`）和两台 2006（`0x203`、`0x204`）：

| 通道 | 旧 CAN 总线与 ID（**严禁照搬至新工程**） | 反馈结构与下标 | 发送函数与槽位 | 实际职责 |
|---|---|---|---|---|
| **通道 0：yaw 2006** | CAN1 `CAN_2006_M3_ID = 0x203`（`CAN_receive.h` L35；`CAN_receive.c` L158–160） | `Motortrigger[0]` → `Dart_Data.G2006_motor[0]`（`shoot_task.c` L223） | `CAN_Cmd2006()` → `dart_motor_current[2]`（`CAN_receive.c` L255–259） | **真实 yaw 通道**：视觉控制（`visionv_task.c` L88–174、L200–208；`shoot_task.c` L1419–1425）与手动控制 `M2006_Control()`（`shoot_task.c` L253–279、L1426–1431） |
| **通道 1：aux 2006** | CAN1 `CAN_2006_M4_ID = 0x204`（`CAN_receive.h` L36；`CAN_receive.c` L161–163） | `Motortrigger[1]` → `Dart_Data.G2006_motor[1]`（`shoot_task.c` L223） | `CAN_Cmd2006_Aux()` → `dart_motor_current[3]`（`CAN_receive.c` L261–265） | **独立辅助 2006 通道（不是 yaw）**：`shoot_task()` 每 1 ms 调用 `M2006_Aux_Control()`（`shoot_task.c` L299–354、L2135）；代码未注明其具体机械机构名称，**不可当作 yaw 事实** |

> **CAN 编号隔离提醒**：新工程 CAN1 `0x203` 已被 `pitch_motor`（M3508）占用；绝不能照搬旧工程 CAN1 `0x203`/`0x204`，新工程应继续沿用 `dart-fw/User/RobotConfig/artisans_dart.yaml` 的 **CAN2 + ID 2（`0x202`）**。

---

## 3. 各通道控制机制逐项证据

### 3.1 多圈编码器计数（两通道共用）
- `application/CAN_receive.c` L37–49（`get_motor_measure` 宏）：
  - 过零判据：`ecd - last_ecd > 4096` 则 `round_cnt--`；`ecd - last_ecd < -4096` 则 `round_cnt++`。
  - 累计计数：`total_angle = (round_cnt + 1) * 8192 + ecd`（单位：**转子原始 counts**，8192 counts = 转子 1 圈）。
  - 起点特征：上电无绝对零点标定；因 `last_ecd` 初值为 0 且含 `+1` 偏置，`total_angle` 仅代表相对上电的连续多圈增量，不对应机械绝对零位。

### 3.2 通道 0（yaw 2006）— 视觉自动瞄准路径
- `MDK-ARM/usbv_task.c` L115–139：
  - 将视觉包 `vision_packet.yaw`（rad）取绝对值并乘 `57.2957795f` 转为度 `target_angle`（L119–120），按 `direction`（0→0，1→正，2→负，L122–135）赋号后调用 `vision_update_target(target_angle, 0.0f, target_found)`（L139）。
- `MDK-ARM/visionv_task.c` L88–174、L186–212：
  - **单位与机械比例**：`current_total_angle = (float)total_angle * 360.0f / 8192.0f`（单位：**转子累计角度，度**，L96–97、L141–143）。
  - **关键经验换算（L95、L110）**：注释写明“**内围转子转 1 圈（360 度）= target_angle 0.01**”，执行 `target_total_angle = current_total_angle + angle * 36000.0f`。即视觉偏角每 **1°**，M2006 转子转 **36000°（100 圈）**，36:1 减速后输出轴/联轴器转约 **2.78 圈（1000°）**。
  - **目标锁定与松手/超时**：`angle` 从非 0 变为 0 时，锁定 `target_total_angle = current_total_angle` 并清 PID 积分（L99–107）；若超过 `TARGET_UPDATE_TIMEOUT = 500 ms` 未更新或 `target_found == 0`，清 PID 并下发 `CAN_Cmd2006(0)`（L27、L125–133、L193–198），**无持久保持**。
  - **PID 与减速**：外环位置 PID（`kp=1.5, ki=0, kd=1.0`，max 3000 rpm）叠加 `30 * sqrt(abs_error)` 减速包络（误差 `< 1°` 转子角死区停机，L15–19、L158–168）；内环速度 PID（`kp=5.0, ki=0.1, kd=0`，max ±10000，L22–24、L171、L205–206）。
  - **初始化/限位/回零**：**无软限位、无硬限位、无堵转回零**，完全基于当前 `total_angle` 做相对增量。

### 3.3 通道 0（yaw 2006）— 手动遥控路径 `M2006_Control()`
- `MDK-ARM/shoot_task.c` L253–279（仅在 `update_yaw_aim_control()` L1406–1432 无视觉接管且 `s[1]==DOWN && s[0]==MID` 时调用）：
  - **非多圈**：读 `rc.ch[0]` 映射到 `target_pos ∈ [-45°, +45°]`（L255–257）；读单圈 `ecd` 换算 `current_angle ∈ (-180°, +180°]`（L259–263），经 `PID_Calc_Angle` + 速度环输出限幅 `±2000`（L270–278）。
  - **松手行为**：`|ch0| < 10` 直接 `CAN_Cmd2006(0); return;`（L265–268），**松手断力不保持**。
  - **限位/回零**：**无回零、无多圈限位**，且在控制空间（单圈转子角 ±45°）上与视觉路径（多圈转子累计角）不一致。

### 3.4 通道 1（aux 2006）— `M2006_Aux_Control()`（仅供架构参考，非 yaw）
- `MDK-ARM/shoot_task.c` L281–354、L2135；`Inc/shoot_task.h` L62–74：
  - **初始化起点**：首次运行（`!aux2006_hold_initialized`）将当前多圈计数直接锁为初始目标 `aux2006_position_set = (fp32)aux_measure->total_angle`（单位：**转子 counts**，L322–328）；安全停（`s[1]==UP`）调用 `M2006_Aux_ResetHold()` 清零并给 0 电流（L308–313），恢复时再以当时位置重采样为起点。
  - **目标更新**：使能档（`s[1]==MID && s[0]==DOWN`，L292–297）下 `|rc.ch[2]| > 10` 时每 1 ms 相对累加 `aux2006_position_set += rc_ch * 0.5f` counts（L330–337、`shoot_task.h` L62–64）。
  - **松手保持**：只要未触发安全停，松开摇杆或切出使能档仍持续运行 `total_angle` 位置-速度串级 PID（位置环 `kp=0.5` 限幅 3000 rpm，速度环 `kp=8, ki=0.1` 限幅 ±5000，L339–353）锁住当前位置。
  - **限位/回零/机械换算**：**无软限位、无硬限位、无回零、无导程/角度换算**。

---

## 4. 代码能确认 vs 不能确认的事实

### 4.1 旧代码可确认的事实
1. **旧车 yaw（通道 0）视觉控制运行在转子多圈角度空间，且联轴器/转子旋转远超 12°**：`visionv_task.c` L95/L110 的经验系数为 1° 偏角 ↔ 36000°（100 圈）转子旋转（联轴器约 2.78 圈 = 1000°），与用户反馈一致。
2. **旧工程两路 2006 均无回零、无软限位、无机械几何公式**：既没有单边 `0~12°` 销轴公式，也没有堵转找零点逻辑。
3. **旧工程“上电当前位为基准 + 相对增量 + 松手位置保持”的完整实现仅存在于通道 1（aux 2006）**，而通道 0（yaw 2006）的手动函数反而是单圈 `±45°` 且松手归 0。

### 4.2 代码不能确认的事实（严禁杜撰，须人工/现场确认）
1. **新车 yaw 真实机械行程与中点位置**：总行程是多少转子圈 / 多少 mm / 多少度；当前常驻的“中间位置”到左、右两端的可用余量是否对称。
2. **新车 yaw 真实传动换算**：丝杠导程、销轴臂长或旧视觉 `36000.0f`（转子度/度）系数在新车机械上是否仍成立；旧工程里没有任何丝杠导程/半径常量可验证。
3. **新车是否具备可安全堵转的机械回零挡块或限位开关**：老代码完全没做回零，无法从中推断机械端面能否承受堵转力。
4. **新车 yaw 电机方向（`reverse`）、实际摩擦力与安全电流上限**。

---

## 5. 对原 4j 方案的修订建议（原定稿暂停，待批准后再定型）

1. **撤销原 4j 方案中“默认单边 `0~12°` + 负向堵转回零”的直接上车策略**：
   - 现行 `DartLauncher` 的 `YawConfig()`（`src/hw/launcher_hw.cpp` L42–45）设定 `min=0, max=Slide(12°)=125.7 mm, home=0, home_direction=-1`，且 `YawGeometry::Angle()`（`src/yaw_geometry.cpp` L11–14）把所有负位移夹死为 `0.00 deg`；同时 `StartHome(ManualGroup::Aim)`（L726–729）会把 yaw 与 pitch 一起推去堵转回零。
   - 在 yaw **通常位于中间、实际范围未知、旧代码从未对 yaw 做过单边堵转回零**的前提下，直接按原 4j 定稿执行瞄准组回零存在撞毁未知端面或反向顶死的高风险，且无法支持居中双向调节。
2. **建议将 4j 拆为“安全首测（不假定新机械模型）”与“标定定型”两步**：
   - **第一步（修订后的 4j 首测，仅验证上线、方向、双向低速微调与行程测绘）**：
     - 总线与 ID 保持 **CAN2 + C610 ID 2（`0x202`）**（绝不照搬旧工程 CAN1 `0x203`）。
     - **先不做 yaw 堵转回零**，且在 `StartHome(ManualGroup::Aim)` 与 4f 自动回零中明确隔离 yaw（避免回 pitch 时误带 yaw 堵转撞块）。
     - 借鉴 `aux2006` 的安全思路（但不照搬无保护做法）：上电确认在线后以**当前位置为相对 0 基准**，在**极低速度 + 极低电流上限**下允许**正负双向手动点动**并**松手位置保持**，串口直接打印**未经 `0~12°` 夹紧的原始转子圈数/计数（或原始位移 mm）、`cmd`、`act`、`rpm`**，由操作手目视测出左右极限与中点、确认 `reverse` 方向及“转子圈数 ↔ 发射架转角”实测比例。
   - **第二步（4k 或后续标定任务）**：拿到实测左右极限、中点、导程/几何比例及是否装限位后，再正式批准新机械模型（例如对称 `[-max, +max]` 或新 `YawGeometry`）、软限位、回零策略及 `Aimed()`/`4f` 联动。
3. **须由人工拍板的决策点（未确认前不改代码）**：
   - 确认 4j 首测是否采用“**开机当前位相对零点 + 双向低速点动 + 松手保持 + 暂不回零**”来摸清实际行程，还是实车已确认存在可靠硬挡块必须做单端堵转回零；
   - 确认若 yaw 暂不堵转回零，在 `all_homed` / `Aimed()` / 4f 自动回零中如何处理（例如上电上线即以当前位视为相对就绪 `HomeAt(0)`，或首测期间独立验证不触发自动发射）。
