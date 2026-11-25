# 任务 4i：临时关闭 GM6020 摆臂与 TD-8130MG 推杆舵机以验证 4f 自动回零

- **日期**：2026-11-24
- **状态**：已完成
- **对应事项**：Orca item 3
- **基线提交**：
  - `xrobot-local/DartLauncher`：分支 `codex/apply-load-position-calibration`，HEAD `3f2eb79e533df297bb6238e37d63561ee67dfce9`（4g RS485 读数接入完成；主机基线 `PASS: 31569 checks` + `PASS: 2193 HX711 checks`）
  - `dart-fw`：分支 `dart-launcher`，HEAD `c154c462d6d591ac570dea47941c9558de344779`；`Modules/modules.yaml` 与 `xrobot.lock` 锁定 `local/DartLauncher@3f2eb79`；当前 `dart-fw/build/debug/DevC.elf` SHA-256 `C31D63EFD3B4312540FD55FBC39B6110168408C7BA2F3963C62C04F72D882B57`（旧 4f 备份 `dart-fw/build/DevC-c154c46-before-4g.elf`）
  - `dart-fw/Modules/QDU-Robomaster/Referee`：3 处本地补丁原样保留（`Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp`）

---

## 1. 背景与问题根因

1. 当前整车换弹机构已长期拆除，GM6020 摆臂电机（CAN2 ID 1 / `0x205`）与 TD-8130MG 推杆舵机（`pwm_tim8_ch3`）均未接入。
2. 现有固件（`3f2eb79`）在 `src/dart_params.hpp` 中仍配置 `kArmInstalled = true`（L247）与 `kCrankInstalled = true`（L251）：
   - `LauncherHardware::RefreshState()`（`src/hw/launcher_hw.cpp` L120–133、L158–160）通过 `kMotorInstalled` 数组要求摆臂电机在线且建零；6020 离线超过 `kMotorOfflineTime = 0.1 s` 即触发 `SafetyFault::MotorOffline`（`fault=电机离线`），导致全车 Safety 停止，pitch 与滑块无法使能、无法进行 4f 自动回零验证。
   - `crank_` 在构造时因 `kCrankInstalled && kCrankServoCalibrated == true` 会初始化并驱动 `pwm_tim8_ch3` 输出收起脉宽。
3. 本任务通过**最小编译期配置变更**临时关闭摆臂与推杆装配开关，解除 6020 离线对全车的阻断，使已安装的 pitch 与滑块可独立开展 **4f 自动回零顺序（瞄准 pitch → 滑块）** 的现场验证；同时严格保持不接入发射、换弹或磁铁流程，保留既有标定参数与 4g RS485 拉力读数。

---

## 2. 临时配置位置与最小改动范围

### 2.1 核心配置改动

| 文件 | 行号 / 符号 | 改前 | 改后 | 原因 |
|---|---|---|---|---|
| `xrobot-local/DartLauncher/src/dart_params.hpp` | L247 `kArmInstalled` | `true` | `false` | 解除 6020 离线检查与建零要求，停止摆臂控制/堵转保护，保持 CAN2 零电流帧发送 |
| `xrobot-local/DartLauncher/src/dart_params.hpp` | L251 `kCrankInstalled` | `true` | `false` | 上电不初始化、不驱动 `pwm_tim8_ch3`，避免空载/拆机通道输出并避免干扰共用 TIM8 的锁止舵机 |

### 2.2 明确保持不变的参数（严禁误改）

| 参数 | 保持值 | 说明 |
|---|---|---|
| `kSliderInstalled`（L241） | `true` | 两侧滑块正常在线、回零与托持保护 |
| `kPitchInstalled`（L242） | `true` | pitch 正常在线、堵转回零与位置保持 |
| `kYawInstalled`（L243） | `false` | 未安装，保持关闭 |
| `kAutoEnabled`（L239） | `true` | 进入 `Mode::Automatic` 触发 4f 自动回零的必要前提 |
| `kCockEnabled`（L240）、`kLockServoCalibrated`（L260） | `true` | 不动既有锁止配置；本轮验收不触发上膛/发射手势 |
| `kMagnetInstalled`（L254） | `true`（默认不动，见第 7 节人工确认） | 最小变更边界：不碰磁铁逻辑；上电保持断电吸持电平，换弹/取镖入口已被关闭 |
| `kCrankServoCalibrated`（L255）、`kLoadCalibrated`（L257）及全部摆臂/推杆/装填标定常量 | 原值保留 | 保留已实测标定参数，后续重装换弹机构时无需重新标定即可恢复 |
| `kReloadInstalled`（L259） | 派生表达式自动求值为 `false` | `kArmInstalled && kCrankInstalled && kMagnetInstalled && kCrankServoCalibrated && kLoadCalibrated`，无需手改表达式 |

