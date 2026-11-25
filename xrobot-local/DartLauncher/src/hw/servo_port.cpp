// 舵机上电即输出安全初值，随后按目标限速。enabled=false 时不驱动 PWM。
#include "servo_port.hpp"
#include <cmath>
namespace dart {
ServoPort::ServoPort(LibXR::PWM &pwm, double initial, double offset, double direction,
                     bool enabled)
    : pwm_(pwm), ramp_(kServoSpeed, initial), offset_(offset), direction_(direction),
      enabled_(enabled) {
    if (enabled_) {
        pwm_.SetConfig({static_cast<std::uint32_t>(kServoFrequency)});
        pwm_.SetDutyCycle(static_cast<float>(ServoRamp::PulseUs(initial, offset_, direction_) / 20000.0));
        pwm_.Enable();
    }
}
void ServoPort::SetTarget(double angle, double speed) {
    if (enabled_) { ramp_.SetTarget(angle, speed); }
}
void ServoPort::Update(double dt) {
    if (!enabled_) { return; }
    const double pulse = ServoRamp::PulseUs(ramp_.Step(dt), offset_, direction_);
    pwm_.SetDutyCycle(static_cast<float>(pulse / 20000.0));
}
}
