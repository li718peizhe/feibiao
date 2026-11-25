# 任务 3q：推杆标定 + 自动取镖试运行（先左后右）

只读这一个文件就够了。不要读 `plans/dart-fw-libxr.md`、`handoff.md` 和别的任务文件，也不要运行 graphify（包括只读查询）。做完写汇报，然后停下。

## 背景
- 工作区 `C:\Users\Lenovo\Desktop\xrobot`（本身不是 git 仓库）。
- 模块仓库 `xrobot-local\DartLauncher\`：git，分支 `main`，HEAD `2cc1764`，主机测试 `PASS: 21702 checks`。
- 固件工程 `dart-fw\`：git，分支 `dart-launcher`，HEAD `be3d3ee`。模块检出在 `Modules\local\DartLauncher`，不要改这份副本，不要手改 `xrobot.lock`。板上现在跑的就是这一版。
- 两个仓库都在当前分支上直接提交，不新建、不切换分支。
- 代码规范：注释用中文；不新增宏；一行一条语句；写法和周围代码一致。`src\` 是纯逻辑，不能引用 LibXR；`src\hw\` 是适配层，主机测试不编译它，只有固件编译检查它。
- 固件日志 `XR_LOG_INFO` / `XR_LOG_WARN` 一条最多 64 字节（一个汉字 3 字节），新加的日志每条不超过 18 个汉字。

## 为什么要做
- 摆臂（GM6020）在任务 3p 调好了：2026-11-12 23:43 实车追踪里两侧镖座松手几乎不动（0~0.04°），角度和编码器原始值一致（±0.1°）。操作手现在手动取镖：装弹组（右拨杆中、左拨杆下）左摇杆横向把摆臂摆到镖座，右摇杆往上推，推杆（TD-8130MG 舵机）下探，磁铁（断电吸持、通电放开）吸住镖，推杆收起就算取到了。
- 用户要自动取镖，先左后右。磁铁一次只能吸一发；把镖装到发射机构上要用的放镖推杆脉宽、滑块抬升高度还没实测。所以这次做“取镖试运行”：每侧取起来、举着停一下、再放回原镖座，左边做完做右边，最后摆臂回 0°。以后的任务再把“放回”换成装到发射机构上。
- 推杆现在是未标定的原始脉宽模式（`kCrankServoCalibrated=false`）：流程里的推杆步骤不出力、只会超时；推杆角报无效值，发射被“推杆没有收起”拦下。11-12 用户确认过：收起约 1325 µs，右镖位 2296 µs，左镖位 2335 µs，脉宽变大是往下探（方向 +1）。这次按它们标定。
- `kReloadInstalled` 现在等于“摆臂、推杆、磁铁都装了且推杆标定过”。推杆一标定它就变成 true，发射会走完整的自动换弹（发射后自动装填、预取，要压滑块、开夹爪），那部分没实测，不能打开。所以加 `kLoadCalibrated=false` 挡住。
- 流程里的摆臂移动现在是 `SetTarget` 一步把目标跳到终点，只有位置环比例项，没有速度前馈和摩擦前馈；摆臂静摩擦约 0.45 N·m，最后一点要靠积分慢慢攒。手动点动（速度前馈、约 0.3 N·m 摩擦前馈、目标最多领先 2°）在实车上很顺。所以给 `MotorAxis` 加 `MoveTo`：像推摇杆一样走向终点，离终点越近越慢，到了和松手一样保持。
- 镖座保持电流常常正好卡在 1.2 A（电机凉时的堵转限流值），堵转保护 2 s 后触发，触发期间电流限在 1.2 A。从左镖座摆到右镖座要爬坡，需要超过 1.2 A，会爬不上去。所以流程每开始一段摆臂移动，让堵转保护重新计时。

## 做完以后怎么用
- 右拨杆中（手动）、左拨杆下（装弹组），拨轮往上推到头保持 2 s：开始自动取镖试运行。这个手势原来是“摆臂回零”，摆臂按编码器绝对角本来不用回零，只打一条提示。
- 运行中推任何摇杆超过 0.1、拨左拨杆、遥控器掉线：停下（取消流程，不报故障，交回手动）。右拨杆打到下（无力）也直接停下，不暂停以后接着做：摆臂断力会掉，重新上力以后不能在掉下来的位置接着下探。
- 一共 23 步：磁铁断电吸持 → 推杆收起 → 摆到左侧镖座 → 下探 → 等磁铁吸稳 → 抬起 → 举着停一下 → 下探放回 → 磁铁通电放镖 → 等镖脱开 → 推杆收起 → 磁铁断电 → 右侧同样 10 步 → 摆臂回 0°。

## 要改的地方（DartLauncher）

### 1. `src\dart_params.hpp`
1. `kFetchRight` 改成 `Radians(46.8)`，注释：`// 11-12 17:38 用户确认的右镖位（raw 3008）；23:43 追踪手动取镖在 46.1°~47.2°。`。`kFetchLeft` 数值不变，注释改成：`// 11-11 手转 raw 1021；23:43 追踪左镖座约 −40.3° 开始碰到，不再往外压。`。改了 `kFetchRight` 以后如果有已有检查失败，把它改回 `Radians(46.2)`，在汇报里写明，接着做（这条不算卡住）。
2. 删掉 `kCrankFetch` 那一行，删掉 `kCrankServoOffset`、`kCrankServoDirection` 那两行（下面第 5 条重新定义）。`kCrankRetracted = 0` 保留，注释改成 `// 推杆机构角 0 是收起，对应 kCrankPulseRetracted。`；`kCrankSeat` 保留，注释改成 `// 放镖位，还没实测；只有自动换弹用，kLoadCalibrated 为 false 时用不到。`
3. `kCrankServoCalibrated` 改成 `true`，注释：`// 2026-11-13 任务 3q 按实测脉宽标定（见 kCrankPulseRetracted）；重装舵盘以后改回 false 重新量。`
4. 在 `kReloadInstalled` 前面加：
   ```cpp
   // 放镖推杆脉宽 kCrankSeat 和滑块抬升高度 kLoadPosition 还没实测；量好以前不自动换弹，发射后也不自动装填。
   constexpr bool kLoadCalibrated = false;
   ```
   `kReloadInstalled` 改成 `kArmInstalled && kCrankInstalled && kMagnetInstalled && kCrankServoCalibrated && kLoadCalibrated`，它上面的注释跟着改。
