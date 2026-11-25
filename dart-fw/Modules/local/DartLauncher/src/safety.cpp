// 离线阈值为电机和遥控器 100 ms、裁判系统 1 s。
#include "safety.hpp"
#include <cmath>

namespace dart {
namespace {
SafetyFault FireCommonBlocker(const SafetyState &s) {
    if (s.fault != SafetyFault::None) { return s.fault; }
    if (s.remote_required && !s.remote_online) { return SafetyFault::RemoteOffline; }
    if (!s.motors_online) { return SafetyFault::MotorOffline; }
    if (!s.all_homed) { return SafetyFault::NotHomed; }
    if (s.latch_state != LatchState::Latched) { return SafetyFault::NotLatched; }
    if (!s.seated) { return SafetyFault::NoDarts; }
    if (!std::isfinite(s.slider) || s.slider < kFireSliderMin) { return SafetyFault::NotParked; }
    return SafetyFault::None;
}
SafetyFault FireMatchBlocker(const SafetyState &s) {
    if (s.competition && !s.referee_online) { return SafetyFault::RefereeOffline; }
    if (!s.launch_allowed) { return SafetyFault::OutOfMatch; }
    if (s.competition && !s.gate_open) { return SafetyFault::GateClosed; }
    return SafetyFault::None;
}
}
bool Safety::MotorFeedback(int index, double now) {
    if (index < 0 || index >= 5 || !std::isfinite(now) || now < 0 || now < motors_[index]) { return false; }
    motors_[index] = now;
    return true;
}
bool Safety::RemoteFeedback(double now) {
    if (!std::isfinite(now) || now < 0 || now < remote_) { return false; }
    remote_ = now;
    return true;
}
bool Safety::RefereeFeedback(double now) {
    if (!std::isfinite(now) || now < 0 || now < referee_) { return false; }
    referee_ = now;
    return true;
}
SafetyFault Safety::Check(double now, bool competition, bool remote_required) {
    if (!std::isfinite(now) || now < 0 || now < last_check_) { Latch(SafetyFault::InvalidInput); }
    last_check_ = now;
    if (latched_ != SafetyFault::None) {
        return latched_;
    }
    if (remote_required && (remote_ < 0 || now - remote_ > kRemoteOfflineTime)) { return SafetyFault::RemoteOffline; }
    for (double stamp : motors_) {
        if (stamp < 0 || now - stamp > kMotorOfflineTime) { return SafetyFault::MotorOffline; }
    }
    if (competition && (referee_ < 0 || now - referee_ > kRefereeOfflineTime)) { return SafetyFault::RefereeOffline; }
    return SafetyFault::None;
}
void Safety::Latch(SafetyFault fault) {
    if (latched_ == SafetyFault::None) {
        latched_ = fault;
    }
}
void Safety::ClearLatched() { latched_ = SafetyFault::None; }
void Safety::Reset() {
    motors_.fill(-1);
    remote_ = -1;
    referee_ = -1;
    last_check_ = -1;
    latched_ = SafetyFault::None;
}
SafetyFault Safety::FirePreparationBlocker(const SafetyState &s) {
    const SafetyFault common = FireCommonBlocker(s);
    return common == SafetyFault::None ? FireMatchBlocker(s) : common;
}
SafetyFault Safety::FireBlocker(const SafetyState &s) {
    const SafetyFault common = FireCommonBlocker(s);
    if (common != SafetyFault::None) { return common; }
    const bool supported = s.held && s.supported_side != 0 && s.held_side == s.supported_side &&
        std::isfinite(s.crank) && std::isfinite(s.arm) &&
        std::fabs(s.crank - (s.supported_side > 0 ? kCrankFetchRight : kCrankFetchLeft)) < kServoTolerance &&
        std::fabs(s.arm - (s.supported_side > 0 ? kFetchRight : kFetchLeft)) < kServoTolerance;
    if (!supported && (!std::isfinite(s.crank) || std::fabs(s.crank) >= Radians(2))) { return SafetyFault::CrankNotRetracted; }
    if (!supported && (!std::isfinite(s.arm) || (s.held && std::fabs(s.arm) < kStandby - Radians(1)))) { return SafetyFault::ArmInPath; }
    if (!s.aimed) { return SafetyFault::NotAimed; }
    return FireMatchBlocker(s);
}
bool Safety::SliderMustHold(const SafetyState &s) {
    return s.slider_homed && s.latch_state != LatchState::Latched &&
        std::isfinite(s.slider) && s.slider < kSliderPark - 0.01;
}
bool Safety::LockOpenAllowed(const SafetyState &s) {
    return s.latch_state == LatchState::Released || s.at_bottom;
}
const char *Safety::Message(SafetyFault fault) {
    switch (fault) {
    case SafetyFault::None: return "无故障";
    case SafetyFault::RemoteOffline: return "遥控器离线";
    case SafetyFault::MotorOffline: return "电机离线";
    case SafetyFault::RefereeOffline: return "裁判系统离线";
    case SafetyFault::NotHomed: return "轴未回零";
    case SafetyFault::NotLatched: return "发射机构没有锁止";
    case SafetyFault::NotParked: return "3508 滑块没有回到顶部";
    case SafetyFault::CrankNotRetracted: return "推杆没有收起";
    case SafetyFault::ArmInPath: return "吸着的镖挡住发射路径";
    case SafetyFault::NotAimed: return "yaw 或 pitch 没到位";
    case SafetyFault::GateClosed: return "闸门没有打开";
    case SafetyFault::SliderSync: return "左右滑块不同步";
    case SafetyFault::LatchFailed: return "发射机构没有锁住";
    case SafetyFault::ProbeFailed: return "探测不出发射机构状态";
    case SafetyFault::NoDarts: return "没有飞镖了";
    case SafetyFault::Timeout: return "步骤超时";
    case SafetyFault::QueueOverflow: return "流程队列溢出";
    case SafetyFault::InvalidInput: return "输入无效";
    case SafetyFault::PressBlocked: return "下压被挡住";
    case SafetyFault::NoBottom: return "压到最低也没碰到底";
    case SafetyFault::HomeOverrun: return "回零走完全程也没碰到头";
    case SafetyFault::OutOfMatch: return "当前比赛阶段不许发射";
    case SafetyFault::ReleaseInterrupted: return "放镖途中无力过，磁铁可能重新吸住镖";
    }
    return "输入无效";
}
}
