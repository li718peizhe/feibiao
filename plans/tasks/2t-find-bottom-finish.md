# 任务 2t：接着 2s 做完找底（装填加接近步骤，补检查和 README）

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，HEAD 1769388）。上一次（2s）改到一半，工作区里有 7 个文件没提交：`src\dart_params.hpp`、`src\safety.hpp`、`src\safety.cpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`。**在这些改动上接着做，不要还原、不要 stash、不要 checkout 文件。**
- 固件工程：`dart-fw\`（git，分支 dart-launcher，HEAD 70a67a1，工作区干净）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 2s 做了什么、为什么停下
- 已经改好、审过的：`src\` 里找底的实现（规则见下一节）、两个新故障 `PressBlocked`“下压被挡住”和 `NoBottom`“压到最低也没碰到底”、`SafetyState::at_bottom`、`LockOpenAllowed` 改成只认“已放开”或 `at_bottom`、滑块轴下限改成 `kPressSearchLimit`、手动往下点动只到 `kManualSliderFloor`。这些逻辑不要改，下面第 1 条除外。
- 停下的原因：2s 的任务文件把装填里两步压下直接改成了慢压，“装填：滑块把发射机构带下去”是从停靠位 0.366 出发的，以 0.05 m/s 压到 `kPressSearchLimit` 要 14 s，超过 `kPressTimeout` 8 s。这是任务文件写错了，不是要延长超时。上膛、退膛在慢压之前都有一步快速“下到锁止位上方”，装填漏了。
- 测试停在 `FAIL: fault during servo opening holds supported slider`：这条在装填流程里等“装填：张开夹爪”，等 100 拍还没到，也是因为同一个原因。

## 找底的规则（已经实现，写检查时照这个）
1. 进入慢压：清掉上一次的找底结果，滑块以 `kPressSpeed` 往 `kPressSearchLimit` 压。
2. 电流大于 `kLatchCheckCurrent` 时，记下“推着走的起点”；电流不大于它时，起点作废。
3. 电流不小于 `kBottomCurrent`，并且在 `kBottomTime` 里位置变化不超过 `kBottomStill`，就算压不动。
4. 压不动时，推着走不到 `kMinPushTravel` 就报 `PressBlocked`；否则当前位置就是底，锁止位置 = 底 + `kCockLift`，滑块以 `kPressSpeed` 退到锁止位置；在锁止位置 1 mm 以内停稳，这一步才完成。
5. 还没找到底，就已经到 `kPressSearchLimit` 停稳，报 `NoBottom`。超时照旧报 `Timeout`。
6. 合上夹爪（`Condition::Lock`、`kLockClosed`）要求已经找到底，并且滑块在锁止位置 1 mm 以内停稳。
7. 快照里的 `at_bottom` = 找到了底，并且滑块离锁止位置不超过 `kBottomHoldTolerance`。`Reset()`、`Cancel()`、`Fail()`、进入任何一步慢压，都会清掉找底结果。

## 要改的地方
1. `src\launch_sequence.cpp` 的 `LoadSteps`：在“装填：滑块把发射机构带下去”和“装填：压下锁止”前面，各加一步 `{"装填：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout}`，`Prepend` 的个数从 12 改成 14。两步压下保持 `Action::Press`、目标 `kPressSearchLimit`、超时 `kPressTimeout`。不要改 `kPressTimeout`。
   - 另外在 `src\launch_sequence.hpp` 的公开接口里加一个只读函数 `bool AtBottom() const { return Snapshot().at_bottom; }`，注释写“这次慢压找到了底、滑块正压在底上；给测试和以后的状态显示用”。只加这一个，不改别的逻辑。
2. `test\host_test.cpp`：
   - `FakeIo` 注意：`kProbeSpeed`、`kPressSpeed`、`kLoadRiseSpeed` 都是 0.05 m/s，不能只靠速度分辨是不是慢压，否则“退膛：滑块慢慢回位”“装填：慢慢抬升到磁铁正下方”这些往上走的步骤也会被当成慢压。用“目标低于硬底”或者单独的标志来分辨，往上走的步骤保持原来的模拟方式。
   - 原来的检查不准删，安全判断不准放宽；每条旧检查想测的东西，都要还有检查测到。装填流程多了两步，按新的步骤顺序改相关检查。
   - 2s 已经改好的 4 条开夹爪检查（`"latched lock at nominal cock position cannot open without bottom snapshot"` 等）保留。
   - 新增下面这些检查，每种至少一条：
     1. 硬底在 `kCockBottom`：上膛找到底，在底 + 1.5 mm 合上夹爪，最后 `Latched`。
     2. 硬底比 `kCockBottom` 高 15 mm、低 20 mm 各一次：都在各自的底 + 1.5 mm 合上夹爪，最后 `Latched`。
     3. 电流在滑块还在往下走时就超过 `kBottomCurrent`：不算找到底，滑块继续往下压。
     4. 一进慢压就压不动，推着走不到 `kMinPushTravel`：报 `PressBlocked`，滑块保持（`StopMotors(true)`），没有发过合上夹爪的指令。
     5. 压到 `kPressSearchLimit` 也没压不动：报 `NoBottom`，滑块保持。
     6. 退膛：慢压途中还没找到底时，没有发过开夹爪指令；找到底、滑块退到底 + 1.5 mm 以后，才发开夹爪指令，并记为“已放开”。
     7. 找到底、滑块退到锁止位置以后 `AtBottom()` 为 true；分别在 `Reset()`、`Cancel()`、注入一个故障以后，`AtBottom()` 都变回 false。
     8. 装填：两步压下之前都先快速下到 `kProbePosition`，压下都找到了底；整个装填在超时内走完。
3. `README.md`：
   - 上膛、退膛、装填的步骤说明改成找底的写法：先快速下到探测位，再以 0.05 m/s 压到压不动（电流不小于上限的 95%、0.2 s 内移动不超过 0.5 mm），把那里当作底，退到底上 1.5 mm 再合上或张开夹爪；最低压到 `kPressSearchLimit`（名义硬底下面 40 mm）。
   - 写明两个新故障各在什么情况下出现，出现后滑块照旧原地托着。
   - 写明：开夹爪只认“已放开”或“这次找到了底、滑块正压在底上”，不再看固定读数（防撞块挪了以后固定读数会让退膛走火）；手动往下点动最低到探测位 `kManualSliderFloor`；手动开夹爪只在“已放开”时允许。
   - 原来写“滑块轴的控制下限是上膛位”的那句，改成新的上下限说法。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\launch_sequence.cpp`（只改 `LoadSteps`）、`src\launch_sequence.hpp`（只加 `AtBottom()`）、`test\host_test.cpp`、`README.md`。如果为了让测试通过必须改别的 `src` 内容，先停下写汇报。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`；`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher status --porcelain` 应该正好是上面那 7 个文件。不对就停下写汇报。
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
   应该打印 `PASS: N checks`，N 会变，只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "慢压改成自己找底，开夹爪只认压在底上"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断、延长超时才能通过
- 要改“允许改的文件”以外的东西，包括 `src\` 里除 `LoadSteps` 和 `AtBottom()` 以外的逻辑
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 编译错误修了两轮还没好
- 这份文件里的规则互相矛盾，或者照着做会让某个安全判断变松

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么
- 新增的检查各是哪几条（写检查的提示文字）；装填流程相关的旧检查改成了什么
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
