// LibXR PWM 电磁铁继电器适配。
#pragma once
#include "pwm.hpp"
namespace dart {
// 继电器只认恒定电平：50 Hz PWM 只用 0/1 占空比；通电对应高还是低由 kMagnetRelayActiveHigh 决定。
class MagnetPort {
public:
    // 先写断电电平再启用通道。enabled=false 时不初始化 PWM。
    explicit MagnetPort(LibXR::PWM &pwm, bool enabled = true);
    // on=true 为通电消磁，false 为断电吸持，无单位。限时由 MagnetGuard 管，这里只写电平。
    void Set(bool on);
private:
    LibXR::PWM &pwm_;
    bool enabled_;
};
}
