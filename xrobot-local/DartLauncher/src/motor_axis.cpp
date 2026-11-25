// 回零通过速度环找堵转端点，建立编码器偏置后低速退开。
#include "motor_axis.hpp"
#include <algorithm>
#include <cmath>

namespace dart {
MotorAxis::MotorAxis(AxisConfig config)
    : config_(config), loop_(config.position_gains, config.speed_gains, config.command_max) {
    target_ = end_ = Clamp(0, config_.minimum, config_.maximum);
    if (!std::isfinite(config_.counts_per_unit) || config_.counts_per_unit == 0 ||
        config_.minimum >= config_.maximum || config_.home_speed <= 0 ||
        config_.current_limit <= 0) {
        MarkFault();
    }
}
bool MotorAxis::SetTarget(double target) {
    if (!Homed() || !std::isfinite(target)) {
        return false;
    }
    target_ = end_ = Clamp(target, config_.minimum, config_.maximum);
    moving_ = false;
    active_ = true;
    jogging_ = false;
    rate_ = jog_ = 0;
    return true;
}
bool MotorAxis::RampTo(double target, double rate) {
    if (!Homed() || !std::isfinite(target) || !std::isfinite(rate) || rate <= 0) {
        return false;
    }
    target_ = Clamp(Position(), config_.minimum, config_.maximum);
    end_ = Clamp(target, config_.minimum, config_.maximum);
    moving_ = false;
    rate_ = rate;
    jog_ = 0;
    active_ = true;
    jogging_ = false;
    return true;
}
bool MotorAxis::Jog(double velocity) {
    moving_ = false;
    if (home_state_ == HomeState::Failed || home_state_ == HomeState::Seeking ||
        home_state_ == HomeState::Retreating || !std::isfinite(velocity)) {
        return false;
    }
    if (!Homed() && velocity * config_.home_direction < 0) {
        // 拒绝反向点动时停掉上一次的点动，防止摇杆跳过死区后按旧速度继续走。
        jog_ = 0;
        return false;
    }
    if (Homed() && config_.jog_moves_target) {
        // 松手只在点动中或停机后第一次收到 0 时取位置，遥控每 10 ms 重复的 0 不再重取。
        if (velocity == 0) {
            return jog_ != 0 || !active_ ? HoldPosition() : true;
        }
        // 从保持接着点动时沿用原目标和积分，接管时支撑力不跳。
        if (!active_ || jogging_) {
            target_ = end_ = Clamp(Position(), config_.minimum, config_.maximum);
        }
        // 松手时为压住摆臂并进积分的那部分如果顶着这次点动的方向，开始点动时扣回，
        // 否则换向要先把电流从摩擦带一头慢慢拉到另一头才开始动；点动时托住重力的积分照留。
        if (active_ && jog_ == 0) {
            const double motion = (velocity > 0) == (config_.counts_per_unit > 0) ? 1.0 : -1.0;
            if (hold_preload_ * motion < 0) {
                const double integral = loop_.SpeedIntegral() - hold_preload_;
                loop_.ResetForHold(integral, integral);
            }
        }
        hold_preload_ = 0;
        jog_ = velocity;
        rate_ = 0;
        jogging_ = false;
        active_ = true;
        return true;
    }
    // 松手保持的轴：回零以后只在点动中第一次收到 0 时取位置，遥控每 10 ms 重复的 0 不再重取。
    if (Homed() && config_.hold_on_release && velocity == 0) {
        if (jogging_ || !active_) {
            return HoldPosition();
        }
        return true;
    }
    jog_ = Homed() ? velocity : Clamp(velocity, -config_.home_speed, config_.home_speed);
    jogging_ = true;
    active_ = true;
    return true;
}
bool MotorAxis::HoldPosition() {
    moving_ = false;
    if (!Homed()) {
        return false;
    }
    const bool releasing = config_.jog_moves_target && active_ && jog_ != 0 && feedback_valid_ &&
        std::fabs(rpm_) <= config_.release_still_rpm &&
        jog_ * config_.counts_per_unit * GravityFeedforward() >= 0;
    target_ = end_ = Clamp(Position(), config_.minimum, config_.maximum);
    rate_ = 0;
    jog_ = 0;
    jogging_ = false;
    active_ = true;
    // 靠电流托住重力的轴松手时不清积分：清掉会让支撑力瞬间消失、轴往下掉。
    // 往外点动中几乎不动时松手，那一拍让电流保持连续（摩擦前馈和比例项并入积分）：摆臂有弹性，
    // 11-12 22:57 实车松手时电流一降就弹回约 0.5°，往外点一下的位移被吃掉大半。快速转动中松手不保持，
    // 免得推着它的电流让它继续滑。往中间点动时也只保留积分，移除驱动它回落的摩擦前馈。
    // 两种都只把重力前馈加积分限到 hold_integral_limit。
    if (config_.hold_integral_limit > 0) {
        const double limit = std::min(config_.hold_integral_limit, config_.current_limit);
        const double gravity = feedback_valid_ ? GravityFeedforward() : 0;
        hold_preload_ = 0;
        if (releasing) {
            const double before = loop_.SpeedIntegral();
            const double integral = Clamp(follow_command_, -limit, limit) - gravity;
            loop_.ResetForHold(integral, integral);
            hold_preload_ = integral - before;
        } else {
            loop_.ResetForHold(-limit - gravity, limit - gravity);
        }
    } else {
        loop_.Reset();
    }
    return true;
}
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
    move_still_ = 0;
    still_position_ = Position();
    moving_ = true;
    return true;
}
void MotorAxis::StepMove(double dt) {
    const double position = Position();
    const double remaining = goal_ - position;
    if (std::fabs(remaining) > config_.move_near || std::fabs(position - still_position_) > config_.move_still_band) {
        still_position_ = position;
        move_still_ = 0;
    } else {
        move_still_ += dt;
    }
    // 走到了、越过了终点（和点动方向相反）、或者在终点附近卡住且加大推力也推不动：
    // 和摇杆松手一样按当前位置保持，几乎不动时电流保持连续。
    if (std::fabs(remaining) <= config_.move_tolerance || remaining * jog_ < 0 ||
        (config_.move_near > 0 && move_still_ >= config_.move_settle_time + config_.move_push_time)) {
        HoldPosition();
        return;
    }
    double speed = std::fmax(std::fmin(move_speed_, std::fabs(remaining) * config_.approach_gain), config_.move_creep);
    // 在终点附近卡住：像把摇杆推大一样加大速度目标，电流很快涨上去；目标还是不越过终点。
    if (config_.move_near > 0 && move_still_ >= config_.move_settle_time) { speed = std::fmax(speed, config_.move_push); }
    jog_ = std::copysign(speed, remaining);
}
bool MotorAxis::SetHolding(bool holding) {
    if (holding && (!Homed() || !feedback_valid_)) {
        return false;
    }
    if (holding_ != holding) {
        hold_target_ = Position();
        holding_ = holding;
        loop_.Reset();
    }
    return true;
}
void MotorAxis::Feedback(double counts, double rpm) {
    if (!std::isfinite(counts) || !std::isfinite(rpm)) {
        MarkFault();
        return;
    }
    counts_ = counts;
    rpm_ = rpm;
    feedback_valid_ = true;
}
double MotorAxis::PositionToCounts(double position) const {
    return (position - offset_) * config_.counts_per_unit;
}
double MotorAxis::CountsToPosition(double counts) const {
    return counts / config_.counts_per_unit + offset_;
}
bool MotorAxis::Settled(double tolerance, double velocity_tolerance) const {
    return active_ && !holding_ && feedback_valid_ && std::fabs(Position() - end_) < tolerance &&
        std::fabs(Velocity()) < velocity_tolerance;
}
bool MotorAxis::StartHome() {
    if (!feedback_valid_ || home_state_ == HomeState::Failed) {
        return false;
    }
    home_state_ = HomeState::Seeking;
    moving_ = false;
    home_start_ = Position();
    home_overrun_ = false;
    holding_ = false;
    home_time_ = 0;
    stall_time_ = 0;
    active_ = true;
    jogging_ = false;
    jog_ = 0;
    loop_.Reset();
    return true;
}
bool MotorAxis::HomeAt(double position) {
    if (!feedback_valid_ || home_state_ == HomeState::Failed || !std::isfinite(position)) {
        return false;
    }
    offset_ = position - counts_ / config_.counts_per_unit;
    moving_ = false;
    target_ = end_ = position;
    rate_ = jog_ = home_time_ = stall_time_ = 0;
    active_ = true;
    jogging_ = holding_ = false;
    home_state_ = HomeState::Homed;
    loop_.Reset();
    overspeed_feedback_ = rpm_;
    return true;
}
int MotorAxis::CurrentLimit() const {
    return Homed() ? config_.current_limit :
        static_cast<int>(config_.current_limit * config_.home_current_fraction);
}
void MotorAxis::TrackLimitedCurrent(int requested, int actual) {
    loop_.TrackLimitedOutput(requested, actual);
}
double MotorAxis::GravityFeedforward() const {
    const double position = Position();
    double current = config_.gravity_current * std::sin(position - config_.gravity_zero);
    // 软限位外面逐渐不补：前馈比实际重力大时（例如加了配重没改参数），也不会一直把轴往硬限位推。
    const double beyond = std::fmax(std::fmax(position - config_.maximum, config_.minimum - position), 0.0);
    if (config_.gravity_fade > 0) { current *= std::fmax(1 - beyond / config_.gravity_fade, 0.0); }
    return config_.counts_per_unit < 0 ? -current : current;
}
int MotorAxis::FollowTarget(double dt) {
    overspeed_feedback_ += dt / (config_.overspeed_filter_time + dt) * (rpm_ - overspeed_feedback_);
    const double position = Position();
    double velocity = 0;
    if (jog_ != 0) {
        // 目标离软限位越近走得越慢，到限位正好停下；领先反馈位置 jog_lead 以后不再往前走，
        // 被挡住时误差不会越积越大，挡开以后也不会猛冲。
        velocity = Clamp(jog_, -std::fmax(target_ - config_.minimum, 0.0) * config_.approach_gain,
                         std::fmax(config_.maximum - target_, 0.0) * config_.approach_gain);
        double next = Clamp(target_ + velocity * dt, config_.minimum, config_.maximum);
        if (config_.jog_lead > 0) {
            next = Clamp(next, std::fmin(target_, position - config_.jog_lead),
                         std::fmax(target_, position + config_.jog_lead));
        }
        // MoveTo：目标不越过终点。
        if (moving_) { next = jog_ > 0 ? std::fmin(next, goal_) : std::fmax(next, goal_); }
        target_ = end_ = next;
    }
    // 目标停住、误差在死区内时不再积分，否则静摩擦卡住时积分一直涨，冲开后又往回找。
    const bool still = jog_ == 0 && (rate_ <= 0 || target_ == end_);
    const double reference = still && std::fabs(target_ - position) < config_.hold_deadband ? position : target_;
    const double to_rpm = config_.counts_per_unit * 60 / kEncoderRange;
    double speed = loop_.Position(PositionToCounts(reference), counts_) + velocity * to_rpm;
    // 按实际位置离软限位的距离限制往外的速度，惯性或摩擦突然松脱冲过目标时也会刹住；
    // 只限速度目标，不切电流，托住重力的积分照常输出。
    const double low = -std::fmax(position - config_.minimum, 0.0) * config_.approach_gain * to_rpm;
    const double high = std::fmax(config_.maximum - position, 0.0) * config_.approach_gain * to_rpm;
    speed = Clamp(speed, std::fmin(low, high), std::fmax(low, high));
    // 重力前馈按实际角度给，摩擦前馈顺着点动方向给；电流正方向对应位置的 counts_per_unit 符号方向。
    // 摩擦前馈不跟着接近软限位的减速缩小（镖座离软限位只有 3.5~4.5°，缩小以后在镖座微调推不动），
    // 只在目标离限位最后 jog_friction_fade 里减到 0；按平滑后的超速量撤补偿，
    // 避免低速瞬时 rpm 跳变把整份摩擦补偿反复切掉。
    double friction = 0;
    if (jog_ != 0 && velocity != 0) {
        const double room = jog_ > 0 ? config_.maximum - target_ : target_ - config_.minimum;
        double scale = config_.jog_friction_fade > 0 ? Clamp(room / config_.jog_friction_fade, 0, 1) : 1;
        const double motion = (jog_ > 0) == (config_.counts_per_unit > 0) ? 1.0 : -1.0;
        if (config_.overspeed_rpm > 0) {
            scale *= Clamp(1 - (overspeed_feedback_ - speed) * motion / config_.overspeed_rpm, 0, 1);
        }
        friction = std::copysign(config_.jog_friction_current * scale, jog_);
    }
    if (config_.counts_per_unit < 0) { friction = -friction; }
    const double feedforward = GravityFeedforward() + friction;
    const double total = loop_.Speed(speed, rpm_, config_.current_limit) + feedforward;
    const int command = static_cast<int>(Clamp(total, -config_.current_limit, config_.current_limit));
    // 加上前馈超了上限时把超出的部分退回积分，免得积分接着往外涨。
    if (std::fabs(total) > config_.current_limit) { loop_.TrackLimitedOutput(static_cast<int>(total), command); }
    // 挡开后猛冲、惯性冲过目标时，积分存着的大电流还在往前推，比例项刹不住：
    // 摆臂正顺着积分的方向转、又比速度目标快一截时，让这部分积分快速回落。
    // 摆臂还没动、或正往积分的反方向转（从镖座往回拉）时不泄：积分这时是托住重力的电流，
    // 11-12 21:05~21:29 实车回中突然一冲就是把它泄掉了。松手保持时还要已经冲过目标（超出保持死区）才泄。
    const double integral = loop_.SpeedIntegral();
    const double over = overspeed_feedback_ - speed;
    const double past = (position - target_) * config_.counts_per_unit;
    const bool overshot = std::fabs(position - target_) > config_.hold_deadband && past * integral > 0;
    if (config_.overspeed_rpm > 0 && config_.overspeed_bleed_time > 0 && rpm_ * integral > 0 &&
        std::fabs(over) > config_.overspeed_rpm && over * integral > 0 && (jog_ != 0 || overshot)) {
        loop_.BleedSpeedIntegral(dt / config_.overspeed_bleed_time);
    }
    follow_command_ = command;
    return command;
}
int MotorAxis::Update(double dt, bool enabled, bool online) {
    if (!online) {
        moving_ = false;
        loop_.Reset();
        overspeed_feedback_ = rpm_;
        feedback_valid_ = false;
        active_ = false;
        jogging_ = false;
        holding_ = false;
        jog_ = 0;
        stall_time_ = 0;
        if (home_state_ != HomeState::Failed) {
            home_state_ = HomeState::Idle;
        }
        return 0;
    }
    if (!std::isfinite(dt) || dt <= 0) {
        MarkFault();
        return 0;
    }
    if (!enabled) {
        moving_ = false;
        loop_.Reset();
        overspeed_feedback_ = rpm_;
        stall_time_ = 0;
        // 不自锁的轴断力时会被重力带走，重新使能后等新命令按当时位置取目标，不拉回断力前的目标。
        if (config_.jog_moves_target) {
            active_ = false;
            jog_ = 0;
        }
        return 0;
    }
    if ((!active_ && !holding_) || !feedback_valid_ || home_state_ == HomeState::Failed) {
        return 0;
    }
    const int low_limit = static_cast<int>(config_.current_limit * config_.home_current_fraction);
    if (home_state_ == HomeState::Seeking || home_state_ == HomeState::Retreating) {
        home_time_ += dt;
        if (home_time_ > kHomeTimeout) {
            MarkFault();
            return 0;
        }
        if (home_state_ == HomeState::Seeking) {
            // 联轴器打滑或者没碰到挡块时，电机一直转、判不成堵转；走过上限就锁存故障。
            if (std::fabs(Position() - home_start_) > config_.home_seek_limit) {
                home_overrun_ = true;
                MarkFault();
                return 0;
            }
            const double velocity = config_.home_direction * config_.home_speed;
            const int command = loop_.Speed(velocity * config_.counts_per_unit * 60 / kEncoderRange,
                                             rpm_, low_limit);
            stall_time_ = std::abs(command) >= low_limit && std::fabs(rpm_) < kHomeStallRpm
                ? stall_time_ + dt
                : 0;
            if (stall_time_ >= config_.home_stall_time) {
                offset_ = config_.home_position - counts_ / config_.counts_per_unit;
                target_ = end_ = Clamp(
                    config_.home_position - config_.home_direction * config_.retreat,
                    config_.minimum, config_.maximum);
                home_state_ = HomeState::Retreating;
                loop_.Reset();
                return 0;
            }
            return command;
        }
        const double error = end_ - Position();
        if (std::fabs(error) < config_.home_tolerance && std::fabs(rpm_) < kHomeStallRpm) {
            home_state_ = HomeState::Homed;
            loop_.Reset();
            return 0;
        }
        return static_cast<int>(Clamp(
            loop_.Update(PositionToCounts(target_), counts_, rpm_), -low_limit, low_limit));
    }
    if (holding_) {
        return static_cast<int>(Clamp(loop_.Update(PositionToCounts(hold_target_), counts_, rpm_),
                                      -config_.current_limit, config_.current_limit));
    }
    if (jogging_) {
        double velocity = jog_;
        if (Homed()) {
            // 离软限位越近，往那边允许的速度越小，到限位正好停下；在软限位外面只准往回走，不往外走，也不强拉回去。
            // 以前按“这一拍正好走到限位”夹速度，等于增益 1/dt 的位置环，速度环跟不上，摆臂在限位上猛抖。
            velocity = Clamp(velocity, -std::fmax(Position() - config_.minimum, 0.0) * config_.approach_gain,
                              std::fmax(config_.maximum - Position(), 0.0) * config_.approach_gain);
        }
        // 摇杆松回 0 时不管回没回零都清速度环，免得没回零点动堵转后积分顶着上限不回落。
        if (velocity == 0) {
            loop_.Reset();
        }
        return loop_.Speed(velocity * config_.counts_per_unit * 60 / kEncoderRange,
                            rpm_, CurrentLimit());
    }
    if (!Homed()) { return 0; }
    if (rate_ > 0) { target_ += Clamp(end_ - target_, -rate_ * dt, rate_ * dt); }
    if (config_.jog_moves_target) {
        if (moving_) { StepMove(dt); }
        return FollowTarget(dt);
    }
    return static_cast<int>(Clamp(loop_.Update(PositionToCounts(target_), counts_, rpm_),
                                  -config_.current_limit, config_.current_limit));
}
void MotorAxis::Stop() {
    moving_ = false;
    active_ = false;
    jogging_ = false;
    holding_ = false;
    jog_ = 0;
    hold_preload_ = 0;
    loop_.Reset();
    overspeed_feedback_ = rpm_;
    if (home_state_ == HomeState::Seeking || home_state_ == HomeState::Retreating) {
        home_state_ = HomeState::Idle;
    }
}
void MotorAxis::MarkFault() {
    Stop();
    home_state_ = HomeState::Failed;
}
void MotorAxis::ClearFault() {
    if (home_state_ != HomeState::Failed) {
        return;
    }
    home_state_ = HomeState::Idle;
    moving_ = false;
    home_overrun_ = false;
    feedback_valid_ = false;
    active_ = false;
    jogging_ = false;
    jog_ = 0;
    loop_.Reset();
}
}
