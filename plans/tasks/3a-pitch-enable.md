# 任务 3a：接上 pitch 电机（CAN2，电调 ID 3）

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 1b66853），主机测试现在是 `PASS: 18719 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD aba4b85）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。`User\xrobot_main.hpp` 是 `xrobot gen` 生成的，不进 git。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-20 用户把 pitch 滑台的 M3508 接到了 CAN2，电调 ID 设成 3，反馈 ID 是 0x203，也就是 515。配置里 pitch 还是默认的 0x201（513），`kPitchInstalled` 还是 false。
- 这次只把 pitch 接进来：在线检测、点动、回零、软限位、电流输出，状态行显示 pitch 的电流指令。目标预置位和瞄准以后再做。电机方向还没在车上试过，`reverse` 先不动。
- pitch 下端软限位现在正好在回零点下面 `kPitchTravel` 180 mm，实际行程没量过。先留 5 mm，免得点动到底撞上机械端。

## 要改的地方
1. `xrobot-local\DartLauncher\src\dart_params.hpp`：
   - `kPitchInstalled` 改成 true，注释改成：`// 2026-10-20 接上：CAN2，电调 ID 3（反馈 0x203）；方向和行程待实车确认。`
   - 在 `kPitchBottom` 前面加：`constexpr double kPitchEndMargin = 0.005; // m，下端软限位离机械端留 5 mm，实测行程以后再改，待标定。`
   - `kPitchBottom` 改成 `kPitchTop - kPitchTravel + kPitchEndMargin`。
   - 别的 pitch 常量（导程、电流上限、PID、回零速度、点动速度、到位范围）都不动。
2. `xrobot-local\DartLauncher\src\hw\launcher_hw.hpp`、`launcher_hw.cpp`：
   - 监控快照里照 `slider_current` 的写法加 `pitch_current`：装了 pitch 时取 `pitch_.Current()`，没装时是 0。
   - 状态打印的第 5 行，在 `pi=%.2fmm%s` 后面加 ` pI=%d`，打 pitch 的电流指令原始值。其他字段和顺序都不动。
3. `xrobot-local\DartLauncher\test\host_test.cpp`：
   - `Check(!kPitchInstalled, "pitch is not installed for stage 2e");` 改成 `Check(kPitchInstalled, "pitch is installed on CAN2 ID 3 since 2026-10-20");`
   - 紧跟着加一条：`Check(Near(kPitchBottom, kPitchTop - kPitchTravel + 0.005, 1e-9) && kPitchBottom < kPitchTop, "pitch bottom keeps 5 mm from the far end");`
   - 其他检查都不动。如果有别的检查因为 pitch 装上了而失败，先停下写汇报，不要改。
4. `xrobot-local\DartLauncher\README.md`：
   - 设备表里 pitch 那一行的 `M3508 / 0x201` 改成 `M3508 / 0x203（电调 ID 3）`。
   - 开关表里 `kPitchInstalled` 的值改成 `true`。
   - 写明：装了 pitch 以后，上膛和发射都要求 pitch 也回零了（左拨杆上，拨轮往上拨到底保持 2 s）；pitch 没回零时只能往回零方向慢慢点动；下端软限位留 5 mm。
   - 状态行说明里加上 `pI=`：pitch 电流指令原始值，上限是 `kPitchCurrentCap`。
5. `dart-fw\User\RobotConfig\artisans_dart.yaml`：
   - `pitch_motor` 里的 `feedback_id: 513` 改成 `feedback_id: 515`，`reverse` 还是 `false`。
   - `dart_launcher` 里的 `pitch_id: 513` 改成 `pitch_id: 515`。
   - 别的都不动：yaw 还是 514，摆臂还是 517，两台滑块不动。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`User\RobotConfig\artisans_dart.yaml`、`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~4 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: 18720 checks`（多了 1 条）。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "接上 pitch：CAN2 ID 3，下端软限位留 5 mm，状态行显示 pitch 电流"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 改 dart-fw 的配置（第 5 条），然后更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   Select-String -Path User\xrobot_main.hpp -Pattern '515'
   cmake --build --preset debug
   ```
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - `git -C Modules\local\DartLauncher log --oneline -1` 必须是第 5 步的提交。不是的话就停下写汇报，不要编译，不要自己去副本里 fetch 或 checkout。
   - `Select-String` 应该能在 `pitch_motor` 的参数和 `dart_launcher` 的构造参数里各找到一处 515。找不到就停下写汇报。
   - `build\debug\DevC.elf` 的修改时间应该是这次的。
   - 如果 `xrobot setup` 联网失败，就停下写汇报。不要手改 `xrobot.lock`。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock User/RobotConfig/artisans_dart.yaml`，再单独运行 `git commit -m "pitch 接到 CAN2 ID 3，更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交，或者生成的文件里找不到 515
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么
- 主机测试的 `PASS` 数；第 4 步的结果；第 6 步 `Select-String` 找到的两行
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
