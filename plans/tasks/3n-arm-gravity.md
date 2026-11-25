# 任务 3n：摆臂重力补偿与换弹推杆标定

2026-11-12 Claude 写，交给 GPT/Codex 在新对话里执行。不要读 `plans/dart-fw-libxr.md`，不要运行 graphify，不要运行仿真（`python -m dart_sim.*`）。

## 背景

### 为什么推满后往回掉

摆臂旋转轴倾斜 37°（CAD 的「旋转_joint」轴 = (0, −sin37°, cos37°)），装填位 0° 是重力最低点，往两侧转是往上爬：

- 仿真静态测量：±50° 保持力矩约 0.17 N·m（空载）/ 0.31 N·m（带镖）。
- 现在堵转保护限到 0.85 A，对应 0.85 × 0.741 ≈ 0.63 N·m，大于空载重力矩但不到带镖重力矩。

推满后下掉的直接原因：

1. **推满时位置接近软限位，速度目标被夹成 0（`kSoftLimitApproachGain × 0`）→ `loop_.Reset()` 清速度环 → 电流降 → 重力矩把摆臂往下拉**。这是当前"推到最高处后下掉"的主因。

2. **松手保持清积分**：`HoldPosition()` 若转速非零（摇杆回弹瞬间）就清积分而非保留，切换瞬间失去支撑力。`hold_integral_rpm=2` 是保留积分的阈值，摇杆弹回时 rpm 可能短暂超过 2。

3. **位置保持 kp 过低**：`kArmPos.kp=0.05`，5° 误差只给出约 0.26° 单圈的位置命令，折算到速度环后电流不够抵消重力矩，会向着重力方向慢漂。

### 换弹推杆当前状态

- `kCrankServoCalibrated=false`，推杆未标定，`kReloadInstalled=false`，自动换弹关闭。
- 推杆舵机接在 TIM8_CH3（PWM 第 7 列 C7），手动可用原始脉宽调（左拨杆下、右摇杆纵向，按 `kCrankPulseRate=200 µs/s`，第一次推先到中位 1500 µs）。
- 标定需要量三个位置的脉宽：收起（`kCrankRetracted`）、取镖（`kCrankFetch`，推杆顶端正对镖座中心）、放镖（`kCrankSeat`，推杆把镖推进磁铁下方）；以及方向（脉宽增大时推杆是伸出还是收起）和偏置（偏置 = 收起对应的 rad，通常 0 或 `kCrankFetch` 附近）。

## 成功标准

1. 摇杆推满：摆臂走到软限位附近（约 ±45°~50°）停住，不碰机构。
2. 推满后摇杆慢慢回中：摆臂 5 秒内角度变化不超过 1°（包括大角度侧，左右都测）。
3. 松手后不往中间回超过 1°（非重力方向也一样）。
4. 推杆三个位置脉宽量出来，`kCrankServoCalibrated=true`，`kReloadInstalled=true`，主机测试通过，固件编译通过。
5. 单次取镖（取镖六步流程）主机测试能跑完。

## 第 1 步：预检

```powershell
git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher log --oneline -3
git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher status --porcelain
git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw log --oneline -3
git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw status --porcelain
```

应该是 DartLauncher HEAD `a438756`（`摆臂堵转限流提高到 0.85 A`）、dart-fw HEAD `d2fb146`；两仓库干净。与上面不一致时在汇报里写明，按实际继续。Referee 模块的三处本地改动（`Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp`）保留不动。

## 第 2 步：摆臂重力补偿——前馈 + 位置保持调整

### 2.1 仿真数据

仿真（旧 CAD，仅供量级参考，不用于调参决策）给出：

- 重力矩系数：空载 k₀ ≈ 0.224 N·m/sinθ，带镖 k₁ ≈ 0.407 N·m/sinθ（θ 以装填位 0° 算）。
- ±50° 保持力矩：空载 0.17 N·m，带镖 0.31 N·m。

实车摆臂质量和磁铁+镖质量未精确量，前馈系数需要现场标定（见第 4 步）。

### 2.2 新增参数（`src/dart_params.hpp`）

在 `kArmPos` 附近加：

