// 1 kHz 反馈与轴控制，每 10 拍执行遥控映射和流程；故障按受力状态保持滑块。
#include "launcher_hw.hpp"
#include "dart_params.hpp"
#include "logger.hpp"
#include "stdio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace dart {
namespace {
// 顺序与 Safety 的反馈槽位一致；滑块开关同时控制左右两台。
constexpr std::array<bool, 5> kMotorInstalled{
    kSliderInstalled, kSliderInstalled, kPitchInstalled, kYawInstalled, kArmInstalled};
constexpr double kM3508RotorTorqueConstant = 0.0156224; // N·m/A，转子端，和 RMMotor.hpp 里 KGetTorque() 的 M3508 值一致。
constexpr double kGm6020TorqueConstant = 0.741; // N·m/A，和 RMMotor.hpp 里 KGetTorque() 的 GM6020 值一致；±3 A 对应 ±16384。
AxisConfig Axis(double counts, double min, double max, double home, double speed,
                int direction, int current, int command, PidGains pos, PidGains vel) {
    return {counts, min, max, home, speed, kHomeRetreat, 0.001, direction, current, command, pos, vel};
}
bool Homing(const MotorAxis &axis) {
    return axis.State() == HomeState::Seeking || axis.State() == HomeState::Retreating;
}
}
AxisConfig LauncherHardware::SliderConfig() {
    // 皮带每转位移 2πr；左右镜像已由 RMMotor reverse 处理，坐标向上为正。
    // 流程找底最低压到这里，手动点动另由 kManualSliderFloor 限制。
    return Axis(kM3508Ratio * kEncoderRange / (2 * kPi * kBeltRadius),
                kPressSearchLimit, kSliderTop, kSliderTop, kSliderHomeSpeed, 1,
                kSliderCurrentCap, 16384, kSliderPos, kSliderSpeed);
}
AxisConfig LauncherHardware::PitchConfig() {
    auto config = Axis(kM3508Ratio * kEncoderRange / kPitchLead, kPitchBottom, kPitchTopLimit,
                       kPitchTop, kPitchHomeSpeed, 1, kPitchCurrentCap, 16384, kPitchPos, kPitchSpeed);
    config.home_current_fraction = kPitchHomeCurrentFraction;
    config.home_stall_time = kPitchHomeStallTime;
    config.home_seek_limit = kPitchHomeSeekLimit;
    config.hold_on_release = true;
    return config;
}
AxisConfig LauncherHardware::YawConfig() {
    auto config = Axis(kM2006Ratio * kEncoderRange / (2 * kPi), -kYawMaxSpan, kYawMaxSpan,
                       0, kYawHomeSpeed, -1, kYawCurrentCap, 10000, kYawPos, kYawSpeed);
    config.dual_end_home = true;
    config.home_timeout = kYawHomeTimeout;
    config.home_current_fraction = static_cast<double>(kYawHomeCurrent) / kYawCurrentCap;
    config.retreat = kYawBackoff;
    config.home_tolerance = kYawTolerance;
    config.home_seek_limit = kYawMaxSpan;
    config.center_speed = kYawCenterSpeed;
    config.end_margin = kYawEndMargin;
    config.min_span = kYawMinSpan;
    config.max_span = kYawMaxSpan;
    config.backoff_timeout = kYawBackoffTimeout;
    config.center_settle_time = kYawCenterSettleTime;
    config.contact_counts = kYawContactCounts;
    config.contact_rpm = kYawContactRpm;
    config.contact_time = kYawContactTime;
    config.hold_on_release = true;
    return config;
}
AxisConfig LauncherHardware::ArmConfig() {
    // GM6020 直驱，按绝对编码器建立零点；不执行堵转回零。
    auto config = Axis(kEncoderRange / (2 * kPi), kArmMin, kArmMax, kArmMin,
                       Radians(1), -1, kArmCurrentCap, 16384, kArmPos, kArmSpeedGains);
    config.retreat = Radians(1);
    config.home_tolerance = Radians(0.1);
    // 摇杆推着位置目标走，点动、松手、到软限位都不清速度环积分：清掉会让托住摆臂的电流瞬间消失，
    // 11-12 松手下掉和限位附近回摆都是这样来的。松手时电流保持连续，最多留到电机凉时的限流值。
    config.hold_on_release = true;
    config.jog_moves_target = true;
    config.jog_lead = kArmJogLead;
    config.hold_deadband = kArmHoldDeadband;
    config.hold_integral_limit = kArmBoostCurrent;
    config.approach_gain = kArmApproachGain;
    config.overspeed_rpm = kArmOverspeedRpm;
    config.overspeed_bleed_time = kArmOverspeedBleedTime;
    config.overspeed_filter_time = kArmOverspeedFilterTime;
    config.gravity_current = kArmGravityCurrent;
    config.gravity_zero = kArmGravityZero;
    config.gravity_fade = kArmGravityFade;
    config.jog_friction_current = kArmJogFrictionCurrent;
    config.jog_friction_fade = kArmJogFrictionFade;
    config.release_still_rpm = kArmStallRpm;
    config.move_creep = kArmMoveCreep;
    config.move_tolerance = kArmMoveTolerance;
    config.move_near = kArmMoveNear;
    config.move_settle_time = kArmMoveSettleTime;
    config.move_still_band = kArmMoveStillBand;
    config.move_push = kArmMovePush;
    config.move_push_time = kArmMovePushTime;
    return config;
}
LauncherHardware::LauncherHardware(RMMotor &sl, RMMotor &sr, RMMotor &pitch,
                                   RMMotor &yaw, RMMotor &arm, LibXR::CAN &cs,
                                   LibXR::CAN &cp, LibXR::CAN &cy, LibXR::CAN &ca,
                                   std::uint32_t sli, std::uint32_t sri, std::uint32_t pi,
                                   std::uint32_t yi, std::uint32_t ai, Referee &ref,
                                   LibXR::UART &dbus, LibXR::PWM &lp,
                                   LibXR::PWM &cpwm, LibXR::PWM &mp,
                                   LibXR::UART &tension_uart)
    : slider_left_(sl, cs, sli, SliderConfig(), 0),
      slider_right_(sr, cs, sri, SliderConfig(), 1),
      pitch_(pitch, cp, pi, PitchConfig(), 2), yaw_(yaw, cy, yi, YawConfig(), 3),
      arm_(arm, ca, ai, ArmConfig(), 4),
      // kLockServoCalibrated 为 false 时锁止 PWM 上电不输出信号，避免未标定的舵机猛转。
      lock_(lp, kLockClosed, kLockServoOffset, kLockServoDirection, kLockServoCalibrated),
      // 推杆装好且标定过才在上电时初始化 PWM；未标定时按 kCrankRetracted 输出是 500 µs，会让舵机顶到行程一端。
      crank_(cpwm, kCrankRetracted, kCrankServoOffset, kCrankServoDirection,
             kCrankInstalled && kCrankServoCalibrated),
      // 电磁铁按装配开关决定是否初始化 PWM。
      magnet_(mp, kMagnetInstalled),
      tension_right_(),
      tension_rs485_(tension_uart),
      remote_(dbus), referee_(ref), sequence_(*this, inventory_) {
    // 两发测试的起始位置由操作手确认：发射座和左镖座各一发。
    if constexpr (kReloadInstalled) {
        inventory_.ResetSeatedLeft();
    } else {
        inventory_.ResetManual();
    }
    // 摆臂那条 CAN 上收到的标准帧全部计数：6020 不在线时，分得清是总线上什么都没收到，还是有帧但 ID 不对。
    // LibXR 的 CAN 会把一帧交给所有匹配的订阅者，这个计数不影响电机自己的回调。
    arm_bus_callback_ = LibXR::CAN::Callback::Create(
        [](bool, LauncherHardware *self, const LibXR::CAN::ClassicPack &) {
            self->arm_bus_frames_.fetch_add(1, std::memory_order_relaxed);
        }, this);
    ca.Register(arm_bus_callback_, LibXR::CAN::Type::STANDARD,
                LibXR::CAN::FilterMode::ID_RANGE, 0x000, 0x7FF);
}

