# 任务 3m：摆臂装车调通

2026-11-12 Claude 写，交给 GPT 在新对话里执行。不要读 `plans/dart-fw-libxr.md`，不要运行 graphify。

## 背景
- 摆臂电机是 GM6020，直接驱动，在 CAN2 上，反馈 ID 0x205（517），配置 id `arm_motor`。推杆舵机装在摆臂上，本任务不动推杆和电磁铁。
- 坐标：站在发射架后面、面朝发射方向，右为正。往右转编码器 raw 变大，`reverse: false` 不改。
- 11-11 已完成：
  - `kArmInstalled=true`。
  - 零点 raw 1942（装填位）；左镖座 −40.5°，右镖座 +46.2°；两边快碰到机构的位置是 −49.3° / +54.5°；软限位 −45° / +50°。机构没再动过，不用重测零点。
  - 电流上限 1.0 A（`kArmCurrentCap`=5461，0.74 N·m），速度环积分可以用满上限。速度环 kp 30、ki 0.5 是 10-28 对空电机调的，当时空电机在 ±95° 软限位冲过约 0.4°。
- 板上固件：DartLauncher `804c7c1`、dart-fw `3ea7210`，产物 `dart-fw/build/debug/DevC.elf`。主机测试 `PASS: 21563 checks`，`test/host_test.cpp` 里含 `Check(` 的行有 551 行。
- 11-12 00:08 用 1.0 A 试转：往右推满停在 +19.3°，往左停在 −13.4°；`cmd` 顶在 ±5461，`act` 也到了，`rpm=0`；松手后往中间回约 5.5°；动起来一顿一顿。用户反馈：无力时手转全程力量一样（阻力不随角度变）；松手回中像是摇杆回弹带的；要求加大电流。推的时候摆臂只是慢慢爬（往右约 5°/s，摇杆满杆的目标是 115°/s），`cmd` 一直顶在上限，说明全程都有接近 0.74 N·m 的阻力，卡住的地方阻力稍大一点，所以加电流应该有用。松手那一秒回中、之后不动，摇杆回弹和“有力往中间拉”都解释得通，试转时摇杆慢慢回中就能分清。
- GM6020 手册：
  - 额定（最大连续）1.2 N·m、1.62 A；持续堵转 0.86 N·m、0.90 A。
  - 电流指令 ±16384 对应 ±3 A，转矩常数 0.741 N·m/A。
  - 电机自己 100°C 报警、125°C 保护。另外 QDU RMMotor 的 `CurrentControl` 在电机温度超过 75°C 时把输出清零。
- 摆臂现在没开松手保持。`src/hw/launcher_hw.cpp` 的 `ArmConfig()` 里有一行注释“摆臂先不开松手保持：零点没标……标好零点再开”。不开时，松手后速度环每拍清零，摆臂没有保持力。

## 成功标准
1. 手动装弹组（右拨杆中、左拨杆下）左摇杆推满：摆臂走到软限位附近（约 +50° / −45°）停住，不碰机构。
2. 摇杆慢慢回中后，摆臂 5 秒内角度变化不超过 1°。
3. 用户看着动作平滑，不一顿一顿。
4. 试转中 `tmp=` 不超过 60；转动途中不出现“摆臂堵转”事件（推到软限位前 5° 以内、再推时出现不算，见第 5 步）。
5. 主机测试、固件编译通过，两个仓库都本地提交，结果写回文档。

## 第 1 步：预检
- `git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current` 应该是 `main`，HEAD 应该是 `804c7c1`。
- `git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current` 应该是 `dart-launcher`，HEAD 应该是 `3ea7210`。
- 两个仓库的 `git status --porcelain` 都应该为空。
- 和上面不一致时，在汇报里写明，然后按实际 HEAD 继续。
- Referee 模块的三处本地改动保留不动。

