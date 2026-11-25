# 任务 3g：pitch 推力上限从 1000 N 放到 2000 N

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 979df03），主机测试现在是 `PASS: 18730 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD dc67ccf）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 2026-10-22 车上：pitch 电流上限 869（按理想扭矩算的 1000 N），往下满电流只有约 4 mm/s，离上端约 25 mm 时往上完全推不动（指令 869，电调实际电流约 900，转速 0）。电调、电机、固件都正常，是推力不够。
- 用户 2026-10-22 确认：丝杠 BPGF-C7-12-2 实际额定负载 6000 N。原来的 1000 N 太保守。
- 先放到 2000 N，也就是额定的三分之一，按理想扭矩算约 1738（约 2.1 A），实际推力扣掉损耗大约 1400 N。不直接用满 6000 N：回零时会用满上限往上顶住上端 1 s，卡住时整个 pitch 机构都要吃这个力，不只丝杠。
- yaw 的丝杠不一样，`kScrewForce`、`kLoadMargin`、`kYawCurrentCap` 都不动。

## 要改的地方
1. `src\dart_params.hpp`：
   - 在 `kScrewForce` 后面加：`constexpr double kPitchScrewForce = 6000.0; // N，2026-10-22 用户确认 BPGF-C7-12-2 实际额定负载 6000 N。`
   - `kPitchLoadMargin` 改成 `1.0 / 3.0`，注释改成：`// 2026-10-22：按 1000 N 时往上推不动；先用额定的三分之一，约 2000 N，回零时顶住上端也是这个力，待实车确认。`
   - `kPitchCurrentCap` 改成 `CurrentCap(kPitchScrewForce * kPitchLoadMargin, kPitchLead / (2.0 * kPi), 0.3, 16384, 20.0)`。
   - `kPitchHomeCurrentFraction`、`kPitchHomeStallTime` 都不动（回零照旧用满上限、等 1 s）。
2. `test\host_test.cpp`：
   - `Check(std::abs(kPitchCurrentCap - 869) <= 1, ...)` 里的 869 改成 `kPitchCurrentCap` 的新值（应该是 1738 左右，按实际算出来的写，误差 1 以内）。
   - 原来那条 `"pitch alone uses the full screw rating and homes with its full limit"` 改成：`Check(Near(kPitchScrewForce, 6000.0) && Near(kPitchScrewForce * kPitchLoadMargin, 2000.0, 1e-6) && Near(kLoadMargin, 0.8) && Near(kPitchHomeCurrentFraction, 1.0), "pitch uses a third of its 6000 N screw rating and homes with its full limit");`
   - yaw 上限那条（2829）不动。其他检查都不动。
3. `README.md`：pitch 电流上限那段改成：丝杠实际额定 6000 N，先用三分之一约 2000 N，上限约 1738；回零时用满上限顶住上端 1 s；1000 N 时往上推不动。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

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
   应该还是 `PASS: 18730 checks`（只改了两条检查的内容）。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "pitch 推力上限放到丝杠额定的三分之一，约 2000 N"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；`kPitchCurrentCap` 的实际值
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
