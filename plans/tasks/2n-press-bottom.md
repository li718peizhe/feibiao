# 任务 2n：上膛下压不顶底、最后一段慢压，状态行加滑块电流

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD cd3156c），主机测试现在是 `PASS: 18233 checks`
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS；`src\hw\`：LibXR 适配层
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD b55ec4a）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。
- 滑块坐标向上为正，最顶端 0.370；锁止位 `kLatchSlider` 约 −0.314，探测位 `kProbePosition` = 锁止位 + 30 mm。

## 为什么要改
2026-10-16 第一次实车上膛：
- 探测正常，滑块停在 −0.284，判定已放开，夹爪张开。
- “上膛：滑块下压”的目标是 `kCockPosition` = −0.322，可滑块到 −0.3195 就下不去了。用户看了：发射机构已经进了夹爪，底部越过爪子约 1 cm，是顶到底了。卡了 8 s，报“步骤超时”。
- 这一步用 `Action::Slider`，走默认限速 `kSliderMoveSpeed` = 0.3 m/s，最后几厘米是快速撞到底的。
- 状态是已放开时（比如发射以后）再上膛，`ExpandCock` 不走探测，会从停靠位一路 0.3 m/s 压到底。
- 状态行里看不到滑块电流，分不清是碰到底了，还是电机的力不够。

改法：
- 下压终点改成离硬底 1.5 mm。
- 下压分两段：先按默认限速下到探测位，夹爪是张开的，发射机构底部还在爪子上方约 3 cm；再用 `kPressSpeed` = 0.05 m/s 慢慢压到终点。
- 第 5 行状态加上滑块电流。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kCockPosition` 那一行换成下面两行，顺序不能变：
     ```cpp
     constexpr double kCockBottom = -0.3195; // m，2026-10-16 实测：发射机构压到底（底部已越过爪子约 1 cm）时的滑块位置。
     constexpr double kCockPosition = kCockBottom + 0.0015; // m，离底留 1.5 mm，不顶底；这时发射机构底部仍越过爪子。
     ```
   - 在 `kProbeSpeed` 那一行后面加：`constexpr double kPressSpeed = 0.05; // m/s，从探测位往下压到上膛位的限速，免得撞底，待标定。`
   - 其他常量都不动，`kLatchSlider`、`kLatchCheckRise`（20 mm）都不改。
2. `src\launch_sequence.hpp`：
   - `Action` 末尾加 `Press`，`Condition` 末尾加 `Approach`。
3. `src\launch_sequence.cpp`：
   - `Enter()` 加 `case Action::Press:`，调用 `io_.SetSlider(current_.target, kPressSpeed);`，上面注释 `// 慢压到目标，免得撞底。`
   - `Done()` 加 `case Condition::Approach:`，返回 `io_.SliderAt(current_.target, kSliderTolerance, kSliderVelocityTolerance)`，上面注释 `// 到本步目标位置并停稳。`
   - `ExpandCock()` 最后那组步骤（已放开时）改成 6 步：
     ```cpp
     {"上膛：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout},
     {"上膛：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
     {"上膛：滑块慢压", Action::Press, Condition::Pressed, kCockPosition, kPressTimeout},
     {"上膛：锁止", Action::Lock, Condition::Lock, kLockClosed, kServoTimeout},
     {"上膛：锁止确认", Action::LatchCheck, Condition::LatchCheck, 0, kPressTimeout},
     {"上膛：滑块回位", Action::Slider, Condition::Parked, kSliderPark, kReturnTimeout}
     ```
     `Prepend(steps, 5)` 跟着改成 6。
   - `LoadSteps()` 不动（换弹机构没装，这次不改）。
4. `src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`：
   - `MonitorFeedback` 加 `double slider_current = 0;`。
   - `Tick()` 末尾在锁里填快照的地方加 `monitor_feedback_.slider_current = kSliderInstalled ? SliderCurrent() : 0;`。
   - `OnMonitor()` 第 5 行（`Dart online=...`），在 `sl=%.2f/%.2fmm` 后面加 ` I=%d`，参数是 `static_cast<int>(feedback.slider_current)`。这一行加上以后不能超过 120 个字符。
   - 别的地方不动。
5. `test\host_test.cpp`：
   - `FakeIo` 记录最后一次 `SetSlider` 的速度参数，比如加成员 `slider_speed`。
   - 原来的上膛测试按新的 6 步改：步骤名、步数、`kCockPosition` 的新值。不要删检查，也不要放宽安全判断。
   - 新增检查：
     - `Near(kCockPosition - kCockBottom, 0.0015, 1e-9)`，`kCockPosition < kLatchSlider`，`kCockPosition > kCockBottom`。
     - 已放开时上膛：第二步“上膛：滑块下到锁止位上方”的 `SetSlider` 目标是 `kProbePosition`，速度是默认值 0；第三步“上膛：滑块慢压”的目标是 `kCockPosition`，速度是 `kPressSpeed`；整个流程跑完是 Latched，滑块回到 `kSliderPark`，没有故障。
     - 未知状态上膛（先探测）：跑完同样是 Latched；从探测位开始，下压那一步的速度也是 `kPressSpeed`。
6. `README.md`：
   - 上膛步骤的说明改成新的 6 步，写明硬底 −0.3195、终点 −0.318、慢压 0.05 m/s。原因：2026-10-16 第一次上膛时，原来的 −0.322 顶到底下不去，最后一段还是 0.3 m/s 撞上去的。
   - 状态行第 5 行的说明加上 `I=`：两台滑块电流绝对值的最大值，电调原始单位；上限是 `kSliderCurrentCap`，约 9178。
   - 记一条待办：`LoadSteps` 里压到 `kCockPosition` 的两步还是默认限速，装换弹机构之前要改。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 改代码（第 1~6 条）。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，把 N 写进汇报。
3. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "上膛下压不顶底、最后一段慢压，状态行加滑块电流"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
5. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - 用 `git -C Modules\local\DartLauncher log --oneline -1` 确认副本已经是第 4 步的提交；`build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败，就停下写汇报。不要手改 `xrobot.lock`，也不要自己去副本里 fetch 或 checkout。
6. 在 dart-fw 提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 主机测试没有 `PASS`，或者要改 `safety.cpp`、删检查、放宽安全判断才能通过
- 第 3 步有输出
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件；改了哪些旧测试，各改了什么
- 主机测试的 `PASS` 数；第 3 步的结果；第 5 行 `printf` 格式串的原文
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
