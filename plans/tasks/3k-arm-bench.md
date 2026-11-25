# 任务 3k：打开 GM6020 摆臂，桌上测 6020

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`，也不要读别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区：`C:\Users\Lenovo\Desktop\xrobot`
- 模块仓库：`xrobot-local\DartLauncher\`（git，分支 main，当前 HEAD 6754e92），主机测试现在是 `PASS: 19159 checks`
- 固件工程：`dart-fw\`（git，分支 dart-launcher，当前 HEAD 110d260）。模块会检出到 `Modules\local\DartLauncher`，不要改这份副本，也不要手改 `xrobot.lock`。这次 dart-fw 只更新模块，不改配置。
- 两个仓库都直接在当前分支上提交：DartLauncher 在 `main`，dart-fw 在 `dart-launcher`。不要新建分支，也不要切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码保持一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层。

## 为什么要改
- 2026-10-28 用户用 RoboMaster Assistant 给换弹摆臂的 GM6020 打开了电流环，要先在桌上单独测这台电机。它接 CAN2，ID 1（反馈 0x205，控制帧 0x1FE），和配置 `artisans_dart.yaml` 里的 `arm_motor`（can2、517）、`dart_launcher` 的 `arm_can: can2`、`arm_id: 517` 一致，配置不用改。皮筋现在拆着，这段时间不发射。
- 现在 `kArmInstalled` 是 false，摆臂不算在线、不输出，状态行里也看不到 6020 的编码器和电流。
- 改法：
  1. `kArmInstalled` 改成 true。摆臂上线后照旧按绝对编码器建零点；手动模式装弹组（左拨杆下）左摇杆横向点动摆臂，限速 `kArmSpeed`（3 rad/s）。
  2. 状态打印加一行 `Dart arm cmd= act= rpm= bus=`，和 pitch 那行一样：`cmd` 是电流指令，`act` 是 6020 回报的实际转矩电流（换成同一单位），`rpm` 是输出轴转速，`bus` 是摆臂那条 CAN 开机以来收到的帧数。`act` 跟着 `cmd` 走，说明电流环开对了。10-20 pitch 接 CAN2 时一直没上线，原因没查到；6020 不在线时，`bus` 为 0 说明这条总线上什么都没收到（查线、接头、终端电阻），`bus` 在涨说明有帧、只是 ID 不对（查拨码）。
  3. 修一处点动的问题：`MotorAxis::Update()` 里回零以后的点动按软限位夹速度，轴在软限位外面时，夹出来的速度会大到上万，等于满电流把它拉回范围。摆臂按绝对角建零点，`kArmZeroCounts` 还没标（是 0），软限位 ±95°，上电时的角度差不多有一半落在范围外；一进手动模式就会每拍 `Jog(0)`，电机会突然满电流甩回去。改成：在软限位外面时只准往回走，不能往外走，也不强拉；在范围里面和原来完全一样。
- 不改的：滑块和 pitch 的行为；摆臂的 PID、软限位、零点（都还没标定）；推杆、电磁铁仍然没装，`kReloadInstalled` 还是 false，单发流程不变。
- 副作用：装了摆臂以后，6020 掉线也和别的电机一样报“电机离线”，整车停。没接 6020 时要用车，就烧 3j 的固件，所以编译前先把现在的 `DevC.elf`（3j 编的）另存一份。

## 要改的地方
1. `src\dart_params.hpp`：`kArmInstalled` 改成 `true`，注释改成：`// 2026-10-28 桌上测：GM6020 接 CAN2，ID 1（反馈 0x205），已开电流环；零点、范围和 PID 待标定。`
2. `src\motor_axis.cpp`：`MotorAxis::Update()` 里点动那段
   ```cpp
           if (Homed()) {
               velocity = Clamp(velocity, (config_.minimum - Position()) / dt,
                                 (config_.maximum - Position()) / dt);
           }
   ```
   改成
   ```cpp
           if (Homed()) {
               // 在软限位外面（比如摆臂按绝对角建零点时就在外面）只准往回走，不往外走，也不强拉回去。
               velocity = Clamp(velocity, std::fmin((config_.minimum - Position()) / dt, 0.0),
                                 std::fmax((config_.maximum - Position()) / dt, 0.0));
           }
   ```
   别的不动。