void LauncherHardware::RefreshState(double now) {
    bool any_installed = false;
    state_.motors_online = true;
    state_.all_homed = true;
    std::size_t index = 0;
    for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) {
        if (kMotorInstalled[index++]) {
            any_installed = true;
            state_.motors_online = state_.motors_online && port->Online();
            state_.all_homed = state_.all_homed && port->Axis().Homed();
            if (port->Axis().State() == HomeState::Failed) {
                safety_.Latch(port->Axis().HomeOverrun() ? SafetyFault::HomeOverrun : SafetyFault::InvalidInput);
            }
        }
    }
    // 全部电机未装时不能把空集合当成全部在线、全部回零。
    state_.motors_online = any_installed && state_.motors_online;
    state_.all_homed = any_installed && state_.all_homed;
    state_.slider_homed = kSliderInstalled && slider_left_.Axis().Homed() && slider_right_.Axis().Homed();
    if (state_.slider_homed && !previous_slider_homed_) {
        slider_pair_.Rebase(slider_left_.Axis().Position(), slider_right_.Axis().Position());
    }
    previous_slider_homed_ = state_.slider_homed;
    state_.slider = (slider_left_.Axis().Position() + slider_right_.Axis().Position()) * 0.5;
    // 推杆装了但没标定时不知道它在哪，按无效值报，发射检查会拦下“推杆没有收起”。
    state_.crank = kCrankInstalled && !kCrankServoCalibrated ?
        std::numeric_limits<double>::quiet_NaN() : crank_.Value();
    state_.arm = arm_.Axis().Position();
    state_.pitch = kPitchInstalled ? pitch_.Axis().Position() : 0;
    state_.aimed = Aimed();
    state_.held = inventory_.Held();
    state_.seated = inventory_.Seated();
    state_.referee_online = referee_input_.online;
    state_.gate_open = referee_input_.gate_open;
    state_.competition = referee_seen_;
    state_.remote_required = !MatchFlow::RemoteOptional(referee_seen_, referee_input_);
    state_.launch_allowed = MatchFlow::LaunchAllowed(referee_seen_, referee_input_);
    if (state_.slider_homed && !slider_pair_.Update(
            slider_left_.Axis().Position(), slider_right_.Axis().Position())) {
        safety_.Latch(SafetyFault::SliderSync);
    }
    state_.fault = safety_.Check(now, state_.competition, state_.remote_required);
    if (state_.fault == SafetyFault::None && !state_.motors_online) {
        state_.fault = SafetyFault::MotorOffline;
    }
    if (state_.fault == SafetyFault::None) { state_.fault = sequence_.Fault(); }
}

