# 任务 2h：滑块电机方向取反，补几处小问题

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD ada2c06）
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS
  - `src\hw\`：LibXR 适配层
  - `test\host_test.cpp`：主机测试
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 4fb27f6）
  - 机器人配置：`User\RobotConfig\artisans_dart.yaml`
  - 模块会从 `xrobot-local\DartLauncher` 检出到 `Modules\local\DartLauncher`。不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 2026-10-14 实车：手动模式下，右摇杆往上推，滑块实际往下走。
- 滑块没回零时，固件只接受往回零端点动，也就是固件坐标的 +，按设计应该是向上。它能动，说明固件认为自己在往 + 走；打印里两台滑块的位置也确实一起在增加。可滑块实际往下走，说明两台电机的方向整体反了。两台是一起反的，所以左右镜像没错。
- 不改的话，回零会往下压，后面的探测和保持都会跟着反。皮筋已经装上了，最后会走火。

## 要改的地方
1. `dart-fw\User\RobotConfig\artisans_dart.yaml`：
   - `id: slider_left` 那一项：`reverse: true` 改成 `reverse: false`。
   - `id: slider_right` 那一项：`reverse: false` 改成 `reverse: true`。
   - 只改这两处。ID、总线、DartLauncher 的参数都不动。
2. `xrobot-local\DartLauncher\README.md`：
   - 接线表里左右滑块的 reverse 跟着改，加一句：“2026-10-14 实车确认：原来的设置下固件的 + 方向是向下，两台一起取反。”
   - 加一条规则：任何轴第一次点动时方向和预期相反，就改对应 RMMotor 实例的 `reverse`，不要改 `MotorAxis` 的正负号。成对的电机（两台滑块）要一起改。
3. `src\hw\launcher_hw.cpp` 的 `OnMonitor()`，第 4 行（`Dart sw R=... L=...`）：
   - 拨杆改成打印字母：`Switch::Up` 打 `U`，`Switch::Middle` 打 `M`，`Switch::Down` 打 `D`。`Switch` 定义在 `src\mode_logic.hpp`，值分别是 1、0、−1。用 `%s` 传字符串。
   - 把那一行的注释改对。现在写的是“1 上 2 中 3 下”，这是错的。
4. 第 4 行现在直接读成员变量 `remote_input_`。它由控制线程写，`OnMonitor()` 却在另一个线程里不加锁读，可能读到写了一半的值。改法：
   - `launcher_hw.hpp` 的 `MonitorFeedback` 里加一个字段 `RemoteInput remote{};`。
   - `Tick()` 末尾在 `monitor_mutex_` 锁里填快照的地方，加一句 `monitor_feedback_.remote = remote_input_;`。
   - `OnMonitor()` 改成读 `feedback.remote`。
5. 耗时统计 `RecordControlTiming`：快照现在拷的是当前窗口算到一半的值，窗口一清零，打印的数字就会跳。改法：
   - 加三个成员变量 `last_tick_avg_us_`、`last_tick_max_us_`、`last_late_wakes_`，类型 `std::uint32_t`，初值 0。
   - 每满 1000 拍时，先把这个窗口的平均耗时、最大耗时、迟唤醒次数存进这三个变量，再清零。
   - 快照里的 `tick_avg_us`、`tick_max_us`、`late_wakes` 改成从这三个变量拷。第一个窗口满之前打印 0。
6. 两处小修：
   - `launcher_hw.cpp` 里，`kCrankInstalled` 的 else 分支中，`monitor_feedback_.crank_angle = 0;` 这一行缩进不对，改对。
   - `src\hw\remote_port.cpp` 里，`uart_.Read(...)` 返回值不是 `OK` 的那个分支：先调 `static_cast<void>(self->uart_.read_port_->ClearQueuedData());`，再调 `framer_.Reset();`，和解析失败的分支一样。

## 允许改的文件
- `dart-fw\User\RobotConfig\artisans_dart.yaml`：只改第 1 条里的那两处。
- `xrobot-local\DartLauncher\`：`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`src\hw\remote_port.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。
- 其他文件都不碰。纯逻辑层（`src\*.hpp`、`src\*.cpp`）和测试都不用改。

## 步骤
1. 改代码（第 2~6 条）。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 不少于 17509。
3. 框架引用检查。下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 在 DartLauncher 仓库提交：先 `git add -A`，再 `git commit -m "监控快照和耗时统计修正"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
5. 改 `artisans_dart.yaml`（第 1 条）。
6. 更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - 如果 `xrobot setup` 联网失败（比如 GitHub SSL 握手失败），就停下写汇报，说明停在这一步。不要手改 `xrobot.lock`，也不要自己去 `Modules\local\DartLauncher` 里 fetch 或 checkout。用户会在自己的终端里跑这三条命令。
7. 在 dart-fw 提交：先 `git add User/RobotConfig/artisans_dart.yaml xrobot.lock`，再 `git commit -m "滑块电机方向取反"`。提交后同样核对。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件
- 主机测试的通过数、框架检查的结果、固件编译的结果、`arm-none-eabi-size build\debug\DevC.elf` 的输出
- `artisans_dart.yaml` 里 `slider_left`、`slider_right` 两项的原文（每项 8 行左右）
- 两个仓库的提交号；如果中途停下了，写停在哪一步
