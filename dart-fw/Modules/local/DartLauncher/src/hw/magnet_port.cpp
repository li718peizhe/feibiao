// 电磁铁继电器使用 50 Hz 定时器输出恒定电平；enabled=false 时跳过硬件。
#include "magnet_port.hpp"
#include "dart_params.hpp"
namespace dart {
MagnetPort::MagnetPort(LibXR::PWM &pwm, bool enabled) : pwm_(pwm), enabled_(enabled) {
    if (enabled_) {
        pwm_.SetConfig({50});
        Set(false);
        pwm_.Enable();
    }
}
void MagnetPort::Set(bool on) {
    // 占空比 1 一直高、0 一直低；控制板复位期间引脚不归固件管，继电器是否保持断开靠模块电路。
    if (enabled_) { pwm_.SetDutyCycle(on == kMagnetRelayActiveHigh ? 1.0F : 0.0F); }
}
}
