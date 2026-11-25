# 任务 2r：滑块停靠位上移到回零点下面 4 mm

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD f481755），主机测试现在是 `PASS: 17830 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 618c4a2）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。xrobot 只认 DartLauncher 的 `main`；上次提交在新分支上，固件就没更新。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 滑块回零时往上顶到防撞块，那里记为 `kSliderTop` 0.370，再退 3 mm 停在 0.367。
- 停靠位 `kSliderPark` 现在是 0.360，滑块每次上膛、退膛以后停在防撞块下面将近 1 cm。2026-10-19 用户要求：滑块要停到最顶端。
- 停靠位不能贴着防撞块：贴上去以后位置环一直往上顶，电流会一路涨到上限，电机一直发热。所以停在回零点下面 4 mm，也就是 0.366，比回零后停的 0.367 低 1 mm。
- `kFireSliderMin = kSliderPark - 0.005` 和 `Safety::SliderMustHold` 里的 `kSliderPark - 0.01` 都跟着停靠位走，算法不改：发射条件变成滑块不低于 361 mm，托住判断的门槛变成 356 mm，都比原来更严。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kSliderPark`：0.360 改成 0.366，这一行末尾加注释：`// m，2026-10-19 用户要求停到最顶端：回零点（防撞块）下面 4 mm，不贴着防撞块，免得电机一直顶着发热。`
   - `kFireSliderMin` 那行注释里的“355 mm”改成“361 mm”。算法 `kSliderPark - 0.005` 不改。
   - 别的常量都不动。
2. `test\host_test.cpp`：
   - `Check(Near(kFireSliderMin, 0.355), "fire slider minimum is 355 mm");` 改成：`Check(Near(kFireSliderMin, 0.361) && Near(kSliderPark, 0.366), "slider parks 4 mm below the bumper and fires from 361 mm");`
   - 第 417 行允许发射的那组 `{0.370, 0.360, 0.355}` 改成 `{0.370, 0.367, kSliderPark, kFireSliderMin}`，下面提示文字里的“355 mm”改成“361 mm”。边界直接用常量本身，免得浮点误差。
   - 第 422 行拦住发射的那组里，`0.354, 0.350` 改成 `kFireSliderMin - 0.0005, 0.355`，其余几个不动；提示文字里的“355 mm”改成“361 mm”。
   - 其他检查都不动。
3. `README.md`：
   - 第 112、113、123 行里的“355 mm”都改成“361 mm”。
   - 第 147 行“6. 滑块回到停靠位 `kSliderPark`。”改成：“6. 滑块回到停靠位 `kSliderPark=0.366 m`，在回零点（防撞块）下面 4 mm，不贴着防撞块，免得电机一直顶着发热。”

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`。不是就停下写汇报。
2. 改代码（第 1~3 条）。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`。停靠位变了，有的仿真循环步数会变，N 不一定正好是 17831；只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "滑块停靠位上移到回零点下面 4 mm"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 分支不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
