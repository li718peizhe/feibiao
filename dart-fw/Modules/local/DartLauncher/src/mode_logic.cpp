// 自动拨杆事件只接受自动模式内的中位到上下位；重连不重放旧裁判指令。
#include "mode_logic.hpp"
#include <cmath>

namespace dart {
namespace {
// 归一化摇杆死区后重新缩放，保留正负方向和满量程。
double Stick(double value) {
    value = Clamp(value, -1, 1);
    const double magnitude = std::fabs(value);
    return magnitude <= kStickDeadband ? 0 :
        std::copysign((magnitude - kStickDeadband) / (1 - kStickDeadband), value);
}
// 摆臂摇杆：更大的死区滤掉回中时的反向抖动，死区外按 kArmStickExponent 次方给速度。
double ArmStick(double value) {
    value = Clamp(value, -1, 1);
    const double magnitude = std::fabs(value);
    return magnitude <= kArmStickDeadband ? 0 :
        std::copysign(std::pow((magnitude - kArmStickDeadband) / (1 - kArmStickDeadband), kArmStickExponent), value);
}
}
ModeLogic::ModeLogic(bool auto_enabled) : auto_enabled_(auto_enabled) {}
ModeCommand ModeLogic::Update(const RemoteInput &r, const RefereeInput &f, double dt) {
    ModeCommand out;
    const bool valid = std::isfinite(dt) && dt > 0 &&
        std::isfinite(r.left_x) && std::isfinite(r.right_y) && std::isfinite(r.wheel) &&
        (r.left == Switch::Up || r.left == Switch::Middle || r.left == Switch::Down) &&
        (r.right == Switch::Up || r.right == Switch::Middle || r.right == Switch::Down);
    if (r.online && valid) {
        out.mode = r.right == Switch::Middle ? Mode::Manual :
            r.right == Switch::Up && auto_enabled_ ? Mode::Automatic : Mode::Powerless;
    }
    if (r.online && valid && r.right == Switch::Down) {
        fault_reset_time_ += dt;
    } else {
        out.fault_reset = r.online && valid && r.right == Switch::Middle &&
            fault_reset_time_ >= kFaultResetTime;
        fault_reset_time_ = 0;
    }
    out.group = r.left == Switch::Up ? ManualGroup::Aim :
        r.left == Switch::Down ? ManualGroup::Loading : ManualGroup::Slider;
    out.stop_sequence = !r.online || !valid ||
        (initialized_ && previous_mode_ == Mode::Automatic && out.mode == Mode::Manual);
    out.leave_manual = initialized_ && previous_mode_ == Mode::Manual && out.mode != Mode::Manual;
    if (out.mode == Mode::Manual) {
        out.stop_sequence = out.stop_sequence || std::fabs(r.left_x) > 0.1 ||
            std::fabs(r.right_y) > 0.1 || (initialized_ && previous_left_ != r.left);
        const double x = Stick(r.left_x), y = Stick(r.right_y);
        switch (out.group) {
        case ManualGroup::Aim:
            out.yaw_rate = x * kManualYawSpeed;
            out.pitch_rate = y * kManualPitchSpeed;
            break;
        case ManualGroup::Slider:
            // 滑块坐标向上为正，按实车方向将右摇杆纵向通道直接映射到滑块速度。
            out.slider_rate = y * kManualSliderSpeed;
            out.lock_rate = x * kServoSpeed;
            break;
        case ManualGroup::Loading:
            out.arm_rate = ArmStick(r.left_x) * kArmSpeed;
            out.crank_rate = y * kServoSpeed;
            break;
        }
        if (previous_mode_ != Mode::Manual || previous_group_ != out.group) {
            wheel_time_ = 0;
            wheel_sent_ = false;
        }
        const int direction = r.wheel >= 0.95 ? 1 : r.wheel <= -0.95 ? -1 : 0;
        if (direction != wheel_direction_) {
            wheel_time_ = 0;
            wheel_sent_ = false;
        }
        wheel_direction_ = direction;
        if (direction != 0) {
            wheel_time_ += dt;
            if (wheel_time_ >= kHomeWheelTime && !wheel_sent_) {
                // 正向在瞄准组同时回零瞄准与滑块，在滑块组仅回零滑块，在装弹组是自动取镖试运行（摆臂按编码器绝对角，不用回零）。
                out.home_aim = direction > 0 && out.group == ManualGroup::Aim;
                out.home_slider = direction > 0 && (out.group == ManualGroup::Aim || out.group == ManualGroup::Slider);
                out.fetch_request = direction > 0 && out.group == ManualGroup::Loading;
                out.cock_request = direction < 0 && out.group == ManualGroup::Slider;
                // 负向在滑块组是上膛，在装弹组是切换电磁铁；和回零、上膛一样每次保持只出一次。
                out.magnet_toggle = direction < 0 && out.group == ManualGroup::Loading;
                wheel_sent_ = true;
            }
        } else {
            wheel_time_ = 0;
            wheel_sent_ = false;
        }
    } else {
        wheel_direction_ = 0;
        wheel_time_ = 0;
        wheel_sent_ = false;
    }
    if (out.mode == Mode::Automatic && previous_mode_ == Mode::Automatic &&
        initialized_ && previous_left_ == Switch::Middle) {
        out.fire = r.left == Switch::Up;
        out.salvo = r.left == Switch::Down;
    }
    const bool new_time = f.online && previous_referee_online_ && referee_initialized_ &&
        f.launch_time != previous_launch_time_;
    if (out.mode == Mode::Automatic && f.gate_open && new_time) { out.fire = true; }
    if (f.online) {
        previous_launch_time_ = f.launch_time;
        referee_initialized_ = true;
    }
    previous_referee_online_ = f.online;
    previous_mode_ = out.mode;
    previous_group_ = out.group;
    previous_left_ = r.left;
    initialized_ = true;
    return out;
}
}