### 2.3 同步改动清单（允许修改的全部文件）

1. **`xrobot-local/DartLauncher/src/dart_params.hpp`**（必改）：
   - L247 `kArmInstalled = false`（补充简短中文注释说明临时拆除换弹机构关闭）；
   - L251 `kCrankInstalled = false`（补充简短中文注释说明临时关闭，保留 3q 标定脉宽）。
2. **`xrobot-local/DartLauncher/test/host_test.cpp`**（必改，仅更新 4 处编译期开关断言，不删减任何算法/几何/安全/4f 回归测试）：
   - L224：`Check(kArmInstalled, ...)` → `Check(!kArmInstalled, "arm GM6020 is temporarily disabled while the reload mechanism is removed");`
   - L254：`Check(kCrankInstalled && kCrankServoCalibrated, ...)` → `Check(!kCrankInstalled && kCrankServoCalibrated, "crank servo is temporarily disabled while keeping task 3q calibration pulses");`
   - L259：`Check(kReloadInstalled, ...)` → `Check(!kReloadInstalled, "automatic reload stays off while arm and crank are disabled");`
   - L285：`Check(kMagnetInstalled && kLoadCalibrated && kReloadInstalled, ...)` → `Check(kMagnetInstalled && kLoadCalibrated && !kReloadInstalled, "load calibration parameters stay intact while automatic reload is disabled");`
   - 保留 L225–253（摆臂软限位/电流上限/堵转温降）、L268–284（推杆 3q/3v 脉宽换算）、L4652 `TestAutomaticHomeStartRegression()` 等全部测试不变。
3. **`xrobot-local/DartLauncher/README.md`**（必改）：
   - 更新“实车分阶段装配”表中 `kArmInstalled`、`kCrankInstalled` 当前值为 `false`（L43–44），同步更新 L48（当前在线与全部回零仅要求两侧滑块和 pitch）、L72 与 L199（当前因临时关闭摆臂与推杆，`kReloadInstalled=false`，自动换弹停用，库存走 `ResetManual`）。
4. **`xrobot-local/DartLauncher/src/hw/launcher_hw.cpp`**（条件改动，默认不改）：
   - **仅当** ARM 固件编译在 `-Wall -Werror` 下因 `kArmInstalled = false` 报 L17 `kGm6020TorqueConstant` `-Wunused-const-variable` 错误时，允许将 L17 常量移入 L583 `if constexpr (kArmInstalled)` 块内（零行为变化）；若编译无警告则**不修改**该文件。
5. **`dart-fw/Modules/modules.yaml` 与 `dart-fw/xrobot.lock`**（必改，仅通过 `xrobot` CLI 同步更新）：
   - 锁定 `local/DartLauncher` 新提交；**不手改 `xrobot.lock`**，**不编辑 `dart-fw/Modules/local/DartLauncher` 副本**。
   - **无需改动** `dart-fw/User/RobotConfig/artisans_dart.yaml`、`dart-fw/Core/Src/main.c`、`dart-fw/User/app_main.cpp`，**无需运行 `xrobot gen`**（模块构造参数未变，且 `arm_motor` 仍保留在 YAML 中由 `arm_.Send()` 每拍发零电流维持 CAN 帧完整性）。
6. **`plans/tasks/4i-disable-arm-crank-temp.md` 与 `handoff.md`**（收口阶段由实施/主对话更新）：
   - 记录实际提交哈希、验证结果、ELF SHA-256 及现场验收状态。

