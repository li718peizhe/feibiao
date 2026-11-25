# 任务 3f：没回零的点动松手清积分，pitch 回零多等起步

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD d644014），主机测试现在是 `PASS: 18723 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD fbbd4b8）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-21 车上：pitch 没回零时往上点动，指令 869，电调回报的实际电流 870~1180，转子最高 2013 rpm，往上走了约 4 mm 顶到上端。电调和电机都正常。
- 问题 1：顶到上端、摇杆松回 0 以后，指令还一直是 869，顶了 8 秒，直到切无力。原因在 `MotorAxis::Update` 的点动分支：只有 `Homed()` 时速度为 0 才 `loop_.Reset()`；没回零时速度环的积分在堵转时涨满，松手后误差是 0，积分不会退，输出就一直顶在上限。滑块没回零时点动也一样。
- 问题 2：pitch 满电流下起步慢，往上要过一会儿才转起来。回零时 `|指令| ≥ 限流` 且 `|rpm| < kHomeStallRpm` 持续 `kHomeStallTime`（0.2 s）就当作顶到头，2026-10-20、10-21 两次都在原地或半路误判成了零点。pitch 单独多等一会儿。
- 别的轴回零的判定照旧。

## 要改的地方
1. `src\motor_axis.cpp`：`MotorAxis::Update` 的点动分支里，把“速度为 0 就 `loop_.Reset()`”从 `if (Homed())` 里挪出来：先按原来的写法算出 `velocity`（回零后照旧夹软限位），然后不管回没回零，只要 `velocity == 0` 就 `loop_.Reset()`。别的不动。
2. `src\motor_axis.hpp`：`AxisConfig` 在 `home_current_fraction` 后面再加一个成员：`double home_stall_time = kHomeStallTime;`，注释写“回零时满限流且不转持续多久才算顶到头，单位 s”。
3. `src\motor_axis.cpp`：回零 `Seeking` 里判堵转的 `kHomeStallTime` 改成 `config_.home_stall_time`。
4. `src\dart_params.hpp`：在 `kPitchHomeCurrentFraction` 后面加：`constexpr double kPitchHomeStallTime = 1.0; // s，2026-10-21：pitch 满电流下要过一会儿才转起来，0.2 s 会在原地误判成到头，待标定。`
5. `src\hw\launcher_hw.cpp`：`PitchConfig()` 里再加一行 `config.home_stall_time = kPitchHomeStallTime;`。
6. `test\host_test.cpp`：
   - 加一条：一个没回零的轴（用测试里已有的 `SliderConfig()`），反馈转速一直是 0，往回零方向点动若干拍，输出顶到 `CurrentLimit()`；然后 `Jog(0)`，下一拍 `Update` 的输出是 0。提示文字：`"unhomed jog release clears the wound-up speed integral"`。
   - 加一条：同一个配置把 `home_stall_time` 设成 1.0，开始回零，反馈转速一直是 0：0.5 s 时还在 `Seeking`；1.0 s 多一点就进了 `Retreating`。提示文字：`"axis waits its own home stall time before taking the stop"`。
   - 加一条：`Check(Near(kPitchHomeStallTime, 1.0) && Near(kHomeStallTime, 0.2), "only the pitch waits longer to start homing");`
   - 原来的检查都不动；如果有旧检查因为第 1 条失败，先停下写汇报，不要改它。
7. `README.md`：写明松开摇杆（点动速度为 0）时速度环会清零，回没回零都一样；pitch 回零要满电流不转持续 1 s 才算顶到头，别的轴还是 0.2 s。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\motor_axis.hpp`、`src\motor_axis.cpp`、`src\dart_params.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
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
   应该打印 `PASS: N checks`，N 大约是 18726。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "没回零的点动松手清积分，pitch 回零等 1 s 起步"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - `git -C Modules\local\DartLauncher log --oneline -1` 必须是第 5 步的提交。不是的话就停下写汇报，不要编译，不要自己去副本里 fetch 或 checkout。
   - `build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败，就停下写汇报。不要手改 `xrobot.lock`。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；点动分支改完后的那几行代码原文
- 新增的三条检查的提示文字；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
