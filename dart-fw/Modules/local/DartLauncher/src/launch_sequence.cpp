// 动态展开的顺序与 Python 相同；实车用固定出膛等待替代发射及飞行检测。
#include "launch_sequence.hpp"
#include "dart_params.hpp"
#include <cmath>

namespace dart {
LaunchSequence::LaunchSequence(LauncherIo &io, DartInventory &inventory)
    : io_(io), inventory_(inventory) {}

void LaunchSequence::Reset() {
    Cancel();
    fault_ = SafetyFault::None;
    shots_ = 0;
    label_ = "空闲";
    last_time_ = -1;
    paused_ = false;
}

void LaunchSequence::Cancel() {
    size_ = 0;
    has_current_ = false;
    paused_ = false;
    decocking_ = false;
    cocking_ = false;
    fetch_test_ = false;
    release_window_ = false;
    bottom_found_ = false;
    push_started_ = false;
    push_start_ = 0;
    bottom_window_start_ = -1;
    const SafetyState state = io_.State();
    if (prefetch_held_side_ != 0) { io_.SetCrank(state.crank); }
    prefetch_supported_side_ = 0;
    io_.StopMotors(Safety::SliderMustHold(state));
    label_ = "手动控制";
}

bool LaunchSequence::Accept() {
    return fault_ == SafetyFault::None && !Busy() && !inventory_.Fault();
}

SafetyState LaunchSequence::Snapshot() const {
    SafetyState state = io_.State();
    state.at_bottom = bottom_found_ && std::isfinite(state.slider) &&
        std::fabs(state.slider - lock_position_) <= kBottomHoldTolerance;
    state.held = inventory_.Held();
    state.held_side = inventory_.HeldSide();
    state.supported_side = prefetch_supported_side_;
    if (state.supported_side != 0 &&
        (!io_.ArmAt(state.supported_side > 0 ? kFetchRight : kFetchLeft) ||
         !io_.CrankAt(state.supported_side > 0 ? kCrankFetchRight : kCrankFetchLeft))) {
        state.supported_side = 0;
    }
    state.seated = inventory_.Seated();
    state.aimed = io_.Aimed();
    return state;
}

bool LaunchSequence::Append(const Step *steps, int count) {
    if (count < 0 || size_ + count > static_cast<int>(queue_.size())) {
        Fail(SafetyFault::QueueOverflow);
        return false;
    }
    for (int i = 0; i < count; ++i) {
        queue_[size_++] = steps[i];
    }
    return true;
}

bool LaunchSequence::Prepend(const Step *steps, int count) {
    if (count < 0 || size_ + count > static_cast<int>(queue_.size())) {
        Fail(SafetyFault::QueueOverflow);
        return false;
    }
    for (int i = size_ - 1; i >= 0; --i) {
        queue_[i + count] = queue_[i];
    }
    for (int i = 0; i < count; ++i) {
        queue_[i] = steps[i];
    }
    size_ += count;
    return true;
}

void LaunchSequence::Fail(SafetyFault fault) {
    size_ = 0;
    has_current_ = false;
    decocking_ = false;
    cocking_ = false;
    fetch_test_ = false;
    release_window_ = false;
    fault_ = fault;
    bottom_found_ = false;
    push_started_ = false;
    push_start_ = 0;
    bottom_window_start_ = -1;
    label_ = Safety::Message(fault);
    const SafetyState state = io_.State();
    if (prefetch_held_side_ != 0) { io_.SetCrank(state.crank); }
    prefetch_supported_side_ = 0;
    io_.StopMotors(Safety::SliderMustHold(state));
}

bool LaunchSequence::CommandCock() {
    // 当前比赛阶段不许上膛（R50）时直接拒绝，不报故障；适配层和比赛流程各有一道，这里是第三道。
    if (!Accept() || !io_.State().launch_allowed) {
        return false;
    }
    const Step step{"上膛", Action::Cock, Condition::Immediate};
    const bool added = Append(&step, 1);
    cocking_ = added;
    return added;
}

bool LaunchSequence::CommandDecock() {
    if (!Accept() || io_.State().latch_state != LatchState::Latched) {
        return false;
    }
    const Step steps[]{
        {"退膛：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
        {"退膛：滑块慢压", Action::Press, Condition::Pressed, kPressSearchLimit, kPressTimeout},
        {"退膛：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout},
        {"退膛：滑块慢慢回位", Action::Rise, Condition::Parked, kSliderPark, kDecockTimeout}
    };
    const bool added = Append(steps, 4);
    decocking_ = added;
    return added;
}

bool LaunchSequence::CommandLoad() {
    if (!Accept() || !io_.State().launch_allowed) {
        return false;
    }
    const Step step{"装填", Action::Load, Condition::Immediate};
    if (!Append(&step, 1)) {
        return false;
    }
    if (io_.State().latch_state == LatchState::Unknown) {
        ProbeSteps();
    }
    return fault_ == SafetyFault::None;
}

bool LaunchSequence::CommandLoadFromFetch() {
    if (!CommandLoad()) {
        return false;
    }
    if (fetch_holding_) {
        inventory_.ResetHeld(fetch_held_side_);
        fetch_holding_ = false;
    }
    return true;
}

bool LaunchSequence::CommandFire() {
    if (!Accept()) {
        return false;
    }
    const SafetyFault blocker = Safety::FirePreparationBlocker(Snapshot());
    if (blocker != SafetyFault::None) {
        Fail(blocker);
        return false;
    }
    FireSteps(false);
    return fault_ == SafetyFault::None;
}

bool LaunchSequence::CommandSalvo(int count) {
    if (!Accept() || count < 1 || count > 4 || !io_.State().launch_allowed) {
        return false;
    }
    const SafetyState state = io_.State();
    if (state.latch_state == LatchState::Unknown ||
        (state.latch_state != LatchState::Latched && inventory_.Seated())) {
        const Step step{"上膛", Action::Cock, Condition::Immediate};
        if (!Append(&step, 1)) {
            return false;
        }
    }
    static constexpr std::array<const char *, 4> names{{"第 1 发", "第 2 发", "第 3 发", "第 4 发"}};
    for (int i = 0; i < count; ++i) {
        const Step step{names[i], Action::One, Condition::Immediate};
        if (!Append(&step, 1)) {
            return false;
        }
    }
    return true;
}

void LaunchSequence::ProbeSteps() {
    // 从顶端按探测限速走完整段需要超过 8 s，超时按全行程时间加回位余量计算。
    const Step step{"探测", Action::Probe, Condition::Probe, kProbePosition, kProbeTimeout};
    Prepend(&step, 1);
}

void LaunchSequence::ExpandCock() {
    const LatchState state = io_.State().latch_state;
    if (state == LatchState::Unknown) {
        const Step retry{"上膛", Action::Cock, Condition::Immediate};
        if (Prepend(&retry, 1)) {
            ProbeSteps();
        }
        return;
    }
    if (state == LatchState::Latched) {
        if (io_.SliderAt(kSliderPark, kSliderTolerance, kSliderVelocityTolerance)) {
            return;
        }
        const Step step{"上膛：滑块回位", Action::Slider, Condition::Parked,
                        kSliderPark, kReturnTimeout};
        Prepend(&step, 1);
        return;
    }
    const Step steps[]{
        {"上膛：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout},
        {"上膛：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
        {"上膛：滑块慢压", Action::Press, Condition::Pressed, kPressSearchLimit, kPressTimeout},
        {"上膛：锁止", Action::Lock, Condition::Lock, kLockClosed, kServoTimeout},
        {"上膛：锁止确认", Action::LatchCheck, Condition::LatchCheck, 0, kPressTimeout},
        {"上膛：滑块回位", Action::Slider, Condition::Parked, kSliderPark, kReturnTimeout}
    };
    Prepend(steps, 6);
}

void LaunchSequence::LoadSteps() {
    const bool from_right = inventory_.HeldSide() > 0;
    const Step steps[]{
        {"装填：推杆收起", Action::Crank, Condition::Crank, 0, kServoTimeout},
        {from_right ? "装填：摆臂回 +1.5°" : "装填：摆臂回 +2.0°", Action::Arm, Condition::Arm,
         Radians(from_right ? 1.5 : 2.0), kArmTimeout},
        {"装填：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
        {"装填：滑块把发射机构带下去", Action::Press, Condition::Pressed, kPressSearchLimit, kPressTimeout},
        {"装填：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout},
        {"装填：慢慢抬升到磁铁正下方", Action::Rise, Condition::Loaded, kLoadPosition, kPressTimeout},
        {"装填：推杆下放", Action::Crank, Condition::Crank, kCrankSeat, kServoTimeout},
        {"装填：等待推杆压到位", Action::Wait, Condition::Wait, kLoadCrankSettleTime, kLoadCrankSettleTime + 1},
        {"装填：磁铁通电消磁", Action::Release, Condition::MagnetWait, 0, 1},
        {"装填：推杆收起", Action::Crank, Condition::Crank, 0, kServoTimeout},
        {"装填：滑块下到锁止位上方", Action::Slider, Condition::Approach, kProbePosition, kPressTimeout},
        {"装填：压下锁止", Action::Press, Condition::Pressed, kPressSearchLimit, kPressTimeout},
        {"装填：锁止", Action::Lock, Condition::Lock, kLockClosed, kServoTimeout},
        {"装填：锁止确认", Action::LatchCheck, Condition::LatchCheck, 0, kPressTimeout},
        {"装填：滑块回位", Action::Slider, Condition::Parked, kSliderPark, kReturnTimeout}
    };
    Prepend(steps, 15);
}

bool LaunchSequence::CommandFetchTest() {
    // 库存记着磁铁上有镖、或者上一发取上来还没放镖时，不能去镖座下探，会撞上镖座里的镖。
    if (!Accept() || inventory_.Held() || fetch_holding_) {
        return false;
    }
    const Step start[]{
        {"取镖试运行：磁铁断电吸持", Action::Magnet, Condition::Immediate, 0},
        {"取镖试运行：推杆收起", Action::Crank, Condition::Crank, kCrankRetracted, kServoTimeout}
    };
    fetch_test_ = Append(start, 2) && FetchTestSteps(fetch_side_);
    return fetch_test_;
}

bool LaunchSequence::FetchTestSteps(int side) {
    const double arm = side < 0 ? kFetchLeft : kFetchRight;
    const double crank = side < 0 ? kCrankFetchLeft : kCrankFetchRight;
    // 吸住镖以后按 kCrankLiftSpeed 慢慢抬，超时按抬完的时间加上平常推杆步骤的余量。
    const double lift_timeout = kServoTimeout + crank / kCrankLiftSpeed;
    const Step steps[]{
        {side < 0 ? "取镖试运行：摆到左侧镖座" : "取镖试运行：摆到右侧镖座", Action::Arm, Condition::Arm, arm, kFetchArmTimeout},
        {side < 0 ? "取镖试运行：左侧下探" : "取镖试运行：右侧下探", Action::Crank, Condition::Crank, crank, kServoTimeout},
        {side < 0 ? "取镖试运行：左侧等磁铁吸稳" : "取镖试运行：右侧等磁铁吸稳", Action::FetchGrip, Condition::Wait,
         kFetchGripTime, kFetchGripTime + 1, side},
        {side < 0 ? "取镖试运行：左侧慢慢抬起" : "取镖试运行：右侧慢慢抬起", Action::Crank, Condition::Crank,
         kCrankRetracted, lift_timeout, 0, kCrankLiftSpeed}
    };
    return Append(steps, 4);
}

void LaunchSequence::FetchSteps(int side, double park) {
    const Step steps[]{
        {"取镖：磁铁断电吸持", Action::Magnet, Condition::Immediate, 0},
        {"取镖：推杆收起", Action::Crank, Condition::Crank, 0, kServoTimeout},
        {side > 0 ? "取镖：摆到右侧镖座" : "取镖：摆到左侧镖座", Action::Arm,
         Condition::Arm, side > 0 ? kFetchRight : kFetchLeft, kArmTimeout},
        {"取镖：下探", Action::Crank, Condition::Crank, side > 0 ? kCrankFetchRight : kCrankFetchLeft, kServoTimeout},
        {"取镖：等磁铁吸稳", Action::Grip, Condition::Wait, kFetchGripTime, kFetchGripTime + 1, side},
        {"取镖：抬起", Action::Crank, Condition::Crank, 0,
         kServoTimeout + (side > 0 ? kCrankFetchRight : kCrankFetchLeft) / kCrankLiftSpeed, 0, kCrankLiftSpeed},
        {park != 0 ? "取镖：摆到待命位" : "取镖：摆回 0°", Action::Arm,
         Condition::Arm, park, kArmTimeout}
    };
    Prepend(steps, 7);
}

void LaunchSequence::LiftPrefetch(int side, double park) {
    const double crank = side > 0 ? kCrankFetchRight : kCrankFetchLeft;
    const Step steps[]{
        {"取镖：抬起", Action::LiftPrefetch, Condition::Crank, 0,
         kServoTimeout + crank / kCrankLiftSpeed, 0, kCrankLiftSpeed},
        {park != 0 ? "取镖：摆到待命位" : "取镖：摆回 0°", Action::Arm,
         Condition::Arm, park, kArmTimeout}
    };
    Prepend(steps, 2);
}

void LaunchSequence::FireSteps(bool front) {
    const Step steps[]{
        {"发射准备", Action::Standby, Condition::Immediate},
        {"瞄准：等 yaw / pitch 到位", Action::Aim, Condition::Aimed, 0, kAimTimeout},
        {"发射：解锁", Action::Unlock, Condition::Lock, kLockOpen, 1.5},
        {"飞行", Action::Flight, Condition::FlightWait, 0, kReturnTimeout}
    };
    if (front) {
        Prepend(steps, 4);
    } else {
        Append(steps, 4);
    }
}

void LaunchSequence::ExpandOne() {
    if (inventory_.Remaining() == 0) {
        return;
    }
    FireSteps(true);
    if (fault_ != SafetyFault::None) {
        return;
    }
    const Step steps[]{
        {"装填", Action::Load, Condition::Immediate},
        {"预取下一发", Action::Prefetch, Condition::Immediate}
    };
    Prepend(steps, 2);
}

void LaunchSequence::ExpandLoad() {
    const SafetyState state = io_.State();
    if (state.latch_state == LatchState::Unknown) {
        const Step retry{"装填", Action::Load, Condition::Immediate};
        if (Prepend(&retry, 1)) {
            ProbeSteps();
        }
        return;
    }
    if (inventory_.Seated() && state.latch_state == LatchState::Latched) {
        return;
    }
    if (inventory_.Seated() && state.latch_state != LatchState::Latched) {
        const Step step{"上膛", Action::Cock, Condition::Immediate};
        Prepend(&step, 1);
        return;
    }
    if (inventory_.Held()) {
        if (prefetch_held_side_ != 0) {
            const Step retry{"装填", Action::Load, Condition::Immediate};
            if (Prepend(&retry, 1)) { LiftPrefetch(prefetch_held_side_, 0); }
            return;
        }
        LoadSteps();
        return;
    }
    const int side = inventory_.NextHolder();
    if (side != 0) {
        const Step retry{"装填", Action::Load, Condition::Immediate};
        if (Prepend(&retry, 1)) {
            FetchSteps(side, 0);
        }
    } else if (!inventory_.Seated()) {
        Fail(SafetyFault::NoDarts);
    }
}

void LaunchSequence::ExpandPrefetch() {
    const int side = inventory_.NextHolder();
    if (inventory_.Held() && prefetch_held_side_ != 0) {
        LiftPrefetch(prefetch_held_side_, prefetch_held_side_ * kStandby);
        return;
    }
    if (!inventory_.Held() && side != 0) {
        const Step steps[]{
            {"取镖：磁铁断电吸持", Action::Magnet, Condition::Immediate, 0},
            {"取镖：推杆收起", Action::Crank, Condition::Crank, 0, kServoTimeout},
            {side > 0 ? "取镖：摆到右侧镖座" : "取镖：摆到左侧镖座", Action::Arm,
             Condition::Arm, side > 0 ? kFetchRight : kFetchLeft, kArmTimeout},
            {"取镖：下探", Action::Crank, Condition::Crank, side > 0 ? kCrankFetchRight : kCrankFetchLeft, kServoTimeout},
            {"取镖：等磁铁吸稳", Action::PrefetchGrip, Condition::Wait, kFetchGripTime, kFetchGripTime + 1, side}
        };
        Prepend(steps, 5);
    }
}

void LaunchSequence::ExpandStandby() {
    if (prefetch_supported_side_ != 0) { return; }
    if (prefetch_held_side_ != 0) {
        LiftPrefetch(prefetch_held_side_, prefetch_held_side_ * kStandby);
        return;
    }
    const SafetyState state = io_.State();
    Step steps[2];
    int count = 0;
    if (std::fabs(state.crank) >= Radians(2)) {
        steps[count++] = {"发射：推杆收起", Action::Crank, Condition::Crank, 0, kServoTimeout};
    }
    if (inventory_.Held() && std::fabs(state.arm) < kStandby - Radians(1)) {
        const int side = inventory_.HeldSide() == 0 ? 1 : inventory_.HeldSide();
        steps[count++] = {"发射：吸着的镖摆开", Action::Arm, Condition::Arm,
                          side * kStandby, kArmTimeout};
    }
    Prepend(steps, count);
}

void LaunchSequence::Enter() {
    switch (current_.action) {
    case Action::Magnet:
        io_.SetMagnet(current_.target > 0.5);
        break;
    case Action::Wait:
        break;
    case Action::FetchGrip:
        // 推杆已经压到镖上、磁铁断电吸持：从这一步起磁铁上可能有镖，操作手放镖以前不能再去镖座。
        fetch_holding_ = true;
        fetch_held_side_ = current_.side;
        break;
    case Action::One:
        ExpandOne();
        break;
    case Action::Cock:
        ExpandCock();
        break;
    case Action::Load:
        ExpandLoad();
        break;
    case Action::Prefetch:
        ExpandPrefetch();
        break;
    case Action::Standby:
        ExpandStandby();
        break;
    case Action::Arm:
        io_.SetArm(current_.target);
        break;
    case Action::Crank:
    case Action::LiftPrefetch:
        if (current_.action == Action::LiftPrefetch) { prefetch_supported_side_ = 0; }
        io_.SetCrank(current_.target, current_.speed);
        break;
    case Action::Lock:
        if (current_.target == kLockOpen && !Safety::LockOpenAllowed(Snapshot())) {
            Fail(SafetyFault::NotLatched);
            break;
        }
        io_.SetLock(current_.target);
        io_.RecordLatched(current_.target == kLockOpen ? LatchState::Released : LatchState::Unknown);
        hold_current_sum_ = 0;
        hold_current_count_ = 0;
        break;
    case Action::LatchCheck: {
        const SafetyState state = io_.State();
        latch_check_target_ = Clamp(state.slider + kLatchCheckRise, kSliderBottom, kSliderTop);
        latch_hold_start_ = -1;
        io_.SetSlider(latch_check_target_, kLatchCheckSpeed);
        break;
    }
    case Action::Probe:
        probe_hold_start_ = -1;
        probe_result_ = LatchState::Unknown;
        io_.SetSlider(current_.target, kProbeSpeed);
        break;
    case Action::Slider:
        io_.SetSlider(current_.target);
        break;
    case Action::Press:
        bottom_found_ = false;
        push_started_ = false;
        push_start_ = 0;
        bottom_window_start_ = -1;
        bottom_position_ = 0;
        lock_position_ = 0;
        io_.SetSlider(current_.target, kPressSpeed);
        break;
    case Action::Rise:
        io_.SetSlider(current_.target, kLoadRiseSpeed);
        break;
    case Action::Grip:
    case Action::PrefetchGrip:
        if (!inventory_.Grip(current_.side)) {
            Fail(SafetyFault::NoDarts);
        } else if (current_.action == Action::PrefetchGrip) {
            prefetch_held_side_ = current_.side;
        }
        break;
    case Action::Release:
        io_.SetMagnet(true);
        // 下一步收推杆；收起以前磁铁要一直通电，否则会把镖重新吸住带走。
        release_window_ = true;
        if (!inventory_.Release()) {
            Fail(SafetyFault::NoDarts);
        }
        break;
    case Action::Unlock: {
        const SafetyFault blocker = Safety::FireBlocker(Snapshot());
        if (blocker != SafetyFault::None) {
            Fail(blocker);
            break;
        }
        io_.SetLock(kLockOpen);
        io_.RecordLatched(LatchState::Released);
        // 发射记录：编号接着上一发，记下解锁这一拍的 pitch 和最近一次确认锁住时的数据。
        const std::uint32_t number = last_shot_.number + 1;
        last_shot_ = latch_record_;
        last_shot_.number = number;
        last_shot_.pitch = io_.State().pitch;
        last_shot_.wait = latch_record_.source == LatchSource::None ? 0 : last_time_ - latch_time_;
        latch_record_ = ShotRecord{};
        break;
    }
    case Action::Aim:
    case Action::Flight:
        break;
    }
}

bool LaunchSequence::Done() {
    switch (current_.condition) {
    case Condition::Wait:
        return elapsed_ >= current_.target;
    case Condition::Immediate:
        return true;
    case Condition::Arm:
        return io_.ArmAt(current_.target);
    case Condition::Crank:
        // 斜坡按 kServoSpeed 估，TD-8130MG 实际更慢，至少等 kCrankSettleTime 再看推杆到位。
        if (elapsed_ < kCrankSettleTime) {
            return false;
        }
        return io_.CrankAt(current_.target);
    case Condition::Lock:
        // 合上夹爪这一步，滑块在锁止位托着发射机构；累计托住的电流，记进这一发的发射记录。
        if (current_.target == kLockClosed && bottom_found_ &&
            io_.SliderAt(lock_position_, 0.001, kSliderVelocityTolerance) &&
            std::isfinite(io_.SliderCurrent())) {
            hold_current_sum_ += std::fabs(io_.SliderCurrent());
            ++hold_current_count_;
        }
        // 斜坡很快就走完，锁止舵机实际更慢，至少等 kLockSettleTime 再进下一步。
        if (elapsed_ < kLockSettleTime) {
            return false;
        }
        if (!io_.LockAt(current_.target)) {
            return false;
        }
        if (current_.target == kLockClosed) {
            if (!bottom_found_ ||
                !io_.SliderAt(lock_position_, 0.001, kSliderVelocityTolerance)) {
                return false;
            }
        }
        return true;
    case Condition::LatchCheck:
        if (!io_.SliderAt(latch_check_target_, kSliderTolerance, kSliderVelocityTolerance)) {
            latch_hold_start_ = -1;
            return false;
        }
        if (!std::isfinite(io_.SliderCurrent()) ||
            std::fabs(io_.SliderCurrent()) > kLatchCheckCurrent) {
            Fail(SafetyFault::LatchFailed);
            return false;
        }
        if (latch_hold_start_ < 0) {
            latch_hold_start_ = elapsed_;
            check_current_max_ = 0;
        }
        check_current_max_ = std::fmax(check_current_max_, std::fabs(io_.SliderCurrent()));
        if (elapsed_ - latch_hold_start_ < kLatchCheckHold) {
            return false;
        }
        // 记下这次锁住的数据，正式发射解锁时拷进发射记录。
        latch_record_ = ShotRecord{};
        latch_record_.source = LatchSource::Press;
        latch_record_.bottom = bottom_position_;
        latch_record_.hold_current = hold_current_count_ > 0 ? hold_current_sum_ / hold_current_count_ : 0;
        latch_record_.check_current = check_current_max_;
        latch_time_ = last_time_;
        io_.RecordLatched(LatchState::Latched);
        inventory_.Latched();
        return true;
    case Condition::Probe: {
        if (!io_.SliderAt(current_.target, kSliderTolerance, kSliderVelocityTolerance)) {
            probe_hold_start_ = -1;
            probe_result_ = LatchState::Unknown;
            return false;
        }
        const double current = io_.SliderCurrent();
        if (!std::isfinite(current)) {
            Fail(SafetyFault::ProbeFailed);
            return false;
        }
        const LatchState result = std::fabs(current) > kLatchCheckCurrent ?
            LatchState::Released : LatchState::Latched;
        if (probe_result_ != LatchState::Unknown && probe_result_ != result) {
            Fail(SafetyFault::ProbeFailed);
            return false;
        }
        probe_result_ = result;
        if (probe_hold_start_ < 0) {
            probe_hold_start_ = elapsed_;
            check_current_max_ = 0;
        }
        check_current_max_ = std::fmax(check_current_max_, std::fabs(current));
        if (elapsed_ - probe_hold_start_ < kLatchCheckHold) {
            return false;
        }
        io_.RecordLatched(result);
        if (result == LatchState::Latched) {
            inventory_.Latched();
            // 探测判成已锁住：没有慢压找底，只记探测保持窗口里的最大电流。
            latch_record_ = ShotRecord{};
            latch_record_.source = LatchSource::Probe;
            latch_record_.check_current = check_current_max_;
            latch_time_ = last_time_;
        }
        const Step step = result == LatchState::Released ?
            Step{"探测：张开夹爪", Action::Lock, Condition::Lock, kLockOpen, kServoTimeout} :
            Step{"探测：滑块回位", Action::Slider, Condition::Parked, kSliderPark, kReturnTimeout};
        return Prepend(&step, 1);
    }
    case Condition::Approach:
        // 到本步目标位置并停稳。
        return io_.SliderAt(current_.target, kSliderTolerance, kSliderVelocityTolerance);
    case Condition::Pressed:
    {
        const SafetyState state = io_.State();
        const double current = io_.SliderCurrent();
        if (!std::isfinite(state.slider) || !std::isfinite(current)) {
            Fail(SafetyFault::InvalidInput);
            return false;
        }
        if (current > kLatchCheckCurrent) {
            if (!push_started_) {
                push_start_ = state.slider;
                push_started_ = true;
            }
        } else {
            push_started_ = false;
        }
        if (!bottom_found_) {
            if (current >= kBottomCurrent) {
                if (bottom_window_start_ < 0) {
                    bottom_window_start_ = elapsed_;
                    bottom_window_position_ = state.slider;
                } else if (std::fabs(state.slider - bottom_window_position_) > kBottomStill) {
                    bottom_window_start_ = elapsed_;
                    bottom_window_position_ = state.slider;
                } else if (elapsed_ - bottom_window_start_ >= kBottomTime) {
                    if (!push_started_ || push_start_ - state.slider < kMinPushTravel) {
                        Fail(SafetyFault::PressBlocked);
                        return false;
                    }
                    bottom_position_ = state.slider;
                    lock_position_ = bottom_position_ + kCockLift;
                    bottom_found_ = true;
                    io_.SetSlider(lock_position_, kPressSpeed);
                }
            } else {
                bottom_window_start_ = -1;
            }
            if (!bottom_found_ && io_.SliderAt(kPressSearchLimit, kSliderTolerance,
                                               kSliderVelocityTolerance)) {
                Fail(SafetyFault::NoBottom);
                return false;
            }
        }
        return bottom_found_ && io_.SliderAt(lock_position_, 0.001,
                                              kSliderVelocityTolerance);
    }
    case Condition::Parked:
        return io_.SliderAt(kSliderPark, kSliderTolerance, kSliderVelocityTolerance);
    case Condition::Loaded:
        return io_.SliderAt(kLoadPosition, kSliderTolerance, kSliderVelocityTolerance);
    case Condition::MagnetWait:
        return elapsed_ > kMagnetWait;
    case Condition::Aimed:
        return io_.Aimed();
    case Condition::FlightWait:
        if (elapsed_ < kFlightWait) {
            return false;
        }
        if (!inventory_.Fire()) {
            Fail(SafetyFault::NoDarts);
            return false;
        }
        ++shots_;
        if (prefetch_supported_side_ != 0) {
            LiftPrefetch(prefetch_supported_side_, prefetch_supported_side_ * kStandby);
        }
        return true;
    }
    return false;
}

void LaunchSequence::Tick(double now, bool powered) {
    if (fault_ != SafetyFault::None) {
        return;
    }
    if (!std::isfinite(now) || now < 0 || (last_time_ >= 0 && now < last_time_)) {
        Fail(SafetyFault::InvalidInput);
        return;
    }
    const double dt = last_time_ < 0 ? 0 : now - last_time_;
    last_time_ = now;
    if (!Busy()) {
        label_ = "空闲";
        return;
    }
    const SafetyState state = io_.State();
    if (state.fault != SafetyFault::None) {
        Fail(state.fault);
        return;
    }
    if (state.remote_required && !state.remote_online) {
        Fail(SafetyFault::RemoteOffline);
        return;
    }
    if (!state.motors_online) {
        Fail(SafetyFault::MotorOffline);
        return;
    }
    // 取镖试运行只动摆臂、推杆和磁铁，摆臂建没建零点由适配层在开始前检查。
    if (!state.all_homed && !fetch_test_) {
        Fail(SafetyFault::NotHomed);
        return;
    }
    if (state.competition && !state.referee_online) {
        Fail(SafetyFault::RefereeOffline);
        return;
    }
    if (!powered) {
        // 取镖试运行无力就停：摆臂断力会掉下去，重新上力以后不能在掉下来的位置接着下探。
        if (fetch_test_ || (prefetch_held_side_ != 0 && !release_window_)) {
            Cancel();
            return;
        }
        paused_ = true;
        return;
    }
    if (paused_ && release_window_) {
        // 无力时适配层已让磁铁断电，镖可能又被吸住；接着收推杆会把镖带起来，不自动重放，交给人处理。
        Fail(SafetyFault::ReleaseInterrupted);
        return;
    }
    if (!has_current_) {
        current_ = queue_[0];
        for (int i = 1; i < size_; ++i) {
            queue_[i - 1] = queue_[i];
        }
        --size_;
        has_current_ = true;
        elapsed_ = 0;
        label_ = current_.name;
        ++revision_;
        Enter();
        if (fault_ != SafetyFault::None) {
            return;
        }
    } else if (!paused_) {
        elapsed_ += dt;
    }
    paused_ = false;
    if (Done()) {
        has_current_ = false;
        if (current_.action == Action::PrefetchGrip) { prefetch_supported_side_ = current_.side; }
        if (current_.action == Action::LiftPrefetch) { prefetch_held_side_ = 0; }
        // 放镖后的推杆收起做完，磁铁离开了镖，之后断电吸持不会再带走它。
        if (release_window_ && current_.action == Action::Crank) { release_window_ = false; }
        // 最后一步做完立刻显示空闲，适配层不会再调用 Tick。
        if (!Busy()) {
            label_ = "空闲";
            decocking_ = false;
            cocking_ = false;
            // 取镖试运行做完一发：下一发换另一边。
            if (fetch_test_) { fetch_side_ = -fetch_side_; }
            fetch_test_ = false;
        }
    } else if (fault_ == SafetyFault::None && elapsed_ > current_.timeout) {
        Fail(SafetyFault::Timeout);
    }
}
}