5. 在 `kCrankPulseRate` 那行后面加（必须在 `kServoPulseMin`、`kServoPulseMax`、`kServoRange` 定义之后）：
   ```cpp
   // 任务 3q：推杆按 2026-11-12 实车脉宽标定。脉宽变大往下探（方向 +1），机构角 0 是收起；
   // 机构角和脉宽按舵机 270° 对应 2000 µs 换算。取不到镖或压得太深就改两个镖位脉宽。
   constexpr double kCrankPulseRetracted = 1325; // µs，15:26 用户确认收起。
   constexpr double kCrankPulseFetchLeft = 2335; // µs，17:44 用户确认的左镖位。
   constexpr double kCrankPulseFetchRight = 2296; // µs，17:38 用户确认的右镖位。
   constexpr double kCrankServoDirection = 1;
   constexpr double kCrankServoOffset = (kCrankPulseRetracted - kServoPulseMin) /
       (kServoPulseMax - kServoPulseMin) * kServoRange; // rad，收起时的舵机角。
   // 推杆脉宽 µs 换成机构角 rad，收起为 0。
   constexpr double CrankAngleFromPulse(double pulse) {
       return (pulse - kCrankPulseRetracted) / (kServoPulseMax - kServoPulseMin) * kServoRange;
   }
   constexpr double kCrankFetchLeft = CrankAngleFromPulse(kCrankPulseFetchLeft);
   constexpr double kCrankFetchRight = CrankAngleFromPulse(kCrankPulseFetchRight);
   // 手动推杆的范围和速度和原始脉宽模式一样：最深到 kCrankRawPulseMax，满杆每秒 kCrankPulseRate。
   constexpr double kCrankManualMax = CrankAngleFromPulse(kCrankRawPulseMax);
   constexpr double kCrankManualSpeed = kCrankPulseRate / (kServoPulseMax - kServoPulseMin) * kServoRange; // rad/s
   ```
6. 在 `kArmTimeout` 那行后面加：
   ```cpp
   // 任务 3q：流程里的摆臂移动和自动取镖试运行，都待实车调。
   constexpr double kArmAutoSpeed = Radians(60); // rad/s，流程摆臂巡航速度；11-12 23:43 追踪手动大幅摆动平均 40~66°/s。
   constexpr double kArmMoveCreep = Radians(4); // rad/s，离终点很近时的最低速度；摩擦大，太慢会停在半路。
   constexpr double kArmMoveTolerance = Radians(0.3); // rad，离终点这么近（或已越过终点）就和松手一样保持。
   constexpr double kFetchArmTimeout = 6; // s，从一侧镖座摆到另一侧约 2.5 s。
   constexpr double kFetchGripTime = 0.5; // s，下探到位以后等磁铁吸稳。
   constexpr double kFetchShowTime = 1.0; // s，抬起来举着停一下，看镖有没有吸住。
   constexpr double kFetchReleaseTime = 0.5; // s，放回时磁铁通电以后等镖脱开再收推杆。
   constexpr double kFetchMaxTemp = kArmBoostFullTemp; // °C，6020 比它热就不开始：右镖座要约 1.06 A，热了堵转限流会降。
   ```

### 2. `src\motor_axis.hpp` / `src\motor_axis.cpp`
1. `AxisConfig` 末尾加：
   ```cpp
       // MoveTo 用（jog_moves_target 的轴）：离终点很近时的最低速度（m/s 或 rad/s）和算走到的距离；任一为 0 时不能用 MoveTo。
       double move_creep = 0;
       double move_tolerance = 0;
   ```
