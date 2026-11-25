// 正反过零采用最短的单圈差值。
#include "rotor_counter.hpp"

namespace dart {
bool RotorCounter::Update(std::uint16_t angle) {
    if (angle >= 8192) { return false; }
    if (!initialized_) {
        counts_ = angle;
        initialized_ = true;
    }
    else {
        int delta = static_cast<int>(angle) - last_;
        if (delta > 4096) { delta -= 8192; }
        else if (delta < -4096) { delta += 8192; }
        counts_ += delta;
    }
    last_ = angle;
    return true;
}
void RotorCounter::Reset() {
    initialized_ = false;
    last_ = 0;
    counts_ = 0;
}
}
