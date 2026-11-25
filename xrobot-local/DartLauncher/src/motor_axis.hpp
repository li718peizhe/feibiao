// 单电机轴：位置换算、软限位、限速、到位和低速堵转回零。
#pragma once
#include "dart_params.hpp"
#include "pid.hpp"
#include <limits>

namespace dart {
enum class HomeState { Idle, Seeking, Retreating, Homed, Failed };
// 位置为 m 或 rad；counts_per_unit 的符号定义电机镜像方向。
struct AxisConfig {
    double counts_per_unit, minimum, maximum, home_position;
    double home_speed, retreat, home_tolerance;
    int home_direction, current_limit, command_max;
    PidGains position_gains, speed_gains;
    double home_current_fraction = kHomeCurrentFraction; // 没回零时点动和回零的限流比例。
    double home_stall_time = kHomeStallTime; // 回零时满限流且不转持续多久才算顶到头，单位 s。
    double home_seek_limit = std::numeric_limits<double>::infinity(); // 回零最多走多远（m 或 rad），超过还没堵转就锁存故障；默认不限。
    bool hold_on_release = false; // 回零以后点动松手（速度为 0）时，按松手的位置保持并算到位；默认照旧只把速度目标设成 0。
    int hold_integral_limit = 0; // 松手保持时重力前馈加速度环积分最多留多少，电流原始值；0 表示清零。
    // 转子 rpm，jog_moves_target 的轴向外支撑时松手，转速不超过它才保持电流连续；0 表示不保持。
    double release_still_rpm = 0;
    // 回零以后点动推着位置目标走，给靠电流托住重力的轴用：点动、松手、到软限位都不清速度环积分；
    // 目标和实际位置离软限位越近，往外允许的速度越小，到限位正好为 0。
    bool jog_moves_target = false;
    double jog_lead = 0; // m 或 rad，点动目标最多领先反馈位置多远；被挡住时目标不再往前走，0 表示不限。
    double hold_deadband = 0; // m 或 rad，目标不动且误差在此以内时按已到位处理，积分不再推着轴在静摩擦里来回找位置。
    double approach_gain = kSoftLimitApproachGain; // 1/s，点动接近软限位时允许的速度 = 离限位的距离 × 它。
    // 转子 rpm，实际转速比速度目标快出这么多、速度环积分又在往同一方向推时，积分按 overspeed_bleed_time（s）回落；0 表示关闭。
    double overspeed_rpm = 0;
    double overspeed_bleed_time = 0.02;
    double overspeed_filter_time = 0; // s，超速补偿采用的转速低通时间常数；速度环仍用原始反馈。
    // 重力前馈：位置离 gravity_zero（rad）转过 90° 时补的电流原始值，按 sin 缩放、推离 gravity_zero；0 表示不补。
    int gravity_current = 0;
    double gravity_zero = 0;
    double gravity_fade = 0; // m 或 rad，越过软限位这么远时重力前馈线性减到 0；0 表示不减。
    // 推摇杆时顺着点动方向补的电流原始值，少等积分攒够越过摩擦的电流；0 表示不补。
    int jog_friction_current = 0;
    // m 或 rad，点动目标离软限位不到这么远时摩擦前馈线性减到 0；0 表示目标到限位才不补。
    double jog_friction_fade = 0;
    // MoveTo 用（jog_moves_target 的轴）：离终点很近时的最低速度（m/s 或 rad/s）和算走到的距离；任一为 0 时不能用 MoveTo。
    double move_creep = 0;
    double move_tolerance = 0;
    // MoveTo 离终点不到 move_near 时，move_settle_time（s）内挪动不到 move_still_band 就算卡住：先按 move_push（单位/s）给速度目标，
    // 再推 move_push_time（s）还不动就按当时位置保持，算走到。move_near 为 0 表示不用这条。
    double move_near = 0;
    double move_settle_time = 0;
    double move_still_band = 0;
    double move_push = 0;
    double move_push_time = 0;
};
// 上电无力且未回零；未回零只允许低速点动和回零。
class MotorAxis {
public:
    explicit MotorAxis(AxisConfig config);
    // 回零后设置 m 或 rad 目标，软限位夹紧；非法值返回 false。
    bool SetTarget(double target);
    // 从反馈位置以 rate 单位/s 限速到终点，仅回零后可用。
    bool RampTo(double target, double rate);
    // holding=true 时保持进入瞬间的位置 m 或 rad；重复进入不重采样，退出继续原目标。
    // 未回零时拒绝进入并返回 false；保持期间电流仍受正常上限约束。
    bool SetHolding(bool holding);
    // 点动单位/s；未回零限速为回零速度，限流比例由配置决定。
    // 未回零时往回零反方向的点动返回 false，并停掉之前的点动。
    // jog_moves_target 的轴回零以后按速度移动位置目标，松手按当时位置保持。
    bool Jog(double velocity);
    // 回零后把目标设成当前位置（软限位夹紧）并清环路，按位置环保持，Settled 以它为终点；没回零返回 false。
    // hold_integral_limit 大于 0 时保留速度环积分；低速向外点动松手时电流连续，向中间松手时移除点动前馈。
    // 重力前馈加积分限到 hold_integral_limit。
    bool HoldPosition();
    // jog_moves_target 的轴回零以后用：像推摇杆一样以 speed（单位/s）走向 goal（软限位夹紧），离终点越近越慢
    // （距离 × approach_gain，最低 move_creep），离终点不到 move_tolerance 或已越过终点时和松手一样保持。
    // 点动、SetTarget、RampTo、HoldPosition、Stop、断力和离线都会结束它；不能用时返回 false。
    bool MoveTo(double goal, double speed);
    // MoveTo 还没走到。
    bool Moving() const { return moving_; }
    // 接收连续转子计数及转子 rpm；非法反馈锁存故障。
    void Feedback(double counts, double rpm);
    // 每 dt 秒计算电流；disabled 输出零并清积分，offline 清状态并等待重连。
    int Update(double dt, bool enabled, bool online);
    // 配置位置单位与连续转子计数双向换算。
    double PositionToCounts(double position) const;
    double CountsToPosition(double counts) const;
    // 最终目标误差和机构速度均严格小于给定阈值才到位。
    bool Settled(double tolerance, double velocity_tolerance) const;
    // 开始朝配置端点低速回零；无反馈或故障不允许启动。
    bool StartHome();
    // 有效反馈下把当前位置定义成 position（m 或 rad），目标同位，无运动；故障时拒绝。
    bool HomeAt(double position);
    // 停止输出；中断回零则恢复未回零状态。
    void Stop();
    // 锁存轴故障。
    void MarkFault();
    // 清除可恢复故障，重新等待反馈和回零。
    void ClearFault();
    // 查询回零状态、最终和限速目标、反馈位置及机构速度。
    bool Homed() const { return home_state_ == HomeState::Homed; }
    // 回零走过 home_seek_limit 还没堵转而锁存的故障；ClearFault 清掉。
    bool HomeOverrun() const { return home_overrun_; }
    HomeState State() const { return home_state_; }
    double Target() const { return target_; }
    double Position() const { return CountsToPosition(counts_); }
    double Velocity() const { return rpm_ * kEncoderRange / (60 * config_.counts_per_unit); }
    // 当前阶段的原始电流限幅。
    int CurrentLimit() const;
    // 把外部堵转限流后的电流反馈给速度环，未限流时不改变状态。
    void TrackLimitedCurrent(int requested, int actual);
private:
    // MoveTo 每拍按离终点的距离设点动速度，走到了就和松手一样保持。
    void StepMove(double dt);
    bool moving_ = false;
    double goal_ = 0, move_speed_ = 0;
    double move_still_ = 0; // MoveTo 离终点不到 move_near 且挪动不到 move_still_band 的累计时间，s。
    double still_position_ = 0; // 开始累计 move_still_ 时的位置。
    // jog_moves_target 的轴回零以后的位置控制：移动点动目标，把点动速度前馈给速度环，按离软限位的距离限速。
    int FollowTarget(double dt);
    // 当前位置的重力前馈，电流原始值，已按 counts_per_unit 的符号换到电流方向。
    double GravityFeedforward() const;
    AxisConfig config_;
    Cascade loop_;
    double counts_ = 0, rpm_ = 0, offset_ = 0;
    double target_ = 0, end_ = 0, rate_ = 0, jog_ = 0;
    double home_time_ = 0, stall_time_ = 0;
    double home_start_ = 0; // 回零开始时的位置。
    bool feedback_valid_ = false, active_ = false, jogging_ = false;
    bool holding_ = false;
    double hold_target_ = 0;
    HomeState home_state_ = HomeState::Idle;
    bool home_overrun_ = false;
    int follow_command_ = 0; // FollowTarget 上一拍算出的电流，点动中松手时据此保持电流连续。
    double overspeed_feedback_ = 0; // rpm，平滑摩擦撤除和积分回落的判据。
    double hold_preload_ = 0; // 松手时为保持电流连续并进积分的那部分，往反方向点动时扣回。
};
}