2. `MotorAxis` 公开部分，`HoldPosition()` 后面加：
   ```cpp
       // jog_moves_target 的轴回零以后用：像推摇杆一样以 speed（单位/s）走向 goal（软限位夹紧），离终点越近越慢
       // （距离 × approach_gain，最低 move_creep），离终点不到 move_tolerance 或已越过终点时和松手一样保持。
       // 点动、SetTarget、RampTo、HoldPosition、Stop、断力和离线都会结束它；不能用时返回 false。
       bool MoveTo(double goal, double speed);
       // MoveTo 还没走到。
       bool Moving() const { return moving_; }
   ```
   私有部分加：
   ```cpp
       // MoveTo 每拍按离终点的距离设点动速度，走到了就和松手一样保持。
       void StepMove();
       bool moving_ = false;
       double goal_ = 0, move_speed_ = 0;
   ```
3. `motor_axis.cpp` 新增：
   ```cpp
   bool MotorAxis::MoveTo(double goal, double speed) {
       if (!Homed() || !config_.jog_moves_target || !feedback_valid_ || !std::isfinite(goal) ||
           !std::isfinite(speed) || speed <= 0 || config_.move_creep <= 0 || config_.move_tolerance <= 0) {
           return false;
       }
       const double end = Clamp(goal, config_.minimum, config_.maximum);
       const double remaining = end - Position();
       if (std::fabs(remaining) <= config_.move_tolerance) {
           return HoldPosition();
       }
       // 和推摇杆一样起步：从保持接着走时沿用目标和积分，顶着这次方向的松手预压扣回。
       if (!Jog(std::copysign(config_.move_creep, remaining))) {
           return false;
       }
       goal_ = end;
       move_speed_ = speed;
       moving_ = true;
       return true;
   }
   void MotorAxis::StepMove() {
       const double remaining = goal_ - Position();
       // 走到了，或者越过了终点（和点动方向相反）：和摇杆松手一样按当前位置保持，几乎不动时电流保持连续。
       if (std::fabs(remaining) <= config_.move_tolerance || remaining * jog_ < 0) {
           HoldPosition();
           return;
       }
       const double speed = std::fmax(std::fmin(move_speed_, std::fabs(remaining) * config_.approach_gain),
                                      config_.move_creep);
       jog_ = std::copysign(speed, remaining);
   }
   ```
4. 其他改动：
   - `Jog()`、`HoldPosition()` 的第一行加 `moving_ = false;`。`HoldPosition()` 别的一点都不改。
   - `SetTarget()`、`RampTo()`、`StartHome()`、`HomeAt()` 成功设置状态的地方，`Stop()` 开头，`ClearFault()` 清故障的地方，都把 `moving_` 置 false。
   - `Update()`：离线分支、不使能分支都把 `moving_` 置 false；最后的 `if (config_.jog_moves_target) { return FollowTarget(dt); }` 改成先 `if (moving_) { StepMove(); }`，再 `return FollowTarget(dt);`。
   - `FollowTarget()` 点动那段，在按 `jog_lead` 夹紧 `next` 之后、`target_ = end_ = next;` 之前加：
     ```cpp
             // MoveTo：目标不越过终点。
             if (moving_) { next = jog_ > 0 ? std::fmin(next, goal_) : std::fmax(next, goal_); }
     ```
   - 别的不动。

### 3. `src\stall_guard.hpp` / `src\stall_guard.cpp`
加一个公开函数：
```cpp
    // 回到未触发、重新计时；流程每开始一段摆臂移动时用。
    void Reset();
```
实现：`tripped_ = false; recovering_ = false; elapsed_ = 0;`（三行）。

### 4. `src\launch_sequence.hpp` / `src\launch_sequence.cpp`
1. `hpp` 公开部分，`CommandSalvo` 后面加：
   ```cpp
       // 自动取镖试运行（任务 3q）：先左后右，每侧取起来、举着停一下、再放回原镖座，最后摆臂回 0°；
       // 只要摆臂建了零点，不要求滑块和 pitch 回零。忙、有故障、库存记着磁铁上有镖时返回 false。
       bool CommandFetchTest();
   ```
   `enum class Action` 末尾加 `Magnet, Wait`；`enum class Condition` 末尾加 `Wait`。私有部分加 `bool FetchTestSteps(int side);`，成员加 `bool fetch_test_ = false;`（和 `decocking_`、`cocking_` 放在一起）。
2. `CommandFetchTest()`：
   ```cpp
   bool LaunchSequence::CommandFetchTest() {
       // 库存记着磁铁上有镖时不能去镖座下探，会撞上镖座里的镖。
       if (!Accept() || inventory_.Held()) {
           return false;
       }
       const Step start[]{
           {"取镖试运行：磁铁断电吸持", Action::Magnet, Condition::Immediate, 0},
           {"取镖试运行：推杆收起", Action::Crank, Condition::Crank, kCrankRetracted, kServoTimeout}
       };
       const Step finish{"取镖试运行：摆臂回 0°", Action::Arm, Condition::Arm, 0, kFetchArmTimeout};
       fetch_test_ = Append(start, 2) && FetchTestSteps(-1) && FetchTestSteps(1) && Append(&finish, 1);
       return fetch_test_;
   }
   ```
