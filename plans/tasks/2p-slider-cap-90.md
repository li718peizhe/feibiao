# 任务 2p：滑块电流上限放到皮带额定的 90%

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD d3639fd），主机测试现在是 `PASS: 18268 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 2a84548）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- `src\dart_params.hpp` 里，滑块的电流上限是按每根皮带 `kBeltForce` 220 N × `kLoadMargin` 0.8 = 176 N 算的：`kSliderCurrentCap` 约 9178。
- 2026-10-16 实车上膛，慢压到上膛位时电流正好顶到 9178，力已经没有余量了。
- 用户定：只把滑块的上限放到皮带额定的 90%，每根约 198 N，上限约 10326。以后会装拉力传感器，再核对实际拉力。
- `kLoadMargin` 还被 yaw、pitch 的丝杆上限共用，它们不改，所以给滑块单独加一个余量。

## 要改的地方
1. `src\dart_params.hpp`：
   - 在 `kLoadMargin` 那一行后面加：
     `constexpr double kSliderLoadMargin = 0.9; // 2026-10-16 用户定：0.8 时皮筋拉满正好用满上限，滑块单独放到皮带额定的 90%；以后装了拉力传感器再核对。`
   - `kSliderCurrentCap` 里的 `kLoadMargin` 换成 `kSliderLoadMargin`。
   - `kLoadMargin`、`kYawCurrentCap`、`kPitchCurrentCap` 都不动。`kLatchCheckCurrent` 还是 `kSliderCurrentCap * 0.25`，不动。
2. `test\host_test.cpp`：
   - `Check(std::abs(kSliderCurrentCap - 9178) <= 1, "slider current cap");` 里的 9178 改成 10326。
   - 紧跟着加一条：`Check(Near(kSliderLoadMargin, 0.9) && Near(kLoadMargin, 0.8), "only the slider load margin is raised");`
   - yaw、pitch 上限那两条检查（2829、695）不动，它们本来就该照旧通过。
3. `README.md` 第 175 行附近，“上限是 `kSliderCurrentCap`，约 9178”改成：
   “上限是 `kSliderCurrentCap`，约 10326，也就是皮带额定 220 N 的 90%，每根约 198 N。2026-10-16 按 80% 算时，上膛慢压正好用满上限，所以单独放宽了滑块；以后装了拉力传感器再核对。”

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 改代码（第 1~3 条）。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 应该是 18269（多了 1 条）。只要是 `PASS` 就行，把 N 写进汇报。
3. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "滑块电流上限放到皮带额定的 90%"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 3 步有输出
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件
- 主机测试的 `PASS` 数；第 3 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
