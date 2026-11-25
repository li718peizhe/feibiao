# 任务 2s：上膛和退膛的慢压改成固件自己找底

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 1769388），主机测试现在是 `PASS: 17888 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 70a67a1）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 滑块坐标以回零点为准，回零点是滑块顶到防撞块的地方。防撞块挪一次，所有读数就整体偏一次。2026-10-18、10-19 就因为这个，慢压到不了 `kCockPosition`，上膛失败。用户以后还要把防撞块往上挪。
- 用户定：慢压一直压到压不动，就把那里当作底，往上留 1.5 mm 再锁。以后挪防撞块，不用再改固件。代价是每次上膛、退膛都会慢慢顶一下底。
- 顺带修一个隐患：`Safety::LockOpenAllowed` 现在只要滑块读数不高于 `kLatchSlider - 1 mm` 就允许开夹爪。防撞块往上挪以后，读数偏低，滑块可能还没压住发射机构，读数就已经过线了，退膛时开夹爪就会走火。改成只认这次慢压真的找到了底、滑块正压在底上。

## 找底的规则（`上膛：滑块慢压`、`退膛：滑块慢压`，以及装填里两步压下）
1. 进入这一步：清掉上一次的找底结果，滑块以 `kPressSpeed` 往 `kPressSearchLimit` 压。
2. 每次检查时，读 `io_.State().slider` 和 `io_.SliderCurrent()`。任一不是有限数，就报 `InvalidInput`。
3. 推着走：电流大于 `kLatchCheckCurrent` 时，如果还没记起点，就把当前位置记为“推着走的起点”；电流不大于 `kLatchCheckCurrent` 时，起点作废。
4. 压不动：电流不小于 `kBottomCurrent`，并且滑块在 `kBottomTime` 里移动不超过 `kBottomStill`，就算压不动。窗口从电流第一次达到 `kBottomCurrent` 开始算；位置比窗口起点变化超过 `kBottomStill`，就从当前位置重新开窗口；电流掉到 `kBottomCurrent` 以下，窗口作废。时间用这一步的 `elapsed_`。
5. 判定压不动的那一刻：
   - 如果没有推着走的起点，或者起点到当前位置不到 `kMinPushTravel`，报新故障 `PressBlocked`（“下压被挡住”）。这一条防的是：滑块还没推着发射机构走，就被别的东西卡住了，这时候绝不能当作底。
   - 否则当前位置就是底，记下来；锁止位置 = 底 + `kCockLift`；滑块以 `kPressSpeed` 退到锁止位置。
6. 找到底以后，滑块在锁止位置 1 mm 以内并且停稳（`SliderAt(锁止位置, 0.001, kSliderVelocityTolerance)`），这一步才算完成。
7. 还没找到底，滑块就已经到了 `kPressSearchLimit` 并停稳（`SliderAt(kPressSearchLimit, kSliderTolerance, kSliderVelocityTolerance)`），报新故障 `NoBottom`（“压到最低也没碰到底”）。
8. 超时照旧用 `kPressTimeout`，报 `Timeout`。
9. 所有故障都走原来的 `Fail`，滑块照旧按 `SliderMustHold` 保持。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kCockBottom`、`kCockPosition` 保留，注释改成“名义值：只用来算找底下限、退膛超时和测试；实际锁止位置每次由找底决定”。`kCockPosition` 改成 `kCockBottom + kCockLift`。
   - 在 `kCockPosition` 前面加 `kCockLift = 0.0015`（m，找到底后往上留的距离，和 2q 一样）。
   - 在 `kCockPosition` 后面加 `kPressSearchLimit = kCockBottom - 0.040`（m，找底最低压到名义硬底下面 40 mm；防撞块往上挪 40 mm 以内都找得到）。
   - 在压下相关常量附近加：`kBottomCurrent = kSliderCurrentCap * 0.95`（压不动的电流门槛；2q 慢压途中最高 9458，约为上限的 92%）；`kBottomStill = 0.0005`（m）；`kBottomTime = 0.2`（s）；`kMinPushTravel = 0.002`（m，压不动之前至少推着发射机构走这么远）；`kBottomHoldTolerance = 0.002`（m，判定“正压在底上”的范围）。
   - 在 `kProbePosition` 后面加 `kManualSliderFloor = kProbePosition`（m，手动往下点动最低到探测位，不让手动压到夹爪附近）。
   - 每个新常量都写一句中文注释，末尾标“待标定”。
2. `src\safety.hpp`、`src\safety.cpp`：
   - `SafetyFault` 加 `PressBlocked`、`NoBottom`，`Message` 分别返回“下压被挡住”“压到最低也没碰到底”。别处对 `SafetyFault` 的 `switch` 也要补全。
   - `SafetyState` 加 `bool at_bottom = false;`，注释写“这次慢压找到了底，滑块正压在底上；只由流程的快照填”。
   - `LockOpenAllowed` 改成 `s.latch_state == LatchState::Released || s.at_bottom`，去掉 `kLatchSlider` 那一条。
