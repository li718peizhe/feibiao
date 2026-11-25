# 任务：飞镖发射架新固件（LibXR / XRobot 1.0）

> 2026-10-14 起，给 Codex 的任务每个单独写一个短文件，放在 `plans/tasks/` 下，Codex 只读那个文件。这份文档是背景和历史记录，Codex 不用读；阶段 3~5（用户上车调试）的步骤仍在这里。

## 目标
给新机械写一套新固件，框架用 LibXR + XRobot 1.0，放在新工程里。旧 Keil 工程 `四发完整流程 - 副本` 是旧机械的，只作参考，一律不改。

## 怎么执行
- 一次只做用户指定的那个阶段。做完写汇报，然后停下，不要接着做下一阶段。
- 标【用户】的阶段要联网或要接板子，Codex 不做。
- 用户运行的命令：
  ```powershell
  codex exec -s workspace-write --skip-git-repo-check -o codex-report.md "按 plans/dart-fw-libxr.md 执行阶段 N，完成后列出改了哪些文件和验证结果"
  ```

## 背景（已确认）
### 硬件
- 主控：RoboMaster 开发板 C 型（STM32F407IGH6）。
- 电机（机械 2026-10-11 回填）：
  - 滑块：M3508 ×2，原装 19:1 减速箱，S3M 40 齿带轮，左右镜像安装。节圆半径 = 40×0.003/(2π) = 0.0190986 m。
  - pitch：M3508，原装 19:1，丝杆 BPGF-C7-12-2，导程 2 mm，直连。
  - yaw：M2006（36:1），丝杆 SFUR1204，导程 4 mm，梅花联轴器直连。联轴器紧定螺钉有滑丝风险。
  - 换镖：GM6020。驱动哪个机构还没确认，先按装弹摆臂写。
  - 舵机：2026-10-13 换成众灵 PM 系列 PWM 舵机。锁止和推杆先都按这款写，具体型号和用在哪几个机构还没确认。
    - 资料在 `D:\BaiduNetdiskDownload\001-PWM舵机（PM系列）\`。
    - 周期 20 ms（支持 50~330 Hz）。500~2500 µs 对应 0~270°，1500 µs 是中位，1500→2500 µs 是逆时针。PM10 系列是 180°。
    - 供电 5~8.4 V。
  - 电磁铁的驱动方式还没确认（机械第 19 条），先按开关量写。
  - 锁止机构（机械 2026-10-12 说明）：舵机相当于锁止机构的开关，只负责拨动它，发射机构是靠锁止机构本身卡住的。所以危险只在舵机什么时候拨到“开”，规则见“代码规范”。
    - 已确认（2026-10-13）：锁止合上时夹爪不会被顶开，发射机构从上面压不过去。所以夹爪合着的时候，不能让滑块把停在顶部的发射机构往下压，否则会卡死在夹爪上。
- 额定力：丝杆螺母 1000 N，同步带每根 220 N。乘 0.8 余量，换算成电流指令上限（算法同 `simulation\dart_sim\control.py` 的 `current_cap`）：
  - 滑块 9178（仿真里带轮半径取 0.0191，算出来是 9179）
  - yaw 2829
  - pitch 695
- 实车行程：
  - 滑块 710 mm
  - 发射机构从锁住点到上止点 600 mm
  - pitch 滑块 180 mm
  - yaw 滑台 310 mm，角度 0~12°，只往一边转；正对中线时滑台在 0 mm
- 还没测：yaw 几何（第 9 条）、限位开关（第 11 条）、皮筋拉力（第 14 条）、质量、舵机和电磁铁（第 17~19 条）。

### 动作和参数的依据
- `simulation\dart_sim\sequence.py`：发射流程，固件照它移植。
- `simulation\dart_sim\control.py`、`motors.py`：串级 PID、增益、电流上限、舵机限速。
- `simulation\dart_sim\params.py`。
- `simulation\dart_sim\model\meta.json`：取镖角、装填位等，来自旧 CAD，只能当初值。
- 旧固件只能参考通用约定：裁判系统接 UART6，DBUS 接 USART3。它的流程和常数都不能用。

### 底座工程和复用的模块
- 底座用 QDU-Robomaster/bsp-dev-c（Apache-2.0），克隆到 `C:\Users\Lenovo\Desktop\xrobot\dart-fw`。
  - 内容：C 板 + FreeRTOS + LibXR 子模块（`Middlewares\Third_Party\LibXR`）+ CMake preset，工程文件 `DevC.ioc`。
  - `User\app_main.cpp` 已经注册的硬件：can1、can2、usart1、usart3、usart6、pwm_tim1_ch1~4、pwm_tim8_ch1~3、pwm_tim4_ch3（蜂鸣器）、LED_R/G/B、USB CDC 终端（terminal、ramfs）、Flash 数据库（database）。
- 遥控器不用 QDU 的 DR16 模块（阶段 2 第一次执行时查到）：
  - 它只把拨杆的变化当事件发出去，摇杆和拨轮混进 CMD 的底盘、云台指令里，原始值存在私有成员 `data_review_`，而且只有调试编译才有。
  - 所以我们自己在 usart3 上收 DBUS 帧，用纯逻辑函数解析。帧格式和串口设置照抄 `DR16.hpp` 的 `ParseRC`、`ThreadDr16`。
- 复用 QDU 索引里的模块：QDU-Robomaster/RMMotor（支持 M2006、M3508、GM6020）、Referee。RMMotor 有几点要注意：
  - 只给单圈转子角，多圈要自己累计。
  - 同一个控制帧里的电机，每个周期都要发指令；少发一个，整帧都不发。
  - 要周期调用 `Update()`。它在连续 255 次没收到反馈之前都返回 `OK`，分不出这一拍有没有新反馈；反馈里也没有时间戳。所以不能拿它做 100 ms 离线检测（阶段 2 第二次执行时查到），离线计时的做法见文件划分表里的 `motor_port`。
  - 电流指令走 `Control()`：`mode` 设成 `MODE_CURRENT`，归一化电流放在 `velocity` 字段，等于原始值 ÷ 满量程（M3508、GM6020 是 16384，M2006 是 10000）。
  - 设了 `reverse` 以后，RMMotor 会把反馈（角度、转速）和电流指令一起取反。
  - 温度超过 75 °C 时输出强制为 0。
  - GM6020 用 0x1FE 电流控制帧，电机固件要支持电流模式。
- 状态灯和蜂鸣器用 XRobot 官方的 BlinkLED、BuzzerAlarm。

### 本机工具（都已装好）
- cmake 4.3.2、ninja 1.13.2、arm-none-eabi-gcc 13.3.1，都在 PATH 里。
- STM32CubeMX：`D:\keil5\cube\STM32CubeMX.exe`，固件包 STM32Cube_FW_F4_V1.28.3。这一版不改 .ioc。
- 主机 g++ 13.1：`C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin`。用之前把这个目录加到 PATH 最前面。
- OpenOCD：`C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe`。

### Codex 沙箱的限制
- 不能联网。
- 这台电脑上 git 写 ref 可能被沙箱吞掉。每次 `git commit` 之后，都用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。

## 新工程布局
```text
C:\Users\Lenovo\Desktop\xrobot\
  dart-fw\                          新固件 BSP（bsp-dev-c 的克隆，分支 dart-launcher）
    User\RobotConfig\artisans_dart.yaml   本车的模块实例和参数
  xrobot-local\
    index.yaml                      本地模块索引（已有 ATSRuntime，阶段 2 加 DartLauncher）
    DartLauncher\                   新模块，本地 Git 仓库，分支 main
      DartLauncher.hpp              模块入口
      CMakeLists.txt
      README.md                     接线、模式、标定说明
      src\                          按功能分的 .hpp/.cpp（见下表）
      test\host_test.cpp            纯逻辑层的主机测试
```

## 代码规范（每个阶段都要遵守）
- C++17（以 BSP 的 CMake 设置为准）。文件名用小写加下划线，类名用 PascalCase。
- 一个功能一对 `.hpp/.cpp`，头文件用 `#pragma once`。
- 不新增宏：
  - 常量用 `constexpr`，选项用 `enum class`，小函数写成 `constexpr` 或 `inline` 函数。
  - 只用框架自带的宏（`XR_LOG_*`、`XR_REGISTER`、模块 manifest 注释块）。
- 注释用中文：
  - 每个文件开头写这个文件管什么。
  - 每个类和公有函数写清做什么、参数和返回值的单位。
  - 正方向和单位换算要写清楚。
- 单位统一用 m、rad、s、N、A。电流指令用电调原始值：C620 ±16384，C610 ±10000，GM6020 ±16384。
- 纯逻辑层（下表里标“纯逻辑”的文件）不 include LibXR、HAL、FreeRTOS，只用标准库（`<cstdint>`、`<cmath>`、`<array>`），这样能在电脑上测试。
- 控制循环里不 `new`、不 `malloc`。用固定容量数组，不用 `std::vector`、`std::function`、异常、RTTI。
- 不留 TODO。没确认的参数照样写成 `constexpr`，注释标“待标定”或“待确认（机械第 N 条）”。
- 上电默认无力。离线、故障、无力模式、取消流程时，电机输出 0，也不解锁。
- 只有一个例外：滑块已经回零、发射机构没有确认锁住、而且滑块低于 park 位 10 mm 以上。
  - 这时滑块正托着被皮筋拉紧的发射机构。电流一清零，发射机构就会连同滑块一起弹上去，相当于走火。
  - 所以滑块要留在当前位置做位置保持，其他电机输出 0。
  - 这个判断写成纯函数 `SliderMustHold()`，流程和控制循环都调用它。
- 锁止舵机只有下面两种情况能拨到“开”：
  - 正式发射：只在发射流程的“发射：解锁”这一步，而且 `FireBlocker()` 全部通过。其中“已锁止”指锁止确认已经通过，开机时的未知状态不算。
  - 其他开锁（上膛和装填里的“张开夹爪”、手动开锁）：`LockOpenAllowed()` 返回 true，也就是发射机构已放开（发射后停在顶部），或者滑块正压在锁止位以下 1 mm 以上。
    - 锁止位是发射机构正好被夹爪卡住的位置，对应滑块坐标 `kLatchSlider = −kPushGap`。
    - 判断条件写成 `slider ≤ kLatchSlider − 0.001`。上膛压到的 `kCockPosition` 比锁止位还低 3 mm，所以滑块停在它附近 ±1 mm 内都满足。
- 锁止状态什么时候记录（流程和手动操作都一样）：
  - 发出开锁指令的那一刻，就记“已放开”。不能等舵机转到位才记。
  - 发出合上指令的那一刻，记“未知”。
  - 只有锁止确认通过，才记“已锁住”。确认失败时保持“未知”。
  - 唯一的例外是“探测”（阶段 1d）：滑块停在锁止位上方 30 mm，按保持电流判断。电流一直高记“已放开”，一直低记“已锁住”，有高有低就报故障，状态保持“未知”。
- 比赛规则不允许比赛开始前上膛（2026-10-14 用户说明）。所以正式比赛时的起始状态是：发射机构停在顶部，皮筋松着，没上膛。上电以后由操作手触发上膛（现在是拨轮往下，以后可以做成自动）。探测会把这个状态判成“已放开”，然后走完整的上膛流程。
  - 这也说明开机时发射机构更可能在顶部，而不是锁着。但固件不能假定，照旧按“未知”处理，先探测。
- 开机时发射机构的状态未知，按“可能已经上膛”处理。状态是“未知”时，任何流程都先“探测”，弄清楚是已锁住还是已放开，再往下走（见阶段 1d）。

