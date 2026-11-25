// 比赛流程（阶段 3c）：按裁判系统的比赛阶段和闸门状态接管遥控器，自动回零、上膛、发射、退膛。
// 纯逻辑，不引用 LibXR；结果写进 ModeCommand，适配层按手动手势同样的前提条件执行。
#pragma once
#include "dart_params.hpp"
#include "mode_logic.hpp"
#include "safety.hpp"

namespace dart {
// 机构当前状态，适配层每次调用前填一份。没装的机构按“已回零”算，免得挡住后面的步骤。
struct MatchMechanism {
    bool slider_homed = false, aim_homed = false, all_homed = false, homing = false;
    bool motors_online = false, busy = false, decocking = false, cocking = false, seated = false;
    LatchState latch = LatchState::Unknown;
    SafetyFault fault = SafetyFault::None;
    int darts = 0; // 库存剩余
};
// 没接过裁判系统（competition 为假）时什么都不改，保持调试行为。
class MatchFlow {
public:
    // 比赛中和结算中遥控器可以不在线：比赛流程不依赖遥控器。
    static bool RemoteOptional(bool competition, const RefereeInput &referee);
    // 只有没接过裁判系统，或者阶段是“未开始”“比赛中”才允许上膛和发射（R50）。
    static bool LaunchAllowed(bool competition, const RefereeInput &referee);
    // 遥控器不在线时可以自动复位的故障：卡在上膛或发射流程里的、会自己消失的。
    // 左右滑块不同步、输入无效、回零走完全程、没有飞镖这类要人看过再说，不自动复位。
    static bool Recoverable(SafetyFault fault);
    // 每个控制周期（10 ms）在 ModeLogic::Update 之后调用一次，直接改 command；dt 为 s。
    void Update(ModeCommand &command, const RemoteInput &remote, const RefereeInput &referee,
                const MatchMechanism &mechanism, bool competition, double dt);
    // 遥控器急停是否锁存：从开赛到下一场开始前，右拨杆这一拍拨到下才算（开赛时本来就在下、遥控器断线重连时还在下，都不算），
    // 故障复位手势（下停 1 s 再拨回中）或下一场开始（未开始、准备阶段）才解除；裁判系统掉线、阶段变未知都不会把它清掉。
    bool EmergencyStop() const { return estop_; }
    // 这一场已经自动复位的次数。
    int Retries() const { return retries_; }
    bool Overriding() const { return overriding_; }
    // 适配层在 StartHome() 被拒时调用，立即终止本次普通自动启动归零准备，不重试、不触发取消、不进发射。
    void RejectAutoHome();
private:
    enum class AutoHomeStage : std::uint8_t { Idle, Pending, WaitAim, WaitSlider };
    // 普通自动启动一次发射前的自动归零协调：先瞄准后滑块，全回零后发一次 salvo。
    void AutoHomeStart(ModeCommand &command, const RemoteInput &remote, const MatchMechanism &mechanism);
    void CancelAutoHome(ModeCommand &command);
    // 比赛中和结算退膛时接管：遥控器只剩急停和故障复位，其余输入一律不理。
    void Takeover(ModeCommand &command);
    // 当前阶段不许上膛、发射：丢掉请求并标记，已经锁住时的“上膛”手势是退膛，照样放行。
    static void RefuseLaunch(ModeCommand &command, const MatchMechanism &mechanism);
    // 有故障时的处理，返回是否有故障：遥控器在线由指挥手复位，不在线才自动复位可恢复的故障，次数有限。
    bool Recover(ModeCommand &command, const RemoteInput &remote, const RefereeInput &referee,
                 const MatchMechanism &mechanism, double dt);
    void InMatch(ModeCommand &command, const RemoteInput &remote, const RefereeInput &referee,
                 const MatchMechanism &mechanism, double dt);
    void Settlement(ModeCommand &command, const RemoteInput &remote, const RefereeInput &referee,
                    const MatchMechanism &mechanism, double dt);
    // 到点才发：被拒绝的请求隔 kMatchRequestInterval 再发，免得每个周期都刷。
    static bool Due(double &wait);
    // 还有轴没回零或正在回零：没回零的自动回零，返回 true 表示这时不发别的请求（上膛、退膛、发射都要全部回零）。
    bool NeedsHoming(ModeCommand &command, const MatchMechanism &mechanism);
    bool estop_ = false, overriding_ = false;
    // ever_online_：遥控器启动以来上过线；session_：这一场开过赛（比赛中出现过，到下一场开始前一直算）。
    bool ever_online_ = false, session_ = false;
    Switch previous_right_ = Switch::Down;
    int retries_ = 0;
    double retry_wait_ = 0;
    double home_wait_ = 0, cock_wait_ = 0, decock_wait_ = 0, fire_wait_ = 0;
    AutoHomeStage auto_home_stage_ = AutoHomeStage::Idle;
    bool auto_home_started_ = false;
};
}
