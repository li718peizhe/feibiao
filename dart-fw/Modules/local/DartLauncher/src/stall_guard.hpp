// 摆臂堵转保护，不引用硬件框架。
#pragma once

namespace dart {
class StallGuard {
public:
    // limit 为电调原始电流，rpm 为不转阈值，time 为触发与解除所需的持续时间（s）。
    StallGuard(int limit, double rpm, double time);
    // 每拍输入电流请求、反馈转速和时间步，返回允许下发的电流。
    int Update(int request, double rpm, double dt);
    // 换限值（例如按电机温度降额），下一拍起触发、夹紧和解除都按新值。
    void SetLimit(int limit) { limit_ = limit; }
    int Limit() const { return limit_; }
    bool Tripped() const { return tripped_; }
    // 回到未触发、重新计时；流程每开始一段摆臂移动时用。
    void Reset();
private:
    int limit_;
    double rpm_, time_, elapsed_ = 0;
    bool tripped_ = false, recovering_ = false;
};
}