## 模块 DartLauncher 的文件划分
| 文件 | 层 | 职责 |
|---|---|---|
| `src/dart_params.hpp` | 纯逻辑 | 机构常量：减速比、带轮半径、导程、行程、额定力、电流上限（用 constexpr 函数按额定力算）、PID 增益（照搬仿真 `GAINS`）、流程超时、角度和位置初值（标“待标定”）、`kAutoEnabled = false`（标定完再打开自动流程） |
| `src/pid.hpp/.cpp` | 纯逻辑 | `Pid` 和 `Cascade`，行为和 `motors.py` 完全一致：D 用相邻两次误差差分，积分先限幅再求和；位置环输出转子 rpm，速度环输出电流指令 |
| `src/rotor_counter.hpp/.cpp` | 纯逻辑 | 把单圈转子角（0~8191）累计成多圈计数，处理正反向过零 |
| `src/motor_axis.hpp/.cpp` | 纯逻辑 | 一个电机的位置轴，直线（m）或转动（rad）都用它：位置 ↔ 转子计数、软限位、目标限速（同 `ramp_slider`）、到位判断（同 `settled`）、堵转回零状态机。没回零时只允许低速、低电流点动 |
| `src/slider_pair.hpp/.cpp` | 纯逻辑 | 两台滑块电机共用一个目标。左右位移差超过 5 mm（待标定）判为同步带跳齿，报故障 |
| `src/yaw_geometry.hpp/.cpp` | 纯逻辑 | yaw 角 ↔ 滑台位移。先用 s = R·sin ψ，R = 0.6045 m（旧 CAD），角度限制在 0~12°；第 9 条测完再换 |
| `src/servo_ramp.hpp/.cpp` | 纯逻辑 | 舵机目标限速（同 `RateLimiter`）；角度换算成脉宽，0~180° 对应 500~2500 µs（待确认） |
| `src/dart_inventory.hpp/.cpp` | 纯逻辑 | 记录四发飞镖在哪：发射机构上、磁铁上、左右镖座。没有传感器，按流程推断；操作手可以重置成满装（发射机构 1、磁铁 1、左右镖座各 1，同仿真） |
| `src/launcher_io.hpp` | 纯逻辑 | 纯虚接口：流程要用的执行器操作和状态查询（滑块、yaw、pitch、摆臂、推杆、锁止、电磁铁） |
| `src/launch_sequence.hpp/.cpp` | 纯逻辑 | 发射流程状态机，移植 `sequence.py`，只通过 `LauncherIo` 读写执行器（见下一节） |
| `src/mode_logic.hpp/.cpp` | 纯逻辑 | 按遥控器和裁判系统决定模式（无力、手动、自动），并算出手动指令（见“遥控器映射”） |
| `src/safety.hpp/.cpp` | 纯逻辑 | 离线计时（电机和遥控器 100 ms，裁判系统 1 s）、故障码、能不能发射（见下一节） |
| `src/hw/motor_port.hpp/.cpp` | LibXR | 包装 RMMotor：调用 `Update()`、读转子角和转速、写电流指令、判断离线。离线计时：在电机所在的 CAN 总线上，按它的反馈 ID 再注册一个接收回调（`CAN::Register`），回调里只把帧计数加一（可能在中断里调用，只做这一件事）。控制循环发现计数变了，才调 `Safety::MotorFeedback`。LibXR 的 CAN 会把一帧交给所有匹配的回调，不影响 RMMotor 自己接收。`Update()` 返回 `NO_RESPONSE` 时也按离线处理，作为兜底 |
| `src/hw/servo_port.hpp/.cpp` | LibXR | 用 `LibXR::PWM` 输出 50 Hz 舵机信号 |
| `src/hw/magnet_port.hpp/.cpp` | LibXR | 电磁铁开关。先占一个 PWM 通道，占空比给 0 或 1，这样不用改 .ioc；第 19 条定了接线再改 |
| `src/dbus_frame.hpp/.cpp` | 纯逻辑 | 解析 18 字节 DBUS 帧：4 路摇杆、2 个拨杆、拨轮；检查范围，换算成 `RemoteInput` |
| `src/hw/remote_port.hpp/.cpp` | LibXR | 在 usart3 上收 DBUS 帧，交给 `dbus_frame` 解析，记下最近一帧有效数据和收到的时间 |
| `src/hw/referee_port.hpp/.cpp` | LibXR | 订阅 Referee 的 `launcher_ref` Topic（`Referee::LauncherPack`），取里面的 `dc`，也就是 0x020A 飞镖选手端指令：`opening_status` 只有等于 0 才算闸门已开（1 关闭，2 正在打开，同 QDU `Dart.hpp` 的 `OPENING_STATUS`）；`operator_cmd_launch_time` 是最近一次发射指令的时间。0x0105 现在用不到，不取 |
| `src/hw/launcher_hw.hpp/.cpp` | LibXR | 实现 `LauncherIo`，把轴、舵机、电磁铁接起来 |
| `DartLauncher.hpp` | LibXR | 模块入口：manifest、构造参数、1 kHz 控制线程，`OnMonitor()` 每秒打印一次状态 |
| `test/host_test.cpp` | 主机测试 | 纯逻辑层的单元测试。不用测试框架，自己写 `check()` |

## 发射流程（launch_sequence 移植 sequence.py）
- 每一步有四项：名称、进入动作、完成条件、超时。超时就报故障：流程停止，保持当前状态，不解锁。
- 步骤队列用固定容量数组（至少 48 步）。“展开”步骤插到队首，和 Python 版一样。步骤名照抄仿真的中文名。
- 上膛：张开夹爪 → 滑块下压到锁止位下方 3 mm → 锁止 → 锁止确认 → 滑块回 park 位。
  - 状态未知时，先做“探测”（阶段 1d）。探测出已锁住，滑块回 park 位，上膛就算完成；探测出已放开，先张开夹爪，再走上面这条流程。
- 装填（磁铁上已经吸着一发时）：推杆收起 → 摆臂回 0° → 滑块把发射机构带下去 → 张开夹爪 → 以 0.05 m/s 慢慢抬升到装填位 → 推杆下放到放镖角 → 磁铁断电，等 0.15 s → 推杆收起 → 压下 → 锁止 → 锁止确认 → 滑块回 park 位。
  - 状态未知时同样先探测，再按探测结果装填。探测出已放开时会先张开夹爪，免得“滑块把发射机构带下去”时夹爪还合着。
- 取镖：推杆收起 → 摆到镖座角 → 推杆下探 → 磁铁通电，等 0.15 s → 推杆抬起 → 摆到待命角（±24°）或 0°。
- 发射：推杆收起；磁铁上吸着镖时，先摆到待命角 → 等 yaw、pitch 到位 → 解锁 → 等 0.5 s（实车没有出膛检测，待标定）。
- 单发和连发的展开逻辑照抄 `_expand_one`、`_expand_load`、`_expand_prefetch`、`_expand_standby`。
- 去掉两项只有仿真才能做的检查：“磁铁下面没有飞镖”和“飞镖没放到推板上”。实车没有对应的传感器。
- “已锁止”的判断：实车没有传感器，靠“锁止确认”这一步来判断；开机状态未知时靠“探测”（阶段 1d）。
  - “锁止”这一步只等舵机到位，不记已锁止。
  - “锁止确认”：滑块以 0.02 m/s 慢慢上移 10 mm，到位后再保持 0.2 s（`kLatchCheckHold`，待标定）。只看这 0.2 s 里的电流，要一直低于阈值。阈值初值取滑块电流上限的 25%，待标定。
    - 上移过程中的电流不能算：上膛时发射机构被压到锁止位下方 3 mm，滑块刚开始上移的那 3 mm 里，发射机构还压在滑块上，没被夹爪接住，这时电流本来就高。
    - 没锁住时，皮筋会一直把发射机构顶在滑块上，到位以后电流也降不下来。
  - 超过阈值就报 `LatchFailed`，滑块保持在当前位置，状态保持“未知”；一直没超过，才记为已锁住。
  - 确认通过后，滑块再快速回 park 位。回位过程中不检查电流，因为加速时电流本来就会到上限。
- 不能发射的条件（`safety`），任意一条成立就不解锁：
  - 没锁止：状态不是“已锁住”（开机的未知状态也算没锁止）
  - 滑块不在 park 位（差 10 mm 以上）
  - 推杆没收起，或者吸着镖的摆臂停在发射路径上（|摆臂角| < 待命角 − 1°）
  - yaw 或 pitch 没到位（标准同仿真：yaw 0.05°，pitch 0.5 mm）
  - 有轴没回零，或者任何电机离线、有故障
  - 比赛模式下闸门没开。“比赛模式”指启动以来收到过裁判系统数据（阶段 2b 第 8 条）；没接裁判系统时不检查闸门，自动模式照样能用

## 遥控器映射（mode_logic 先按这个写，用户可以改）
- 右拨杆下：无力。电机电流给 0，流程暂停。`SliderMustHold()` 成立时滑块除外。舵机保持最后的角度，电磁铁保持原状态，免得掉镖。
- 右拨杆中：手动。摇杆给目标速度，到软限位就停。左拨杆选控制哪一组：
  - 上：瞄准。左摇杆横向控制 yaw，右摇杆纵向控制 pitch。
  - 中：滑块和锁止。右摇杆纵向控制滑块，左摇杆横向控制锁止舵机。流程卡在半路时，用这一组手动压下、锁止。
  - 下：装弹。左摇杆横向控制摆臂，右摇杆纵向控制推杆。
- 右拨杆上：自动。只有 `kAutoEnabled` 打开时才生效。
  - 左拨杆从中拨到上：发一发。
  - 左拨杆从中拨到下：把剩下的连发打完。
  - 裁判系统在线时：闸门开着，并且收到新的发射指令时间，就发一发。
  - 没接裁判系统时，自动模式照样能用，只是没有裁判系统触发的那一路（阶段 2b 第 8 条）。
- 拨轮（只在手动模式下有效，拨到底保持 2 s 才算，一次只触发一次，方向换了重新计时）：
  - 往上（`wheel` 为正）：当前这一组的轴回零。
  - 往下（`wheel` 为负）：只在“滑块和锁止”组有效，执行一次上膛（状态未知时会先探测）。不受 `kAutoEnabled` 限制，因为上膛不发射。开机状态未知时，手动只能把滑块往上走，要靠这个操作让固件弄清楚发射机构的状态。
  - 实车上方向反了，就在 `remote_port` 里把 `wheel` 取反。
- 手动模式下流程在跑时（比如上面的上膛），摇杆指令不生效。左摇杆横向或右摇杆纵向离开中位超过 10%，或者左拨杆换组，就取消流程，回到手动控制。
- 遥控器离线超过 100 ms：无力，流程停止。`SliderMustHold()` 成立时滑块除外。

## 硬件分配（默认值，以电控实际接线为准，只改 artisans_dart.yaml）
- CAN1：
  - 滑块右 M3508，电调 ID1（反馈 0x201）
  - 滑块左 M3508，电调 ID2（反馈 0x202）。2026-10-14 实车确认方向以后（阶段 2h），改成左 `reverse: false`、右 `reverse: true`
- CAN2：
  - pitch M3508，电调 ID1（0x201）
  - yaw M2006，电调 ID2（0x202）
  - GM6020，ID1（反馈 0x205）
- 分两路的原因：五个电机挂一路 CAN 时，1 kHz 反馈加上控制帧，负载接近 90%；分两路后每路 50%~65%。
- 串口：裁判系统 usart6，遥控器 DBUS usart3。
- PWM：
  - 锁止舵机 pwm_tim8_ch2。2026-10-14 用户确认锁止舵机和旧车一样的接法、一样的接口；旧固件 `bsp_servo_pwm.c` 里它是 `servo_id 0`，即 TIM8 CH2。TIM8 的三个通道共用一个频率，设成 50 Hz。
  - 推杆舵机 pwm_tim1_ch2，电磁铁开关 pwm_tim1_ch3。这两样还没装，接线以实际为准。TIM1 的四个通道共用一个频率，统一设成 50 Hz。
  - 旧固件里锁止舵机的角度：锁死 72°、放开 100°，按 0~180° 对应 500~2500 µs 换算，就是 1300 µs 和 1611 µs（`shoot_task.c` 的 `SERVO_LOCK_ANGLE`、`SERVO_RELEASE_ANGLE`）。

## 阶段 0【用户】：克隆底座工程，编译一次（要联网）
在自己的 PowerShell 里依次运行。先在 master 上跑 `xrobot setup`，模块版本按上游的 `xrobot.lock` 锁定，然后再建分支：
```powershell
Set-Location C:\Users\Lenovo\Desktop\xrobot
git clone --recursive https://github.com/QDU-Robomaster/bsp-dev-c.git dart-fw
Set-Location dart-fw
xrobot setup
libxr stm32 toolchain gcc
cmake --preset debug
cmake --build --preset debug
git switch -c dart-launcher
```
- 成功的标志：`build\debug\DevC.elf` 存在。
- 如果 cmake 报 “Compatibility with CMake < 3.5 has been removed”，配置那一步改成 `cmake --preset debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5`。
- 其他报错，把输出贴给 Claude。

