# 任务 3h：pitch 回零限到 1000 N、放慢，上端软限位留 5 mm，回零走过全程报故障

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 599fe3b），主机测试现在是 `PASS: 18730 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 2e91ea6）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-22 车上（pitch 上限 1738）：pitch 回零按 10 mm/s 撞上上端挡块后，联轴器打滑了：滑块停住了，电机还按 10 mm/s 一直转。回零要满限流、转子不转持续 1 s 才算到头，所以一直判不成，用户切无力才停下；不切的话要到 `kHomeTimeout` 90 s 才停。联轴器用户已经修好，上端有能挡住的挡块，没坏。3f 用 869 回零时只跑到约 6 mm/s，联轴器没打滑。
- 用户 2026-10-22 又说：设计上顶到上端的力不能超过 1000 N，不然联轴器会打滑。
- 这次改四处，都只针对 pitch：
  1. 回零和没回零的点动都会顶到上端挡块，这两样按 1000 N 限流，约 869（`kPitchHomeCurrentFraction` 从 1.0 改成 0.5）。回零以后的点动不碰挡块，还用满上限 1738。869 在离上端 2 cm 以内能往上走，再往下可能推不动，所以回零前 pitch 要放在上端附近。
  2. 回零速度从 10 mm/s 放到 5 mm/s，撞挡块轻一些。没回零的点动限速就是回零速度，也跟着变成 5 mm/s。
  3. 上端软限位离挡块留 5 mm，和下端一样。现在上端软限位就是挡块的位置，回零以后往上点动会按 10 mm/s 直接撞上挡块。改完以后只有回零才去顶挡块，回零后退到 95 mm。
  4. 回零走过一段距离还没堵转，就锁存故障，报“回零走完全程也没碰到头”。pitch 是整段行程再加 10 mm，也就是 190 mm。别的轴不设上限，行为不变。
- 滑块、yaw、摆臂都不动。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kPitchEndMargin` 的注释改成：`// m，上下端软限位都离机械端留 5 mm，实测行程以后再改，待标定。`
   - 在 `kPitchBottom` 后面加：`constexpr double kPitchTopLimit = kPitchTop - kPitchEndMargin; // m，上端软限位：离上端挡块留 5 mm，平常点动不碰挡块，只有回零去顶。`
   - `kHomeSpeed` 的注释改成：`// m/s，yaw 用，待标定；pitch 用 kPitchHomeSpeed，转轴需显式提供 rad/s。`
   - 把 `kPitchHomeCurrentFraction` 那一行换成下面两行：
     - `constexpr double kPitchTopForce = 1000.0; // N，2026-10-22 用户：设计上顶到上端的力不能超过 1000 N，不然联轴器打滑。`
     - `constexpr double kPitchHomeCurrentFraction = kPitchTopForce / (kPitchScrewForce * kPitchLoadMargin); // 回零和没回零的点动会顶到上端挡块，按 1000 N 限流，约 869；回零以后的点动不碰挡块，用满上限。`
   - 在 `kPitchHomeStallTime` 后面加两行：
     - `constexpr double kPitchHomeSpeed = 0.005; // m/s，2026-10-22：10 mm/s 撞上端挡块时联轴器打滑过（已修），pitch 回零和没回零的点动放慢一半，待标定。`
     - `constexpr double kPitchHomeSeekLimit = kPitchTravel + 0.010; // m，回零走过整段行程再多 10 mm 还没堵转，就当联轴器打滑或没碰到挡块，锁存故障。`
2. `src\motor_axis.hpp`：
   - 加 `#include <limits>`。
   - `AxisConfig` 在 `home_stall_time` 后面加：`double home_seek_limit = std::numeric_limits<double>::infinity(); // 回零最多走多远（m 或 rad），超过还没堵转就锁存故障；默认不限。`
   - `MotorAxis` 公开部分加一个查询，注释写“回零走过 home_seek_limit 还没堵转而锁存的故障；ClearFault 清掉”：`bool HomeOverrun() const { return home_overrun_; }`
   - 私有成员加：`double home_start_ = 0;`（回零开始时的位置）和 `bool home_overrun_ = false;`
3. `src\motor_axis.cpp`：
   - `StartHome()`：开始回零时记下 `home_start_ = Position();`，并把 `home_overrun_` 置为 false。
   - `Update()` 的 `Seeking` 分支：一进分支、算指令之前，加上
     ```cpp
     // 联轴器打滑或者没碰到挡块时，电机一直转、判不成堵转；走过上限就锁存故障。
     if (std::fabs(Position() - home_start_) > config_.home_seek_limit) {
         home_overrun_ = true;
         MarkFault();
         return 0;
     }
     ```
   - `ClearFault()`：把 `home_overrun_` 置为 false。
   - 别的不动。
4. `src\safety.hpp`：`SafetyFault` 在 `NoBottom` 后面加 `HomeOverrun`。`src\safety.cpp`：`Safety::Message` 加 `case SafetyFault::HomeOverrun: return "回零走完全程也没碰到头";`。
5. `src\hw\launcher_hw.cpp`：
   - `PitchConfig()`：软限位上端从 `kPitchTop` 改成 `kPitchTopLimit`（回零位置仍是 `kPitchTop`）；回零速度从 `kHomeSpeed` 改成 `kPitchHomeSpeed`；再加一行 `config.home_seek_limit = kPitchHomeSeekLimit;`。
   - `RefreshState()`：轴是 `HomeState::Failed` 时，`HomeOverrun()` 为真就锁存 `SafetyFault::HomeOverrun`，否则照旧锁存 `SafetyFault::InvalidInput`。