void LauncherHardware::Tick(double now, double dt) {
    int feedback_index = 0;
    for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) {
        port->Update(now, safety_);
        if (!kMotorInstalled[feedback_index]) {
            // 未安装电机的槽位每拍刷新，避免 Safety::Check 把缺席设备判为离线。
            safety_.MotorFeedback(feedback_index, now);
        }
        ++feedback_index;
    }
    if constexpr (kArmInstalled) {
        // 摆臂在线、有反馈、还没 Homed 也不是 Failed 时，按绝对编码器建立零点。
        if (arm_.Online() && !arm_.Axis().Homed() && arm_.Axis().State() != HomeState::Failed) {
            double angle = (static_cast<double>(arm_.Raw()) - kArmZeroCounts) * 2 * kPi / kEncoderRange;
            if (angle <= -kPi) { angle += 2 * kPi; }
            if (angle > kPi) { angle -= 2 * kPi; }
            // 无力时手转后再切手动，要按当时的位置保持，不能沿用上电建零点时的目标。
            if (arm_.Axis().HomeAt(angle)) { arm_.Axis().Stop(); }
        }
    }
    std::uint32_t count = 0, received_ms = 0;
    std::uint32_t frames_ok = 0, frames_cleared = 0, frames_bad = 0;
    const bool received = remote_.Read(remote_input_, count, received_ms,
                                       frames_ok, frames_cleared, frames_bad);
    // 在复制快照以后取时间，避免接收线程发布时刻晚于本拍起点而造成无符号回绕。
    const std::uint32_t sampled_ms = LibXR::Timebase::GetMilliseconds();
    const double age = static_cast<std::uint32_t>(sampled_ms - received_ms) / 1000.0;
    if (received && count != remote_sequence_) {
        remote_sequence_ = count;
        // 接收时刻可能早于控制线程启动，夹到启动原点而非延长旧帧寿命。
        safety_.RemoteFeedback(std::max(0.0, now - age));
    }
    remote_input_.online = received && age <= kRemoteOfflineTime;
    state_.remote_online = remote_input_.online;
    if (referee_.Update(referee_input_, now)) {
        referee_seen_ = true;
        referee_time_ = now;
        safety_.RefereeFeedback(now);
    }
    referee_input_.online = referee_time_ >= 0 && now - referee_time_ <= kRefereeOfflineTime;
    RefreshState(now);
    const bool enabled = command_.mode != Mode::Powerless && state_.fault == SafetyFault::None;
    const bool hold = (!enabled || stop_hold_) && Safety::SliderMustHold(state_);
    slider_left_.Compute(dt, enabled && kSliderInstalled, hold);
    slider_right_.Compute(dt, enabled && kSliderInstalled, hold);
    pitch_.Compute(dt, enabled && kPitchInstalled);
    yaw_.Compute(dt, enabled && kYawInstalled);
    arm_.Compute(dt, enabled && kArmInstalled);
    RefreshState(now);

    mode_dt_ += dt;
    if (++ticks_ % 10 == 0) {
        command_ = mode_.Update(remote_input_, referee_input_, mode_dt_);
        MatchMechanism mechanism;
        mechanism.slider_homed = !kSliderInstalled || state_.slider_homed;
        mechanism.aim_homed = (!kPitchInstalled || pitch_.Axis().Homed()) &&
            (!kYawInstalled || yaw_.Axis().Homed());
        mechanism.all_homed = state_.all_homed;
        mechanism.homing = Homing(slider_left_.Axis()) || Homing(slider_right_.Axis()) ||
            (kPitchInstalled && Homing(pitch_.Axis())) || (kYawInstalled && Homing(yaw_.Axis()));
        mechanism.motors_online = state_.motors_online;
        mechanism.busy = sequence_.Busy();
        mechanism.decocking = sequence_.Decocking();
        mechanism.cocking = sequence_.Cocking();
        mechanism.seated = inventory_.Seated();
        mechanism.latch = state_.latch_state;
        mechanism.fault = state_.fault;
        mechanism.darts = inventory_.Remaining();
        // 阶段 3c：接着裁判系统时，按比赛阶段接管遥控器，自动回零、上膛、发射、退膛；没接裁判系统时什么都不改。
        match_.Update(command_, remote_input_, referee_input_, mechanism, referee_seen_, mode_dt_);
        if (command_.cock_refused && now - match_warn_time_[0] >= 2.0) {
            XR_LOG_WARN("当前阶段不许上膛");
            match_warn_time_[0] = now;
        }
        if (command_.fire_refused && now - match_warn_time_[1] >= 2.0) {
            XR_LOG_WARN("当前阶段不许发射");
            match_warn_time_[1] = now;
        }
        if (command_.cocked_warning && now - match_warn_time_[2] >= 5.0) {
            XR_LOG_WARN("准备阶段发现上着膛");
            match_warn_time_[2] = now;
        }
        if ((command_.fault_reset || command_.auto_reset) && state_.fault != SafetyFault::None) {
            if (command_.auto_reset) { XR_LOG_WARN("比赛中自动复位故障"); }
            // Reset 内的 StopMotors 继续按当前受力状态保持滑块，复位不改变锁止状态。
            sequence_.Reset();
            safety_.ClearLatched();
            for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) {
                if (port->Axis().State() == HomeState::Failed) { port->Axis().ClearFault(); }
            }
            slider_pair_.Rebase(slider_left_.Axis().Position(), slider_right_.Axis().Position());
            XR_LOG_INFO("故障已复位");
        }
        if (command_.stop_sequence && sequence_.Busy()) { sequence_.Cancel(); }
        // 离开手动模式或取消普通自动回零时停掉未完成的运动；托着发射机构时照样保持。
        if ((command_.leave_manual || command_.auto_home_cancel) && !sequence_.Busy()) {
            StopMotors(Safety::SliderMustHold(state_));
            if (command_.auto_home_cancel) { XR_LOG_INFO("自动回零已取消"); }
        }
        RefreshState(now);
        if (state_.fault == SafetyFault::None) {
            if (!sequence_.Busy() && (command_.home_aim || command_.home_slider)) {
                if (command_.home_aim && command_.home_slider) {
                    const bool aim_installed = kPitchInstalled || kYawInstalled;
                    const bool any_installed = aim_installed || kSliderInstalled;
                    if (!any_installed) {
                        WarnNotInstalled(now);
                    } else {
                        if (aim_installed) { StartHome(ManualGroup::Aim, now); }
                        if (kSliderInstalled) { StartHome(ManualGroup::Slider, now); }
                    }
                } else if (command_.home_aim) {
                    StartHome(ManualGroup::Aim, now);
                } else {
                    StartHome(ManualGroup::Slider, now);
                }
            }
            if (command_.auto_home_slider && !sequence_.Busy()) {
                if (command_.match_override) {
                    XR_LOG_INFO("比赛中自动回零：滑块");
                } else {
                    XR_LOG_INFO("自动回零：滑块");
                }
                const HomeStartResult result = StartHome(ManualGroup::Slider, now);
                if (!command_.match_override && result != HomeStartResult::Started) {
                    match_.RejectAutoHome();
                }
            }
            if (command_.auto_home_aim && !sequence_.Busy()) {
                if (command_.match_override) {
                    XR_LOG_INFO("比赛中自动回零：瞄准");
                } else {
                    XR_LOG_INFO("自动回零：瞄准");
                }
                const HomeStartResult result = StartHome(ManualGroup::Aim, now);
                if (!command_.match_override && result != HomeStartResult::Started) {
                    match_.RejectAutoHome();
                }
            }
            if (command_.cock_request && !sequence_.Busy()) {
                // 已锁住时拨轮下拨是退膛：不发射，把皮筋慢慢放松。
                Cock(state_.latch_state == LatchState::Latched);
            }
            if (command_.auto_cock && !sequence_.Busy()) {
                XR_LOG_INFO("比赛中自动上膛");
                Cock(false);
            }
            if (command_.auto_decock && !sequence_.Busy()) { Cock(true); }
            if (command_.magnet_toggle && !sequence_.Busy()) {
                if constexpr (kMagnetInstalled) {
                    // 台架上手动切换电磁铁通断，不改库存推断；通电满 kMagnetOnLimit 没切回也会自动断电。
                    const bool on = !magnet_guard_.On();
                    SetMagnet(on);
                    if (on) {
                        // 取镖试运行吸上来的镖由操作手这样放掉，之后才允许取下一发。
                        sequence_.ReleaseFetch();
                        XR_LOG_INFO("电磁铁通电消磁命令");
                    } else {
                        XR_LOG_INFO("电磁铁断电吸持命令");
                    }
                } else {
                    WarnNotInstalled(now);
                }
            }
            if (command_.fetch_request && !sequence_.Busy()) {
                if (sequence_.FetchHolding() || inventory_.Held()) {
                    if (sequence_.CommandLoadFromFetch()) {
                        XR_LOG_INFO("独立装填开始，完成后停住");
                    } else {
                        XR_LOG_WARN("装填请求未被接受");
                    }
                } else {
                    StartFetchTest(now);
                }
            }
            if (command_.fire && !sequence_.Busy()) {
                if (!state_.launch_allowed) {
                    // 比赛流程已经丢掉这类请求；这里是最后一道，阶段不对绝不发射。
                    XR_LOG_WARN("当前阶段不许发射");
                } else {
                    if constexpr (kReloadInstalled) {
                        sequence_.CommandSalvo(1);
                    } else {
                        // 没装换弹机构：镖由操作手手动放，直接发射发射机构上这一发，不装填下一发。
                        sequence_.CommandFire();
                    }
                }
            }
            if (command_.salvo && !sequence_.Busy()) {
                if (!state_.launch_allowed) {
                    XR_LOG_WARN("当前阶段不许发射");
                } else {
                    if constexpr (!kReloadInstalled) {
                        XR_LOG_WARN("换弹机构未安装");
                    } else {
                        sequence_.CommandSalvo(inventory_.Remaining());
                    }
                }
            }
            if (command_.mode == Mode::Manual && !sequence_.Busy()) { Manual(command_, mode_dt_, now); }
        }
        if (sequence_.Busy()) { sequence_.Tick(now, command_.mode != Mode::Powerless); }
        mode_dt_ = 0;
        RefreshState(now);
    }
    // 模式改变或流程新故障必须在本拍下发前生效，受力滑块同步进入保持。
    const bool output_enabled = command_.mode != Mode::Powerless && state_.fault == SafetyFault::None;
    const bool output_hold = (!output_enabled || stop_hold_) && Safety::SliderMustHold(state_);
    slider_left_.Gate(dt, output_enabled && kSliderInstalled, output_hold);
    slider_right_.Gate(dt, output_enabled && kSliderInstalled, output_hold);
    pitch_.Gate(dt, output_enabled && kPitchInstalled);
    yaw_.Gate(dt, output_enabled && kYawInstalled);
    arm_.Gate(dt, output_enabled && kArmInstalled);
    if constexpr (kArmInstalled) {
        const bool was_tripped = arm_stall_.Tripped();
        // 电机凉时允许更大的保持电流，越热越接近持续堵转以下的 kArmStallCurrent。
        arm_stall_.SetLimit(ArmStallLimit(arm_.FeedbackTemp()));
        const int requested = arm_.Current();
        const int guarded = arm_stall_.Update(requested, arm_.FeedbackRpm(), dt);
        // 限流时把实际电流反馈给速度环，积分不再往被限掉的方向涨。
        if (guarded != requested) { arm_.Axis().TrackLimitedCurrent(requested, guarded); }
        arm_.SetCommand(guarded);
        if (arm_stall_.Tripped() && !was_tripped) {
            XR_LOG_WARN("摆臂堵转，电流限到 %.2f A", arm_stall_.Limit() * 3.0 / 16384);
        }
        if (!arm_stall_.Tripped() && was_tripped) { XR_LOG_INFO("摆臂电流限制解除"); }
    }
    // RMMotor 同组控制帧要求五个实例每拍都调用 Control，零电流也要发送。
    // 未安装电机的电流指令是 0，但不能跳过发送，否则同组已安装电机也发不出去。
    for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) { port->Send(); }
    if (output_enabled) {
        if constexpr (kLockServoCalibrated) {
            lock_.Update(dt);
        }
        if constexpr (kCrankInstalled && kCrankServoCalibrated) {
            crank_.Update(dt);
        }
    }
    // 电磁铁只许短时通电：流程和手动的命令都经限时，到时、无力或故障立即断电吸持；只在通断变化时写继电器。
    switch (magnet_guard_.Update(dt, output_enabled)) {
    case MagnetCut::Expired:
        XR_LOG_INFO("电磁铁通电满 %.1f s，自动断电吸持", kMagnetOnLimit);
        break;
    case MagnetCut::Blocked:
        XR_LOG_INFO("无力或故障，电磁铁断电吸持");
        break;
    case MagnetCut::None:
        break;
    }
    if (magnet_guard_.On() != magnet_on_) {
        magnet_on_ = magnet_guard_.On();
        magnet_.Set(magnet_on_);
    }
    LibXR::Mutex::LockGuard guard(monitor_mutex_);
    monitor_state_ = state_;
    monitor_mode_ = command_.mode;
    monitor_step_ = sequence_.StepName();
    monitor_inventory_ = inventory_.Remaining();
    std::size_t index = 0;
    for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) {
        monitor_feedback_.online[index] = kMotorInstalled[index] && port->Online();
        monitor_feedback_.homed[index] = kMotorInstalled[index] && port->Axis().Homed();
        monitor_feedback_.position[index] = kMotorInstalled[index] ? port->Axis().Position() : 0;
        ++index;
    }
    monitor_feedback_.slider_current = kSliderInstalled ? SliderCurrent() : 0;
    monitor_feedback_.pitch_current = kPitchInstalled ? pitch_.Current() : 0;
    monitor_feedback_.pitch_feedback_torque = kPitchInstalled ? pitch_.FeedbackTorque() : 0;
    monitor_feedback_.pitch_feedback_rpm = kPitchInstalled ? pitch_.FeedbackRpm() : 0;
    monitor_feedback_.yaw_current = kYawInstalled ? yaw_.Current() : 0;
    monitor_feedback_.yaw_feedback_torque = kYawInstalled ? yaw_.FeedbackTorque() : 0;
    monitor_feedback_.yaw_feedback_rpm = kYawInstalled ? yaw_.FeedbackRpm() : 0;
    monitor_feedback_.yaw_span = kYawInstalled ? yaw_.Axis().HomeSpan() : 0;
    monitor_feedback_.yaw_stage = kYawInstalled ? yaw_.Axis().DualStage() : DualHomeStage::Idle;
    monitor_feedback_.yaw_fault = kYawInstalled ? yaw_.Axis().DualFault() : DualHomeFault::None;
    monitor_feedback_.arm_current = kArmInstalled ? arm_.Current() : 0;
    monitor_feedback_.arm_feedback_torque = kArmInstalled ? arm_.FeedbackTorque() : 0;
    monitor_feedback_.arm_feedback_rpm = kArmInstalled ? arm_.FeedbackRpm() : 0;
    monitor_feedback_.arm_feedback_temp = kArmInstalled ? arm_.FeedbackTemp() : 0;
    monitor_feedback_.arm_stall = kArmInstalled && arm_stall_.Tripped();
    monitor_feedback_.arm_bus_frames = arm_bus_frames_.load(std::memory_order_relaxed);
    monitor_feedback_.arm_raw = kArmInstalled ? arm_.Raw() : 0;
    if constexpr (kLockServoCalibrated) {
        monitor_feedback_.lock_angle = lock_.Value();
        monitor_feedback_.lock_pulse = lock_.PulseUs();
        monitor_feedback_.lock_raw_mode = false;
    } else {
        monitor_feedback_.lock_angle = 0;
        monitor_feedback_.lock_pulse = lock_raw_enabled_ ? lock_raw_pulse_ : 0;
        monitor_feedback_.lock_raw_mode = true;
    }
    if constexpr (kCrankInstalled && kCrankServoCalibrated) {
        monitor_feedback_.crank_angle = crank_.Value();
        monitor_feedback_.crank_pulse = crank_.PulseUs();
        monitor_feedback_.crank_raw_mode = false;
    } else if constexpr (kCrankInstalled) {
        monitor_feedback_.crank_angle = 0;
        monitor_feedback_.crank_pulse = crank_raw_enabled_ ? crank_raw_pulse_ : 0;
        monitor_feedback_.crank_raw_mode = true;
    } else {
        monitor_feedback_.crank_angle = 0;
        monitor_feedback_.crank_pulse = 0;
        monitor_feedback_.crank_raw_mode = false;
    }
    monitor_feedback_.magnet_on = magnet_on_;
    monitor_feedback_.remote = remote_input_;
    monitor_feedback_.remote_ok = frames_ok;
    monitor_feedback_.remote_clr = frames_cleared;
    monitor_feedback_.remote_bad = frames_bad;
    monitor_feedback_.tick_avg_us = last_tick_avg_us_;
    monitor_feedback_.tick_max_us = last_tick_max_us_;
    monitor_feedback_.late_wakes = last_late_wakes_;
    monitor_feedback_.shot = sequence_.LastShot();
    monitor_feedback_.referee = referee_input_;
    monitor_feedback_.competition = referee_seen_;
    monitor_feedback_.match_override = command_.match_override;
    monitor_feedback_.estop = match_.EmergencyStop();
    monitor_feedback_.remote_required = state_.remote_required;
    monitor_feedback_.match_retries = match_.Retries();
}

