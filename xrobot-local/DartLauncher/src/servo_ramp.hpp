// 舵机角目标限速及角度到 PWM 脉宽的换算。
#pragma once
#include "dart_params.hpp"

namespace dart {
// 角度 rad，角速度 rad/s；默认估计值待确认。
class ServoRamp {
public:
    explicit ServoRamp(double speed = kServoSpeed, double initial = 0);
    // 设置 0~kServoRange rad 的目标，非法值返回 false；speed 为这一段的角速度 rad/s，0 表示按构造时的速度。
    bool SetTarget(double angle, double speed = 0);
    // 经过 dt 秒后的限速角度 rad，非法周期保持原值。
    double Step(double dt);
    // 目标角误差小于 tolerance rad 时返回 true。
    bool Done(double tolerance = 1e-3) const;
    // 当前角度 rad。
    double Value() const { return value_; }
    // 舵机角 = 偏置 rad + 方向 × 机构角 rad，夹到量程后映射为 500~2500 µs。
    static double PulseUs(double angle, double offset = 0, double direction = 1);
private:
    double default_speed_, speed_, value_, target_;
};
}