3. `FetchTestSteps(side)` 用 `Append` 加 10 步，返回 `Append` 的结果。`side < 0` 用下面这些（`side > 0` 把名字里的“左”换成“右”，目标换成 `kFetchRight`、`kCrankFetchRight`）：

   | 名字 | Action | Condition | target | timeout |
   |---|---|---|---|---|
   | 取镖试运行：摆到左侧镖座 | Arm | Arm | kFetchLeft | kFetchArmTimeout |
   | 取镖试运行：左侧下探 | Crank | Crank | kCrankFetchLeft | kServoTimeout |
   | 取镖试运行：左侧等磁铁吸稳 | Wait | Wait | kFetchGripTime | kFetchGripTime + 1 |
   | 取镖试运行：左侧抬起 | Crank | Crank | kCrankRetracted | kServoTimeout |
   | 取镖试运行：左侧举着停一下 | Wait | Wait | kFetchShowTime | kFetchShowTime + 1 |
   | 取镖试运行：左侧下探放回 | Crank | Crank | kCrankFetchLeft | kServoTimeout |
   | 取镖试运行：左侧磁铁通电放镖 | Magnet | Immediate | 1 | 默认 |
   | 取镖试运行：左侧等镖脱开 | Wait | Wait | kFetchReleaseTime | kFetchReleaseTime + 1 |
   | 取镖试运行：左侧推杆收起 | Crank | Crank | kCrankRetracted | kServoTimeout |
   | 取镖试运行：左侧磁铁断电 | Magnet | Immediate | 0 | 默认 |
4. `Enter()` 加 `case Action::Magnet: io_.SetMagnet(current_.target > 0.5); break;` 和 `case Action::Wait: break;`。`Magnet` 不改库存，也不设 `release_window_`。
5. `Done()` 加 `case Condition::Wait: return elapsed_ >= current_.target;`。
6. `FetchSteps()` 里的 `kCrankFetch` 换成 `side > 0 ? kCrankFetchRight : kCrankFetchLeft`，步骤和名字都不变。
7. `Tick()`：
   - `if (!state.all_homed)` 改成 `if (!state.all_homed && !fetch_test_)`，上面加注释 `// 取镖试运行只动摆臂、推杆和磁铁，摆臂建没建零点由适配层在开始前检查。`
   - `if (!powered) { paused_ = true; return; }` 改成：
     ```cpp
         if (!powered) {
             // 取镖试运行无力就停：摆臂断力会掉下去，重新上力以后不能在掉下来的位置接着下探。
             if (fetch_test_) {
                 Cancel();
                 return;
             }
             paused_ = true;
             return;
         }
     ```
   - 最后一步做完那段（`decocking_ = false; cocking_ = false;`）后面加 `fetch_test_ = false;`。
8. `Cancel()`、`Fail()` 里也把 `fetch_test_` 置 false。

### 5. `src\mode_logic.hpp` / `src\mode_logic.cpp` / `src\match_flow.cpp`
1. `ModeCommand` 里 `magnet_toggle` 后面加 `bool fetch_request = false; // 装弹组拨轮正向保持 2 s：自动取镖试运行（任务 3q）。`；结构体上面“回零、上膛、电磁铁切换、故障复位和发射命令均为单拍事件”的注释加上取镖。
2. `ModeLogic::Update()` 拨轮那段，`out.home_request = direction > 0;` 换成：
   ```cpp
                   // 正向在装弹组是自动取镖试运行（摆臂按编码器绝对角，不用回零），其他组是回零。
                   out.home_request = direction > 0 && out.group != ManualGroup::Loading;
                   out.fetch_request = direction > 0 && out.group == ManualGroup::Loading;
   ```
3. `match_flow.cpp` 里清请求那一句（`c.home_request = c.cock_request = c.fire = c.salvo = c.magnet_toggle = false;`）加上 `c.fetch_request`，比赛接管时不许开始。

### 6. `src\hw\launcher_hw.hpp` / `src\hw\launcher_hw.cpp`
1. `ArmConfig()` 末尾（`release_still_rpm` 后面）加 `config.move_creep = kArmMoveCreep;` 和 `config.move_tolerance = kArmMoveTolerance;`。
2. `Tick()`：电磁铁切换那段（`if (command_.magnet_toggle && !sequence_.Busy()) { ... }`）后面加 `if (command_.fetch_request && !sequence_.Busy()) { StartFetchTest(now); }`。
3. 新增私有函数，`hpp` 里声明并注释“装弹组拨轮往上 2 s：检查推杆标定、摆臂零点和温度，再开始自动取镖试运行”：
   ```cpp
   void LauncherHardware::StartFetchTest(double now) {
       if constexpr (!kArmInstalled || !kCrankInstalled || !kMagnetInstalled) {
           WarnNotInstalled(now);
       } else if constexpr (!kCrankServoCalibrated) {
           XR_LOG_WARN("推杆未标定，不能自动取镖");
       } else {
           const double temp = arm_.FeedbackTemp();
           if (!arm_.Axis().Homed()) {
               XR_LOG_WARN("摆臂没建零点，不能取镖");
           } else if (!(temp <= kFetchMaxTemp)) {
               XR_LOG_WARN("摆臂 %.0f°C 太热，先不取镖", temp);
           } else if (sequence_.CommandFetchTest()) {
               XR_LOG_INFO("自动取镖：先左后右");
           } else {
               XR_LOG_WARN("取镖试运行没被接受");
           }
       }
   }
   ```
