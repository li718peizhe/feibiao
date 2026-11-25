// 左右滑块共用目标与限速，反馈坐标均向上为正。
#pragma once
#include "dart_params.hpp"

namespace dart {
// 镜像方向由电机适配层处理，本层比较相对基准的物理位移差 m。
class SliderPair {
public:
    // 设置两轴共同 m 目标，软限位夹紧；故障或非法值不接受。
    bool SetTarget(double target);
    // 从左右平均位置，以 speed m/s 向目标限速移动。
    bool RampTo(double target, double speed, double left, double right);
    // 每 dt 秒更新目标并监视同步差；故障锁存到 Rebase。
    bool Update(double left, double right, double dt = 0.001);
    // 以当前左右位置 m 的差重设基准并清同步故障；非法值返回 false。
    bool Rebase(double left, double right);
    // 查询共同目标 m 和同步故障。
    double Target() const { return target_; }
    bool Fault() const { return fault_; }
private:
    double target_ = 0, end_ = 0, speed_ = 0, baseline_ = 0;
    bool fault_ = false;
};
}