void LauncherHardware::RecordControlTiming(std::uint32_t tick_us, std::uint32_t wake_gap_ms) {
    tick_us_sum_ += tick_us;
    if (tick_us > tick_us_max_) { tick_us_max_ = tick_us; }
    if (wake_gap_ms > 2) { ++late_wakes_; }
    if (++timing_ticks_ >= 1000) {
        // 完整窗口先结算，下一拍在锁里复制结果，监控不读取正在累计的窗口。
        last_tick_avg_us_ = static_cast<std::uint32_t>(tick_us_sum_ / timing_ticks_);
        last_tick_max_us_ = tick_us_max_;
        last_late_wakes_ = late_wakes_;
        tick_us_sum_ = 0;
        tick_us_max_ = 0;
        late_wakes_ = 0;
        timing_ticks_ = 0;
    }
}

void LauncherHardware::OnMonitor() {
    SafetyState state;
    Mode mode;
    const char *step;
    int remaining;
    MonitorFeedback feedback;
    {
        LibXR::Mutex::LockGuard guard(monitor_mutex_);
        state = monitor_state_;
        mode = monitor_mode_;
        step = monitor_step_;
        remaining = monitor_inventory_;
        feedback = monitor_feedback_;
    }
    // BSP 日志消息仅 64 字节，标定快照直接写入已注册的 USB STDIO。
    // 每行单独发送，不补空格。
    LibXR::STDIO::Printf<"Dart mode=%d fault=%s darts=%d home=%d latch=%d\r\n">(
                static_cast<int>(mode), Safety::Message(state.fault), remaining,
                static_cast<int>(state.all_homed), static_cast<int>(state.latch_state));
    // STDIO 走 USB CDC，发送缓冲只有 128 字节；每行之间等 20 ms，让上一行发完，免得下一行被截短。
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart step=%s\r\n">(step);
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart remote ok=%u clr=%u bad=%u\r\n">(
                static_cast<unsigned>(feedback.remote_ok),
                static_cast<unsigned>(feedback.remote_clr),
                static_cast<unsigned>(feedback.remote_bad));
    LibXR::Thread::Sleep(20);
    // 拨杆 U 上 M 中 D 下，通道值保留两位小数，阶段 3 靠这行核对方向。
    const auto switch_letter = [](Switch value) {
        return value == Switch::Up ? 'U' : value == Switch::Middle ? 'M' : 'D';
    };
    LibXR::STDIO::Printf<"Dart sw R=%c L=%c sticks %.2f %.2f %.2f %.2f wheel %.2f\r\n">(
                switch_letter(feedback.remote.right), switch_letter(feedback.remote.left),
                static_cast<float>(feedback.remote.right_x), static_cast<float>(feedback.remote.right_y),
                static_cast<float>(feedback.remote.left_x), static_cast<float>(feedback.remote.left_y),
                static_cast<float>(feedback.remote.wheel));
    LibXR::Thread::Sleep(20);
    // LibXR 的 STDIO 遇到空字符串，会把这一次打印后面的内容全丢掉，所以 %s 参数不能是空串。
    LibXR::STDIO::Printf<"Dart online=%d/%d/%d/%d/%d home=%d/%d/%d/%d/%d "
                        "sl=%.2f/%.2fmm I=%d pi=%.2f%s pI=%d yaw=%.2f%s\r\n">(
                static_cast<int>(feedback.online[0]), static_cast<int>(feedback.online[1]),
                static_cast<int>(feedback.online[2]), static_cast<int>(feedback.online[3]),
                static_cast<int>(feedback.online[4]), static_cast<int>(feedback.homed[0]),
                static_cast<int>(feedback.homed[1]), static_cast<int>(feedback.homed[2]),
                static_cast<int>(feedback.homed[3]), static_cast<int>(feedback.homed[4]),
                static_cast<float>(feedback.position[0] * 1000),
                static_cast<float>(feedback.position[1] * 1000),
                static_cast<int>(feedback.slider_current),
                static_cast<float>(feedback.position[2] * 1000),
                kPitchInstalled ? "mm" : "mm[X]",
                static_cast<int>(feedback.pitch_current),
                 static_cast<float>(feedback.position[3] / (2 * kPi)),
                 kYawInstalled ? "rev" : "rev[X]");
    LibXR::Thread::Sleep(20);
    // 推杆没装加 [X]，未标定、显示原始脉宽时加 [R]，和锁止一样；电磁铁没装加 [X]。
    const char *crank_mark = !kCrankInstalled ? "[X] " : feedback.crank_raw_mode ? "[R] " : " ";
    if (feedback.lock_raw_mode) {
        LibXR::STDIO::Printf<"Dart arm=%.2f raw=%u%slock=%.0f[R] crank=%.0f%smag=%d%s"
                            "tick %u/%uus late=%u\r\n">(
                    static_cast<float>(feedback.position[4] * 180 / kPi),
                    static_cast<unsigned>(feedback.arm_raw),
                    kArmInstalled ? " " : "[X] ",
                    static_cast<float>(feedback.lock_pulse),
                    static_cast<float>(feedback.crank_pulse),
                    crank_mark,
                    static_cast<int>(feedback.magnet_on),
                    kMagnetInstalled ? " " : "[X] ",
                    static_cast<unsigned>(feedback.tick_avg_us),
                    static_cast<unsigned>(feedback.tick_max_us),
                    static_cast<unsigned>(feedback.late_wakes));
    } else {
        LibXR::STDIO::Printf<"Dart arm=%.2f raw=%u%slock=%.2fdeg/%.0f crank=%.0f%smag=%d%s"
                            "tick %u/%uus late=%u\r\n">(
                    static_cast<float>(feedback.position[4] * 180 / kPi),
                    static_cast<unsigned>(feedback.arm_raw),
                    kArmInstalled ? " " : "[X] ",
                    static_cast<float>(feedback.lock_angle * 180 / kPi),
                    static_cast<float>(feedback.lock_pulse),
                    static_cast<float>(feedback.crank_pulse),
                    crank_mark,
                    static_cast<int>(feedback.magnet_on),
                    kMagnetInstalled ? " " : "[X] ",
                    static_cast<unsigned>(feedback.tick_avg_us),
                    static_cast<unsigned>(feedback.tick_max_us),
                    static_cast<unsigned>(feedback.late_wakes));
    }
    LibXR::Thread::Sleep(20);
    // 三个标定位置取自同一份快照；推杆角由输出脉宽估计，贴合与避让仍由操作手判断。
    LibXR::STDIO::Printf<"Dart cal crank=%.0fus/%.5frad slider=%.5fm arm=%.2fdeg home=%d/%d\r\n">(
                static_cast<float>(feedback.crank_pulse),
                static_cast<float>(feedback.crank_angle),
                static_cast<float>(state.slider),
                static_cast<float>(feedback.position[4] * 180 / kPi),
                static_cast<int>(state.slider_homed), static_cast<int>(feedback.homed[4]));
    if constexpr (kPitchInstalled) {
        LibXR::Thread::Sleep(20);
        const int actual_current = static_cast<int>(std::lround(
            feedback.pitch_feedback_torque / kM3508RotorTorqueConstant / 20.0 * 16384));
        LibXR::STDIO::Printf<"Dart pitch cmd=%d act=%d rpm=%.0f\r\n">(
                    static_cast<int>(feedback.pitch_current), actual_current,
                    static_cast<float>(feedback.pitch_feedback_rpm));
    }
    if constexpr (kArmInstalled) {
        LibXR::Thread::Sleep(20);
        // act is the driver's raw feedback value, not an independently calibrated current.
        // act 是 6020 回报的实际转矩电流，按 0.741 N·m/A、±3 A 对应 ±16384 换回和 cmd 一样的单位。
        const int arm_actual = static_cast<int>(std::lround(
            feedback.arm_feedback_torque / kGm6020TorqueConstant / 3.0 * 16384));
        LibXR::STDIO::Printf<"Dart arm cmd=%d act=%d rpm=%.0f tmp=%.0f stall=%d bus=%u\r\n">(
                    static_cast<int>(feedback.arm_current), arm_actual,
                    static_cast<float>(feedback.arm_feedback_rpm),
                    static_cast<float>(feedback.arm_feedback_temp),
                    static_cast<int>(feedback.arm_stall),
                    static_cast<unsigned>(feedback.arm_bus_frames));
    }
    // 阶段 3c：裁判系统和比赛流程的状态。seen 是启动以来收到过裁判数据；stage 同 0x0001（-1 未知，4 比赛中）；
    if constexpr (kYawInstalled) {
        LibXR::Thread::Sleep(20);
        LibXR::STDIO::Printf<"Dart yaw phase=%d fault=%d span=%.3frev\r\n">(
            static_cast<int>(feedback.yaw_stage), static_cast<int>(feedback.yaw_fault),
            static_cast<float>(feedback.yaw_span / (2 * kPi)));
        LibXR::Thread::Sleep(20);
        const int actual = static_cast<int>(std::lround(feedback.yaw_feedback_torque / 0.005 / 10 * 16384));
        LibXR::STDIO::Printf<"Dart yaw cmd=%d act=%d rpm=%.0f\r\n">(
            feedback.yaw_current, actual, static_cast<float>(feedback.yaw_feedback_rpm));
    }
    // rem 是本阶段剩余秒数；gate 为 1 是闸门已全开；launch 是选手端最后一次确认发射的比赛剩余秒数；
    // tgt、cd、hit 来自 0x0105（选中的目标、发射剩余秒数、最近命中的目标/累计被命中次数），只显示；
    // rc 为 0 是比赛中遥控器可以不在线；est 是遥控器急停，ovr 是比赛流程接管，try 是已自动复位的次数。
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart ref seen=%d stage=%d rem=%u gate=%d launch=%u tgt=%u cd=%u hit=%u/%u rc=%d est=%d ovr=%d try=%d\r\n">(
                static_cast<int>(feedback.competition),
                static_cast<int>(feedback.referee.stage),
                static_cast<unsigned>(feedback.referee.stage_remaining),
                static_cast<int>(feedback.referee.gate_open),
                static_cast<unsigned>(feedback.referee.launch_time),
                static_cast<unsigned>(feedback.referee.dart_target),
                static_cast<unsigned>(feedback.referee.dart_countdown),
                static_cast<unsigned>(feedback.referee.dart_last_target),
                static_cast<unsigned>(feedback.referee.dart_hits),
                static_cast<int>(feedback.remote_required),
                static_cast<int>(feedback.estop),
                static_cast<int>(feedback.match_override),
                feedback.match_retries);
    // 新的一发打一行发射记录，每发只打一次。%s 参数不能是空串。
    if (feedback.shot.number != printed_shot_) {
        printed_shot_ = feedback.shot.number;
        LibXR::Thread::Sleep(20);
        if (feedback.shot.source == LatchSource::Press) {
            LibXR::STDIO::Printf<"Dart shot n=%u pi=%.2fmm bot=%.1fmm hold=%d chk=%d wait=%.1fs\r\n">(
                        static_cast<unsigned>(feedback.shot.number),
                        static_cast<float>(feedback.shot.pitch * 1000),
                        static_cast<float>(feedback.shot.bottom * 1000),
                        static_cast<int>(feedback.shot.hold_current),
                        static_cast<int>(feedback.shot.check_current),
                        static_cast<float>(feedback.shot.wait));
        } else {
            LibXR::STDIO::Printf<"Dart shot n=%u pi=%.2fmm bot=%s chk=%d wait=%.1fs\r\n">(
                        static_cast<unsigned>(feedback.shot.number),
                        static_cast<float>(feedback.shot.pitch * 1000),
                        feedback.shot.source == LatchSource::Probe ? "探测" : "无",
                        static_cast<int>(feedback.shot.check_current),
                        static_cast<float>(feedback.shot.wait));
        }
    }
    const auto tension = tension_right_.Read();
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart tension R raw=%d new=%d n=%u age=%ums\r\n">(
                static_cast<int>(tension.raw), static_cast<int>(tension.updated),
                static_cast<unsigned>(tension.samples), static_cast<unsigned>(tension.age_ms));
    const auto rs485 = tension_rs485_.Read();
    const int left_new = rs485.left.samples != printed_tension_left_;
    const int right_new = rs485.right.samples != printed_tension_right_;
    printed_tension_left_ = rs485.left.samples;
    printed_tension_right_ = rs485.right.samples;
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart rs485 L raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums\r\n">(
                static_cast<int>(rs485.left.raw), left_new,
                static_cast<unsigned>(rs485.left.samples), static_cast<unsigned>(rs485.left.timeouts),
                static_cast<unsigned>(rs485.left.exceptions), static_cast<unsigned>(rs485.left.errors),
                static_cast<unsigned>(rs485.left.age_ms));
    LibXR::Thread::Sleep(20);
    LibXR::STDIO::Printf<"Dart rs485 R raw=%d new=%d n=%u to=%u ex=%u err=%u age=%ums\r\n">(
                static_cast<int>(rs485.right.raw), right_new,
                static_cast<unsigned>(rs485.right.samples), static_cast<unsigned>(rs485.right.timeouts),
                static_cast<unsigned>(rs485.right.exceptions), static_cast<unsigned>(rs485.right.errors),
                static_cast<unsigned>(rs485.right.age_ms));
}

