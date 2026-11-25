# 任务 2o：加退膛；发射时滑块不低于 355 mm 就算到顶；流程结束显示“空闲”；手动点动不低于上膛位

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 50f7b4f），主机测试现在是 `PASS: 18243 checks`
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS；`src\hw\`：LibXR 适配层
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 5a3e923）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。
- 滑块坐标向上为正：最顶端 0.370，停靠位 `kSliderPark` = 0.360，探测位 `kProbePosition` ≈ −0.284，上膛位 `kCockPosition` = −0.318，硬底 `kCockBottom` = −0.3195。

## 为什么要改
2026-10-16 实车第二次上膛成功（`latch=1`），暴露了下面四个问题：
1. 已经锁住以后，没有办法不发射就把皮筋放松。拨轮下拨 2 s 时，`ExpandCock()` 看到已锁住，只会让滑块回位。用户要一个“退膛”。
2. `Safety::FireBlocker` 要求 `|slider − kSliderPark| ≤ 0.01`。滑块被推到最顶端 0.370 时，正好卡在边界上，浮点误差一算就会报 `NotParked`。用户要求：滑块不低于 355 mm 就算到顶，一直到最顶端都可以发。
3. 流程最后一步做完以后，`LauncherHardware::Tick()` 只在 `sequence_.Busy()` 时调用 `sequence_.Tick()`，所以 `label_` 一直停在最后一步的名字，不会变回“空闲”。
4. 滑块轴的下限是 `kSliderBottom` = −0.42。可是发射机构一直在滑块下面，到 −0.3195 就压到底了。已放开状态下手动往下点动，会用电机的全力把发射机构撞到底。

## 要改的地方
1. `src\dart_params.hpp`：
   - 在 `kSliderPark` 那一行后面加：`constexpr double kFireSliderMin = kSliderPark - 0.005; // m，发射时滑块不低于 355 mm，往上一直到最顶端都算到顶。`
   - 在 `kReturnTimeout` 那一行后面加：`constexpr double kDecockTimeout = (kSliderPark - kCockPosition) / kLoadRiseSpeed + kReturnTimeout; // s，退膛时从上膛位慢慢升回停靠位。`。如果 `kLoadRiseSpeed` 定义在后面，就把这一行放到它后面，保证能编译。
2. `src\safety.cpp` 的 `FireBlocker`：
   - 把 `std::fabs(s.slider - kSliderPark) > 0.01` 换成 `s.slider < kFireSliderMin`，`!std::isfinite(s.slider)` 照旧。返回的还是 `SafetyFault::NotParked`，提示文字不变。
   - `safety.cpp` 里别的地方都不动。
3. `src\launch_sequence.hpp`、`src\launch_sequence.cpp`：
   - 新增 `bool CommandDecock();`，放在 `CommandCock()` 旁边，注释：`// 已锁住时退膛：滑块压住发射机构，张开夹爪，再带着它慢慢升回停靠位；没锁住或不能接受命令时返回 false，不报故障。`
   - 实现：`Accept()` 不通过，或者 `io_.State().latch_state != LatchState::Latched`，就返回 false。否则 `Append` 下面 4 步，返回 `Append` 的结果：
     ```cpp
     {"退膛：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
     {"退膛：滑块慢压", Action::Press, Condition::Pressed, kCockPosition, kPressTimeout},
     {"退膛：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout},
     {"退膛：滑块慢慢回位", Action::Rise, Condition::Parked, kSliderPark, kDecockTimeout}
     ```
     - “退膛：张开夹爪”走现有的 `Action::Lock`：`Enter()` 会先检查 `LockOpenAllowed`，这时滑块在上膛位、低于锁止位，可以通过；开锁以后记为 Released。
     - `Action::Rise` 用的是 `kLoadRiseSpeed`，等于 0.05 m/s。
     - `ExpandCock()` 不动。
   - `Tick()` 末尾 `if (Done()) { has_current_ = false; }` 改成：`Done()` 为真时 `has_current_ = false;`，再加 `if (!Busy()) { label_ = "空闲"; }`，上面注释 `// 最后一步做完立刻显示空闲，适配层不会再调用 Tick。`
4. `src\hw\launcher_hw.cpp`：
   - `Tick()` 里处理 `command_.cock_request` 的最后那个 `else` 分支，原来是 `if (state_.all_homed) { sequence_.CommandCock(); } else { XR_LOG_WARN("轴未全部回零，拒绝上膛"); }`，改成：
     ```cpp
     if (!state_.all_homed) {
         XR_LOG_WARN("轴未全部回零，拒绝上膛");
     } else if (state_.latch_state == LatchState::Latched) {
         // 已锁住时拨轮下拨是退膛：不发射，把皮筋慢慢放松。
         XR_LOG_INFO("退膛");
         sequence_.CommandDecock();
     } else {
         sequence_.CommandCock();
     }
     ```
     原来的注释“流程展开前继续保持……”留在 `CommandCock()` 那一支上面。
   - `SliderConfig()` 里传给 `Axis(...)` 的下限从 `kSliderBottom` 换成 `kCockPosition`。上面的注释补一句：`下限取上膛位：发射机构一直在滑块下面，到硬底就压不动了，手动点动和流程目标都不低于这里。`
   - 别的地方不动。
5. `test\host_test.cpp`：
   - `FireBlocker` 相关的旧测试按新规则改：滑块在 0.370、0.360、0.355 都允许发射；0.354、更低的位置和 NaN 报 `NotParked`。别的安全判断不放宽。
   - 新增 `TestDecock()`，在 `main()` 里调用，照现有 `FakeIo` 的写法：
     - 已锁住、滑块在停靠位时，`CommandDecock()` 返回 true。跑完的步骤依次是上面 4 步。
     - 第 1 步 `SetSlider` 的目标是 `kProbePosition`，速度 0；第 2 步目标 `kCockPosition`，速度 `kPressSpeed`；第 3 步锁止目标 `kLockOpen`；第 4 步目标 `kSliderPark`，速度 `kLoadRiseSpeed`。
     - 跑完：锁止状态 Released，滑块在停靠位，没有故障，`StepName()` 是“空闲”。
     - 状态是 Unknown 或 Released 时，`CommandDecock()` 返回 false，`Fault()` 仍是 None，不排任何步骤。
   - 新增检查：上膛流程正常跑完以后，`StepName()` 是“空闲”。
   - `kFireSliderMin` 等于 0.355；`kDecockTimeout` 大于 `(kSliderPark - kCockPosition) / kLoadRiseSpeed`。
6. `README.md`：
   - 加“退膛”的说明：已锁住时，右拨杆中、左拨杆中，拨轮往下拨到底保持 2 s。滑块先下到锁止位上方，再慢压到上膛位顶住发射机构，然后张开夹爪，最后每秒 5 cm 带着发射机构升回顶上。状态是“未知”时，要先上膛一次让固件探测；如果探测出来已经锁住，再拨一次就是退膛。退膛途中出问题，跟上膛一样：滑块原地保持，不能断电，按脱困处理。
   - 发射条件里“滑块回到顶部”改成“滑块不低于 355 mm”。
   - 写明滑块轴的下限是上膛位 −0.318，手动往下点动到这里就停。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\safety.cpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
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
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "加退膛，发射不低于 355 mm 算到顶，流程结束显示空闲，点动不低于上膛位"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 主机测试没有 `PASS`，或者要删检查、放宽“到顶”以外的安全判断才能通过
- 第 3 步有输出
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件；改了哪些旧测试，各改了什么
- 主机测试的 `PASS` 数；第 3 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