### 2.4 严禁修改的文件与模块
- 禁止修改 `src/safety.*`、`src/match_flow.*`、`src/mode_logic.*`、`src/launch_sequence.*`、`src/motor_axis.*`、`src/dart_inventory.*`、`src/tension_modbus.*`、`src/hw/tension_port.*`、`src/hw/hx711_port.*`、`src/hw/magnet_port.*`、`src/hw/servo_port.*`、`src/hw/motor_port.*`。
- 禁止清理或覆盖 `dart-fw/Modules/QDU-Robomaster/Referee` 的 3 处本地补丁。
- 不新建 Git 分支、不 `git push`、不 `reset/restore/clean/amend`、不在 worker 阶段自行烧录。

---

## 3. 影响范围逐链路核对

| 子系统 / 代码位置 | 变更后行为 | 核对结论 |
|---|---|---|
| **电机在线与 Safety** (`launcher_hw.cpp` L14–15, L116–134, L164–173, L376–378) | `kMotorInstalled = {true, true, true, false, false}`；`Tick()` 每拍主动给槽位 3（yaw）与槽位 4（arm）调用 `safety_.MotorFeedback()` 并由 `Send()` 发零电流；`motors_online` 与 `all_homed` 仅检查左滑块、右滑块、pitch | 无 6020 时不再报 `fault=电机离线`，三台已装电机掉线仍照常报故障停机 |
| **摆臂控制与建零** (`launcher_hw.cpp` L174–183, L211, L361–375, L418–424) | 跳过绝对编码器建零、`Compute`/`Gate` 使能恒为 `false`、跳过 `arm_stall_` 堵转限流；监控反馈 `arm_current`/`arm_raw` 为 0 | CAN2 无 6020 时零电流静默，无误触发堵转日志 |
| **推杆 PWM 与 TIM8** (`launcher_hw.cpp` L93–94, L141–142, L383–385, L434–446, L809–839) | `crank_` 构造参数 `enabled = false`，不调用 `SetConfig`/`Enable`，不输出 PWM；`state_.crank = crank_.Value() = 0`（有限值，非 NaN）；不干扰同定时器 TIM8 的锁止舵机 `pwm_tim8_ch2` | 推杆拆机完全静默，无误动、不打断锁止脉冲 |
| **自动换弹与库存** (`dart_params.hpp` L259, `launcher_hw.cpp` L101–105, L329–346, L689–763) | `kReloadInstalled` 自动变为 `false`；开机库存走 `inventory_.ResetManual()`（`darts=1`）；装弹组取镖 `StartFetchTest()`、装弹组回零及摇杆均被拦下并提示“该机构未安装” | 换弹流程彻底关闭；自动连发入口 `command_.salvo` 被 `if constexpr (!kReloadInstalled)` 拦截 |
| **4f 自动回零协调** (`match_flow.cpp` L156–210, `launcher_hw.cpp` L217–223, L266–287, L337–346) | 未接裁判（或 `Stage::NotStarted`）时，自动模式下左拨杆**中→下**触发 `salvo` 边沿：`AutoHomeStart()` 消费 `salvo` → 先发 `auto_home_aim`（仅 pitch 回零并退让至 95 mm）→ 等 `aim_homed && !homing` 后发 `auto_home_slider`（双滑块同步回零并退让）→ 等 `slider_homed && !homing && all_homed` 后发出一次 `c.salvo` → 适配层 L341 命中 `!kReloadInstalled`，仅打日志 `XR_LOG_WARN("换弹机构未安装")` 后自然停住 | **天然闭环安全边界**：可完整验证 4f pitch→滑块自动回零顺序与退让等待，回零完成后绝不会进入上膛、换弹或发射 |
| **USB 串口监控打印** (`launcher_hw.cpp` L519–594, L640–656) | `online=1/1/1/0/0`、`home=hSL/hSR/hP/0/0`；状态行带未装标记 `arm=0.00 raw=0[X] ... crank=0[X]`；不再打印 `Dart arm cmd=...` 行；`Dart pitch cmd=...` 与 4g 的 `Dart rs485 L/R` 照常打印 | 与既有日志格式完全兼容，保留 4g 拉力读数，不截断 |
| **重要操作边界（风险隔离）** (`launcher_hw.cpp` L288–336) | `!kReloadInstalled` 时：自动模式左拨杆**中→上**（`command_.fire`）会进入手动单发 `CommandFire()`；手动滑块组拨轮负向 2 s 会进入 `Cock()`；手动装弹组拨轮负向 2 s 在 `kMagnetInstalled=true` 时仍可切换电磁铁继电器 | 现场验收**严禁**操作以上三项手势（详见第 5、8 节） |