## 第 2 步：电流上限 2.0 A，加堵转保护和温度打印
1. `src/dart_params.hpp`：
   - `kArmCurrentCap` 改成 `static_cast<int>(2.0 / 3.0 * 16384)`（10922，约 1.48 N·m）。上方注释改成：11-12 用 1.0 A 推到 +19.3° / −13.4° 停住，加到 2.0 A；6020 额定 1.62 A、持续堵转 0.90 A，所以只在转动时用满，不转时由堵转保护降下来。
   - `kArmSpeedGains` 的 kp 从 30 改成 300，ki 和两个上限不变。注释写原因：30 是 10-28 对空电机调的。装上摆臂后转动惯量大很多，kp 30 时速度环太软：到软限位刹不住，冲过的角度可能超过软限位到碰撞点的约 4°，电流加大以后更危险；积分作用压过比例作用，慢转会一顿一顿。按估计摆臂惯量是转子的 10 倍以上，kp 300 时环路增益和空电机 kp 30 时相近，不会更容易振；万一有嗡嗡声或高频抖动，按第 5 步的表往下调。
   - 新增：
     ```cpp
     // 摆臂堵转保护：电流超过 kArmStallCurrent、转速不超过 kArmStallRpm 持续 kArmStallTime，
     // 就把电流限到 kArmStallCurrent，直到控制器要的电流回到它以下（比如摇杆回中）。
     constexpr int kArmStallCurrent = static_cast<int>(0.8 / 3.0 * 16384); // 约 0.59 N·m，低于持续堵转 0.86 N·m。
     constexpr double kArmStallRpm = 2; // rpm，6020 输出轴。
     constexpr double kArmStallTime = 2.0; // s
     ```
2. 新建 `src/stall_guard.hpp`、`src/stall_guard.cpp`。要求：纯逻辑，不包含 LibXR、HAL、FreeRTOS 头文件；用 `#pragma once` 和 `namespace dart`；风格照 `src/magnet_guard.*`。
   ```cpp
   class StallGuard {
   public:
       // limit：限流后的电流（电调原始单位）；rpm：转速不超过它算“不转”；time：持续多久算堵转，单位 s。
       StallGuard(int limit, double rpm, double time);
       // 每拍调用：request 是控制器算出的电流，rpm 是反馈转速，dt 单位 s。
       // 返回允许下发的电流，绝对值不超过 |request|。
       int Update(int request, double rpm, double dt);
       bool Tripped() const;
   };
   ```
   规则：
   - 没触发时：`|request| > limit` 且 `|rpm| <= rpm` 就累计时间，否则时间清零。累计到 `time` 就触发，从这一拍起返回 `request` 夹到 ±limit 的值。
   - 触发后：`|request| <= limit` 时解除，时间清零，返回 `request`；否则返回夹到 ±limit 的值。
   - `rpm` 或 `dt` 不是有限数，或者 `dt < 0`：直接触发，返回夹到 ±limit 的值。
3. `src/hw/motor_port.hpp/.cpp`：
   - 新反馈帧到来时记下 `feedback.temp`，加 `double FeedbackTemp() const`，单位 °C。
   - 加 `void SetCommand(int command)`：覆盖本拍待发送的电流，只给堵转保护用。
4. `src/hw/launcher_hw.hpp/.cpp`：
   - `launcher_hw.hpp` 里 include `stall_guard.hpp`，加成员 `StallGuard arm_stall_{kArmStallCurrent, kArmStallRpm, kArmStallTime};`。
   - 在 `Tick()` 里 `arm_.Gate(...)` 之后、所有电机 `Send()` 之前加：
     ```cpp
     if constexpr (kArmInstalled) {
         const bool was_tripped = arm_stall_.Tripped();
         arm_.SetCommand(arm_stall_.Update(arm_.Current(), arm_.FeedbackRpm(), dt));
         if (arm_stall_.Tripped() && !was_tripped) {
             XR_LOG_WARN("摆臂堵转，电流限到 %.1f A", kArmStallCurrent * 3.0 / 16384);
         }
         if (!arm_stall_.Tripped() && was_tripped) { XR_LOG_INFO("摆臂电流限制解除"); }
     }
     ```
   - 监控快照加摆臂温度和 `arm_stall_.Tripped()`。状态行 `Dart arm cmd=… act=… rpm=… bus=…` 改成 `Dart arm cmd=%d act=%d rpm=%.0f tmp=%.0f stall=%d bus=%u`。参数类型照现有写法：`%.0f` 传 `static_cast<float>`，`%d` 传 `static_cast<int>`，`%u` 传 `static_cast<unsigned>`，否则编译不过。