```cpp
// 摆臂重力前馈：每拍按 sin(arm_angle) 叠加一个固定电流，补偿倾斜轴的重力矩。
// 系数 kArmGravityFeedforward：正值表示往正方向（右）推时需要的补偿。
// 单位：电调原始值 / rad（除以 sinθ 后的斜率）。
// 初始值按仿真空载估算：0.224 N·m / (0.741 N·m/A) / (3 A / 16384) ≈ 1649。
// 实车标定：无力时手转摆臂到 ±30°，看串口 cmd 的稳态值，调到 stall=0、cmd≈act。
constexpr int kArmGravityFeedforward = 1649;

// 位置保持 kp：0.05 在大角度时不够对抗重力矩，提高到 0.3。
// 位置环输出是速度环的目标 rpm；0.3 在 5° 误差时给出约 1.5 rpm 的速度命令，速度环积分来补足。
// 注意 kArmPos.max_out=60，对应约 ±60 rpm，够摆臂稳定。
```

修改 `kArmPos`：`{0.05, 0, 0, 60, 0}` → `{0.3, 0, 0, 60, 0}`。在上面加注释说原因。

### 2.3 前馈在适配层（`src/hw/launcher_hw.cpp`）

在 `arm_.Gate(...)` 之后、`arm_stall_.Update` 之前加前馈叠加（前馈不经堵转保护，保证始终有补偿）：

```cpp
if constexpr (kArmInstalled) {
    // 重力前馈：叠加到 Gate 算出的指令上，再送进堵转保护。
    // 在软限位处 LimitAtSoftLimits 还会把向外的电流清零，前馈不会推过限位。
    const int feedforward = static_cast<int>(
        std::sin(arm_.Axis().Position()) * kArmGravityFeedforward);
    const int with_ff = std::clamp(arm_.Current() + feedforward,
                                   -kArmCurrentCap, kArmCurrentCap);
    const bool was_tripped = arm_stall_.Tripped();
    arm_.SetCommand(arm_.Axis().LimitAtSoftLimits(
        arm_stall_.Update(with_ff, arm_.FeedbackRpm(), dt)));
    if (arm_stall_.Tripped() && !was_tripped) {
        XR_LOG_WARN("摆臂堵转，电流限到 %.2f A", kArmStallCurrent * 3.0 / 16384);
    }
    if (!arm_stall_.Tripped() && was_tripped) { XR_LOG_INFO("摆臂电流限制解除"); }
}
```

把原来的堵转保护块替换成上面这段。`std::clamp` 在 `<algorithm>` 里，launcher_hw.cpp 已经 include 了它。

### 2.4 `hold_integral_rpm` 放宽

`ArmConfig()` 里 `config.hold_integral_rpm = kArmStallRpm;`（2 rpm）→ 改成 10 rpm：

```cpp
config.hold_integral_rpm = 10; // 摇杆回弹瞬间转速短暂超过 2 rpm，不清积分；仍要低速才保留。
```

这让摇杆回弹瞬间松手也能保住积分，不会瞬间失去支撑力。

### 2.5 `test/host_test.cpp` 检查

新增一条 Check：

```cpp
Check(kArmGravityFeedforward > 0 && kArmGravityFeedforward <= kArmCurrentCap,
      "arm gravity feedforward is positive and within the current cap");
Check(kArmPos.kp >= 0.1,
      "arm position kp is raised to hold against gravity at large angles (2026-11-12)");
```

原有摆臂参数检查不删，只改受影响的那条（kp 下限）。

### 2.6 README 更新

README 摆臂段加：重力前馈参数 `kArmGravityFeedforward` 的含义和现场标定方法（见第 4 步）；位置 kp 提高到 0.3 的原因。

## 第 3 步：推杆舵机标定

推杆舵机要单独降压供电（6~7.4 V），地和 C 板连在一起。标定前确认舵机已接好且有独立电源。

### 3.1 标定流程（操作手用遥控器完成）

**上电后不要切手动**，先切无力（右拨杆下），确认摆臂不会乱动。再切手动装弹组（右拨杆中、左拨杆下）。

推杆舵机第一次收到摇杆信号时先到 1500 µs（中位），之后按右摇杆纵向调脉宽（每秒最多 200 µs）：