void LauncherHardware::WarnNotInstalled(double now) {
    if (mechanism_warn_time_ < 0 || now - mechanism_warn_time_ >= 1.0) {
        XR_LOG_WARN("该机构未安装");
        mechanism_warn_time_ = now;
    }
}

void LauncherHardware::Cock(bool decock) {
    if constexpr (!kLockServoCalibrated) {
        // 锁止舵机未标定时，流程里的锁止步骤会超时报故障，先拒绝。
        XR_LOG_WARN("锁止舵机未标定");
    } else if constexpr (!kCockEnabled) {
        // 锁止舵机单独供电、锁止位置实测之前，先拒绝上膛。
        XR_LOG_WARN("上膛未开放");
    } else {
        if (!state_.all_homed) {
            XR_LOG_WARN("轴未全部回零，拒绝上膛");
        } else if (decock) {
            XR_LOG_INFO("退膛");
            sequence_.CommandDecock();
        } else if (!state_.launch_allowed) {
            // 比赛流程已经丢掉这类请求；这里是最后一道，阶段不对绝不上膛（R50）。
            XR_LOG_WARN("当前阶段不许上膛");
        } else {
            // 流程展开前继续保持，实际 SetSlider 接管目标时再退出保持。
            sequence_.CommandCock();
        }
    }
}

