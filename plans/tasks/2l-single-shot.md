# 任务 2l：没装换弹机构时也能单发（上膛后直接发射）

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要运行 graphify。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 9a3a5af），主机测试现在是 `PASS: 17459 checks`
  - `src\`：纯逻辑，不 include LibXR、HAL、FreeRTOS；`src\hw\`：LibXR 适配层
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD f3a047c）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。

## 为什么要改
- 车上只有两台滑块 M3508 和锁止舵机，摆臂、推杆、电磁铁（换弹机构）和 pitch、yaw 都没装。目标：上膛以后，由操作手手动放镖，再直接发射这一发。
- 上膛流程（`CommandCock`）不用改。发射这一路现在走不通：
  1. `launcher_hw.cpp` 的 `Tick()` 里，`command_.fire` 要求换弹机构三样都装了，否则只打印“换弹机构未安装”。
  2. `DartInventory` 开机按满装算（`ResetFull`：发射机构、摆臂、两侧镖座各一发）。`held` 为 true 时，`FireBlocker` 会因为摆臂没摆开报 `ArmInPath`；发完一发 `seated` 变 false，以后再也发不了。
  3. `LauncherHardware::Aimed()` 要求 pitch、yaw 都到位，没装的轴永远不到位，“瞄准”这一步会超时。
- 自动模式要打开才能用左拨杆发射。打开之前要先补一个漏洞：从手动切到别的模式时，`Manual()` 不再被调用，滑块会按摇杆最后的速度一直点动下去。

## 要改的地方
1. `src\dart_params.hpp`：
   - `kAutoEnabled` 改成 `true`，注释改成 `// 2026-10-16 打开：自动模式下左拨杆从中拨到上就单发；没接裁判系统也能用。`
   - 在 `kMagnetInstalled` 那一行后面加一行：
     `constexpr bool kReloadInstalled = kArmInstalled && kCrankInstalled && kMagnetInstalled; // 换弹机构三样都装好才自动换弹；否则镖由操作手手动放，只能单发。`
   - `kCockEnabled` 保持 `false`，别的常量都不动。
2. `src\dart_inventory.hpp`、`src\dart_inventory.cpp`：
   - 新增 `void ResetManual();`，注释：`// 没装换弹机构时用：只记发射机构上那一发，摆臂和两侧镖座都记为空。` 实现：`seated_ = true`，`held_`、`right_`、`left_` 都是 false，`held_side_ = 0`，`fault_ = false`，`manual_ = true`。
   - 新增 `void Latched();`，注释：`// 锁止确认或探测判定已锁住时调用：手动放镖时认为操作手会放上一发；自动换弹时不变。` 实现：`manual_` 为 true 时把 `seated_` 设成 true，否则什么都不做。
   - 私有成员加 `bool manual_ = false;`；`ResetFull()` 里加 `manual_ = false;`。
3. `src\launch_sequence.cpp` 的 `Done()`：
   - `Condition::LatchCheck` 分支里，`io_.RecordLatched(LatchState::Latched);` 后面加 `inventory_.Latched();`。
   - `Condition::Probe` 分支里，`io_.RecordLatched(result);` 后面加 `if (result == LatchState::Latched) { inventory_.Latched(); }`。
   - 别的地方不动，`FireBlocker` 和 `safety.cpp` 都不改。
4. `src\mode_logic.hpp`、`src\mode_logic.cpp`：
   - `ModeCommand` 里在 `stop_sequence` 旁边加 `bool leave_manual = false;`，注释：`// 这一次从手动切到别的模式，适配层据此停掉手动点动。`
   - `Update()` 里，在算 `out.stop_sequence` 的地方旁边加：`out.leave_manual = initialized_ && previous_mode_ == Mode::Manual && out.mode != Mode::Manual;`
