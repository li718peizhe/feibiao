// LibXR PWM 舵机适配：50 Hz 脉宽由纯逻辑 ServoRamp 计算。
#pragma once
#include "servo_ramp.hpp"
#include "pwm.hpp"
#include <cmath>

namespace dart {
// 到位为估计角度，实车没有舵机反馈。
class ServoPort {
public:
    // initial、offset 为 rad，direction 为方向系数。
    // enabled=false 时不初始化 PWM 通道，Update/SetTarget 直接返回。
    ServoPort(LibXR::PWM &pwm, double initial, double offset, double direction,
              bool enabled = true);
    // 设置机构角 rad，非法值由 ServoRamp 拒绝；speed 为这一段的角速度 rad/s，0 表示按 kServoSpeed。
    void SetTarget(double angle, double speed = 0);
    // 用 dt（s）推进角度估计并输出脉宽。
    void Update(double dt);
    // 查询 rad 到位条件和当前限速角度。
    bool At(double angle) const { return enabled_ && ramp_.Done() && std::fabs(ramp_.Value() - angle) < kServoTolerance; }
    double Value() const { return ramp_.Value(); }
    // 当前输出脉宽 µs，与本拍 PWM 占空比一致。
    double PulseUs() const { return enabled_ ? ServoRamp::PulseUs(ramp_.Value(), offset_, direction_) : 0; }
    // 直接访问底层 PWM 通道，仅供未标定的原始脉宽模式使用。
    LibXR::PWM &PwmRef() { return pwm_; }
private:
    LibXR::PWM &pwm_;
    ServoRamp ramp_;
    double offset_, direction_;
    bool enabled_;
};
}
