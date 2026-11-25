// 电磁铁通电限时：通电消磁只许短时，到时、无力或故障都自动断电吸持；不引用硬件框架。
#pragma once
#include <cstdint>

namespace dart {
// Update 这一拍自动断电的原因：没断、通电到时、无力或故障（或时间步无效）。
enum class MagnetCut : std::uint8_t { None, Expired, Blocked };

// 只管通断命令，不知道继电器电平；适配层每拍按 On() 写继电器。
class MagnetGuard {
public:
    // on=true 通电消磁，false 断电吸持。已经通电时重复通电命令不重新计时，免得一直续上。
    void Command(bool on);
    // 每拍调用，dt 单位 s；allowed=false（无力或故障）立即断电。
    MagnetCut Update(double dt, bool allowed);
    bool On() const { return on_; }
private:
    bool on_ = false;
    double on_time_ = 0; // s，这次通电已持续的时间
};
}
