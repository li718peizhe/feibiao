# RoboMaster 飞镖发射系统（feibiao）

本仓库为 RoboMaster 飞镖发射系统的全量工程归档，整合 **STM32 下位机固件工程**、**MuJoCo 仿真模型与控制台**、**日常开发计划与定稿文档**、**项目交接总结** 以及 **实车调试与问题排查日志**。

---

## 1. 硬件与系统架构

- **主控与软件框架**：DJI RoboMaster C 板（`STM32F407`），基于 **LibXR / XRobot 1.0** 构建；`dart-fw/Modules/local/DartLauncher/src/` 为 C++17 纯逻辑层（可脱离硬件在主机运行全量单元测试），`src/hw/` 接入 LibXR 硬件抽象，固件沿用 BSP 的 C++20 构建配置。
- **控制频率**：1 kHz 反馈与轴控制线程，每 10 拍更新遥控器映射与发射流程状态机，支持 USB CDC 状态日志监控（`OnMonitor()`）。

### 执行器与传感器清单

| 模块 / 设备 | 型号 / 规格 | 总线 / 通道与 ID | 当前装配状态 & 说明 |
|---|---|---|---|
| **滑块（左右）** | 双 M3508（19:1，S3M 40 齿带轮） | `can1`，右 `0x201`（`reverse=true`）、左 `0x202` | **已启用**（`kSliderInstalled=true`），坐标向上为正 |
| **Pitch 俯仰** | M3508 丝杠（导程 2 mm） | `can1`，ID 3 / `0x203` | **已启用**（`kPitchInstalled=true`），上下软限位留 5 mm 余量 |
| **Yaw 偏航** | M2006 丝杠（导程 4 mm） | `can2`，C610 ID 2 / `0x202` | **已启用**（`kYawInstalled=true`），双端寻端自动回中（4j 实测行程 `span=43.375rev`） |
| **锁止舵机** | 众灵 PM60S（270°） | `pwm_tim8_ch2`（50 Hz） | **已标定启用**（`kLockServoCalibrated=true`），最大锁止 `1469 µs` / 最大张开 `2166 µs` |
| **电磁铁** | CNMYE1-P25/29K + SRD-03VDC-SL-C 继电器 | `pwm_tim1_ch3`（50 Hz，低电平吸合） | **已启用**（`kMagnetInstalled=true`），断电吸持、通电消磁（单次最长 3 s） |
| **换弹摆臂 / 推杆** | GM6020（`can2` `0x205`）/ TD-8130MG | `can2` / `pwm_tim8_ch3` | **当前临时关闭**（4i：`kArmInstalled=false`、`kCrankInstalled=false`，保留既有标定值） |
| **双路拉力传感器** | RS485 Modbus RTU 双站轮询 | `usart1`（9600 bps，主从轮询） | **已启用**（4g），通过 USB CDC 输出 `rs485 L/R raw` 及通信计数 |
| **遥控器** | DR16（DBUS） | `usart3`（100000 bps，偶校验） | 使用 `DbusFramer` 按 3 ms 线路空闲分帧 |
| **裁判系统** | RoboMaster 裁判系统串口 | `usart6`（115200 bps） | 解析 `0x0001`（比赛阶段）、`0x0105`（飞镖数据）、`0x020A`（闸门与发射指令） |

---

## 2. 核心特性与安全机制

- **全流程状态机**：覆盖各轴回零、状态未知探测（`Probe`）、慢压找底（`FindBottom`）、锁止/上膛（`Cock`）、退膛（`Decock`）、单发/两发自动连发（`Salvo`）以及接裁判系统后的比赛自动接管（`MatchFlow`，含 `NotStarted` / `Preparing` / `SelfCheck` / `Countdown` / `InMatch` / `Settlement` 阶段门控与急停锁存）。
- **`SliderMustHold` 托持防走火红线**：当滑块已回零、锁止状态非“已锁住”、且滑块低于停靠位 10 mm 以上（门槛 `356 mm`）时，说明滑块正托着未锁住的发射机构。无论发生故障、取消、切无力或遥控器离线，滑块均必须原地闭环保持位置、绝不松力（`kSliderCurrentCap` 内维持），避免皮筋弹起走火。
- **4g RS485 双路拉力读数**：在纯逻辑层实现 Modbus RTU CRC 与帧解析，`usart1` 9600 bps 轮询左右两路拉力传感器并输出至 USB 监控日志（`rs485 L/R raw`、超时/异常/错误计数），不依赖外部标定即可结合 3508 电流做联合推力与一致性核对。
- **4j Yaw 双端寻端回中**：M2006 按 `848` 限流、`7200 rpm` 依次负向寻端 → 退离 `0.5 rev` → 正向寻端 → 退离 `0.5 rev` → 以 `6000 rpm`（`kYawPos.kp=0.06`）无超调回到实测行程算术中点，并接入 `AutoHomeStart` 自动准备序列。
- **4k 手动 `R=M, L=U` 拨轮上 2s 联动滑块归零**：在手动瞄准组（`R=M, L=U`）将拨轮正向推到底保持 2 s，在归零 `pitch` + `yaw` 的同时一并启动两侧 `3508` 滑块归零；`R=M, L=M` 仍保留单独滑块归零。若滑块正处于 `SliderMustHold` 托持态，则仅拒绝滑块回零并原地保持，`pitch`/`yaw` 照常归零。

---

## 3. 遥控器操作速查

