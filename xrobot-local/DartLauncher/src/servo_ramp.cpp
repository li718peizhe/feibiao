// 每拍角度变化与 motors.py RateLimiter 相同。
#include "servo_ramp.hpp"
#include <cmath>

namespace dart {
ServoRamp::ServoRamp(double speed, double initial)
    : default_speed_(std::isfinite(speed) ? std::fabs(speed) : 0), speed_(default_speed_),
      value_(std::isfinite(initial) ? Clamp(initial, 0, kServoRange) : 0), target_(value_) {}
bool ServoRamp::SetTarget(double angle, double speed) {
    if (!std::isfinite(angle)) { return false; }
    target_ = Clamp(angle, 0, kServoRange);
    speed_ = std::isfinite(speed) && speed > 0 ? speed : default_speed_;
    return true;
}
double ServoRamp::Step(double dt) {
    if (std::isfinite(dt) && dt > 0) { value_ += Clamp(target_ - value_, -speed_ * dt, speed_ * dt); }
    return value_;
}
bool ServoRamp::Done(double tolerance) const { return std::fabs(target_ - value_) < tolerance; }
double ServoRamp::PulseUs(double angle, double offset, double direction) {
    const double mapped = (std::isfinite(offset) ? offset : 0) +
        (std::isfinite(direction) ? direction : 1) * (std::isfinite(angle) ? angle : 0);
    return kServoPulseMin + Clamp(mapped, 0, kServoRange) / kServoRange *
        (kServoPulseMax - kServoPulseMin);
}
}