---

## 4. 实施阶段与验证命令

在 PowerShell 7 中按顺序分阶段执行，每步检查退出码；主机编译/测试与同步限 120 s，固件构建限 300 s。

### 阶段 A：仓库预检与主机基线确认
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot'
git -C xrobot-local/DartLauncher branch --show-current
git -C xrobot-local/DartLauncher status --short
git -C xrobot-local/DartLauncher rev-parse HEAD
git -C dart-fw branch --show-current
git -C dart-fw status --short
git -C dart-fw/Modules/local/DartLauncher rev-parse HEAD
git -C dart-fw/Modules/local/DartLauncher status --short
git -C dart-fw/Modules/QDU-Robomaster/Referee status --short
```
预期：`DartLauncher` 在 `codex/apply-load-position-calibration` @ `3f2eb79e533df297bb6238e37d63561ee67dfce9`、`dart-fw` 在 `dart-launcher` @ `c154c462d6d591ac570dea47941c9558de344779`、同步副本与源码 HEAD 一致、`Referee` 仅保留 3 处既有修改。

### 阶段 B：修改配置、测试与 README，运行主机回归
按第 2.3 节修改 `src/dart_params.hpp`、`test/host_test.cpp`、`README.md`（确认改动文件保持 LF 行尾），随后运行主机回归与纯逻辑依赖扫描：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher'
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$taskSources = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $taskSources -o build-host/host_test_4i.exe
.\build-host\host_test_4i.exe
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Itest/hx711_stubs -Isrc/hw test/hx711_port_test.cpp src/hw/hx711_port.cpp -o build-host/hx711_port_test_4i.exe
.\build-host\hx711_port_test_4i.exe
Select-String -Path src\*.hpp,src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
```
预期：
- `host_test_4i.exe` 通过（预计 `PASS: 31569 checks`，0 失败，且 4 处更新断言与 `TestAutomaticHomeStartRegression` 全通过）；
- `hx711_port_test_4i.exe` 通过 `PASS: 2193 HX711 checks`；
- 纯逻辑依赖扫描输出为空。

### 阶段 C：ARM 单文件预检与源码仓本地提交
在提交前必须针对**修改后的 `xrobot-local/DartLauncher` 源码与头文件**执行 ARM C++20 `-Wall -Werror` 单文件预检。注意：`dart-fw/build/debug/compile_commands.json` 中的 `-I` 与源文件路径默认指向同步副本 `dart-fw/Modules/local/DartLauncher`，在阶段 D 执行 `xrobot setup --update` 前副本中的 `dart_params.hpp` 仍是 `kArmInstalled = true`，直接复用会产生“假通过”。必须按以下 PowerShell 命令将所有 `Modules/local/DartLauncher` 路径替换为 `xrobot-local/DartLauncher` 后再调用 `arm-none-eabi-g++`（输出至 `build-host/launcher_hw_4i.obj`，不污染 `dart-fw/build/debug/`）：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw\build\debug'
$entry = (Get-Content -Raw -LiteralPath 'compile_commands.json' | ConvertFrom-Json) |
    Where-Object { ($_.file -replace '\\', '/') -like '*Modules/local/DartLauncher/src/hw/launcher_hw.cpp' } |
    Select-Object -First 1
