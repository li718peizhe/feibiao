// PID 的积分先限幅，微分使用相邻两次误差。
#include "pid.hpp"
#include <cmath>

namespace dart {
Pid::Pid(PidGains gains) : gains_(gains) {}
double Pid::Calc(double reference, double feedback) {
    if (!std::isfinite(reference) || !std::isfinite(feedback)) {
        Reset();
        return 0;
    }
    const double error = reference - feedback;
    integral_ = Clamp(integral_ + gains_.ki * error, -gains_.max_iout, gains_.max_iout);
    const double output = Clamp(gains_.kp * error + integral_ + gains_.kd * (error - last_error_),
                                -gains_.max_out, gains_.max_out);
    last_error_ = error;
    return output;
}
void Pid::Reset() {
    integral_ = 0;
    last_error_ = 0;
}
void Pid::LimitIntegral(double low, double high) {
    if (!std::isfinite(low) || !std::isfinite(high) || low > high) {
        Reset();
        return;
    }
    integral_ = Clamp(integral_, low, high);
    last_error_ = 0;
}
void Pid::TrackLimitedOutput(int requested, int actual) {
    integral_ = Clamp(integral_ + static_cast<double>(actual) - requested,
                      -gains_.max_iout, gains_.max_iout);
}
void Pid::BleedIntegral(double fraction) {
    integral_ *= 1 - Clamp(fraction, 0, 1);
}
Cascade::Cascade(PidGains position, PidGains speed, int command_max)
    : position_(position), speed_(speed), command_max_(command_max) {}
int Cascade::Update(double target_counts, double counts, double rpm, double feedforward) {
    if (!std::isfinite(target_counts) || !std::isfinite(counts) || !std::isfinite(rpm) ||
        !std::isfinite(feedforward)) {
        Reset();
        return 0;
    }
    return static_cast<int>(Clamp(speed_.Calc(position_.Calc(target_counts, counts), rpm) + feedforward,
                                  -command_max_, command_max_));
}
void Cascade::Reset() {
    position_.Reset();
    speed_.Reset();
}
void Cascade::ResetForHold(double low, double high) {
    position_.Reset();
    speed_.LimitIntegral(low, high);
}
void Cascade::TrackLimitedOutput(int requested, int actual) {
    speed_.TrackLimitedOutput(requested, actual);
}
void Cascade::BleedSpeedIntegral(double fraction) {
    speed_.BleedIntegral(fraction);
}
double Cascade::Position(double target_counts, double counts) {
    return position_.Calc(target_counts, counts);
}
int Cascade::Speed(double reference_rpm, double rpm, int current_limit) {
    return static_cast<int>(Clamp(speed_.Calc(reference_rpm, rpm), -current_limit, current_limit));
}
}
