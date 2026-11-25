# 任务 3d：pitch 推得动，状态行不再被空字符串截断

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。（编号跳过 3c：3c 留给以后的比赛流程。）

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 2d81ac3），主机测试现在是 `PASS: 18720 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD ea13e26）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
1. 状态行第 5 行在 `pi=` 后面被截断，`pI=`、`yaw=` 都没打出来，连行尾换行也丢了。真正的原因是：LibXR 的 `STDIO::CompiledSink::Write`（`dart-fw\Middlewares\Third_Party\LibXR\src\core\rw\stdio.cpp` 第 30~34 行）遇到长度为 0 的片段，就把 `saturated_` 置真，这一次打印后面的内容全部丢掉。装上 pitch 以后，`kPitchInstalled ? "" : "[X]"` 变成了空字符串，第 5 行就从这里断了。所以 `Printf` 的 `%s` 参数不能是空字符串。LibXR 是第三方库，不改它，改我们自己的打印。
2. 2026-10-20 pitch 在 CAN1 上在线了，但点动推不动。没回零时，点动和回零的电流只有上限的 30%，也就是 208（约 0.25 A）。上限本身 695（约 0.85 A），是按丝杠螺母额定 1000 N × 0.8、按理想扭矩算的，没扣电机自己的空载电流和丝杠效率。用户说 pitch 回零应该往上走，往上要抬起发射架，0.25 A 推不动。
   - pitch 电流上限改成按螺母额定的 100% 算，约 869（约 1.06 A）。算的时候不扣任何损耗，实际推到螺母上的力只会比 1000 N 小，不会超过额定。
   - pitch 没回零时的点动和回零都用满上限。顶到上端时，螺母受力也不会超过额定。
   - 滑块、yaw、摆臂没回零时的电流照旧是 30%。

## 要改的地方
1. `src\dart_params.hpp`：
   - 在 `kSliderLoadMargin` 后面加：`constexpr double kPitchLoadMargin = 1.0; // 2026-10-20：按 0.8 只有约 0.85 A，推不动；pitch 单独按螺母额定 100% 算，不扣损耗，实际推力只会更小。`
   - `kPitchCurrentCap` 里的 `kLoadMargin` 换成 `kPitchLoadMargin`。`kLoadMargin`、`kYawCurrentCap` 都不动。
   - 在 `kHomeCurrentFraction` 后面加：`constexpr double kPitchHomeCurrentFraction = 1.0; // pitch 往上回零要抬发射架，没回零的点动和回零都用满上限，待标定。`
2. `src\motor_axis.hpp`、`src\motor_axis.cpp`：
   - `AxisConfig` 最后加一个成员：`double home_current_fraction = kHomeCurrentFraction;`，注释写“没回零时点动和回零的限流比例”。加在最后，现有按位置初始化的写法都不用改。
   - `MotorAxis::CurrentLimit()` 和 `Update()` 里的 `low_limit`，都改用 `config_.home_current_fraction`，不再直接用 `kHomeCurrentFraction`。
3. `src\hw\launcher_hw.cpp`：
   - `PitchConfig()` 照 `ArmConfig()` 的写法：先 `auto config = Axis(...)`，再 `config.home_current_fraction = kPitchHomeCurrentFraction;`，然后 `return config;`。其他参数不动。
   - 状态打印里所有 `%s` 参数都不能是空字符串：
     - 第 5 行：`pi=%.2fmm%s pI=%d yaw=%.2fdeg%s` 改成 `pi=%.2f%s pI=%d yaw=%.2f%s`，对应参数改成 `kPitchInstalled ? "mm" : "mm[X]"`、`kYawInstalled ? "deg" : "deg[X]"`。
     - 第 6 行（`lock_raw_mode` 两个分支都改）：`raw=%u%s lock=` 改成 `raw=%u%slock=`，参数改成 `kArmInstalled ? " " : "[X] "`；`crank=%.0f%s tick` 改成 `crank=%.0f%stick`，参数改成 `kCrankInstalled ? " " : "[X] "`。
     - 改完以后，打出来的字样要和原来一样，比如没装时仍然是 `pi=0.00mm[X]`、`raw=0[X] lock=`。
     - 再把 `src\hw\` 里所有 `Printf` 的 `%s` 参数查一遍，凡是可能为空字符串的都同样改掉，写进汇报（`fault=`、`step=` 这类本来就不会为空的不用动）。
   - 在第 5 行那次打印上面加一行注释：`// LibXR 的 STDIO 遇到空字符串，会把这一次打印后面的内容全丢掉，所以 %s 参数不能是空串。`
   - 每行之间的 `Thread::Sleep(20)` 不动。
4. `test\host_test.cpp`：
   - `Check(std::abs(kPitchCurrentCap - 695) <= 1, "pitch current cap");` 里的 695 改成 869。
   - 紧跟着加：`Check(Near(kPitchLoadMargin, 1.0) && Near(kLoadMargin, 0.8) && Near(kPitchHomeCurrentFraction, 1.0), "pitch alone uses the full screw rating and homes with its full limit");`
   - 加两条 `MotorAxis` 检查：用测试里已有的 `SliderConfig()` 建一个轴，没回零时 `CurrentLimit()` 等于 `static_cast<int>(current_limit * kHomeCurrentFraction)`；同一个配置把 `home_current_fraction` 改成 1.0 再建一个轴，没回零时 `CurrentLimit()` 等于 `current_limit`。
   - 其他检查都不动。
5. `README.md`：写明 pitch 电流上限约 869，是按螺母额定 1000 N 的 100%、不扣损耗算的；pitch 没回零时点动和回零都用满上限，其他轴还是 30%；状态行的 `%s` 不能是空串，以及原因。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.hpp`、`src\motor_axis.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。如果查出 `src\hw\` 别的文件也有可能为空的 `%s`，也可以改，写进汇报。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰，LibXR 也不碰。

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
   应该打印 `PASS: N checks`，N 大约是 18723。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "pitch 按螺母额定 100% 限流、回零用满上限，状态行不用空字符串"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 需要改“允许改的文件”以外的东西，包括 LibXR
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；查出来的所有可能为空的 `%s` 各在哪一行、怎么改的
- `kPitchCurrentCap` 的实际值；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