4. `Manual()` 里推杆已标定那段换成：
   ```cpp
               if (command.crank_rate != 0) {
                   // 手动推杆和原始脉宽模式一样快：满杆每秒 kCrankPulseRate，最深到 kCrankRawPulseMax。
                   const double target = Clamp(crank_.Value() + command.crank_rate / kServoSpeed * kCrankManualSpeed * dt,
                                               kCrankRetracted, kCrankManualMax);
                   if (std::fabs(target - crank_.Value()) >= 1e-6) { SetCrank(target); }
               }
   ```
   未标定的原始脉宽分支原样留着（以后重新标定时还要用）。
5. `SetArm()` 换成：
   ```cpp
   void LauncherHardware::SetArm(double angle) {
       // 流程每开始一段摆臂移动，堵转保护重新计时：镖座保持电流常常正好在限流值上，
       // 触发以后从一侧镖座爬到另一侧会被限在 1.2 A 爬不上去。
       arm_stall_.Reset();
       arm_.Axis().MoveTo(angle, kArmAutoSpeed);
   }
   ```
   `ArmAt()` 在 `Homed()` 后面加 `!arm_.Axis().Moving() &&`。`hpp` 里这两个函数的注释改成：“流程摆臂移动：像推摇杆一样以 kArmAutoSpeed 走到目标 rad，到了按松手保持；堵转保护重新计时。”“查询摆臂已回零、MoveTo 走完、实际角在目标附近且几乎不动。”
6. `StartHome()` 不改（装弹组的拨轮往上以后不会再走到它）。

### 7. `test\host_test.cpp`
已有的检查只许改下面第 1、2 条列出的几条；别的已有检查失败就停下。

1. `TestPidAndParams()`：
   - `"crank servo is on the bench in raw pulse mode since 2026-10-28"` 那条改成 `Check(kCrankInstalled && kCrankServoCalibrated, "crank servo is calibrated from the 2026-11-12 pulses (task 3q)");`
   - `"crank servo stays in raw pulse mode until its angles are calibrated"` 那条换成：
     ```cpp
         Check(Near(ServoRamp::PulseUs(kCrankRetracted, kCrankServoOffset, kCrankServoDirection), kCrankPulseRetracted, 1e-6) &&
                   Near(ServoRamp::PulseUs(kCrankFetchLeft, kCrankServoOffset, kCrankServoDirection), kCrankPulseFetchLeft, 1e-6) &&
                   Near(ServoRamp::PulseUs(kCrankFetchRight, kCrankServoOffset, kCrankServoDirection), kCrankPulseFetchRight, 1e-6) &&
                   Near(ServoRamp::PulseUs(kCrankManualMax, kCrankServoOffset, kCrankServoDirection), kCrankRawPulseMax, 1e-6),
               "calibrated crank angles give the measured retract and fetch pulses");
         Check(Near(kCrankRetracted, 0) && kCrankFetchLeft > 0 && kCrankFetchLeft < kCrankManualMax &&
                   kCrankFetchRight > 0 && kCrankFetchRight < kCrankManualMax,
               "crank fetch angles sit between retracted and the manual limit");
         Check(Near(kCrankManualSpeed * (kServoPulseMax - kServoPulseMin) / kServoRange, kCrankPulseRate, 1e-9),
               "the manual crank moves as fast as the raw pulse mode did");
     ```
   - `"with the magnet enabled, the unplugged arm and uncalibrated crank keep the reload and fetch sequences off"` 那条改成 `Check(kMagnetInstalled && !kLoadCalibrated && !kReloadInstalled, "automatic reload stays off until the load pulse and rise height are measured");`
   - 函数末尾加：
     ```cpp
         Check(kArmMoveTolerance > 0 && kArmMoveTolerance < kArmHoldDeadband && kArmMoveCreep > 0 &&
                   kArmMoveCreep < kArmAutoSpeed && kArmAutoSpeed <= kArmSpeed && kFetchArmTimeout > kArmTimeout,
               "automatic arm moves are slower than a full stick and stop inside the hold deadband");
         Check(kFetchGripTime > 0 && kFetchShowTime > 0 && kFetchReleaseTime > 0 &&
                   kFetchReleaseTime + kServoTimeout < kMagnetOnLimit && kFetchMaxTemp <= kArmBoostFullTemp,
               "fetch test waits fit the magnet on limit and it starts only while the arm has its full hold current");
     ```
2. `TestWheelAndManualCancel()` 开头那个三组 × 两方向的循环：
   - 加一个计数 `fetches += command.fetch_request;`；`i < 199` 那条检查同时要求 `!command.fetch_request`。
   - `"wheel positive homes exactly once per group"` 那条改成 `Check(homes == (direction > 0 && group != Switch::Down ? 1 : 0), "wheel positive homes once in the aim and slider groups");`，后面加 `Check(fetches == (direction > 0 && group == Switch::Down ? 1 : 0), "wheel positive in the loading group starts the fetch test once");`
   - `"wheel neutral rearms trigger"` 那条里的 `retrigger.home_request == (direction > 0)` 改成 `retrigger.home_request == (direction > 0 && group != Switch::Down) && retrigger.fetch_request == (direction > 0 && group == Switch::Down)`，提示文字不变。
