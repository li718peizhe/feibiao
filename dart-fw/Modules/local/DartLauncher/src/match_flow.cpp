// 比赛中遥控器只剩急停和故障复位；准备、自检、倒计时、结算阶段不许上膛和发射（R50）。
#include "match_flow.hpp"
#include <algorithm>

namespace dart {
bool MatchFlow::RemoteOptional(bool competition, const RefereeInput &f) {
    return competition && f.online && (f.stage == Stage::InMatch || f.stage == Stage::Settlement);
}

bool MatchFlow::LaunchAllowed(bool competition, const RefereeInput &f) {
    return !competition || (f.online && (f.stage == Stage::NotStarted || f.stage == Stage::InMatch));
}

bool MatchFlow::Recoverable(SafetyFault fault) {
    switch (fault) {
    case SafetyFault::Timeout:
    case SafetyFault::ProbeFailed:
    case SafetyFault::LatchFailed:
    case SafetyFault::PressBlocked:
    case SafetyFault::NoBottom:
    case SafetyFault::GateClosed:
    case SafetyFault::NotAimed:
    case SafetyFault::NotParked:
    case SafetyFault::RemoteOffline:
    case SafetyFault::MotorOffline:
    case SafetyFault::RefereeOffline:
        return true;
    default:
        return false;
    }
}

bool MatchFlow::Due(double &wait) {
    if (wait > 0) { return false; }
    wait = kMatchRequestInterval;
    return true;
}

void MatchFlow::Takeover(ModeCommand &c) {
    c.match_override = true;
    c.mode = estop_ || !kAutoEnabled ? Mode::Powerless : Mode::Automatic;
    c.yaw_rate = c.pitch_rate = c.slider_rate = c.lock_rate = c.arm_rate = c.crank_rate = 0;
    c.home_aim = c.home_slider = c.cock_request = c.fire = c.salvo = c.magnet_toggle = c.fetch_request = false;
    // ModeLogic 在遥控器不在线时会要求停流程，比赛中不能听它的；只有急停才停。
    c.stop_sequence = estop_;
    // 只在接管的第一拍通知适配层停掉遥控器留下的手动点动。ModeLogic 自己的 leave_manual（遥控器掉线、拨杆切换引起的）
    // 不能带进来：适配层收到它会 StopMotors，把正在回零的轴打断。
    c.leave_manual = !overriding_;
}

bool MatchFlow::NeedsHoming(ModeCommand &c, const MatchMechanism &m) {
    if (kMatchAutoHome && !m.homing && (!m.slider_homed || !m.aim_homed)) {
        if (Due(home_wait_)) {
            c.auto_home_slider = !m.slider_homed;
            c.auto_home_aim = !m.aim_homed;
        }
        return true;
    }
    return !m.all_homed || m.homing;
}

void MatchFlow::RefuseLaunch(ModeCommand &c, const MatchMechanism &m) {
    if (c.fire || c.salvo) {
        c.fire = c.salvo = false;
        c.fire_refused = true;
    }
    if (c.cock_request && m.latch != LatchState::Latched) {
        c.cock_request = false;
        c.cock_refused = true;
    }
}

bool MatchFlow::Recover(ModeCommand &c, const RemoteInput &r, const RefereeInput &f,
                        const MatchMechanism &m, double dt) {
    if (m.fault == SafetyFault::None) {
        retry_wait_ = 0;
        return false;
    }
    if (!r.online && f.online && m.motors_online && retries_ < kMatchAutoRetries &&
        Recoverable(m.fault)) {
        retry_wait_ += dt;
        if (retry_wait_ >= kMatchRetryDelay) {
            c.auto_reset = true;
            ++retries_;
            retry_wait_ = 0;
        }
    } else {
        retry_wait_ = 0;
    }
    return true;
}

void MatchFlow::InMatch(ModeCommand &c, const RemoteInput &r, const RefereeInput &f,
                        const MatchMechanism &m, double dt) {
    Takeover(c);
    if (estop_) {
        retry_wait_ = 0;
        return;
    }
    if (Recover(c, r, f, m, dt) || m.busy) { return; }
    // 比赛开始时还有轴没回零：自动回零。准备阶段本来就该用遥控器回好，这里只是兜底。
    if (NeedsHoming(c, m)) { return; }
    const bool cocked = m.latch == LatchState::Latched;
    if (cocked && f.gate_open && m.seated && m.darts > 0) {
        // 闸门全开就发，瞄准到位由发射流程里的“瞄准”一步等；选手端的发射指令只是备用，这里不用等它。
        if (Due(fire_wait_)) { c.fire = true; }
    } else if (cocked) {
        // 快结束了还上着膛、闸门又没开：退膛，保证进结算前退完。
        if (f.stage_remaining <= kMatchDecockLead && Due(decock_wait_)) { c.auto_decock = true; }
    } else if (m.darts > 0 && f.stage_remaining > kMatchCockCutoff) {
        if (Due(cock_wait_)) { c.auto_cock = true; }
    }
}

void MatchFlow::Settlement(ModeCommand &c, const RemoteInput &r, const RefereeInput &f,
                           const MatchMechanism &m, double dt) {
    RefuseLaunch(c, m);
    // 结算时只留退膛和正在跑的上膛，其余流程取消。上膛不取消：取消会停在半路，滑块托着没锁住的发射机构，
    // 之后没有任何自动办法处理；让它做完（多半已经快完了），锁住以后再退膛。
    const bool cancel = m.busy && !m.decocking && !m.cocking;
    const bool work = m.latch == LatchState::Latched || m.decocking || m.cocking;
    if (!work) {
        retry_wait_ = 0;
        if (cancel) { c.stop_sequence = true; }
        return; // 没有要退的膛：遥控器照常
    }
    Takeover(c);
    // Takeover 把 stop_sequence 改成只看急停，取消要放在它后面。
    if (cancel) { c.stop_sequence = true; }
    if (estop_) {
        retry_wait_ = 0;
        return;
    }
    if (Recover(c, r, f, m, dt) || m.busy) { return; }
    // 退膛要全部回零；中途丢了回零（电机掉线再恢复）先回零。
    if (NeedsHoming(c, m)) { return; }
    if (m.latch == LatchState::Latched && Due(decock_wait_)) { c.auto_decock = true; }
}

void MatchFlow::RejectAutoHome() {
    auto_home_stage_ = AutoHomeStage::Idle;
    auto_home_started_ = false;
}

void MatchFlow::CancelAutoHome(ModeCommand &c) {
    if (auto_home_stage_ == AutoHomeStage::Idle) {
        return;
    }
    if (auto_home_started_) {
        c.auto_home_cancel = true;
    }
    auto_home_stage_ = AutoHomeStage::Idle;
    auto_home_started_ = false;
}

void MatchFlow::AutoHomeStart(ModeCommand &c, const RemoteInput &r, const MatchMechanism &m) {
    if (auto_home_stage_ == AutoHomeStage::Idle) {
        if (!c.salvo || c.mode != Mode::Automatic || !r.online || c.fault_reset ||
            c.stop_sequence || m.busy || m.fault != SafetyFault::None) {
            return;
        }
        if (m.aim_homed && m.slider_homed && m.all_homed && !m.homing) {
            return;
        }
        c.salvo = false;
        c.fire = false;
        auto_home_stage_ = AutoHomeStage::Pending;
        auto_home_started_ = false;
    } else {
        c.salvo = false;
        c.fire = false;
        if (c.mode != Mode::Automatic || !r.online || c.fault_reset || c.stop_sequence ||
            m.fault != SafetyFault::None || m.busy) {
            CancelAutoHome(c);
            return;
        }
    }
    if (auto_home_stage_ == AutoHomeStage::WaitAim) {
        if (!m.aim_homed || m.homing) {
            return;
        }
        auto_home_stage_ = AutoHomeStage::Pending;
    }
    if (auto_home_stage_ == AutoHomeStage::WaitSlider) {
        if (!m.slider_homed || m.homing) {
            return;
        }
        auto_home_stage_ = AutoHomeStage::Pending;
    }
    if (m.homing) {
        return;
    }
    if (!m.aim_homed) {
        c.auto_home_aim = true;
        auto_home_started_ = true;
        auto_home_stage_ = AutoHomeStage::WaitAim;
        return;
    }
    if (!m.slider_homed) {
        c.auto_home_slider = true;
        auto_home_started_ = true;
        auto_home_stage_ = AutoHomeStage::WaitSlider;
        return;
    }
    if (m.all_homed) {
        c.salvo = true;
        auto_home_stage_ = AutoHomeStage::Idle;
        auto_home_started_ = false;
    }
}

void MatchFlow::Update(ModeCommand &c, const RemoteInput &r, const RefereeInput &f,
                       const MatchMechanism &m, bool competition, double dt) {
    // 急停只认开赛以后拨到下：右拨杆这一拍从别的位置到了下才算；开赛时本来就停在下不算。遥控器断线重连时拨杆
    // 还在下也不算（受干扰断个 100 ms 就急停，比赛流程就成了依赖遥控器）；遥控器断线期间拨过杆，重连后位置变了，照样算。
    // 只有遥控器启动以来第一次上线就在下，当作急停：多半是中途现开遥控器想拦住它。
    // RemotePort 在离线时保留最后一帧的拨杆位置，所以 previous_right_ 离线期间也是最后知道的位置。
    const bool down_edge = r.online && r.right == Switch::Down &&
        (!ever_online_ || previous_right_ != Switch::Down);
    if (r.online) { ever_online_ = true; }
    previous_right_ = r.right;
    for (double *wait : {&home_wait_, &cock_wait_, &decock_wait_, &fire_wait_}) {
        *wait = std::max(0.0, *wait - dt);
    }
    if (!competition) {
        // 从没收到过裁判系统：调试行为，按普通自动启动协调归零。
        estop_ = false;
        session_ = false;
        overriding_ = false;
        AutoHomeStart(c, r, m);
        return;
    }
    const Stage stage = f.online ? f.stage : Stage::Unknown;
    const bool match = stage == Stage::InMatch, settlement = stage == Stage::Settlement;
    // 一场从“比赛中”出现算起，到下一场开始（未开始、准备）为止。这期间急停锁存只由故障复位手势解除，
    // 裁判系统掉线、阶段变成未知都不能清掉它，不然链路一恢复，流程就会在拨杆还按在下的时候接着走；
    // 阶段未知时拨到下的动作也要记下来。
    if (match) {
        session_ = true;
    } else if (stage == Stage::NotStarted || stage == Stage::Preparing) {
        session_ = false;
    }
    if (session_) {
        if (down_edge) { estop_ = true; }
        if (c.fault_reset) { estop_ = false; }
    } else {
        estop_ = false;
    }
    if (stage == Stage::NotStarted || stage == Stage::Preparing) { retries_ = 0; }
    c.estop = estop_;
    if (stage == Stage::NotStarted) {
        AutoHomeStart(c, r, m);
    } else {
        CancelAutoHome(c);
    }
    // overriding_ 在这一拍结束前一直是“上一拍是否接管”，Takeover 靠它只在第一拍发 leave_manual。
    if (match) {
        InMatch(c, r, f, m, dt);
    } else if (settlement) {
        Settlement(c, r, f, m, dt);
    } else if (stage != Stage::NotStarted) {
        // 准备、自检、倒计时和阶段未知：遥控器照常，只是不许上膛和发射；发现上着膛只报警，不自动动。
        RefuseLaunch(c, m);
        if (m.latch == LatchState::Latched) { c.cocked_warning = true; }
        if (m.busy && !m.decocking) { c.stop_sequence = true; }
    }
    overriding_ = c.match_override;
}
}