1. **找收起位**（推杆完全缩回，不顶任何东西）：先往一个方向推摇杆，看推杆朝哪边动。如果缩回是摇杆往上推，就一直往上推，直到推杆缩到最短、不再动。停在这里，记下串口 `Dart arm=... crank=...[R] ...` 里的 `crank=` 脉宽值，记为 P_retracted。
2. **找取镖位**（推杆顶端正对右镖座的镖中心，从上往下能把镖套进来）：先把摆臂摆到右镖座位置（左摇杆横向推到右边，停在 +46.2° 附近）。再调推杆脉宽，让推杆顶端对准镖座中心——推杆顶端要能从上面放下去套住镖，但还没贴住磁铁。记下脉宽，记为 P_fetch。
3. **找放镖位**（推杆把镖推进磁铁正下方，可以吸住）：在取镖位基础上继续推推杆，让镖贴到磁铁下端。记下脉宽，记为 P_seat。

如果推杆在某侧顶住了（脉宽不再变、舵机开始发热），立刻回到中位，不要继续往外调。

把三个脉宽值发回来。

### 3.2 回填参数（拿到脉宽后 GPT 执行）

拿到 P_retracted、P_fetch、P_seat 以后，计算偏置和方向。

舵机的角度 = offset + direction × pulse_normalized，其中 pulse_normalized ∈ [0, kServoRange]。
pulse_normalized = (pulse_us - kServoPulseMin) / (kServoPulseMax - kServoPulseMin) × kServoRange。

选收起 = 0 rad，则：

```
kCrankRetracted = 0
kCrankServoOffset = (P_retracted - kServoPulseMin) / (kServoPulseMax - kServoPulseMin) * kServoRange
```

方向：脉宽从 P_retracted 增大时推杆是伸出还是收起？
- 若 P_fetch > P_retracted：伸出对应脉宽增大，`kCrankServoDirection = 1`，`kCrankFetch = (P_fetch - P_retracted) / (kServoPulseMax - kServoPulseMin) * kServoRange`。
- 若 P_fetch < P_retracted：伸出对应脉宽减小，`kCrankServoDirection = -1`，`kCrankFetch = (P_retracted - P_fetch) / (kServoPulseMax - kServoPulseMin) * kServoRange`。

同理算 `kCrankSeat`。

在 `src/dart_params.hpp` 里修改这四个常量：`kCrankRetracted`、`kCrankServoOffset`、`kCrankServoDirection`、`kCrankFetch`、`kCrankSeat`，把计算结果和三个实测脉宽写进注释（带日期）。

然后改 `kCrankServoCalibrated = true`。`kReloadInstalled` 由它自动变成 `true`。

### 3.3 更新检查（`test/host_test.cpp`）

原有：
```
Check(kCrankInstalled && !kCrankServoCalibrated, "crank servo is on the bench in raw pulse mode since 2026-10-28");
Check(!kCrankServoCalibrated, "crank servo stays in raw pulse mode until its angles are calibrated");
Check(kMagnetInstalled && !kReloadInstalled, "with the magnet enabled ...");
```

改成：
```
Check(kCrankInstalled && kCrankServoCalibrated, "crank servo is calibrated (2026-11-12)");
Check(kReloadInstalled, "automatic reload is installed after crank servo calibration (2026-11-12)");
Check(kCrankFetch > kCrankRetracted, "crank fetch angle is past the retracted position");
Check(kCrankSeat > kCrankRetracted && kCrankSeat <= kCrankFetch, "crank seat is between retracted and fetch");
Check(kCrankServoOffset >= 0 && kCrankServoOffset < kServoRange, "crank servo offset is in servo range");
```

另外把原来保证 `!kReloadInstalled` 的那条检查删掉，改成 `kReloadInstalled`。

用于主机测试的 `TestCrankSettle()` 里有 `io.state.crank = kCrankSeat`，本任务不改流程逻辑，检查应该继续通过。如果某条旧检查因参数变化失败，说明是参数设错了，不要为了通过就改检查，停下汇报。

## 第 4 步：主机测试、编译、提交、烧录