3. `FakeIo` 加三个记录 `std::vector<double> arm_log, crank_log;`、`std::vector<bool> magnet_log;`，分别在 `SetArm`、`SetCrank`、`SetMagnet` 里 `push_back` 参数。
4. `ArmAxisConfig()` 加 `config.move_creep = kArmMoveCreep;`、`config.move_tolerance = kArmMoveTolerance;`（和适配层一致）。`ArmRig` 加成员 `bool stick = true;`，为 false 时 `Tick` 不调用 `axis.Jog`（`tick` 照样加 1）。
5. `TestStallGuard()` 末尾加：一个 `StallGuard` 用超过限值的请求、转速 0 跑 2.1 s 触发，调 `Reset()` 以后 `Tripped()` 为假、下一拍 `Update` 原样返回请求。提示 `"reset re-arms the stall guard"`。
6. 新函数 `TestArmMoveTo()`：
   - 接口（`MotorAxis axis(ArmAxisConfig());`，`Feedback(0, 0)`）：
     - 没建零点时 `MoveTo(Radians(10), kArmAutoSpeed)` 返回 false：`"an arm without a zero refuses MoveTo"`
     - `HomeAt(0)` 以后 `MoveTo(Radians(10), kArmAutoSpeed)` 为真且 `Moving()`：`"MoveTo starts a move"`
     - `MoveTo(kNan, kArmAutoSpeed)`、`MoveTo(0, 0)`、`MoveTo(0, -1)` 都返回 false：`"MoveTo rejects invalid goals and speeds"`
     - 分别在 `Jog(0.1)`、`Stop()`、`Update(0.001, false, true)` 以后 `Moving()` 为假（每种之前重新 `MoveTo` 一次）：`"a stick jog, a stop or losing power ends the move"`
     - 离目标不到 `kArmMoveTolerance` 时 `MoveTo` 返回真、`Moving()` 为假：`"a goal already reached just holds"`
     - 一条 `SliderConfig()` 的轴用 `FinishHome()` 回零以后 `MoveTo(0.1, 0.05)` 返回 false：`"axes without jog_moves_target refuse MoveTo"`
   - 受力模型：惯量 {0.01, 0.03} × 三种负载 `ArmPlant{惯量, 0.95, 0.45, 0.35}`（11-12 追踪拟合）、`ArmPlant{惯量, 1.3, 0.45, 0.35}`（吸着镖、偏重）、`ArmPlant{惯量, 0.95, 0.33, 0.25}`（摩擦偏小）。每种建一个 `ArmRig`（`stick = false`），先 `Run(0.3, 0)`，再依次走 `kFetchLeft`、`kFetchRight`、`0` 三段：每段之前 `rig.guard.Reset()`（和适配层一样），调 `rig.axis.MoveTo(goal, kArmAutoSpeed)`，然后每拍 `Tick(0)` 直到 `!Moving()`，最多 `kFetchArmTimeout`。每段检查：
     - 在 `kFetchArmTimeout` 内走完：`"an automatic arm move arrives before the fetch timeout"`
     - 走完再跑 0.5 s，离终点不超过 0.5°：`"an automatic arm move stops within half a degree of its goal"`
     - 运动方向上越过终点不超过 1°，越过软限位不超过 0.5°，不碰硬挡（`plant.hit` 为假）：`"an automatic arm move barely overshoots and never reaches a stop"`
     - 过程中最大速度不超过 `2 * kArmAutoSpeed`（约等于满杆手动）：`"an automatic arm move is never faster than a full-stick swing"`
     - 走完以后再跑 3 s，位置变化不超过 0.5°：`"an arm parked by an automatic move stays put"`
   - 模型检查过不去时，可以在这个范围内调参数再试：`kArmAutoSpeed` 40~60°/s，`kArmMoveCreep` 2~6°/s，`kArmMoveTolerance` 0.2~0.4°（要小于 `kArmHoldDeadband`）。汇报写清改成了多少。还过不去就停下，汇报哪种负载、哪一段、数是多少。不许放宽上面的阈值。