3. `src\hw\launcher_hw.hpp`：
   - 开头的 `#include` 里加 `#include <atomic>`。
   - `MonitorFeedback` 里 `double pitch_feedback_torque = 0, pitch_feedback_rpm = 0;` 下面加：
     ```cpp
             // 摆臂 6020 的电流指令、回报的转矩 N·m 和转速 rpm，以及摆臂那条 CAN 开机以来收到的帧数。
             int arm_current = 0;
             double arm_feedback_torque = 0, arm_feedback_rpm = 0;
             std::uint32_t arm_bus_frames = 0;
     ```
   - 私有成员最后（`last_late_wakes_` 那行后面）加：
     ```cpp
         // 摆臂那条 CAN 收到的所有标准帧计数，由 CAN 回调递增，控制线程拷进监控快照。
         std::atomic<std::uint32_t> arm_bus_frames_{0};
         LibXR::CAN::Callback arm_bus_callback_;
     ```
4. `src\hw\launcher_hw.cpp`：
   - 匿名命名空间里 `kM3508RotorTorqueConstant` 那行下面加：
     ```cpp
     constexpr double kGm6020TorqueConstant = 0.741; // N·m/A，和 RMMotor.hpp 里 KGetTorque() 的 GM6020 值一致；±3 A 对应 ±16384。
     ```
   - 构造函数体里，`if constexpr (!kReloadInstalled) { inventory_.ResetManual(); }` 后面加（在函数体里赋值再注册，不放进初始化列表，免得成员初始化顺序报 `-Wreorder`）：
     ```cpp
         // 摆臂那条 CAN 上收到的标准帧全部计数：6020 不在线时，分得清是总线上什么都没收到，还是有帧但 ID 不对。
         // LibXR 的 CAN 会把一帧交给所有匹配的订阅者，这个计数不影响电机自己的回调。
         arm_bus_callback_ = LibXR::CAN::Callback::Create(
             [](bool, LauncherHardware *self, const LibXR::CAN::ClassicPack &) {
                 self->arm_bus_frames_.fetch_add(1, std::memory_order_relaxed);
             }, this);
         ca.Register(arm_bus_callback_, LibXR::CAN::Type::STANDARD,
                     LibXR::CAN::FilterMode::ID_RANGE, 0x000, 0x7FF);
     ```
   - `Tick()` 末尾拷监控快照的地方，`monitor_feedback_.pitch_feedback_rpm = ...;` 下面加：
     ```cpp
         monitor_feedback_.arm_current = kArmInstalled ? arm_.Current() : 0;
         monitor_feedback_.arm_feedback_torque = kArmInstalled ? arm_.FeedbackTorque() : 0;
         monitor_feedback_.arm_feedback_rpm = kArmInstalled ? arm_.FeedbackRpm() : 0;
         monitor_feedback_.arm_bus_frames = arm_bus_frames_.load(std::memory_order_relaxed);
     ```
   - `OnMonitor()`：在 `if constexpr (kPitchInstalled) { ... }` 那段后面、发射记录那段（`if (feedback.shot.number != printed_shot_)`）前面加：
     ```cpp
         if constexpr (kArmInstalled) {
             LibXR::Thread::Sleep(20);
             // act 是 6020 回报的实际转矩电流，按 0.741 N·m/A、±3 A 对应 ±16384 换回和 cmd 一样的单位。
             const int arm_actual = static_cast<int>(std::lround(
                 feedback.arm_feedback_torque / kGm6020TorqueConstant / 3.0 * 16384));
             LibXR::STDIO::Printf<"Dart arm cmd=%d act=%d rpm=%.0f bus=%u\r\n">(
                         static_cast<int>(feedback.arm_current), arm_actual,
                         static_cast<float>(feedback.arm_feedback_rpm),
                         static_cast<unsigned>(feedback.arm_bus_frames));
         }
     ```
   - 别的不动。
5. `test\host_test.cpp`：
   - `TestPidAndParams()` 里 `Check(!kArmInstalled, "arm is not installed for stage 2e");` 改成 `Check(kArmInstalled, "arm GM6020 is enabled for bench testing since 2026-10-28");`。`Check(!kReloadInstalled, ...)` 不动。
   - `TestHomeAtAndRebase()` 里 `"reconnected absolute axis homes without movement"` 那条后面加 4 条（都用 `SliderConfig()` 的新轴，先 `Feedback(12345, 0)`，每拍 `Update(0.001, true, true)`）：
     - 轴 `below`：`HomeAt(kSliderBottom - 0.05)` 成功，`Jog(0)` 后这一拍输出 0。提示文字：`"jogging outside the soft limit does not drag the axis back"`。
     - 接着 `below.Jog(-0.05)` 后这一拍输出 0。提示文字：`"jogging further outside the soft limit is refused"`。
     - 接着 `below.Jog(0.05)` 后这一拍输出大于 0。提示文字：`"jogging back toward the soft limit range is allowed"`。
     - 轴 `above`：`HomeAt(kSliderTop + 0.05)` 成功；`Jog(0)` 一拍输出 0；`Jog(0.05)` 一拍输出 0；`Jog(-0.05)` 一拍输出小于 0。合成一条，提示文字：`"above the upper soft limit jogging only moves back down"`。
   - 原来的检查都不动；如果有旧检查因为这次的改动失败，先停下写汇报，不要改它。
