# 任务 3e：状态打印加一行 pitch 诊断（电调回报的实际电流、转速、温度）

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD baf7cac），主机测试现在是 `PASS: 18723 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 79ad90c）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-20、10-21 车上：pitch（CAN1，电调 ID 3）在线，电调绿灯闪 3 下正常，线没松，断电后手能推动。可固件发出 ±869 的满电流指令（状态行 `pI=±869`），pitch 往上往下都不动，电机也没声音。
- 现在只能看到我们发出去的指令，看不到电调实际做了什么。C620 的反馈帧里有实际转矩电流和转子转速，`RMMotor` 已经解码成 `Motor::Feedback` 的 `torque`（转子力矩 N·m）和 `velocity`（转子 rpm）。把它们打印出来，就能分清是“电调送了电流但推不动”，还是“电调没送出电流”。
- 温度不用打：用户说 RM 电机实际不回传温度，日志里也没有高温报警。

## 要改的地方
1. `src\hw\motor_port.hpp`、`motor_port.cpp`：
   - `MotorPort::Update` 里，拿到新反馈帧时（现在读 `motor_.GetFeedback()` 的地方），顺便把 `feedback.torque`、`feedback.velocity` 存进两个成员。
   - 加两个 const 读取函数，比如 `FeedbackTorque()`、`FeedbackRpm()`，返回最近一次存下的值，单位照原样：N·m、rpm。
   - 不改离线判定、计数和控制逻辑。
2. `src\hw\launcher_hw.hpp`、`launcher_hw.cpp`：
   - 监控快照里照 `pitch_current` 的写法，加 pitch 的实际力矩、转速两个字段，装了 pitch 时从 `pitch_` 读，没装时是 0。
   - 在 `launcher_hw.cpp` 的匿名命名空间里加一个常量：`constexpr double kM3508RotorTorqueConstant = 0.0156224; // N·m/A，转子端，和 RMMotor.hpp 里 KGetTorque() 的 M3508 值一致。`
   - 状态打印在第 6 行之后、`kPitchInstalled` 为 true 时多打一行（前面同样等 `Thread::Sleep(20)`）：
     `LibXR::STDIO::Printf<"Dart pitch cmd=%d act=%d rpm=%.0f\r\n">(...)`
     - `cmd`：我们发出去的电流指令原始值（就是第 5 行的 `pI=`）。
     - `act`：电调回报的实际电流，换成同样的原始单位：`力矩 / kM3508RotorTorqueConstant / 20.0 * 16384`，四舍五入成整数。
     - `rpm`：转子转速。
   - 这一行没有 `%s`，不会碰到空字符串截断的问题；其他行一个字都不动。
3. `README.md`：状态行说明里加上第 7 行（只有装了 pitch 才打）：`cmd` 是指令，`act` 是电调回报的实际电流（同一原始单位，上限 869），`rpm` 是转子转速。
4. 测试不用改（都是适配层）。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\hw\motor_port.hpp`、`src\hw\motor_port.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰，`RMMotor`、LibXR 也不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~3 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该还是 `PASS: 18723 checks`。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src README.md`，再单独运行 `git commit -m "状态打印加 pitch 诊断行：指令、实际电流、转速、温度"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 主机测试没有 `PASS`
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；新加那一行的完整格式串（应该只有 cmd、act、rpm 三项）
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
