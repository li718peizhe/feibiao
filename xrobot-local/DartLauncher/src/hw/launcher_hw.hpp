// LibXR 机构实现：把五个电机、三个 PWM 和裁判/遥控输入接到 LauncherIo。
#pragma once
#include "launcher_io.hpp"
#include "launch_sequence.hpp"
#include "match_flow.hpp"
#include "motor_port.hpp"
#include "servo_port.hpp"
#include "magnet_port.hpp"
#include "hx711_port.hpp"
#include "tension_port.hpp"
#include "magnet_guard.hpp"
#include "remote_port.hpp"
#include "referee_port.hpp"
#include "slider_pair.hpp"
#include "stall_guard.hpp"
#include "uart.hpp"
#include "pwm.hpp"
#include "can.hpp"
#include "thread.hpp"
#include "yaw_geometry.hpp"
#include <atomic>

namespace dart {
class LauncherHardware final : public LauncherIo {
public:
    // 电机引用和 CAN/反馈 ID 必须匹配 YAML；PWM 分别为锁止、推杆、电磁铁。
    LauncherHardware(RMMotor &slider_left, RMMotor &slider_right, RMMotor &pitch,
                     RMMotor &yaw, RMMotor &arm, LibXR::CAN &can_slider,
                     LibXR::CAN &can_pitch, LibXR::CAN &can_yaw, LibXR::CAN &can_arm,
                     std::uint32_t slider_left_id, std::uint32_t slider_right_id,
                     std::uint32_t pitch_id, std::uint32_t yaw_id, std::uint32_t arm_id,
                     Referee &referee, LibXR::UART &dbus, LibXR::PWM &lock_pwm,
                     LibXR::PWM &crank_pwm, LibXR::PWM &magnet_pwm,
                     LibXR::UART &tension_uart);
    // 控制线程每 1 ms 调用，dt 使用 s，now 为单调启动时间 s。
    void Tick(double now, double dt);
    // 由控制线程在每拍 Tick 前后计时后调用，统计平均/最长耗时和迟唤醒次数。
    void RecordControlTiming(std::uint32_t tick_us, std::uint32_t wake_gap_ms);
    // 每秒打印一份控制线程快照，不读取并发修改的轴对象。
    void OnMonitor();
    // 设置滑块目标 m 和限速 m/s，speed=0 使用 kSliderMoveSpeed。
    void SetSlider(double position, double speed = 0) override;
    // 两侧实际位置误差 m 和速度 m/s 均满足阈值才到位。
    bool SliderAt(double position, double tolerance, double velocity_tolerance) const override;
    // 返回本拍两侧电流指令绝对值较大者，单位为电调原始值。
    double SliderCurrent() const override;
    // 设置 yaw 角 rad，经几何换算为滑台目标 m。
    void SetYaw(double angle) override;
    // 设置 pitch 滑台目标 m。
    void SetPitch(double position) override;
    // 查询 yaw 和 pitch 的实际位置、速度是否满足到位阈值。
    bool Aimed() const override;
    // 流程摆臂移动：像推摇杆一样以 kArmAutoSpeed 走到目标 rad，到了按松手保持；堵转保护重新计时。
    void SetArm(double angle) override;
    // 查询摆臂已回零、MoveTo 走完、实际角在目标附近且几乎不动。
    bool ArmAt(double angle) const override;
    // 设置推杆机构角 rad，speed 为 rad/s（0 按 kServoSpeed）；推杆未标定时不改变输出。
    void SetCrank(double angle, double speed = 0) override;
    // 查询推杆估计角 rad 是否到位；推杆未标定时总为 false。
    bool CrankAt(double angle) const override;
    // 设置锁止机构角 rad，同时记录开锁或合上的即时状态。
    void SetLock(double angle) override;
    // 查询锁止估计角 rad 是否到位。
    bool LockAt(double angle) const override;
    // 由探测或锁止确认记录机构状态，无单位。
    void RecordLatched(LatchState state) override { state_.latch_state = state; }
    // 记下电磁铁通断命令，本拍末尾经限时判断后才写继电器；不改变库存推断。
    void SetMagnet(bool enabled) override;
    // 返回最近控制拍安全快照，位置 m、角度 rad。
    SafetyState State() const override;
    // 停止轴输出；hold_slider=true 时将滑块停在当前位置保持。
    void StopMotors(bool hold_slider) override;
private:
    enum class HomeStartResult : std::uint8_t { Started, NotInstalled, BlockedBySliderHold };
    void RefreshState(double now);
    HomeStartResult StartHome(ManualGroup group, double now);
    // 装弹组拨轮往上 2 s：检查推杆标定、摆臂零点、温度和上一发放没放，再取下一发（左右轮流）。
    void StartFetchTest(double now);
    // 未安装机构的回零或摇杆请求共用每秒一次的提示，now 单位 s。
    void WarnNotInstalled(double now);
    // 上膛（decock 为假）或退膛的共同前提：锁止舵机标定过、上膛已开放、全部回零；上膛还要当前阶段允许。
    void Cock(bool decock);
    void Manual(const ModeCommand &command, double dt, double now);
    static AxisConfig SliderConfig();
    static AxisConfig PitchConfig();
    static AxisConfig YawConfig();
    static AxisConfig ArmConfig();
    SafetyState state_{};
    Safety safety_{};
    MotorPort slider_left_, slider_right_, pitch_, yaw_, arm_;
    StallGuard arm_stall_{kArmStallCurrent, kArmStallRpm, kArmStallTime};
    SliderPair slider_pair_;
    ServoPort lock_, crank_;
    MagnetPort magnet_;
    Hx711Port tension_right_;
    TensionPort tension_rs485_;
    std::uint32_t printed_tension_left_ = 0, printed_tension_right_ = 0;
    RemotePort remote_;
    RefereePort referee_;
    DartInventory inventory_;
    LaunchSequence sequence_;
    ModeLogic mode_;
    MatchFlow match_;
    ModeCommand command_{};
    std::uint32_t ticks_ = 0;
    std::uint32_t remote_sequence_ = 0;
    double referee_time_ = -1, mode_dt_ = 0;
    // 启动以来收到过裁判数据就持续启用比赛检查，掉线和故障复位不清除。
    bool referee_seen_ = false;
    bool stop_hold_ = false, previous_slider_homed_ = false;
    // 锁止舵机未标定时的原始脉宽模式状态。
    bool lock_raw_enabled_ = false;
    double lock_raw_pulse_ = 0;
    // 推杆舵机已装、未标定时的原始脉宽模式状态，和锁止一样。
    bool crank_raw_enabled_ = false;
    double crank_raw_pulse_ = 0;
    // 电磁铁通电限时；SetMagnet 只改它的命令。
    MagnetGuard magnet_guard_;
    // 最近写给继电器的通断（限时以后的命令），只在 Tick 末尾改；不代表实际电压或吸持反馈。
    bool magnet_on_ = false;
    // 未安装机构的回零和点动共用提示计时，单位 s。
    double mechanism_warn_time_ = -1;
    // 比赛流程拒绝上膛、拒绝发射、准备阶段发现上着膛的限频提示时刻，单位 s。
    std::array<double, 3> match_warn_time_{{-10, -10, -10}};
    // 控制线程复制的标定快照；位置 m/rad、机构角 rad、输出脉宽 µs。
    struct MonitorFeedback {
        RemoteInput remote{};
        std::array<bool, 5> online{}, homed{};
        std::array<double, 5> position{};
        double slider_current = 0;
        int pitch_current = 0;
        double pitch_feedback_torque = 0, pitch_feedback_rpm = 0;
        // 摆臂 6020 的电流指令、回报的转矩 N·m、转速 rpm、温度 °C 和堵转状态。
        int arm_current = 0;
        double arm_feedback_torque = 0, arm_feedback_rpm = 0, arm_feedback_temp = 0;
        bool arm_stall = false;
        std::uint32_t arm_bus_frames = 0;
        std::uint16_t arm_raw = 0;
        double lock_angle = 0, crank_angle = 0, lock_pulse = 0, crank_pulse = 0;
        bool lock_raw_mode = !kLockServoCalibrated;
        bool crank_raw_mode = kCrankInstalled && !kCrankServoCalibrated;
        bool magnet_on = false;
        // 遥控器收帧计数与控制线程耗时统计，每 1000 拍结算一次。
        std::uint32_t remote_ok = 0, remote_clr = 0, remote_bad = 0;
        std::uint32_t tick_avg_us = 0, tick_max_us = 0, late_wakes = 0;
        // 最近一发的发射记录，控制线程每拍拷一份。
        ShotRecord shot{};
        // 阶段 3c：裁判输入和比赛流程状态，控制线程每拍拷一份。
        RefereeInput referee{};
        bool competition = false, match_override = false, estop = false, remote_required = true;
        int match_retries = 0;
    };
    LibXR::Mutex monitor_mutex_;
    MonitorFeedback monitor_feedback_{};
    // 只由 OnMonitor 读写：上一次打印过的发射记录编号。
    std::uint32_t printed_shot_ = 0;
    SafetyState monitor_state_{};
    Mode monitor_mode_ = Mode::Powerless;
    const char *monitor_step_ = "空闲";
    int monitor_inventory_ = 4;
    RefereeInput referee_input_{};
    RemoteInput remote_input_{};
    // 耗时累计只由控制线程写，快照复制时读取；每 1000 拍结算并清零。
    std::uint64_t tick_us_sum_ = 0;
    std::uint32_t tick_us_max_ = 0, late_wakes_ = 0, timing_ticks_ = 0;
    // 上一完整 1000 拍窗口的结果；首个窗口结束前保持 0。
    std::uint32_t last_tick_avg_us_ = 0, last_tick_max_us_ = 0, last_late_wakes_ = 0;
    // 摆臂那条 CAN 收到的所有标准帧计数，由 CAN 回调递增，控制线程拷进监控快照。
    std::atomic<std::uint32_t> arm_bus_frames_{0};
    LibXR::CAN::Callback arm_bus_callback_;
};
}
