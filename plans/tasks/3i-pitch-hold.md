# 任务 3i：pitch 回零后松手按位置保持并算到位，停机也保持

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 182b74b），主机测试现在是 `PASS: 18737 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 4ae3a98）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-22 车上：pitch 回零、上下软限位、全程上下都正常了。
- 发射流程里“瞄准：等 yaw / pitch 到位”这一步等 `Aimed()`，pitch 要 `Settled()`：`active_` 为真、离终点 `end_` 不到 0.5 mm、几乎不动。现在有两个问题：
  1. 手动点动不改 `end_`。回零以后 `end_` 是 95 mm，手动把 pitch 挪到别的地方，就永远到不了位。
  2. 发射要切到自动模式。离开手动模式时 `StopMotors()` 会对 pitch 调 `Stop()`，`active_` 变成假，pitch 在哪都算没到位。所以现在装了 pitch 就发射不了：会在瞄准这一步等满 `kAimTimeout`（20 s），报步骤超时。
- 另外，回零以后点动松手，现在只是把速度目标设成 0（每拍清积分，只剩 P），没有位置环，pitch 靠摩擦不往下溜。
- 改法：
  - 给轴加一个“松手保持”的配置，只有 pitch 打开。回零以后点动松手（速度为 0），就把目标设成松手时的位置，用位置环保持，`Settled()` 以它为终点，手动瞄好就算到位。
  - 遥控每 10 ms 都会调一次 `Jog(0)`，所以只在点动中第一次收到 0 时取位置，后面重复的 0 不能再重取，否则 pitch 往下溜了也不会拉回来。
  - `StopMotors()` 对回零以后的 pitch 也改成按当前位置保持，不再 `Stop()`。不使能时（无力、有故障）`Update` 照样输出 0，重新使能时 pitch 原地不动，算到位。没回零的 pitch 照旧 `Stop()`。
- 滑块、yaw、摆臂的行为都不变。

## 要改的地方
1. `src\motor_axis.hpp`：
   - `AxisConfig` 在 `home_seek_limit` 后面加：`bool hold_on_release = false; // 回零以后点动松手（速度为 0）时，按松手的位置保持并算到位；默认照旧只把速度目标设成 0。`
   - `MotorAxis` 公开部分，在 `Jog` 后面加一个函数，注释写“回零后把目标设成当前位置（软限位夹紧）并清环路，按位置环保持，Settled 以它为终点；没回零返回 false”：`bool HoldPosition();`
2. `src\motor_axis.cpp`：
   - 新增 `HoldPosition()`：没回零返回 false；回零了就 `target_ = end_ = Clamp(Position(), config_.minimum, config_.maximum);`，`rate_`、`jog_` 置 0，`jogging_` 置假，`active_` 置真，`loop_.Reset();`，返回 true。
   - `Jog()`：在“没回零时拒绝反向点动”那段之后、给 `jog_` 赋值之前，加上
     ```cpp
     // 松手保持的轴：回零以后只在点动中第一次收到 0 时取位置，遥控每 10 ms 重复的 0 不再重取。
     if (Homed() && config_.hold_on_release && velocity == 0) {
         if (jogging_ || !active_) {
             return HoldPosition();
         }
         return true;
     }
     ```
   - 别的不动。
3. `src\hw\launcher_hw.cpp`：
   - `PitchConfig()` 里加一行 `config.hold_on_release = true;`。
   - `StopMotors()` 里 `pitch_.Axis().Stop();` 换成：
     ```cpp
     // pitch 回零以后停机也按当前位置保持：不使能时照样输出 0，重新使能时原地不动，算到位。
     if (!pitch_.Axis().HoldPosition()) {
         pitch_.Axis().Stop();
     }
     ```
   - 别的不动。
4. `test\host_test.cpp`，在 `TestAxis()` 里 `"axes without a seek limit keep homing as before"` 那条后面加（都用 `SliderConfig()`，用已有的 `FinishHome()` 回零，每拍 1 ms）：
   - 一条打开 `hold_on_release` 的轴，回零后反馈放在 0.25 m、转速 0；`Jog(0.05)` 走一拍 `Update`；再 `Jog(0)`。检查 `Target()` 接近 0.25，并且 `Settled(kPitchTolerance, kPitchVelocityTolerance)` 为真。提示文字：`"jog release holds the release position and counts as settled"`。
   - 接着把反馈挪到 0.252 m、转速 0，再 `Jog(0)`：`Target()` 还是接近 0.25（没有重取），这一拍 `Update` 的输出小于 0（往回拉）。提示文字：`"repeated zero jogs keep the first release target and pull back against drift"`。
   - 一条不打开 `hold_on_release` 的轴，同样回零、反馈 0.25 m、`Jog(0.05)` 一拍、`Jog(0)`：这一拍 `Update` 输出 0，`Settled(kPitchTolerance, kPitchVelocityTolerance)` 为假。提示文字：`"axes without hold_on_release keep the old zero-speed jog release"`。
   - 没回零的轴（有反馈）调 `HoldPosition()` 返回假。提示文字：`"unhomed axis refuses to hold its position"`。
   - 回零后反馈放在 `kSliderTop + 0.001`（超过软限位上端），`HoldPosition()` 以后 `Target()` 接近 `kSliderTop`。提示文字：`"holding past a soft limit holds at the limit"`。
   - 原来的检查都不动；如果有旧检查因为这次的改动失败，先停下写汇报，不要改它。
5. `README.md`：pitch 那段加一句 2026-10-22 的说明：回零以后点动松手，pitch 按松手的位置用位置环保持，算到位，手动瞄好就能发射；离开手动模式、取消、故障这些停机，回零以后的 pitch 也按当前位置保持（不使能时输出 0）。改之前离开手动模式会让 pitch 停用，发射会在“瞄准：等 yaw / pitch 到位”等满 20 s 超时。滑块、yaw、摆臂不变。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\motor_axis.hpp`、`src\motor_axis.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~5 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 大约是 18742。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "pitch 回零后松手按位置保持并算到位，停机也保持"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 需要改“允许改的文件”以外的东西，或者要改滑块、yaw、摆臂的行为
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；`HoldPosition()`、`Jog()` 新加那段和 `StopMotors()` 改完后的原文
- 新增检查的提示文字；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