3. `src\launch_sequence.hpp`、`src\launch_sequence.cpp`：
   - 按上面“找底的规则”实现，状态放在 `LaunchSequence` 的私有成员里。
   - `ExpandCock` 和 `CommandDecock` 里两步慢压的目标改成 `kPressSearchLimit`。`LoadSteps` 里“装填：滑块把发射机构带下去”“装填：压下锁止”两步改成 `Action::Press`、目标 `kPressSearchLimit`、超时 `kPressTimeout`。
   - `Condition::Lock` 里合上夹爪时，原来检查 `SliderAt(kCockPosition, 0.001, ...)`，改成检查找底得到的锁止位置；还没找到底就不算到位。
   - `Snapshot()` 填 `at_bottom`：找到了底，并且 `state.slider` 是有限数、离锁止位置不超过 `kBottomHoldTolerance`。
   - `Reset()`、`Cancel()`、`Fail()` 和进入任何一步慢压时，都清掉找底结果。
4. `src\hw\launcher_hw.cpp`：
   - `SliderConfig()` 的下限从 `kCockPosition` 改成 `kPressSearchLimit`，上面的注释改成：流程找底最低压到这里，手动点动另由 `kManualSliderFloor` 限制。
   - `Manual()` 里算出滑块速度以后，如果速度小于 0，并且左右任一台滑块位置不高于 `kManualSliderFloor`，速度改成 0；后面的保持逻辑不动。往上点动不受影响。
   - 手动开夹爪仍然走 `Safety::LockOpenAllowed(State())`。适配层的 `State()` 里 `at_bottom` 永远是 false，所以手动只有“已放开”时才能开夹爪。这是有意的。
5. `test\host_test.cpp`：
   - `FakeIo` 加一个可设的硬底，默认 `kCockBottom`。滑块目标低于硬底时，滑块停在硬底。停在硬底以后，`SliderCurrent()` 返回可设的 `bottom_current`，默认 `kSliderCurrentCap`；往下走的途中照旧返回 `motion_current`。旧的 `cock_offset` 改成按新规则来测。
   - 原来按“慢压目标是 `kCockPosition`”写的检查，改成按新规则检查：慢压目标是 `kPressSearchLimit`，速度是 `kPressSpeed`；锁止位置是找到的底 + `kCockLift`。不准删检查，也不准放宽安全判断；每条旧检查想测的东西，都要还有检查测到。
   - 原来 `"pressed lock may open"`、`"pressed lock tolerates arrival above cock target"` 两条，测的是“已锁住、滑块在 `kCockPosition` 或略高 0.8 mm 就允许开夹爪”。按新规则这两种情况都不允许了，要改成断言不允许，再各加一条：同样的位置、`at_bottom` 为 true 时允许。这是有意收紧，不算放宽。`"latch position alone cannot open lock"`、`"parked latched lock cannot open"`、`"released lock may open"` 照旧。
   - 新增下面这些检查，每种至少一条：
     1. 硬底在 `kCockBottom`：上膛找到底，在底 + 1.5 mm 合上夹爪，最后 `Latched`。
     2. 硬底比 `kCockBottom` 高 15 mm、低 20 mm 各一次：都在各自的底 + 1.5 mm 合上夹爪。
     3. 电流在滑块还在往下走时就超过 `kBottomCurrent`：不算找到底。
     4. 一进慢压就压不动，推着走不到 `kMinPushTravel`：报 `PressBlocked`，滑块保持（`StopMotors(true)`），没有发过合上夹爪的指令。
     5. 压到 `kPressSearchLimit` 也没压不动：报 `NoBottom`，滑块保持。
     6. 退膛：慢压途中还没找到底时，没有发过开夹爪指令；找到底、滑块退到底 + 1.5 mm 以后，才发开夹爪指令，并记为“已放开”。
     7. `LockOpenAllowed`：已锁住、滑块在原来的 `kCockPosition`、`at_bottom` 为 false 时不允许；`at_bottom` 为 true 时允许；“已放开”时允许。
     8. 找到底以后 `Reset()`、`Cancel()`、故障都会清掉，快照里的 `at_bottom` 变回 false。
6. `README.md`：上膛、退膛的步骤说明改成找底的写法；写明两个新故障、手动往下点动只到探测位、手动开夹爪只在“已放开”时允许、`LockOpenAllowed` 不再看固定读数。数值都写进去。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\safety.hpp`、`src\safety.cpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。如果补全 `SafetyFault` 的 `switch` 必须改别的 `src` 文件，可以改，但要写进汇报。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`。不是就停下写汇报。
2. 改代码（第 1~6 条）。
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
- 分支不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好
- 这份文件里的规则互相矛盾，或者照着做会让某个安全判断变松

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么
- 新增的检查各是哪几条（写检查的提示文字）；原来按 `kCockPosition` 写的检查分别改成了什么
- 主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