7. 新函数 `TestFetchTest()`：`FakeIo` 加 `LaunchSequence`，每拍先 `io.Advance()` 再 `sequence.Tick(tick * 0.01)`，最多 20000 拍，和 `TestSequence()` 一样按 `Revision()` 变化记步骤名。`DartInventory` 默认构造是满装（磁铁上也记着有镖），这里先 `inventory.ResetManual()`。
   - 接受并跑完，没有故障：`"the fetch test runs left then right without a fault"`
   - 步骤名依次是下面 23 个：`"the fetch test steps run in the planned order"`
     ```
     取镖试运行：磁铁断电吸持
     取镖试运行：推杆收起
     取镖试运行：摆到左侧镖座
     取镖试运行：左侧下探
     取镖试运行：左侧等磁铁吸稳
     取镖试运行：左侧抬起
     取镖试运行：左侧举着停一下
     取镖试运行：左侧下探放回
     取镖试运行：左侧磁铁通电放镖
     取镖试运行：左侧等镖脱开
     取镖试运行：左侧推杆收起
     取镖试运行：左侧磁铁断电
     取镖试运行：摆到右侧镖座
     取镖试运行：右侧下探
     取镖试运行：右侧等磁铁吸稳
     取镖试运行：右侧抬起
     取镖试运行：右侧举着停一下
     取镖试运行：右侧下探放回
     取镖试运行：右侧磁铁通电放镖
     取镖试运行：右侧等镖脱开
     取镖试运行：右侧推杆收起
     取镖试运行：右侧磁铁断电
     取镖试运行：摆臂回 0°
     ```
   - `arm_log` 是 `{kFetchLeft, kFetchRight, 0}`：`"the fetch test visits the left seat, the right seat, then the middle"`
   - `crank_log` 是 `{0, kCrankFetchLeft, 0, kCrankFetchLeft, 0, kCrankFetchRight, 0, kCrankFetchRight, 0}`：`"the fetch test lowers each side to its own fetch angle twice"`
   - `magnet_log` 是 `{false, true, false, true, false}`，跑完 `io.magnet` 为假：`"the magnet holds while fetching and releases only to put the dart back"`
   - 跑完库存 `Remaining()` 不变、`!Fault()`、`!Held()`：`"the fetch test leaves the inventory unchanged"`
   - `io.state.all_homed = false` 时照样跑完、没有故障：`"the fetch test needs only the arm zero"`
   - 忙的时候、流程有故障以后（例如开始以后把 `io.state.motors_online` 设成 false，让它报电机离线）、默认满装库存时，`CommandFetchTest()` 都返回 false：`"the fetch test is refused while busy, after a fault or with a dart on the magnet"`
   - 跑到步骤名是 `取镖试运行：左侧抬起` 时 `Tick(t, false)`：流程不忙、没有故障、`io.stops` 加了 1；之后 `CommandFetchTest()` 又能接受：`"losing power stops the fetch test instead of pausing it"`
   - 跑到一半 `Cancel()`：不忙、没有故障：`"a stick cancel stops the fetch test"`
   - `io.blocked = true`（推杆一直不到位）时报 `SafetyFault::Timeout`：`"a stuck crank ends the fetch test with a timeout"`
8. 比赛接管：仿照 `"sticks wheel and switches are ignored during the match"` 那段（`MatchRemote`、`MatchTick`），左拨杆改成 `Switch::Down`、拨轮 1，300 拍里 `fetch_request` 一直为假：`"the fetch request is ignored during the match"`。
9. `main()` 里在 `TestArmPlantModel();` 后面调用 `TestArmMoveTo();` 和 `TestFetchTest();`。

### 8. `README.md`
加一节“自动取镖试运行（任务 3q，2026-11-13）”：手势、23 步、怎么停、相关参数（`kFetchLeft`、`kFetchRight`、`kCrankPulseFetchLeft`、`kCrankPulseFetchRight`、`kArmAutoSpeed`、`kFetchShowTime` 等）。另外写清楚：推杆按 1325 / 2335 / 2296 µs 标定、方向 +1；手动推杆的速度和范围没变；上电时推杆会自己收起到 1325 µs；推杆标定以后单发不再被“推杆没有收起”拦下（推杆没收起时，发射前会先自动收起）；自动换弹仍然关着（`kLoadCalibrated`）；流程每段摆臂移动按点动走、开始时堵转保护重新计时。原来写推杆未标定、`kCrankFetch`、装弹组拨轮往上回零的地方跟着改。

## 不要动
- 3p 调好的摆臂控制：`kArmJogLead`、`kArmHoldDeadband`、`kArmApproachGain`、重力和摩擦前馈、超速泄积分、`HoldPosition()`（只在开头加一行）、摆臂摇杆死区和曲线、堵转限流值和温度降额。
- 自动换弹的流程（`LoadSteps`、`FetchSteps` 的步骤、`ExpandLoad`、`CommandSalvo`），只把 `FetchSteps` 里的 `kCrankFetch` 换成分左右的值。
- 推杆未标定时的原始脉宽模式代码（`if constexpr` 分支）留着。