```powershell
$env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
New-Item -ItemType Directory -Force build-host | Out-Null
$src = (Get-ChildItem src\*.cpp -Recurse | Where-Object { $_.DirectoryName -notmatch "hw$" -or $_ -match "\.cpp$" }).FullName
# 包含 hw 子目录：
$src = (Get-ChildItem src -Recurse -Filter *.cpp).FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
.\build-host\host_test.exe
(Select-String -Path test\host_test.cpp -Pattern 'Check\(').Count
Select-String -Path src\*.hpp, src\*.cpp, src\hw\*.hpp, src\hw\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)'
git add src test README.md
git status --short
git commit -m "摆臂重力前馈、位置kp提高，推杆标定后打开换弹"
git rev-parse --short HEAD
git branch --show-current
git status --porcelain
Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
xrobot setup --update local/DartLauncher
git -C Modules\local\DartLauncher log --oneline -1
xrobot gen -c User/RobotConfig/artisans_dart.yaml
cmake --build --preset debug
git add xrobot.lock
git commit -m "更新 DartLauncher（摆臂重力前馈和换弹打开）"
git rev-parse --short HEAD
git branch --show-current
git status --porcelain
```

每步都要打印 `PASS`，否则停下汇报，不提交。`Select-String` 那行应该没有任何输出（纯逻辑不能引用 LibXR/HAL）。

烧录命令给用户在自己的 PowerShell 里运行，烧录会复位，电机会松开，手离开机构：

```powershell
& "C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset exit"
```

`** Verified OK **` 才算烧上。

## 第 5 步：现场标定 kArmGravityFeedforward

烧上以后，让用户：

1. 右拨杆中、左拨杆下（装弹组）。
2. 把摆臂摇到 +30° 附近，摇杆回到 0（松手保持），等 3 秒稳住。
3. 看串口 `Dart arm cmd=... act=... stall=...`：
   - 如果 `stall=0` 且 `|cmd|` 比 `kArmStallCurrent`（4642）小不少（比如小于 3000），前馈够用，记下 cmd 的稳态值。
   - 如果 `stall=1`，说明前馈不足或位置kp输出加上前馈超过堵转保护，停下汇报。
   - 如果摆臂慢慢往下漂超过 1°/5s，说明前馈不足，把 `kArmGravityFeedforward` 加 300，重新编译烧录，重测。
4. 在 −30° 重复，`cmd` 应该是负数且量级相近（允差 ±30%）。

调到两侧都稳住、`stall=0`、漂移不超过 1°/5s，把最终的 `kArmGravityFeedforward` 写进提交注释和 handoff.md。

## 第 6 步：换弹联调（取镖单次）

推杆标定完成后，做第一个换弹动作：**单次取镖**。

条件：右镖座里放一根镖（平放进去，测试时不带电磁铁）。

操作：
1. 右拨杆中、左拨杆中（滑块组），拨轮正向保持 2 s，滑块回零。
2. 切到装弹组（左拨杆下），摆臂已经回零（上电自动按编码器建零点）。
3. 切到自动模式（右拨杆上），左拨杆从中拨到下——触发连发，会自动取镖+装填+上膛+发射。
   - 因为发射危险，不做完整连发；只看"取镖"六步：
     - 推杆收起 → 摆到右镖座 → 推杆下探 → 断电吸持（取到镖）→ 推杆抬起 → 摆到待命位。
   - 在"取镖：磁铁断电吸持"这一步停下（切无力）。
4. 看串口 `step=取镖：摆到待命位` 或 `step=取镖：磁铁断电吸持`；日志里有没有故障（`fault≠无故障`）。
5. 把操作过程发给 DeepSeek，用文末模板提问，把结果贴回来。

取镖成功后再做完整单次：取镖 → 装填 → 上膛 → 发射。把镖放进右镖座，全程日志用 DeepSeek 分析。

## 停止条件

- 摆臂在 ±30° 以内松手仍漂移超过 1°/5s（前馈超过 kArmCurrentCap 时停下，不再加）。
- 松手后摆臂往 0° 方向回超过 1°（前馈方向错了或过大；改小方向或系数）。
- 取镖时出现 `Timeout`（推杆到位超时）、`NoDarts`（库存错误）、`ArmInPath`（镖挡住发射路径）以外的故障：停下汇报。
- 推杆顶住机构或者拉坏线缆：立刻无力，停下汇报。
- 摆臂碰到机构，`tmp=` 到 60，电机离线或故障反复出现。

## 第 7 步：收尾

更新 `handoff.md`：