6. `test\host_test.cpp`，在 `TestAxis()` 里 `"axis waits its own home stall time before taking the stop"` 那条后面加：
   - 一条轴用 `SliderConfig()`，把 `home_seek_limit` 设成 0.010，开始回零。每拍 1 ms，反馈按回零方向、按回零速度一直往前走（计数每拍加 `kSliderHomeSpeed * counts_per_unit * 0.001`，转速按同样的速度换算成转子 rpm），一直不堵转：
     - 走到约 9 mm 时还是 `Seeking`，`HomeOverrun()` 为假；
     - 走到约 12 mm 以前变成 `Failed`，`HomeOverrun()` 为真，那一拍 `Update` 输出 0；
     - `ClearFault()` 以后 `HomeOverrun()` 为假，状态是 `Idle`。
     - 提示文字：`"homing that runs past its seek limit without a stall latches an overrun"`。
   - 同样的走法，用默认的 `SliderConfig()`（不设上限）走 12 mm 以后还是 `Seeking`。提示文字：`"axes without a seek limit keep homing as before"`。
   - 在 `"pitch bottom keeps 5 mm from the far end"` 那条后面加：`Check(Near(kPitchTopLimit, kPitchTop - 0.005, 1e-9) && kPitchTopLimit > kPitchBottom, "pitch top soft limit keeps 5 mm under the stop");`
   - 原来那条 `"pitch uses a third of its 6000 N screw rating and homes with its full limit"` 改成：`Check(Near(kPitchScrewForce, 6000.0) && Near(kPitchScrewForce * kPitchLoadMargin, 2000.0, 1e-6) && Near(kLoadMargin, 0.8) && Near(kPitchHomeCurrentFraction, 0.5, 1e-9), "pitch uses a third of its 6000 N screw rating and homes at half of it");`
   - 在它后面加：`Check(Near(kPitchTopForce, 1000.0) && std::abs(static_cast<int>(kPitchCurrentCap * kPitchHomeCurrentFraction) - 869) <= 1, "pitch pushes its top stop with at most 1000 N");`
   - 在 `"only the pitch waits longer to start homing"` 那条后面加：`Check(Near(kPitchHomeSpeed, 0.005) && Near(kHomeSpeed, 0.01) && Near(kPitchHomeSeekLimit, kPitchTravel + 0.010, 1e-9) && kPitchHomeSeekLimit / kPitchHomeSpeed < kHomeTimeout, "pitch homes at half speed and gives up after its full travel before the home timeout");`
   - 除了上面点名要改的那一条，原来的检查都不动；如果有别的旧检查因为这次的改动失败，先停下写汇报，不要改它。
7. `README.md`：
   - pitch 那段里“pitch 下端软限位留了 5 mm（`kPitchEndMargin`）”改成：上下端软限位都离机械端留 5 mm；上端软限位 `kPitchTopLimit` 是 95 mm，平常点动不碰上端挡块，只有回零去顶，回零后退到 95 mm。
   - 同一段加一句 2026-10-22 的说明：联轴器在 10 mm/s 撞挡块时打滑过（已修），pitch 回零和没回零的点动放到 5 mm/s（`kPitchHomeSpeed`）；回零走过 190 mm（`kPitchHomeSeekLimit`）还没堵转，就报“回零走完全程也没碰到头”，故障复位后要重新回零；别的轴回零不设距离上限。
   - 把“回零时仍用满上限顶住上端 1 s”和“没回零时点动和回零都用满上限（`kPitchHomeCurrentFraction=1.0`）”改成：用户说设计上顶到上端的力不能超过 1000 N（联轴器会打滑），所以回零和没回零的点动按 1000 N 限流，约 869（`kPitchTopForce`，`kPitchHomeCurrentFraction=0.5`），回零时按这个电流顶住上端 1 s；回零以后的点动不碰挡块，用满上限 1738。869 在离上端 2 cm 以内能往上走，再往下可能推不动，会在半路误判成到头，所以回零前 pitch 要放在上端附近。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.hpp`、`src\motor_axis.cpp`、`src\safety.hpp`、`src\safety.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~7 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 大约是 18737。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "pitch 回零限到 1000 N、放慢到 5 mm/s，上端软限位留 5 mm，回零走过全程报故障"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - `xrobot setup` 偶尔连 GitHub 报 502 这类临时网络错误，可以原样重跑一次；还不行就停下写汇报。不要手改 `xrobot.lock`。
   - `git -C Modules\local\DartLauncher log --oneline -1` 必须是第 5 步的提交。不是的话就停下写汇报，不要编译，不要自己去副本里 fetch 或 checkout。
   - `build\debug\DevC.elf` 的修改时间应该是这次的。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西，或者要改 `MotorAxis` 里别的行为
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；`Seeking` 分支新加那几行和 `PitchConfig()` 改完后的原文
- 新增检查的提示文字；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