if (-not $entry) { throw 'compile_commands.json 中未找到 launcher_hw.cpp 条目' }
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
$argsList.Add('-c')
$argsList.Add('-o')
$argsList.Add('C:/Users/Lenovo/Desktop/xrobot/xrobot-local/DartLauncher/build-host/launcher_hw_4i.obj')
if ($argsList | Where-Object { $_ -match 'Modules[/\\]local[/\\]DartLauncher' }) {
    throw '预检参数仍残留 dart-fw/Modules/local/DartLauncher 同步副本路径'
}
$argsList | Where-Object { $_ -like '*DartLauncher*' }
& $compiler @argsList
if ($LASTEXITCODE -ne 0) { throw "ARM 单文件预检失败，退出码: $LASTEXITCODE" }
```
若上述预检因 `kArmInstalled = false` 触发 `launcher_hw.cpp` L17 `kGm6020TorqueConstant` 的 `-Wunused-const-variable` 报错，按第 2.3 节第 4 条将其移入 L583 `if constexpr (kArmInstalled)` 块内，重新执行阶段 B 主机回归与上述 ARM 单文件预检直至退出码为 0 且零警告。

预检零警告通过后，逐个暂存并提交（`git add` 与 `git commit` 分开执行）：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher'
git diff --check
git add src/dart_params.hpp test/host_test.cpp README.md
# 若阶段 C 调整了 launcher_hw.cpp 作用域，再显式 git add src/hw/launcher_hw.cpp
git diff --cached --check
git diff --cached
git commit -m "Temporarily disable arm and crank for auto-home check"
git rev-parse HEAD
git branch --show-current
git status --porcelain
```

### 阶段 D：备份 4g ELF、同步模块与构建固件
先确认备份文件不存在并备份板上 4g ELF，再用上一步真实完整提交哈希同步模块、复跑同步副本主机回归、构建 debug 固件：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw'
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
Copy-Item -LiteralPath build/debug/DevC.elf -Destination build/DevC-3f2eb79-before-4i.elf
xrobot module remove local/DartLauncher
xrobot module add 'local/DartLauncher@<源码完整提交哈希>'
xrobot setup --update local/DartLauncher
git -C Modules/local/DartLauncher rev-parse HEAD
git -C Modules/local/DartLauncher status --short
xrobot format --check
cmake --build --preset debug
Get-FileHash -LiteralPath build/debug/DevC.elf -Algorithm SHA256
```
并在 `dart-fw/Modules/local/DartLauncher` 中执行同步副本主机回归（输出至 `C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher\build-host\host_test_4i_sync.exe`）：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw\Modules\local\DartLauncher'
$env:PATH = 'C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;' + $env:PATH
$syncSources = (Get-ChildItem -LiteralPath src -Filter '*.cpp').FullName
g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test/host_test.cpp $syncSources -o C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher\build-host\host_test_4i_sync.exe
C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher\build-host\host_test_4i_sync.exe
```
全部通过后，在 `dart-fw` 仅暂存 `Modules/modules.yaml` 和 `xrobot.lock` 并提交：
```powershell
Set-Location 'C:\Users\Lenovo\Desktop\xrobot\dart-fw'
git add Modules/modules.yaml xrobot.lock
git diff --cached
git commit -m "Lock DartLauncher with arm and crank temporarily disabled"
git rev-parse HEAD
git branch --show-current
git status --porcelain
git -C Modules/QDU-Robomaster/Referee status --short
```

---

## 5. 烧录前提与现场验收（4f 自动回零边界）

### 5.1 烧录前提（由用户在自己终端执行）
- 烧录前必须逐项确认：
  1. 确认未上膛（`latch=2`），滑块未处于托持未锁止发射机构状态（`SliderMustHold` 期间**严禁断电或烧录**）；
  2. 发射座无镖，建议无皮筋（或确认滑块可安全回顶），手离开发射机构、滑块导轨、pitch 丝杠与夹爪；
  3. 确认 pitch 当前位于上端挡块下方 1–2 cm 以内（pitch 堵转回零从低位起步尚未现场验证，低位 869 限流可能中途误判堵转）；
  4. 确认 6020 与推杆舵机确实已拆除/断线，发射方向与机构包络内无人。
- 烧录命令（匹配新构建的 `dart-fw/build/debug/DevC.elf`，看到 `** Verified OK **` 才算成功）：
  ```powershell
  & 'C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c 'program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset exit'
  ```

### 5.2 现场验收步骤与通过判据（严格限定在自动回零边界内）
全程录制 USB CDC 串口日志（常见 COM46），**禁止**操作“自动模式左拨杆中→上（单发）”“滑块组拨轮负向 2 s（上膛）”“装弹组拨轮负向 2 s（电磁铁）”。

1. **上电与无离线故障检查（无力模式：右拨杆下）**：
   - 打开遥控器，保持右拨杆在下；
   - **判据**：串口打印 `fault=无故障`（不再出现 `fault=电机离线`）；`online=1/1/1/0/0`、`home=0/0/0/0/0`；状态行显示 `arm=0.00 raw=0[X]` 与 `crank=0[X]`；无 `Dart arm cmd=...` 行；`Dart rs485 L/R` 读数照常更新、`late` 无异常增长。
