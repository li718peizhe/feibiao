# 任务 2q：顶上装了防撞块，滑块锁止几何整体上移约 20 mm

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 8215a6e），主机测试现在是 `PASS: 18269 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 5beebc4）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 滑块坐标以回零点为准：回零时滑块往上顶到不动，那里记为 `kSliderTop` 0.370。
- 2026-10-16 以后，机械在滑块顶上加了一块 35 mm 的防撞块。回零时滑块停在防撞块上，回零点比 10-16 低了约 20 mm，滑块所有读数都偏大约 20 mm。
- 实车：2026-10-18、10-19 两次上膛，发射机构都已经压到底（底部越过爪子约 1 cm，和 10-16 一样），滑块读数左右平均却只有 −0.2992、−0.2999 m；10-16 同一个底读 −0.3195。`kCockPosition` −0.318 到不了，慢压一直顶在电流上限，步骤超时，舵机没有锁。
- 用户定：防撞块留着，按实测把和锁止有关的几何整体上移。终点要比两次实测的底都高至少 1.5 mm，所以硬底取两次里浅的那次（−0.2992，取 −0.2990）。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kSliderTravel`：0.790 改成 0.770，注释改成：`// m，2026-10-16 机械给的最大行程 790 mm；之后顶上加了 35 mm 防撞块，回零点按实测低约 20 mm，按 770 mm 算。`
   - `kTopGap`：0.130 改成 0.110。它上面两行“2026-10-16 实测”的注释保留不动，在 `kTopGap` 这一行末尾加注释：`// m，10-16 实测 130 mm；装防撞块后回零点低约 20 mm，按 110 mm 算。`
   - `kCockBottom`：−0.3195 改成 −0.2990，注释改成：`// m，装防撞块后 2026-10-18、10-19 两次压到底（底部越过爪子约 1 cm）读数左右平均 −0.2992、−0.2999，取浅的那次，让终点离两次的底都至少 1.5 mm；10-16 没装防撞块时是 −0.3195。`
   - `kLatchSlider` 那行注释里的“约 −0.314”改成“约 −0.294”。
   - 别的常量不动：`kSliderTop`、`kSliderPark`、`kFireSliderMin`、`kProbeMargin`，以及 `kCockPosition = kCockBottom + 0.0015` 这个算法都不改。改完以后应该是：`kLatchSlider` −0.294、`kCockPosition` −0.2975、`kProbePosition` −0.264、`kSliderBottom` −0.400。
2. `test\host_test.cpp`：
   - `Check(Near(kLatchSlider, -0.314, 1e-9), "latch slider follows measured geometry");` 里的 −0.314 改成 −0.294。
   - `Check(Near(kCockBottom, -0.3195, 1e-9) && Near(kCockPosition, -0.318, 1e-9), ...);` 里的两个数改成 −0.2990 和 −0.2975。
   - 紧跟着加一条：`Check(Near(kTopGap, 0.110, 1e-9) && Near(kSliderTravel, 0.770, 1e-9) && Near(kProbePosition, -0.264, 1e-9), "geometry follows the top bumper");`
   - 其他检查都不动。
3. `README.md` 第 135~146 行：
   - 第 135 行改成：“2026-10-16 实测滑块最大行程 790 mm。之后顶上加了 35 mm 防撞块，回零时滑块停在防撞块上，按实测回零点低约 20 mm，行程按 770 mm 算：顶部坐标 `kSliderTop=0.370 m`，底部为 `−0.400 m`。换防撞块或者改顶上的结构，所有滑块位置都要重新量。”
   - 第 136 行的 `−0.318 m` 改成 `−0.2975 m`。
   - 第 137~139 行改成：“滑块在最顶端时，压面到顶部发射机构被压面 10-16 量的是 130 mm，装防撞块后按 110 mm 算（`kTopGap`）；被压面到夹爪 630 mm，夹爪卡住的底部比被压面低 76 mm，因此锁止位置 `kLatchSlider=0.370−0.110−(0.630−0.076)=−0.294 m`。”
   - 第 140 行不动。
   - 第 141 行改成：“实测发射机构硬底：10-16 是 −0.3195 m；装防撞块后 10-18、10-19 两次压到底读 −0.2992、−0.2999 m，取浅的那次 `kCockBottom=−0.2990 m`，终点 `kCockPosition=−0.2975 m`，离底至少 1.5 mm。”
   - 第 145 行的“−0.284 m”改成“−0.264 m”；第 146 行的“−0.318 m”改成“−0.2975 m”。

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
   应该打印 `PASS: N checks`。几何变了，有的仿真循环步数会变，N 不一定正好是 18270；只要是 `PASS` 就行，把 N 写进汇报。
3. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "顶上装防撞块后锁止几何整体上移约 20 mm"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