6. `README.md`：
   - 装配开关表里 `kArmInstalled` 的值改成 `true`，说明后面加：2026-10-28 打开，桌上测 GM6020（CAN2，ID 1，已开电流环），零点、范围、PID 还没标定。
   - “在线和全部回零只检查已安装电机，当前只要求两侧滑块”改成“……当前要求两侧滑块、pitch 和摆臂；6020 掉线也报电机离线，整车停”。
   - “遥控与安全”一节“右拨杆中：手动”那条后面加一句：回零以后的轴在软限位外面时（比如摆臂按绝对角建零点时就在外面），点动只准往回走，不往外走，也不会被满电流拉回去。
   - “状态打印”：开头补的那句“有新的一发时，再多打一行发射记录（第 8 条）”改成“（第 9 条）”；第 7 条后面加新的第 8 条：`Dart arm cmd=... act=... rpm=... bus=...`（只有装了摆臂才打印）：`cmd` 是电流指令，`act` 是 6020 回报的实际转矩电流（同一原始单位，±16384 对应 ±3 A），`rpm` 是输出轴转速，`bus` 是摆臂那条 CAN（现在是 CAN2）开机以来收到的帧数，6020 在线时每秒涨约 1000；原来的第 8 条（发射记录）改成第 9 条。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock`，xrobot 生成的文件，以及新建 `build\DevC-3j.elf`（`build` 不进 git）。其他文件都不碰。

## 步骤
1. 先确认分支：`git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，`git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，两个仓库的 `git status --porcelain` 都应该是空的。不对就停下写汇报。
2. 改 DartLauncher（第 1~6 条）。改之前先数一下 `test\host_test.cpp` 里含 `Check(` 的行数（现在应该是 357），改完再数一次，应该多 4 行。
3. 跑主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 大约是 19163。只要是 `PASS` 就行，把 N 写进汇报。
4. 框架引用检查，下面这条命令应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 仓库的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "打开 GM6020 摆臂，状态打印加摆臂电流和总线帧数，软限位外点动只准往回走"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对，因为沙箱有时会吞掉 git 的写入。
6. 先另存 3j 的固件，再更新固件并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   (Get-Item build\debug\DevC.elf).LastWriteTime
   Copy-Item build\debug\DevC.elf build\DevC-3j.elf
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   ```
   - 第一条打印的修改时间应该是 2026-10-28 01:52 左右（3j 那次编的）。不是的话先别复制，停下写汇报。
   - xrobot 会提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要去升级 xrobot。
   - `xrobot setup` 偶尔连 GitHub 报 502 这类临时网络错误，可以原样重跑一次；还不行就停下写汇报。不要手改 `xrobot.lock`。
   - `git -C Modules\local\DartLauncher log --oneline -1` 必须是第 5 步的提交。不是的话就停下写汇报，不要编译，不要自己去副本里 fetch 或 checkout。
   - 编译提示 CCMRAM 用了 100% 是正常的（FreeRTOS 的堆正好占满），不用管。
   - `build\debug\DevC.elf` 的修改时间应该是这次的，`build\DevC-3j.elf` 应该还是 01:52 那个。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher"`。提交后同样核对；`git status --porcelain` 里还有别的文件，不要提交，写进汇报。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，或者要删检查、放宽别的判断才能通过
- 第 4 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西，或者要改滑块、pitch 的行为，或者要改摆臂的 PID、软限位、零点
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 改了哪些文件，每个文件一两句说改了什么；`Update()` 里点动那段、构造函数里新加那段和 `OnMonitor()` 新加那段的原文
- 新增检查的提示文字；改前改后含 `Check(` 的行数；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 和 `DevC-3j.elf` 的修改时间；`arm-none-eabi-size build\debug\DevC.elf` 的输出
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；如果中途停下了，写停在哪一步