| 模式 / 档位 | 操作方式 | 功能说明 |
|---|---|---|
| **无力模式** | 右拨杆 = **下（D）** | 流程计时暂停；舵机保持当前估计角，电磁铁断电吸持 |
| **故障复位** | 右拨杆在下保持 **≥ 1 s** → 拨到 **中（M）** | 取消流程、清锁存故障与失败轴并重设滑块同步差基准；托持态滑块继续保持 |
| **手动点动** | 右拨杆 = **中（M）** | 左拨杆 **上（U）**：yaw / pitch；**中（M）**：滑块 / 锁止；**下（D）**：摆臂 / 推杆（当前暂关） |
| **联动回零（4k）** | `R=M, L=U`，拨轮 **往上到底保持 2 s** | 同时启动 `pitch`、`yaw` 与两侧 `3508` 滑块回零（托持态仅拒绝滑块并保持） |
| **单独滑块回零** | `R=M, L=M`，拨轮 **往上到底保持 2 s** | 仅启动两侧 `3508` 滑块回零（托持态拒绝并保持） |
| **上膛 / 退膛** | `R=M, L=M`，拨轮 **往下到底保持 2 s** | 未锁住时执行上膛（探测/慢压找底/锁止/回顶）；已锁住时执行退膛（慢压接住/开锁/回顶），须全部已装轴先回零 |
| **自动 / 比赛模式** | 右拨杆 = **上（U）** | 左拨杆 **中→上** 单发；**中→下** 启动自动准备回零与连发；比赛中切 **R=D** 触发急停锁存 |

> **安全警告（托持不断电）**：若串口日志显示滑块正在托着未锁住的发射机构（未回到顶部停靠位且 `latch` 非已锁住），切无力（`R=D`）时滑块会原地闭环保持防走火——**此时切勿直接断电**，请先故障复位并安全退膛或回顶后再断电。

---

## 4. 快速上手

### 主机单元测试（C++17，PowerShell）

```powershell
cd dart-fw/Modules/local/DartLauncher
$env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
$src = (Get-ChildItem src\*.cpp).FullName
if (-not (Test-Path build-host)) { New-Item -ItemType Directory -Path build-host | Out-Null }
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
.\build-host\host_test.exe
```

### 固件编译与 OpenOCD 烧录

```powershell
# 1. CMake Debug 构建（生成 build/debug/DevC.elf）
cd dart-fw
cmake --build --preset debug

# 2. OpenOCD 烧录（烧录前确认滑块未处于托持态、发射座无镖、手离开夹爪）
& "C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program build/debug/DevC.elf verify reset exit"
```

### MuJoCo 仿真入口

```powershell
# 从 CAD 导出生成模型（首次运行或修改几何参数后）
python simulation/dart.py build

# 启动中文控制台 + 三维视图（加 --darts 放上飞镖）
python simulation/dart.py console --darts

# 无界面运行完整四发流程并打印每发结果
python simulation/dart.py demo --darts
```

---

## 5. 仓库目录与文档导航

- **[`dart-fw/`](dart-fw/)**：STM32F407 下位机固件工程（基于 LibXR / XRobot）
  - [`dart-fw/Modules/local/DartLauncher/`](dart-fw/Modules/local/DartLauncher/)：飞镖发射核心模块源码、主机单元测试及详细硬件/流程文档 [`README.md`](dart-fw/Modules/local/DartLauncher/README.md)
  - [`dart-fw/Modules/QDU-Robomaster/Referee/`](dart-fw/Modules/QDU-Robomaster/Referee/)：含本地 3 处补丁的裁判系统模块
  - [`dart-fw/User/RobotConfig/artisans_dart.yaml`](dart-fw/User/RobotConfig/artisans_dart.yaml)：整车外设与 CAN/UART/PWM 引脚配置
- **[`xrobot-local/`](xrobot-local/)**：本地 XRobot 模块仓库基线备份（`DartLauncher/`、`ATSRuntime/`）
- **[`simulation/`](simulation/)**：MuJoCo 飞镖发射与装填仿真（[`dart.py`](simulation/dart.py)、`dart_sim/`、`test_dart_sim.py`、`test_dart_console.py`）
- **[`plans/`](plans/)**：日常开发计划、各阶段任务方案与定稿文档
  - [`plans/tasks/`](plans/tasks/)：`2h` 至 `4l` 各阶段任务方案与定稿记录（含 `4g` RS485、`4j` yaw 双端回中、`4k` 联动滑块归零、`4l` 仓库整理同步）及 `diagnostics/` 诊断数据
  - [`plans/rules-2027-dart.md`](plans/rules-2027-dart.md)、[`plans/vision-protocol-v1.0-20261031.md`](plans/vision-protocol-v1.0-20261031.md)
- **[`handoff.md`](handoff.md)**：项目交接总览、当前状态、关键安全规则与实车标定参数记录
- **问题触发与实车日志排查记录**：
  - [`飞镖日志汇总_21-05至21-29.md`](飞镖日志汇总_21-05至21-29.md)
  - [`DartLauncher_arm_filtered.txt`](DartLauncher_arm_filtered.txt)、[`DartLauncher_arm_release_012751-013022.md`](DartLauncher_arm_release_012751-013022.md)、[`DartLauncher_calib_0401-0403.md`](DartLauncher_calib_0401-0403.md)、[`DartLauncher_release_015526-015551.md`](DartLauncher_release_015526-015551.md)、[`DartLauncher_right_release_013745-013835.md`](DartLauncher_right_release_013745-013835.md)、[`DartLauncher_standby_001126-001145.md`](DartLauncher_standby_001126-001145.md)
- **[`SYNC_MANIFEST.md`](SYNC_MANIFEST.md)**：本地 30 个嵌套 Git 仓库快照、排除项与同步边界清单
- **远端既有文档**：[`下位机开发协议文档.md`](下位机开发协议文档.md)、`遥控器控制左中右下控制皮筋（未进行高强度测试）/`
