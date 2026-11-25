// 流程执行器接口：坐标向上为正，摆臂右侧为正；不引用硬件框架。
#pragma once
#include "safety.hpp"

namespace dart {
// 实现层负责 PID、左右镜像、舵机实际到位与锁止推断。
class LauncherIo {
public:
    virtual ~LauncherIo() = default;
    // 设置滑块位置 m，speed 为 m/s，0 表示按 kSliderMoveSpeed 限速。
    virtual void SetSlider(double position, double speed = 0) = 0;
    // 位移 m 和速度 m/s 同时满足到位条件。
    virtual bool SliderAt(double position, double tolerance, double velocity_tolerance) const = 0;
    // 滑块电流绝对值最大值，电调原始单位。
    virtual double SliderCurrent() const = 0;
    // yaw 目标 rad 和 pitch 目标 m。
    virtual void SetYaw(double angle) = 0;
    virtual void SetPitch(double position) = 0;
    // yaw 与 pitch 的位置和速度满足仿真阈值。
    virtual bool Aimed() const = 0;
    // 摆臂、推杆、锁止角 rad；到位同时要求目标限速结束。
    virtual void SetArm(double angle) = 0;
    virtual bool ArmAt(double angle) const = 0;
    // speed 为推杆机构角 rad/s，0 表示按 kServoSpeed。
    virtual void SetCrank(double angle, double speed = 0) = 0;
    virtual bool CrankAt(double angle) const = 0;
    virtual void SetLock(double angle) = 0;
    virtual bool LockAt(double angle) const = 0;
    // 记录发射机构的未知、已锁住或已放开状态。
    virtual void RecordLatched(LatchState state) = 0;
    // true 通电消磁，false 断电吸持；实现层限制通电时长，无力或故障时断电吸持。
    virtual void SetMagnet(bool enabled) = 0;
    // 当前物理反馈快照，库存由流程补充。
    virtual SafetyState State() const = 0;
    // 清零其他电机；hold_slider 为真时滑块保持当前位置。
    virtual void StopMotors(bool hold_slider) = 0;
};
}