## 允许改的文件
- `xrobot-local\DartLauncher\`：`src\dart_params.hpp`、`src\motor_axis.hpp`、`src\motor_axis.cpp`、`src\stall_guard.hpp`、`src\stall_guard.cpp`、`src\launch_sequence.hpp`、`src\launch_sequence.cpp`、`src\mode_logic.hpp`、`src\mode_logic.cpp`、`src\match_flow.cpp`、`src\hw\launcher_hw.hpp`、`src\hw\launcher_hw.cpp`、`test\host_test.cpp`、`README.md`。
- `dart-fw\`：`xrobot setup` 改动的 `xrobot.lock` 和它生成的文件，`build\` 下的产物，备份 `build\DevC-2cc1764.elf`。

## 步骤
1. 确认分支和工作区：
   ```powershell
   git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher branch --show-current
   git -C C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher status --porcelain
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw branch --show-current
   git -C C:\Users\Lenovo\Desktop\xrobot\dart-fw status --porcelain
   ```
   应该分别是 `main`、空、`dart-launcher`、空。不对就停下写汇报。
2. 按“要改的地方”改 DartLauncher。
3. 主机测试，在 PowerShell 里：
   ```powershell
   $env:PATH = "C:\Users\Lenovo\AppData\Local\Programs\Webots\msys64\mingw64\bin;" + $env:PATH
   Set-Location C:\Users\Lenovo\Desktop\xrobot\xrobot-local\DartLauncher
   New-Item -ItemType Directory -Force build-host | Out-Null
   $src = (Get-ChildItem src\*.cpp).FullName
   g++ -std=c++17 -Wall -Wextra -Werror -O1 -Isrc test\host_test.cpp $src -o build-host\host_test.exe
   .\build-host\host_test.exe
   ```
   应该打印 `PASS: N checks`，N 比 21702 多。
4. 框架引用检查，下面这条应该没有任何输出：
   ```powershell
   Select-String -Path src\*.hpp, src\*.cpp -Pattern '#include\s*[<"](libxr|stm32|cmsis|FreeRTOS|main\.h)|#define'
   ```
5. 在 DartLauncher 的 `main` 上提交：先单独运行 `git add src test README.md`，再单独运行 `git commit -m "推杆按实测脉宽标定；装弹组拨轮往上 2 s 自动取镖试运行（先左后右，取起再放回）；流程摆臂移动按点动走"`。提交后用 `git rev-parse HEAD`、`git branch --show-current`、`git status --porcelain` 核对（沙箱有时会吞掉 git 的写入）。
6. 备份板上那版固件，更新模块并编译：
   ```powershell
   Set-Location C:\Users\Lenovo\Desktop\xrobot\dart-fw
   Copy-Item build\debug\DevC.elf build\DevC-2cc1764.elf
   xrobot setup --update local/DartLauncher
   git -C Modules\local\DartLauncher log --oneline -1
   cmake --build --preset debug
   Get-FileHash build\debug\DevC.elf -Algorithm SHA256
   ```
   - xrobot 提示安装版本 1.0.0 和固定版本 1.0.1 不同，这是已知的，退出码是 0 就继续，不要升级 xrobot。不用跑 `xrobot gen`（模块构造参数没变）。
   - `xrobot setup` 偶尔报 502 之类的临时网络错误，原样重跑一次；还不行就停下。不要手改 `xrobot.lock`。
   - 副本的 `log --oneline -1` 必须是第 5 步的提交；不是就停下，不要进副本 fetch 或 checkout。
   - 编译输出里模块文件（`launcher_hw.cpp` 等）不能有新警告；referee.h 的 packed 警告是老的。编译错误可以修，修了要重新跑第 3~5 步（新提交）再更新副本；修两轮还不行就停下。
7. 在 dart-fw 的 `dart-launcher` 上提交：先单独运行 `git add xrobot.lock`，再单独运行 `git commit -m "更新 DartLauncher：推杆标定，自动取镖试运行"`，同样核对。`git status --porcelain` 里还有别的文件，不要提交，写进汇报。不要烧录（沙箱里连不上调试器，烧录由用户做）。

## 卡住就停
遇到下面任何一种情况，就停下写汇报，不要自己想办法绕过去：
- 分支或工作区状态不对
- 主机测试没有 `PASS`，而失败的不是上面允许改的那几条已有检查；或者要删检查、放宽阈值才能通过
- 受力模型检查在允许的参数范围里调过还过不去
- 第 4 步有输出
- 联网重跑一次还失败，或者副本没有更新到第 5 步的提交
- git 提交后核对不上
- 需要改“允许改的文件”以外的东西，或者要改“不要动”里列的东西
- 编译错误修了两轮还没好

## 汇报
命令里的 `-o codex-report.md` 会用你最后一条回复覆盖这个文件，所以下面这些都写在最后一条回复里：
- 每个改动的文件一两句话说改了什么；`MoveTo()`、`StepMove()`、`CommandFetchTest()`、`StartFetchTest()`、`SetArm()`、`ArmAt()` 改完后的原文
- 受力模型参数调没调、调成了多少；`kFetchRight` 最后是 46.8° 还是退回了 46.2°
- 新增检查的提示文字和改过的已有检查；主机测试的 `PASS` 数；第 4 步的结果
- `Modules\local\DartLauncher` 的提交号；`DevC.elf` 的修改时间和 SHA-256；编译输出里的 FLASH、RAM 占用
- 两个仓库的提交号、当前分支和 `git status --porcelain` 的输出；中途停下就写停在哪一步，附原文报错

## 实车验证（用户做，Codex 不做）
烧录和上车按 Claude 给的步骤做，这里只记要点：
- 上电时推杆会自己收起到 1325 µs，手别放在推杆下面；锁止舵机照常在 1500 µs。
- 先两侧镖座都不放镖、磁铁上不吸镖，试一遍：看摆臂到两侧镖座的位置、推杆下探的深度、放回和回 0°。要停就碰一下摇杆。
- 再两侧放镖（磁铁上不能有镖）试一遍：每侧能不能吸起来、举着、放回原位。
- 位置不对就告诉 Claude 差多少（磁铁偏里还是偏外、推杆没碰到还是压太深），改 `kFetchLeft`、`kFetchRight`、`kCrankPulseFetchLeft`、`kCrankPulseFetchRight`。
- 3p 的追踪脚本只认 `2cc1764` 的 ELF，新固件上会拒绝运行；要追踪得先按新 ELF 更新地址。