5. `test/host_test.cpp`：
   - 提示文字为 "arm current cap stays under the GM6020 continuous stall torque" 的那条检查改成同时检查三项：`kArmCurrentCap <= 16384`；`kArmStallCurrent < kArmCurrentCap`；`kArmStallCurrent * 3.0 / 16384 * 0.741 < 0.86`。提示文字改成 "arm cap fits the 3 A command range and the stall guard current stays under the GM6020 continuous stall torque"。
   - 提示文字为 "arm keeps the bench speed gains and jog speed until they are tuned on the arm" 的那条检查：`kp <= 30` 改成 `kp <= 300`，提示文字改成 "arm speed gains are sized for the mounted arm (2026-11-12)"。其余摆臂参数检查不动。
   - 新增 `TestStallGuard()`，在 `main()` 里调用。测试里自己构造 `StallGuard`（例如 limit 100、rpm 2、time 1.0，dt 用 0.25），不依赖 `dart_params.hpp` 的值；dt 用二进制能精确表示的数，免得累加误差让“差一拍”的检查不稳。至少覆盖：
     - 电流不超过 limit 时，永不触发。
     - 超过 limit 且不转时：差一拍到 time 不触发；到 time 触发，并夹到 ±limit，正负两个方向都测。
     - 转速超过阈值时，计时清零。
     - 触发后 request 回到 limit 以内，就解除。
     - `dt` 为 NaN 或负数、`rpm` 为 NaN 时，直接限流。
6. 文档：README 的摆臂一段写上新电流、kp 和堵转保护；状态行说明加 `tmp`（6020 温度，°C）和 `stall`（1 表示堵转保护正在限流）。`handoff.md`“操作速查”第 8 条同步改。

## 第 3 步：打开松手保持
- 在 `ArmConfig()` 里 `return config;` 的前面加一行 `config.hold_on_release = true;`。
- 把“摆臂先不开松手保持……”那行注释换成新的说明：零点 2026-11-11 已按手转标定；不开时，松手后速度环每拍清零，摆臂没有保持力。
- README 里“摆臂先不开松手保持”那段改成已经打开，并补一句：在无力状态下把摆臂手推到软限位外面，再切到手动，摆臂会被拉回软限位（软限位离碰撞点只有约 4°）。
- 这项改的是适配层配置，主机测试测不到。照样跑一遍主机测试，确认没有改坏别的。
- 同一步还要防止摆臂“一下拉回去”：
  - 原因：开了松手保持以后，`HomeAt` 留下的目标是上电那一刻的位置。如果上电后在无力状态下手转过摆臂，再切到手动，摇杆为 0 时 `Jog(0)` 不会重新取位置，位置环会把摆臂一下拉回上电时的位置。
  - 改法：`Tick()` 里摆臂建零点那一行 `arm_.Axis().HomeAt(angle);` 改成 `if (arm_.Axis().HomeAt(angle)) { arm_.Axis().Stop(); }`，并写注释说明上面的原因。Stop 以后轴先不出力；第一次摇杆为 0 时，`Jog(0)` 按当时的位置保持。
  - 在 `TestHomeAtAndRebase()` 里加两条检查，说明为什么要 Stop。用 `SliderConfig()` 并设 `hold_on_release = true` 建两个轴，都先给反馈再 `HomeAt(p0)`，p0 在软限位内。反馈位置用 `axis.Feedback(axis.PositionToCounts(p), 0)` 给，位置用 `Near` 比较：
    - 第一个轴不 Stop：反馈换到 p1（软限位内，和 p0 不同）后 `Jog(0)`，`Target()` 仍等于 p0。这就是会拉回去的原因。
    - 第二个轴 HomeAt 后 `Stop()`：反馈换到 p1 后 `Jog(0)`，`Target()` 等于 p1。再 `Stop()`，反馈换到软限位外面，`Jog(0)` 后 `Target()` 等于软限位。
  - 这两条测的是现有 `MotorAxis` 的行为，改代码前后都应该通过。

