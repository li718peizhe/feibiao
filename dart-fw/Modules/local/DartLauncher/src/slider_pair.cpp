// 左右物理位移差相对基准变化超过 5 mm 判为跳齿，故障后目标保持。
#include "slider_pair.hpp"
#include <cmath>

namespace dart {
bool SliderPair::SetTarget(double target) {
    if (fault_ || !std::isfinite(target)) { return false; }
    target_ = Clamp(target, kSliderBottom, kSliderTop);
    end_ = target_;
    speed_ = 0;
    return true;
}
bool SliderPair::RampTo(double target, double speed, double left, double right) {
    if (!std::isfinite(target) || !std::isfinite(speed) || speed <= 0 ||
        !Update(left, right)) { return false; }
    target_ = Clamp((left + right) * 0.5, kSliderBottom, kSliderTop);
    end_ = Clamp(target, kSliderBottom, kSliderTop);
    speed_ = speed;
    return true;
}
bool SliderPair::Update(double left, double right, double dt) {
    if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(dt) || dt <= 0 ||
        std::fabs(left - right - baseline_) > kSliderSyncTolerance) { fault_ = true; }
    if (!fault_ && speed_ > 0) { target_ += Clamp(end_ - target_, -speed_ * dt, speed_ * dt); }
    return !fault_;
}
bool SliderPair::Rebase(double left, double right) {
    if (!std::isfinite(left) || !std::isfinite(right)) { return false; }
    baseline_ = left - right;
    fault_ = false;
    return true;
}
}