void LauncherHardware::StartFetchTest(double now) {
    if constexpr (!kArmInstalled || !kCrankInstalled || !kMagnetInstalled) {
        WarnNotInstalled(now);
    } else if constexpr (!kCrankServoCalibrated) {
        XR_LOG_WARN("推杆未标定，不能自动取镖");
    } else {
        const double temp = arm_.FeedbackTemp();
        if (!arm_.Axis().Homed()) {
            XR_LOG_WARN("摆臂没建零点，不能取镖");
        } else if (!(temp <= kFetchMaxTemp)) {
            XR_LOG_WARN("摆臂 %.0f°C 太热，先不取镖", temp);
        } else if (sequence_.FetchHolding()) {
            XR_LOG_WARN("磁铁上可能有镖，先放镖");
        } else if (sequence_.CommandFetchTest()) {
            if (sequence_.NextFetchSide() < 0) {
                XR_LOG_INFO("自动取镖：左侧");
            } else {
                XR_LOG_INFO("自动取镖：右侧");
            }
        } else {
            XR_LOG_WARN("取镖试运行没被接受");
        }
    }
}

LauncherHardware::HomeStartResult LauncherHardware::StartHome(ManualGroup group, double now) {
    if ((group == ManualGroup::Aim && !kPitchInstalled && !kYawInstalled) ||
        (group == ManualGroup::Loading && !kArmInstalled && !kCrankInstalled) ||
        (group == ManualGroup::Slider && !kSliderInstalled)) {
        WarnNotInstalled(now);
        return HomeStartResult::NotInstalled;
    }
    if (group == ManualGroup::Slider && Safety::SliderMustHold(state_)) {
        XR_LOG_WARN("滑块托着发射机构，拒绝回零");
        return HomeStartResult::BlockedBySliderHold;
    }
    switch (group) {
    case ManualGroup::Aim:
        if constexpr (kYawInstalled) { yaw_.Axis().StartHome(); }
        if constexpr (kPitchInstalled) { pitch_.Axis().StartHome(); }
        return HomeStartResult::Started;
    case ManualGroup::Slider:
        stop_hold_ = false;
        if constexpr (kSliderInstalled) {
            slider_left_.Axis().StartHome();
            slider_right_.Axis().StartHome();
        }
        return HomeStartResult::Started;
    case ManualGroup::Loading:
        if constexpr (kArmInstalled) {
            XR_LOG_INFO("摆臂按编码器绝对角，不需要回零");
        }
        return HomeStartResult::NotInstalled;
    }
    return HomeStartResult::NotInstalled;
}