- 当前状态里加"3n 重力前馈和换弹联调"的结果。
- 仓库与产物表：HEAD、编译时间、SHA-256。
- 操作速查"摆臂"段加前馈参数和现场标定值。

README 加：
- 重力前馈 `kArmGravityFeedforward` 的含义和最终标定值。
- 推杆三个标定角度和对应脉宽。
- 换弹首次联调的结果。

本文件末尾写执行进度。

汇报里写：改了哪些文件、主机测试 Check 数改前和改后、两个仓库各提交的哈希、前馈参数最终值、推杆三个脉宽和算出的参数、取镖试验结果，以及还需要现场确认的项。

## 附：推杆标定和取镖的 DeepSeek 提问模板

```
下面是飞镖发射架 C 板的串口日志，行首 [hh:mm:ss] << 是工具加的，可能拆行，请先拼回整行。
- "Dart mode=… fault=…"：mode 1 手动、2 自动；fault 正常是"无故障"。
- "Dart step=…"：当前流程步骤名。
- "Dart arm=… crank=…[R] mag=…"：摆臂角度（度），推杆脉宽（µs，[R] 表示未标定原始模式），电磁铁通断。
- "Dart arm cmd=… stall=…"：电流指令和堵转保护状态（0/1）。
请只回答：
1. mode 和 fault；有故障引用原文。
2. step 经历了哪些步骤，最后停在哪一步。
3. crank 脉宽从多少变到多少，共几次变化。
4. mag 有没有变成 1；变成 1 时 step 是什么。
5. 有没有出现事件行（非 "Dart" 开头的中文行）；有就原样引用。
```

## 执行进度

### 2026-11-12 Codex 预检：按失败停止条件暂停

执行结果：未修改功能代码，未同步模块、未构建新固件、未提交、未烧录；只更新本执行记录及根目录 `handoff.md`。用户确认推杆三个脉宽“未测”，`kCrankServoCalibrated=false`、`kReloadInstalled=false` 保持原值。

步骤状态：

1. 预检与基线核实已完成，发现既有测试失败。
2. 重力补偿、kp 和积分保留阈值修改因停止条件未执行。
3. 推杆回填、固件同步构建、提交和现场验收未执行。
4. 失败记录和交接已完成。

仓库核对：

- DartLauncher：分支 `main`，HEAD `a438756`（`摆臂堵转限流提高到 0.85 A`）。已有 6 个未提交文件：`src/hw/launcher_hw.cpp`、`src/motor_axis.cpp`、`src/motor_axis.hpp`、`src/pid.cpp`、`src/pid.hpp`、`test/host_test.cpp`，全部保留。
- dart-fw：分支 `dart-launcher`，HEAD `d2fb146`（`更新 DartLauncher 堵转限流至 0.85 A`），工作区干净；模块副本工作区干净。
- Referee 独立仓库的 `Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp` 三处本地修改保留。

验证结果：

- 首次沙箱内调用 `g++` 失败：`The term 'g++' is not recognized`；经批准使用现有编译器的绝对路径重试。
- 本任务第 4 步的递归源文件命令失败：`g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $taskSources -o build-host/host_test.exe`，其中 `$taskSources = (Get-ChildItem src -Recurse -Filter *.cpp).FullName`。它包含依赖 LibXR 的 `src/hw`，报 `fatal error: RMMotor.hpp: No such file or directory`，另缺 `pwm.hpp`、`Referee.hpp`、`uart.hpp`；README 第 354 行规定主机测试只编译 `src` 顶层 `.cpp`。
- 按 README 范围诊断基线：`$taskSources = (Get-ChildItem src -Filter *.cpp).FullName`，同一 g++ 编译命令返回 0，打印 `PASS: pure-logic baseline compilation`；随后 `./build-host/host_test.exe` 返回 1：`FAIL: bounded release support holds an idealized load below the stall limit`（`test/host_test.cpp:2461`）。这与 `handoff.md` 已记录的既有失败一致，没有删改检查。
- `Check(` 行数改前/改后均为 **597**；测试提前退出，没有整套通过项数，单次取镖相关测试尚未执行。
- 未运行 `python -m dart_sim.*`，未运行 graphify，未读取 `plans/dart-fw-libxr.md`。