## 第 4 步：测试、提交、编译，用户烧录并试转
1. 在 PowerShell 里运行下面的命令。DartLauncher 可以分两次提交：第 2 步一次，第 3 步一次；dart-fw 等两次都提交后再同步、编译、提交一次。每次提交前主机测试都要打印 `PASS`，否则不要提交，停下汇报。
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   (Select-String -Path test\host_test.cpp -Pattern 'Check\(').Count
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   git add src test README.md
   git commit -m "<本次改动>"
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   xrobot gen -c User/RobotConfig/artisans_dart.yaml
   cmake --build --preset debug
   git add xrobot.lock
   git commit -m "更新 DartLauncher（<本次改动>）"
   ```
   - `Select-String` 那一行应该没有任何输出。
   - 每次 `git commit` 之后，运行 `git rev-parse --short HEAD`、`git branch --show-current`、`git status --porcelain`，核对提交确实写进去了，而且还在原分支上。
   - xrobot 会提示“安装 1.0.0 与固定 1.0.1 不同”，退出码是 0 就继续。
   - `log --oneline -1` 必须是刚才在 DartLauncher 里的提交，不是就停下汇报。
   - 只在当前分支上提交，不新建分支。
2. 烧录：把下面的命令给用户，让用户在自己的 PowerShell 里运行。Codex 沙箱里 OpenOCD 连不上调试器：11-11 两次都报 `cannot read IDR`，用户在自己的终端里烧就成功了。给命令时提醒一句：烧录会复位，电机会松开，手离开夹爪，上着膛要先退膛。输出里出现 `** Verified OK **` 才算烧上。
   ```powershell
   & "C:\Users\Lenovo\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe" -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset exit"
   ```
3. 让用户这样试转：
   - 右拨杆拨到中，左拨杆拨到下。摆臂转动范围里不要放手和镖。一只手放在右拨杆上，不对就拨到下（无力）。
   - 推的时候看着摆臂上推杆舵机和电磁铁的线，被扯紧就拨到无力。电流比上次大一倍，扯坏的风险也大。
   - 先半杆往右推 2 秒，用拇指扶着摇杆慢慢回中，不要直接松手让它弹回。有嗡嗡声或抖动，就拨到无力，停止试转。
   - 左摇杆往右推满，摆臂停下后再推 2 秒，然后慢慢回中，等 5 秒。
   - 往左同样做一遍。
   - 推回中间附近，慢慢回中，等 5 秒。
   - 最后摸一下 6020 外壳，说烫不烫。
4. 日志：用户的串口工具一直开着。让用户把这段日志交给 DeepSeek，提问用文末的模板，再把 DeepSeek 的回答和自己看到的情况一起贴回来。

## 第 5 步：判断和调参
- 达到成功标准：去第 6 步。
- 推满后停在软限位前 5° 以内，再推时出现“摆臂堵转”：算正常。离限位越近，允许的速度越小，阻力大时摆臂会停在差一点的地方，堵转保护把电流降到 0.8 A。
- 推满时停在离软限位还差 5° 以上的地方，`cmd` 顶在上限，出现“摆臂堵转”事件：
  - 当前是 2.0 A：把 `kArmCurrentCap` 改成 `16384`（3.0 A，6020 的最大值，约 2.2 N·m），回到第 4 步再试一轮。
  - 当前已经是 3.0 A：停下汇报 1.0 A、2.0 A、3.0 A 三次的停住角度，不再加。
- 摇杆慢慢回中后，摆臂往中间回超过 1°；或者摆臂停着不动时出现“摆臂堵转”：停下汇报，不调参数。这说明有把摆臂往中间拉的力。
- 推住摇杆时在软限位附近来回抖：停下汇报，不改参数。多半是到了限位、速度目标被夹成 0 以后，速度环每拍清零；改法涉及所有轴，要另开任务。
- `tmp=` 到 60，或者外壳烫手：停止试转，等凉下来，汇报。
- 其他情况（能到限位但不平滑等），按现象改 `src/dart_params.hpp`。每轮只改一项，改完回到第 4 步：

  | 现象 | 改什么 |
  |---|---|
  | 有嗡嗡声、高频抖动 | `kArmSpeedGains` 的 kp：300 → 150 → 100 |
  | 动起来仍一顿一顿，没有嗡嗡声 | `kArmSpeedGains` 的 kp：300 → 500 |
  | 起步慢、跟不上 | `kArmSpeedGains` 的 ki：0.5 → 1.0 |
  | 松手后保持太软（手轻推就偏几度） | `kArmPos` 的 kp：0.05 → 0.1 → 0.2；松手后来回晃，退回上一档 |
  | 推满太快，不好控制 | `kArmSpeed`：2 → 1 |

- 改了速度环 kp，就把 `test/host_test.cpp` 里提示文字为 "arm speed gains are sized for the mounted arm (2026-11-12)" 的那条检查的 kp 上限改成本轮的值。
- 调参最多 4 轮（加电流那一轮不算）。4 轮后还不达标，停下汇报每一轮的参数和现象。

## 第 6 步：收尾
- 更新 `handoff.md`：
  - “当前状态”里摆臂那一段。
  - “仓库与产物”表：两个仓库的 HEAD，`build/debug/DevC.elf` 的编译时间和 SHA-256，板上固件。
  - “提交记录”表加一行。
- README 的摆臂段写上最终参数和试转结果。
- 本文件末尾写执行进度。
- 汇报完整写在最后一条回复里，内容：
  - 改了哪些文件。
  - 主机测试的 PASS 数，`Check(` 行数改前和改后，改动或新增了哪些检查。
  - 两个仓库每个提交的哈希和主题。
  - 每一轮的参数和现象（含停住角度、`tmp` 最大值），最终参数。
  - 还要现场看的项。

## 停止条件
- 3.0 A 仍停在离软限位 5° 以外。
- 摇杆慢慢回中后摆臂还往中间回，或者停着不动时堵转保护触发。
- 推住摇杆时在软限位附近来回抖。
- 摆臂碰到机构，电机发烫（`tmp=` 到 60），或者电机离线、故障反复出现。
- 调参 4 轮仍不达标。

## 附：DeepSeek 提问模板
新开 DeepSeek 对话时，先发这段格式说明：
```
下面是飞镖发射架 C 板的串口日志，每秒打印一组以 "Dart" 开头的状态行。日志工具会在每段前加 [hh:mm:ss] <<，一行常被拆成几段（"Dart" 可能拆成 "D" 和 "art"），请先拼回整行；开头几行可能是旧缓存，忽略。
- "Dart mode=… fault=…"：mode 0 无力、1 手动、2 自动；fault 正常是“无故障”。
- "Dart sw R=… L=… sticks a b c d wheel …"：R、L 是右、左拨杆（U 上、M 中、D 下）；sticks 四个数依次是右摇杆横、右摇杆纵、左摇杆横、左摇杆纵，范围 -1~1。
- "Dart online=a/b/c/d/e …"：第 5 个数表示摆臂电机 6020 是否在线。
- "Dart arm=… raw=… …"：arm 是摆臂角度（度），raw 是 6020 编码器原始值 0~8191。
- "Dart arm cmd=… act=… rpm=… tmp=… stall=… bus=…"：电流指令、电机回报的实际电流（16384 对应 3 A）、转速、电机温度（°C）、堵转保护（1 表示正在限流）。
- 不以 "Dart" 开头、带中文的行是事件日志。
```

试转：
```
这次测试：右拨杆中、左拨杆下。左摇杆横向（sticks 后面 4 个数里的第 3 个）先半杆往右推 2 秒，慢慢回中；再往右推满，摆臂停下后再推 2 秒，慢慢回中，等 5 秒；往左同样；最后推回中间附近，慢慢回中，等 5 秒。请只回答：
1. mode 和 fault；有故障就原样引用那几行。
2. 每次推杆时 arm= 从多少走到多少，最后停在多少。
3. 每次摇杆回到 0 以后 5 秒内，arm= 变了多少度。
4. "Dart arm cmd=…" 那一行：推杆期间 cmd 绝对值一般是多少、最大是多少，有没有顶在同一个最大值超过 1 秒；摇杆回到 0 以后 cmd 的范围；rpm 的最大绝对值；tmp 的最大值；stall 有没有变成 1。
5. 有没有出现“摆臂堵转”“摆臂电流限制解除”；出现的话写时间，并原样引用。
```

## 执行进度

本会话未提供计划工具，以下清单记录进度。

- [x] 预检、记录基线与 Referee 现有改动。
- [x] 复现并实现堵转保护、保持配置与监控，完成主机验证。
- [x] 分别提交模块与固件，核对副本并编译固件。
- [x] kp 150 固件已重新编译、烧录并只读校验。
- [ ] 查明实车下掉和阻挡的原因，根据结果完成第 5、6 步的验收和最终参数记录。

- 2026-11-12 预检：DartLauncher `main` / `804c7c1`、dart-fw `dart-launcher` / `3ea7210`，两仓库干净；Referee 原有 `Referee.hpp`、`RefereeTypes.hpp`、`tests/types_test.cpp` 三处修改保留。
- 已修改 DartLauncher 的 `src/dart_params.hpp`、`src/hw/motor_port.hpp/.cpp`、`src/hw/launcher_hw.hpp/.cpp`、`test/host_test.cpp`、`README.md`，新增 `src/stall_guard.hpp/.cpp`；根目录 `handoff.md` 与本文件已更新。
- 保持行为检查先在原实现上通过（`PASS: 21565 checks`）；堵转测试在不限制电流的占位实现上先报 `FAIL: positive stall trips and clamps on the timeout tick`，实现后主机测试 `PASS: 21596 checks`。`Check(` 551 → 570；两条参数检查更新，新增堵转限流、计时复位、正反向、异常输入和零点后保持检查；纯逻辑依赖扫描无输出。
- 提交：DartLauncher `47dec109b99fd9ae56702b4a5b491eabb67499ea`（`摆臂增加堵转限流和松手保持`）；dart-fw `f2340a869204a4b71b3a14b643c174770bfcf5ac`（`更新 DartLauncher 摆臂堵转保护和保持`）。模块副本 HEAD 为 `47dec10`，两仓库均在原分支且干净。
- `xrobot setup --update local/DartLauncher`、`xrobot gen -c User/RobotConfig/artisans_dart.yaml` 和 `cmake --build --preset debug` 成功；XRobot 提示安装 1.0.0 与固定 1.0.1 不同，退出码均为 0。`build/debug/DevC.elf` 编译时间 2026-11-12 02:35:28（UTC+8），SHA-256 `819A819326DBD3A7E5514BAB4DDCBCD0275DAD44886C988B88DC50009A16E29C`。
- `python C:/Users/Lenovo/.agents/skills/build-cmake/scripts/cmake_builder.py --scan-artifacts C:/Users/Lenovo/Desktop/XROBOT/dart-fw/build/debug --no-save-profile` 首次失败：`ModuleNotFoundError: No module named 'tool_config'`；指定已安装的共享目录到 `PYTHONPATH` 后重试成功，识别 `DevC.elf` 为首选产物。
- 02:35 状态：最后已知板上为 `804c7c1` / `3ea7210`。此前 1.0 A 试转右停 +19.3°、左停 −13.4°，最高温度未记录；当时 2.0 A、kp/ki 300/0.5 尚未烧录、试转。

### 2026-11-12 调参第 1 轮

- 用户此前运行 `openocd.exe -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf verify reset exit"` 时报告 `unable to find a matching CMSIS-DAP device`，当次初始化失败；随后反馈 kp 300 试转有嗡鸣、抖动，+48.74° / −43.06° 附近触发堵转保护后下掉一段再慢慢爬上去，`tmp` 最大值未提供。
- 按用户要求只将 `kArmSpeedGains.kp` 和相应测试上限从 300 降到 150，更新 README；电流上限、ki 和其他参数不变。先改单项测试时按预期报告 `FAIL: arm speed gains are sized for the mounted arm (2026-11-12)`，修改参数后 `PASS: 21596 checks`，`Check(` 570 → 570。
- DartLauncher `0af6f896f77753873afe43a8c0e14129e534bf2e`（`摆臂速度环 kp 降到 150`）；dart-fw `d401f01ab5a31c2ffac3a27a0995f88a570145e8`（`更新 DartLauncher 摆臂速度环 kp 150`）。两仓库仍在原分支且干净，Referee 原有三处改动保留。
- 固件于 2026-11-12 02:55:12 编译，`build/debug/DevC.elf` SHA-256 为 `7D422B9FA898755019526E4561ECC40DC029A7B73C95DA817B607F6B604D4B3E`。烧录脚本报告成功并校验通过，但截断了 OpenOCD 的原始验证行。
- 独立检查 `openocd.exe -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "init; verify_image C:/Users/Lenovo/Desktop/xrobot/dart-fw/build/debug/DevC.elf; shutdown"` 时目标未暂停，报告 `target not halted` / `error executing cortex_m crc algorithm`；随后暂停目标再执行 `verify_image`，成功后恢复运行并由校验命令输出 `** Verified OK **`。
- 当时 kp 150 尚未实车试转；尚未进行 3.0 A 或其他调参轮次。

### kp 150 实车反馈与当前状态

- 用户反馈试转时左拨杆下、右拨杆中：感觉电机使不上劲，摆臂上到最高处后向下掉；有力状态下手拨时感到被顶住，认为最高处尚未到两边取镖位置。本次运动过程没有记录串口日志，最高角度、保持变化、堵转状态及试转时最高温度未知。
- 此前 kp 300 一轮报告的 +48.74° / −43.06° 数值分别超过源码标定的右取镖 +46.2° / 左取镖 −40.5°；与这次对实际取镖位置的观察需要结合位置反馈和现场对位核对，不能用不同轮次的数值推断本次已到位。
- 在用户停止操作后，用 `python C:/Users/Lenovo/.agents/skills/serial-monitor/scripts/serial_monitor.py --port COM46 --baud 115200 --duration 5` 采集当前状态：`mode=0 fault=遥控器离线`，`remote ok=3984` 在采样期间未增加；摆臂在线，`arm=31.32 raw=2655`、`cmd=0 rpm=0 tmp=23 stall=0`。脚本因日志里的遥控器离线返回 1；快照不能还原试转时的情况，遥控器离线期间的拨杆值也不能当作实时输入。
- 手动有力且摇杆回中时摆臂会按位置保持；堵转保护在请求电流超过 0.8 A 且转速不超过 2 rpm 持续 2 s 时触发，慢转也可能触发。当前没有证据区分保持力、限流和实际机械干涉；先暂停有力试转和调参，由机械同学在无力状态下检查是否碰撞或线束受拉，不强推摆臂。

### 2026-11-12 堵转限流调整

- [x] 核对手册中的持续堵转电流及现有堵转保护规则。
- [x] 将保护限流从 0.80 A 调到 0.85 A，保留速度阈值与计时，并验证主机测试。
- [x] 本地提交模块、同步固件并编译。
- [x] 烧录新固件并校验。
- [x] 记录现场试转结果；因限位附近回摆和保持漂移触发停止条件，暂停继续调参。

- 用户说明“顶住”是手上感觉到反力，要求调大堵转保护。新源码将限流值设为 0.85 A（4642），事件日志打印两位小数；手册注明的持续堵转电流为 0.90 A，手册数据在 25°C、正常散热条件下测得。运行上限 2.0 A、速度环 kp/ki 150/0.5、堵转阈值 2 rpm 和 2 s 均未改。
- 主机测试 `PASS: 21596 checks`，`Check(` 行数 570 → 570；参数检查增加限流值低于 0.90 A。固件 `cmake --build --preset debug` 成功，`build/debug/DevC.elf` 于 2026-11-12 12:39:53 生成，SHA-256 `88A0E5A92E23C8236A88165602A2B85C97C0381E3F7AE0558065380C170C1046`。
- DartLauncher `a4387568b2b7f344ba718eb817b291ed7a49de88`（`摆臂堵转限流提高到 0.85 A`）；dart-fw `d2fb1464124b961adc7222f1fcec1e88fb163937`（`更新 DartLauncher 堵转限流至 0.85 A`）。两仓库在原分支且干净；Referee 三处原有改动保留。
- 用户要求烧录后，2026-11-12 12:45 使用 CMSIS-DAP / `target/stm32f4x.cfg` 写入该 ELF；OpenOCD 输出 `** Programming Finished **`、`** Verified OK **`、`** Resetting Target **`，退出码 0。
- 烧录后 `python C:/Users/Lenovo/.agents/skills/serial-monitor/scripts/serial_monitor.py --port COM46 --baud 115200 --duration 4` 显示 `mode=0 fault=遥控器离线`、`remote ok=0`；摆臂在线，`arm=31.03 raw=2648 cmd=0 rpm=0 tmp=20 stall=0`。脚本因检测到遥控器离线返回 1；该记录发生在试转之前。

### 0.85 A 试转日志分析

- 用户提供了 12:46:41–12:47:24 的串口日志；重新拼接被 `[时间] <<` 拆开的行，同一组 `Dart` 行来自同一控制快照。手动试转期间 `mode=1 fault=无故障`，摆臂在线；12:47:20 切无力，12:47:22 起遥控器离线。`tmp` 采样最大 24°C。
- 左摇杆保持向右满杆时，12:47:12 摆臂 `+48.68° cmd=9148 stall=0`，12:47:13 回到 `+41.09° cmd=3695 stall=0`，12:47:14 又到 `+44.65° cmd=6645 stall=0`；先回摆 7.59°，之后又上升，不能把该段回摆归因于已触发的堵转限流。采样约每 1.16 s，无法还原两次快照之间的瞬时过程。
- 12:46:51 和 12:47:15 各报一次“摆臂堵转，电流限到 0.85 A”；后一次下一组快照在 `+46.63° cmd=4642 act=4647 stall=1`，随后在 `+45.65°` 附近保持至切无力前，距右软限位 3.37–4.35°，属于任务所说距限位 5° 内的触发。两次分别在 12:46:53 和 12:47:19 报“摆臂电流限制解除”。
- 左侧采样最低 `−42.79°`，右侧最高 `+48.68°`；数值已越过记录中的取镖角 `−40.5° / +46.2°`，不代表现场取镖已经对准。绝对值最大采样指令为 `9615`（约 1.76 A），`act` 最大绝对值为 `9751`；采样不能排除其间存在更大电流。
- 12:47:02 到 12:47:05 摇杆中位时，`arm=+3.86° → +5.01°`，约 3 s 变化 1.15°，超过 5 s 内不超过 1° 的成功标准；右侧前一采样为 `+47.54°`、摇杆回中后的下一采样为 `+41.84°`，但回中瞬间的位置未知。
- 限位附近推住摇杆时回摆，符合第 5 步停止条件；暂停有力试转和参数调整，不提高到 3 A。需另行分析软限位控制、负载或其他回摆原因，当前日志不足以确定机制。
- 日志结束后现场 COM46 复查 `python C:/Users/Lenovo/.agents/skills/serial-monitor/scripts/serial_monitor.py --port COM46 --baud 115200 --duration 3` 显示 `mode=0 fault=遥控器离线`、摆臂在线、`arm=31.51 cmd=0 rpm=0 tmp=21 stall=0`；脚本因遥控器离线返回 1。

### 2026-11-12 松手下掉排查（未完成）

- [x] 用只读 SWD 记录现有固件的摇杆、角度、速度环积分、电流和堵转状态。
- [x] 实现候选保持切换，并检查软限位、堵转保护和双向负载仿真。
- [x] 复核设备无力状态并记录未通过的检查。
- [ ] 设计并验证不会造成限位超程的控制方案；完成前不编译烧录候选版本，不做有力试转。

- 采样文件 `plans/tasks/diagnostics/arm-20261112-131718-736492.csv` 来自板上 `a438756` / `d2fb146`，采样中的多个字段分次读取，不是严格原子快照。摇杆信号从约 0.0515 进入死区时，速度环积分从约 8929 降到约 38，电流指令从 8965 降到 1050，角度随后降到约 +41.22°；`Jog(0)` 切换保持时清积分能解释这次电流突降。
- 非零摇杆推到软限位时，点动允许速度也会变为零，并在 `MotorAxis::Update()` 中清除积分；现有候选只改变松手切换，不能宣称修复了持续推杆时的回摆。采样中右拨杆切到无力后角度从约 +46° 降到 +27.42°，需用可靠支撑防止无力时下落。
- DartLauncher 工作区候选方案在低速松手时保留最多 0.85 A 的积分，并在软限位处禁止向外电流；此修改**尚不可烧录**。主机测试失败：`FAIL: bounded release support holds an idealized load below the stall limit`。固定惯量 0.05 kg·m²、负载 0.55 N·m、从距限位 1° 处释放的理想化测试最大偏移约 1.061°，超过 1° 标准。
- 额外诊断 `plans/tasks/diagnostics/arm-release-simulation.cpp` 假定外部阻挡使积分累积后在距限位 1° 处突然解除，并使用未实测的惯量、阻尼和 1 rpm 量化反馈；在惯量 0.05 kg·m² 的双向零负载模型中，越过软限位约 12.56°。这是故意构造的边界情形，不是实车运动预测，但说明禁止向外电流本身不能保证惯性停在安全范围内。临时尝试在限位处清积分或反向制动后仍未满足要求，均已撤回到原候选方案。
- 项目现有 `simulation/dart_sim/model/dart_launcher.xml` 是 10-12 的旧 CAD 模型，零件密度和摆臂电机惯量含假设，不能当作实车测量。在 `+49°` 处其质量矩阵的摆臂对角项约 0.00923 kg·m²（含人为设置的 0.002 kg·m² armature）；据此做同一构造性仿真，零负载越过软限位约 11.10°，固定 0.17 N·m 负载时约 1.32°。
- 仅在诊断模型中测试提前 0.2° 全限流反向制动：上述小惯量、0.17 N·m 假设下 5 s 内电流方向换了 602 次；惯量 0.05 kg·m²、零负载下偏移仍达 2.72°。这一试验没有进入固件，也不能作为实车表现的预测。
- `python -m dart_sim.arm_force` 首次因 GBK 无法编码 `²` 报 `UnicodeEncodeError`，在 PowerShell 设置 `$env:PYTHONUTF8 = '1'` 后成功；该旧 CAD 仿真在 50° 给出空载重力矩约 0.172 N·m、吸镖约 0.312 N·m，仅供建模参考，未包含实车未知阻力，不能用来提高堵转限流。
- COM46 / 115200 当前只读采样显示遥控器在线、`R=D L=D`、`mode=0 fault=无故障`，摆臂约 +0.79°、`cmd=0 rpm=0 tmp=24 stall=0`。`serial_monitor.py --port COM46 --baud 115200 --duration 5` 返回 1，因为脚本将包含 `fault=无故障` 的行误识为错误；串口本身正常。
- 板上和 `dart-fw/build/debug/DevC.elf` 仍为 `a438756` / `d2fb146`、SHA-256 `88A0E5A92E23C8236A88165602A2B85C97C0381E3F7AE0558065380C170C1046`。本轮无新提交、无固件编译或烧录。右拨杆保持下位；无力时摆臂需要可靠支撑，避免自然下落。
