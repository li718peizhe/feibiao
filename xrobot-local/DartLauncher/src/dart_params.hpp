// 机构单位、传动、电流上限和控制初值；未完成实车标定的初值集中放在这里。
#pragma once
#include <cstdint>

namespace dart {
constexpr double kPi = 3.14159265358979323846;
// 角度 deg 转 rad。
constexpr double Radians(double degrees) { return degrees * kPi / 180.0; }
// 以输入单位将有限数值夹紧到闭区间。
constexpr double Clamp(double value, double low, double high) {
    return value < low ? low : value > high ? high : value;
}
// 力 N 和输出轴每弧度位移 m/rad 换算为电调原始电流值，向零取整。
constexpr int CurrentCap(double force, double lever, double kt, int command_max, double amps_max) {
    return static_cast<int>(force * lever / kt * command_max / amps_max);
}
constexpr double kM3508Ratio = 3591.0 / 187.0; // 原装约 19:1，与仿真一致。
constexpr double kM2006Ratio = 36.0;
constexpr double kEncoderRange = 8192.0;
constexpr double kBeltRadius = 40.0 * 0.003 / (2.0 * kPi);
constexpr double kYawLead = 0.004;
constexpr double kPitchLead = 0.002;
constexpr double kBeltForce = 220.0;
constexpr double kScrewForce = 1000.0;
constexpr double kPitchScrewForce = 6000.0; // N，2026-10-22 用户确认 BPGF-C7-12-2 实际额定负载 6000 N。
constexpr double kLoadMargin = 0.8;
constexpr double kSliderLoadMargin = 0.9; // 2026-10-16 用户定：0.8 时皮筋拉满正好用满上限，滑块单独放到皮带额定的 90%；以后装了拉力传感器再核对。
constexpr double kPitchLoadMargin = 1.0 / 3.0; // 2026-10-22：按 1000 N 时往上推不动；先用额定的三分之一，约 2000 N，回零时顶住上端也是这个力，待实车确认。
constexpr int kSliderCurrentCap = CurrentCap(kBeltForce * kSliderLoadMargin, kBeltRadius, 0.3, 16384, 20.0);
constexpr int kYawCurrentCap = CurrentCap(kScrewForce * kLoadMargin, kYawLead / (2.0 * kPi), 0.18, 10000, 10.0);
constexpr int kPitchCurrentCap = CurrentCap(kPitchScrewForce * kPitchLoadMargin, kPitchLead / (2.0 * kPi), 0.3, 16384, 20.0);
// 2026-11-12：1.0 A 推到 +19.3° / −13.4° 停住，上限加到 2.0 A（约 1.48 N·m）。
// 6020 额定 1.62 A、持续堵转 0.90 A；不转时由堵转保护降到 ArmStallLimit（凉时 1.2 A，热时 0.85 A）。
constexpr int kArmCurrentCap = static_cast<int>(2.0 / 3.0 * 16384);
// 电流超过限值（ArmStallLimit，电机热时就是 kArmStallCurrent）且转速不超过 kArmStallRpm 持续 kArmStallTime 时限流，
// 直到控制器请求的电流连续 kArmStallTime 秒低于限值。
constexpr int kArmStallCurrent = static_cast<int>(0.85 / 3.0 * 16384); // 约 0.63 N·m，低于持续堵转 0.86 N·m。
constexpr double kArmStallRpm = 2; // rpm，6020 输出轴。
constexpr double kArmStallTime = 2.0; // s。
// 2026-11-12 20:42 实车：右镖座附近约 48° 要约 1.06 A（cmd≈5800）才托得住，0.85 A 只能托在 46.8°~47.4°，差一点到镖座。
// 电机凉时堵转限流放到 kArmBoostCurrent；6020 温度从 kArmBoostFullTemp 到 kArmBoostEndTemp 线性降回
// kArmStallCurrent（持续堵转 0.90 A 以下），更热时一直用它。手册：绕组最高允许 125 °C，超过电机自己报过温。
constexpr int kArmBoostCurrent = static_cast<int>(1.2 / 3.0 * 16384); // 约 0.89 N·m。
constexpr double kArmBoostFullTemp = 50; // °C，6020 回报温度。
constexpr double kArmBoostEndTemp = 65; // °C。
// 按 6020 回报温度换算本拍堵转限流值；温度无效（NaN 比较全为假）和过热一样按 kArmStallCurrent。
constexpr int ArmStallLimit(double temp) {
    if (!(temp > -40 && temp < kArmBoostEndTemp)) { return kArmStallCurrent; }
    if (temp <= kArmBoostFullTemp) { return kArmBoostCurrent; }
    return static_cast<int>(kArmBoostCurrent - (kArmBoostCurrent - kArmStallCurrent) *
                            (temp - kArmBoostFullTemp) / (kArmBoostEndTemp - kArmBoostFullTemp));
}
// 摆臂点动推着位置目标走：目标最多领先实际位置 kArmJogLead，被挡住或静摩擦卡住时不再往前，松脱后最多追这么远；
// 保持时误差在 kArmHoldDeadband 以内不再积分，免得在静摩擦里来回找位置。
// 速度环 kp 150 对装车摆臂偏软，刹车主要靠积分回落：接近软限位按 kArmApproachGain 提前减速（比其他轴的 5/s 缓），
// 实际转速比速度目标快 kArmOverspeedRpm 以上时把往前推的积分在 kArmOverspeedBleedTime 内泄掉。
// 2026-11-12 按惯量 0.01~0.06 kg·m²、重力 0~1 N·m、库仑摩擦 0~0.55 N·m 的构造性仿真取值，都待实车标定。
constexpr double kArmJogLead = Radians(2);
constexpr double kArmHoldDeadband = Radians(0.5);
constexpr double kArmApproachGain = 3.5; // 1/s
constexpr double kArmOverspeedRpm = 2;
constexpr double kArmOverspeedBleedTime = 0.02; // s
constexpr double kArmOverspeedFilterTime = 0.05; // s，滤除低速瞬时转速跳变对摩擦补偿和积分泄放的触发。
// 2026-11-12 22:34 实车 50 Hz SWD 追踪拟合（plans/tasks/diagnostics/arm3p-20261112-223438.csv）：
// 重力 90° 时约 0.94~0.97 N·m、自然下垂在 0°~5.5°，动摩擦约 0.35 N·m、静摩擦至少约 0.45 N·m，惯量约 0.013 kg·m²。
// 摩擦很大：在镖座小推量往外微调只有 0.2~2.5°/s，要反复推摇杆。重力前馈宁少勿多，补约 0.8 N·m；
// 推摇杆时顺着点动方向再补约 0.3 N·m（低于动摩擦），目标离软限位最后 1° 里、或平滑转速已比速度目标快时减小。
// 几乎不动时松手（不超过 kArmStallRpm）电流保持连续，只把重力前馈加积分限到 kArmBoostCurrent：22:57 追踪里
// 松手时把积分压到 ±0.25 N·m，摆臂的弹性让每次往外点完弹回约 0.5°；往反方向点动时把这部分扣回。都待实车标定。
// 机械给摆臂加了配重或弹簧以后，重力前馈会变成把摆臂往外推，必须跟着改小 kArmGravityCurrent 或改成 0；
// 越过软限位 kArmGravityFade 以后前馈减到 0，只是兜底。
constexpr int kArmGravityCurrent = static_cast<int>(1.08 / 3.0 * 16384); // 约 0.8 N·m。
constexpr double kArmGravityZero = 0; // rad。
constexpr int kArmJogFrictionCurrent = static_cast<int>(0.405 / 3.0 * 16384); // 约 0.3 N·m。
constexpr double kArmJogFrictionFade = Radians(1);
constexpr double kArmGravityFade = Radians(2);
// PID 初值待标定；积分每拍累加，位置误差为转子计数，速度误差为 rpm。
struct PidGains { double kp, ki, kd, max_out, max_iout; };
constexpr PidGains kSliderPos{0.08, 0, 0, 6000, 0};
constexpr PidGains kSliderSpeed{15, 0.5, 0, kSliderCurrentCap, kSliderCurrentCap};
constexpr PidGains kYawPos{0.2, 0, 0, 15000, 0};
constexpr PidGains kYawSpeed{3, 0.05, 0, kYawCurrentCap, kYawCurrentCap};
constexpr PidGains kPitchPos{0.2, 0, 0, 15000, 0};
constexpr PidGains kPitchSpeed{15, 0.5, 0, kPitchCurrentCap, kPitchCurrentCap};
constexpr PidGains kArmPos{0.05, 0, 0, 60, 0};
// 30 是 2026-10-28 对空电机调的；装上摆臂后惯量大，速度环过软可能刹不住、冲过软限位到碰撞点约 4° 的余量，
// 积分作用压过比例作用时慢转也会一顿一顿。按摆臂惯量大于转子 10 倍估算，300 的环路增益接近空电机的 30；
// 2026-11-12 实车用 300 时有嗡鸣和抖动，按 3m 任务的调参表降到 150。
constexpr PidGains kArmSpeedGains{150, 0.5, 0, kArmCurrentCap, kArmCurrentCap};
// 行程使用实车回填，坐标向上为正；端点和流程位置待标定。
constexpr double kSliderTravel = 0.770; // m，2026-10-16 机械给的最大行程 790 mm；之后顶上加了 35 mm 防撞块，回零点按实测低约 20 mm，按 770 mm 算。
constexpr double kSliderTop = 0.370;
constexpr double kSliderBottom = kSliderTop - kSliderTravel;
constexpr double kSliderPark = 0.366; // m，2026-10-19 用户要求停到最顶端：回零点（防撞块）下面 4 mm，不贴着防撞块，免得电机一直顶着发热。
constexpr double kFireSliderMin = kSliderPark - 0.005; // m，发射时滑块不低于 361 mm，往上一直到最顶端都算到顶。
// 2026-10-16 实测：滑块在最顶端时，压面到停在顶部的发射机构被压面 130 mm；被压面到夹爪 630 mm；
// 夹爪卡发射机构底部，底部比被压面低 76 mm。
constexpr double kTopGap = 0.110; // m，10-16 实测 130 mm；装防撞块后回零点低约 20 mm，按 110 mm 算。
constexpr double kShuttleTravel = 0.630 - 0.076; // m，发射机构从放松到底部刚好到夹爪要走的距离。
constexpr double kPushGap = kTopGap + kShuttleTravel - kSliderTop;
constexpr double kLatchSlider = -kPushGap; // m，夹爪刚好卡住发射机构底部时的滑块位置，按上面的实测几何算，约 −0.294。
constexpr double kCockBottom = -0.2990; // 名义值：只用来算找底下限、退膛超时和测试。
constexpr double kCockLift = 0.0015; // m，找到底后往上留的距离，和 2q 一样；待标定。
constexpr double kCockPosition = kCockBottom + kCockLift; // 名义值：只用来算找底下限、退膛超时和测试；实际锁止位置每次由找底决定。
constexpr double kPressSearchLimit = kCockBottom - 0.040; // m，找底最低压到名义硬底下面 40 mm；待标定。
constexpr double kLoadPosition = -0.19720; // 2026-11-14 实测：左 −197.52 mm、右 −196.88 mm，近 0° 放镖落座且收杆不带起。
constexpr double kPitchTop = 0.100;
constexpr double kPitchTravel = 0.180;
constexpr double kPitchEndMargin = 0.005; // m，上下端软限位都离机械端留 5 mm，实测行程以后再改，待标定。
constexpr double kPitchBottom = kPitchTop - kPitchTravel + kPitchEndMargin;
constexpr double kPitchTopLimit = kPitchTop - kPitchEndMargin; // m，上端软限位：离上端挡块留 5 mm，平常点动不碰挡块，只有回零去顶。
constexpr double kYawTravel = 0.310;
constexpr double kYawRadius = 0.6045; // 待确认（机械第 9 条），旧 CAD 销轴几何。
constexpr double kYawMax = Radians(12);
// 2026-11-11 装车后无力手转实测（站在发射架后面、面朝发射方向；往右 raw 变大，和右为正一致，reverse 不用改）：
// 装填位 raw 1942；左镖座 1021（-40.5°），往左快碰到 820（-49.3°）；右镖座 2993（+46.2°），往右快碰到 3183（+54.5°）。
constexpr double kArmMin = Radians(-45); // 左镖座和左边碰撞点的中间。
constexpr double kArmMax = Radians(50); // 右镖座和右边碰撞点的中间。
constexpr std::uint16_t kArmZeroCounts = 1942; // 0~8191，摆臂在装填位（放镖角，0 rad）时的编码器原始值。
constexpr double kFetchRight = Radians(46.35); // 11-14 自动取镖仍高约 0.03°，由 46.38° 下调。
constexpr double kFetchLeft = Radians(-41.65); // 11-13 01:42 用户手动对准的位置（raw 994）；从约 −40.3° 起有东西往回顶，要压进去约 1.35°。自动停在 −40.5° 时磁铁对偏，10 次只吸上 2 次。
constexpr double kStandby = Radians(36.16); // 2026-11-14 持镖收杆实测：左 −36.16°、右 36.04° 均有用户确认的安全间隙，取绝对值较大者。
constexpr double kCrankRetracted = 0; // 推杆机构角 0 是收起，对应 kCrankPulseRetracted。
constexpr double kLockClosed = 0; // rad，锁死的机构角，对应 kLockPulseClosed。
constexpr double kLockOpen = Radians(45); // rad，放开的机构角，对应 kLockPulseOpen。
constexpr double kServoSpeed = 7; // rad/s，待标定；推杆 TD-8130MG 7.2 V 空载 0.17 s/60°，约 6 rad/s，带载更慢，推杆到位另等 kCrankSettleTime。
constexpr double kArmSpeed = 2; // rad/s，2026-10-28 桌上测：1 rad/s（不到 10 rpm）太慢，空电机齿槽让它一顿一顿；PID 调好再定，待标定（机械第 17 条）。
constexpr double kServoRange = Radians(270); // rad，锁止 PM60S 和推杆 TD-8130MG 都是 270°。
constexpr double kServoPulseMin = 500; // µs，推杆 TD-8130MG 为 500~2500 µs，中位 1500 µs（机械第 17 条）。
constexpr double kServoPulseMax = 2500; // µs，推杆 TD-8130MG 为 500~2500 µs，中位 1500 µs（机械第 17 条）。
constexpr double kLockPulseClosed = 1500; // µs，2026-10-16 实测完全锁死，停 30 s 不发热。
constexpr double kLockPulseOpen = 2000; // µs，2026-10-16 实测放开，发射机构能顺畅通过。
// 锁止舵机按两个实测脉宽反推方向和偏置，换成 180° 或 270° 舵机都一样。
constexpr double kLockServoDirection = (kLockPulseOpen - kLockPulseClosed) /
    (kServoPulseMax - kServoPulseMin) * kServoRange / (kLockOpen - kLockClosed);
constexpr double kLockServoOffset = (kLockPulseClosed - kServoPulseMin) /
    (kServoPulseMax - kServoPulseMin) * kServoRange - kLockServoDirection * kLockClosed;
constexpr double kServoFrequency = 50; // Hz，锁止和推杆在 TIM8，电磁铁在 TIM1，上电时 CubeMX 都已配成 50 Hz，待确认（机械第 19 条）。
constexpr double kLoadRiseSpeed = 0.05; // m/s，待标定。
constexpr double kHomeSpeed = 0.01; // m/s，yaw 用，待标定；pitch 用 kPitchHomeSpeed，转轴需显式提供 rad/s。
constexpr double kSliderHomeSpeed = 0.03; // m/s，只给滑块用，待标定；pitch 导程只有 2 mm，不能跟着提速。
constexpr double kHomeCurrentFraction = 0.3; // 待标定。
constexpr double kPitchTopForce = 1000.0; // N，2026-10-22 用户：设计上顶到上端的力不能超过 1000 N，不然联轴器打滑。
constexpr double kPitchHomeCurrentFraction = kPitchTopForce / (kPitchScrewForce * kPitchLoadMargin); // 回零和没回零的点动会顶到上端挡块，按 1000 N 限流，约 869；回零以后的点动不碰挡块，用满上限。
constexpr double kPitchHomeStallTime = 1.0; // s，2026-10-21：pitch 满电流下要过一会儿才转起来，0.2 s 会在原地误判成到头，待标定。
constexpr double kPitchHomeSpeed = 0.005; // m/s，2026-10-22：10 mm/s 撞上端挡块时联轴器打滑过（已修），pitch 回零和没回零的点动放慢一半，待标定。
constexpr double kPitchHomeSeekLimit = kPitchTravel + 0.010; // m，回零走过整段行程再多 10 mm 还没堵转，就当联轴器打滑或没碰到挡块，锁存故障。
constexpr double kHomeStallRpm = 1; // 转子 rpm，待标定。
constexpr double kHomeStallTime = 0.2; // s，待标定。
constexpr double kHomeRetreat = 0.003; // m，待标定。
constexpr double kHomeTimeout = 90; // s，待标定。
constexpr double kSliderSyncTolerance = 0.005; // m，待标定。
constexpr double kSoftLimitApproachGain = 5; // 1/s，回零以后点动接近软限位时，允许的速度 = 离限位的距离 × 这个数，最后一小段慢慢停到限位上；待标定。
constexpr double kLatchCheckRise = 0.020; // m，要大于下压深度 8 mm，滑块才能离开发射机构，待标定。
constexpr double kLatchCheckSpeed = 0.02; // m/s，待标定。
constexpr double kLatchCheckCurrent = kSliderCurrentCap * 0.25; // 电调原始值，待标定。
constexpr double kBottomCurrent = kSliderCurrentCap * 0.95; // 压不动的电流门槛；待标定。
constexpr double kBottomStill = 0.0005; // m，压不动窗口内允许的位置变化；待标定。
constexpr double kBottomTime = 0.2; // s，连续压不动的判断时间；待标定。
constexpr double kMinPushTravel = 0.002; // m，压不动之前至少推着发射机构走的距离；待标定。
constexpr double kBottomHoldTolerance = 0.002; // m，判定滑块正压在底上的范围；待标定。
constexpr double kLatchCheckHold = 0.2; // s，到位后连续低电流的保持时间，待标定。
constexpr double kProbeSpeed = 0.05; // m/s，未知状态向下探测的限速，待标定。
constexpr double kPressSpeed = 0.05; // m/s，从探测位往下压到上膛位的限速，免得撞底，待标定。
constexpr double kProbeMargin = 0.030; // m，探测终点高于锁止位的余量，待标定。
constexpr double kProbePosition = kLatchSlider + kProbeMargin;
constexpr double kManualSliderFloor = kProbePosition; // m，手动往下点动的最低位置；待标定。
constexpr double kSliderTolerance = 0.003; // m，待标定。
constexpr double kSliderVelocityTolerance = 0.05; // m/s，待标定。
constexpr double kYawTolerance = Radians(0.05); // rad，待标定。
constexpr double kYawVelocityTolerance = 0.01; // rad/s，待标定。
constexpr double kPitchTolerance = 0.0005; // m，待标定。
constexpr double kPitchVelocityTolerance = 0.002; // m/s，待标定。
constexpr double kServoTolerance = Radians(1.5); // rad，待标定。
constexpr double kMagnetWait = 0.15; // s，待标定。
// 继电器模块跳线设在 L（低电平触发）：IN 低 → 继电器吸合 → COM–NO 接通 → 磁铁通电消磁，断电电平是高。
// PE13 在 CubeMX 里配了内部上拉，固件接管引脚以前（上电初始化、烧录、没开磁铁的旧固件）是高电平；
// 2026-11-11 实测这时跳线在 L 继电器不吸合、在 H 会吸合，所以用 L。
constexpr bool kMagnetRelayActiveHigh = false;
// s，一次通电消磁最长时间，到时自动断电吸持。CNMYE1-P25/29K 只写“短时通电释放”，没给允许时长；
// 先取 3 s，够放镖后推杆收起（kMagnetWait + kServoTimeout），台架上摸温升后再调。
constexpr double kMagnetOnLimit = 3.0;
constexpr double kFlightWait = 0.5; // s，无出膛传感器，待标定。
constexpr double kServoTimeout = 2; // s，待标定。
constexpr double kLockSettleTime = 0.6; // s，锁止舵机转 67.5° 的时间加余量；流程只看斜坡，这里等舵机真的转到位，待实测。
constexpr double kCrankSettleTime = 0.6; // s，TD-8130MG 比 kServoSpeed 估的慢，斜坡走完推杆还没到；推杆步骤至少等这么久再看到位，待实测。
constexpr double kLoadCrankSettleTime = 0.6; // s，装填下放步骤完成后再停留，给带载舵机推到底后才通电放镖，待实测。
constexpr double kArmTimeout = 3; // s，待标定。
// 任务 3q：流程里的摆臂移动和自动取镖试运行，都待实车调。
constexpr double kArmAutoSpeed = Radians(60); // rad/s，流程摆臂巡航速度；11-12 23:43 追踪手动大幅摆动平均 40~66°/s。
constexpr double kArmMoveCreep = Radians(4); // rad/s，离终点很近时的最低速度；摩擦大，2°/s 时主机受力模型卡在终点前 0.44°。
constexpr double kArmMoveTolerance = Radians(0.3); // rad，离终点这么近（或已越过终点）就和松手一样保持。
// 离终点不到 kArmMoveNear 时，kArmMoveSettleTime 内挪动不到 kArmMoveStillBand 就算卡住：先像把摇杆推大一样按 kArmMovePush 给速度目标，
// 电流很快涨上去，冲开静摩擦、把左镖座压进去（目标仍不越过终点）；再推 kArmMovePushTime 还不动就按当时位置保持，算走到。
// 一直顶着不行：顶久了堵转保护会把电流限在 1.2 A，摆臂在镖座往外爬要 1.3~1.5 A，只能等超时报故障。
// 11-13 主机弹性镖座模型：卡住 0.3 s 就停时停在 −40.77°，加上加大推力以后压到 −41.28°~−41.47°。
constexpr double kArmMoveNear = Radians(1); // rad，小于流程到位判断的 kServoTolerance。
constexpr double kArmMoveSettleTime = 0.3; // s，比正常爬完最后 1°（约 0.25 s）稍长。
// 这段时间里挪动不到它才算卡住。不能看转速读数：压进左镖座时只有 1~3°/s，读数也是 0，会还没压到就停。
constexpr double kArmMoveStillBand = Radians(0.1); // rad，约 2 个编码器计数。
constexpr double kArmMovePush = Radians(20); // rad/s，大约是摇杆推到 0.45 的速度。
constexpr double kArmMovePushTime = 1.0; // s，和上面的 0.3 s 加起来要短于堵转保护的 2 s。
constexpr double kFetchArmTimeout = 6; // s，从一侧镖座摆到另一侧约 2.5 s。
constexpr double kFetchGripTime = 0.5; // s，下探到位以后等磁铁吸稳。
// 推杆吸住镖以后往上抬的速度：11-13 用户要求伸到底以后慢一点上去。约 990 µs/s，收起 1000 µs 左右要约 1 s；
// 平常的推杆斜坡是 kServoSpeed，约 2970 µs/s。
constexpr double kCrankLiftSpeed = kServoSpeed / 3; // rad/s
constexpr double kFetchMaxTemp = kArmBoostFullTemp; // °C，6020 比它热就不开始：右镖座要约 1.06 A，热了堵转限流会降。
constexpr double kPressTimeout = 8; // s，待标定。
constexpr double kReturnTimeout = 6; // s，待标定。
constexpr double kDecockTimeout = (kSliderPark - kCockPosition) / kLoadRiseSpeed + kReturnTimeout; // s，退膛时从上膛位慢慢升回停靠位。
constexpr double kProbeTimeout = (kSliderTop - kProbePosition) / kProbeSpeed + kReturnTimeout;
constexpr double kAimTimeout = 20; // s，待标定。
constexpr double kMotorOfflineTime = 0.1;
constexpr double kRemoteOfflineTime = 0.1;
constexpr double kRefereeOfflineTime = 1;
constexpr double kHomeWheelTime = 2;
constexpr double kFaultResetTime = 1.0; // s，无力连续保持后切到手动复位故障，待标定。
// 比赛流程（阶段 3c，接裁判系统）。时间单位 s；比赛阶段剩余时间来自 0x0001，闸门和发射指令来自 0x020A。
constexpr double kMatchDecockLead = 20; // 比赛中剩余时间不超过它，还上着膛又没开闸门，就开始退膛。退膛实测约 17 s，留 3 s 余量，保证进结算前退完；待实车核对。
constexpr double kMatchCockCutoff = 40; // 比赛中剩余时间少于它不再开始上膛：上膛约 20 s，上完马上就要退膛了。
constexpr double kRefereeStageStale = 5; // 0x0001（每秒 1 帧）超过这么久没更新，比赛阶段当作未知。
constexpr double kRefereeGateStale = 2; // 0x020A（每秒 3 帧）超过这么久没更新，闸门当作没开。
constexpr double kMatchRequestInterval = 2; // 回零、上膛、退膛、发射的自动请求被拒绝以后，隔这么久再发一次。
constexpr int kMatchAutoRetries = 2; // 遥控器不在线时，一场比赛里最多自动复位几次可恢复的故障。
constexpr double kMatchRetryDelay = 2; // 故障以后等这么久再自动复位，让机构停稳。
// 比赛开始时还有轴没回零就自动回零（2026-10-18 用户定的兜底）。pitch 回零靠堵转，起点离上端太远会误判零点，所以准备阶段应该先用遥控器回零；不想要这个兜底就改成 false。
constexpr bool kMatchAutoHome = true;
constexpr double kStickDeadband = 0.05; // 归一化摇杆死区，待标定。
// 摆臂（装弹组左摇杆横向）单独用更大的死区：DR16 摇杆松开回中时会冲过中位、在 ±0.05~0.14 抖，
// 2026-11-12 23:15 追踪里每次往外点完常跟一下反方向的小点动（90 s 内 20 次），把松手时压住摆臂的电流扣掉，摆臂弹回更多。
// 死区外按 1.5 次方给速度：0.3 杆量约 8.5°/s、0.5 约 30°/s，满杆仍是 kArmSpeed。
constexpr double kArmStickDeadband = 0.15;
constexpr double kArmStickExponent = 1.5;
constexpr double kSliderMoveSpeed = 0.3; // m/s，流程默认移动限速，待标定。
constexpr double kManualSliderSpeed = 0.1; // m/s，待标定。
constexpr double kManualPitchSpeed = 0.01; // m/s，待标定。
constexpr double kManualYawSpeed = Radians(2); // rad/s，待标定。
constexpr bool kAutoEnabled = true; // 2026-10-16 打开：自动模式下左拨杆从中拨到上就单发；没接裁判系统也能用。
constexpr bool kCockEnabled = true; // 2026-10-16 打开：锁止舵机已单独供电，锁止位置按实测几何算。
constexpr bool kSliderInstalled = true; // 两台滑块 M3508，装好后改 true。
constexpr bool kPitchInstalled = true; // 2026-10-20 接上：CAN2，电调 ID 3（反馈 0x203）；方向和行程待实车确认。
constexpr bool kYawInstalled = false; // yaw M2006，装好后改 true。
// 2026-10-28 桌上测过 GM6020（CAN2，ID 1，反馈 0x205，已开电流环），零点、范围和 PID 待标定。
// 当晚先拆下单独测推杆。2026-11-11 摆臂装上车、6020 接好后打开；零点、方向、范围已按手转实测填好（见 kArmMin 上方），
// PID 和松手保持待装车试转。6020 不接时这里必须是 false，不然报电机离线，手动和流程都不动。
constexpr bool kArmInstalled = false; // 2026-11-24 换弹机构临时拆除，关闭摆臂。
// 推杆舵机 TD-8130MG（270°，500~2500 µs）。2026-10-28 桌上测：舵机单独接 PWM 排针第 7 列（C7 信号、B7 5 V、A7 地，TIM8_CH3），
// 第 2 列（TIM1_CH2）信号正常但舵机不动，用户改到第 7 列；没装机构；
// 未标定时推杆角报无效值，发射会被“推杆没有收起”拦下，要打镖又没标定推杆时改回 false。
constexpr bool kCrankInstalled = false; // 临时关闭推杆，保留任务 3q 标定脉宽。
// 2026-11-11 打开：CNMYE1-P25/29K 断电吸持、通电消磁，经 SRD-03VDC-SL-C 继电器（IN 接 PWM 第 3 列 C3，COM 接 NO）。
// 通电最长 kMagnetOnLimit，无力或故障立即断电吸持；按住复位键（引脚浮空）时继电器是否保持断开待台架确认。
constexpr bool kMagnetInstalled = true;
constexpr bool kCrankServoCalibrated = true; // 2026-11-13 任务 3q 按实测脉宽标定（见 kCrankPulseRetracted）；重装舵盘以后改回 false 重新量。
// 三项装填参数已按手动实测回填；2026-11-14 用户确认独立自动装填现场初步通过，打开自动换弹。
constexpr bool kLoadCalibrated = true;
// 换弹机构三样都装好、推杆和装填位置都标定过才自动换弹；否则镖由操作手手动放，只能单发。
constexpr bool kReloadInstalled = kArmInstalled && kCrankInstalled && kMagnetInstalled && kCrankServoCalibrated && kLoadCalibrated;
constexpr bool kLockServoCalibrated = true; // 2026-10-16 已按 kLockPulseClosed、kLockPulseOpen 标定；重装舵盘后改回 false 重新量。
constexpr double kLockRawPulseMin = 1469; // µs，2026-10-15 舵盘重装后实测的最大锁止，未标定时原始脉宽不低于它。
constexpr double kLockRawPulseMax = 2166; // µs，2026-10-15 实测的最大张开，脉宽变大是张开，未标定时原始脉宽不高于它。
constexpr double kLockBootPulseUs = (kLockRawPulseMin + kLockRawPulseMax) / 2; // µs，首次唤醒转到实测行程中间；PM60S 顶住限位会卡死不响应。
constexpr double kLockPulseRate = 200; // µs/s，未标定时手动调整锁止脉宽的速率，待标定。
constexpr double kCrankRawPulseMin = 1300; // µs，中位往下 200 µs（约 27°）；机构的安全行程量出来之前，未标定时原始脉宽不低于它。
constexpr double kCrankRawPulseMax = 2500; // µs，2026-11-12：1700 µs 实车只下探两三厘米，按用户要求放到舵机额定脉宽上端；到机构所需位置即停。
constexpr double kCrankBootPulseUs = 1500; // µs，首次唤醒转到 TD-8130MG 中位；按 kCrankRetracted 输出是 500 µs，会顶到行程一端。
constexpr double kCrankPulseRate = 200; // µs/s，未标定时手动调整推杆脉宽的速率，待标定。
// 任务 3q：推杆按 2026-11-12 实车脉宽标定。脉宽变大往下探（方向 +1），机构角 0 是收起；
// 机构角和脉宽按舵机 270° 对应 2000 µs 换算。取不到镖或压得太深就改两个镖位脉宽。
constexpr double kCrankPulseRetracted = 1325; // µs，15:26 用户确认收起。
constexpr double kCrankPulseFetchLeft = 2288; // µs，11-13 01:42 用户手动压到能吸住的深度。
constexpr double kCrankPulseFetchRight = 2395; // µs，11-13 01:46 用户手动压到能吸住的深度；2296 时 10 次一次没吸上。
constexpr double kCrankServoDirection = 1;
constexpr double kCrankServoOffset = (kCrankPulseRetracted - kServoPulseMin) /
    (kServoPulseMax - kServoPulseMin) * kServoRange; // rad，收起时的舵机角。
// 推杆脉宽 µs 换成机构角 rad，收起为 0。
constexpr double CrankAngleFromPulse(double pulse) {
    return (pulse - kCrankPulseRetracted) / (kServoPulseMax - kServoPulseMin) * kServoRange;
}
constexpr double kCrankSeat = CrankAngleFromPulse(2150); // 2026-11-14 独立装填实测需 2150 µs 才够，释放落座和收杆待验。
constexpr double kCrankFetchLeft = CrankAngleFromPulse(kCrankPulseFetchLeft);
constexpr double kCrankFetchRight = CrankAngleFromPulse(kCrankPulseFetchRight);
// 手动推杆的范围和速度和原始脉宽模式一样：最深到 kCrankRawPulseMax，满杆每秒 kCrankPulseRate。
constexpr double kCrankManualMax = CrankAngleFromPulse(kCrankRawPulseMax);
constexpr double kCrankManualSpeed = kCrankPulseRate / (kServoPulseMax - kServoPulseMin) * kServoRange; // rad/s
// DBUS 分帧：一帧 18 字节，100000 波特按 9 位加起止位传输约 2 ms；DR16 每 14 ms
// 发一帧，帧间空闲约 12 ms。DMA 半满/全满中断会把一帧切成两段，两段之间不超过
// 2 ms，所以空闲 3 ms 以上才算一帧结束。接收队列 128 字节，按每 14 ms 一帧约
// 85 ms 溢出，超过 20 ms 没轮询就当作可能丢过字节。
constexpr std::size_t kDbusFrameSize = 18;
constexpr std::uint32_t kDbusQuietMs = 3;
constexpr std::uint32_t kDbusStallMs = 20;
}