任务与代码核对发现：

- 第 2.3 步在 `arm_.Gate()` 后无条件叠加前馈会重新产生无力状态的电流；后续实现必须检查输出使能、电机在线及轴状态，维持故障和无力的零输出。前馈应进入现有堵转保护及最终软限位检查；该段代码本身也调用了 `arm_stall_.Update(with_ff, ...)`，与“前馈不经堵转保护”的文字冲突。
- 位置环输入为编码器计数（`src/pid.cpp:40`、`src/hw/launcher_hw.cpp:48`）：5° 误差为约 113.78 计数，kp=0.3 时位置环输出约 **34.13 rpm**，第 2.2 步的“1.5 rpm”估算不适用，未据此修改 kp。
- 背景中的 0.63 N·m 大于给出的带镖重力矩 0.31 N·m；重力矩不足的判断不能由这两个数推出。

交付状态：`kArmGravityFeedforward` 尚未新增，1649 仅为任务提议初值；`kArmPos.kp=0.05`、`hold_integral_rpm=kArmStallRpm` 保持原值。推杆脉宽和计算参数、两侧保持漂移、软限位停车及取镖试验均无本次实测结果。两个仓库均无本次新提交。

原有固件保留：`dart-fw/build/debug/DevC.elf`，2026-11-12 12:39:53（Asia/Shanghai），SHA-256 `88A0E5A92E23C8236A88165602A2B85C97C0381E3F7AE0558065380C170C1046`；本次未生成新的可烧录产物。

### 2026-11-12 15:15 起推杆现场采样：机构未动，用户已断电

- 用户要求由 Codex 直接采串口测量；使用已有 Python 3.11 和 serial-monitor 脚本读取 COM46 / 115200，未安装依赖、未发送串口控制命令、未复位或烧录。`.venv` 不含 pyserial，首次枚举报 `ModuleNotFoundError: No module named 'serial'`；改用现有 Python 3.11 后串口可读取。
- 初始 `mode=0 fault=遥控器离线`，`crank=1503[R]`；用户确认推杆完全收起。随后遥控器上线，`mode=0 fault=无故障`。1503 µs 仅为现场外观对应的候选收起输出，尚未通过机构动作验证，不作为有效标定值。
- 按手动装弹组操作，日志显示 `mode=1 fault=无故障`、`R=M L=D`，推杆指令经历 `1503 → 1517 → 1691 → 1700 µs`，随后回到 `1660 µs`；这些数是固件输出指令，未测量 PWM 引脚波形，也不是舵机实际角度反馈。用户反馈“没有动作”，并确认独立 6～7.4 V 电源已接通，伸出方向和三个标定位置均未确认。
- 用户随后说明怀疑初始位置卡住、“掰不动舵机、不发热”，并已先断电；停止进一步通电动作。该描述不能单独证明机械卡死或舵机故障，也不能据此扩大脉宽范围。后续需断电、支撑推杆并解除舵机与连杆连接后，分别检查连杆行程及舵机响应。
- 证据保存在模块忽略目录 `build-host/3n-crank-state-20261112.log`、`build-host/3n-crank-observe-20261112.log`、`build-host/3n-crank-direction-20261112.log`。采集均已结束、串口已关闭；serial-monitor 返回 1，初始日志确有遥控器离线，后续通用分析器也把 `fault=无故障` 匹配为错误，实际状态按原文判断。
- 本次未修改功能代码、未提交；推杆仍未标定，自动换弹仍关闭。

### 2026-11-12 15:21 推杆恢复动作，确认方向

- 用户报告断电重启后恢复动作：左拨杆下、右拨杆中，右摇杆向上带动推杆向下。结合前次串口脉宽上升，确认该操作方向对应脉宽增大、推杆下探；在后续机构角以收起为零、下探为正的标定中，方向应取 `+1`，尚未写入标定结果。
- COM46 五秒新采样显示 `mode=1 fault=无故障`、`R=M L=D`、`crank=1700[R]`，摆臂约 +3.25°、电机温度 25°C；1700 µs 已到原始模式的软件上限，不能据此认定机构到达取镖位或放镖位。用户所说断电重启的具体设备未确认，未假定 C 板发生复位。
- 已请用户只轻推右摇杆向下、完全收起后立即回中并报告到位，Codex 读取脉宽；随后的 45 秒采样中 `crank=1700[R]` 未变化，尚未收到新的收起到位确认，因此未回填收起脉宽或开启自动换弹。
- 新日志：`build-host/3n-crank-restart-20261112.log`、`build-host/3n-crank-retracted-20261112.log`；采集已结束并关闭串口。脚本返回 1 的样例全部为 `fault=无故障`，属于通用分析器匹配 `fault` 字段，原始日志未显示本次故障。
- 仅更新根目录文档，功能代码和 Git HEAD 均未变化；此前主机基线失败仍未解决，未构建、提交或烧录。