2. **装弹组屏蔽检查（手动模式：右拨杆中、左拨杆下）**：
   - 推动摇杆超过死区（左摇杆横向 > 0.15 或右摇杆纵向 > 0.05）或拨轮正向保持 2 s；
   - **判据**：串口打印 `该机构未安装`，无任何电机或 PWM 误动，不报故障。
3. **4f 一次自动启动顺序验收（全未回零 → 自动模式：右拨杆上）**：
   - 先切到右拨杆上、左拨杆中，静置 3 s 确认**不会**自行启动回零；
   - 将左拨杆**从中拨到下**（触发 4f 自动回零边沿），之后保持拨杆不动，观察全过程：
   - **判据（按时间顺序必须全部满足）**：
     1. 串口先出现 `自动回零：瞄准`，pitch 以约 5 mm/s、限流 ≤869 向上寻零，顶到上端保持约 1 s 后退让至约 `95 mm`，期间滑块**不得**提前启动；
     2. pitch 寻零和退让均完成后（`home=0/0/1/0/0` 且退出 `Retreating`），串口才出现 `自动回零：滑块`，左右滑块同步向上回零顶防撞块并退让；
     3. 滑块退让完成、`home=1/1/1/0/0` 且 `home=1` 成立后，串口仅出现一次 `换弹机构未安装` 警告，**不启动任何上膛、换弹或发射步骤**（`step=空闲`）；
     4. 保持左拨杆在下不动，系统**不会**重复触发回零或重复刷屏警告。
4. **已回零不重回与取消/中断边界检查**：
   - 在全部已回零状态下将左拨杆拨回中再拨到下：直接提示一次 `换弹机构未安装`，**不重回 pitch 或滑块**；
   - （可选）复位重启后，在 pitch 或滑块自动回零途中将右拨杆切回无力（下）或手动（中）：串口打印 `自动回零已取消`，机构立即停机；重新上力进入自动档（无新边沿）**不会**继续自动回零。

---

## 6. 回滚方式

### 6.1 现场快速固件回滚（不改代码，立即回到 4g）
若现场需要立即恢复烧录前 4g 固件（确认未上膛、非滑块托持态、手离机构）：
```powershell
& 'C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe' -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c 'program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/DevC-3f2eb79-before-4i.elf verify reset exit'
```
核对备份 ELF SHA-256 应为 `C31D63EFD3B4312540FD55FBC39B6110168408C7BA2F3963C62C04F72D882B57`。

### 6.2 换弹机构重装后的代码恢复（正式回滚）
换弹机构重新装车并接线核对完毕后，按新任务单独提交恢复：
1. 在 `xrobot-local/DartLauncher/src/dart_params.hpp` 将 `kArmInstalled`、`kCrankInstalled` 改回 `true`（`kReloadInstalled` 自动恢复为 `true`，全部历史标定值无需重填）；
2. 在 `test/host_test.cpp` 将第 224、254、259、285 行 4 处断言恢复为启用态，并更新 `README.md`；
3. 按既有流程跑主机回归、源码仓提交、`dart-fw` 模块同步与 debug 构建；严禁使用 `git reset --hard` 或强推覆盖历史。

---

## 7. 风险点与需人工确认处

1. **`kMagnetInstalled` 是否保持 `true`（需确认）**：
   - 本计划按最小变更仅关闭 `kArmInstalled` 与 `kCrankInstalled`（`kReloadInstalled` 自动为 `false`，且自动取镖/装填入口均被关闭），保持 `kMagnetInstalled = true` 不动；上电 `MagnetPort` 仍写断电吸持电平。
   - **需人工确认**：若电磁铁继电器控制线也已随换弹机构拆除，且希望上电完全不初始化 `pwm_tim1_ch3`（同时彻底禁用装弹组拨轮负向 `magnet_toggle`），可在实施前明确要求将 `kMagnetInstalled` 一并临时改为 `false`（对应 `host_test.cpp` L255、L285 同步更新）。
