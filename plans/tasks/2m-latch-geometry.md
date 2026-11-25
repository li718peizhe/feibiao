# 任务 2m：按实测几何写入锁止位置，锁止步骤等舵机转到位，打开上膛

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 4f3b7f9），主机测试现在是 `PASS: 17485 checks`
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS；`src\hw\`：LibXR 适配层
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD eccb34f）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。
- 滑块坐标向上为正，最顶端是 `kSliderTop = 0.370`。

## 为什么要改
1. 2026-10-16 实测：
   - 滑块最大行程 790 mm，固件现在按 710 mm 算（`kSliderTravel`）。
   - 滑块顶在最上面时，它的压面到停在顶部的发射机构被压面，距离 130 mm。
   - 发射机构被压面到夹爪 630 mm。夹爪卡的是发射机构底部，底部比被压面低 76 mm。
   - 所以夹爪刚好卡住时，滑块坐标 = 0.370 − 0.130 − (0.630 − 0.076) = −0.314。现在用的是旧 CAD 估的 −0.317（`kPushGap = 0.317`）。
2. 量的误差可能有几毫米。现在上膛先把滑块压到锁止位下方 3 mm（`kCockPosition`）再合夹爪，然后锁止确认上抬 10 mm（`kLatchCheckRise`）：
   - 锁止位估高 2 mm 以上，夹爪就合不上；估低 7 mm 以上，锁止确认又会误报“没有锁住”。
   - 改成下压 8 mm、上抬 20 mm。上抬必须大于下压深度，滑块才能离开发射机构。探测停在锁止位上方 30 mm（`kProbeMargin`），不动。
3. `LaunchSequence::Done()` 的 `Condition::Lock` 只看 `LockAt()`，也就是看斜坡走没走完：机构角 45°，按 `kServoSpeed = 7 rad/s` 算，约 0.11 s 就走完。可锁止舵机 PM60S 实际要转 67.5°，比这慢。
   - 在“探测：张开夹爪”“上膛：张开夹爪”之后，滑块马上就开始下压，夹爪可能还没张开，发射机构底部就撞上去了。
   - 改成锁止步骤至少等 `kLockSettleTime = 0.6 s`。
4. 锁止舵机已经单独供电（10-16 用户确认），可以打开 `kCockEnabled`。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kSliderTravel` 改成 `0.790`，加注释 `// m，2026-10-16 机械给的滑块最大行程。`
   - `kShuttleTravel`、`kPushGap` 两行换成下面几行，顺序不能变：
     ```cpp
     // 2026-10-16 实测：滑块在最顶端时，压面到停在顶部的发射机构被压面 130 mm；被压面到夹爪 630 mm；
     // 夹爪卡发射机构底部，底部比被压面低 76 mm。
     constexpr double kTopGap = 0.130;
     constexpr double kShuttleTravel = 0.630 - 0.076; // m，发射机构从放松到底部刚好到夹爪要走的距离。
     constexpr double kPushGap = kTopGap + kShuttleTravel - kSliderTop;
     ```
   - `kLatchSlider` 的注释改成 `// m，夹爪刚好卡住发射机构底部时的滑块位置，按上面的实测几何算，约 −0.314。`
   - `kCockPosition` 改成 `constexpr double kCockPosition = kLatchSlider - 0.008; // m，多压 8 mm 再合夹爪，量的误差几毫米也能合上。`
   - `kLatchCheckRise` 改成 `0.020`，注释 `// m，要大于下压深度 8 mm，滑块才能离开发射机构，待标定。`
   - 在 `kServoTimeout` 那一行后面加：`constexpr double kLockSettleTime = 0.6; // s，锁止舵机转 67.5° 的时间加余量；流程只看斜坡，这里等舵机真的转到位，待实测。`
   - `kCockEnabled` 改成 `true`，注释改成 `// 2026-10-16 打开：锁止舵机已单独供电，锁止位置按实测几何算。`
   - 其他常量都不动（`kLoadPosition`、`kProbeMargin`、`kSliderPark` 等）。
2. `src\launch_sequence.cpp` 的 `Done()`，`case Condition::Lock:` 一开头加：
   ```cpp
   // 斜坡很快就走完，锁止舵机实际更慢，至少等 kLockSettleTime 再进下一步。
   if (elapsed_ < kLockSettleTime) {
       return false;
   }
   ```
   后面原来的判断不动。这对所有锁止步骤都生效，包括“发射：解锁”，这是有意的。
3. `test\host_test.cpp`：
   - `Check(!kCockEnabled, ...)` 改成 `Check(kCockEnabled, "cocking is enabled after the lock servo has its own supply");`
   - 新增检查：
     - `Near(kLatchSlider, -0.314, 1e-9)`，标签 `"latch slider follows measured geometry"`
     - `kLatchCheckRise > kLatchSlider - kCockPosition`，标签 `"latch check rise clears the press depth"`
     - `kCockPosition > kSliderBottom + 0.05`，标签 `"cock position keeps 50 mm from the slider bottom"`
     - 锁止步骤：用 `FakeIo` 跑一次上膛，`LockAt` 立刻为真时，“上膛：张开夹爪”这一步不到 `kLockSettleTime` 不能结束，滑块还不能开始下压；过了 `kLockSettleTime` 才往下走。照现有上膛测试的写法来。
   - 原来的测试如果假设锁止步骤一拍就完成，或者写死了旧的 −0.317、3 mm、10 mm，就跟着新常量改（比如加大 `RunSequence` 的循环次数）。不要删检查，也不要放宽安全相关的判断。
4. `README.md`：
   - 写锁止位置、上膛下压和锁止确认的地方，换成新的几何和数（130、630、76 mm，−0.314，下压 8 mm，上抬 20 mm），说明锁止步骤至少等 0.6 s 的原因。
   - 开关表里 `kCockEnabled` 改成 `true`，写明 2026-10-16 锁止舵机已单独供电。
   - 滑块行程改成 790 mm。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\launch_sequence.cpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 改代码（第 1~4 条）。
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
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "按实测几何写入锁止位置，锁止步骤等舵机到位，打开上膛"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 改了哪些文件；为了新常量和 0.6 s 等待改了哪些旧测试，各改了什么
- 主机测试的 `PASS` 数；第 3 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
