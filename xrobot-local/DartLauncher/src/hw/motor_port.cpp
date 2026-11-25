// RMMotor 的反馈回调只递增计数，离线判定在控制线程完成。
#include "motor_port.hpp"
#include <cmath>

namespace dart {
MotorPort::MotorPort(RMMotor &motor, LibXR::CAN &can, std::uint32_t id,
                     AxisConfig config, int index)
    : motor_(motor), axis_(config), index_(index), command_max_(config.command_max),
      callback_(LibXR::CAN::Callback::Create([](bool isr, MotorPort *p,
                                                const LibXR::CAN::ClassicPack &pack) {
          OnFrame(isr, p, pack);
      }, this)) {
    can.Register(callback_, LibXR::CAN::Type::STANDARD, LibXR::CAN::FilterMode::ID_RANGE, id, id);
}
void MotorPort::OnFrame(bool, MotorPort *self, const LibXR::CAN::ClassicPack &) {
    self->frames_.fetch_add(1, std::memory_order_relaxed);
}
void MotorPort::Update(double now, Safety &safety) {
    const auto frames = frames_.load(std::memory_order_relaxed);
    const auto result = motor_.Update();
    if (frames != seen_frames_) {
        seen_frames_ = frames;
        last_feedback_ = now;
        safety.MotorFeedback(index_, now);
        const auto &feedback = motor_.GetFeedback();
        feedback_torque_ = feedback.torque;
        feedback_rpm_ = feedback.velocity;
        feedback_temp_ = feedback.temp;
        // 反馈 rad 可为负；取模前四舍五入避免浮点误差丢失一个编码器计数。
        const double angle = feedback.position;
        if (!std::isfinite(angle) || !std::isfinite(feedback.velocity)) {
            axis_.MarkFault();
        } else {
            const auto counts = static_cast<std::int64_t>(std::llround(angle / (2 * kPi) * kEncoderRange));
            raw_ = static_cast<std::uint16_t>((counts % 8192 + 8192) % 8192);
            counter_.Update(raw_);
            axis_.Feedback(static_cast<double>(counter_.Counts()), feedback.velocity);
        }
    }
    online_ = last_feedback_ >= 0 && now - last_feedback_ <= kMotorOfflineTime &&
        result != LibXR::ErrorCode::NO_RESPONSE;
}
void MotorPort::Compute(double dt, bool enabled, bool hold) {
    axis_.SetHolding(hold);
    holding_ = hold;
    command_ = axis_.Update(dt, enabled || hold, online_ || hold);
    command_invalid_ = false;
}
void MotorPort::Gate(double dt, bool enabled, bool hold) {
    if (command_invalid_ || (hold && !holding_)) {
        // 取消后手动操作可能已接管目标，禁止在受力交接处发送临时零电流。
        Compute(dt, enabled, hold);
    } else {
        axis_.SetHolding(hold);
        holding_ = hold;
        if (!enabled && !hold) { command_ = 0; }
    }
}
void MotorPort::Send() {
    Motor::MotorCmd cmd{Motor::MODE_CURRENT};
    // GetLSB 为 RMMotor 私有成员，使用轴配置中的电调满量程。
    cmd.velocity = static_cast<float>(command_) / command_max_;
    motor_.Control(cmd);
}
}