void LauncherHardware::Manual(const ModeCommand &command, double dt, double now) {
    // 未选中的组停止点动；回零中的轴由其状态机接管。
    if (kYawInstalled && !Homing(yaw_.Axis())) {
        yaw_.Axis().Jog(command.yaw_rate);
    }
    if (kPitchInstalled && !Homing(pitch_.Axis())) { pitch_.Axis().Jog(command.pitch_rate); }
    if constexpr (kArmInstalled) {
        if (!Homing(arm_.Axis())) { arm_.Axis().Jog(command.arm_rate); }
    }
    if ((command.group == ManualGroup::Aim && !kPitchInstalled && !kYawInstalled &&
         (command.yaw_rate != 0 || command.pitch_rate != 0)) ||
        (command.group == ManualGroup::Loading && !kArmInstalled && !kCrankInstalled &&
         (command.arm_rate != 0 || command.crank_rate != 0))) {
        WarnNotInstalled(now);
    }
    if (kSliderInstalled && !Homing(slider_left_.Axis()) && !Homing(slider_right_.Axis())) {
        double speed = state_.latch_state == LatchState::Unknown
            ? std::max(0.0, command.slider_rate) : command.slider_rate;
        if (speed < 0 && (slider_left_.Axis().Position() <= kManualSliderFloor ||
                          slider_right_.Axis().Position() <= kManualSliderFloor)) {
            speed = 0;
        }
        if (speed != 0) { stop_hold_ = false; }
        // 手动摇杆回中时，托着发射机构的滑块仍必须保持当前位置。
        if (speed == 0 && Safety::SliderMustHold(state_)) {
            stop_hold_ = true;
        } else {
            slider_left_.Axis().Jog(speed);
            slider_right_.Axis().Jog(speed);
        }
    }
    if constexpr (kLockServoCalibrated) {
        // 标定过的锁止舵机：走机构角换算。
        if (command.lock_rate != 0) {
            const double target = Clamp(lock_.Value() + command.lock_rate * dt, kLockClosed, kLockOpen);
            if (std::fabs(target - lock_.Value()) >= 1e-6 &&
                (target <= lock_.Value() || Safety::LockOpenAllowed(State()))) { SetLock(target); }
        }
    } else {
        // 锁止舵机未标定：手动用原始脉宽微调，不走机构角。
        if (command.lock_rate != 0) {
            if (!lock_raw_enabled_) {
                // 首次非零输入时才使能通道，先转到实测行程中间，避免上电猛转，也不顶限位。
                lock_raw_pulse_ = kLockBootPulseUs;
                lock_.PwmRef().SetConfig({static_cast<std::uint32_t>(kServoFrequency)});
                lock_.PwmRef().SetDutyCycle(static_cast<float>(lock_raw_pulse_ / 20000.0));
                lock_.PwmRef().Enable();
                lock_raw_enabled_ = true;
                XR_LOG_INFO("锁止舵机首次唤醒 %.0f us", lock_raw_pulse_);
            } else {
                // 按 kLockPulseRate 调整脉宽，夹在实测行程内，摇杆推过头也不顶限位。
                lock_raw_pulse_ = Clamp(lock_raw_pulse_ + command.lock_rate / kServoSpeed *
                                         kLockPulseRate * dt,
                                         kLockRawPulseMin, kLockRawPulseMax);
                lock_.PwmRef().SetDutyCycle(static_cast<float>(lock_raw_pulse_ / 20000.0));
            }
            // 原始脉宽不能推断机构状态，标定阶段保持“未知”。
            state_.latch_state = LatchState::Unknown;
        }
    }
    if constexpr (kCrankInstalled) {
        if constexpr (kCrankServoCalibrated) {
            // 标定过的推杆舵机：走机构角换算。
            if (command.crank_rate != 0) {
                // 手动推杆和原始脉宽模式一样快：满杆每秒 kCrankPulseRate，最深到 kCrankRawPulseMax。
                const double target = Clamp(crank_.Value() + command.crank_rate / kServoSpeed * kCrankManualSpeed * dt,
                                            kCrankRetracted, kCrankManualMax);
                if (std::fabs(target - crank_.Value()) >= 1e-6) { SetCrank(target); }
            }
        } else {
            // 推杆舵机未标定：手动用原始脉宽微调，不走机构角。
            if (command.crank_rate != 0) {
                if (!crank_raw_enabled_) {
                    // 首次非零输入时才使能通道，先转到中位 1500 µs，避免上电按 500 µs 顶到 270° 行程一端。
                    crank_raw_pulse_ = kCrankBootPulseUs;
                    // 不再设频率：TIM1、TIM8 上电时 CubeMX 已配成 50 Hz；推杆和锁止共用 TIM8，
                    // 这里再 SetConfig 会重新初始化定时器，锁止舵机的脉冲会被打断一下。
                    crank_.PwmRef().SetDutyCycle(static_cast<float>(crank_raw_pulse_ / 20000.0));
                    crank_.PwmRef().Enable();
                    crank_raw_enabled_ = true;
                    XR_LOG_INFO("推杆舵机首次唤醒 %.0f us", crank_raw_pulse_);
                } else {
                    // 按 kCrankPulseRate 调整脉宽，夹在保守行程内，摇杆推过头也不出这段。
                    crank_raw_pulse_ = Clamp(crank_raw_pulse_ + command.crank_rate / kServoSpeed *
                                              kCrankPulseRate * dt,
                                              kCrankRawPulseMin, kCrankRawPulseMax);
                    crank_.PwmRef().SetDutyCycle(static_cast<float>(crank_raw_pulse_ / 20000.0));
                }
            }
        }
    }
}

