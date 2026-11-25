// 与 motors.py 一致的位置式 PID 和转子计数串级控制。
#pragma once
#include "dart_params.hpp"

namespace dart {
// 积分和微分以控制拍计算，不乘除周期。
class Pid {
public:
    explicit Pid(PidGains gains);
    // 计算同单位的目标与反馈，输出单位由增益决定。
    double Calc(double reference, double feedback);
    // 清空积分、前次误差和输出。
    void Reset();
    // 将积分夹在 [low, high]，并清除前次误差；非法范围清空积分。
    void LimitIntegral(double low, double high);
    // 按外部限流前后的差值修正积分，避免继续积累被限掉的电流。
    void TrackLimitedOutput(int requested, int actual);
    // 积分按比例 fraction（0~1）向 0 回落。
    void BleedIntegral(double fraction);
    // 当前积分，单位同输出。
    double Integral() const { return integral_; }
private:
    PidGains gains_;
    double integral_ = 0, last_error_ = 0;
};
// 位置环输出转子 rpm，速度环输出电调原始电流值。
class Cascade {
public:
    Cascade(PidGains position, PidGains speed, int command_max);
    // 目标和反馈为连续转子计数，速度 rpm，前馈为电调原始值。
    int Update(double target_counts, double counts, double rpm, double feedforward = 0);
    // 同时清空两个环。
    void Reset();
    // 清位置环，并将速度环积分夹在 [low, high]（电流原始值）。
    void ResetForHold(double low, double high);
    // 外部限流后同步速度环积分。
    void TrackLimitedOutput(int requested, int actual);
    // 速度环积分按比例向 0 回落；SpeedIntegral 返回当前积分（电流原始值）。
    void BleedSpeedIntegral(double fraction);
    double SpeedIntegral() const { return speed_.Integral(); }
    // 只算位置环：输入连续转子计数，输出转子 rpm 速度目标；和 Speed 搭配，中间可以叠加点动的速度前馈并限速。
    double Position(double target_counts, double counts);
    // 速度环，输入转子 rpm，输出原始电流值。
    int Speed(double reference_rpm, double rpm, int current_limit);
private:
    Pid position_, speed_;
    int command_max_;
};
}