### 2026-11-12 15:26 收起位确认

- 用户回复推杆“已收起”后，COM46 连续三组新采样均为 `crank=1325[R]`，`mode=1 fault=无故障`、`R=M L=D`、摇杆均回中。记录 **P_retracted≈1325 µs**（串口按整数微秒显示），替代此前未经动作验证的 1503 µs 候选值；脉宽减小收起、增大下探，方向为 `+1`。
- 原始日志 `build-host/3n-crank-retracted-confirmed-20261112.log`；串口已关闭。采集脚本返回 1，错误样例为 `fault=无故障`，原始日志无本次故障；推杆没有实际位置传感器，到位依据为用户现场确认。
- 下一步：推杆保持收起，在无力状态手动对准右镖座，再确认取镖和放镖位置；尚未回填功能参数或开启自动换弹。

### 2026-11-12 15:36 取镖位受软件上限限制；扩大下探范围并构建

现场结果：

- 用户确认无力手移后已对准右镖座，日志摆臂约 +47～48°；切回手动装弹组，推杆达到 1700 µs 后用户反馈“停止下探但还没到位”，随后说明只推出两三厘米。1700 µs 不是有效取镖位，P_fetch 和 P_seat 仍未取得。
- `build-host/3n-crank-fetch-20261112.log` 显示摆臂在手动模式中从约 +47.59° 到 +43.95° / +44.43°，随后有一拍切无力时到 +28.12°，再切手动稳定在约 +28.04°；用户不确定前段是否自行回落，不能把全段约 20° 下降都归为有力保持漂移。最后短采样为手动、`fault=无故障`、摆臂 +28.12°、推杆 1698 µs、`stall=0`、温度 26°C；未取得有力保持验收通过的结论。
- 用户明确要求先扩大推杆范围，并否定仅提高到 1900 µs。最终只将 `kCrankRawPulseMax` 从 **1700 改为 2500 µs**（舵机额定脉宽上端），下限 **1300 µs**、首次唤醒 **1500 µs**、调节速率 **200 µs/s** 保持；README 同步说明方向、收起位约 1325 µs 和机械到位即回中的操作。没有改变摆臂控制参数或自动换弹开关。

实施与验证步骤均已完成：

1. 已核对并保留原有 6 个未提交摆臂候选文件，在忽略目录从模块已提交 HEAD `a438756` 导出基线；基线主机测试 `PASS: 21596 checks`，`Check(` 570 行。
2. 只加入本次 `dart_params.hpp` 修改后，用同样命令编译、运行隔离基线：`PASS: 21596 checks`，`Check(` 仍为 570 行；本次参数改动没有新增测试，既有脉宽范围检查及整套流程回归通过。工作区原有 597 行检查的候选负载测试失败仍保留，本次没有把这些未提交改动纳入测试版本或固件。
3. 审查暂存区后，仅提交 `src/dart_params.hpp` 和 `README.md`：DartLauncher `bd18d84`，`推杆原始脉宽上限扩大到 2500 微秒`，分支 `main`。
4. `xrobot setup --update local/DartLauncher` 成功，模块副本干净且 HEAD 为 `bd18d84`；`xrobot gen -c User/RobotConfig/artisans_dart.yaml` 成功；`cmake --build --preset debug` 返回 0，`DevC.elf built for product: artisans_dart`。
5. 审查暂存区后，仅提交生成的 `xrobot.lock`：dart-fw `60d63ec`，`更新 DartLauncher 推杆脉宽上限至 2500 微秒`，分支 `dart-launcher`，工作区干净；Referee 三处修改不动。根目录任务记录及交接无 Git 仓库，直接保存。

