// 电磁铁通电限时，时间单位 s。
#include "magnet_guard.hpp"
#include "dart_params.hpp"
#include <cmath>

namespace dart {
void MagnetGuard::Command(bool on) {
    if (!on) {
        on_ = false;
        on_time_ = 0;
    } else if (!on_) {
        on_ = true;
        on_time_ = 0;
    }
}

MagnetCut MagnetGuard::Update(double dt, bool allowed) {
    if (!on_) { return MagnetCut::None; }
    // 时间步无效时不知道已经通了多久，按断电处理。
    if (!allowed || !std::isfinite(dt) || dt < 0) {
        Command(false);
        return MagnetCut::Blocked;
    }
    on_time_ += dt;
    if (on_time_ >= kMagnetOnLimit) {
        Command(false);
        return MagnetCut::Expired;
    }
    return MagnetCut::None;
}
}