void LauncherHardware::SetSlider(double position, double speed) {
    const double rate = speed > 0 ? speed : kSliderMoveSpeed;
    stop_hold_ = false;
    slider_pair_.RampTo(position, rate, slider_left_.Axis().Position(), slider_right_.Axis().Position());
    slider_left_.Axis().RampTo(position, rate);
    slider_right_.Axis().RampTo(position, rate);
}
bool LauncherHardware::SliderAt(double position, double tolerance, double vt) const {
    return state_.slider_homed && std::fabs(slider_left_.Axis().Position() - position) < tolerance &&
        std::fabs(slider_right_.Axis().Position() - position) < tolerance &&
        std::fabs(slider_left_.Axis().Velocity()) < vt && std::fabs(slider_right_.Axis().Velocity()) < vt;
}
double LauncherHardware::SliderCurrent() const {
    return std::max(std::abs(slider_left_.Current()), std::abs(slider_right_.Current()));
}
void LauncherHardware::SetYaw(double angle) { yaw_.Axis().SetTarget(angle); }
void LauncherHardware::SetPitch(double position) { pitch_.Axis().SetTarget(position); }
bool LauncherHardware::Aimed() const {
    // 没装的 pitch/yaw 按到位算：发射架整体固定，方向由操作手摆好。
    return (!kYawInstalled || yaw_.Axis().Settled(kYawTolerance, kYawVelocityTolerance)) &&
        (!kPitchInstalled || pitch_.Axis().Settled(kPitchTolerance, kPitchVelocityTolerance));
}
void LauncherHardware::SetArm(double angle) {
    // 流程每开始一段摆臂移动，堵转保护重新计时：镖座保持电流常常正好在限流值上，
    // 触发以后从一侧镖座爬到另一侧会被限在 1.2 A 爬不上去。
    arm_stall_.Reset();
    arm_.Axis().MoveTo(angle, kArmAutoSpeed);
}
bool LauncherHardware::ArmAt(double angle) const {
    return arm_.Axis().Homed() && !arm_.Axis().Moving() && std::fabs(arm_.Axis().Position() - angle) < kServoTolerance &&
        std::fabs(arm_.Axis().Velocity()) < Radians(1);
}
void LauncherHardware::SetCrank(double angle, double speed) {
    if constexpr (kCrankInstalled && !kCrankServoCalibrated) {
        // 推杆舵机未标定时，SetCrank 不改变输出，流程里的推杆步骤会超时报故障。
        XR_LOG_WARN("推杆舵机未标定");
        return;
    }
    crank_.SetTarget(angle, speed);
}
bool LauncherHardware::CrankAt(double angle) const {
    if constexpr (kCrankInstalled && !kCrankServoCalibrated) {
        // 未标定时一律返回 false，流程中的推杆步骤会超时。
        return false;
    }
    return crank_.At(angle);
}
void LauncherHardware::SetLock(double angle) {
    if constexpr (!kLockServoCalibrated) {
        // 锁止舵机未标定时，SetLock 不改变输出，自动流程里的锁止步骤会超时报故障。
        XR_LOG_WARN("锁止舵机未标定");
        return;
    }
    if (angle > lock_.Value()) { state_.latch_state = LatchState::Released; }
    else if (angle < lock_.Value() || angle == kLockClosed) { state_.latch_state = LatchState::Unknown; }
    lock_.SetTarget(angle);
}
bool LauncherHardware::LockAt(double angle) const {
    if constexpr (!kLockServoCalibrated) {
        // 未标定时一律返回 false，流程中的锁止步骤会超时。
        return false;
    }
    return lock_.At(angle);
}
void LauncherHardware::SetMagnet(bool enabled) {
    // 只记命令；Tick 末尾经限时判断后才写继电器，避免同一拍先通后断的毛刺。
    magnet_guard_.Command(enabled);
}
SafetyState LauncherHardware::State() const { return state_; }
void LauncherHardware::StopMotors(bool hold) {
    stop_hold_ = hold;
    for (auto *port : {&slider_left_, &slider_right_, &pitch_, &yaw_, &arm_}) { port->ClearCommand(); }
    slider_left_.Axis().Stop();
    slider_right_.Axis().Stop();
    slider_left_.Axis().SetHolding(hold);
    slider_right_.Axis().SetHolding(hold);
    // pitch 回零以后停机也按当前位置保持：不使能时照样输出 0，重新使能时原地不动，算到位。
    if (!pitch_.Axis().HoldPosition()) {
        pitch_.Axis().Stop();
    }
    if (!yaw_.Axis().HoldPosition()) { yaw_.Axis().Stop(); }
    arm_.Axis().Stop();
}
}