5. `src\hw\launcher_hw.cpp`：
   - 构造函数的函数体开头加：`if constexpr (!kReloadInstalled) { inventory_.ResetManual(); }`，上面一行注释：`// 没装换弹机构时，镖由操作手手动放，库存只记发射机构上那一发。`
   - `Tick()` 里，在 `if (command_.stop_sequence && sequence_.Busy()) { sequence_.Cancel(); }` 后面加一行注释和一行代码：
     ```cpp
     // 离开手动模式时停掉手动点动，免得切到自动后滑块还按摇杆最后的速度走；托着发射机构时照样保持。
     if (command_.leave_manual && !sequence_.Busy()) { StopMotors(Safety::SliderMustHold(state_)); }
     ```
   - `command_.fire` 那一段改成：装了换弹机构（`kReloadInstalled`）照旧 `sequence_.CommandSalvo(1);`；没装就 `sequence_.CommandFire();`，上面注释 `// 没装换弹机构：镖由操作手手动放，直接发射发射机构上这一发，不装填下一发。` 用 `if constexpr`。
   - `command_.salvo` 那一段的条件换成 `!kReloadInstalled`，其他照旧（没装还是打印“换弹机构未安装”）。
   - `Aimed()` 改成没装的轴按到位算：yaw 部分写成 `!kYawInstalled || yaw_.Axis().Settled(...)`，pitch 部分写成 `!kPitchInstalled || pitch_.Axis().Settled(...)`，`Settled` 的参数照旧，两个结果相与。注释改成 `// 没装的 pitch/yaw 按到位算：发射架整体固定，方向由操作手摆好。`
6. `README.md`：
   - “pitch 或 yaw 未装时瞄准未到位”改成“没装的 pitch/yaw 按到位算”。
   - “换弹流程要求摆臂、推杆、电磁铁三个开关全部开启……”那一段改成：三样都开（`kReloadInstalled`）才走自动换弹；没装时连发拒绝并提示“换弹机构未安装”，单发改成直接发射发射机构上那一发，库存只记这一发（`ResetManual`），每次锁止确认通过后记为已放镖。
   - 写 `kAutoEnabled` 默认关闭的地方，改成 2026-10-16 已打开。
   - 在“遥控与安全”一节末尾加“没装换弹机构时的单发”操作步骤：
     1. 上电，锁止舵机转到锁死。
     2. 右拨杆中、左拨杆中（滑块组），拨轮往上拨到底保持 2 s，滑块回零。
     3. 拨轮往下拨到底保持 2 s，上膛（第一次会先探测），需要 `kCockEnabled` 为 true。
     4. 上膛完成、滑块回到顶部以后，手动放镖。
     5. 左拨杆放在中，右拨杆拨到上（自动）；再把左拨杆从中拨到上，发射。没锁住、滑块没回顶部等情况下发射会报故障，要按故障复位处理。
     6. 再发一发：右拨杆拨回中，从第 3 步开始。
7. `test\host_test.cpp`：
   - 原来用默认参数构造 `ModeLogic`、又默认自动模式关闭的检查，改成显式传 `false`，别改检查本身的意思。
   - 新增检查（写法照周围的测试，用现有的 `FakeIo`、`RunSequence`）：
     - `kAutoEnabled` 为 true，`kReloadInstalled` 为 false。
     - `ResetManual()` 以后：`Remaining()` 是 1，`Seated()` 是 true，`Held()` 是 false，两侧镖座都没有；`Fire()` 以后 `Remaining()` 是 0；再 `Latched()`，`Seated()` 回到 true。`ResetFull()` 以后发一发再 `Latched()`，`Seated()` 仍然是 false。
     - `ModeLogic`：手动切到自动、手动切到无力，这一次 `leave_manual` 为 true；自动切到手动、一直保持同一个模式时为 false。
     - 手动放镖的单发：库存 `ResetManual()`；`FakeIo` 设成全部回零、在线、已锁住、滑块在 `kSliderPark`、已瞄准。`CommandFire()` 返回 true，跑完以后锁止目标是 `kLockOpen`，锁止状态是 `Released`，发数加 1，库存是 0，没有故障。再 `CommandFire()` 应该失败，故障是 `NotLatched`。
     - 手动放镖模式下，上膛流程锁止确认通过以后，库存回到 1。照现有上膛测试的写法来。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\dart_inventory.hpp`、`src\dart_inventory.cpp`、`src\launch_sequence.cpp`、`src\mode_logic.hpp`、`src\mode_logic.cpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `xrobot setup` 改动的 `dart-fw\xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 改代码（第 1~7 条）。
2. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 比 17459 多。把 N 写进汇报。
3. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
4. 在 DartLauncher 仓库提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "没装换弹机构时单发直接发射，离开手动停掉点动"`。提交后用 `git rev-parse HEAD` 和 `git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
- 主机测试没有 `PASS`，或者要改 `FireBlocker`、`safety.cpp` 才能通过
- 第 3 步有输出
- 联网失败
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件；为了 `kAutoEnabled` 改了哪些旧测试
- 主机测试的 `PASS` 数；第 3 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
