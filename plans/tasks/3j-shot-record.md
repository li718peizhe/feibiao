# 任务 3j：每发打一行发射记录

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD f3c5c3a），主机测试现在是 `PASS: 19148 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 13b308f）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-22 第一次带镖实打，飞了约 23 m，固件全流程没有故障。接下来要同一俯仰连打 5~10 发量散布，要知道每一发是在什么条件下打出去的，好和落点对照。
- 现在串口只有每秒 7 行状态。发射前后几个步骤都不到 1 s，正好落在两次打印之间，看不到。
- 改法：流程在“发射：解锁”打开夹爪的那一拍写一条发射记录，监控线程看到新记录就在那一秒的状态行后面多打一行，每发只打一次：
  - `n`：开机以来第几发，故障复位不清零。
  - `pi`：解锁时的 pitch 位置。
  - `bot`：锁住那次上膛慢压找到的底。
  - `hold`：合上夹爪那一步（至少 0.6 s），滑块在锁止位托着发射机构时电流的平均值。没有拉力传感器之前，拿它近似皮筋拉满时的拉力。
  - `chk`：锁止确认 0.2 s 保持窗口里滑块电流的最大值。
  - `wait`：确认锁住到解锁隔了多少秒。皮筋拉着放久了拉力会变小。
- 锁住是探测判出来的（开机状态未知、探测电流低），就没有慢压找底：`bot` 打 `探测`，不打 `hold`，`chk` 记探测保持窗口里的最大电流。没有记录过锁住（只有主机测试里直接给“已锁住”时才会这样），`bot` 打 `无`。
- 只加记录和打印，不改任何流程步骤、安全判断和电机输出。

## 要改的地方
1. `src\safety.hpp`：`SafetyState` 里 `double slider = 0, crank = 0, arm = 0;` 下面加一行：
   ```cpp
       double pitch = 0; // pitch 位置 m，没装 pitch 时为 0；只给发射记录用，安全判断不看它
   ```
2. `src\launch_sequence.hpp`：
   - `class LaunchSequence` 前面加：
     ```cpp
     // 锁住是怎么确认的：没记录、慢压找底后锁止确认、探测判成已锁住。
     enum class LatchSource : std::uint8_t { None, Press, Probe };
     // 一发的发射记录，在“发射：解锁”打开夹爪那一拍写下；位置 m，电流是电调原始值，时间 s。
     struct ShotRecord {
         std::uint32_t number = 0; // 开机以来第几发，故障复位不清零；0 表示还没发过
         double pitch = 0; // 解锁时的 pitch 位置
         LatchSource source = LatchSource::None;
         double bottom = 0; // 锁住那次慢压找到的底，只有 Press 有
         double hold_current = 0; // 合上夹爪那一步，滑块在锁止位托着发射机构时电流的平均值，只有 Press 有
         double check_current = 0; // 锁止确认保持窗口里滑块电流的最大值；Probe 时是探测保持窗口里的
         double wait = 0; // 确认锁住到解锁隔了多久，None 时为 0
     };
     ```
   - 公开部分 `int Shots() const { return shots_; }` 下面加：
     ```cpp
         // 最近一发的发射记录；number 为 0 表示开机以来还没发过。
         const ShotRecord &LastShot() const { return last_shot_; }
     ```
   - 私有成员 `LatchState probe_result_ = LatchState::Unknown;` 下面加：
     ```cpp
         // 发射记录：确认锁住时把来源、底和电流记进 latch_record_（number、pitch、wait 不用），解锁时拷进 last_shot_。
         ShotRecord last_shot_{}, latch_record_{};
         double latch_time_ = -1;
         double hold_current_sum_ = 0;
         int hold_current_count_ = 0;
         double check_current_max_ = 0;
     ```
3. `src\launch_sequence.cpp`（`Reset()`、`Cancel()`、`Fail()` 都不碰新加的成员，发射记录和编号在故障复位后保留）：
   - `Enter()` 的 `case Action::Lock:`，在 `io_.RecordLatched(current_.target == kLockOpen ? ...);` 那行后面加：
     ```cpp
             hold_current_sum_ = 0;
             hold_current_count_ = 0;
     ```
   - `Enter()` 的 `case Action::Unlock:`，在 `io_.RecordLatched(LatchState::Released);` 后面、`break;` 前面加：
     ```cpp
             // 发射记录：编号接着上一发，记下解锁这一拍的 pitch 和最近一次确认锁住时的数据。
             const std::uint32_t number = last_shot_.number + 1;
             last_shot_ = latch_record_;
             last_shot_.number = number;
             last_shot_.pitch = io_.State().pitch;
             last_shot_.wait = latch_record_.source == LatchSource::None ? 0 : last_time_ - latch_time_;
             latch_record_ = ShotRecord{};
     ```
   - `Done()` 的 `case Condition::Lock:`，在最前面（`if (elapsed_ < kLockSettleTime)` 之前）加：
     ```cpp
             // 合上夹爪这一步，滑块在锁止位托着发射机构；累计托住的电流，记进这一发的发射记录。
             if (current_.target == kLockClosed && bottom_found_ &&
                 io_.SliderAt(lock_position_, 0.001, kSliderVelocityTolerance) &&
                 std::isfinite(io_.SliderCurrent())) {
                 hold_current_sum_ += std::fabs(io_.SliderCurrent());
                 ++hold_current_count_;
             }
     ```
   - `Done()` 的 `case Condition::LatchCheck:`：
     - 把
       ```cpp
               if (latch_hold_start_ < 0) {
                   latch_hold_start_ = elapsed_;
               }
       ```
       改成
       ```cpp
               if (latch_hold_start_ < 0) {
                   latch_hold_start_ = elapsed_;
                   check_current_max_ = 0;
               }
               check_current_max_ = std::fmax(check_current_max_, std::fabs(io_.SliderCurrent()));
       ```
     - 在 `io_.RecordLatched(LatchState::Latched);` 前面加：
       ```cpp
               // 记下这次锁住的数据，正式发射解锁时拷进发射记录。
               latch_record_ = ShotRecord{};
               latch_record_.source = LatchSource::Press;
               latch_record_.bottom = bottom_position_;
               latch_record_.hold_current = hold_current_count_ > 0 ? hold_current_sum_ / hold_current_count_ : 0;
               latch_record_.check_current = check_current_max_;
               latch_time_ = last_time_;
       ```
   - `Done()` 的 `case Condition::Probe:`：
     - 把
       ```cpp
               if (probe_hold_start_ < 0) {
                   probe_hold_start_ = elapsed_;
               }
       ```
       改成
       ```cpp
               if (probe_hold_start_ < 0) {
                   probe_hold_start_ = elapsed_;
                   check_current_max_ = 0;
               }
               check_current_max_ = std::fmax(check_current_max_, std::fabs(current));
       ```
     - 把 `if (result == LatchState::Latched) { inventory_.Latched(); }` 改成
       ```cpp
               if (result == LatchState::Latched) {
                   inventory_.Latched();
                   // 探测判成已锁住：没有慢压找底，只记探测保持窗口里的最大电流。
                   latch_record_ = ShotRecord{};
                   latch_record_.source = LatchSource::Probe;
                   latch_record_.check_current = check_current_max_;
                   latch_time_ = last_time_;
               }
       ```
   - 别的不动。
4. `src\hw\launcher_hw.hpp`：
   - `MonitorFeedback` 里 `std::uint32_t tick_avg_us = 0, tick_max_us = 0, late_wakes = 0;` 下面加：
     ```cpp
             // 最近一发的发射记录，控制线程每拍拷一份。
             ShotRecord shot{};
     ```
   - 私有成员 `MonitorFeedback monitor_feedback_{};` 下面加：
     ```cpp
         // 只由 OnMonitor 读写：上一次打印过的发射记录编号。
         std::uint32_t printed_shot_ = 0;
     ```
5. `src\hw\launcher_hw.cpp`：
   - `RefreshState()`：`state_.arm = arm_.Axis().Position();` 下面加 `state_.pitch = kPitchInstalled ? pitch_.Axis().Position() : 0;`
   - `Tick()` 末尾拷监控快照的地方，`monitor_feedback_.late_wakes = last_late_wakes_;` 下面加 `monitor_feedback_.shot = sequence_.LastShot();`
   - `OnMonitor()` 最后（`if constexpr (kPitchInstalled) { ... }` 那段后面，函数结束前）加：
     ```cpp
         // 新的一发打一行发射记录，每发只打一次。%s 参数不能是空串。
         if (feedback.shot.number != printed_shot_) {
             printed_shot_ = feedback.shot.number;
             LibXR::Thread::Sleep(20);
             if (feedback.shot.source == LatchSource::Press) {
                 LibXR::STDIO::Printf<"Dart shot n=%u pi=%.2fmm bot=%.1fmm hold=%d chk=%d wait=%.1fs\r\n">(
                             static_cast<unsigned>(feedback.shot.number),
                             static_cast<float>(feedback.shot.pitch * 1000),
                             static_cast<float>(feedback.shot.bottom * 1000),
                             static_cast<int>(feedback.shot.hold_current),
                             static_cast<int>(feedback.shot.check_current),
                             static_cast<float>(feedback.shot.wait));
             } else {
                 LibXR::STDIO::Printf<"Dart shot n=%u pi=%.2fmm bot=%s chk=%d wait=%.1fs\r\n">(
                             static_cast<unsigned>(feedback.shot.number),
                             static_cast<float>(feedback.shot.pitch * 1000),
                             feedback.shot.source == LatchSource::Probe ? "探测" : "无",
                             static_cast<int>(feedback.shot.check_current),
                             static_cast<float>(feedback.shot.wait));
             }
         }
     ```
   - 别的不动。
6. `test\host_test.cpp`：新加一个函数 `void TestShotRecord()`，放在 `TestProbeAndLatch()` 后面、`int main()` 前面，并在 `main()` 里 `TestProbeAndLatch();` 后面调用它。用已有的 `FakeIo`、`DartInventory`、`RunSequence`，每拍 0.01 s。同一个 `LaunchSequence` 跑第二段流程时，时间要接着往后走（从 1000 s 开始），不然 `Tick` 会因为时间倒退报 `InvalidInput`。一共加 11 条 `Check`：
   - 慢压那条：
     - `FakeIo io;`，`io.state.latch_state = LatchState::Released;`，`io.hard_bottom = kCockBottom - 0.018;`，`io.state.pitch = -0.02512;`，`io.slider_current = 6000;`；`DartInventory inventory;`，`inventory.ResetManual();`；`LaunchSequence sequence(io, inventory);`
     - 检查 `sequence.LastShot().number == 0`。提示文字：`"no shot record before the first shot"`。
     - `Check(sequence.CommandCock(), "shot record cock accepts a released launcher");`
     - 循环最多 2000 拍：`io.Advance(); sequence.Tick(tick * 0.01);`，然后如果步骤名是 `"上膛：锁止确认"`，就把 `io.slider_current` 设成 1200；`sequence.Busy()` 为假就退出。（`FakeIo` 在滑块停稳时返回 `slider_current`：合上夹爪那一步是 6000，锁止确认停稳以后是 1200，低于 `kLatchCheckCurrent`。）
     - 检查流程没有故障、`io.state.latch_state == LatchState::Latched`、`sequence.LastShot().number == 0`。提示文字：`"cocking alone does not add a shot record"`。
     - `Check(sequence.CommandFire(), "shot record fire accepts the latched launcher");`
     - 循环最多 2000 拍：`io.Advance(); sequence.Tick(1000 + tick * 0.01);`，记下步骤名第一次是 `"发射：解锁"` 的那一拍，`sequence.LastShot().number` 是不是已经是 1；`sequence.Busy()` 为假就退出。检查那一拍已经是 1。提示文字：`"shot record is written on the tick the fire unlock opens the jaws"`。
     - `const ShotRecord &shot = sequence.LastShot();`，检查 `shot.number == 1`、`shot.source == LatchSource::Press`、`Near(shot.pitch, -0.02512)`、`Near(shot.bottom, io.hard_bottom)`、`Near(shot.hold_current, 6000, 1e-6)`、`Near(shot.check_current, 1200, 1e-6)`、`shot.wait > 800 && shot.wait < 1000.1`，并且流程没有故障、`sequence.Shots() == 1`。提示文字：`"press shot record keeps pitch, bottom, hold and check currents and the latched wait"`。
   - 探测那条：
     - `FakeIo io;`，`io.state.latch_state = LatchState::Unknown;`，`io.slider_current = 800;`；`DartInventory inventory;`，`inventory.ResetManual();`；`LaunchSequence sequence(io, inventory);`
     - `const bool cocked = sequence.CommandCock(); RunSequence(sequence, io);`，`const bool fired = sequence.CommandFire(); RunSequence(sequence, io, 1000);`，检查 `cocked && fired`、流程没有故障。提示文字：`"probe shot record cocks by probing and fires"`。
     - 检查 `LastShot()` 的 `number == 1`、`source == LatchSource::Probe`、`Near(check_current, 800, 1e-6)`、`Near(hold_current, 0)`、`Near(bottom, 0)`、`wait > 0`。提示文字：`"probe-latched shot record has no press bottom and keeps the probe current"`。
   - 没记录过锁住、编号接着数：
     - `FakeIo io;`（默认已锁住、停在停靠位）；`DartInventory inventory;`，`inventory.ResetManual();`；`LaunchSequence sequence(io, inventory);`
     - `const bool fired = sequence.CommandFire(); RunSequence(sequence, io);`，检查 `fired`、`LastShot()` 的 `number == 1`、`source == LatchSource::None`、`Near(wait, 0)`。提示文字：`"shot without a recorded latch reports no latch source"`。
     - `sequence.Reset();`，记下这时 `LastShot().number`；再 `inventory.ResetManual();`、`io.state.latch_state = LatchState::Latched;`，`const bool again = sequence.CommandFire(); RunSequence(sequence, io, 1000);`。检查复位后记下的编号是 1、`again` 为真、现在 `LastShot().number == 2`。提示文字：`"fault reset keeps the last shot record and shot numbers keep counting"`。
   - 退膛那条：`FakeIo io;`（默认已锁住）；`DartInventory inventory;`；`LaunchSequence sequence(io, inventory);`；`const bool decocked = sequence.CommandDecock(); RunSequence(sequence, io);`，检查 `decocked`、流程没有故障、`LastShot().number == 0`。提示文字：`"decock does not add a shot record"`。
   - 原来的检查都不动；如果有旧检查因为这次的改动失败，先停下写汇报，不要改它。
7. `README.md`：
   - “状态打印”一节开头“每秒……打印七行短行”那句后面补一句：有新的一发时，再多打一行发射记录（第 8 条）。
   - 第 7 条后面加第 8 条：`Dart shot n=... pi=...mm bot=...mm hold=... chk=... wait=...s`（2026-10-28 起，每发只打一次）。正式发射在“发射：解锁”打开夹爪时记一条，下一次打印时跟在第 7 行后面。`n` 是开机以来第几发，故障复位不清零；`pi` 是解锁时的 pitch 位置；`bot` 是锁住那次上膛慢压找到的底；`hold` 是合上夹爪那一步滑块在锁止位托着发射机构时电流的平均值，没有拉力传感器之前拿它近似皮筋拉满时的拉力；`chk` 是锁止确认保持窗口里电流的最大值；`wait` 是确认锁住到解锁隔了多少秒；电流都是电调原始单位。锁住是探测判出来的，`bot` 打 `探测`，不打 `hold`，`chk` 是探测保持窗口里的最大电流；没记录过锁住打 `无`。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\safety.hpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，以及 xrobot 生成的文件。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~7 条）。改之前先数一下 `test\host_test.cpp` 里 `Check(` 出现了几次（现在应该是 346），改完再数一次，应该多 11 次。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 大约是 19159。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "每发打一行发射记录：pitch、找到的底、托住和确认的电流、锁住到解锁的时间"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
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
   - 编译报 `Printf` 格式的错（比如小数位超限）就停下写汇报，不要换别的打印方式。
   - `build\debug\DevC.elf` 的修改时间应该是这次的。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西，或者要改流程步骤、安全判断、电机输出
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；`Enter()` 里 `Action::Unlock` 新加那段、`Done()` 里三处改动（`Condition::Lock`、`Condition::LatchCheck`、`Condition::Probe`）和 `OnMonitor()` 新加那段的原文
- 新增检查的提示文字；改前改后 `Check(` 的个数；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
