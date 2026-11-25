// LibXR RMMotor 适配：读取反馈、累计转子角并输出电调原始电流。
#pragma once
#include "motor_axis.hpp"
#include "rotor_counter.hpp"
#include "safety.hpp"
#include "RMMotor.hpp"
#include "can.hpp"
#include <atomic>

namespace dart {
// 电机方向完全由 RMMotor reverse 处理；轴坐标使用同一正方向。
class MotorPort {
public:
    // config 定义 m 或 rad 坐标，index 为 Safety 中的 0~4 设备编号。
    MotorPort(RMMotor &motor, LibXR::CAN &can, std::uint32_t feedback_id,
              AxisConfig config, int index);
    // 更新反馈并只在新 CAN 帧到来时刷新 now（s）对应的离线时刻。
    void Update(double now, Safety &safety);
    // 每 dt 秒计算电调原始电流；保持受力滑块时保留最后的反馈基准。
    void Compute(double dt, bool enabled, bool hold = false);
    // 下发前禁止旧电流；新进入保持时立即计算保持输出，其他轴等待下一拍。
    void Gate(double dt, bool enabled, bool hold = false);
    // 取消流程时清本拍待发送电流。
    void ClearCommand() { command_ = 0; command_invalid_ = true; }
    // 堵转保护覆盖本拍待发送的电流。
    void SetCommand(int command) { command_ = command; }
    // 每拍所有电机都要发送，包括零电流。
    void Send();
    // 查询轴控制器、电流指令原始值和 100 ms 在线状态。
    const MotorAxis &Axis() const { return axis_; }
    MotorAxis &Axis() { return axis_; }
    int Current() const { return command_; }
    bool Online() const { return online_; }
    // 最近有效反馈的单圈编码器计数，范围 0~8191，方向同 RMMotor 反馈。
    std::uint16_t Raw() const { return raw_; }
    // 最近有效反馈的转子力矩，单位 N·m。
    double FeedbackTorque() const { return feedback_torque_; }
    // 最近有效反馈的转子转速，单位 rpm。
    double FeedbackRpm() const { return feedback_rpm_; }
    // 最近有效反馈的电机温度，单位 °C。
    double FeedbackTemp() const { return feedback_temp_; }
private:
    static void OnFrame(bool in_isr, MotorPort *self, const LibXR::CAN::ClassicPack &pack);
    RMMotor &motor_;
    RotorCounter counter_;
    MotorAxis axis_;
    int index_, command_max_, command_ = 0;
    bool online_ = false, holding_ = false, command_invalid_ = false;
    std::atomic<std::uint32_t> frames_{0};
    std::uint32_t seen_frames_ = 0;
    std::uint16_t raw_ = 0;
    double feedback_torque_ = 0, feedback_rpm_ = 0, feedback_temp_ = 0;
    double last_feedback_ = -1;
    LibXR::CAN::Callback callback_;
};
}