构建工具限制：`python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --source . --list-presets --no-save-profile` 返回 1，`ModuleNotFoundError: No module named 'tool_config'`；改用现有 `cmake --list-presets` 和 `cmake --build --preset debug`，未安装软件。XRobot 提示安装 1.0.0 与 pin 1.0.1 不同，但同步与生成均返回 0，未升级工具。

产物：`dart-fw/build/debug/DevC.elf`，2026-11-12 **15:35:08**（Asia/Shanghai），SHA-256 **`257200F0BFB95CFDE3668AB5646680D8A5AE6516DD5B8B6F74C6C9F18860CC6B`**；旧固件备份 `dart-fw/build/DevC-a438756-before-crank-2500.elf`，哈希仍为 `88A0E5A92E23C8236A88165602A2B85C97C0381E3F7AE0558065380C170C1046`。

本次未烧录，按本任务要求将烧录命令交给用户执行；尚无扩大范围后的实测脉宽、取镖或放镖结果。重力前馈、位置 kp 和候选摆臂控制仍未落实，本任务整体未验收完成。

### 2026-11-12 17:38 右镖位现场读数

- 用户说明当前位置为右镖位，Codex 通过 COM46 / 115200 清空旧缓存后采集 5 秒，得到 17:38:29～17:38:32 四组完整状态：`mode=1 fault=无故障`、`R=M L=D`、四个摇杆读数均为 0，`step=空闲`。
- 6020：四组 `arm=46.84 raw=3008`、`rpm=0`；电流指令均为 `cmd=4642`（约 0.85 A），实际电流原始读数 `act=4637/4653/4652/4637`，温度 30～31°C，四组均 `stall=1`，即堵转限流已触发。只能说明本次短采样角度读数不变，不能据此认定保持及温升验收通过；已提示不继续加力，切无力前可靠支撑。
- 推杆：四组 `crank=2296[R]` µs，电磁铁 `mag=0`；记录为用户确认右镖位时的现场输出脉宽。用户尚未明确确认该推杆位置已满足套住镖且未贴磁铁的取镖条件，因此暂不把 2296 µs 回填为最终 P_fetch，P_seat 仍未测。与现有参数 `kFetchRight=46.2°` 相比，本次摆臂现场角度大约高 0.64°，未修改参数。
- 原始日志 `xrobot-local/DartLauncher/build-host/3n-right-dart-position-20261112.log`；采集结束且串口已关闭。脚本返回 1，错误样例全为 `fault=无故障`，属于通用分析器对 `fault` 字段的匹配；堵转状态按 `stall=1` 单独判断。
- 运行时已能输出 2296 µs，超过旧 1700 µs 上限，说明板上运行范围已扩大；未取得用户烧录输出或镜像只读校验，不能单由日志确定板上完整提交版本。源码 HEAD 仍为 DartLauncher `bd18d84`、dart-fw `60d63ec`；本次只保存采样及根目录记录，无功能代码修改或新提交。

### 2026-11-12 17:44 左镖位现场读数

- 用户说明当前位置为左镖位，COM46 / 115200 清空缓存后采集 5 秒，17:44:21～17:44:25 四组完整状态均为 `mode=1 fault=无故障`、`R=M L=D`、摇杆回中、`step=空闲`。
- 6020 四组 `arm=-39.11 raw=1052`、`rpm=0`、温度 33°C；指令均为正值 `cmd=4642`（约 +0.85 A），实际电流原始值 `act=4656/4614/4656/4626`，四组 `stall=1`。按原始日志记录正电流，未根据负角度推断或改写电流符号；限流状态下短时间角度不变不代表保持验收通过。
- 推杆四组 `crank=2335[R]` µs，锁止 1500 µs，`mag=0`；记录为用户确认左镖位时的现场输出脉宽，未据此修改 kFetchLeft、kCrankFetch、标定开关或自动换弹开关。
- 证据 `xrobot-local/DartLauncher/build-host/3n-left-dart-position-20261112.log`，串口已关闭；脚本返回 1，错误样例均为 `fault=无故障` 的通用分析器匹配，实际堵转限流依据 `stall=1`。本次只更新根目录文档，无控制代码修改、新构建或提交，原有工作区修改保留。
