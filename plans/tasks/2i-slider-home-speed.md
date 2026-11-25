# 任务 2i：滑块回零提速到 30 mm/s，未回零的反向点动要停下

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD b0c814c）
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS
  - `src\hw\`：LibXR 适配层
  - `test\host_test.cpp`：主机测试，现在是 `PASS: 17509 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD a5d7a9d）
  - 模块会从 `xrobot-local\DartLauncher` 检出到 `Modules\local\DartLauncher`。不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
1. 滑块没回零时，点动和回零都限在 `kHomeSpeed`，也就是 10 mm/s。用户嫌慢，而且回零走满 710 mm 要 71 s，离 90 s 的回零超时太近。用户定了：只把滑块提到 30 mm/s，走满 24 s。
   - 不能改 `kHomeSpeed` 本身：pitch、yaw 回零也用它。pitch 丝杆导程只有 2 mm，30 mm/s 对应转子约 17000 rpm，M3508 到不了。
   - 滑块是同步带，30 mm/s 对应转子约 290 rpm。回零靠堵转判停，看的是电流顶到 30% 且转子低于 1 rpm 持续 0.2 s，和速度无关。没回零时电流照样限在 30%。
2. `src\motor_axis.cpp` 的 `MotorAxis::Jog()`：轴没回零时，往回零反方向的点动会被拒绝，直接 `return false`，但 `jog_` 和 `jogging_` 都没动。所以上一帧还在往回零方向点动的话，摇杆一下甩到反方向（遥控器约 14 ms 一帧，甩得快会直接跳过 5% 死区），电机会按上一帧的速度接着走，直到摇杆回中。
   - 现在的滑块碰不到：锁止状态未知时，`launcher_hw.cpp` 会先把滑块的反向速度清成 0 再调 `Jog()`。pitch、yaw 装上以后就会碰到。

## 要改的地方
1. `src\dart_params.hpp`：`kHomeSpeed` 那一行改成下面两行。`kHomeSpeed` 的值不变，只改注释。
   ```cpp
   constexpr double kHomeSpeed = 0.01; // m/s，pitch、yaw 用，待标定；转轴需显式提供 rad/s。
   constexpr double kSliderHomeSpeed = 0.03; // m/s，只给滑块用，待标定；pitch 导程只有 2 mm，不能跟着提速。
   ```
2. `src\hw\launcher_hw.cpp` 的 `LauncherHardware::SliderConfig()`：传给 `Axis(...)` 的 `kHomeSpeed` 换成 `kSliderHomeSpeed`。`PitchConfig()`、`YawConfig()` 不动。
3. `test\host_test.cpp`：
   - 测试用的 `SliderConfig()`（第 243 行附近）里的 `kHomeSpeed` 也换成 `kSliderHomeSpeed`，和固件保持一致。
   - `TestAxis()` 开头加一条：
     ```cpp
     Check(kSliderTravel / kSliderHomeSpeed < kHomeTimeout / 2, "slider home fits half timeout");
     ```
   - 紧跟在 `"unhomed axis only jogs toward home"` 那条检查后面，新建一个轴。不要复用已有的 `axis`，免得影响后面的检查：
     ```cpp
     MotorAxis reversed(SliderConfig());
     reversed.Feedback(0, 0);
     Check(reversed.Jog(0.01) && !reversed.Jog(-0.01) && reversed.Update(0.001, true, true) == 0,
           "rejected reverse jog stops previous jog");
     ```
4. `src\motor_axis.cpp` 的 `MotorAxis::Jog()`：在 `if (!Homed() && velocity * config_.home_direction < 0)` 这个分支里，`return false;` 前面加 `jog_ = 0;`，上面加一行注释：拒绝反向点动时停掉上一次的点动，防止摇杆跳过死区后按旧速度继续走。其他分支不动。
5. `src\motor_axis.hpp`：`Jog` 上面的注释后面补一句：未回零时往回零反方向的点动返回 false，并停掉之前的点动。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.cpp`、`src\motor_axis.cpp`、`src\motor_axis.hpp`、`test\host_test.cpp`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。
- 其他文件都不碰。`launcher_hw.cpp` 里只改 `SliderConfig()` 这一处，手动点动那段（锁止状态未知时清反向速度）不动。

## 步骤
1. 先做“要改的地方”第 1~3 条。`motor_axis.cpp`、`motor_axis.hpp` 先不改。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   这一次应该打印 `FAIL: rejected reverse jog stops previous jog`，然后退出。这是预期的，说明新检查能抓到这个问题。如果打印的是别的 `FAIL`，或者直接 `PASS`，就停下汇报。
3. 做第 4、5 条，再跑一次第 2 步的命令。这次应该打印 `PASS: N checks`。
   - N 不一定是 17509 + 2，可能会少一些，这是正常的：`FinishHome()` 里每拍都有一条检查，回零速度变快以后，判出堵转用的拍数可能变少。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查。下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库提交：
   ```powershell
   git add src/dart_params.hpp src/hw/launcher_hw.cpp src/motor_axis.cpp src/motor_axis.hpp test/host_test.cpp
   git commit -m "滑块回零提速到 30 mm/s，未回零反向点动停下"
   ```
   提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同。这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - 用 `git -C Modules\local\DartLauncher log --oneline -1` 确认副本已经是第 5 步的提交。
   - 编译输出里应该能看到重新编译了 `motor_axis.cpp` 和 `launcher_hw.cpp`，`build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败（比如 GitHub SSL 握手失败），就停下写汇报，说明停在这一步。不要手改 `xrobot.lock`，也不要自己去 `Modules\local\DartLauncher` 里 fetch 或 checkout。
7. 在 dart-fw 提交：先 `git add xrobot.lock`，再 `git commit -m "更新 DartLauncher"`。提交后同样核对。`git status --porcelain` 里如果还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 第 2 步打印的不是 `FAIL: rejected reverse jog stops previous jog`
- 第 3 步没有 `PASS`
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件
- 第 2 步主机测试的输出，第 3 步的 `PASS` 数
- 框架检查的结果
- `Modules\local\DartLauncher` 的提交号；固件编译有没有重新编译 `motor_axis.cpp`、`launcher_hw.cpp`；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