2. **`launcher_hw.cpp` L17 `-Wunused-const-variable` 潜在编译警告**：
   - `dart-fw` 启用 `-Wall -Werror`，关闭 `kArmInstalled` 后 `kGm6020TorqueConstant` 成为未引用局部常量；如 ARM 编译器报错，按第 2.3 节第 4 条将其移入 `if constexpr (kArmInstalled)` 块内，不改动任何其他逻辑。
3. **自动模式左拨杆“中→上”仍对应手动单发**：
   - 因 `kAutoEnabled = true` 且 `kCockEnabled = true`，`!kReloadInstalled` 下仅拦截 `salvo`（左拨杆中→下），不拦截 `fire`（左拨杆中→上）。现场验收时务必只使用左拨杆**中→下**验证 4f 自动回零，严禁在无清场/无发射计划下拨中→上或拨轮负向上膛。
4. **pitch 起点高度风险**：
   - 现场验证 4f 自动回零前，需先确认 pitch 起点在上端挡块 1–2 cm 内；若从低位起步，869 限流可能中途误判堵转停在半路。

---

## 8. 停止条件（触发任一条立即停止并汇报）

### 软件实施与构建阶段
- 主机基线测试即失败，或改动后除预期 4 处参数开关断言外出现任何其他测试失败；
- 纯逻辑依赖检查（`src/*.hpp`、`src/*.cpp`）出现非空匹配；
- `launcher_hw.cpp` 在 ARM 编译下出现超出 `kGm6020TorqueConstant` 作用域之外的新警告或错误；
- `xrobot setup --update` 后副本 HEAD 与源码提交不一致、工作区不干净、`xrobot format --check` 失败，或发现 `Referee` 3 处本地补丁被修改/丢失；
- 任何需要修改 `Safety`、`MatchFlow`、`LaunchSequence` 安全规则才能绕过的问题。

### 现场烧录与验收阶段
- 滑块正处于托持未锁止发射机构状态（`SliderMustHold`）——**严禁断电或烧录**；
- 烧录后在无 6020/推杆条件下仍报 `fault=电机离线` 或任何未知故障；
- 自动回零中 pitch 与滑块**同时启动**、pitch 尚未退让完成滑块就开始移动、或回零完成后系统尝试进入上膛/开锁/发射；
- pitch 回零出现联轴器打滑、异响、超时或触发 `HomeOverrun`，或滑块出现不同步（`SliderSync`）、电机非预期动作、控制周期 `late` 持续升高。

---

## 9. 实施与审查验收记录

- **实施完成时间**：2026-11-24
- **源码仓提交**：`xrobot-local/DartLauncher@e17b490e8ffbefedc08c96d8d74396ba257487de`（仅修改 `src/dart_params.hpp`、`test/host_test.cpp`、`README.md`）
- **固件仓提交**：`dart-fw@fa0838b6b405db715d3be8eb552784da49f3cc35`（锁定 `local/DartLauncher@e17b490`，保留 `main.c` 9600 波特率与 `artisans_dart.yaml` 的 `tension_uart` 既有配置，保留 `Referee` 3 处补丁）
- **测试结果**：
  - 主机单元测试：`PASS: 31569 checks`
  - HX711 单元测试：`PASS: 2193 HX711 checks`
  - ARM 单文件预检：针对源码仓头文件零警告
  - 同步副本主机回归：`PASS: 31569 checks`
  - 代码风格检查：`xrobot format --check` 退出 0
  - 固件编译：`cmake --build --preset debug` 退出 0
- **固件哈希**：
  - 4g 备份固件 `dart-fw/build/DevC-3f2eb79-before-4i.elf` SHA-256：`C31D63EFD3B4312540FD55FBC39B6110168408C7BA2F3963C62C04F72D882B57`
  - 4i 新编译固件 `dart-fw/build/debug/DevC.elf` SHA-256：`C2A54393EEF4D721E73F1400F02749BF3977B649D5DECF71CC04B9696FD1B41E`
- **代码审查**：Reviewer 审查通过（`VERDICT: APPROVED`）
- **当前状态**：实车现场验收 100% 通过（串口日志已核验：无离线故障、pitch 先回零退让至 95 mm → 滑块后回零退让完成 → 警告“换弹机构未安装”自然停住，未进入发射）。任务圆满完成。
