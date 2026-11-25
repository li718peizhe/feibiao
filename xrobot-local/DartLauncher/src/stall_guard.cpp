#include "stall_guard.hpp"
#include <algorithm>
#include <cmath>

namespace dart {
StallGuard::StallGuard(int limit, double rpm, double time)
    : limit_(limit), rpm_(rpm), time_(time) {}

void StallGuard::Reset() {
    tripped_ = false;
    recovering_ = false;
    elapsed_ = 0;
}

int StallGuard::Update(int request, double rpm, double dt) {
    if (!std::isfinite(rpm) || !std::isfinite(dt) || dt < 0) {
        tripped_ = true;
        recovering_ = false;
        elapsed_ = 0;
    } else if (tripped_) {
        // 保持一段连续的低请求，避免转速反馈跳动使高电流反复重新计时。
        if (request > -limit_ && request < limit_) {
            if (recovering_) {
                elapsed_ += dt;
                if (elapsed_ >= time_) { tripped_ = false; recovering_ = false; elapsed_ = 0; }
            } else {
                recovering_ = true;
                elapsed_ = 0;
            }
        } else {
            recovering_ = false;
            elapsed_ = 0;
        }
    } else if ((request > limit_ || request < -limit_) && std::fabs(rpm) <= rpm_) {
        elapsed_ += dt;
        if (elapsed_ >= time_) { tripped_ = true; elapsed_ = 0; }
    } else {
        elapsed_ = 0;
    }
    return tripped_ ? std::clamp(request, -limit_, limit_) : request;
}
}
