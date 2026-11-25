// 在线超时、锁存故障和解锁前的安全条件。
#pragma once
#include "dart_params.hpp"
#include <array>
#include <cstdint>

namespace dart {
enum class SafetyFault : std::uint8_t {
    None, RemoteOffline, MotorOffline, RefereeOffline, NotHomed, NotLatched,
    NotParked, CrankNotRetracted, ArmInPath, NotAimed, GateClosed,
    SliderSync, LatchFailed, NoDarts, Timeout, QueueOverflow, InvalidInput, ProbeFailed,
    PressBlocked, NoBottom, HomeOverrun, OutOfMatch, ReleaseInterrupted
};
enum class LatchState : std::uint8_t { Unknown, Latched, Released };
// 解锁瞬间的反馈快照；位置 m、角度 rad；库存由流程补充。
struct SafetyState {
    bool remote_online = false, motors_online = false, all_homed = false;
    bool slider_homed = false;
    bool at_bottom = false; // 这次慢压找到了底，滑块正压在底上；只由流程的快照填
    LatchState latch_state = LatchState::Unknown;
    bool held = false, seated = false, aimed = false;
    int held_side = 0, supported_side = 0;
    bool competition = false, referee_online = false, gate_open = false;
    // 阶段 3c：比赛中和结算中遥控器可以不在线（remote_required 为假）；
    // 接着裁判系统时，只有“未开始”和“比赛中”允许发射（launch_allowed），准备、自检、倒计时、结算和阶段未知都不行。
    bool remote_required = true, launch_allowed = true;
    double slider = 0, crank = 0, arm = 0;
    double pitch = 0; // pitch 位置 m，没装 pitch 时为 0；只给发射记录用，安全判断不看它
    SafetyFault fault = SafetyFault::None;
};
// 每个设备的新反馈驱动计时；调用方每拍检查并统一关断电机。
class Safety {
public:
    // 更新单个电机有效反馈时刻 s，编号 0~4。
    bool MotorFeedback(int index, double now);
    // 更新遥控器和裁判系统有效帧的单调时刻 s。
    bool RemoteFeedback(double now);
    bool RefereeFeedback(double now);
    // 按单调时刻 s 检查离线；比赛模式才要求裁判系统在线，remote_required 为假时不检查遥控器。
    SafetyFault Check(double now, bool competition, bool remote_required = true);
    // 将轴和机构故障锁存到下次显式复位。
    void Latch(SafetyFault fault);
    // 只清锁存故障，保留所有反馈时刻和单调时间基准。
    void ClearLatched();
    // 清空所有在线基准和故障，须重新接收反馈。
    void Reset();
    // 发射条件检查，返回第一项明确故障码。
    static SafetyFault FireBlocker(const SafetyState &state);
    // 发射准备前检查不依赖准备步骤调整的推杆、摆臂和瞄准状态。
    static SafetyFault FirePreparationBlocker(const SafetyState &state);
    // 滑块已回零、未确认锁住且低于 park 位 10 mm 时必须保持。
    static bool SliderMustHold(const SafetyState &state);
    // 只有发射机构已放开或本次慢压找到底且滑块正压在底上时允许开锁。
    static bool LockOpenAllowed(const SafetyState &state);
    // 故障码对应中文原因。
    static const char *Message(SafetyFault fault);
private:
    std::array<double, 5> motors_{{-1, -1, -1, -1, -1}};
    double remote_ = -1, referee_ = -1, last_check_ = -1;
    SafetyFault latched_ = SafetyFault::None;
};
}