## 阶段 1：纯逻辑层和主机测试（不碰 dart-fw）
### 允许的改动
只新建 `C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher\`，里面只放三样东西：
- `src\` 下纯逻辑的文件
- `test\host_test.cpp`
- `.gitignore`（忽略 `build-host/`）

其他目录都不碰。

### 步骤
1. 先检查 `xrobot-local\DartLauncher` 不存在，存在就停。然后读仿真文件：
   - `sequence.py`、`control.py`、`motors.py`、`params.py`
   - `model\meta.json` 里的 `loader` 一段
2. 建仓库：
   ```powershell
   New-Item -ItemType Directory C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher | Out-Null
   git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher init -b main
   ```
3. 按“文件划分”表写纯逻辑的文件。`dart_params.hpp` 里的初值如下，全部标“待标定”：
   - 增益：
     - 滑块、pitch、yaw 照搬仿真 `GAINS`。速度环的输出上限和积分上限取 `min(原值, 电流上限)`。
     - GM6020 摆臂：位置环 (0.05, 0, 0, 60 rpm, 0)，速度环 (30, 0.5, 0, 8192, 4000)；目标限速 3 rad/s（同仿真 `arm_speed`）。
   - 摆臂角度：
     - 取镖：右 +48.36°，左 −48.35°
     - 待命：±24°
     - 放镖：0°
   - 推杆曲柄角：收起 0°，取镖下探 100.7°，放镖 96.4°。后两个是用 meta.json 的下探量 54.7 mm、49.8 mm，按曲柄 60 mm、连杆 114 mm 算出来的。
   - 锁止舵机：锁死 0°，解锁 45°，限速 7 rad/s。
   - 位置坐标和仿真一致，单位 m，向上为正：
     - 滑块：回零到顶部，记为 0.37；park 位 0.36。
     - 锁止位以下 3 mm 时，滑块在 −0.320（= −0.003 − push_gap 0.317）。
     - 装填位：滑块在 −0.226（= q_load 0.091 − push_gap）。
     - pitch：回零到上端，记为 0.10，行程 180 mm。
     - yaw：回零到 0 mm 那一端，记为 0。
   - 回零：
     - 速度 10 mm/s，电流用正常上限的 30%。
     - 堵转判定：电流指令已经到这个上限，转子转速小于 1 rpm，持续 200 ms。
     - 回零后退 3 mm。
4. 写 `test\host_test.cpp`，至少覆盖下面这些：
   - 电流上限：滑块 9178、yaw 2829、pitch 695，允许 ±1。
   - `Pid` 和 `motors.py` 的 `Pid` 对同一组输入的输出一致。预期值在测试里手写几组。
   - `rotor_counter` 正向、反向过零都对。
   - `motor_axis`：软限位、目标限速、到位判断、回零状态机。用假的转速和电流驱动。
   - `yaw_geometry` 正算、反算一致，超过 12° 会被夹住。
   - `slider_pair`：左右差超过阈值时报故障。
   - `launch_sequence`，配一个理想的假 `LauncherIo`（目标几拍之后到位）：
     - 满装状态下连发 4 发：步骤名的顺序和仿真一致，结束后库存为空，没有故障。可以写个临时脚本 `build-host\trace_salvo.py`，跑仿真打印 4 发连射的步骤名做对照；两边的差异只能是“发射流程”一节列出的那几处。
     - 不能发射时（比如没锁止），`cmd_fire` 报对应的故障。
     - 步骤超时会报故障。
   - `mode_logic`：拨杆和摇杆输入对应到正确的模式和手动指令；遥控器离线时进无力。
5. 编译并运行主机测试：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
6. 检查纯逻辑层没有引用框架、没有宏。下面这条命令应该没有输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
7. 提交：`git add -A`，`git commit -m "纯逻辑层和主机测试"`。然后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告。
- 第 6 步没有输出，提交核对无误。

## 阶段 1b：按审查意见修正纯逻辑层
阶段 1 已提交（b32e7be）。审查发现下面几处要改。只改 `xrobot-local\DartLauncher\src\` 和 `test\`，不碰 dart-fw。

### 要改的地方
1. 故障时滑块不能放手（最重要）。
   - 现在 `Fail()`、`Cancel()`、无力暂停都会调 `StopMotors()`，把滑块电流也清零。如果这时滑块正托着没锁住的发射机构，发射机构会被皮筋弹上去，等于走火。比如“上膛：滑块下压”因为皮筋太紧而超时，就会出现这种情况。
   - 改法：
     - 在 `safety` 里加纯函数 `SliderMustHold(const SafetyState&)`，条件见“代码规范”。
     - `LauncherIo::StopMotors()` 改成 `StopMotors(bool hold_slider)`。为 true 时，滑块目标设成当前位置并保持，其他电机清零。
     - `Fail()`、`Cancel()` 和 `Tick()` 里的安全检查分支，都按当前状态的 `SliderMustHold()` 传参。
2. 无力暂停不要调 `StopMotors`。
   - 无力由控制循环处理：给各轴传 `enabled = false`，输出 0，但保留目标；`SliderMustHold` 成立时滑块除外。这部分在阶段 2 写。
   - 流程暂停时只停计时。恢复后各轴目标还在，“瞄准”这一步能正常完成。
   - 去掉恢复时重发目标的那段代码。
3. 锁止确认改成单独的一步，规则见“发射流程”一节。
   - 新增 `Action::LatchCheck` 和对应的完成条件。
   - “锁止”这一步只等舵机到位，不再记已锁止。
   - `Condition::Parked` 去掉电流检查。回 park 位是直接给目标，加速时速度环必然打到电流上限，所以现在每次都会误报 `LatchFailed`。
   - 新增常量（都标“待标定”）：
     - `kLatchCheckRise = 0.010`
     - `kLatchCheckSpeed = 0.02`
     - `kLatchCheckCurrent = 0.25 × kSliderCurrentCap`
   - 删掉 `kLatchCurrentThreshold`。
4. 发射机构的状态改成三种：未知、已锁住、已放开（发射后停在顶部）。开机时是未知。
   - `RecordLatched(bool)` 换成能记这三种状态的接口。
   - 上膛的顺序：
     - 未知时：先下压，再张开夹爪，然后锁止 → 锁止确认 → 回位。
     - 已放开时：按仿真的顺序。
   - `ExpandLoad`：发射机构上有镖、但还没确认锁住时，先按未知状态的顺序上膛，不能走装填。现在的逻辑在开机满装时，会把磁铁上的镖往已经有镖的推板上放，然后报 `NoDarts`。
   - 新增纯函数 `LockOpenAllowed(状态)`：状态是已放开，或者滑块正在锁止位以下，才返回 true。上膛和装填里的开锁，以及阶段 2 的手动开锁，都要先经过它。正式发射的“发射：解锁”不走它，走 `FireBlocker()`（规则见“代码规范”）。
5. “锁止”（合上）这一步，要求滑块此刻就在锁止位以下（判断同 `Pressed`）。不再用那个一直不清零的 `pressed_` 标志。
6. 离线时不要把 `MotorAxis` 永久锁死。
   - 现在只要有一拍 `online = false`，就会 `MarkFault()`，而且只有重新构造对象才能恢复。开机第一帧反馈到来之前调一次，轴就废了；CAN 抖一下，也得重启。
   - 改法：
     - 离线时输出 0，清掉控制器状态，标记反馈无效。
     - 已经回过零的轴，退回“未回零”，等重新回零。
     - 不再锁存故障。
   - 加 `ClearFault()`，能从 `Failed`（比如回零超时）回到 `Idle`。
7. 回零的回退阶段改用正常的位置环。
   - 现在用 `error / dt` 当速度目标，相当于 1000 1/s 的位置增益，比速度环的带宽还高。实车上可能来回抖、判不了到位，最后回零超时。
   - 改法：目标按 `home_speed` 限速，走正常的位置环，电流仍限在 30%；到位后进 `Homed`。
8. 滑块没回零时，只准往上点动（往回零的顶端走），不准往下压发射机构。
9. 舵机角度换算：舵机角 = 安装偏置 + 方向符号 × 机构角，再夹到舵机量程内（见阶段 1c 第 8 条），最后换算成脉宽。偏置（`kLockServoOffset`、`kCrankServoOffset`）是机构角为 0 时的舵机角，初值 0，待标定。
10. `mode_logic`：手动“滑块和锁止”这一组，加上左摇杆横向控制锁止舵机的角速度（`lock_rate`）。
11. 代码排版：一行只写一条语句。现在很多行挤着三四条语句，不好读。

### 要补的测试
- 发射机构正被滑块托着时，下面每种情况都要调 `StopMotors(true)`：超时、取消、遥控器离线、电机离线。发射机构已锁住，或者滑块在 park 位时，调 `StopMotors(false)`。
- 无力暂停时不调 `StopMotors`；恢复后“瞄准”这一步能完成。
- 锁止确认：电流低时通过，记为已锁住；电流高时报 `LatchFailed`，滑块保持在原位。
- 回 park 位的过程中电流一直在上限，不报 `LatchFailed`。
- 开机状态未知时：`CommandCock` 的第一步是下压；滑块在 park 位时，`LockOpenAllowed` 返回 false。
- 已锁住、滑块在 park 位时：`LockOpenAllowed` 返回 false，手动开锁被拒；发射流程的“发射：解锁”在 `FireBlocker` 通过时照常开锁。开机未知状态下 `FireBlocker` 报 `NotLatched`。
- 开机满装时连发：先上膛再发射，不报 `NoDarts`。
- `MotorAxis`：
  - 开机后前几拍离线，不会锁死。
  - 回零后离线，退回未回零。
  - `ClearFault()` 有效。
  - 没回零时往下点动被拒绝。
- 回退阶段：假电机加一阶滞后（转速时间常数 10 ms），2 s 内到位，过冲小于 1 mm。
- 4 发连射和仿真对照：从“已锁住”状态开始。和 Python 的差异只能是每个“锁止”后面多出来的“锁止确认”。

### 验证和提交
- 编译和运行主机测试、检查框架引用：命令和阶段 1 的第 5、6 步一样。
- 提交信息写“按审查修正安全逻辑”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告。
- 框架引用和宏检查没有输出。

## 阶段 1c：第二轮审查修正（纯逻辑层）
阶段 1b 已提交（0aa8c21）。只改 `xrobot-local\DartLauncher\src\` 和 `test\`，不碰 dart-fw。

结果：已提交 12a2ce6，1248 项检查通过。第 1~6、8 条审查通过；第 7 条的做法本身有问题，阶段 1d 重写。

### 要改的地方
1. 开锁条件差了 3 mm。
   - 现在 `LockOpenAllowed` 写的是 `slider <= kCockPosition`。“下压”到位允许 ±1 mm 的误差，滑块停在 `kCockPosition` 上方一点时，下一步“张开夹爪”就会报 `NotLatched`。实车上大约一半次数会这样。
   - 按“代码规范”改：新增 `kLatchSlider = -kPushGap`，条件改成 `slider <= kLatchSlider - 0.001`。
2. 锁止确认只看到位以后的电流。
   - 现在上移的每一拍都检查电流。前 3 mm 里发射机构还压在滑块上，电流一定超过阈值，实车上每次都会误报 `LatchFailed`。
   - 按“发射流程”一节改：到位后再保持 `kLatchCheckHold = 0.2`（s，待标定），只在这段时间里检查电流。
3. 锁止状态的记录时机。
   - 现在开锁要等舵机转到位才记“已放开”。舵机转动的约 0.1 s 里如果出故障，状态还是“已锁住”，`SliderMustHold` 返回 false，滑块电流被清零；可夹爪还在继续打开，发射机构会被皮筋弹出去。
   - 按“代码规范”改：`Action::Lock` 一发出开锁指令就记“已放开”，一发出合上指令就记“未知”；锁止确认失败时保持“未知”。
4. `MotorAxis` 加保持模式，给阶段 2 的无力暂停用。
   - 进入时记下当前位置，位置环把轴保持在这里，电流照常限幅。限速目标不再前进，原来的目标、终点和限速都保留。
   - 退出后接着走原来的目标，流程不用重发。
   - 没回零的轴不能进保持模式，因为没有位置基准。
5. 舵机方向：`ServoRamp::PulseUs` 按阶段 1b 第 9 条（已更正）的公式改。
   - 现在方向取 −1 时，所有角度都被夹到 0，舵机不会动。
   - `host_test.cpp` 里断言 500 µs 的那条测试也要跟着改。
6. 4 发连射恢复逐步对照。
   - 现在的测试只数了“锁止确认”有几个，没有比对完整的步骤顺序。
   - 把 `build-host\python_trace.txt` 的 74 个步骤名写成测试里的常量数组，每个“装填：锁止”后面插一个“装填：锁止确认”，一共 77 步。从“已锁住”状态跑 4 发连射，逐步比较。
   - 测试运行时不读文件，因为 `build-host` 不进 git。
7. 状态“未知”时先探测。（执行后审查发现这个做法会误判，阶段 1d 改成到位后再判断。）
   - 现在状态未知时，上膛是先下压、再开锁。机械已经确认夹爪合着时发射机构压不过去，所以如果发射机构其实停在顶部，下压会把它顶死在夹爪上，还可能压坏夹爪。
   - 新增一步“探测”：滑块以 `kProbeSpeed = 0.05`（m/s，待标定）限速往下走，终点是 `kLatchSlider + kProbeMargin`（`kProbeMargin = 0.030` m，待标定）。一路都看电流：
     - 电流超过 `kLatchCheckCurrent`：说明发射机构没锁住，正被滑块往下推。立刻停在当前位置，状态记“已放开”。这时滑块托着发射机构，开锁是安全的。后面按“已放开”的流程走。
     - 走到终点，电流都没超过：说明发射机构锁在锁止位，状态记“已锁住”。上膛到这里就算完成，滑块回 park 位。
   - 上膛、装填、连发展开时，状态只要是“未知”，就先插入“探测”，再按探测结果展开。
   - 探测全程不动锁止舵机。
8. 新舵机的量程。
   - 新增 `kServoRange = Radians(270)`。舵机型号待确认，PM10 系列是 180°。
   - `PulseUs` 的算法：舵机角 = 偏置 + 方向 × 机构角，夹到 0~`kServoRange`，再线性换算成 500~2500 µs。
   - `kServoFrequency` 保持 50 Hz。

### 要补的测试
- 开锁条件：滑块停在 `kCockPosition + 0.0008` 时，`LockOpenAllowed` 返回 true，上膛流程能走完；停在 `kLatchSlider` 时返回 false。
- 锁止确认：
  - 假 IO 在上移阶段电流高、到位后电流低：确认通过，状态记“已锁住”。
  - 到位后的 0.2 s 里电流高：报 `LatchFailed`，状态是“未知”，调了 `StopMotors(true)`。
- 开锁指令已发出、舵机还没转到位时出故障：状态已经是“已放开”，调的是 `StopMotors(true)`。
- 保持模式：
  - 在限速目标中途进入，位置不再前进；退出后继续走到原来的终点。
  - 在直接目标（不限速）中途进入，也停在进入时的位置。
- 舵机方向和量程：
  - 偏置 270°、方向 −1：机构角 0 对应 2500 µs，机构角 135° 对应 1500 µs。
  - 偏置 0、方向 +1：机构角 90° 对应 1166.7 µs。
- 探测：
  - 状态未知、假 IO 在下降途中电流升高：探测停在原地，记“已放开”，然后张开夹爪 → 下压 → 锁止 → 锁止确认 → 回位，全程没有故障。
  - 状态未知、电流一直低：探测走到终点，记“已锁住”，滑块回位，全程没有开锁。
  - 状态未知时，装填和 4 发连射的第一步都是探测。

### 验证和提交
- 编译和运行主机测试、检查框架引用：命令同阶段 1 第 5、6 步。
- 提交信息写“第二轮审查修正”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告。
- 框架引用和宏检查没有输出。

## 阶段 1d：探测改成到位后再判断（纯逻辑层）
阶段 1c 已提交（12a2ce6）。只改 `xrobot-local\DartLauncher\src\` 和 `test\`，不碰 dart-fw。

结果：已提交 fc53348，15943 项检查通过，审查通过。

### 为什么要改
现在的“探测”（`Condition::Probe`）在移动途中只要有一拍电流超过 `kLatchCheckCurrent`，就停下来记“已放开”。这样有三个问题：
1. 误判会走火。导轨的静摩擦、卡滞，或者一拍电流毛刺，都可能让电流超过阈值。这时发射机构其实还锁着，流程却记了“已放开”，下一步“上膛：张开夹爪”就会对着锁住的发射机构开锁。开机时发射机构上默认有镖，等于意外发射一次。
2. 探测出“已放开”时，夹爪还合着（开机默认锁死）。如果是从 `CommandLoad` 进来，而且发射机构上没镖、磁铁上有镖，接下来就是装填。装填的第一个滑块动作“装填：滑块把发射机构带下去”，会把发射机构压到合着的夹爪上卡死。
3. 从 `CommandLoad` 进来、探测出“已锁住”、发射机构上有镖时，流程结束后滑块停在探测位，没有回 park 位。

### 要改的地方
1. 判断改成到位以后看保持电流。
   - 移动的方式不变：以 `kProbeSpeed` 限速走到探测位。在 `dart_params.hpp` 里加两个常量：
     - `kProbePosition = kLatchSlider + kProbeMargin`
     - `kProbeTimeout`，算法和现在 `ProbeSteps` 里的式子一样。
   - 移动途中不看电流，也不中途停下。探测位在锁止位上方 30 mm，发射机构没锁住时会被滑块推到这里，离合着的夹爪还有 30 mm，不会卡住。
   - 到位后保持 `kLatchCheckHold`，只看这段时间里的电流。计时和锁止确认一样，离开到位范围就重新计时。
     - 每拍都超过 `kLatchCheckCurrent`：滑块正托着被皮筋拉紧的发射机构，记“已放开”。
     - 每拍都没超过：记“已锁住”。
     - 有高有低，或者有一拍电流不是有限值：报新故障 `ProbeFailed`（中文“探测不出发射机构状态”），状态保持“未知”。
   - 移动和判断的时候都不动锁止舵机。
2. 判断出结果后，先把状态整理好，再接着原来的流程。
   - 已放开：插入“探测：张开夹爪”（`Action::Lock`，开锁角）。这时发射机构压在滑块上，开锁是安全的，`LockOpenAllowed` 也会通过。这样一来，“已放开”时夹爪总是开的。
   - 已锁住：插入“探测：滑块回位”，回 park 位。
3. 去掉用 `Step::side` 兼做“探测完接着上膛”标志的写法，改成展开步骤。做法同 `ExpandLoad` 取镖后重试“装填”。
   - 新增展开步骤“上膛”（`Action::Cock`）：
     - 未知：插入探测，后面再插入它自己。
     - 已放开：插入五步上膛流程。
     - 已锁住：滑块不在 park 位就插入“上膛：滑块回位”，否则什么都不做。
   - `CommandCock` 改成追加“上膛”。`CommandSalvo` 里的先上膛也改成追加“上膛”，条件不变。
   - `ExpandLoad` 在状态未知时，照旧插入探测和“装填”。

### 要补和要改的测试
- 移动途中电流高、到位后电流低：记“已锁住”，接着是“探测：滑块回位”，全程没有发开锁指令。移动途中只有一拍电流高时也一样。这就是现在会误判的情况。
- 移动途中电流低、到位后一直高：记“已放开”，接着是“探测：张开夹爪”，然后走完上膛流程（张开夹爪 → 下压 → 锁止 → 锁止确认 → 回位），最后是“已锁住”，没有故障。
- 到位后有高有低：报 `ProbeFailed`，状态还是“未知”，调了 `StopMotors(true)`，没有发开锁指令。
- 到位后电流不是有限值：报 `ProbeFailed`。现在报 `InvalidInput` 的那条测试跟着改。
- 发射机构上没镖、磁铁上有镖、夹爪合着、状态未知、到位后电流高：`CommandLoad` 以后，进入“装填：滑块把发射机构带下去”时，锁止舵机的目标已经是开锁角，全程没有故障。
- 发射机构上有镖、状态未知、电流一直低：`CommandLoad` 走完后，滑块在 park 位。
- 状态未知时，上膛、装填、4 发连射在探测出结果之前，都不发锁止舵机指令，也不发探测以外的滑块指令。
- `kProbeTimeout` 大于 `(kSliderTop - kProbePosition) / kProbeSpeed + kLatchCheckHold`。
- 现在按“途中电流高就停”写的探测测试，改成上面的新行为。77 步对照、锁止确认、保持模式等其他测试不改，照样要通过。

### 验证和提交
- 编译和运行主机测试、检查框架引用：命令同阶段 1 第 5、6 步。
- 提交信息写“探测改成到位后判断”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告。
- 框架引用和宏检查没有输出。

## 阶段 2：LibXR 适配层、模块入口、配置、固件编译
结果：DartLauncher 提交了 6ec00b5、99d7b9d，dart-fw 提交了 bea833f，主机测试 17250 项通过，固件编译通过。审查发现几处会卡住上车调试，阶段 2b 修正。

### 前提
- 阶段 0 完成：`dart-fw\build\debug\DevC.elf` 存在，当前分支是 dart-launcher。
- 阶段 1、1b、1c、1d 都已经提交。

不满足就停。

### 允许的改动
- `xrobot-local\DartLauncher\`：加 LibXR 层的文件、`DartLauncher.hpp`、`CMakeLists.txt`、`README.md`。
- 纯逻辑层：新建 `src\dbus_frame.hpp/.cpp`，改 `src\mode_logic.hpp/.cpp` 和 `test\host_test.cpp`，只为第 2 步里的 DBUS 解析和“拨轮触发上膛”。其他纯逻辑文件不改。
- `xrobot-local\index.yaml`：加一条 DartLauncher。改之前先备份到 `C:\Users\Lenovo\Desktop\xrobot\.backup\dart-fw\index.yaml`。
- `dart-fw` 里只改这几处：
  - `Modules\modules.yaml`、`Modules\sources.yaml`
  - `User\RobotConfig\`：新建 `artisans_dart.yaml`，删除其他配置
  - `User\xrobot.yaml`
  - 阶段 0 里 `libxr stm32 toolchain gcc` 改过的 preset 文件
  - xrobot 命令生成的文件
- 不改 `Modules\` 下别人的模块代码。也不改 `DevC.ioc`、`Core\`、`Drivers\`、`Middlewares\`、`User\app_main.cpp`。

### 步骤
1. 先读接口，把结论写进汇报：
   - `dart-fw\Modules\QDU-Robomaster\` 下 RMMotor、Motor、Referee 的头文件和 manifest
   - `DR16\DR16.hpp`：只作参考，不实例化。看 `ParseRC` 的位布局、`ThreadDr16` 的串口设置和读法（`SetConfig`、阻塞 `Read`、`SleepUntil`）
   - `User\app_main.cpp` 里注册的硬件名
   - `git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw show master:User/RobotConfig/dart.yaml`：看别的模块怎么拿到电机实例
   - LibXR 里 `Thread`、`PWM`、`Topic`、`UART`、`Mutex`、`CAN` 的头文件
   - `xrobot-local\ATSRuntime` 的 manifest 和 `CMakeLists.txt` 写法

   Referee 拿不到 0x020A 数据就停下汇报。（前两次执行已经查过：`launcher_ref` 里的 `dc` 就是 0x020A，可以用。）

   接口和计划描述的不一样时，按下面的顺序处理：
   - 先看“背景”一节和文件划分表里有没有写怎么处理。DR16 不公开原始数据、RMMotor 的 `Update()` 不能做 100 ms 离线检测、XRobot 1.0 没有 `hw`、`app`，这三处已经写了做法，照做就行，不用停。
   - 只是写法不同（参数怎么传、文件怎么组织、API 叫什么名字），照现有 QDU 模块的写法做，在汇报里写明，不用停。
   - 要改变“代码规范”或“发射流程”里的安全规则，或者要改“允许的改动”以外的文件，才停下汇报。
2. 写 LibXR 层的文件和 `DartLauncher.hpp`：
   - manifest V2 的 `depends` 写 RMMotor、Referee。不依赖 DR16。
   - 构造参数：五个 `RMMotor&`、每个电机所在的 `LibXR::CAN&` 和反馈 ID、`Referee&`、DBUS 用的 `LibXR::UART&`（usart3）、三个 `LibXR::PWM&`。
     - XRobot 1.0 没有 `hw`、`app`，公有构造函数的参数就是模块接口，写法参照 QDU `Dart.hpp`。YAML 里填 `XR_REGISTER` 的名字（can1、usart3、pwm_tim1_ch1 等）和前面实例的 id。
     - 控制线程在构造函数里用 `LibXR::Thread::Create` 自己开，同 `DR16` 的写法。`OnMonitor()` 写成公有成员函数，生成的主循环会每秒调一次。
     - CAN 总线和反馈 ID 是给离线计时用的。RMMotor 不公开这两个值，只能在这里再写一遍，和 RMMotor 实例的配置要一致。
     - 写错时的表现是对应电机一直显示离线，所有电机无力。README 里写清楚：改接线时两处都要改。
   - 左滑块在 YAML 里设了 `reverse`，RMMotor 已经把它的反馈和指令都取反了。两台滑块的 `MotorAxis` 用同一个 `counts_per_unit` 符号，不要再取反一次，否则两台电机会对着拧。
   - `LauncherIo::SliderCurrent()` 返回两台滑块电机这一拍电流指令（电调原始单位，就是 `MotorAxis::Update` 的输出）绝对值的较大值，不用反馈里的电流。锁止确认和探测的阈值都是按电流指令的上限定的。
   - `dbus_frame`（纯逻辑）：
     - 18 字节解析成 4 路摇杆、左右拨杆、拨轮，位布局照抄 `DR16::ParseRC`。
     - 4 路摇杆都在 364~1684、两个拨杆都是 1~3 才算有效帧，否则整帧丢掉。
     - 拨杆：1 是上，3 是中，2 是下（同 `DR16::SwitchPos` 的换算）。
     - 摇杆换算成 −1~1：（原始值 − 1024）/ 660，再夹紧。向右、向上为正。
     - 拨轮（第 16、17 字节）同样换算。不在 364~1684 时当作 0，但不丢整帧，因为有的接收机这个字段一直是 0。
     - 补测试：中位帧换算成全 0；四个摇杆分别打满；拨杆三个值的对应；摇杆越界、拨杆为 0 时整帧无效；拨轮为 0 时帧仍有效、拨轮当作 0。测试帧按位布局手工拼。
   - `remote_port`（LibXR）：
     - 用一个接收线程，照 `ThreadDr16` 的写法设串口、阻塞读 18 字节、解析。线程栈和优先级参照 master 上 dart.yaml 里 DR16 的配置（1536，`HIGH`）。
     - 有效帧存起来，带一个递增的帧计数，用互斥锁保护。
     - 控制循环每次取一份副本；帧计数变了，才调 `Safety::RemoteFeedback(now)`。
   - 控制线程 1 kHz，用 LibXR 的线程定时（比如 `Thread::SleepUntil`）。每一拍按顺序做：
     1. 读所有电机反馈
     2. 安全检查
     3. 轴控制
     4. 每 10 拍跑一次 `mode_logic` 和 `launch_sequence`
     5. 给 5 个电机都发指令。无力、遥控器离线、有故障时发 0；`SliderMustHold()` 成立时，滑块调 `SetHolding(true)`（阶段 1c），停在原位。这时滑块那根轴的 `Update` 要传 `enabled = true`，因为 `enabled = false` 会先清零输出，保持模式不出力
   - `mode_logic` 按“遥控器映射”一节改拨轮：
     - `ModeCommand` 加单拍事件 `cock_request`。往上拨保持 2 s 出 `home_request`（所有组），往下拨保持 2 s 出 `cock_request`（只在“滑块和锁止”组）。
     - 手动模式下摇杆离开中位超过 10%，或者左拨杆换组时，`stop_sequence` 为 true。
     - 补测试：两个方向各自只触发一次；`cock_request` 只在“滑块和锁止”组出现；方向中途换了重新计时；摇杆超过 10% 和换组时 `stop_sequence` 为 true。
   - 控制循环里怎么用：
     - `cock_request` 时调 `CommandCock()`。
     - `stop_sequence` 为 true 而且流程 `Busy()` 时才调 `Cancel()`。`Cancel()` 每次都会调 `StopMotors`，流程没在跑时调它，会把手动操作也停掉。
     - 流程 `Busy()` 时不执行摇杆指令。
   - 手动锁止：开锁前先过 `LockOpenAllowed()`；锁止状态按“代码规范”记录，开锁指令一发出就记“已放开”，合上指令一发出就记“未知”。状态“未知”时，手动只准把滑块往上走；要往下压，先用拨轮往下触发一次上膛（上膛会先探测）。
   - `LauncherIo::SliderAt(position, ...)` 要用滑块的实际位置和传入的 `position` 比，速度用实际速度，不能拿轴自己的目标代替。流程里有好几处传入的位置和滑块当前的目标不一样，比如“上膛”展开时查是不是已经在 park 位。
   - RMMotor 给的角度单位是弧度，设了 `reverse` 时还是负数。要先换回 0~8191 的计数（取模成非负），再交给 `RotorCounter`。
   - 舵机一上电就输出“锁死”和“推杆收起”对应的脉宽，不能先输出别的角度。
   - LibXR 层的 `.hpp/.cpp` 都放进 `src\hw\`，纯逻辑留在 `src\` 下。这样阶段 1 第 5、6 步的主机测试编译命令和框架引用检查都不用改，还是只碰纯逻辑。
   - `CMakeLists.txt` 写法参照 QDU RMMotor 的：`target_include_directories(xr PUBLIC ...)` 加模块根目录、`src`、`src/hw` 三个目录（头文件互相 include 时不带路径）；`target_sources(xr PRIVATE ...)` 加 `src/*.cpp` 和 `src/hw/*.cpp`。不加 `test`。
3. 在 `xrobot-local\index.yaml` 的模块列表里加一条：
   ```yaml
   - id: local/DartLauncher
     repo: ./DartLauncher
   ```
   然后在 DartLauncher 仓库提交，用 `git rev-parse HEAD`、`git status --porcelain` 核对。
4. 改 `dart-fw` 的配置：
   - `Modules\sources.yaml`：加 `C:\Users\Lenovo\Desktop\xrobot\xrobot-local\index.yaml`，priority 1。
   - `Modules\modules.yaml`：只留 QDU-Robomaster/RMMotor、Referee 和 xrobot-org/BlinkLED、BuzzerAlarm，再加 `local/DartLauncher@main`。版本写法和原文件一样，依赖让 xrobot 自己解析（Referee 依赖 CMD，会被自动带上）。
   - `artisans_dart.yaml` 里不要建 DR16 和 CMD 实例。DR16 会和我们抢 usart3。Referee 的 `cmd` 参数传 `nullptr`，写法同 master 上的 dart.yaml。
   - 删掉 `User\RobotConfig\` 下 QDU 的其他配置。
   - 用 `xrobot instance add` 建 `User\RobotConfig\artisans_dart.yaml`。参数按“硬件分配”一节填，写法参照第 1 步读到的 dart.yaml。
5. 生成代码并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   如果 `xrobot setup` 因为要联网而失败，就停下，请用户在自己的终端里跑这三条命令，再把输出贴回来。（`xrobot setup` 要从网上读模块索引，`--offline` 也不行，因为 DartLauncher 是第一次检出。）
   - `xrobot setup` 会把 DartLauncher 从 `xrobot-local\DartLauncher` 克隆到 `dart-fw\Modules\local\DartLauncher`。索引里写的相对路径按 `index.yaml` 所在目录解析。
   - 要改模块代码，只改 `xrobot-local\DartLauncher`，提交以后运行 `xrobot setup --update local/DartLauncher`，再编译。不要直接改 `Modules\local\DartLauncher` 里的副本。
6. 在 dart-fw 的 dart-launcher 分支提交，不推送，提交后核对。

### 验收
- 编译 0 错误，DartLauncher 的文件没有新警告。
- 汇报里附上 `arm-none-eabi-size build\debug\DevC.elf` 的输出。
- 主机测试仍然全部通过，包括新加的 DBUS 解析和拨轮测试。
- 阶段 1 第 6 步的框架引用和宏检查（只查 `src\` 下的纯逻辑）没有输出。
- 下面这条命令没有输出：
  ```powershell
  Get-ChildItem C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher -Recurse -Include *.hpp, *.cpp | Select-String '#define'
  ```

## 阶段 2b：上车前的修正
阶段 2 已提交（DartLauncher 99d7b9d，dart-fw bea833f）。审查发现下面几处会让阶段 3 卡住，或者有走火风险。

结果：第 1~7、9 条已提交（DartLauncher 92b6a56，dart-fw c352752），17328 项检查通过，审查通过。第 8 条当时还没写进计划，留到阶段 2c。

### 允许的改动
- `xrobot-local\DartLauncher\`：
  - 纯逻辑：`src\mode_logic.*`、`src\motor_axis.*`、`src\safety.*`、`src\slider_pair.*`、`src\dart_params.hpp`；`src\launcher_io.hpp` 只改 `SetSlider` 的注释。
  - LibXR 层：`src\hw\` 下的文件。
  - `README.md`、`test\host_test.cpp`。
- `dart-fw`：只有 `xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 其他文件都不碰。

### 要改的地方
1. 故障复位（最重要）。
   - 现在流程故障（`LaunchSequence::Fault()`）和 `Safety::Latch` 锁存的故障都清不掉。`RefreshState` 把它们放进 `state_.fault`，之后所有输出关断，手动也用不了，只能重启。
     - 比如还没全部回零就用拨轮触发上膛，流程马上报 `NotHomed`，整台车就锁死了。
     - 如果出故障时滑块正托着没锁住的发射机构（比如 `LatchFailed`），滑块会一直保持。这时重启等于断电放手，发射机构会被皮筋弹出去。
   - 复位操作：右拨杆在下（无力）连续保持 `kFaultResetTime = 1.0` s（待标定），再拨到中（手动）。`mode_logic` 在拨到中的那一拍发出单拍事件 `fault_reset`。
     - 只有遥控器在线、帧有效、右拨杆确实在下时才累计时间。中途离线或者拨到别的位置，就清零重来。
   - 控制循环收到 `fault_reset`，而且 `state_.fault` 不是 `None` 时，依次做下面几件事。没有故障时什么都不做，免得把暂停中的流程取消掉：
     - 调 `sequence_.Reset()`。
     - 新增纯函数 `Safety::ClearLatched()`：只清锁存的故障，不清在线计时。不要用 `Safety::Reset()`，它会让所有设备变成离线。
     - 处于 `HomeState::Failed` 的轴调 `ClearFault()`。这些轴要重新回零。
     - 新增 `SliderPair::Rebase(left, right)`：清掉同步故障，把当前左右差记成基准。以后左右差减去基准，超过 `kSliderSyncTolerance` 才报故障。另外，两台滑块电机都回零完成的那一拍，也调一次 `Rebase`。
     - 打印一行“故障已复位”。
   - 还没消失的故障（比如电机离线），下一拍会自己再报出来，这是对的。
   - 复位以后，滑块托着发射机构时照样保持。要处理卡住的流程，就用拨轮往下重新上膛。状态未知时会先探测：滑块在探测位以下时，探测会带着发射机构慢慢上到探测位，再判断。
   - 补测试：
     - 无力保持 1 s 以上再拨到中，`fault_reset` 只出现一次。
     - 不到 1 s 就拨到中，不出现。
     - 无力期间遥控器离线一下，计时重来。
     - `ClearLatched()` 以后，反馈新鲜时 `Check()` 返回 `None`。
     - `Rebase` 以后，同样的左右差不再报故障，差值再变大超过阈值时照样报。
2. 没有全部回零时，不接受上膛。
   - 只在 `state_.all_homed` 为 true 时，才因为 `cock_request` 调 `CommandCock()`。否则不调，打印一行警告。
   - 第 1 条修好以后，这种情况也不会锁死了，但这样能少一次复位。
3. 滑块的默认移动速度。
   - 现在 `SetSlider(position, 0)` 按 `kManualSliderSpeed`（0.1 m/s）限速。从压下位（−0.320 m）回 park 位（0.360 m）有 0.68 m，要 6.8 s，超过 `kReturnTimeout`（6 s）。所以“上膛：滑块回位”“装填：滑块回位”“探测：滑块回位”在实车上每次都会超时。
   - 新增 `kSliderMoveSpeed = 0.3`（m/s，待标定），`speed` 为 0 时用它。`launcher_io.hpp` 里 `SetSlider` 的注释改成“0 表示按 kSliderMoveSpeed 限速”。
   - 补测试：`(kSliderPark - kCockPosition) / kSliderMoveSpeed + 1` 小于 `kReturnTimeout`，也小于 `kPressTimeout`；`(kSliderPark - kProbePosition) / kSliderMoveSpeed + 1` 小于 `kReturnTimeout`。
4. 摆臂（GM6020）不再堵转回零，改用编码器的绝对角。
   - 现在摆臂以 1°/s 去找 −95° 的机械端点。从 0° 出发要 95 s，从另一端出发要 190 s，都超过 `kHomeTimeout`（90 s），所以回零一定失败：轴变成 `Failed`，被锁存成故障。另外，开机时库存默认磁铁上吸着一发，堵转回零会带着镖撞端点。
   - GM6020 直接驱动摆臂，它的编码器在一圈以内是绝对角。摆臂的范围（±95°）不到一圈，所以一上电就知道角度，不用动。
   - 新增 `kArmZeroCounts`：摆臂在 0° 时编码器的原始值，0~8191，待标定，初值 0。
   - `MotorAxis` 新增 `bool HomeAt(double position)`：有有效反馈而且不是 `Failed` 时，把当前位置定成 `position`，直接进 `Homed`，目标也设成 `position`，不产生运动，返回 true。否则返回 false。
   - `MotorPort` 记下最近一次的原始编码器值（0~8191，就是现在换算出来交给 `RotorCounter` 的那个值），提供 `Raw()`。
   - 控制循环：摆臂在线、有反馈、没有 `Homed` 也不是 `Failed` 时，算出角度：(`Raw()` − `kArmZeroCounts`) × 2π / 8192，换算到 (−π, π]，然后调 `HomeAt`。掉线以后轴会退回未回零，重新上线时会这样自动恢复。
   - “装弹”组拨轮往上时，不再让摆臂回零，只打印一行“摆臂按编码器绝对角，不需要回零”。
   - 补测试：`HomeAt` 以后 `Position()` 等于给定值，`SetTarget` 能用；没有反馈或者 `Failed` 时返回 false；掉线以后退回未回零，再调 `HomeAt` 能恢复。
5. 摇杆死区。
   - DR16 的摇杆回中以后，读数还会抖几个计数。现在这些抖动直接换算成速度，造成三个问题：
     - “滑块和锁止”组：左摇杆往“合上”方向抖一下，就会调 `SetLock(kLockClosed)`。`SetLock` 把它当成合上指令，记成“未知”。上膛确认过的“已锁住”就丢了，又得重新探测。
     - “装弹”组：推杆会慢慢漂。
     - 滑块：速度不是 0，就不进保持，会慢慢爬。
   - `mode_logic` 加 `kStickDeadband = 0.05`（待标定）。绝对值小于它就当作 0；超过的部分按 (|v| − 死区) / (1 − 死区) 重新缩放，符号不变，这样打满还是满速。左摇杆横向、右摇杆纵向都要过死区。“超过 10% 就取消流程”的判断照旧用原始值。
   - 手动锁止：夹紧以后的目标和当前角度相差不到 1e-6 rad 时，不调 `SetLock`。机构没动，锁止状态就不变。推杆也一样处理。
   - 补测试：输入 0.04、0.05 时输出 0；1 和 −1 是满速；0.5 是满速的 0.45 / 0.95。
6. 滑块托着发射机构时，不准重新回零滑块。
   - 滑块回零是往上走，电流只有正常上限的 30%。如果这时滑块正托着没锁住的发射机构，30% 的电流可能拉不住皮筋，滑块会和发射机构一起被弹上去，等于走火。
   - “滑块和锁止”组拨轮往上时，`SliderMustHold(state_)` 成立就拒绝回零，打印一行警告。这种情况下滑块本来就已经回过零，不需要再回。
7. 补全状态打印，阶段 3 标定要用。`OnMonitor()` 每秒打印两行：
   - 第一行照旧：模式、故障、步骤、库存、锁止状态。
   - 第二行：
     - 5 个电机按左滑块、右滑块、pitch、yaw、摆臂的顺序，各打印在线和回零（1 或 0）。
     - 左右滑块位置（mm）、pitch 位置（mm）、yaw 角（°）和滑台位置（mm）、摆臂角（°）和编码器原始值。
     - 锁止、推杆的机构角（°）和输出脉宽（µs）。
   - 数据仍由控制线程在锁里拷一份，`OnMonitor()` 不直接读轴对象。
8. README 按上面几条更新：故障复位怎么操作、摇杆死区、摆臂不回零以及 `kArmZeroCounts` 怎么标定、滑块托着发射机构时不准回零、打印格式。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“上车前修正”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。
3. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   `xrobot setup` 联网失败就停下，请用户在自己的终端里跑这三条命令。
4. 在 dart-fw 提交 `xrobot.lock`，提交信息写“更新 DartLauncher”，提交后核对。
5. 汇报里附上 `arm-none-eabi-size build\debug\DevC.elf` 的输出。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误，DartLauncher 的文件没有新警告。

## 阶段 2c：没接裁判系统时自动模式也能用
阶段 2b 已提交（DartLauncher 92b6a56，dart-fw c352752）。这一条是 2026-10-14 用户确认的新要求，当时 2b 已经在跑，所以单独做一个阶段。

结果：已提交（DartLauncher 1379f01，dart-fw 0e882ab），17339 项检查通过，审查通过。固件可以上车了。

### 允许的改动
- `xrobot-local\DartLauncher\`：`src\hw\launcher_hw.hpp/.cpp`、`README.md`、`test\host_test.cpp`。
- `dart-fw`：只有 `xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 其他文件都不碰。纯逻辑层不用改：`Safety::Check` 和 `FireBlocker` 里 `competition` 的用法保持原样。

### 要改的地方
1. `launcher_hw.cpp` 里 `state_.competition = command_.mode == Mode::Automatic;` 改成“启动以来收到过裁判数据”。
   - 加一个成员 `bool referee_seen_ = false`。`referee_.Read()` 返回 true 的那一拍置为 true，之后一直保持 true，掉线也不清。
   - `state_.competition = referee_seen_;`
   - 效果：
     - 没接裁判系统：`competition` 一直是 false。自动模式能用，不检查裁判在线和闸门，靠左拨杆中→上发一发、中→下连发。
     - 接了裁判系统：收到第一帧以后变成 true，于是要求裁判在线、闸门打开，中途掉线报 `RefereeOffline`。比赛时就要这样。
2. README 的“遥控与安全”一节补一句：没接裁判系统时自动模式照常能用，只是没有裁判系统触发那一路；接上以后才检查闸门和裁判在线。

### 要补的测试
纯逻辑层没有这个逻辑，所以测 `Safety` 的两种取值就够了，不用新建假 IO：
- `competition = false` 时，从来没调过 `RefereeFeedback`，`Check()` 不报 `RefereeOffline`；`FireBlocker` 在其他条件都满足、`gate_open = false` 时不报 `GateClosed`。
- `competition = true` 时，没调过 `RefereeFeedback` 就报 `RefereeOffline`；调过但过了 1 s 以上也报 `RefereeOffline`。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“没接裁判系统也允许自动模式”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。
3. 更新固件并编译，命令同阶段 2b 的验证第 3 步。`xrobot setup` 联网失败就停下，请用户自己跑。
4. 在 dart-fw 提交 `xrobot.lock`，提交信息写“更新 DartLauncher”，提交后核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误，DartLauncher 的文件没有新警告。

## 阶段 2d：按实车现状改（皮筋和锁止舵机已装，换弹机构没装）
2026-10-14 用户说明：皮筋和锁止舵机都已经装好，不好拆；换弹机构（GM6020 摆臂、推杆舵机、电磁铁）整套还没装。阶段 2c 已提交（DartLauncher 1379f01，dart-fw 0e882ab）。

现在的固件在这种状态下上电有两个问题：
- 锁止舵机一上电就输出 500 µs（`kLockClosed = 0` 加偏置 0 的结果），也就是舵机量程的一端。偏置和方向都还没标定，舵机会从装好的位置猛地转过去，最多 270°。锁止机构有限位，这一下可能顶坏连杆或者舵机齿轮。万一发射机构这时正被夹爪锁着，夹爪被拨开就是走火。
- 摆臂电机不在车上，一直离线。`state_.motors_online` 为 false，`Safety::Check` 也会报 `MotorOffline`，于是所有电机无力，滑块、pitch、yaw 一个都动不了，阶段 3 没法做。

### 允许的改动
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.hpp/.cpp`、`src\hw\servo_port.hpp/.cpp`、`src\hw\magnet_port.hpp/.cpp`、`README.md`、`test\host_test.cpp`。
- `dart-fw`：只有 `xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 其他文件都不碰。纯逻辑层的安全规则不改。

### 要改的地方
1. 换弹机构没装时，不要求它在线。
   - 新增 `constexpr bool kLoaderInstalled = false;`（待确认，装好以后改成 true）。
   - 为 false 时：
     - `state_.motors_online` 只看滑块两台、pitch、yaw 四台，不看摆臂。
     - `state_.all_homed` 同样不看摆臂。
     - 每拍都调一次 `safety_.MotorFeedback(4, now)`，让摆臂那个槽位保持新鲜，否则 `Safety::Check` 会报 `MotorOffline`。注释写明原因。
     - 摆臂的 `MotorPort::Send()` 照常调用，电流指令是 0。RMMotor 要求同一控制帧里每个电机每拍都发，少发一个整帧都不发，所以不能跳过。
     - 不按绝对角给摆臂建零点（阶段 2b 第 4 条那段跳过）。
     - 不构造、不驱动推杆舵机和电磁铁的 PWM：`ServoPort` 和 `MagnetPort` 加一个 `enabled` 参数，为 false 时构造函数不调 `SetConfig`、`SetDutyCycle`、`Enable`，`Update`、`Set` 也直接返回。
     - 手动“装弹”组（左拨杆下）不动任何执行器，每次收到该组的摇杆输入时打印一行“换弹机构未安装”（限每秒最多一行，别刷屏）。
     - `CommandLoad`、`CommandSalvo`、`CommandFire` 都拒绝执行，打印一行“换弹机构未安装”。上膛（`cock_request`）照常可用，因为它只用滑块和锁止。
   - 为 true 时行为和现在完全一样。
2. 锁止舵机：没标定前不输出 PWM，由操作手手动唤醒。
   - 新增 `constexpr bool kLockServoCalibrated = false;`（标定完改成 true）和 `constexpr double kLockBootPulseUs = 1500;`（µs，舵机中位）。
   - 为 false 时：
     - 上电不使能锁止 PWM 通道，也不写占空比。舵机收不到信号就不会动，锁止机构留在原位。按机械 2026-10-12 的说明，发射机构是靠锁止机构本身卡住的，舵机只是拨动它的开关，所以不给信号是安全的。
     - 手动“滑块和锁止”组里，左摇杆横向第一次给出非零输入（已经过死区）时，才使能通道并输出 `kLockBootPulseUs`。之后左摇杆横向直接调脉宽，速率 `kLockPulseRate = 200`（µs/s，待标定），范围夹在 `kServoPulseMin`~`kServoPulseMax`。这一路不经过机构角和偏置换算。
     - 这个模式下 `LockAt()` 一律返回 false，`SetLock()` 不改变输出，所以任何自动流程里的锁止步骤都会超时报故障，不会偷偷动舵机。打印一行“锁止舵机未标定”。因此 `cock_request` 在这个模式下也先拒绝，打印同一行。
     - `state_.latch_state` 不随手动调脉宽改变，保持“未知”。标定阶段不记锁止状态。
   - 为 true 时行为和现在完全一样：上电输出锁死角对应的脉宽，流程和手动都走机构角。
3. 第二行打印补上这个阶段要看的值：
   - 锁止舵机未标定时，打印当前原始脉宽（没使能就打印 0）和一个标记，代替原来的 `lock_deg`。
   - 换弹机构没装时，摆臂和推杆那几项打印成固定值，并带一个标记，免得看错。
4. README 补一节“实车分阶段装配”：
   - `kLoaderInstalled`、`kLockServoCalibrated` 两个开关各管什么，什么时候改成 true。
   - 锁止舵机的标定步骤（见阶段 3）。

### 要补的测试
纯逻辑层没有这两个开关，所以只测常量和已有逻辑不受影响：
- `kLoaderInstalled == false` 且 `kLockServoCalibrated == false`（这一版的预期值）。
- `kLockBootPulseUs` 在 `kServoPulseMin`~`kServoPulseMax` 之间，`kLockPulseRate > 0`。
- 已有的 17339 项全部照旧通过。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“按实车现状分阶段装配”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。
3. 更新固件并编译，命令同阶段 2b 的验证第 3 步。`xrobot setup` 联网失败就停下，请用户自己跑。
4. 在 dart-fw 提交 `xrobot.lock`，提交信息写“更新 DartLauncher”，提交后核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误，DartLauncher 的文件没有新警告。

## 阶段 2e：装配开关改成按设备分开
2026-10-14 实车反馈：现在只接了两台滑块 M3508 和锁止舵机，pitch、yaw、摆臂都没接。固件打印 `online=1/1/0/0/0`，报“电机离线”，所有电机无力，手动模式用不了，阶段 3 第 4 步往后都做不了。

阶段 2d 只有一个 `kLoaderInstalled`，管摆臂、推杆、电磁铁三样，没有考虑 pitch 和 yaw 也可以不接。改成每个设备一个开关。阶段 2d 已提交（DartLauncher d4ffe24，dart-fw 8275b40）。

结果：第 1~7 条已提交（DartLauncher 1cd38e3，dart-fw bdfa400），17348 项检查通过，审查通过。第 8、9 条是执行期间才加的，没做，见阶段 2f。

### 允许的改动
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.hpp/.cpp`、`README.md`、`test\host_test.cpp`。
- `dart-fw`：`User\RobotConfig\artisans_dart.yaml`（只改第 8 条那一行），`xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 其他文件都不碰。纯逻辑层的安全规则不改。

### 要改的地方
1. `dart_params.hpp` 里把 `kLoaderInstalled` 换成按设备分开的开关，当前实车状态的初值如下：
   ```
   kSliderInstalled = true    // 两台滑块 M3508
   kPitchInstalled = false
   kYawInstalled = false
   kArmInstalled = false      // GM6020 摆臂
   kCrankInstalled = false    // 推杆舵机
   kMagnetInstalled = false   // 电磁铁
   ```
   每个都注释“装好后改 true”。`kLockServoCalibrated` 保持 false 不变。
2. 把 2d 里所有 `kLoaderInstalled` 的判断换成对应的单个开关：
   - 摆臂在线、回零、绝对角建零点、`Compute`/`Gate` 的 enabled、`Raw()` 打印 → `kArmInstalled`。
   - 推杆 PWM 的构造和 `Update` → `kCrankInstalled`。
   - 电磁铁 PWM → `kMagnetInstalled`。
   - `CommandLoad`、`CommandFire`、`CommandSalvo` 要求 `kArmInstalled && kCrankInstalled && kMagnetInstalled` 全部为 true，否则拒绝并打印“换弹机构未安装”。
3. 在线和回零检查只算装了的设备。
   - `state_.motors_online`：只看开关为 true 的电机。滑块两台受 `kSliderInstalled` 控制，pitch 受 `kPitchInstalled`，yaw 受 `kYawInstalled`，摆臂受 `kArmInstalled`。
   - `state_.all_homed`：同样只看装了的轴。
   - 没装的电机，每拍都调一次 `safety_.MotorFeedback(index, now)`，让它那个槽位保持新鲜，否则 `Safety::Check` 会报 `MotorOffline`。编号：滑块左 0、右 1、pitch 2、yaw 3、摆臂 4。
   - 没装的电机照旧每拍调 `Send()`，电流指令是 0。RMMotor 同一控制帧里少发一个，整帧都不发。
   - 全部电机都没装时（不该出现，但别让它崩）：`motors_online` 和 `all_homed` 都为 false，不要变成“全部在线”。
4. 手动点动和回零只动装了的轴。
   - “瞄准”组：`kPitchInstalled`、`kYawInstalled` 为 false 时不点动对应的轴。两个都没装时，回零和点动都打印“该机构未安装”，限每秒一行（同 2d 的 `loader_warn_time_` 做法，给每组各一个计时变量，或者共用一个）。
   - “装弹”组：`kArmInstalled`、`kCrankInstalled` 都为 false 时，同样打印。
   - “滑块和锁止”组：`kSliderInstalled` 为 false 时不点动滑块，但锁止舵机那一路照常可用。
5. `Aimed()` 在 pitch 或 yaw 没装时返回 false，这样流程里的“瞄准”步骤会超时报故障，不会以为已经对准。现在的实现靠 `Settled()`，没回零就返回 false，行为已经对了；加一条注释说明，不用改代码。
6. 第二行打印：没装的设备那几项打印 0 加 `[X]` 标记，和 2d 的写法一致。现在变成 pitch、yaw 也可能带 `[X]`。
   - 打印分支现在有四种组合（锁止标定 × 换弹装配），再加 pitch/yaw 就太多了。改成先把每一段拼成固定宽度的小段，再用一个 `Printf` 输出，别继续堆分支。
7. README 的“实车分阶段装配”一节按新开关改写，列出六个开关各管什么、什么时候改成 true。
8. 锁止舵机改接 TIM8 CH2（2026-10-14 用户确认和旧车接法一样）。
   - `dart-fw\User\RobotConfig\artisans_dart.yaml` 里 DartLauncher 的 `lock_pwm` 从 `pwm_tim1_ch1` 改成 `pwm_tim8_ch2`。`crank_pwm`、`magnet_pwm` 不动。
   - README 的接线表同步改。
9. 锁止舵机的唤醒脉宽改用旧车的锁死值。
   - `kLockBootPulseUs` 从 1500 改成 1300，注释写明来源：旧固件 `SERVO_LOCK_ANGLE = 72`，按 0~180° 对应 500~2500 µs 换算。
   - 新增 `kLockReleasePulseRef = 1611`（µs），注释写“旧固件 `SERVO_RELEASE_ANGLE = 100` 换算，只作参考，以阶段 3 实测为准”。这一版代码里不用它。
   - 测试：两个值都在 `kServoPulseMin`~`kServoPulseMax` 之间，`kLockBootPulseUs < kLockReleasePulseRef`。

### 要补的测试
- 六个开关的当前预期值（滑块 true，其余 false）。
- `kLockServoCalibrated == false`。
- 已有的 17343 项全部照旧通过。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“装配开关按设备分开”，提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对。
3. 更新固件并编译，命令同阶段 2b 的验证第 3 步。`xrobot setup` 联网失败就停下，请用户自己跑。
4. 在 dart-fw 提交 `xrobot.lock`，提交信息写“更新 DartLauncher”，提交后核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误，DartLauncher 的文件没有新警告。

## 阶段 2f：锁止舵机改接 TIM8 CH2
阶段 2e 已提交（DartLauncher 1cd38e3，dart-fw bdfa400）。2e 的第 8、9 条是在 Codex 执行期间才加进计划的，这次没做，单独补上。

### 允许的改动
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`README.md`、`test\host_test.cpp`。
- `dart-fw`：`User\RobotConfig\artisans_dart.yaml` 的 `lock_pwm` 那一行，`xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 其他文件都不碰。

### 要改的地方
就是阶段 2e 的第 8、9 条，原样照做：
1. `artisans_dart.yaml` 里 DartLauncher 的 `lock_pwm: pwm_tim1_ch1` 改成 `lock_pwm: pwm_tim8_ch2`。`crank_pwm`、`magnet_pwm` 不动。
2. `dart_params.hpp`：
   - `kLockBootPulseUs` 从 1500 改成 1300，注释写明来源：旧固件 `shoot_task.c` 的 `SERVO_LOCK_ANGLE = 72`，按 0~180° 对应 500~2500 µs 换算。
   - 新增 `kLockReleasePulseRef = 1611`（µs），注释写“旧固件 `SERVO_RELEASE_ANGLE = 100` 换算，只作参考，以阶段 3 实测为准”。代码里不用它。
3. README 的接线表：锁止舵机改成 `pwm_tim8_ch2`，注明和旧车接法一样。“实车分阶段装配”一节里唤醒脉宽改成 1300 µs。
4. 测试：`kLockBootPulseUs == 1300`；两个值都在 `kServoPulseMin`~`kServoPulseMax` 之间；`kLockBootPulseUs < kLockReleasePulseRef`。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“锁止舵机按旧车接法和角度”，提交后核对。
3. 更新固件并编译，命令同阶段 2b 的验证第 3 步。`xrobot setup` 联网失败就停下，请用户自己跑。
4. 在 dart-fw 提交 `artisans_dart.yaml` 和 `xrobot.lock`，提交信息写“锁止舵机改接 TIM8 CH2”，提交后核对。
5. 汇报里贴出 `artisans_dart.yaml` 里 `lock_pwm` 那一行，和 `dart_params.hpp` 里两个脉宽常量那两行。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误。
- `artisans_dart.yaml` 里是 `pwm_tim8_ch2`。

## 阶段 2g：遥控器收帧对齐、状态打印分行
阶段 2f 已提交（DartLauncher 16dcac9，dart-fw 155baf1）。2026-10-14 第一次带遥控器上电，发现两个问题。

### 为什么要改
1. 遥控器几乎一直显示离线，偶尔又会收到一帧。
   - 打印一直是 `fault=遥控器离线`。但中间锁止舵机被唤醒了一次（日志“锁止舵机首次唤醒 1300 us”），滑块也往上走了约 1.3 mm。这两件事都要求那一刻遥控器在线、右拨杆在中。所以帧不是完全收不到，而是绝大多数都被丢掉了。
   - 用户在同一块板子上烧旧的 Keil 固件，能看到遥控器数据，所以接线和接收机都没问题。
   - 原因在 `RemotePort::ThreadMain`：它每次从串口字节队列里读 18 个字节就当成一帧，没有按帧边界对齐。
     - 串口 DMA 在 `app_main` 里构造时就开始收，`SetConfig` 又重启一次，开始的时刻是随机的，很可能落在一帧中间。队列容量 128 字节，接收线程启动前如果积攒超过约 7 帧，还会溢出丢字节。
     - 只要错开一次，之后每次读的 18 个字节都横跨两帧，一直错下去。错位的数据大多通不过范围检查，所以显示离线。
     - 偶尔错位的数据刚好落在范围里，就会被当成一帧有效数据，拨杆和摇杆的值都是乱的。**这些乱帧会直接驱动执行器**，和实际拨杆的位置无关。
   - 旧固件靠 IDLE 中断分帧：两次 IDLE 之间正好收到 18 个字节才算一帧。LibXR 不把 IDLE 事件交给模块，所以改成观察队列里的字节数来分帧，效果一样。
   - QDU 的 DR16 模块是同样的读法，所以它也有这个问题。我们不用它，不用管。
2. 第二行状态打印被截断了。
   - 这一行带固定宽度补空格，约 226 字节。USB 虚拟串口的发送缓冲只有 128 字节（`app_main.cpp` 里 `usb_otg_fs_cdc(..., 128, 128, 3, ...)`），后面的摆臂、锁止、推杆都被截掉了。阶段 3 标锁止舵机要看的 `lock_us` 就在被截掉的部分里。

### 允许的改动
- `xrobot-local\DartLauncher\`：
  - 新建纯逻辑 `src\dbus_framer.hpp/.cpp`。
  - `src\dart_params.hpp`、`src\hw\remote_port.hpp/.cpp`、`src\hw\launcher_hw.hpp/.cpp`、`DartLauncher.hpp`、`README.md`、`test\host_test.cpp`。
- `dart-fw`：只有 `xrobot setup --update local/DartLauncher` 改动的 `xrobot.lock`，和 xrobot 生成的文件。
- 不改 LibXR、BSP（`Core\`、`User\app_main.cpp`、`User\libxr_config.yaml`）和 QDU 的模块。

### 要改的地方
1. 新增纯逻辑类 `DbusFramer`，按“线路空闲”分帧。
   - `dart_params.hpp` 加常量：`kDbusFrameSize = 18`，`kDbusQuietMs = 3`，`kDbusStallMs = 20`。注释写依据：
     - 一帧 18 字节，100000 波特 9 位加起止位，传输约 2 ms；DR16 每 14 ms 发一帧，帧间空闲约 12 ms。
     - DMA 的半满、全满中断会把一帧切成两段，两段之间不超过 2 ms，所以空闲 3 ms 以上才算一帧结束。
     - 队列 128 字节，积攒约 85 ms 才会溢出，所以超过 20 ms 没轮询就当作可能丢过字节。
   - 接口：
     ```cpp
     enum class DbusAction : std::uint8_t { None, ReadFrame, Clear };
     class DbusFramer {
     public:
         // now_ms 单调毫秒（允许 32 位回绕），queued 为串口接收队列当前字节数。
         DbusAction Poll(std::uint32_t now_ms, std::size_t queued);
         // 读帧失败等异常时调用，回到未对齐状态。
         void Reset();
     };
     ```
   - 规则（时间差都用无符号减法，处理回绕）：
     - 第一次调用：记下时间和字节数，状态为“未对齐”，返回 `None`。
     - 距离上次 `Poll` 超过 `kDbusStallMs`：改成“未对齐”，因为期间可能溢出丢过字节。
     - 字节数和上次不同：记下新的字节数和这个时刻，返回 `None`。
     - 字节数没变，但距离上次变化不到 `kDbusQuietMs`：返回 `None`。
     - 线路已经空闲：
       - 字节数为 0：改成“已对齐”，返回 `None`。
       - 字节数正好 18，而且“已对齐”：返回 `ReadFrame`。
       - 其他情况（不是 18，或者没对齐）：返回 `Clear`，并改成“已对齐”，因为清空发生在空闲期，下一个字节就是下一帧的开头。
       - 返回 `ReadFrame` 或 `Clear` 以后，把记下的字节数当作 0，时刻当作现在。
2. `remote_port` 改用 `DbusFramer`。
   - 接收线程改成每 1 ms 轮询一次（`SleepUntil(last, 1)`），不再阻塞读 18 字节。
   - 每次轮询：`queued = uart_.read_port_->Size()`，交给 `Poll`。
     - `ReadFrame`：`uart_.Read({frame, 18}, op_)`。这时数据已经在队列里，会马上返回。返回 `OK` 就交给 `ParseDbusFrame`，通过才发布；不是 `OK` 就调 `ClearQueuedData()` 和 `framer_.Reset()`。
     - `Clear`：调 `uart_.read_port_->ClearQueuedData()`。
   - 加三个计数：有效帧 `ok`、清空次数 `clr`、读到 18 字节但解析失败的 `bad`。和帧数据一起放在互斥锁里，控制线程取快照时一起取走。
   - 保留构造函数里的 `SetConfig` 和线程优先级、栈大小。
3. 状态打印分成短行。
   - 每行单独调一次 `LibXR::STDIO::Printf`，结尾 `\r\n`，不补空格。两行之间 `LibXR::Thread::Sleep(5)`，让 128 字节的发送缓冲有时间发完。`OnMonitor()` 在主循环里调用，睡这几毫秒没有影响。
   - 每行最坏情况不超过 110 字节（中文按 UTF-8 每字 3 字节算）。汇报里列出每行的最坏长度。
   - 每行都以 `Dart` 开头。建议排法：
     - 第 1 行：模式、锁止状态、库存、故障。
     - 第 2 行：当前步骤名。步骤名最长约 39 字节，单独一行。
     - 第 3 行：遥控器计数 `ok`、`clr`、`bad`。
     - 第 4 行：遥控器解析结果：左右拨杆（打印成 U、M、D）、四个摇杆和拨轮（保留两位小数）。阶段 3 要靠这一行核对通道和正负号。
     - 第 5 行：五个电机的在线和回零，滑块左右、pitch 位置（mm），yaw 角（°）。没装的设备后面加 `[X]`。
     - 第 6 行：摆臂角和编码器原始值、锁止脉宽（未标定时加 `[R]`，没唤醒时为 0）、推杆脉宽、控制线程耗时（见第 4 条）。
4. 控制线程耗时统计，阶段 3 要看 `double` 运算在 F407 上跑不跑得完。
   - `DartLauncher::ThreadMain` 里，每次 `Tick` 前后读 `LibXR::Timebase::GetMicroseconds()`，交给 `LauncherHardware` 记录。
   - 每 1000 拍统计一次：平均耗时、最长耗时（µs），以及这 1000 拍里两次唤醒间隔超过 2 ms 的次数。放进监控快照，在第 6 行打印。
5. README 补充：遥控器分帧的做法和三个计数的含义；新的打印格式；`clr` 只在上电或者线路受干扰时增加，平时 `ok` 每秒约增加 70。

### 要补的测试
用一个模拟串口队列：按给定的时刻往里放字节，每 1 ms 调一次 `Poll`，`ReadFrame` 时取走 18 字节，`Clear` 时清空。DR16 按每 14 ms 一帧、每帧 18 字节模拟，帧内可以拆成两段。
- 上电时队列里先有 7 个残留字节：第一次空闲时 `Clear`，下一帧完整读到，而且取走的 18 字节正好是那一帧（按字节内容比对）。
- 一帧拆成 10 + 8 两段、相隔 2 ms：只产生一次 `ReadFrame`。
- 只到了 12 个字节就空闲：`Clear`。
- 积攒了两帧（36 字节）才空闲：`Clear`，之后的帧正常读到。
- 轮询中断 25 ms，期间刚好到了一整帧 18 字节：不读，`Clear`；再下一帧正常读到。
- 连续 1 s 的帧流：除了开头最多一次 `Clear`，其余每帧都读到，而且全部对齐。
- 每 7 ms 一帧（帧间空闲约 5 ms）也能全部读到。
- 毫秒计数在 `0xFFFFFFFF` 附近回绕时，行为不变。
- 每次 `ReadFrame` 取出的 18 字节，都和模拟发出的某一整帧完全相同（这是最关键的一条：不能出现错位帧）。
- 已有的检查全部照旧通过。

### 验证和提交
1. 主机测试和框架检查：命令同阶段 1 第 5、6 步。
2. 在 DartLauncher 仓库提交，提交信息写“遥控器按线路空闲分帧，状态打印分行”，提交后核对。
3. 更新固件并编译，命令同阶段 2b 的验证第 3 步。`xrobot setup` 联网失败就停下，请用户自己跑。
4. 在 dart-fw 提交 `xrobot.lock`，提交信息写“更新 DartLauncher”，提交后核对。

### 验收
- 主机测试全部通过，`-Werror` 下没有警告；框架引用和宏检查没有输出。
- 固件编译 0 错误，DartLauncher 的文件没有新警告。
- 汇报里列出每行打印的最坏长度，都不超过 110 字节。

## 阶段 2h：滑块电机方向取反
任务文件：`plans/tasks/2h-slider-reverse.md`。从这一阶段起，每个 Codex 任务单独写一个短文件，不再写进这份文档。

结果：已提交（DartLauncher 1fe973c，dart-fw 5eed0b1），审查通过，已烧录。滑块没回零时现在往上走，电机方向对了。后续状态见工作区根目录的 `handoff.md`。

## 阶段 3【用户】：上车调试
实车现状（2026-10-14）：只接了两台滑块 M3508 和锁止舵机；pitch、yaw、换弹机构（摆臂、推杆、电磁铁）都没接。皮筋已装，发射机构停在顶部。固件按阶段 2e 的开关走：只有 `kSliderInstalled = true`，其余都是 false，`kLockServoCalibrated = false`。

接上新设备以后，要把对应的开关改成 true，重新编译烧录，再调那一部分。告诉我接了什么，我改开关。

1. 用 DAP-Link 烧录：
   ```powershell
   & "C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program build/debug/DevC.elf verify reset exit"
   ```
2. 第一次上电前：
   - 发射机构停在顶部，皮筋松着，就保持这样。先标锁止舵机（第 6 步），再上膛。不要手动把它压下去卡住，那样夹爪一旦被拨开就是满张力的皮筋。
   - 皮筋已经装着。所以在第 7 步标完 `kLatchSlider` 之前，不要让滑块往下走。
   - 锁止舵机不用拆。这一版固件上电不给它信号，它不会动。
   - 推杆舵机和电磁铁没装，固件也不会驱动那两个 PWM 通道。
3. 上电后应该是无力状态。打开 USB 串口终端，看每秒打印的状态（阶段 2g 以后是六行短行）。
   - 先核对遥控器（第 3、4 行）：
     - `ok` 每秒约增加 70；`clr` 只在刚上电时加一两次，之后不再增加；`bad` 一直是 0。
     - 拨左右拨杆，第 4 行的 U/M/D 要跟着对上。推每个摇杆和拨轮，对应的值要在 −1~1 之间变化，方向和你推的方向一致。
     - 上面任何一条不对，先不要往下做，把这几行发给我。
   - 出了故障（打印里 fault 不是“无故障”）：右拨杆拨到下保持 1 s 以上，再拨到中，就会复位。
4. 检查在车上的电机：
   - 装了的设备在线标志是 1，没装的是 0 但不影响出力。现在的预期是 `online=1/1/0/0/0`。
   - 手动模式下低速点动，确认方向，记下每 mm（或每度）的计数。
     - 滑块：右摇杆往上推，滑块要实际往上走，第 5 行 `sl=` 的两个数一起变大。
     - 任何轴方向反了：马上把右拨杆拨到下（无力），不要回零，也不要上膛，把现象发给我。没回零的轴只能往回零端走，方向一反，回零就会往反方向压。
   - 滑块先只往上点动。往下要等第 7 步标完 `kLatchSlider`。
   - 拔掉滑块那条 CAN 线，两台滑块应该变成无力。
5. pitch 和 yaw 接上以后再做（现在没接，跳过）：“瞄准”组回零，拨轮往上拨到底保持 2 s。电流不能超过上限，联轴器不能打滑。
   - 顺便看一下：pitch 电机不出力时，丝杆会不会被皮筋拉着倒转。
6. 锁止舵机标定。必须排在上膛之前：固件在 `kLockServoCalibrated = false` 时会拒绝上膛（锁止步骤本来就会超时），所以得先把这个舵机标出来。
   - 做之前先确认：发射机构停在顶部（皮筋松着），滑块还没回零也没关系，这一步不动滑块。
   - “滑块和锁止”组，左摇杆横向第一次推动时，固件才给锁止舵机 1300 µs（旧车的锁死值）的信号，之后摇杆直接调脉宽。
     - 第一次推动的那一下，舵机会从现在的位置转到 1300 µs。如果舵机和舵盘装法都和旧车一样，这就是锁死位，基本不动。如果换了 270° 的 PM 舵机，同样的脉宽对应的角度不一样，幅度不好预测。所以先把摇杆推一点点就放开，看它往哪边转、转多少。
     - 旧车的放开值是 1611 µs，可以先往这个方向找，但以实测为准。
     - 发射机构停在顶部时，夹爪空着，这一下拨到哪里都不会出事。所以这一步要趁发射机构在顶部的时候做。
   - 然后慢慢调脉宽，找出两个值记下来：夹爪完全锁死时的脉宽、刚好放开时的脉宽。两个值之间来回走几次，确认稳定。
   - 把这两个值发给我，我写进 `dart_params.hpp` 并把 `kLockServoCalibrated` 改成 true，重新编译烧录，再做下面几步。
7. 滑块回零、第一次上膛和 `kLatchSlider` 标定。这一步起皮筋会被拉紧，最危险，严格按顺序做。前提是第 6 步的两个脉宽已经写进固件、重新烧录过。
   - 先把锁止机构手动拨到“锁死”，发射机构仍停在顶部，夹爪空着。
   - “滑块和锁止”组，拨轮往上拨到底保持 2 s，滑块回零（往上走，回到顶部）。
   - 回零以后锁止状态是“未知”，手动只能往上走。拨轮往下拨到底保持 2 s，固件跑一次上膛：
     - 先探测：滑块慢慢下到锁止位上方约 30 mm，要十几秒。发射机构在顶部、被滑块托着，电流会升高，结果应该是“已放开”。
     - 然后按“已放开”的流程走：张开夹爪 → 滑块把发射机构压到锁止位下方 3 mm → 锁止 → 锁止确认 → 滑块回 park 位。这一段滑块在压皮筋，电流会到上限，是正常的。
     - 走完以后打印里 latch 变成 1（已锁住），滑块在 park 位。这时皮筋已经拉紧，发射机构靠夹爪卡着。
     - 中途报故障就停下，把两行打印发给我，不要重试。尤其是“探测不出发射机构状态”和“发射机构没有锁住”。
   - 状态变成“已锁住”以后，才可以手动往下点动滑块。慢慢往下，到刚好碰到发射机构为止，记下这时的滑块坐标，这就是 `kLatchSlider`（现在按旧 CAD 算的是 −0.317 m）。
     - 判断“刚好碰到”：一边往下点动一边看电流，电流明显上去的那一刻就是。别再往下压，继续压会顶夹爪。
   - `kLatchSlider` 现在用的是旧 CAD 的估计值。上面这次上膛就是按它算的位置去压的，所以压到的深度可能和 3 mm 有偏差。标出真值以后我会改掉，再做阶段 5 的联调。
8. 把测到的值发给我：
   - 各轴每 mm / 每度的计数和方向
   - 第 6 步的锁止舵机锁死脉宽、解锁脉宽
   - 第 7 步的 `kLatchSlider`
   - pitch 丝杆会不会被皮筋拉着倒转

   换弹机构装好以后再做阶段 3b（摆臂编码器零点、推杆角度、电磁铁），那时 `kLoaderInstalled` 改成 true。

## 阶段 4：标定值存 Flash，加终端命令
阶段 3 的数据回来之后，Claude 会先补全这一节，然后再执行。
- `database`、`terminal` 都是 `XR_REGISTER` 的名字，和阶段 2 一样作为构造参数传进来。
- 新文件 `src/hw/calibration.hpp/.cpp`（LibXR 层）：用 `database` 存标定值。开机时读出来，读不到就用 `dart_params.hpp` 的初值。要存的值：
  - 各轴的回零偏置、摆臂的 `kArmZeroCounts`
  - 锁止位 `kLatchSlider`、装填位、park 位
  - 摆臂的各个角度、推杆的各个角度、锁止角
  - 锁止确认的电流阈值、出膛等待时间
- 新文件 `src/hw/dart_commands.hpp/.cpp`（LibXR 层）：在 `terminal` 里注册这些命令：
  - `dart status`
  - `dart cal list`、`dart cal set <名字> <值>`、`dart cal save`
  - `dart inventory full`、`dart inventory clear`
  - `dart auto on`、`dart auto off`：只改内存里的开关，重启后回到 `kAutoEnabled` 的值
- 验收：编译 0 错误，主机测试全部通过。

## 阶段 5【用户】：联调
按顺序做，上一步正常了再做下一步：
1. 不装皮筋、不装镖，空跑上膛、装填、取镖、发射流程。
2. 装上皮筋，不装镖，空跑一遍。
3. 装镖，单发。
4. 连发。
5. 接上裁判系统，试比赛模式。

每一步都对照“不能发射的条件”逐条确认。

## 卡住就停
遇到下面任何一种情况就停下，把原始输出写进汇报，不要自己想办法绕过去：
- 需要联网的命令失败：git clone、子模块、`xrobot setup` 拉远程索引。
- git 提交后核对不上（沙箱吞了 ref）。
- 需要改 `.ioc`、`Core\`、`Drivers\`、`Middlewares\`、别人的模块，或者计划里没列出的文件。
- QDU 模块的接口和计划描述的不一样，而且计划里没写处理办法，换方案又会改变安全规则（阶段 2 第 1 步写了怎么判断）。
- 编译错误修了两轮还没好。
- 主机测试失败，而且原因是计划本身前后矛盾。

## 回滚
- 阶段 1：Codex 不删 `xrobot-local\DartLauncher`。要不要删由用户决定。
- 阶段 2：
  - `xrobot-local\index.yaml` 从 `.backup\dart-fw\index.yaml` 复制回去。
  - dart-fw 的改动都在 dart-launcher 分支上。`git -C dart-fw switch master` 就回到上游的状态，不删分支。
- 不做 `git reset --hard`，不删分支。需要的话由用户决定。

## 汇报（写进 codex-report.md）
命令里的 `-o codex-report.md` 会在最后用你的最后一条回复覆盖这个文件，你中途写进去的内容会丢。所以下面这些都直接写在最后一条回复里，不要只写“见汇报”。
- 改了哪些文件，每个文件管什么
- 运行了哪些命令，关键输出是什么：主机测试的通过数、编译结果、`arm-none-eabi-size` 的输出
- 阶段 2 读到的接口摘要
- 和计划不一样的地方、遇到的问题、需要用户决定的事
