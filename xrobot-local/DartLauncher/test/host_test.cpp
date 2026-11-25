// 无硬件主机回归：安全逻辑、PID、传动、回零、库存、模式与发射流程。
#include "dart_inventory.hpp"
#include "dart_params.hpp"
#include "dbus_frame.hpp"
#include "dbus_framer.hpp"
#include "launch_sequence.hpp"
#include "magnet_guard.hpp"
#include "match_flow.hpp"
#include "mode_logic.hpp"
#include "motor_axis.hpp"
#include "pid.hpp"
#include "referee_tracker.hpp"
#include "rotor_counter.hpp"
#include "safety.hpp"
#include "servo_ramp.hpp"
#include "slider_pair.hpp"
#include "stall_guard.hpp"
#include "tension_modbus.hpp"
#include "yaw_geometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace dart;
namespace {
int checks = 0;
constexpr double kNan = std::numeric_limits<double>::quiet_NaN();

void Check(bool condition, const char *label) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}

bool Near(double actual, double expected, double tolerance = 1e-9) {
    return std::fabs(actual - expected) <= tolerance;
}

class FakeIo : public LauncherIo {
public:
    FakeIo() {
        state.remote_online = true;
        state.motors_online = true;
        state.all_homed = true;
        state.slider_homed = true;
        state.latch_state = LatchState::Latched;
        state.aimed = true;
        state.slider = kSliderPark;
    }

    void Advance() {
        if (blocked || (press_blocked && press_mode)) {
            return;
        }
        if (!stopped) {
            const bool probing = Near(slider_target, kProbePosition) && slider_speed == kProbeSpeed;
            const bool pressing = press_mode && slider_target < hard_bottom;
            if (probing) {
                state.slider += Clamp(slider_target - state.slider,
                                      -slider_speed * 0.01, slider_speed * 0.01);
                slider_ticks = Near(state.slider, slider_target) ? 0 : 1;
            } else {
                if (slider_ticks > 0 && slider_speed == kPressSpeed) {
                    --slider_ticks;
                    state.slider += Clamp(slider_target - state.slider,
                                          -0.005, 0.005);
                    if (pressing && state.slider <= hard_bottom) {
                        state.slider = hard_bottom;
                        slider_ticks = 0;
                    } else if (slider_ticks == 0) {
                        state.slider = std::max(slider_target, hard_bottom);
                    }
                } else {
                    Move(state.slider, slider_target, slider_ticks);
                }
            }
            Move(state.arm, arm_target, arm_ticks);
        }
        Move(state.crank, crank_target, crank_ticks);
        Move(lock, lock_target, lock_ticks);
    }

    void SetSlider(double position, double speed = 0) override {
        slider_target = position;
        slider_speed = speed;
        if (speed != kPressSpeed) {
            press_mode = false;
        } else if (position < hard_bottom || position <= hard_bottom + kCockLift + 1e-9) {
            press_mode = true;
        } else {
            press_mode = false;
        }
        slider_ticks = speed == kPressSpeed ?
            std::max(3, static_cast<int>(std::ceil(std::fabs(position - state.slider) /
                                                   (speed * 0.01)))) : 3;
        ++slider_commands;
        stopped = false;
    }

    bool SliderAt(double position, double tolerance, double) const override {
        return slider_ticks == 0 && std::fabs(state.slider - position) < tolerance;
    }

    double SliderCurrent() const override {
        return slider_ticks > 0 ? (press_mode ? press_current : motion_current) :
            slider_target < hard_bottom && Near(state.slider, hard_bottom) ? bottom_current : slider_current;
    }

    void SetYaw(double angle) override { yaw = angle; }

    void SetPitch(double position) override { pitch = position; }

    bool Aimed() const override { return state.aimed; }

    void SetArm(double angle) override {
        arm_log.push_back(angle);
        arm_target = angle;
        arm_ticks = 3;
        stopped = false;
    }

    bool ArmAt(double angle) const override { return arm_ticks == 0 && Near(state.arm, angle); }

    void SetCrank(double angle, double speed = 0) override {
        crank_log.push_back(angle);
        crank_speed_log.push_back(speed);
        crank_target = angle;
        crank_ticks = 3;
    }

    bool CrankAt(double angle) const override {
        return crank_ticks == 0 && Near(state.crank, angle);
    }

    void SetLock(double angle) override {
        lock_target = angle;
        lock_ticks = 3;
        ++lock_commands;
        if (angle == kLockOpen && state.latch_state == LatchState::Latched &&
            Near(state.slider, kSliderPark)) {
            ++unlocks;
        }
    }

    bool LockAt(double angle) const override { return lock_ticks == 0 && Near(lock, angle); }

    void RecordLatched(LatchState value) override { state.latch_state = value; }

    void SetMagnet(bool enabled) override {
        magnet_log.push_back(enabled);
        magnet = enabled;
        ++magnet_commands;
    }

    SafetyState State() const override { return state; }

    void StopMotors(bool hold_slider) override {
        stopped = true;
        hold_requested = hold_slider;
        ++stops;
        if (hold_slider) {
            slider_target = state.slider;
            ++hold_stops;
        }
    }

    SafetyState state;
    std::vector<double> arm_log, crank_log, crank_speed_log;
    std::vector<bool> magnet_log;
    double slider_target = kSliderPark;
    double arm_target = 0;
    double crank_target = 0;
    double lock = 0;
    double lock_target = 0;
    double slider_speed = 0;
    double slider_current = 0;
    double motion_current = 0;
    double press_current = kLatchCheckCurrent + 1;
    double hard_bottom = kCockBottom;
    double bottom_current = kSliderCurrentCap;
    double yaw = 0;
    double pitch = 0;
    int slider_ticks = 0;
    int arm_ticks = 0;
    int crank_ticks = 0;
    int lock_ticks = 0;
    int unlocks = 0;
    int lock_commands = 0;
    int stops = 0;
    int hold_stops = 0;
    int slider_commands = 0;
    int magnet_commands = 0;
    bool magnet = false;
    bool stopped = false;
    bool hold_requested = false;
    bool blocked = false;
    bool press_blocked = false;
    bool press_mode = false;

private:
    static void Move(double &value, double target, int &ticks) {
        if (ticks > 0) {
            --ticks;
            if (ticks == 0) {
                value = target;
            }
        }
    }
};

void TestPidAndParams() {
    Check(kSliderInstalled, "sliders are installed for stage 2e");
    Check(kPitchInstalled, "pitch is installed on CAN2 ID 3 since 2026-10-20");
    Check(Near(kPitchBottom, kPitchTop - kPitchTravel + 0.005, 1e-9) && kPitchBottom < kPitchTop, "pitch bottom keeps 5 mm from the far end");
    Check(Near(kPitchTopLimit, kPitchTop - 0.005, 1e-9) && kPitchTopLimit > kPitchBottom, "pitch top soft limit keeps 5 mm under the stop");
    Check(!kYawInstalled, "yaw is not installed for stage 2e");
    Check(!kArmInstalled, "arm GM6020 is temporarily disabled while the reload mechanism is removed");
    Check(kArmZeroCounts < kEncoderRange && kArmMin < kFetchLeft && kFetchLeft < -kStandby && kStandby < kFetchRight &&
              kFetchRight < kArmMax,
          "arm zero is a single-turn count and fetch and standby angles sit inside the soft limits");
    Check(kArmCurrentCap <= 16384 && kArmStallCurrent < kArmCurrentCap &&
              kArmStallCurrent * 3.0 / 16384 < 0.90 &&
              kArmStallCurrent * 3.0 / 16384 * 0.741 < 0.86,
          "arm cap fits the 3 A command range and the stall guard stays under GM6020 continuous stall limits");
    Check(kArmCurrentCap * 3.0 / 16384 * 0.741 >= 2 * 0.263,
          "arm current cap gives at least twice the 0.26 Nm that could not turn the mounted arm (2026-11-11)");
    Check(Near(kArmSpeedGains.max_iout, kArmCurrentCap),
          "arm speed integral may use the whole current cap, so a stalled arm gets the full torque");
    Check(Near(kArmSpeedGains.max_out, kArmCurrentCap) && kArmSpeedGains.kp <= 150 && kArmSpeed <= 2,
          "arm speed gains are sized for the mounted arm (2026-11-12)");
    Check(kArmHoldDeadband > 0 && kArmHoldDeadband < kArmJogLead && kArmHoldDeadband < kServoTolerance &&
              kArmApproachGain > 0 && kArmApproachGain <= kSoftLimitApproachGain &&
              kArmOverspeedRpm > 1 && kArmOverspeedBleedTime > 0,
          "arm jog lead, hold deadband and braking settings are consistent");
    Check(kArmBoostCurrent > kArmStallCurrent && kArmBoostCurrent <= kArmCurrentCap &&
              kArmBoostCurrent * 3.0 / 16384 >= 1.1,
          "a cool arm may hold more than the 1.06 A measured at the right fetch seat (2026-11-12)");
    Check(ArmStallLimit(25) == kArmBoostCurrent && ArmStallLimit(kArmBoostFullTemp) == kArmBoostCurrent &&
              ArmStallLimit(kArmBoostEndTemp) == kArmStallCurrent && ArmStallLimit(90) == kArmStallCurrent &&
              ArmStallLimit(kNan) == kArmStallCurrent && ArmStallLimit(-100) == kArmStallCurrent,
          "the arm stall limit derates to the continuous stall current when hot or unknown");
    bool derating = true;
    for (double temp = kArmBoostFullTemp; temp < kArmBoostEndTemp; temp += 0.5) {
        if (ArmStallLimit(temp + 0.5) > ArmStallLimit(temp)) { derating = false; }
    }
    Check(derating, "the arm stall limit never rises with temperature");
    Check(!kCrankInstalled && kCrankServoCalibrated, "crank servo is temporarily disabled while keeping task 3q calibration pulses");
    Check(kMagnetInstalled, "the magnet relay is enabled for bench testing since 2026-11-11");
    Check(kMagnetOnLimit > kMagnetWait + kServoTimeout + 0.1,
          "the magnet on limit covers the release wait and the crank retract after it");
    Check(kAutoEnabled, "automatic mode is enabled for single shots");
    Check(!kReloadInstalled, "automatic reload stays off while arm and crank are disabled");
    Check(kLockServoCalibrated, "lock servo is calibrated for stage 2k");
    Check(kServoPulseMin <= kLockRawPulseMin && kLockRawPulseMin < kLockRawPulseMax &&
              kLockRawPulseMax <= kServoPulseMax, "raw lock travel is inside servo range");
    Check(kLockRawPulseMin < kLockBootPulseUs && kLockBootPulseUs < kLockRawPulseMax,
          "raw lock boot pulse stays inside the measured travel");
    Check(kLockBootPulseUs >= kServoPulseMin && kLockBootPulseUs <= kServoPulseMax,
          "raw lock boot pulse is within servo range");
    Check(kLockPulseRate > 0, "raw lock pulse rate is positive");
    Check(Near(ServoRamp::PulseUs(kCrankRetracted, kCrankServoOffset, kCrankServoDirection), kCrankPulseRetracted, 1e-6) &&
              Near(ServoRamp::PulseUs(kCrankFetchLeft, kCrankServoOffset, kCrankServoDirection), kCrankPulseFetchLeft, 1e-6) &&
              Near(ServoRamp::PulseUs(kCrankFetchRight, kCrankServoOffset, kCrankServoDirection), kCrankPulseFetchRight, 1e-6) &&
              Near(ServoRamp::PulseUs(kCrankSeat, kCrankServoOffset, kCrankServoDirection), 2150, 1e-6) &&
              Near(ServoRamp::PulseUs(kCrankManualMax, kCrankServoOffset, kCrankServoDirection), kCrankRawPulseMax, 1e-6),
          "calibrated crank angles give the measured retract, fetch and 2150 us load pulses");
    Check(Near(kCrankRetracted, 0) && kCrankFetchLeft > 0 && kCrankFetchLeft < kCrankManualMax &&
              kCrankFetchRight > 0 && kCrankFetchRight < kCrankManualMax,
          "crank fetch angles sit between retracted and the manual limit");
    Check(Near(kCrankManualSpeed * (kServoPulseMax - kServoPulseMin) / kServoRange, kCrankPulseRate, 1e-9),
          "the manual crank moves as fast as the raw pulse mode did");
    Check(kServoPulseMin <= kCrankRawPulseMin && kCrankRawPulseMin < kCrankBootPulseUs &&
              kCrankBootPulseUs < kCrankRawPulseMax && kCrankRawPulseMax <= kServoPulseMax,
          "raw crank boot pulse sits inside the conservative travel inside the servo range");
    Check(Near(kCrankBootPulseUs, (kServoPulseMin + kServoPulseMax) / 2),
          "raw crank boot pulse is the TD-8130MG center");
    Check(kCrankPulseRate > 0, "raw crank pulse rate is positive");
    Check(kMagnetInstalled && kLoadCalibrated && !kReloadInstalled,
          "load calibration parameters stay intact while automatic reload is disabled");
    Check(kCrankSettleTime > 0 && kCrankSettleTime < kServoTimeout,
          "crank settle wait fits inside the crank step timeout");
    Check(kCockEnabled, "cocking is enabled after the lock servo has its own supply");
    Check(Near(kLatchSlider, -0.294, 1e-9), "latch slider follows measured geometry");
    Check(Near(kCockBottom, -0.2990, 1e-9) && Near(kCockPosition, -0.2975, 1e-9),
          "cock position follows the measured hard bottom");
    Check(Near(kTopGap, 0.110, 1e-9) && Near(kSliderTravel, 0.770, 1e-9) && Near(kProbePosition, -0.264, 1e-9), "geometry follows the top bumper");
    Check(Near(kCockPosition - kCockBottom, 0.0015, 1e-9), "cock position leaves 1.5 mm above hard bottom");
    Check(kCockPosition < kLatchSlider, "cock position is below latch position");
    Check(kCockPosition > kCockBottom, "cock position stays above hard bottom");
    Check(Near(kFireSliderMin, 0.361) && Near(kSliderPark, 0.366), "slider parks 4 mm below the bumper and fires from 361 mm");
    Check(kDecockTimeout > (kSliderPark - kCockPosition) / kLoadRiseSpeed,
          "decock timeout covers the slow rise with margin");
    Check(kLatchCheckRise > kLatchSlider - kCockPosition, "latch check rise clears the press depth");
    Check(kCockPosition > kSliderBottom + 0.05, "cock position keeps 50 mm from the slider bottom");
    Check(kLockRawPulseMin < kLockPulseClosed && kLockPulseClosed < kLockPulseOpen &&
              kLockPulseOpen < kLockRawPulseMax, "calibrated lock pulses stay inside the measured travel");
    Check(Near(ServoRamp::PulseUs(kLockClosed, kLockServoOffset, kLockServoDirection), kLockPulseClosed, 1e-6) &&
              Near(ServoRamp::PulseUs(kLockOpen, kLockServoOffset, kLockServoDirection), kLockPulseOpen, 1e-6),
          "calibrated lock pulses match the measured values");
    Check(std::abs(kSliderCurrentCap - 10326) <= 1, "slider current cap");
    Check(Near(kSliderLoadMargin, 0.9) && Near(kLoadMargin, 0.8), "only the slider load margin is raised");
    Check(std::abs(kYawCurrentCap - 2829) <= 1, "yaw current cap");
    Check(std::abs(kPitchCurrentCap - 1738) <= 1, "pitch current cap");
    Check(Near(kPitchScrewForce, 6000.0) && Near(kPitchScrewForce * kPitchLoadMargin, 2000.0, 1e-6) && Near(kLoadMargin, 0.8) && Near(kPitchHomeCurrentFraction, 0.5, 1e-9), "pitch uses a third of its 6000 N screw rating and homes at half of it");
    Check(Near(kPitchTopForce, 1000.0) && std::abs(static_cast<int>(kPitchCurrentCap * kPitchHomeCurrentFraction) - 869) <= 1, "pitch pushes its top stop with at most 1000 N");
    Check(Near(kPitchHomeStallTime, 1.0) && Near(kHomeStallTime, 0.2), "only the pitch waits longer to start homing");
    Check(Near(kPitchHomeSpeed, 0.005) && Near(kHomeSpeed, 0.01) && Near(kPitchHomeSeekLimit, kPitchTravel + 0.010, 1e-9) && kPitchHomeSeekLimit / kPitchHomeSpeed < kHomeTimeout, "pitch homes at half speed and gives up after its full travel before the home timeout");
    Pid pid({2, 0.5, 3, 10, 2});
    const std::array<double, 6> refs{{1, 2, -1, 10, 0, -10}};
    const std::array<double, 6> outputs{{5.5, 8.5, -10, 10, -10, -10}};
    for (std::size_t i = 0; i < refs.size(); ++i) {
        Check(Near(pid.Calc(refs[i], 0), outputs[i]), "PID Python golden output");
    }
    pid.Reset();
    Check(Near(pid.Calc(1, 0), 5.5), "PID reset");
    Check(pid.Calc(kNan, 0) == 0 && Near(pid.Calc(1, 0), 5.5), "PID invalid input clears state");
    Cascade cascade({0.5, 0, 0, 10, 0}, {2, 0.5, 0, 20, 5}, 20);
    Check(cascade.Update(10, 2, 1) == 7, "cascade truncation");
    Check(cascade.Update(10, 2, 1, 30) == 20, "cascade feedforward cap");
    Check(cascade.Update(kNan, 0, 0) == 0, "cascade invalid input");
    Check(kSliderSpeed.max_iout == kSliderCurrentCap && kPitchSpeed.max_out == kPitchCurrentCap,
          "current cap limits PID");
    Check(kArmMoveTolerance > 0 && kArmMoveTolerance < kArmHoldDeadband && kArmMoveCreep > 0 &&
              kArmMoveCreep < kArmAutoSpeed && kArmAutoSpeed <= kArmSpeed && kFetchArmTimeout > kArmTimeout,
          "automatic arm moves are slower than a full stick and stop inside the hold deadband");
    Check(kArmMoveStillBand > 0 && kArmMoveStillBand < kArmMoveTolerance && kArmMoveNear < kServoTolerance &&
              kArmMovePush > kArmMoveCreep && kArmMovePush <= kArmAutoSpeed &&
              kArmMoveSettleTime + kArmMovePushTime < kArmStallTime,
          "a stuck automatic move pushes harder for less than the stall guard time before it gives up");
    Check(kFetchGripTime > 0 && kCrankLiftSpeed > 0 && kCrankLiftSpeed < kServoSpeed &&
              kFetchMaxTemp <= kArmBoostFullTemp,
          "the fetch test grips, lifts slower than a normal crank move and starts only with the full hold current");
}

void TestGeometryAndServos() {
    RotorCounter rotor;
    Check(rotor.Update(8190) && rotor.Update(2) && rotor.Counts() == 8194, "positive rotor wrap");
    Check(rotor.Update(8189) && rotor.Counts() == 8189, "negative rotor wrap");
    rotor.Reset();
    Check(rotor.Update(1) && rotor.Update(8190) && rotor.Counts() == -2, "reverse rotor wrap");
    Check(Near(YawGeometry::Angle(YawGeometry::Slide(Radians(6))), Radians(6)), "yaw roundtrip");
    Check(Near(YawGeometry::Angle(YawGeometry::Slide(Radians(25))), kYawMax), "yaw clamp");
    ServoRamp servo(7);
    Check(servo.SetTarget(1) && Near(servo.Step(0.1), 0.7), "servo rate limiter");
    ServoRamp slow(7);
    Check(slow.SetTarget(1, 2) && Near(slow.Step(0.1), 0.2) && slow.SetTarget(0) && Near(slow.Step(0.1), 0),
          "a servo move can use its own slower speed and the next move goes back to the default");
    Check(Near(ServoRamp::PulseUs(0), 500) && Near(ServoRamp::PulseUs(kServoRange), 2500),
          "servo pulse endpoints");
    Check(Near(ServoRamp::PulseUs(0, Radians(270), -1), 2500) &&
              Near(ServoRamp::PulseUs(Radians(135), Radians(270), -1), 1500),
          "reverse servo applies direction before offset");
    Check(Near(ServoRamp::PulseUs(Radians(90)), 1166.6666666667, 1e-6),
          "forward servo uses 270 degree range");
    Check(Near(ServoRamp::PulseUs(Radians(-90)), 500) &&
              Near(ServoRamp::PulseUs(Radians(360)), 2500), "servo clamps pulse range");
    Check(servo.SetTarget(kServoRange) && Near(servo.Step(1), kServoRange),
          "servo ramp accepts full new range");
    Check(!servo.SetTarget(kNan), "servo rejects invalid target");
    SliderPair pair;
    Check(pair.SetTarget(1) && Near(pair.Target(), kSliderTop), "paired target soft limit");
    Check(pair.Update(0, 0.0049) && !pair.Update(0, 0.0051) && pair.Fault(),
          "slider mismatch fault");
}

AxisConfig SliderConfig() {
    return {kM3508Ratio / kBeltRadius * kEncoderRange / (2 * kPi),
            kSliderBottom, kSliderTop, kSliderTop, kSliderHomeSpeed, kHomeRetreat, 0.0002,
            1, kSliderCurrentCap, 16384, kSliderPos, kSliderSpeed};
}

void FinishHome(MotorAxis &axis) {
    axis.Feedback(12345, 0);
    Check(axis.StartHome(), "start home with feedback");
    for (int i = 0; i < 10000 && axis.State() == HomeState::Seeking; ++i) {
        axis.Feedback(12345, 0);
        Check(std::abs(axis.Update(0.001, true, true)) <= static_cast<int>(kSliderCurrentCap * 0.3),
              "home current cap");
    }
    Check(axis.State() == HomeState::Retreating && !axis.Homed(), "stall enters retreat");
    axis.Feedback(axis.PositionToCounts(axis.Target()), 0);
    Check(axis.Update(0.001, true, true) == 0 && axis.Homed(), "normal position loop completes retreat");
}

void TestAxis() {
    Check(kSliderTravel / kSliderHomeSpeed < kHomeTimeout / 2, "slider home fits half timeout");
    auto config = SliderConfig();
    MotorAxis default_limit(config);
    Check(default_limit.CurrentLimit() == static_cast<int>(config.current_limit * kHomeCurrentFraction),
          "unhomed axis defaults to the reduced current limit");
    config.home_current_fraction = 1.0;
    MotorAxis full_limit(config);
    Check(full_limit.CurrentLimit() == config.current_limit,
          "unhomed axis can use its full current limit");
    MotorAxis startup(SliderConfig());
    Check(startup.Update(0.001, true, false) == 0 && startup.State() == HomeState::Idle,
          "startup offline remains recoverable");
    startup.Feedback(0, 0);
    Check(startup.StartHome(), "startup axis accepts home after feedback");

    MotorAxis axis(SliderConfig());
    axis.Feedback(0, 0);
    Check(!axis.SetTarget(0.1), "unhomed axis rejects position target");
    Check(!axis.Jog(-0.01) && axis.Jog(0.01), "unhomed axis only jogs toward home");
    MotorAxis reversed(SliderConfig());
    reversed.Feedback(0, 0);
    Check(reversed.Jog(0.01) && !reversed.Jog(-0.01) && reversed.Update(0.001, true, true) == 0,
          "rejected reverse jog stops previous jog");

    // 没回零时点动堵转，速度环积分会涨满；松手后误差为 0，积分不会自己退，输出就一直顶在上限。
    MotorAxis wound(SliderConfig());
    wound.Feedback(0, 0);
    Check(wound.Jog(0.01), "unhomed axis jogs toward home");
    int stalled = 0;
    for (int i = 0; i < 200; ++i) {
        wound.Feedback(0, 0);
        stalled = wound.Update(0.001, true, true);
    }
    Check(stalled == wound.CurrentLimit(), "stalled unhomed jog reaches the current limit");
    Check(wound.Jog(0) && wound.Update(0.001, true, true) == 0,
          "unhomed jog release clears the wound-up speed integral");

    // 每条轴可以单独放宽判堵转的时间：pitch 满电流下起步慢，0.2 s 会在原地误判成顶到头。
    auto patient_config = SliderConfig();
    patient_config.home_stall_time = 1.0;
    MotorAxis patient(patient_config);
    patient.Feedback(12345, 0);
    Check(patient.StartHome(), "axis with its own stall time starts home");
    int patient_ticks = 0;
    for (; patient_ticks < 500; ++patient_ticks) {
        patient.Feedback(12345, 0);
        patient.Update(0.001, true, true);
    }
    Check(patient.State() == HomeState::Seeking, "axis still seeks before its own stall time");
    for (; patient_ticks < 1100 && patient.State() == HomeState::Seeking; ++patient_ticks) {
        patient.Feedback(12345, 0);
        patient.Update(0.001, true, true);
    }
    Check(patient.State() == HomeState::Retreating,
          "axis waits its own home stall time before taking the stop");

    auto limited_config = SliderConfig();
    limited_config.home_seek_limit = 0.010;
    MotorAxis limited(limited_config);
    limited.Feedback(12345, 0);
    const bool limited_started = limited.StartHome();
    const double home_start = limited.Position();
    const double seek_step = limited_config.home_direction * kSliderHomeSpeed * limited_config.counts_per_unit * 0.001;
    const double seek_rpm = limited_config.home_direction * kSliderHomeSpeed * limited_config.counts_per_unit * 60 / kEncoderRange;
    double seek_counts = 12345;
    for (int i = 0; i < 300; ++i) {
        seek_counts += seek_step;
        limited.Feedback(seek_counts, seek_rpm);
        limited.Update(0.001, true, true);
    }
    Check(limited_started && Near(limited.Position() - home_start, 0.009, 1e-9) &&
              limited.State() == HomeState::Seeking && !limited.HomeOverrun(),
          "homing within its seek limit keeps seeking without an overrun");
    int overrun_command = 0;
    for (int i = 300; i < 400 && limited.State() == HomeState::Seeking; ++i) {
        seek_counts += seek_step;
        limited.Feedback(seek_counts, seek_rpm);
        overrun_command = limited.Update(0.001, true, true);
    }
    Check(limited.State() == HomeState::Failed && limited.HomeOverrun() && overrun_command == 0 &&
              limited.Position() - home_start > limited_config.home_seek_limit &&
              limited.Position() - home_start < 0.012,
          "homing that runs past its seek limit without a stall latches an overrun");
    limited.ClearFault();
    Check(!limited.HomeOverrun() && limited.State() == HomeState::Idle,
          "clearing a homing overrun returns the axis to idle");

    MotorAxis unlimited(SliderConfig());
    unlimited.Feedback(12345, 0);
    const bool unlimited_started = unlimited.StartHome();
    seek_counts = 12345;
    for (int i = 0; i < 401; ++i) {
        seek_counts += seek_step;
        unlimited.Feedback(seek_counts, seek_rpm);
        unlimited.Update(0.001, true, true);
    }
    Check(unlimited_started && unlimited.State() == HomeState::Seeking && !unlimited.HomeOverrun() &&
              unlimited.Position() - home_start > 0.012,
          "axes without a seek limit keep homing as before");

    auto release_config = SliderConfig();
    release_config.hold_on_release = true;
    MotorAxis release(release_config);
    FinishHome(release);
    release.Feedback(release.PositionToCounts(0.25), 0);
    release.Jog(0.05);
    release.Update(0.001, true, true);
    release.Jog(0);
    Check(Near(release.Target(), 0.25) && release.Settled(kPitchTolerance, kPitchVelocityTolerance),
          "jog release holds the release position and counts as settled");
    release.Feedback(release.PositionToCounts(0.252), 0);
    release.Jog(0);
    Check(Near(release.Target(), 0.25) && release.Update(0.001, true, true) < 0,
          "repeated zero jogs keep the first release target and pull back against drift");

    MotorAxis speed_release(SliderConfig());
    FinishHome(speed_release);
    speed_release.Feedback(speed_release.PositionToCounts(0.25), 0);
    speed_release.Jog(0.05);
    speed_release.Update(0.001, true, true);
    speed_release.Jog(0);
    Check(speed_release.Update(0.001, true, true) == 0 &&
              !speed_release.Settled(kPitchTolerance, kPitchVelocityTolerance),
          "axes without hold_on_release keep the old zero-speed jog release");

    MotorAxis unhomed_hold(SliderConfig());
    unhomed_hold.Feedback(0, 0);
    Check(!unhomed_hold.HoldPosition(), "unhomed axis refuses to hold its position");
    release.Feedback(release.PositionToCounts(kSliderTop + 0.001), 0);
    Check(release.HoldPosition() && Near(release.Target(), kSliderTop),
          "holding past a soft limit holds at the limit");

    FinishHome(axis);
    axis.Feedback(axis.PositionToCounts(kSliderPark), 0);
    Check(axis.SetTarget(kSliderPark), "homed axis accepts target");
    Check(axis.Update(0.001, true, false) == 0 && !axis.Homed() && axis.State() == HomeState::Idle,
          "homed axis becomes unhomed on offline");
    axis.Feedback(axis.PositionToCounts(kSliderPark), 0);
    Check(axis.StartHome(), "reconnected axis can rehome");

    MotorAxis lagged(SliderConfig());
    lagged.Feedback(12345, 0);
    Check(lagged.StartHome(), "lagged axis starts home");
    for (int i = 0; i < 10000 && lagged.State() == HomeState::Seeking; ++i) {
        lagged.Feedback(12345, 0);
        lagged.Update(0.001, true, true);
    }
    Check(lagged.State() == HomeState::Retreating, "lagged axis enters retreat");
    const double retreat_target = lagged.Target();
    double counts = lagged.PositionToCounts(lagged.Position());
    double rpm = 0;
    double peak_overshoot = 0;
    bool crossed_target = false;
    for (int i = 0; i < 2000 && lagged.State() == HomeState::Retreating; ++i) {
        lagged.Feedback(counts, rpm);
        const int command = lagged.Update(0.001, true, true);
        const double target_rpm = static_cast<double>(command) / lagged.CurrentLimit() * 600;
        rpm += (target_rpm - rpm) * 0.1;
        counts += rpm * kEncoderRange / 60 * 0.001;
        if (lagged.Position() <= retreat_target) {
            crossed_target = true;
        }
        if (crossed_target) {
            peak_overshoot = std::max(peak_overshoot, lagged.Position() - retreat_target);
        }
    }
    Check(lagged.State() == HomeState::Homed && peak_overshoot < 0.001,
          "position loop retreats with first-order motor lag");

    MotorAxis failed(SliderConfig());
    failed.MarkFault();
    Check(!failed.StartHome(), "failed axis rejects home");
    failed.ClearFault();
    Check(failed.State() == HomeState::Idle && failed.StartHome() == false,
          "clear fault waits for feedback");
}

void TestAxisHolding() {
    MotorAxis axis(SliderConfig());
    axis.Feedback(0, 0);
    Check(!axis.SetHolding(true), "unhomed axis rejects holding");
    FinishHome(axis);
    axis.Feedback(axis.PositionToCounts(0.1), 0);
    Check(axis.RampTo(0.2, 0.05), "ramp starts before hold");
    axis.Update(0.1, true, true);
    const double saved_target = axis.Target();
    Check(Near(saved_target, 0.105) && axis.SetHolding(true), "hold enters during ramp");
    for (int i = 0; i < 10; ++i) {
        Check(axis.Update(0.1, true, true) == 0 && Near(axis.Target(), saved_target),
              "holding freezes ramp at actual position");
    }
    axis.Feedback(axis.PositionToCounts(0.11), 0);
    Check(axis.SetHolding(true), "repeated holding preserves entry position");
    const int restoring = axis.Update(0.01, true, true);
    Check(restoring < 0 && std::abs(restoring) <= axis.CurrentLimit(),
          "hold restores entry position within current cap");
    axis.Feedback(axis.PositionToCounts(0.1), 0);
    Check(axis.SetHolding(false), "holding exits");
    axis.Update(0.1, true, true);
    Check(Near(axis.Target(), saved_target + 0.005), "ramp resumes saved rate and target");
    for (int i = 0; i < 30; ++i) {
        axis.Update(0.1, true, true);
        axis.Feedback(axis.PositionToCounts(axis.Target()), 0);
    }
    Check(Near(axis.Target(), 0.2) && axis.Settled(0.001, 0.01), "ramp reaches saved endpoint");

    axis.Feedback(axis.PositionToCounts(0.1), 0);
    Check(axis.SetTarget(0.25) && axis.SetHolding(true), "hold enters during direct target");
    Check(axis.Update(0.1, true, true) == 0 && Near(axis.Target(), 0.25),
          "direct target holds actual position and preserves destination");
    axis.Feedback(axis.PositionToCounts(0.12), 0);
    Check(axis.Update(0.1, true, true) < 0, "direct hold restores captured position");
    Check(axis.SetHolding(false) && axis.Update(0.1, true, true) > 0,
          "direct target resumes without reissue");
    axis.Feedback(axis.PositionToCounts(0.25), 0);
    Check(axis.Settled(0.001, 0.01), "direct target reaches original destination");
    Check(axis.SetHolding(true), "hold enters before offline");
    Check(axis.Update(0.001, true, false) == 0 && !axis.SetHolding(true),
          "offline clears hold and requires homing");
}

void TestSafety() {
    SafetyState held;
    held.slider_homed = true;
    held.slider = kSliderPark - 0.02;
    held.latch_state = LatchState::Unknown;
    Check(Safety::SliderMustHold(held), "unlatched supported slider must hold");
    held.latch_state = LatchState::Latched;
    Check(!Safety::SliderMustHold(held), "latched slider may stop");
    held.latch_state = LatchState::Latched;
    held.slider = kSliderPark;
    Check(!Safety::LockOpenAllowed(held), "parked latched lock cannot open");
    held.slider = kCockPosition;
    Check(!Safety::LockOpenAllowed(held), "latched lock at nominal cock position cannot open without bottom snapshot");
    held.at_bottom = true;
    Check(Safety::LockOpenAllowed(held), "bottom snapshot at nominal cock position may open lock");
    held.at_bottom = false;
    held.slider = kCockPosition + 0.0008;
    Check(!Safety::LockOpenAllowed(held), "latched lock above nominal cock position cannot open without bottom snapshot");
    held.at_bottom = true;
    Check(Safety::LockOpenAllowed(held), "bottom snapshot above nominal cock position may open lock");
    held.at_bottom = false;
    held.slider = kLatchSlider;
    Check(!Safety::LockOpenAllowed(held), "latch position alone cannot open lock");
    held.latch_state = LatchState::Released;
    held.slider = kSliderPark;
    Check(Safety::LockOpenAllowed(held), "released lock may open");

    SafetyState good;
    good.remote_online = true;
    good.motors_online = true;
    good.all_homed = true;
    good.latch_state = LatchState::Latched;
    good.seated = true;
    good.arm = kStandby;
    good.aimed = true;
    good.slider = kSliderPark;
    Check(Safety::FireBlocker(good) == SafetyFault::None, "valid launch snapshot");
    for (int side : {-1, 1}) {
        SafetyState low = good;
        low.held = true;
        low.held_side = low.supported_side = side;
        low.arm = side < 0 ? kFetchLeft : kFetchRight;
        low.crank = side < 0 ? kCrankFetchLeft : kCrankFetchRight;
        Check(Safety::FireBlocker(low) == SafetyFault::None,
              "a gripped dart supported at its own holder permits launch");
        low.supported_side = 0;
        Check(Safety::FireBlocker(low) == SafetyFault::CrankNotRetracted,
              "an unfinished grip cannot qualify for supported launch");
        low.supported_side = -side;
        Check(Safety::FireBlocker(low) == SafetyFault::CrankNotRetracted,
              "a supported dart must match its inventory source");
        low.supported_side = side;
        low.arm = side < 0 ? kFetchRight : kFetchLeft;
        Check(Safety::FireBlocker(low) == SafetyFault::CrankNotRetracted,
              "the arm must be at the source holder");
        low.arm = side < 0 ? kFetchLeft : kFetchRight;
        low.crank += 2 * kServoTolerance;
        Check(Safety::FireBlocker(low) == SafetyFault::CrankNotRetracted,
              "the crank must be at the source holder");
    }
    for (double slider : {0.370, 0.367, kSliderPark, kFireSliderMin}) {
        good.slider = slider;
        Check(Safety::FireBlocker(good) == SafetyFault::None,
              "slider at or above 361 mm allows launch through the top");
    }
    for (double slider : {kFireSliderMin - 0.0005, 0.355, kProbePosition, kCockPosition, kNan,
                          std::numeric_limits<double>::infinity(),
                          -std::numeric_limits<double>::infinity()}) {
        good.slider = slider;
        Check(Safety::FireBlocker(good) == SafetyFault::NotParked,
              "slider below 361 mm or nonfinite blocks launch");
    }
    good.slider = kSliderPark;
    good.latch_state = LatchState::Unknown;
    Check(Safety::FireBlocker(good) == SafetyFault::NotLatched, "unknown latch blocks launch");

    Safety standalone;
    Safety competition;
    for (double now : {0.0, kRefereeOfflineTime, kRefereeOfflineTime + 0.001}) {
        // 电机和遥控器始终新鲜，单独验证裁判系统缺席或超时的行为。
        standalone.RemoteFeedback(now);
        competition.RemoteFeedback(now);
        for (int index = 0; index < 5; ++index) {
            standalone.MotorFeedback(index, now);
            competition.MotorFeedback(index, now);
        }
        Check(standalone.Check(now, false) == SafetyFault::None,
              "standalone allows absent referee with fresh motors and remote");
        if (now == 0) {
            Check(competition.Check(now, true) == SafetyFault::RefereeOffline,
                  "competition requires first referee feedback");
            competition.RefereeFeedback(now);
        }
        Check(competition.Check(now, true) == (now <= kRefereeOfflineTime
                  ? SafetyFault::None : SafetyFault::RefereeOffline),
              "competition referee feedback expires only after one second");
    }

    good.latch_state = LatchState::Latched;
    good.competition = false;
    good.referee_online = false;
    good.gate_open = false;
    Check(Safety::FireBlocker(good) == SafetyFault::None,
          "standalone launch ignores absent referee and closed gate");
    good.competition = true;
    Check(Safety::FireBlocker(good) == SafetyFault::RefereeOffline,
          "competition launch blocks absent referee");
    good.referee_online = true;
    Check(Safety::FireBlocker(good) == SafetyFault::GateClosed,
          "competition launch requires open gate");
    good.gate_open = true;
    Check(Safety::FireBlocker(good) == SafetyFault::None,
          "competition launch allows fresh referee and open gate");
}

void TestMode() {
    ModeLogic mode(false);
    RemoteInput remote;
    RefereeInput referee;
    remote.online = true;
    remote.right = Switch::Middle;
    remote.left = Switch::Middle;
    remote.left_x = 0.5;
    remote.right_y = -0.25;
    const ModeCommand command = mode.Update(remote, referee, 0.001);
    Check(command.mode == Mode::Manual && Near(command.slider_rate, -0.20 / 0.95 * kManualSliderSpeed) &&
              Near(command.lock_rate, 0.45 / 0.95 * kServoSpeed),
          "manual slider and lock mapping");
    remote.online = false;
    Check(mode.Update(remote, referee, 0.001).mode == Mode::Powerless,
          "remote offline is powerless");
}

void RunSequence(LaunchSequence &sequence, FakeIo &io, double start = 0) {
    for (int i = 0; i < 20000 && sequence.Busy(); ++i) {
        io.Advance();
        sequence.Tick(start + i * 0.01);
    }
}

void TestManualInventory() {
    DartInventory inventory;
    Check(!inventory.Grip(1) && inventory.Fault(), "invalid grip marks inventory fault");
    inventory.ResetManual();
    Check(inventory.Remaining() == 1 && inventory.Seated() && !inventory.Held() &&
              !inventory.Has(DartLocation::RightHolder) && !inventory.Has(DartLocation::LeftHolder) &&
              inventory.HeldSide() == 0 && !inventory.Fault(),
          "manual inventory resets to one seated dart without holders or fault");
    Check(inventory.Fire() && inventory.Remaining() == 0, "manual shot empties inventory");
    inventory.Latched();
    Check(inventory.Seated() && inventory.Remaining() == 1, "manual latch restores seated dart");
    inventory.ResetFull();
    Check(inventory.Fire(), "full inventory fires seated dart");
    inventory.Latched();
    Check(!inventory.Seated() && inventory.Remaining() == 3,
          "automatic reload latch does not restore seated dart");
}

void TestLeaveManual() {
    ModeLogic logic;
    RemoteInput remote;
    RefereeInput referee;
    remote.online = true;
    remote.right = Switch::Middle;
    Check(!logic.Update(remote, referee, 0.01).leave_manual, "initial manual is not a departure");
    Check(!logic.Update(remote, referee, 0.01).leave_manual, "staying manual is not a departure");
    remote.right = Switch::Up;
    const auto automatic = logic.Update(remote, referee, 0.01);
    Check(automatic.mode == Mode::Automatic && automatic.leave_manual && !automatic.fire,
          "manual to automatic reports departure without firing");
    Check(!logic.Update(remote, referee, 0.01).leave_manual, "staying automatic is not a departure");
    remote.left = Switch::Up;
    Check(logic.Update(remote, referee, 0.01).fire, "automatic middle to up fires without referee");
    Check(!logic.Update(remote, referee, 0.01).fire, "held automatic switch does not repeat fire");
    remote.right = Switch::Middle;
    Check(!logic.Update(remote, referee, 0.01).leave_manual, "automatic to manual is not a departure");
    remote.right = Switch::Down;
    const auto powerless = logic.Update(remote, referee, 0.01);
    Check(powerless.mode == Mode::Powerless && powerless.leave_manual,
          "manual to powerless reports departure");
    Check(!logic.Update(remote, referee, 0.01).leave_manual, "staying powerless is not a departure");
}

void TestManualSingleShot() {
    FakeIo io;
    DartInventory inventory;
    inventory.ResetManual();
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandFire(), "manual single shot accepts ready launcher");
    RunSequence(sequence, io);
    Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None && sequence.Shots() == 1 &&
              inventory.Remaining() == 0 && !inventory.Fault() && Near(io.lock_target, kLockOpen) &&
              io.state.latch_state == LatchState::Released && io.slider_commands == 0 &&
              io.arm_ticks == 0 && io.crank_ticks == 0,
          "manual single shot unlocks once and completes without reload or fault");
    Check(!sequence.CommandFire() && sequence.Fault() == SafetyFault::NotLatched,
          "manual second shot without cocking fails as not latched");

    for (LatchState latch : {LatchState::Released, LatchState::Unknown}) {
        FakeIo cock_io;
        cock_io.state.latch_state = latch;
        cock_io.state.slider = latch == LatchState::Released ? kCockPosition : kSliderPark;
        DartInventory cock_inventory;
        cock_inventory.ResetManual();
        Check(cock_inventory.Fire(), "manual cock test starts with empty inventory");
        LaunchSequence cock(cock_io, cock_inventory);
        Check(cock.CommandCock(), "manual inventory accepts cocking");
        RunSequence(cock, cock_io);
        Check(!cock.Busy() && cock.Fault() == SafetyFault::None &&
                  cock_io.state.latch_state == LatchState::Latched &&
                  Near(cock_io.state.slider, kSliderPark) && cock_inventory.Remaining() == 1 &&
                  cock_inventory.Seated() && !cock_inventory.Held(),
              "manual latch confirmation or low-current probe restores one dart");
        Check(std::string(cock.StepName()) == "空闲", "completed cocking immediately reports idle");
    }
}

void TestDecock() {
    FakeIo io;
    DartInventory inventory;
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandDecock(), "latched parked launcher accepts decocking");
    Check(!sequence.CommandDecock() && sequence.Fault() == SafetyFault::None,
          "busy decocking rejects another command without fault");
    std::vector<std::string> labels;
    std::uint32_t revision = 0;
    for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
        io.Advance();
        sequence.Tick(tick * 0.01);
        if (sequence.Revision() != revision) {
            revision = sequence.Revision();
            labels.emplace_back(sequence.StepName());
            switch (labels.size()) {
            case 1:
                Check(Near(io.slider_target, kProbePosition) && Near(io.slider_speed, 0) &&
                          io.lock_commands == 0,
                      "decock approaches probe position at default speed with jaws closed");
                break;
            case 2:
                Check(Near(io.slider_target, kPressSearchLimit) && Near(io.slider_speed, kPressSpeed) &&
                          io.lock_commands == 0 && !sequence.AtBottom(),
                      "decock presses slowly before opening jaws");
                break;
            case 3:
                Check(Near(io.state.slider, io.hard_bottom + kCockLift) && Near(io.lock_target, kLockOpen) &&
                          io.lock_commands == 1 && io.state.latch_state == LatchState::Released &&
                          sequence.AtBottom(),
                      "decock opens jaws only while slider supports the launcher");
                break;
            case 4:
                Check(Near(io.slider_target, kSliderPark) && Near(io.slider_speed, kLoadRiseSpeed) &&
                          io.LockAt(kLockOpen),
                      "decock rises to park slowly after jaws open");
                break;
            }
        }
    }
    const std::vector<std::string> expected{
        "退膛：滑块下到锁止位上方", "退膛：滑块慢压", "退膛：张开夹爪", "退膛：滑块慢慢回位"
    };
    Check(labels == expected, "decock executes exactly the four requested steps in order");
    Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None &&
              io.state.latch_state == LatchState::Released && Near(io.state.slider, kSliderPark) &&
              sequence.Shots() == 0 && io.unlocks == 0,
          "decock finishes released and parked without firing or fault");
    Check(std::string(sequence.StepName()) == "空闲", "completed decock immediately reports idle");

    for (LatchState latch : {LatchState::Unknown, LatchState::Released}) {
        FakeIo rejected_io;
        rejected_io.state.latch_state = latch;
        DartInventory rejected_inventory;
        LaunchSequence rejected(rejected_io, rejected_inventory);
        Check(!rejected.CommandDecock() && !rejected.Busy() &&
                  rejected.Fault() == SafetyFault::None && rejected.Revision() == 0 &&
                  rejected_io.slider_commands == 0 && rejected_io.lock_commands == 0,
              "unknown or released launcher rejects decock without fault or queued steps");
    }
}

void TestBottomFinding() {
    for (double hard_bottom : {kCockBottom, kCockBottom + 0.015, kCockBottom - 0.020}) {
        FakeIo io;
        io.state.latch_state = LatchState::Released;
        io.hard_bottom = hard_bottom;
        DartInventory inventory;
        LaunchSequence sequence(io, inventory);
        Check(sequence.CommandCock(), "hard bottom variants accept cocking");
        bool saw_close = false;
        for (int tick = 0; tick < 2000 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(tick * 0.01);
            if (std::string(sequence.StepName()) == "上膛：锁止" && io.lock_ticks > 0) {
                saw_close = true;
                Check(sequence.AtBottom(), "lock closes only after bottom snapshot");
                Check(Near(io.slider_target, hard_bottom + kCockLift),
                          "lock target is measured bottom plus lift");
            }
        }
        Check(saw_close && !sequence.Busy() && sequence.Fault() == SafetyFault::None &&
                  io.state.latch_state == LatchState::Latched && Near(io.state.slider, kSliderPark),
              "hard bottom at nominal, high, and low positions ends latched");
    }

    FakeIo moving;
    moving.state.latch_state = LatchState::Released;
    moving.press_current = kBottomCurrent + 1;
    DartInventory moving_inventory;
    LaunchSequence moving_sequence(moving, moving_inventory);
    Check(moving_sequence.CommandCock(), "moving high-current press accepts cocking");
    bool continued_down = false;
    for (int tick = 0; tick < 2000 && moving_sequence.Busy(); ++tick) {
        moving.Advance();
        moving_sequence.Tick(tick * 0.01);
        if (std::string(moving_sequence.StepName()) == "上膛：滑块慢压" &&
            moving.state.slider > moving.hard_bottom + kBottomStill &&
            moving.state.slider < kProbePosition) {
            continued_down = true;
            Check(moving_sequence.Fault() == SafetyFault::None && !moving_sequence.AtBottom(),
                  "moving high current does not count as bottom");
            break;
        }
    }
    Check(continued_down, "current above bottom threshold while moving continues down");
    RunSequence(moving_sequence, moving, 20);

    FakeIo blocked;
    blocked.state.latch_state = LatchState::Released;
    blocked.hard_bottom = kProbePosition;
    blocked.press_current = kBottomCurrent;
    blocked.press_blocked = true;
    DartInventory blocked_inventory;
    LaunchSequence blocked_sequence(blocked, blocked_inventory);
    Check(blocked_sequence.CommandCock(), "blocked press accepts cocking");
    RunSequence(blocked_sequence, blocked);
    Check(blocked_sequence.Fault() == SafetyFault::PressBlocked && blocked.hold_requested &&
              blocked.lock_target != kLockClosed && !blocked_sequence.AtBottom(),
          "press blocked before minimum push holds slider without closing jaws");

    FakeIo no_bottom;
    no_bottom.state.latch_state = LatchState::Released;
    no_bottom.hard_bottom = kPressSearchLimit - 0.010;
    DartInventory no_bottom_inventory;
    LaunchSequence no_bottom_sequence(no_bottom, no_bottom_inventory);
    Check(no_bottom_sequence.CommandCock(), "no-bottom press accepts cocking");
    RunSequence(no_bottom_sequence, no_bottom);
    Check(no_bottom_sequence.Fault() == SafetyFault::NoBottom && no_bottom.hold_requested,
          "press reaching search limit without contact reports no bottom and holds");

    FakeIo reset_io;
    reset_io.state.latch_state = LatchState::Released;
    DartInventory reset_inventory;
    LaunchSequence reset_sequence(reset_io, reset_inventory);
    Check(reset_sequence.CommandCock(), "bottom snapshot reset test accepts cocking");
    bool reset_ready = false;
    for (int tick = 0; tick < 2000 && reset_sequence.Busy(); ++tick) {
        reset_io.Advance();
        reset_sequence.Tick(tick * 0.01);
        if (std::string(reset_sequence.StepName()) == "上膛：锁止" && reset_io.lock_ticks > 0) {
            reset_ready = true;
            break;
        }
    }
    Check(reset_ready && reset_sequence.AtBottom(), "bottom snapshot is true at measured lock position");
    reset_sequence.Reset();
    Check(!reset_sequence.AtBottom(), "reset clears bottom snapshot");

    FakeIo cancel_io;
    cancel_io.state.latch_state = LatchState::Released;
    DartInventory cancel_inventory;
    LaunchSequence cancel_sequence(cancel_io, cancel_inventory);
    Check(cancel_sequence.CommandCock(), "bottom snapshot cancel test accepts cocking");
    bool cancel_ready = false;
    for (int tick = 0; tick < 2000 && cancel_sequence.Busy(); ++tick) {
        cancel_io.Advance();
        cancel_sequence.Tick(tick * 0.01);
        if (std::string(cancel_sequence.StepName()) == "上膛：锁止" && cancel_io.lock_ticks > 0) {
            cancel_ready = true;
            break;
        }
    }
    Check(cancel_ready && cancel_sequence.AtBottom(), "cancel setup reaches measured lock position");
    cancel_sequence.Cancel();
    Check(!cancel_sequence.AtBottom(), "cancel clears bottom snapshot");

    FakeIo fault_io;
    fault_io.state.latch_state = LatchState::Released;
    DartInventory fault_inventory;
    LaunchSequence fault_sequence(fault_io, fault_inventory);
    Check(fault_sequence.CommandCock(), "bottom snapshot fault test accepts cocking");
    bool fault_ready = false;
    double fault_time = 0;
    for (int tick = 0; tick < 2000 && fault_sequence.Busy(); ++tick) {
        fault_io.Advance();
        fault_time = tick * 0.01;
        fault_sequence.Tick(fault_time);
        if (std::string(fault_sequence.StepName()) == "上膛：锁止" && fault_io.lock_ticks > 0) {
            fault_ready = true;
            break;
        }
    }
    Check(fault_ready && fault_sequence.AtBottom(), "fault setup reaches measured lock position");
    fault_io.state.fault = SafetyFault::SliderSync;
    fault_sequence.Tick(fault_time + 0.01);
    Check(fault_sequence.Fault() == SafetyFault::SliderSync && !fault_sequence.AtBottom(),
          "injected fault clears bottom snapshot");

    FakeIo load_io;
    load_io.state.latch_state = LatchState::Released;
    DartInventory load_inventory;
    Check(load_inventory.Fire(), "load bottom test starts with empty seated slot");
    LaunchSequence load_sequence(load_io, load_inventory);
    Check(load_sequence.CommandLoad(), "load bottom test accepts direct loading");
    int approaches = 0;
    int presses = 0;
    bool close_at_bottom = false;
    std::uint32_t load_revision = 0;
    for (int tick = 0; tick < 20000 && load_sequence.Busy(); ++tick) {
        load_io.Advance();
        load_sequence.Tick(tick * 0.01);
        if (load_sequence.Revision() != load_revision) {
            load_revision = load_sequence.Revision();
            const std::string step = load_sequence.StepName();
            if (step == "装填：滑块下到锁止位上方") {
                ++approaches;
                Check(Near(load_io.slider_target, kProbePosition) && Near(load_io.slider_speed, 0),
                      "load approaches probe position before each slow press");
            }
            if (step == "装填：滑块把发射机构带下去" || step == "装填：压下锁止") {
                ++presses;
                Check(Near(load_io.slider_target, kPressSearchLimit) &&
                          Near(load_io.slider_speed, kPressSpeed),
                      "load uses slow press search target for both descents");
            }
            if (step == "装填：锁止") {
                close_at_bottom = load_sequence.AtBottom();
            }
        }
    }
    Check(approaches == 2 && presses == 2 && close_at_bottom && !load_sequence.Busy() &&
              load_sequence.Fault() == SafetyFault::None && load_inventory.Seated() &&
              load_io.state.latch_state == LatchState::Latched,
          "load performs two approach-and-find-bottom pairs within step timeouts");
}

void TestLockSettle() {
    FakeIo io;
    io.state.latch_state = LatchState::Released;
    DartInventory inventory;
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandCock(), "lock settle test accepts cocking");
    sequence.Tick(0);
    sequence.Tick(0);
    Check(std::string(sequence.StepName()) == "上膛：张开夹爪" && io.lock_commands == 1,
          "cocking starts by opening the jaws");
    // 模拟斜坡立即到位，流程仍须等待舵机实际转动的余量。
    io.lock = io.lock_target;
    io.lock_ticks = 0;
    Check(io.LockAt(kLockOpen), "lock ramp reports arrival immediately");
    for (int i = 0; i < 60; ++i) {
        sequence.Tick(i * kLockSettleTime / 60);
        Check(std::string(sequence.StepName()) == "上膛：张开夹爪" && sequence.Busy() &&
                  io.slider_commands == 0 && Near(io.state.slider, kSliderPark),
              "cocking cannot press before the lock settle time");
    }
    sequence.Tick(kLockSettleTime - 0.001);
    Check(io.slider_commands == 0, "slider stays parked just before the lock settle boundary");
    sequence.Tick(kLockSettleTime);
    sequence.Tick(kLockSettleTime + 0.001);
    Check(std::string(sequence.StepName()) == "上膛：滑块下到锁止位上方" && io.slider_commands == 1 &&
              Near(io.slider_target, kProbePosition) && Near(io.slider_speed, 0) &&
              sequence.Fault() == SafetyFault::None,
          "cocking approaches probe position at default speed after the full lock settle time");
    for (int i = 1; i <= 2; ++i) {
        io.Advance();
        sequence.Tick(kLockSettleTime + 0.001 + i * 0.01);
        Check(std::string(sequence.StepName()) == "上膛：滑块下到锁止位上方" &&
                  io.slider_commands == 1,
              "cocking waits for approach arrival before pressing");
    }
    io.Advance();
    sequence.Tick(kLockSettleTime + 0.031);
    sequence.Tick(kLockSettleTime + 0.041);
    Check(std::string(sequence.StepName()) == "上膛：滑块慢压" && io.slider_commands == 2 &&
              Near(io.slider_target, kPressSearchLimit) && Near(io.slider_speed, kPressSpeed),
          "released cocking presses to cock position at the slow press speed");
    RunSequence(sequence, io, kLockSettleTime + 0.051);
    Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None &&
              io.state.latch_state == LatchState::Latched && Near(io.state.slider, kSliderPark),
          "cocking completes after waiting for the lock servo");
}

void TestCrankSettle() {
    FakeIo io;
    io.state.crank = kCrankSeat;
    DartInventory inventory;
    inventory.ResetManual();
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandFire(), "crank settle test accepts a single shot with the crank out");
    sequence.Tick(0);
    sequence.Tick(0);
    Check(std::string(sequence.StepName()) == "发射：推杆收起" && Near(io.crank_target, 0) &&
              io.crank_ticks == 3,
          "single shot retracts an extended crank first");
    const std::uint32_t crank_revision = sequence.Revision();
    // FakeIo 的舵机 3 拍就到位，流程仍须等推杆舵机实际转动的余量。
    for (int i = 0; i < 3; ++i) {
        io.Advance();
    }
    Check(io.CrankAt(0), "crank ramp reports arrival after three ticks");
    for (int i = 0; i < 60; ++i) {
        sequence.Tick(i * kCrankSettleTime / 60);
        Check(std::string(sequence.StepName()) == "发射：推杆收起" && sequence.Busy() &&
                  sequence.Revision() == crank_revision && io.lock_commands == 0 &&
                  sequence.Fault() == SafetyFault::None,
              "crank step cannot finish before the crank settle time");
    }
    sequence.Tick(kCrankSettleTime - 0.001);
    Check(sequence.Revision() == crank_revision && io.lock_commands == 0,
          "crank step still waits just before the crank settle boundary");
    sequence.Tick(kCrankSettleTime + 0.001);
    sequence.Tick(kCrankSettleTime + 0.002);
    Check(std::string(sequence.StepName()) == "瞄准：等 yaw / pitch 到位" &&
              sequence.Revision() == crank_revision + 1 && sequence.Fault() == SafetyFault::None,
          "crank step finishes once the crank settle time has passed");
    RunSequence(sequence, io, kCrankSettleTime + 0.01);
    Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None && sequence.Shots() == 1 &&
              io.unlocks == 1 && Near(io.state.crank, 0),
          "single shot fires after the crank has settled");
}

void TestProbeAndLatch() {
    Check(Near(kProbePosition, kLatchSlider + kProbeMargin) &&
              kProbeTimeout > (kSliderTop - kProbePosition) / kProbeSpeed + kLatchCheckHold,
          "probe timeout covers full travel and arrival hold");
    for (bool spike : {false, true}) {
        FakeIo moving;
        moving.state.latch_state = LatchState::Unknown;
        DartInventory moving_inventory;
        LaunchSequence moving_sequence(moving, moving_inventory);
        Check(moving_sequence.CommandCock(), "moving-current probe accepts cock");
        bool observed_probe = false;
        bool observed_return = false;
        double arrived = -1;
        for (int tick = 0; tick < 2000 && moving_sequence.Busy(); ++tick) {
            moving.Advance();
            moving.motion_current = !spike || tick == 100 ? kSliderCurrentCap : 0;
            const bool at_probe = moving.SliderAt(kLatchSlider + kProbeMargin,
                                                 kSliderTolerance, kSliderVelocityTolerance);
            if (observed_probe && at_probe && arrived < 0) {
                arrived = tick * 0.01;
            }
            moving_sequence.Tick(tick * 0.01);
            const std::string step = moving_sequence.StepName();
            if (step == "探测") {
                observed_probe = true;
                Check(moving.lock_commands == 0 && moving.slider_commands == 1 &&
                          Near(moving.slider_target, kLatchSlider + kProbeMargin),
                      "moving current cannot stop probe or command lock");
                if (arrived < 0 || tick * 0.01 - arrived < kLatchCheckHold - 1e-9) {
                    Check(moving.state.latch_state == LatchState::Unknown,
                          "probe stays unknown throughout motion and full hold");
                }
            }
            if (step == "探测：滑块回位") {
                observed_return = true;
                Check(moving.state.latch_state == LatchState::Latched,
                      "low hold current confirms latch before probe return");
            }
        }
        Check(observed_probe && observed_return && arrived >= 0 &&
                  !moving_sequence.Busy() && moving_sequence.Fault() == SafetyFault::None &&
                  moving.state.latch_state == LatchState::Latched &&
                  Near(moving.state.slider, kSliderPark) && moving.lock_commands == 0,
              "high or one-spike moving current returns latched without opening");
    }

    FakeIo contact;
    contact.state.latch_state = LatchState::Unknown;
    contact.hard_bottom = kCockBottom - 0.0008;
    DartInventory contact_inventory;
    LaunchSequence contact_sequence(contact, contact_inventory);
    Check(contact_sequence.CommandCock(), "contact probe accepts cock command");
    contact_sequence.Tick(0);
    contact_sequence.Tick(0.01);
    Check(Near(contact.slider_target, kLatchSlider + kProbeMargin) &&
              Near(contact.slider_speed, kProbeSpeed) && contact.lock_commands == 0,
          "probe descends at configured speed without moving lock");
    int tick = 2;
    double contact_arrived = -1;
    contact.slider_current = kLatchCheckCurrent + 1;
    for (; tick < 2000 && contact.state.latch_state == LatchState::Unknown; ++tick) {
        contact.Advance();
        if (contact.slider_ticks == 0 && contact_arrived < 0) {
            contact_arrived = tick * 0.01;
        }
        contact_sequence.Tick(tick * 0.01);
        Check(contact.lock_commands == 0 && contact.slider_commands == 1 &&
                  Near(contact.slider_target, kLatchSlider + kProbeMargin),
              "high hold probe waits without opening or stopping short");
    }
    Check(contact_arrived >= 0 && (tick - 1) * 0.01 - contact_arrived >= kLatchCheckHold - 1e-9 &&
              contact.state.latch_state == LatchState::Released &&
              Near(contact.state.slider, kLatchSlider + kProbeMargin),
          "probe records released only after full high-current arrival hold");
    contact.slider_current = 0;
    std::vector<std::string> cock_labels;
    bool observed_press = false;
    std::uint32_t revision = contact_sequence.Revision();
    for (; tick < 2000 && contact_sequence.Busy(); ++tick) {
        contact.Advance();
        contact_sequence.Tick(tick * 0.01);
        if (contact_sequence.Revision() != revision) {
            revision = contact_sequence.Revision();
            cock_labels.emplace_back(contact_sequence.StepName());
            if (std::string(contact_sequence.StepName()) == "上膛：滑块下到锁止位上方") {
                Check(Near(contact.slider_target, kProbePosition) && Near(contact.slider_speed, 0),
                      "probed cocking approaches probe position at default speed");
            }
            if (std::string(contact_sequence.StepName()) == "上膛：滑块慢压") {
                observed_press = true;
                Check(Near(contact.slider_target, kPressSearchLimit) && Near(contact.slider_speed, kPressSpeed),
                      "probed cocking presses from probe position at the slow press speed");
            }
        }
    }
    const std::array<const char *, 8> expected_cock{{
        "探测：张开夹爪", "上膛", "上膛：张开夹爪", "上膛：滑块下到锁止位上方", "上膛：滑块慢压",
        "上膛：锁止", "上膛：锁止确认", "上膛：滑块回位"
    }};
    Check(cock_labels.size() == expected_cock.size(), "released probe opens lock then retries cock");
    for (std::size_t i = 0; i < expected_cock.size(); ++i) {
        Check(cock_labels[i] == expected_cock[i], "probe cock follows required step order");
    }
    Check(observed_press && !contact_sequence.Busy() && contact_sequence.Fault() == SafetyFault::None &&
              contact.state.latch_state == LatchState::Latched && Near(contact.state.slider, kSliderPark),
          "probe contact cock completes with positive arrival error");

    FakeIo latch;
    latch.state.latch_state = LatchState::Released;
    latch.motion_current = kSliderCurrentCap;
    DartInventory latch_inventory;
    LaunchSequence latch_sequence(latch, latch_inventory);
    Check(latch_sequence.CommandCock(), "cock accepts with high moving current");
    double arrived = -1;
    double confirmed = -1;
    bool observed_close = false;
    for (int i = 0; i < 2000 && latch_sequence.Busy(); ++i) {
        latch.Advance();
        const double now = i * 0.01;
        latch_sequence.Tick(now);
        const std::string step = latch_sequence.StepName();
        if (step == "上膛：锁止" && latch.lock_ticks > 0) {
            observed_close = true;
            Check(latch.state.latch_state == LatchState::Unknown,
                  "close command records unknown before servo arrives");
        }
        if (step == "上膛：锁止确认" && latch.slider_ticks == 0) {
            if (arrived < 0) {
                arrived = now;
            }
            if (latch.state.latch_state == LatchState::Latched) {
                confirmed = now;
            } else {
                Check(latch.state.latch_state == LatchState::Unknown,
                      "latch stays unknown during low-current hold");
            }
        }
    }
    Check(observed_close && arrived >= 0 && confirmed - arrived >= kLatchCheckHold - 1e-9,
          "latch confirms only after full arrival hold");
    Check(!latch_sequence.Busy() && latch_sequence.Fault() == SafetyFault::None &&
              latch.state.latch_state == LatchState::Latched,
          "high current during rise does not fail latch check");

    for (double current : {kLatchCheckCurrent + 1, kNan}) {
        FakeIo failure;
        failure.state.latch_state = LatchState::Released;
        DartInventory failure_inventory;
        LaunchSequence failure_sequence(failure, failure_inventory);
        Check(failure_sequence.CommandCock(), "latch failure test accepts cock");
        double hold_start = -1;
        bool injected = false;
        for (int i = 0; i < 2000 && failure_sequence.Busy(); ++i) {
            failure.Advance();
            const double now = i * 0.01;
            if (std::string(failure_sequence.StepName()) == "上膛：锁止确认" &&
                failure.slider_ticks == 0) {
                if (hold_start < 0) {
                    hold_start = now;
                }
                if (now - hold_start >= 0.1) {
                    failure.slider_current = current;
                    injected = true;
                }
            }
            failure_sequence.Tick(now);
        }
        Check(injected && failure_sequence.Fault() == SafetyFault::LatchFailed &&
                  failure.state.latch_state == LatchState::Unknown && failure.hold_requested &&
                  Near(failure.slider_target, failure.state.slider),
              "high or invalid current inside hold fails unknown and holds current position");
    }

    FakeIo opening;
    DartInventory opening_inventory;
    opening_inventory.Fire();
    LaunchSequence opening_sequence(opening, opening_inventory);
    Check(opening_sequence.CommandLoad(), "load accepts before interrupted opening");
    // 装填先收推杆，推杆步骤至少等 kCrankSettleTime，张开夹爪那一步要过 1 s 才到。
    for (int i = 0; i < 200 && opening_sequence.Busy(); ++i) {
        opening.Advance();
        opening_sequence.Tick(i * 0.01);
        if (std::string(opening_sequence.StepName()) == "装填：张开夹爪") {
            Check(opening.lock_ticks > 0 && opening.state.latch_state == LatchState::Released,
                  "opening command records released before servo arrives");
            opening.state.fault = SafetyFault::SliderSync;
            opening_sequence.Tick((i + 1) * 0.01);
            break;
        }
    }
    Check(opening_sequence.Fault() == SafetyFault::SliderSync && opening.hold_requested,
          "fault during servo opening holds supported slider");

    for (bool seated : {true, false}) {
        FakeIo unknown_load;
        unknown_load.state.latch_state = LatchState::Unknown;
        DartInventory load_inventory;
        if (!seated) {
            load_inventory.Fire();
        }
        LaunchSequence load(unknown_load, load_inventory);
        Check(load.CommandLoad(), "unknown load accepts probe");
        load.Tick(0);
        Check(std::string(load.StepName()) == "探测" && unknown_load.lock_commands == 0,
              "unknown load probes first for seated and empty launcher");
        RunSequence(load, unknown_load, 0.01);
        Check(!load.Busy() && load.Fault() == SafetyFault::None && load_inventory.Seated(),
              "unknown load expands after probe result");
        Check(Near(unknown_load.state.slider, kSliderPark),
              "unknown load returns to park including already seated launcher");
    }

    for (double current : {kNan, std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(), 0.0, kLatchCheckCurrent + 1}) {
        FakeIo failure;
        failure.state.latch_state = LatchState::Unknown;
        failure.state.slider = kProbePosition;
        // 有限值用于两个方向的高低混合；非有限值在保持中途注入。
        failure.slider_current = current == 0 ? kLatchCheckCurrent + 1 : 0;
        DartInventory failure_inventory;
        LaunchSequence failed(failure, failure_inventory);
        Check(failed.CommandCock(), "invalid or mixed probe accepts cock");
        double arrived_at = -1;
        bool injected = false;
        for (int i = 0; i < 100 && failed.Busy(); ++i) {
            failure.Advance();
            const double now = i * 0.01;
            if (std::string(failed.StepName()) == "探测" && failure.slider_ticks == 0) {
                if (arrived_at < 0) {
                    arrived_at = now;
                }
                if (now - arrived_at >= 0.1) {
                    failure.slider_current = current;
                    injected = true;
                }
            }
            failed.Tick(now);
        }
        Check(injected && failed.Fault() == SafetyFault::ProbeFailed &&
                  failure.state.latch_state == LatchState::Unknown && failure.hold_requested &&
                  failure.hold_stops == 1 && Near(failure.slider_target, failure.state.slider) &&
                  failure.lock_commands == 0,
              "mixed or nonfinite arrival current fails unknown and holds without opening");
        Check(std::string(failed.StepName()) == "探测不出发射机构状态",
              "probe failure reports required Chinese reason");
    }

    for (int command = 0; command < 3; ++command) {
        for (double current : {0.0, kLatchCheckCurrent + 1}) {
            FakeIo probing;
            probing.state.latch_state = LatchState::Unknown;
            probing.state.slider = kSliderTop;
            probing.slider_current = current;
            DartInventory probing_inventory;
            if (command == 1) {
                Check(probing_inventory.Fire(), "load scenario leaves dart on magnet only");
            }
            LaunchSequence sequence(probing, probing_inventory);
            Check(command == 0 ? sequence.CommandCock() : command == 1 ? sequence.CommandLoad() :
                      sequence.CommandSalvo(4),
                  "unknown cock load and salvo accept probe");
            bool observed_probe = false;
            bool observed_cleanup = false;
            bool observed_load_descent = false;
            for (int i = 0; i < 20000 && sequence.Busy(); ++i) {
                if (probing.state.latch_state != LatchState::Unknown) {
                    probing.slider_current = 0;
                }
                probing.Advance();
                sequence.Tick(i * 0.01);
                const std::string step = sequence.StepName();
                if (step == "探测") {
                    observed_probe = true;
                    Check(probing.lock_commands == 0 && probing.slider_commands == 1 &&
                              Near(probing.slider_target, kProbePosition) &&
                              Near(probing.slider_speed, kProbeSpeed),
                          "unknown commands issue only probe slider target until classification");
                }
                if (step == (current == 0 ? "探测：滑块回位" : "探测：张开夹爪")) {
                    observed_cleanup = true;
                    Check(probing.state.latch_state == (current == 0 ? LatchState::Latched :
                                                                       LatchState::Released),
                          "probe normalizes mechanism before retrying original command");
                }
                if (step == "装填：滑块把发射机构带下去") {
                    observed_load_descent = true;
                    if (current != 0 && command == 1) {
                        Check(Near(probing.lock_target, kLockOpen) && probing.LockAt(kLockOpen),
                              "released unknown load opens closed jaws before pressing launcher");
                    }
                }
            }
            Check(observed_probe && observed_cleanup && !sequence.Busy() &&
                      sequence.Fault() == SafetyFault::None,
                  "unknown commands complete after low or high hold classification");
            if (command == 1) {
                Check(observed_load_descent && probing_inventory.Seated() &&
                          Near(probing.state.slider, kSliderPark),
                      "unknown load seats magnet dart and parks after probe cleanup");
            } else if (command == 2) {
                Check(sequence.Shots() == 4 && probing_inventory.Remaining() == 0,
                      "unknown salvo completes four shots after either probe result");
            }
        }
    }

    FakeIo drifting;
    drifting.state.latch_state = LatchState::Unknown;
    drifting.state.slider = kProbePosition;
    DartInventory drifting_inventory;
    LaunchSequence drift(drifting, drifting_inventory);
    Check(drift.CommandCock(), "probe hold restart accepts cock");
    double released_at = -1;
    for (int i = 0; i < 2000 && drift.Busy(); ++i) {
        drifting.Advance();
        if (i == 12) {
            drifting.state.slider = kProbePosition + 0.01;
            drifting.slider_ticks = 1;
            drifting.motion_current = kNan;
        } else if (i == 13) {
            drifting.state.slider = kProbePosition;
            drifting.slider_ticks = 0;
            drifting.slider_current = kLatchCheckCurrent + 1;
        }
        drift.Tick(i * 0.01);
        if (released_at < 0 && drifting.state.latch_state == LatchState::Released) {
            released_at = i * 0.01;
            drifting.slider_current = 0;
        }
        if (i >= 13 && i * 0.01 < 0.13 + kLatchCheckHold - 1e-9) {
            Check(drifting.state.latch_state == LatchState::Unknown && drifting.lock_commands == 0,
                  "leaving arrival range restarts hold and current classification");
        }
    }
    Check(released_at >= 0.13 + kLatchCheckHold - 1e-9 &&
              !drift.Busy() && drift.Fault() == SafetyFault::None &&
              drifting.state.latch_state == LatchState::Latched,
          "probe ignores moving nonfinite current and completes restarted high hold");

    for (bool parked : {false, true}) {
        FakeIo latched;
        latched.state.slider = parked ? kSliderPark : kProbePosition;
        DartInventory latched_inventory;
        LaunchSequence cock(latched, latched_inventory);
        Check(cock.CommandCock(), "known latched cock accepts expansion");
        RunSequence(cock, latched);
        Check(!cock.Busy() && cock.Fault() == SafetyFault::None && latched.lock_commands == 0 &&
                  latched.slider_commands == (parked ? 0 : 1) && Near(latched.state.slider, kSliderPark),
              "known latched cock only returns slider when not parked");
    }
}

void TestShotRecord() {
    {
        FakeIo io;
        io.state.latch_state = LatchState::Released;
        io.hard_bottom = kCockBottom - 0.018;
        io.state.pitch = -0.02512;
        io.slider_current = 6000;
        DartInventory inventory;
        inventory.ResetManual();
        LaunchSequence sequence(io, inventory);
        Check(sequence.LastShot().number == 0, "no shot record before the first shot");
        Check(sequence.CommandCock(), "shot record cock accepts a released launcher");
        for (int tick = 0; tick < 2000; ++tick) {
            io.Advance();
            sequence.Tick(tick * 0.01);
            if (std::string(sequence.StepName()) == "上膛：锁止确认") {
                io.slider_current = 1200;
            }
            if (!sequence.Busy()) {
                break;
            }
        }
        Check(sequence.Fault() == SafetyFault::None &&
                  io.state.latch_state == LatchState::Latched && sequence.LastShot().number == 0,
              "cocking alone does not add a shot record");
        Check(sequence.CommandFire(), "shot record fire accepts the latched launcher");
        bool observed_unlock = false;
        bool recorded_on_unlock = false;
        for (int tick = 0; tick < 2000; ++tick) {
            io.Advance();
            sequence.Tick(1000 + tick * 0.01);
            if (!observed_unlock && std::string(sequence.StepName()) == "发射：解锁") {
                observed_unlock = true;
                recorded_on_unlock = sequence.LastShot().number == 1;
            }
            if (!sequence.Busy()) {
                break;
            }
        }
        Check(recorded_on_unlock,
              "shot record is written on the tick the fire unlock opens the jaws");
        const ShotRecord &shot = sequence.LastShot();
        Check(shot.number == 1 && shot.source == LatchSource::Press &&
                  Near(shot.pitch, -0.02512) && Near(shot.bottom, io.hard_bottom) &&
                  Near(shot.hold_current, 6000, 1e-6) && Near(shot.check_current, 1200, 1e-6) &&
                  shot.wait > 800 && shot.wait < 1000.1 &&
                  sequence.Fault() == SafetyFault::None && sequence.Shots() == 1,
              "press shot record keeps pitch, bottom, hold and check currents and the latched wait");
    }
    {
        FakeIo io;
        io.state.latch_state = LatchState::Unknown;
        io.slider_current = 800;
        DartInventory inventory;
        inventory.ResetManual();
        LaunchSequence sequence(io, inventory);
        const bool cocked = sequence.CommandCock();
        RunSequence(sequence, io);
        const bool fired = sequence.CommandFire();
        RunSequence(sequence, io, 1000);
        Check(cocked && fired && sequence.Fault() == SafetyFault::None,
              "probe shot record cocks by probing and fires");
        const ShotRecord &shot = sequence.LastShot();
        Check(shot.number == 1 && shot.source == LatchSource::Probe &&
                  Near(shot.check_current, 800, 1e-6) && Near(shot.hold_current, 0) &&
                  Near(shot.bottom, 0) && shot.wait > 0,
              "probe-latched shot record has no press bottom and keeps the probe current");
    }
    {
        FakeIo io;
        DartInventory inventory;
        inventory.ResetManual();
        LaunchSequence sequence(io, inventory);
        const bool fired = sequence.CommandFire();
        RunSequence(sequence, io);
        Check(fired && sequence.LastShot().number == 1 &&
                  sequence.LastShot().source == LatchSource::None && Near(sequence.LastShot().wait, 0),
              "shot without a recorded latch reports no latch source");
        sequence.Reset();
        const std::uint32_t number_after_reset = sequence.LastShot().number;
        inventory.ResetManual();
        io.state.latch_state = LatchState::Latched;
        const bool again = sequence.CommandFire();
        RunSequence(sequence, io, 1000);
        Check(number_after_reset == 1 && again && sequence.LastShot().number == 2,
              "fault reset keeps the last shot record and shot numbers keep counting");
    }
    {
        FakeIo io;
        DartInventory inventory;
        LaunchSequence sequence(io, inventory);
        const bool decocked = sequence.CommandDecock();
        RunSequence(sequence, io);
        Check(decocked && sequence.Fault() == SafetyFault::None && sequence.LastShot().number == 0,
              "decock does not add a shot record");
    }
}

void TestAutomaticTwoShot() {
    FakeIo io;
    DartInventory inventory;
    inventory.ResetSeatedLeft();
    Check(inventory.Remaining() == 2 && inventory.Seated() && !inventory.Held() &&
              inventory.Has(DartLocation::LeftHolder) && !inventory.Has(DartLocation::RightHolder),
          "two-shot test starts with a seated dart and a left-holder dart");
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandSalvo(2), "two-shot test starts");
    RunSequence(sequence, io);
    Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None && sequence.Shots() == 2 &&
              io.unlocks == 2 && inventory.Remaining() == 0,
          "one start fires both darts without another trigger");
}

void TestManualTwoShotSwitch() {
    ModeLogic logic;
    RemoteInput remote;
    RefereeInput referee;
    remote.online = true;
    remote.right = Switch::Up;
    remote.left = Switch::Middle;
    logic.Update(remote, referee, 0.01);
    Check(!logic.Update(remote, referee, 0.01).salvo, "middle position does not trigger a shot");
    remote.left = Switch::Down;
    Check(logic.Update(remote, referee, 0.01).salvo, "first middle-to-down movement triggers the flow");
    Check(!logic.Update(remote, referee, 0.01).salvo, "holding the switch does not trigger again");
    remote.left = Switch::Middle;
    remote.wheel = kNan;
    Check(!logic.Update(remote, referee, 0.01).salvo, "invalid remote input does not trigger a shot");
    remote.wheel = 0;
    logic.Update(remote, referee, 0.01);
    Check(!logic.Update(remote, referee, 0.01).salvo, "returning to middle only rearms the switch");
    remote.left = Switch::Down;
    Check(logic.Update(remote, referee, 0.01).salvo, "second middle-to-down movement triggers again");
}

void TestSequence() {
    FakeIo io;
    DartInventory inventory;
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandSalvo(4), "four shot salvo accepted");
    std::vector<std::string> labels;
    std::uint32_t revision = 0;
    int magnet_commands = 0;
    double magnet_started = -1;
    for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
        io.Advance();
        sequence.Tick(tick * 0.01);
        if (sequence.Revision() != revision) {
            if (magnet_started >= 0) {
                Check(tick * 0.01 - magnet_started > kMagnetWait,
                      "magnet action waits before the following step");
                magnet_started = -1;
            }
            revision = sequence.Revision();
            labels.emplace_back(sequence.StepName());
            if (labels.back().rfind("装填：摆臂回", 0) == 0) {
                Check(Near(io.arm_target, Radians(inventory.HeldSide() > 0 ? 1.5 : 2.0)),
                      "loading selects the arm target from the held dart source");
            }
        }
        if (io.magnet_commands != magnet_commands) {
            magnet_commands = io.magnet_commands;
            magnet_started = tick * 0.01;
            if (!io.magnet) {
                Check(!inventory.Held(), "fetch commands magnet off before taking the dart");
                magnet_started = -1;
            } else {
                Check(io.magnet && inventory.Seated(),
                      "loading commands magnet on to release the dart");
            }
        }
        Check(sequence.Fault() == SafetyFault::None, "salvo has no fault");
    }
    Check(magnet_commands == 5, "four shot salvo performs three releases and two grips");
    // 三次装填均包含下放后停留、放镖和找底锁止。
    static constexpr std::array<const char *, 88> expected{{
        "第 1 发", "装填", "预取下一发", "发射准备", "发射：吸着的镖摆开",
        "瞄准：等 yaw / pitch 到位", "发射：解锁", "飞行",
        "第 2 发", "装填", "装填：推杆收起", "装填：摆臂回 +2.0°", "装填：滑块下到锁止位上方",
        "装填：滑块把发射机构带下去", "装填：张开夹爪", "装填：慢慢抬升到磁铁正下方",
        "装填：推杆下放", "装填：等待推杆压到位", "装填：磁铁通电消磁", "装填：推杆收起", "装填：滑块下到锁止位上方", "装填：压下锁止",
        "装填：锁止", "装填：锁止确认", "装填：滑块回位", "预取下一发",
        "取镖：磁铁断电吸持", "取镖：推杆收起", "取镖：摆到右侧镖座", "取镖：下探", "取镖：等磁铁吸稳",
        "发射准备", "瞄准：等 yaw / pitch 到位", "发射：解锁", "飞行", "取镖：抬起", "取镖：摆到待命位",
        "第 3 发", "装填", "装填：推杆收起", "装填：摆臂回 +1.5°", "装填：滑块下到锁止位上方",
        "装填：滑块把发射机构带下去", "装填：张开夹爪", "装填：慢慢抬升到磁铁正下方",
        "装填：推杆下放", "装填：等待推杆压到位", "装填：磁铁通电消磁", "装填：推杆收起", "装填：滑块下到锁止位上方", "装填：压下锁止",
        "装填：锁止", "装填：锁止确认", "装填：滑块回位", "预取下一发",
        "取镖：磁铁断电吸持", "取镖：推杆收起", "取镖：摆到左侧镖座", "取镖：下探", "取镖：等磁铁吸稳",
        "发射准备", "瞄准：等 yaw / pitch 到位", "发射：解锁", "飞行", "取镖：抬起", "取镖：摆到待命位",
        "第 4 发", "装填", "装填：推杆收起", "装填：摆臂回 +2.0°", "装填：滑块下到锁止位上方",
        "装填：滑块把发射机构带下去", "装填：张开夹爪", "装填：慢慢抬升到磁铁正下方",
        "装填：推杆下放", "装填：等待推杆压到位", "装填：磁铁通电消磁", "装填：推杆收起", "装填：滑块下到锁止位上方", "装填：压下锁止",
        "装填：锁止", "装填：锁止确认", "装填：滑块回位", "预取下一发",
        "发射准备", "瞄准：等 yaw / pitch 到位", "发射：解锁", "飞行"
    }};
    Check(labels.size() == expected.size(), "salvo completes without manual trigger waits");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        Check(labels[i] == expected[i], "salvo matches every Python trace step");
    }
    Check(!sequence.Busy() && sequence.Shots() == 4 && inventory.Remaining() == 0,
          "four shot salvo completes");
    Check(io.unlocks == 4, "each shot unlocks after latch confirmation");

    FakeIo unknown_salvo_io;
    unknown_salvo_io.state.latch_state = LatchState::Unknown;
    DartInventory unknown_salvo_inventory;
    LaunchSequence unknown_salvo(unknown_salvo_io, unknown_salvo_inventory);
    Check(unknown_salvo.CommandSalvo(4), "unknown full salvo accepts cocking first");
    unknown_salvo.Tick(0);
    unknown_salvo.Tick(0.01);
    Check(std::string(unknown_salvo.StepName()) == "探测", "unknown salvo probes first");
    RunSequence(unknown_salvo, unknown_salvo_io, 0.02);
    Check(unknown_salvo.Fault() == SafetyFault::None && unknown_salvo.Shots() == 4 &&
              unknown_salvo_inventory.Remaining() == 0,
          "unknown full salvo cocks before firing");

    FakeIo park_current;
    DartInventory park_inventory;
    park_inventory.Fire();
    LaunchSequence park_sequence(park_current, park_inventory);
    Check(park_sequence.CommandLoad(), "load accepts for park current check");
    for (int tick = 0; tick < 20000 && park_sequence.Busy(); ++tick) {
        park_current.Advance();
        if (std::string(park_sequence.StepName()) == "装填：滑块回位") {
            park_current.slider_current = kSliderCurrentCap;
        }
        park_sequence.Tick(tick * 0.01);
    }
    Check(park_sequence.Fault() == SafetyFault::None && park_inventory.Seated(),
          "park return ignores normal acceleration current");

    FakeIo unknown;
    unknown.state.latch_state = LatchState::Unknown;
    unknown.state.slider = kSliderPark;
    DartInventory unknown_inventory;
    LaunchSequence cock(unknown, unknown_inventory);
    Check(cock.CommandCock(), "unknown cock accepted");
    cock.Tick(0);
    cock.Tick(0.01);
    Check(std::string(cock.StepName()) == "探测", "unknown starts with probe");
    RunSequence(cock, unknown, 0.02);
    Check(cock.Fault() == SafetyFault::None && unknown.state.latch_state == LatchState::Latched &&
              Near(unknown.state.slider, kSliderPark) && unknown.lock_commands == 0,
          "low-current probe confirms latch and returns without opening lock");

    FakeIo high;
    high.state.latch_state = LatchState::Released;
    high.state.slider = kCockPosition;
    high.slider_current = kLatchCheckCurrent + 1;
    DartInventory high_inventory;
    LaunchSequence high_sequence(high, high_inventory);
    high_sequence.CommandCock();
    for (int tick = 0; tick < 2000 && high_sequence.Busy(); ++tick) {
        high.Advance();
        high_sequence.Tick(tick * 0.01);
    }
    Check(high_sequence.Fault() == SafetyFault::LatchFailed && high.hold_requested &&
              high.state.latch_state == LatchState::Unknown,
          "high latch current fails and holds slider");

    FakeIo held;
    held.state.latch_state = LatchState::Released;
    held.state.slider = kCockPosition;
    held.blocked = true;
    DartInventory held_inventory;
    LaunchSequence timeout(held, held_inventory);
    timeout.CommandCock();
    timeout.Tick(0);
    timeout.Tick(0.01);
    timeout.Tick(kPressTimeout + 1);
    Check(timeout.Fault() == SafetyFault::Timeout && held.hold_requested,
          "timeout holds supported slider");

    FakeIo cancel_io;
    cancel_io.state.latch_state = LatchState::Unknown;
    cancel_io.state.slider = kCockPosition;
    DartInventory cancel_inventory;
    LaunchSequence cancelled(cancel_io, cancel_inventory);
    cancelled.CommandCock();
    cancelled.Cancel();
    Check(cancel_io.hold_requested, "cancel holds supported slider");

    FakeIo offline;
    offline.state.latch_state = LatchState::Unknown;
    offline.state.slider = kCockPosition;
    offline.state.remote_online = false;
    DartInventory offline_inventory;
    LaunchSequence offline_sequence(offline, offline_inventory);
    offline_sequence.CommandCock();
    offline_sequence.Tick(0);
    Check(offline_sequence.Fault() == SafetyFault::RemoteOffline && offline.hold_requested,
          "remote offline holds supported slider");

    FakeIo motor_offline;
    motor_offline.state.latch_state = LatchState::Unknown;
    motor_offline.state.slider = kCockPosition;
    motor_offline.state.motors_online = false;
    DartInventory motor_inventory;
    LaunchSequence motor_sequence(motor_offline, motor_inventory);
    motor_sequence.CommandCock();
    motor_sequence.Tick(0);
    Check(motor_sequence.Fault() == SafetyFault::MotorOffline && motor_offline.hold_requested,
          "motor offline holds supported slider");

    FakeIo locked_timeout;
    locked_timeout.blocked = true;
    DartInventory locked_inventory;
    locked_inventory.Fire();
    LaunchSequence locked_sequence(locked_timeout, locked_inventory);
    Check(locked_sequence.CommandLoad(), "locked load accepts before timeout");
    locked_sequence.Tick(0);
    locked_sequence.Tick(0.01);
    locked_sequence.Tick(kServoTimeout + 1);
    Check(locked_sequence.Fault() == SafetyFault::Timeout && !locked_timeout.hold_requested,
          "locked park timeout releases slider stop");

    FakeIo parked_cancel;
    DartInventory parked_inventory;
    LaunchSequence parked_sequence(parked_cancel, parked_inventory);
    parked_sequence.CommandCock();
    parked_sequence.Cancel();
    Check(parked_cancel.stops == 1 && !parked_cancel.hold_requested,
          "parked cancellation releases slider stop");

    FakeIo pause_io;
    DartInventory pause_inventory;
    LaunchSequence paused(pause_io, pause_inventory);
    Check(paused.CommandFire(), "fire accepts before pause");
    paused.Tick(0);
    paused.Tick(1, false);
    paused.Tick(100, false);
    Check(pause_io.stops == 0, "powerless pause does not stop motors");
    RunSequence(paused, pause_io, 100);
    Check(paused.Fault() == SafetyFault::None && paused.Shots() == 1,
          "paused aim resumes and fires");
}

// 放镖通电以后、推杆收起以前无力过：恢复时报故障，不接着收推杆；推杆收起以后无力照常恢复。
void TestReleasePause() {
    FakeIo io;
    DartInventory inventory;
    LaunchSequence sequence(io, inventory);
    Check(sequence.CommandSalvo(2), "two shot salvo accepted for the release pause");
    int tick = 0;
    for (; tick < 20000 && sequence.Busy() && !io.magnet; ++tick) {
        io.Advance();
        sequence.Tick(tick * 0.01);
    }
    Check(io.magnet && std::string(sequence.StepName()) == "装填：磁铁通电消磁",
          "the salvo reaches the release step");
    const double paused_at = tick * 0.01;
    sequence.Tick(paused_at, false);
    sequence.Tick(paused_at + 5, false);
    Check(sequence.Fault() == SafetyFault::None && sequence.Busy(),
          "powerless in the release window only pauses the sequence");
    const double crank_target = io.crank_target;
    io.Advance();
    sequence.Tick(paused_at + 5.01);
    Check(sequence.Fault() == SafetyFault::ReleaseInterrupted && !sequence.Busy() &&
              io.crank_target == crank_target,
          "resuming after powerless in the release window fails instead of retracting the crank");
    Check(std::string(Safety::Message(SafetyFault::ReleaseInterrupted)) == "放镖途中无力过，磁铁可能重新吸住镖",
          "the release interruption has a message");

    FakeIo later_io;
    DartInventory later_inventory;
    LaunchSequence later(later_io, later_inventory);
    Check(later.CommandSalvo(2), "two shot salvo accepted for a pause after the release");
    bool released = false;
    tick = 0;
    for (; tick < 20000 && later.Busy(); ++tick) {
        later_io.Advance();
        later.Tick(tick * 0.01);
        released = released || later_io.magnet;
        if (released && std::string(later.StepName()) == "装填：滑块下到锁止位上方") { break; }
    }
    Check(released && later.Busy(), "the salvo retracts the crank after the release");
    later.Tick(tick * 0.01 + 0.01, false);
    later.Tick(tick * 0.01 + 3, false);
    RunSequence(later, later_io, tick * 0.01 + 3.01);
    Check(later.Fault() == SafetyFault::None && later.Shots() == 2,
          "powerless after the crank has retracted resumes the salvo");
}

// 手工打包独立测试帧，通道顺序为右横、右纵、左横、左纵。
std::array<std::uint8_t, 18> DbusFrame(std::array<int, 4> channel,
                                     int right = 3, int left = 3, int wheel = 1024) {
    std::array<std::uint8_t, 18> frame{};
    for (int i = 0; i < 4; ++i) {
        for (int bit = 0; bit < 11; ++bit) {
            if ((channel[i] & (1 << bit)) != 0) {
                const int offset = i * 11 + bit;
                frame[offset / 8] |= static_cast<std::uint8_t>(1 << (offset % 8));
            }
        }
    }
    frame[5] |= static_cast<std::uint8_t>((right << 4) | (left << 6));
    frame[16] = static_cast<std::uint8_t>(wheel);
    frame[17] = static_cast<std::uint8_t>(wheel >> 8);
    return frame;
}

void TestDbus() {
    RemoteInput input;
    Check(ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}), input), "DBUS neutral accepted");
    Check(input.online && input.right_x == 0 && input.right_y == 0 &&
          input.left_x == 0 && input.left_y == 0 && input.wheel == 0,
          "DBUS neutral channels zero");
    for (int i = 0; i < 4; ++i) {
        for (int raw : {364, 1684}) {
            std::array<int, 4> channels{{1024, 1024, 1024, 1024}};
            channels[i] = raw;
            Check(ParseDbusFrame(DbusFrame(channels), input), "DBUS full stick accepted");
            const std::array<double, 4> decoded{{input.right_x, input.right_y, input.left_x, input.left_y}};
            for (int j = 0; j < 4; ++j) {
                Check(decoded[j] == (i == j ? (raw == 364 ? -1 : 1) : 0),
                      "DBUS stick layout and signs");
            }
        }
        for (int raw : {363, 1685}) {
            std::array<int, 4> channels{{1024, 1024, 1024, 1024}};
            channels[i] = raw;
            input.wheel = 0.25;
            Check(!ParseDbusFrame(DbusFrame(channels), input), "DBUS invalid stick rejected");
            Check(input.wheel == 0.25, "DBUS rejected frame preserves output");
        }
    }
    const std::array<Switch, 3> expected{{Switch::Up, Switch::Down, Switch::Middle}};
    for (int right = 1; right <= 3; ++right) {
        for (int left = 1; left <= 3; ++left) {
            Check(ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}, right, left), input),
                  "DBUS switches accepted");
            Check(input.right == expected[right - 1] && input.left == expected[left - 1],
                  "DBUS switches mapping");
        }
    }
    Check(!ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}, 0, 3), input),
          "DBUS zero right switch rejected");
    Check(!ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}, 3, 0), input),
          "DBUS zero left switch rejected");
    for (int wheel : {0, 363, 1685, 65535}) {
        Check(ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}, 3, 3, wheel), input) &&
              input.wheel == 0, "DBUS absent or invalid wheel remains valid neutral");
    }
    for (int wheel : {364, 1684}) {
        Check(ParseDbusFrame(DbusFrame({1024, 1024, 1024, 1024}, 3, 3, wheel), input) &&
              input.wheel == (wheel == 364 ? -1 : 1), "DBUS wheel endpoints");
    }
}

// 模拟串口接收队列：按时刻投递字节，Poll 返回动作时取走或清空。
class FakeDbusQueue {
public:
    void FeedAt(std::uint32_t ms, const std::vector<std::uint8_t> &bytes) {
        // 相同时刻的多次投递按顺序累积，模拟 DMA 半满/全满分段。
        pending_.push_back({ms, bytes});
    }

    // 驱动到 end_ms，逐毫秒跑 DbusFramer，ReadFrame 时校验取走字节并记帧。
    void Run(std::uint32_t end_ms) {
        for (; now_ <= end_ms; ++now_) {
            for (const auto &item : pending_) {
                if (item.ms == now_) { queue_.insert(queue_.end(), item.bytes.begin(), item.bytes.end()); }
            }
            const auto action = framer_.Poll(now_, queue_.size());
            if (action == DbusAction::ReadFrame) {
                Check(queue_.size() == kDbusFrameSize, "framer reads exactly 18 bytes");
                std::array<std::uint8_t, kDbusFrameSize> frame{};
                std::copy(queue_.begin(), queue_.end(), frame.begin());
                frames_.push_back(frame);
                queue_.clear();
            } else if (action == DbusAction::Clear) {
                queue_.clear();
                ++clears_;
            }
        }
    }

    // 模拟轮询线程停摆：期间字节照常进入队列，但不调用 Poll。
    void SkipTo(std::uint32_t ms) {
        while (now_ < ms) {
            ++now_;
            for (const auto &item : pending_) {
                if (item.ms == now_) { queue_.insert(queue_.end(), item.bytes.begin(), item.bytes.end()); }
            }
        }
    }

    const std::vector<std::array<std::uint8_t, kDbusFrameSize>> &Frames() const { return frames_; }
    std::uint32_t Clears() const { return clears_; }

private:
    struct Feed { std::uint32_t ms; std::vector<std::uint8_t> bytes; };
    std::vector<Feed> pending_;
    std::vector<std::uint8_t> queue_;
    DbusFramer framer_;
    std::vector<std::array<std::uint8_t, kDbusFrameSize>> frames_;
    std::uint32_t clears_ = 0;
    std::uint32_t now_ = 0;
};

std::array<std::uint8_t, kDbusFrameSize> MarkerFrame(std::uint8_t tag) {
    std::array<std::uint8_t, kDbusFrameSize> frame{};
    frame[0] = tag;
    frame[17] = static_cast<std::uint8_t>(~tag);
    return frame;
}

void TestDbusFramer() {
    // 上电残留 7 字节：第一次空闲清空，下一帧完整读到且内容对齐。
    FakeDbusQueue residue;
    const auto residue_frame = MarkerFrame(0xA1);
    residue.FeedAt(0, std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7});
    residue.FeedAt(20, std::vector<std::uint8_t>(residue_frame.begin(), residue_frame.end()));
    residue.Run(60);
    Check(residue.Clears() == 1 && residue.Frames().size() == 1 &&
              residue.Frames()[0] == residue_frame,
          "residual bytes clear and next frame aligns");

    // 一帧拆 10+8 两段、相隔 2 ms：DMA 半满只产生一次 ReadFrame。
    FakeDbusQueue split;
    const auto split_frame = MarkerFrame(0xA2);
    split.FeedAt(10, std::vector<std::uint8_t>(split_frame.begin(), split_frame.begin() + 10));
    split.FeedAt(12, std::vector<std::uint8_t>(split_frame.begin() + 10, split_frame.end()));
    split.Run(40);
    Check(split.Clears() == 0 && split.Frames().size() == 1 &&
              split.Frames()[0] == split_frame,
          "split frame reads as one aligned frame");

    // 只有 12 字节就空闲：清空。
    FakeDbusQueue partial;
    partial.FeedAt(10, std::vector<std::uint8_t>{9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8});
    partial.Run(40);
    Check(partial.Clears() == 1 && partial.Frames().empty(), "short idle frame clears");

    // 积攒两帧 36 字节才空闲：清空，之后的帧正常。
    FakeDbusQueue two;
    const auto tail = MarkerFrame(0xA3);
    two.FeedAt(10, std::vector<std::uint8_t>(36, 0xEE));
    two.FeedAt(40, std::vector<std::uint8_t>(tail.begin(), tail.end()));
    two.Run(70);
    Check(two.Clears() == 1 && two.Frames().size() == 1 && two.Frames()[0] == tail,
          "two queued frames clear then realign");

    // 轮询中断 25 ms，期间到一整帧：不读，清空；下一帧正常。
    FakeDbusQueue stall;
    const auto stall_frame = MarkerFrame(0xA4);
    stall.FeedAt(15, std::vector<std::uint8_t>(stall_frame.begin(), stall_frame.end()));
    stall.FeedAt(50, std::vector<std::uint8_t>(stall_frame.begin(), stall_frame.end()));
    stall.Run(9);
    stall.SkipTo(35);
    stall.Run(90);
    Check(stall.Clears() == 1 && stall.Frames().size() == 1 &&
              stall.Frames()[0] == stall_frame,
          "stall poll clears queued frame then realigns");

    // 每 7 ms 一帧（空闲约 5 ms）连续 1 s：除开头一次清空外全部读到且对齐。
    FakeDbusQueue stream;
    for (std::uint32_t ms = 0; ms < 1000; ms += 7) {
        const auto frame = MarkerFrame(static_cast<std::uint8_t>(ms & 0xFF));
        stream.FeedAt(ms, std::vector<std::uint8_t>(frame.begin(), frame.end()));
    }
    stream.Run(1100);
    Check(stream.Frames().size() >= 140 && stream.Clears() <= 1, "continuous stream stays aligned");

    // 毫秒计数回绕附近行为不变。
    FakeDbusQueue wrap;
    DbusFramer wrap_framer;
    const std::uint32_t near_wrap = 0xFFFFFF00u;
    Check(wrap_framer.Poll(near_wrap, 0) == DbusAction::None, "wrap first poll none");
    Check(wrap_framer.Poll(near_wrap + 2, 0) == DbusAction::None, "wrap quiet still none");
    Check(wrap_framer.Poll(near_wrap + 5, 0) == DbusAction::None, "wrap idle zero keeps aligned");
    Check(wrap_framer.Poll(near_wrap + 10, kDbusFrameSize) == DbusAction::None,
          "wrap frame arriving none");
    Check(wrap_framer.Poll(near_wrap + 12, kDbusFrameSize) == DbusAction::None,
          "wrap quiet frame none");
    Check(wrap_framer.Poll(near_wrap + 20, kDbusFrameSize) == DbusAction::ReadFrame,
          "wrap aligned frame reads across counter wrap");
}

void TestWheelAndManualCancel() {
    RefereeInput referee;
    for (Switch group : {Switch::Up, Switch::Middle, Switch::Down}) {
        for (int direction : {-1, 1}) {
            ModeLogic logic(false);
            RemoteInput input;
            input.online = true;
            input.right = Switch::Middle;
            input.left = group;
            input.wheel = direction;
            int homes = 0, cocks = 0, fetches = 0;
            for (int i = 0; i < 350; ++i) {
                const auto command = logic.Update(input, referee, 0.01);
                if (i < 199) {
                    Check(!command.home_request && !command.cock_request && !command.fetch_request,
                          "wheel requires full two second hold");
                }
                homes += command.home_request;
                cocks += command.cock_request;
                fetches += command.fetch_request;
            }
            Check(homes == (direction > 0 && group != Switch::Down ? 1 : 0), "wheel positive homes once in the aim and slider groups");
            Check(fetches == (direction > 0 && group == Switch::Down ? 1 : 0), "wheel positive in the loading group starts the fetch test once");
            Check(cocks == (direction < 0 && group == Switch::Middle ? 1 : 0),
                  "wheel negative cocks only slider exactly once");
            input.wheel = 0;
            logic.Update(input, referee, 0.01);
            input.wheel = direction;
            const auto retrigger = logic.Update(input, referee, 2.01);
            Check(retrigger.home_request == (direction > 0 && group != Switch::Down) &&
                  retrigger.fetch_request == (direction > 0 && group == Switch::Down) &&
                  retrigger.cock_request == (direction < 0 && group == Switch::Middle),
                  "wheel neutral rearms trigger");
        }
    }
    ModeLogic logic(false);
    RemoteInput input;
    input.online = true;
    input.right = Switch::Middle;
    input.left = Switch::Middle;
    input.wheel = 1;
    Check(!logic.Update(input, referee, 1.5).home_request, "wheel initial hold incomplete");
    input.wheel = -1;
    Check(!logic.Update(input, referee, 1.5).cock_request, "wheel direction resets timer");
    Check(logic.Update(input, referee, 0.51).cock_request, "wheel new direction reaches two seconds");
    input.wheel = 1;
    Check(!logic.Update(input, referee, 1.5).home_request, "wheel opposite direction resets again");
    Check(logic.Update(input, referee, 0.51).home_request, "wheel opposite direction rearms");
    input.wheel = 0;
    input.left_x = 0.1;
    input.right_y = -0.1;
    Check(!logic.Update(input, referee, 0.01).stop_sequence, "manual ten percent boundary permitted");
    input.left_x = 0.101;
    Check(logic.Update(input, referee, 0.01).stop_sequence, "manual horizontal motion cancels");
    input.left_x = 0;
    input.right_y = -0.101;
    Check(logic.Update(input, referee, 0.01).stop_sequence, "manual vertical motion cancels");
    input.right_y = 0;
    input.left = Switch::Down;
    Check(logic.Update(input, referee, 0.01).stop_sequence, "manual group change cancels");
    Check(!logic.Update(input, referee, 0.01).stop_sequence, "manual unchanged group does not cancel");
    input.right = Switch::Down;
    input.wheel = -1;
    const auto powerless = logic.Update(input, referee, 3);
    Check(!powerless.home_request && !powerless.cock_request, "powerless ignores wheel");
}

void TestMagnetToggle() {
    RefereeInput referee;
    for (Switch group : {Switch::Up, Switch::Middle, Switch::Down}) {
        for (int direction : {-1, 1}) {
            ModeLogic logic(false);
            RemoteInput input;
            input.online = true;
            input.right = Switch::Middle;
            input.left = group;
            input.wheel = direction;
            int toggles = 0, cocks = 0;
            for (int i = 0; i < 350; ++i) {
                const auto command = logic.Update(input, referee, 0.01);
                if (i < 199) {
                    Check(!command.magnet_toggle, "magnet toggle requires the full two second hold");
                }
                toggles += command.magnet_toggle;
                cocks += command.cock_request;
            }
            const bool loading_down = direction < 0 && group == Switch::Down;
            const bool slider_down = direction < 0 && group == Switch::Middle;
            Check(toggles == (loading_down ? 1 : 0),
                  "wheel down toggles the magnet once per hold only in the loading group");
            Check(cocks == (slider_down ? 1 : 0),
                  "slider group keeps the cock request and the loading group never cocks");
            input.wheel = 0;
            logic.Update(input, referee, 0.01);
            input.wheel = direction;
            const auto again = logic.Update(input, referee, kHomeWheelTime + 0.01);
            Check(again.magnet_toggle == loading_down && again.cock_request == slider_down,
                  "wheel neutral rearms the magnet toggle like the other wheel requests");
        }
    }
    ModeLogic logic(false);
    RemoteInput input;
    input.online = true;
    input.right = Switch::Middle;
    input.left = Switch::Middle;
    input.wheel = -1;
    Check(!logic.Update(input, referee, 1.5).magnet_toggle, "slider group wheel down does not toggle the magnet");
    input.left = Switch::Down;
    Check(!logic.Update(input, referee, 1.5).magnet_toggle, "group change restarts the magnet toggle hold");
    Check(logic.Update(input, referee, 0.51).magnet_toggle, "loading group toggles after its own two second hold");
    Check(!logic.Update(input, referee, 5).magnet_toggle, "a longer hold does not toggle the magnet again");
    input.right = Switch::Down;
    input.wheel = 0;
    logic.Update(input, referee, 0.01);
    input.wheel = -1;
    Check(!logic.Update(input, referee, 3).magnet_toggle, "powerless ignores the magnet toggle");

    ModeLogic guarded(true);
    RemoteInput remote;
    remote.online = true;
    remote.right = Switch::Middle;
    remote.left = Switch::Down;
    remote.wheel = -1;
    Check(!guarded.Update(remote, referee, 1.5).magnet_toggle, "partial hold does not toggle");
    remote.online = false;
    Check(!guarded.Update(remote, referee, 3).magnet_toggle, "offline remote cannot toggle");
    remote.online = true;
    Check(!guarded.Update(remote, referee, 1.5).magnet_toggle, "reconnection restarts the hold");
    Check(guarded.Update(remote, referee, 0.51).magnet_toggle, "reconnected valid hold toggles once");
    remote.left_x = kNan;
    Check(!guarded.Update(remote, referee, 3).magnet_toggle, "invalid remote cannot toggle");
    remote.left_x = 0;
    Check(!guarded.Update(remote, referee, 1.5).magnet_toggle, "invalid input restarts the hold");
    Check(guarded.Update(remote, referee, 0.51).magnet_toggle, "valid input rearms after invalid input");
    remote.right = Switch::Up;
    Check(!guarded.Update(remote, referee, 3).magnet_toggle, "automatic mode cannot toggle manually");
}

void TestMagnetGuard() {
    MagnetGuard guard;
    Check(!guard.On() && guard.Update(0.001, true) == MagnetCut::None, "the magnet guard starts de-energized");
    guard.Command(true);
    Check(guard.On() && guard.Update(kMagnetOnLimit - 0.01, true) == MagnetCut::None && guard.On(),
          "an on command energizes the magnet inside the on limit");
    guard.Command(true);
    Check(guard.Update(0.02, true) == MagnetCut::Expired && !guard.On(),
          "a repeated on command does not extend the on limit");
    Check(guard.Update(10, true) == MagnetCut::None && !guard.On(), "after the limit the magnet stays de-energized");
    guard.Command(true);
    Check(guard.On() && guard.Update(0.001, true) == MagnetCut::None,
          "a new on command after the limit starts a new pulse");
    guard.Command(false);
    Check(!guard.On() && guard.Update(0.001, true) == MagnetCut::None, "an off command de-energizes at once");
    guard.Command(true);
    guard.Update(kMagnetOnLimit - 0.01, true);
    guard.Command(false);
    guard.Command(true);
    Check(guard.Update(0.02, true) == MagnetCut::None && guard.On(),
          "an off command in between restarts the pulse timing");
    Check(guard.Update(0.001, false) == MagnetCut::Blocked && !guard.On(),
          "powerless or a fault de-energizes at once");
    guard.Command(true);
    Check(guard.Update(kNan, true) == MagnetCut::Blocked && !guard.On(), "an invalid time step de-energizes");
    guard.Command(true);
    Check(guard.Update(-0.001, true) == MagnetCut::Blocked && !guard.On(), "a negative time step de-energizes");
    Check(guard.Update(0.001, false) == MagnetCut::None, "a de-energized magnet reports no cut");
}

void TestStallGuard() {
    StallGuard low(100, 2, 1.0);
    for (int request : {100, -100, 0, 99, -99}) {
        Check(low.Update(request, 0, 10) == request && !low.Tripped(),
              "current at or below the limit never trips the stall guard");
    }

    StallGuard positive(100, 2, 1.0);
    for (int i = 0; i < 3; ++i) {
        Check(positive.Update(175, 2, 0.25) == 175 && !positive.Tripped(),
              "positive stall remains unrestricted before the timeout");
    }
    Check(positive.Update(175, 2, 0.25) == 100 && positive.Tripped(),
          "positive stall trips and clamps on the timeout tick");
    Check(positive.Update(175, 20, 0.25) == 100 && positive.Tripped(),
          "stall protection stays latched when the motor starts turning");
    Check(positive.Update(-175, 0, 0.25) == -100 && positive.Tripped(),
          "stall protection clamps a reversed request while latched");
    Check(positive.Update(100, 0, 0.25) == 100 && positive.Tripped(),
          "a request at the limit keeps stall protection active");
    for (int i = 0; i < 3; ++i) {
        Check(positive.Update(99, 0, 0.25) == 99 && positive.Tripped(),
              "stall protection waits for a sustained low request");
    }
    Check(positive.Update(100, 0, 0.25) == 100 && positive.Tripped(),
          "a request at the limit restarts the release timer");
    for (int i = 0; i < 4; ++i) { positive.Update(99, 0, 0.25); }
    Check(positive.Update(99, 0, 0.25) == 99 && !positive.Tripped(),
          "a sustained request below the limit releases stall protection");

    StallGuard negative(100, 2, 1.0);
    for (int i = 0; i < 3; ++i) {
        Check(negative.Update(-175, -2, 0.25) == -175 && !negative.Tripped(),
              "negative stall remains unrestricted before the timeout");
    }
    Check(negative.Update(-175, -2, 0.25) == -100 && negative.Tripped(),
          "negative stall trips and clamps on the timeout tick");
    Check(negative.Update(std::numeric_limits<int>::min(), 0, 0.25) == -100,
          "stall clamp handles the most negative integer request");
    Check(negative.Update(-100, 0, 0.25) == -100 && negative.Tripped(),
          "a negative request at the limit keeps stall protection active");
    for (int i = 0; i < 4; ++i) { negative.Update(-99, 0, 0.25); }
    Check(negative.Update(-99, 0, 0.25) == -99 && !negative.Tripped(),
          "a request inside the limit releases stall protection");

    StallGuard reset(100, 2, 1.0);
    for (int i = 0; i < 3; ++i) { reset.Update(175, 0, 0.25); }
    Check(reset.Update(175, 2.01, 0.25) == 175 && !reset.Tripped(),
          "speed above the threshold resets stall timing");
    for (int i = 0; i < 3; ++i) { reset.Update(175, 0, 0.25); }
    Check(!reset.Tripped() && reset.Update(99, 0, 0.25) == 99,
          "a low current request resets stall timing");
    for (int i = 0; i < 3; ++i) { reset.Update(175, 0, 0.25); }
    Check(!reset.Tripped() && reset.Update(175, 0, 0.25) == 100 && reset.Tripped(),
          "stall timing restarts after a reset");

    StallGuard gap(100, 2, 1.0);
    Check(gap.Update(175, 0, 1.0) == 100 && gap.Tripped(),
          "stall protection trips before a long feedback gap");
    Check(gap.Update(0, 0, 1.0) == 0 && gap.Tripped(),
          "one low request after a feedback gap does not prove a sustained low request");
    Check(gap.Update(0, 0, 1.0) == 0 && !gap.Tripped(),
          "stall protection releases after a full observed low-request interval");

    const double inf = std::numeric_limits<double>::infinity();
    for (double invalid_dt : {kNan, inf, -0.25}) {
        StallGuard invalid(100, 2, 1.0);
        Check(invalid.Update(175, 0, invalid_dt) == 100 && invalid.Tripped(),
              "an invalid time step trips and clamps immediately");
        for (int i = 0; i < 4; ++i) { invalid.Update(0, 0, 0.25); }
        Check(invalid.Update(0, 0, 0.25) == 0 && !invalid.Tripped(),
              "a valid low request releases protection after invalid input");
    }
    for (double invalid_rpm : {kNan, inf, -inf}) {
        StallGuard invalid(100, 2, 1.0);
        Check(invalid.Update(-175, invalid_rpm, 0.25) == -100 && invalid.Tripped(),
              "an invalid speed trips and clamps immediately");
    }
    StallGuard invalid_low(100, 2, 1.0);
    Check(invalid_low.Update(25, kNan, 0.25) == 25 && invalid_low.Tripped(),
          "invalid feedback trips without increasing a small request");

    StallGuard derated(150, 2, 1.0);
    for (int i = 0; i < 4; ++i) { derated.Update(175, 0, 0.25); }
    Check(derated.Tripped() && derated.Update(175, 0, 0.25) == 150, "a raised limit clamps at its own value");
    derated.SetLimit(100);
    Check(derated.Limit() == 100 && derated.Update(175, 0, 0.25) == 100,
          "lowering the limit while tripped clamps to the new limit at once");
    for (int i = 0; i < 5; ++i) { derated.Update(120, 0, 0.25); }
    Check(derated.Tripped(), "release compares the request with the current limit");
    StallGuard restarted(kArmBoostCurrent, kArmStallRpm, kArmStallTime);
    restarted.Update(kArmCurrentCap, 0, 2.1);
    const bool tripped = restarted.Tripped();
    restarted.Reset();
    Check(tripped && !restarted.Tripped() && restarted.Update(kArmCurrentCap, 0, 0.001) == kArmCurrentCap,
          "reset re-arms the stall guard");
}

void TestFaultReset() {
    RefereeInput referee;
    RemoteInput input;
    input.online = true;
    ModeLogic logic(false);
    Check(!logic.Update(input, referee, kFaultResetTime + 0.01).fault_reset,
          "powerless hold does not reset until manual transition");
    input.right = Switch::Middle;
    Check(logic.Update(input, referee, 0.01).fault_reset, "powerless to manual resets fault");
    Check(!logic.Update(input, referee, 0.01).fault_reset, "fault reset is a single tick");
    input.right = Switch::Down;
    logic.Update(input, referee, kFaultResetTime - 0.01);
    input.right = Switch::Middle;
    Check(!logic.Update(input, referee, 0.01).fault_reset, "short powerless hold cannot reset");

    for (int interruption = 0; interruption < 4; ++interruption) {
        ModeLogic interrupted(false);
        input = RemoteInput{};
        input.online = true;
        interrupted.Update(input, referee, kFaultResetTime);
        if (interruption == 0) { input.online = false; }
        if (interruption == 1) { input.left_x = kNan; }
        if (interruption == 2) { input.right = Switch::Up; }
        if (interruption == 3) { input.right = static_cast<Switch>(9); }
        Check(!interrupted.Update(input, referee, 0.01).fault_reset,
              "offline invalid or other switch interrupts reset timer");
        input = RemoteInput{};
        input.online = true;
        interrupted.Update(input, referee, kFaultResetTime - 0.01);
        input.right = Switch::Middle;
        Check(!interrupted.Update(input, referee, 0.01).fault_reset,
              "interrupted powerless hold must start over");
        input.right = Switch::Down;
        interrupted.Update(input, referee, kFaultResetTime);
        input.right = Switch::Middle;
        Check(interrupted.Update(input, referee, 0.01).fault_reset,
              "full valid hold rearms fault reset");
    }

    Safety safety;
    for (int i = 0; i < 5; ++i) { Check(safety.MotorFeedback(i, 2), "fresh motor feedback"); }
    Check(safety.RemoteFeedback(2) && safety.RefereeFeedback(2), "fresh input feedback");
    safety.Latch(SafetyFault::LatchFailed);
    Check(safety.Check(2.01, true) == SafetyFault::LatchFailed, "mechanism fault stays latched");
    safety.ClearLatched();
    Check(safety.Check(2.02, true) == SafetyFault::None, "clear latch preserves fresh feedback");
    Check(safety.Check(2.2, false) == SafetyFault::RemoteOffline,
          "clear latch does not extend online lifetime");
    Check(safety.RemoteFeedback(2.2) && safety.Check(2.2, false) == SafetyFault::MotorOffline,
          "clear latch preserves motor timeout");

    FakeIo io;
    io.state.all_homed = false;
    io.state.latch_state = LatchState::Unknown;
    io.state.slider = kCockPosition;
    DartInventory inventory;
    LaunchSequence sequence(io, inventory);
    sequence.CommandCock();
    sequence.Tick(0);
    Check(sequence.Fault() == SafetyFault::NotHomed && io.hold_requested,
          "faulted sequence holds supported slider");
    sequence.Reset();
    Check(sequence.Fault() == SafetyFault::None && !sequence.Busy() && io.hold_requested &&
          io.state.latch_state == LatchState::Unknown && Safety::SliderMustHold(io.state),
          "sequence reset keeps supported slider holding and latch unknown");
}

void TestStickDeadband() {
    RefereeInput referee;
    for (Switch group : {Switch::Up, Switch::Middle, Switch::Down}) {
        ModeLogic logic(false);
        RemoteInput input;
        input.online = true;
        input.right = Switch::Middle;
        input.left = group;
        for (double value : {0.04, 0.05, -0.04, -0.05, 0.12, -0.14, 1.0, -1.0, 0.5, -0.5}) {
            input.left_x = input.right_y = value;
            const auto command = logic.Update(input, referee, 0.01);
            const double scaled = std::fabs(value) <= 0.05 ? 0 :
                std::copysign((std::fabs(value) - 0.05) / 0.95, value);
            if (group == Switch::Up) {
                Check(Near(command.yaw_rate, scaled * kManualYawSpeed) &&
                      Near(command.pitch_rate, scaled * kManualPitchSpeed), "aim stick deadband");
            } else if (group == Switch::Middle) {
                Check(Near(command.lock_rate, scaled * kServoSpeed) &&
                      Near(command.slider_rate, scaled * kManualSliderSpeed), "slider stick deadband");
            } else {
                const double arm = std::fabs(value) <= kArmStickDeadband ? 0 :
                    std::copysign(std::pow((std::fabs(value) - kArmStickDeadband) / (1 - kArmStickDeadband),
                                           kArmStickExponent), value);
                Check(Near(command.arm_rate, arm * kArmSpeed) &&
                      Near(command.crank_rate, scaled * kServoSpeed), "loading stick deadband with the arm curve");
            }
        }
        input.left_x = kNan;
        const auto invalid = logic.Update(input, referee, 0.01);
        Check(invalid.mode == Mode::Powerless && invalid.stop_sequence &&
              invalid.yaw_rate == 0 && invalid.lock_rate == 0 && invalid.arm_rate == 0,
              "invalid stick stays powerless");
    }
}

// 和 LauncherHardware::ArmConfig 一致的摆臂轴配置（适配层不参与主机编译）。
AxisConfig ArmAxisConfig() {
    AxisConfig config{kEncoderRange / (2 * kPi), kArmMin, kArmMax, kArmMin, Radians(1), Radians(1),
                      Radians(0.1), -1, kArmCurrentCap, 16384, kArmPos, kArmSpeedGains};
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

void TestArmTargetJog() {
    for (int direction : {-1, 1}) {
        const double limit = direction > 0 ? kArmMax : kArmMin;
        MotorAxis stuck(ArmAxisConfig());
        stuck.Feedback(0, 0);
        Check(stuck.HomeAt(0) && stuck.Jog(direction * kArmSpeed), "arm target jog starts from a homed arm");
        stuck.Update(0.001, true, true);
        Check(direction * stuck.Target() > 0 && direction * stuck.Target() <= kArmJogLead,
              "arm jog moves the target in the stick direction");
        int pushing = 0;
        for (int i = 0; i < 2000; ++i) { pushing = stuck.Update(0.001, true, true); }
        Check(Near(stuck.Target(), direction * kArmJogLead),
              "a stuck arm target stops at the jog lead instead of running away");
        Check(direction * pushing > kArmBoostCurrent, "a stuck arm builds more than the hold limit while pushed");
        Check(stuck.Jog(0) && Near(stuck.Target(), 0), "release holds where the arm actually is");
        const int held = stuck.Update(0.001, true, true);
        Check(std::abs(direction * held - kArmBoostCurrent) <= 1,
              "release keeps the pushing current but caps it at the cool-motor limit");
        Check(stuck.Jog(0) && stuck.Update(0.001, true, true) == held,
              "repeated zero jogs neither recapture the target nor clear the support");
        Check(stuck.Settled(kServoTolerance, Radians(1)), "a released arm counts as settled at its hold target");
        Check(stuck.Update(0.001, false, true) == 0, "disabled arm sends no current");
        stuck.Feedback(stuck.PositionToCounts(direction * Radians(10)), 0);
        Check(stuck.Update(0.001, true, true) == 0,
              "a re-enabled arm waits for a new command instead of pulling back to the old target");
        const int gravity_only = static_cast<int>(kArmGravityCurrent * std::sin(direction * Radians(10)));
        Check(stuck.Jog(0) && Near(stuck.Target(), direction * Radians(10)) &&
                  std::abs(stuck.Update(0.001, true, true) - gravity_only) <= 1,
              "the first zero jog after a disable holds the new position with gravity feedforward only");

        MotorAxis edge(ArmAxisConfig());
        const double start = limit - direction * Radians(10);
        edge.Feedback(edge.PositionToCounts(start), 0);
        Check(edge.HomeAt(start) && edge.Jog(direction * kArmSpeed), "arm jog approaches its soft limit");
        double previous = edge.Target();
        bool slowed = true, beyond = false;
        for (int i = 0; i < 3000; ++i) {
            edge.Feedback(edge.PositionToCounts(edge.Target()), 0);
            edge.Update(0.001, true, true);
            const double speed = direction * (edge.Target() - previous) / 0.001;
            if (speed > direction * (limit - previous) * kArmApproachGain + 1e-9) { slowed = false; }
            if (direction * (edge.Target() - limit) > 0) { beyond = true; }
            previous = edge.Target();
        }
        Check(slowed && !beyond && direction * (limit - edge.Target()) < Radians(0.1),
              "the arm target slows down near the soft limit and stops on it");
        for (int i = 0; i < 1000; ++i) {
            edge.Feedback(edge.PositionToCounts(limit - direction * Radians(1)), 0);
            edge.Update(0.001, true, true);
        }
        edge.Feedback(edge.PositionToCounts(limit), 0);
        int at_limit = 0;
        for (int i = 0; i < 100; ++i) { at_limit = edge.Update(0.001, true, true); }
        Check(direction * at_limit > 0, "holding the stick at the soft limit keeps the outward support current");
        Check(edge.Jog(0) && Near(edge.Target(), limit) && std::abs(edge.Update(0.001, true, true) - at_limit) <= 1,
              "releasing at the soft limit holds there without dropping the support");
        edge.Feedback(edge.PositionToCounts(limit + direction * Radians(1)), 0);
        int outside = 0;
        for (int i = 0; i < 500; ++i) { outside = edge.Update(0.001, true, true); }
        Check(direction * outside < direction * at_limit,
              "an arm pushed beyond the soft limit gets less outward current");

        MotorAxis lurch(ArmAxisConfig());
        lurch.Feedback(0, 0);
        lurch.HomeAt(0);
        lurch.Jog(direction * kArmSpeed);
        for (int i = 0; i < 2000; ++i) { lurch.Update(0.001, true, true); }
        lurch.Jog(0);
        lurch.Update(0.001, true, true);
        lurch.Feedback(0, -direction * 20);
        int falling = 0;
        for (int i = 0; i < 50; ++i) { falling = lurch.Update(0.001, true, true); }
        Check(direction * falling > kArmBoostCurrent, "the support integral stays when the arm falls against it");
        lurch.Feedback(0, 0);
        int resting = 0;
        for (int i = 0; i < 5; ++i) { resting = lurch.Update(0.001, true, true); }
        lurch.Feedback(0, direction * 20);
        int spiked = 0;
        for (int i = 0; i < 5; ++i) { spiked = lurch.Update(0.001, true, true); }
        lurch.Feedback(0, 0);
        int after_spike = 0;
        for (int i = 0; i < 5; ++i) { after_spike = lurch.Update(0.001, true, true); }
        Check(direction * spiked < direction * resting && direction * after_spike > direction * resting - 200,
              "a speed spike at the hold target does not bleed away the support (2026-11-12 car log)");
        lurch.Feedback(lurch.PositionToCounts(direction * Radians(1)), direction * 20);
        int surging = 0;
        for (int i = 0; i < 50; ++i) { surging = lurch.Update(0.001, true, true); }
        Check(direction * surging < 0, "stored support bleeds away when the arm surges past its target");
    }
    AxisConfig mirrored_config = ArmAxisConfig();
    mirrored_config.counts_per_unit = -mirrored_config.counts_per_unit;
    MotorAxis mirrored(mirrored_config);
    mirrored.Feedback(0, 0);
    Check(mirrored.HomeAt(kArmMax - Radians(1)) && mirrored.Jog(kArmSpeed), "a mirrored arm starts a jog");
    int mirrored_command = 0;
    for (int i = 0; i < 500; ++i) { mirrored_command = mirrored.Update(0.001, true, true); }
    Check(mirrored.Target() <= kArmMax && mirrored.Target() > kArmMax - Radians(1) && mirrored_command < 0,
          "a mirrored axis moves its target the same way and drives with the mirrored current sign");

    for (int direction : {-1, 1}) {
        const double seat = direction * Radians(40);
        const double gravity = kArmGravityCurrent * std::sin(seat);
        MotorAxis fed(ArmAxisConfig());
        fed.Feedback(fed.PositionToCounts(seat), 0);
        Check(fed.HomeAt(seat) && fed.Jog(0) && std::abs(fed.Update(0.001, true, true) - gravity) <= 1,
              "a freshly held arm gets exactly the gravity feedforward for its angle");
        AxisConfig flipped_config = ArmAxisConfig();
        flipped_config.counts_per_unit = -flipped_config.counts_per_unit;
        MotorAxis flipped(flipped_config);
        flipped.Feedback(0, 0);
        Check(flipped.HomeAt(seat) && flipped.Jog(0) && std::abs(flipped.Update(0.001, true, true) + gravity) <= 1,
              "a mirrored axis drives the gravity feedforward with the mirrored current sign");
        const double limit = direction > 0 ? kArmMax : kArmMin;
        MotorAxis faded(ArmAxisConfig());
        faded.Feedback(0, 0);
        faded.HomeAt(limit + direction * kArmGravityFade);
        Check(faded.Jog(0) && faded.Update(0.001, true, true) == 0,
              "gravity feedforward fades out beyond the soft limit");
        MotorAxis started(ArmAxisConfig());
        started.Feedback(0, 0);
        Check(started.HomeAt(0) && started.Jog(direction * kArmSpeed) &&
                  direction * started.Update(0.001, true, true) >= kArmJogFrictionCurrent,
              "a jog adds friction feedforward in its direction from the first tick");

        // 2026-11-12 21:05~21:29 实车：从镖座往回拉时摆臂还没动，超速泄放就把托住重力的积分泄光，摆臂突然掉向中间。
        MotorAxis pulled(ArmAxisConfig());
        pulled.Feedback(pulled.PositionToCounts(seat), 0);
        pulled.HomeAt(seat);
        pulled.Jog(direction * kArmSpeed);
        for (int i = 0; i < 2000; ++i) { pulled.Update(0.001, true, true); }
        pulled.Jog(0);
        const int supported = pulled.Update(0.001, true, true);
        pulled.Jog(-direction * 0.2);
        int pulling = 0;
        for (int i = 0; i < 50; ++i) { pulling = pulled.Update(0.001, true, true); }
        Check(std::abs(direction * supported - kArmBoostCurrent) <= 1 &&
                  direction * pulling > direction * supported - kArmJogFrictionCurrent - 1300,
              "pulling a stuck arm back from its seat keeps most of the support current (2026-11-12 car log)");
    }
}

void TestArmStallTracking() {
    for (int direction : {-1, 1}) {
        MotorAxis axis(ArmAxisConfig());
        StallGuard guard(kArmStallCurrent, kArmStallRpm, kArmStallTime);
        axis.Feedback(0, 0);
        Check(axis.HomeAt(direction * Radians(35)) && axis.Jog(direction * 0.2),
              "stalled arm begins a bidirectional jog");
        int request = 0;
        for (int i = 0; i < 20000; ++i) {
            request = axis.Update(0.001, true, true);
            const int actual = guard.Update(request, 0, 0.001);
            if (actual != request) { axis.TrackLimitedCurrent(request, actual); }
        }
        Check(guard.Tripped() && direction * request <= kArmStallCurrent + 300,
              "stall-limited arm speed integral tracks the actual current");
        bool stayed_limited = true;
        for (int pattern : {0, 1}) {
            for (int i = 0; i < 10000; ++i) {
                const int sampled = pattern == 0 ? (i / 100) % 2 : (i / 100) % 5 - 2;
                const double rpm = direction * sampled;
                axis.Feedback(0, rpm);
                request = axis.Update(0.001, true, true);
                const int actual = guard.Update(request, rpm, 0.001);
                if (actual != request) { axis.TrackLimitedCurrent(request, actual); }
                if (std::abs(actual) > kArmStallCurrent) { stayed_limited = false; }
            }
        }
        Check(stayed_limited, "speed quantization cannot repeatedly release stall protection");
        Check(axis.Update(0.001, false, true) == 0 && axis.Update(0.001, true, false) == 0,
              "stall tracking cannot cause output when disabled or offline");
    }
}

// 摆臂受力模型：往中间拉的重力、库仑摩擦、1 ms 下发延迟、编码器和 rpm 截断量化、堵转保护和限流回灌。
// 惯量、重力和摩擦是按 11-11~11-12 日志估的假设，不是实车预测；只用来卡住松手下滑、限位回摆和越限。
struct ArmPlant {
    double inertia, gravity, stiction, friction;
    double angle = 0, speed = 0, obstacle = 0;
    int obstacle_side = 0;
    // 镖座往回顶的弹性：越过 spring_start 以后按压进去的角度 × spring（N·m/rad）推回中间；spring_side 为 0 表示没有。
    double spring = 0, spring_start = 0;
    int spring_side = 0;
    bool stuck = true, hit = false;
    void Step(double torque, double dt) {
        const double pressed = spring_side != 0 ? std::fmax(spring_side * (angle - spring_start), 0.0) : 0;
        const double drive = torque - gravity * std::sin(angle) - spring_side * spring * pressed;
        if (stuck && std::fabs(drive) <= stiction) {
            speed = 0;
            return;
        }
        stuck = false;
        const double direction = speed != 0 ? (speed > 0 ? 1.0 : -1.0) : (drive > 0 ? 1.0 : -1.0);
        double next = speed + (drive - friction * direction - 0.01 * speed) / inertia * dt;
        if (next * direction < 0) {
            next = 0;
            stuck = true;
        }
        speed = next;
        angle += speed * dt;
        // 硬挡按 11-11 手转快碰到的位置：左 -49.3°，右 +54.5°。
        if (angle <= Radians(-49.3)) { angle = Radians(-49.3); speed = std::fmax(speed, 0.0); hit = true; }
        if (angle >= Radians(54.5)) { angle = Radians(54.5); speed = std::fmin(speed, 0.0); hit = true; }
        if (obstacle_side > 0 && angle >= obstacle) { angle = obstacle; speed = std::fmin(speed, 0.0); }
        if (obstacle_side < 0 && angle <= obstacle) { angle = obstacle; speed = std::fmax(speed, 0.0); }
    }
};

struct ArmRig {
    ArmPlant plant;
    MotorAxis axis{ArmAxisConfig()};
    StallGuard guard{kArmStallCurrent, kArmStallRpm, kArmStallTime};
    long tick = 0;
    int applied = 0;
    bool stick = true;
    double rpm = 0;
    explicit ArmRig(const ArmPlant &model) : plant(model) {
        Sense();
        axis.HomeAt(axis.Position());
        axis.Stop();
    }
    void Sense() {
        rpm = std::trunc(plant.speed * 60 / (2 * kPi));
        axis.Feedback(static_cast<double>(std::llround(plant.angle * kEncoderRange / (2 * kPi))), rpm);
    }
    // 和适配层顺序一致：先算本拍电流，每 10 拍发一次点动，再过堵转保护；电流下一拍才生效。
    void Tick(double rate) {
        Sense();
        guard.SetLimit(ArmStallLimit(30));
        const int request = axis.Update(0.001, true, true);
        if (tick++ % 10 == 0 && stick) { axis.Jog(rate); }
        const int actual = guard.Update(request, rpm, 0.001);
        if (actual != request) { axis.TrackLimitedCurrent(request, actual); }
        const double torque = applied * 3.0 / 16384 * 0.741;
        for (int i = 0; i < 10; ++i) { plant.Step(torque, 0.0001); }
        applied = actual;
    }
    void Run(double seconds, double rate) {
        for (long i = 0, n = std::lround(seconds * 1000); i < n; ++i) { Tick(rate); }
    }
};

void TestArmMoveTo() {
    MotorAxis axis(ArmAxisConfig());
    axis.Feedback(0, 0);
    Check(!axis.MoveTo(Radians(10), kArmAutoSpeed), "an arm without a zero refuses MoveTo");
    axis.HomeAt(0);
    Check(axis.MoveTo(Radians(10), kArmAutoSpeed) && axis.Moving(), "MoveTo starts a move");
    Check(!axis.MoveTo(kNan, kArmAutoSpeed) && !axis.MoveTo(0, 0) && !axis.MoveTo(0, -1),
          "MoveTo rejects invalid goals and speeds");
    axis.Jog(0.1);
    const bool jog_ended = !axis.Moving();
    axis.MoveTo(Radians(10), kArmAutoSpeed);
    axis.Stop();
    const bool stop_ended = !axis.Moving();
    axis.MoveTo(Radians(10), kArmAutoSpeed);
    axis.Update(0.001, false, true);
    Check(jog_ended && stop_ended && !axis.Moving(), "a stick jog, a stop or losing power ends the move");
    Check(axis.MoveTo(kArmMoveTolerance * 0.5, kArmAutoSpeed) && !axis.Moving(),
          "a goal already reached just holds");
    MotorAxis slider(SliderConfig());
    FinishHome(slider);
    Check(!slider.MoveTo(0.1, 0.05), "axes without jog_moves_target refuse MoveTo");

    for (double inertia : {0.01, 0.03}) {
        for (const ArmPlant &model : {ArmPlant{inertia, 0.95, 0.45, 0.35},
                                      ArmPlant{inertia, 1.3, 0.45, 0.35},
                                      ArmPlant{inertia, 0.95, 0.33, 0.25}}) {
            ArmRig rig(model);
            rig.stick = false;
            rig.Run(0.3, 0);
            for (double goal : {kFetchLeft, kFetchRight, 0.0}) {
                const double direction = goal > rig.plant.angle ? 1 : -1;
                double overshoot = 0, beyond = 0, max_speed = 0;
                const auto sample = [&]() {
                    overshoot = std::fmax(overshoot, (rig.plant.angle - goal) * direction);
                    beyond = std::fmax(beyond, std::fmax(rig.plant.angle - kArmMax, kArmMin - rig.plant.angle));
                    max_speed = std::fmax(max_speed, std::fabs(rig.plant.speed));
                };
                rig.guard.Reset();
                const bool started = rig.axis.MoveTo(goal, kArmAutoSpeed);
                int ticks = 0;
                for (; ticks < std::lround(kFetchArmTimeout * 1000) && rig.axis.Moving(); ++ticks) {
                    rig.Tick(0);
                    sample();
                }
                for (int tick = 0; tick < 500; ++tick) {
                    rig.Tick(0);
                    sample();
                }
                const double error = std::fabs(rig.plant.angle - goal);
                const double parked = rig.plant.angle;
                double drift = 0;
                for (int tick = 0; tick < 3000; ++tick) {
                    rig.Tick(0);
                    sample();
                    drift = std::fmax(drift, std::fabs(rig.plant.angle - parked));
                }
                if (!started || rig.axis.Moving() || error > Radians(1) || overshoot > Radians(1) ||
                    beyond > Radians(0.5) || rig.plant.hit || max_speed > 2 * kArmAutoSpeed || drift > Radians(0.5)) {
                    std::fprintf(stderr, "MoveTo model I=%.2f gravity=%.2f stiction=%.2f friction=%.2f goal=%.2f "
                                         "time=%.3f error=%.4f overshoot=%.4f beyond=%.4f max_speed=%.3f drift=%.4f hit=%d moving=%d\n",
                                 inertia, model.gravity, model.stiction, model.friction, goal * 180 / kPi,
                                 ticks * 0.001, error * 180 / kPi, overshoot * 180 / kPi, beyond * 180 / kPi,
                                 max_speed * 180 / kPi, drift * 180 / kPi, rig.plant.hit, rig.axis.Moving());
                }
                Check(started && ticks < std::lround(kFetchArmTimeout * 1000) && !rig.axis.Moving(),
                      "an automatic arm move arrives before the fetch timeout");
                Check(error <= Radians(1), "an automatic arm move ends within a degree of its goal");
                Check(overshoot <= Radians(1) && beyond <= Radians(0.5) && !rig.plant.hit,
                      "an automatic arm move barely overshoots and never reaches a stop");
                Check(max_speed <= 2 * kArmAutoSpeed, "an automatic arm move is never faster than a full-stick swing");
                Check(drift <= Radians(0.5), "an arm parked by an automatic move stays put");
            }
        }
    }
    // 镖座比终点早 0.45° 挡住（在 kArmMoveTolerance 外、kArmMoveNear 内）：卡住一会儿就算走到，
    // 按挡住的位置保持，不一直顶到超时报故障。
    for (double inertia : {0.01, 0.03}) {
        for (int direction : {-1, 1}) {
            const double goal = direction > 0 ? kFetchRight : kFetchLeft;
            ArmRig rig(ArmPlant{inertia, 0.95, 0.45, 0.35});
            rig.stick = false;
            rig.plant.obstacle = goal - direction * Radians(0.45);
            rig.plant.obstacle_side = direction;
            rig.Run(0.3, 0);
            rig.guard.Reset();
            const bool started = rig.axis.MoveTo(goal, kArmAutoSpeed);
            int ticks = 0;
            for (; ticks < std::lround(kFetchArmTimeout * 1000) && rig.axis.Moving(); ++ticks) { rig.Tick(0); }
            Check(started && !rig.axis.Moving() && std::fabs(rig.plant.angle - rig.plant.obstacle) < Radians(0.1) &&
                      !rig.plant.hit,
                  "an automatic arm move blocked just short of its goal ends there instead of timing out");
        }
    }
    // 11-13 实车：左镖座从约 −40.3° 起有东西往回顶，磁铁对准要压到 −41.65°；23:43 追踪在 −42.5° 往外推要约 1.6 A，
    // 弹性约 5 N·m/rad。自动移动压进去时只有几°/s，不能半路当成卡住停下，也不能顶到超时。
    for (double inertia : {0.01, 0.03}) {
        for (double spring : {5.0, 10.0}) {
            ArmRig rig(ArmPlant{inertia, 0.95, 0.45, 0.35});
            rig.stick = false;
            rig.plant.spring = spring;
            rig.plant.spring_start = Radians(-40.3);
            rig.plant.spring_side = -1;
            rig.Run(0.3, 0);
            rig.guard.Reset();
            const bool started = rig.axis.MoveTo(kFetchLeft, kArmAutoSpeed);
            int ticks = 0;
            for (; ticks < std::lround(kFetchArmTimeout * 1000) && rig.axis.Moving(); ++ticks) { rig.Tick(0); }
            rig.Run(0.5, 0);
            const double parked = rig.plant.angle;
            double drift = 0;
            for (int tick = 0; tick < 3000; ++tick) {
                rig.Tick(0);
                drift = std::fmax(drift, std::fabs(rig.plant.angle - parked));
            }
            const bool pressed = started && !rig.axis.Moving() && std::fabs(parked - kFetchLeft) < Radians(0.5) &&
                drift < Radians(0.5) && !rig.plant.hit;
            if (!pressed) {
                std::fprintf(stderr, "left seat model I=%.2f spring=%.0f time=%.3f parked=%.3f drift=%.3f moving=%d",
                             inertia, spring, ticks * 0.001, parked * 180 / kPi, drift * 180 / kPi, rig.axis.Moving());
                std::fputc(10, stderr);
            }
            Check(pressed, "an automatic move presses the arm into the springy left seat and holds it there");
        }
    }
}

void TestLoadFromFetch() {
    {
        FakeIo io;
        io.state.latch_state = LatchState::Released;
        DartInventory inventory;
        inventory.ResetManual();
        LaunchSequence sequence(io, inventory);
        Check(sequence.CommandFetchTest(), "slow crank setup fetches a dart");
        RunSequence(sequence, io);
        Check(sequence.CommandLoadFromFetch(), "slow crank setup starts standalone loading");
        double now = 1000;
        for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
            if (std::string(sequence.StepName()) == "装填：推杆下放") { break; }
        }
        Check(sequence.Busy() && !io.CrankAt(kCrankSeat), "slow crank starts its descent before arrival");
        io.blocked = true;
        for (int tick = 0; tick < 80; ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
            Check(!io.magnet && std::string(sequence.StepName()) == "装填：推杆下放",
                  "relay stays off while the crank has not reached its commanded depth");
        }
        io.blocked = false;
        for (int tick = 0; tick < 20 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
            if (std::string(sequence.StepName()) == "装填：等待推杆压到位") { break; }
        }
        Check(io.CrankAt(kCrankSeat) && sequence.Busy() && !io.magnet,
              "extra settling starts after the slow crank reaches its depth");
        const double wait_started = now - 0.01;
        for (; now - wait_started < kLoadCrankSettleTime - 0.001; now += 0.01) {
            io.Advance();
            sequence.Tick(now);
            Check(!io.magnet && inventory.Held() && Near(io.crank_target, kCrankSeat),
                  "the full extra wait keeps the relay off and the crank lowered");
        }
        for (int tick = 0; tick < 20 && !io.magnet && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
        }
        Check(io.magnet && inventory.Seated() && Near(io.crank_target, kCrankSeat),
              "relay energizes after settling while the crank remains at its depth");
        RunSequence(sequence, io, now);
        Check(sequence.Fault() == SafetyFault::None && !sequence.Busy() && sequence.Shots() == 0 &&
                  io.state.latch_state == LatchState::Latched && Near(io.state.slider, kSliderPark),
              "slow crank standalone loading finishes locked and parked without firing");
    }
    for (int side : {-1, 1}) {
        for (LatchState latch : {LatchState::Released, LatchState::Latched, LatchState::Unknown}) {
            FakeIo io;
            DartInventory inventory;
            inventory.ResetManual();
            LaunchSequence sequence(io, inventory);
            double now = 0;
            const auto run = [&]() {
                for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
                    io.Advance();
                    sequence.Tick(now);
                    now += 0.01;
                }
            };
            if (side > 0) {
                Check(sequence.CommandFetchTest(), "loading setup fetches the first left dart");
                run();
                sequence.ReleaseFetch();
            }
            Check(sequence.CommandFetchTest(), "standalone loading first fetches its selected dart");
            run();
            Check(sequence.FetchHolding() && Near(io.state.arm, side < 0 ? kFetchLeft : kFetchRight),
                  "standalone loading starts with the selected dart held above its holder");
            io.state.latch_state = latch;
            io.arm_log.clear();
            io.crank_log.clear();
            io.magnet_log.clear();
            Check(sequence.CommandLoadFromFetch() && inventory.Held() && !inventory.Seated() &&
                      inventory.Remaining() == 1 && inventory.HeldSide() == side &&
                      !inventory.Fault() && !sequence.FetchHolding(),
                  "accepted loading transfers the fetched dart into a single held inventory");
            bool saw_rise = false, saw_seat = false;
            double seat_arrived = -1;
            for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
                io.Advance();
                sequence.Tick(now);
                now += 0.01;
                const std::string label = sequence.StepName();
                if (label == "装填：慢慢抬升到磁铁正下方") {
                    saw_rise = saw_rise || Near(io.slider_target, kLoadPosition);
                }
                if (label == "装填：推杆下放") {
                    saw_seat = saw_seat || Near(io.crank_target, kCrankSeat);
                    if (seat_arrived < 0 && io.CrankAt(kCrankSeat)) { seat_arrived = now; }
                }
                if (io.magnet) {
                    Check(seat_arrived >= 0 && now - seat_arrived >= kLoadCrankSettleTime,
                          "standalone relay energizes only after the lowered crank settles");
                }
                Check(label.rfind("发射", 0) != 0 && label != "飞行" &&
                          label != "预取下一发" && sequence.Shots() == 0 && io.unlocks == 0,
                      "standalone loading never enters firing, flight or prefetch");
            }
            Check(saw_rise && saw_seat && !sequence.Busy() && sequence.Fault() == SafetyFault::None &&
                      inventory.Seated() && !inventory.Held() && inventory.Remaining() == 1 &&
                      io.state.latch_state == LatchState::Latched && Near(io.state.slider, kSliderPark) &&
                      Near(io.state.crank, kCrankRetracted) && Near(io.state.arm, Radians(side > 0 ? 1.5 : 2.0)) &&
                      io.arm_log == std::vector<double>{Radians(side > 0 ? 1.5 : 2.0)} &&
                      io.crank_log == std::vector<double>{0, kCrankSeat, 0} &&
                      io.magnet_log == std::vector<bool>{true} &&
                      sequence.LastShot().number == 0,
                  "left loads at 2 degrees and right at 1.5 degrees with the dart seated, locked and parked");
        }
    }
    for (const auto &interruption : {std::pair<int, bool>{-1, false}, {-1, true}, {1, false}, {1, true}}) {
        const int side = interruption.first;
        const bool release_pause = interruption.second;
        FakeIo io;
        io.state.latch_state = LatchState::Released;
        DartInventory inventory;
        inventory.ResetManual();
        LaunchSequence sequence(io, inventory);
        if (side > 0) {
            Check(sequence.CommandFetchTest(), "interrupted loading setup fetches the first left dart");
            RunSequence(sequence, io);
            sequence.ReleaseFetch();
        }
        Check(sequence.CommandFetchTest(), "interrupted standalone loading starts by fetching");
        RunSequence(sequence, io, 500);
        Check(sequence.CommandLoadFromFetch(), "interrupted standalone loading accepts the held dart");
        double now = 1000;
        for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
            if (release_pause ? io.magnet :
                    std::string(sequence.StepName()) == "装填：慢慢抬升到磁铁正下方") {
                break;
            }
        }
        Check(sequence.Busy(), "standalone loading reaches the interruption point");
        if (release_pause) {
            Check(io.magnet, "standalone release interruption starts after magnet energizes");
            sequence.Tick(now, false);
            sequence.Tick(now + 1, true);
            Check(sequence.Fault() == SafetyFault::ReleaseInterrupted && sequence.Shots() == 0 &&
                      io.unlocks == 0 && io.hold_requested,
                  "standalone release interruption holds the slider and stops before crank retraction");
            continue;
        }
        sequence.Cancel();
        Check(inventory.Held() && inventory.HeldSide() == side && io.hold_requested && !sequence.CommandFetchTest(),
              "cancelled loading retains the held dart and prevents another fetch");
        Check(sequence.CommandLoadFromFetch(), "cancelled loading can restart from the same held inventory");
        RunSequence(sequence, io, now);
        Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None && !inventory.Held() &&
                  inventory.Seated() && Near(io.state.crank, kCrankRetracted) && io.magnet &&
                  Near(io.state.slider, kSliderPark) && io.state.latch_state == LatchState::Latched &&
                  Near(io.state.arm, Radians(side > 0 ? 1.5 : 2.0)) &&
                  sequence.Shots() == 0 && io.unlocks == 0,
              "restarted standalone loading releases, locks and parks without firing");
    }
}

void TestAutoFetchTiming() {
    for (int side : {-1, 1}) {
        for (bool prefetch : {false, true}) {
            FakeIo io;
            io.magnet = true;
            DartInventory inventory;
            if (side < 0) {
                inventory.ResetSeatedLeft();
            } else {
                inventory.ResetFull();
                Check(inventory.Fire() && inventory.Release(), "right fetch starts with an empty magnet");
            }
            if (!prefetch) { Check(inventory.Fire(), "direct loading starts with an empty seat"); }
            LaunchSequence sequence(io, inventory);
            Check(prefetch ? sequence.CommandSalvo(1) : sequence.CommandLoad(),
                  "automatic fetch starts");

            const double crank = side < 0 ? kCrankFetchLeft : kCrankFetchRight;
            double grip_started = -1;
            bool lowered = false, magnet_off_before_lower = false, lifted = false;
            for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
                const double now = tick * 0.01;
                io.Advance();
                sequence.Tick(now);
                const std::string step = sequence.StepName();
                if (step == "取镖：下探" && !lowered) {
                    lowered = Near(io.crank_target, crank) &&
                        Near(io.arm_target, side < 0 ? kFetchLeft : kFetchRight);
                    magnet_off_before_lower = !io.magnet;
                }
                if (step == "取镖：等磁铁吸稳") {
                    if (grip_started < 0) { grip_started = now; }
                    Check(io.CrankAt(crank) && !io.magnet && inventory.Held() &&
                              inventory.HeldSide() == side,
                          "automatic fetch holds its target with the magnet off throughout the grip wait");
                }
                if (step == "取镖：抬起") {
                    Check(grip_started >= 0 && now - grip_started >= kFetchGripTime - 1e-9,
                          "automatic fetch waits as long as the manual fetch before lifting");
                    Check(lowered && magnet_off_before_lower,
                          "automatic fetch switches the magnet off before lowering to the selected side");
                    Check(inventory.Held() && inventory.HeldSide() == side &&
                              Near(io.crank_target, kCrankRetracted) &&
                              Near(io.crank_speed_log.back(), kCrankLiftSpeed),
                          "automatic fetch records the source and lifts at the manual fetch speed");
                    lifted = true;
                    break;
                }
            }
            Check(lifted && sequence.Fault() == SafetyFault::None,
                  "automatic fetch reaches the lift without a fault");
        }
    }
}

void TestFetchTest() {
    FakeIo io;
    DartInventory inventory;
    inventory.ResetManual();
    const int remaining = inventory.Remaining();
    LaunchSequence sequence(io, inventory);
    // 同一个流程连着取两发，时间要一直往前走。
    double now = 0;
    std::vector<std::string> labels;
    const auto run = [&]() {
        labels.clear();
        std::uint32_t revision = sequence.Revision();
        for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(now);
            now += 0.01;
            if (sequence.Revision() != revision) {
                revision = sequence.Revision();
                labels.emplace_back(sequence.StepName());
            }
        }
    };
    const bool starts_left = sequence.NextFetchSide() < 0;
    const bool left_accepted = sequence.CommandFetchTest();
    run();
    Check(starts_left && left_accepted && !sequence.Busy() && sequence.Fault() == SafetyFault::None,
          "the first fetch test takes the left dart without a fault");
    const std::vector<std::string> left_steps{
        "取镖试运行：磁铁断电吸持", "取镖试运行：推杆收起", "取镖试运行：摆到左侧镖座",
        "取镖试运行：左侧下探", "取镖试运行：左侧等磁铁吸稳", "取镖试运行：左侧慢慢抬起"
    };
    Check(labels == left_steps, "the fetch test lowers the crank once, grips and lifts slowly");
    Check(io.arm_log == std::vector<double>{kFetchLeft} &&
              io.crank_log == std::vector<double>{0, kCrankFetchLeft, 0} &&
              io.crank_speed_log == std::vector<double>{0, 0, kCrankLiftSpeed} &&
              io.magnet_log == std::vector<bool>{false} && !io.magnet,
          "the fetch test stays over the left seat holding the dart up with the magnet off");
    Check(sequence.FetchHolding() && sequence.NextFetchSide() > 0 && !sequence.CommandFetchTest(),
          "the next fetch waits until the operator releases the dart");
    sequence.ReleaseFetch();
    const bool right_accepted = sequence.CommandFetchTest();
    run();
    Check(right_accepted && !sequence.Busy() && sequence.Fault() == SafetyFault::None &&
              io.arm_log == std::vector<double>{kFetchLeft, kFetchRight} &&
              io.crank_log == std::vector<double>{0, kCrankFetchLeft, 0, 0, kCrankFetchRight, 0} &&
              labels.size() == 6 && labels[2] == "取镖试运行：摆到右侧镖座" &&
              labels[5] == "取镖试运行：右侧慢慢抬起",
          "after the release the next fetch takes the right dart");
    Check(sequence.FetchHolding() && sequence.NextFetchSide() < 0,
          "after the right dart the next fetch goes back to the left");
    Check(inventory.Remaining() == remaining && !inventory.Fault() && !inventory.Held(),
          "the fetch test leaves the inventory unchanged");

    FakeIo unhomed;
    unhomed.state.all_homed = false;
    DartInventory unhomed_inventory;
    unhomed_inventory.ResetManual();
    LaunchSequence arm_only(unhomed, unhomed_inventory);
    const bool arm_only_accepted = arm_only.CommandFetchTest();
    for (int tick = 0; tick < 20000 && arm_only.Busy(); ++tick) {
        unhomed.Advance();
        arm_only.Tick(tick * 0.01);
    }
    Check(arm_only_accepted && !arm_only.Busy() && arm_only.Fault() == SafetyFault::None,
          "the fetch test needs only the arm zero");

    FakeIo refused_io;
    DartInventory refused_inventory;
    LaunchSequence refused(refused_io, refused_inventory);
    const bool held_refused = !refused.CommandFetchTest();
    refused_inventory.ResetManual();
    const bool started = refused.CommandFetchTest();
    const bool busy_refused = !refused.CommandFetchTest();
    refused_io.state.motors_online = false;
    refused.Tick(0);
    Check(held_refused && started && busy_refused && refused.Fault() == SafetyFault::MotorOffline &&
              !refused.CommandFetchTest(),
          "the fetch test is refused while busy, after a fault or with a dart on the magnet");

    // 抬起到一半断力：流程停下；磁铁上可能还吸着镖，放镖以后重取同一边。
    FakeIo power_io;
    DartInventory power_inventory;
    power_inventory.ResetManual();
    LaunchSequence power(power_io, power_inventory);
    power.CommandFetchTest();
    bool power_stopped = false;
    double power_time = 0;
    for (int tick = 0; tick < 20000 && power.Busy(); ++tick) {
        power_io.Advance();
        power_time = tick * 0.01;
        power.Tick(power_time);
        if (std::string(power.StepName()) == "取镖试运行：左侧慢慢抬起") {
            const int stops = power_io.stops;
            power.Tick(power_time + 0.01, false);
            power_stopped = !power.Busy() && power.Fault() == SafetyFault::None && power_io.stops == stops + 1;
            break;
        }
    }
    Check(power_stopped && power.FetchHolding() && !power.CommandFetchTest(),
          "losing power stops the fetch test instead of pausing it");
    power.ReleaseFetch();
    Check(power.NextFetchSide() < 0 && power.CommandFetchTest(),
          "a fetch stopped after the grip retries the same side once the dart is released");

    // 下探以前就取消：磁铁没碰到镖，马上可以重新开始。
    FakeIo early_io;
    DartInventory early_inventory;
    early_inventory.ResetManual();
    LaunchSequence early(early_io, early_inventory);
    early.CommandFetchTest();
    for (int tick = 0; tick < 10; ++tick) {
        early_io.Advance();
        early.Tick(tick * 0.01);
    }
    const bool early_busy = early.Busy();
    early.Cancel();
    Check(early_busy && !early.Busy() && early.Fault() == SafetyFault::None && !early.FetchHolding() &&
              early.CommandFetchTest(),
          "a fetch cancelled before the crank reaches the dart can start again at once");

    FakeIo blocked_io;
    blocked_io.blocked = true;
    DartInventory blocked_inventory;
    blocked_inventory.ResetManual();
    LaunchSequence blocked(blocked_io, blocked_inventory);
    blocked.CommandFetchTest();
    for (int tick = 0; tick < 20000 && blocked.Busy(); ++tick) {
        blocked_io.Advance();
        blocked.Tick(tick * 0.01);
    }
    Check(!blocked.Busy() && blocked.Fault() == SafetyFault::Timeout,
          "a stuck crank ends the fetch test with a timeout");
}

void TestArmLowSpeedFeedback() {
    // 11-14 02:14 追踪 44.260~44.288 s：持续向中间点动 1.86°/s，rpm 为 0、-5、0。
    // 先在静摩擦下积累目标滞后，再注入该反馈片段；同时覆盖两侧与电机镜像方向。
    for (int side : {-1, 1}) {
        for (int motor : {-1, 1}) {
            auto config = ArmAxisConfig();
            config.counts_per_unit *= motor;
            MotorAxis axis(config);
            const double start = side * Radians(17.842);
            axis.Feedback(axis.PositionToCounts(start), 0);
            Check(axis.HomeAt(start) && axis.Jog(-side * Radians(1.86)),
                  "low-speed feedback regression starts an inward jog");
            int before = 0;
            for (int i = 0; i < 700; ++i) { before = axis.Update(0.001, true, true); }
            axis.Feedback(axis.PositionToCounts(side * Radians(17.710)), -side * motor * 5);
            const int pulse = axis.Update(0.001, true, true);
            bool inward = side * motor * pulse < 0;
            for (int i = 1; i < 14; ++i) {
                if (side * motor * axis.Update(0.001, true, true) >= 0) { inward = false; }
            }
            axis.Feedback(axis.PositionToCounts(side * Radians(17.666)), 0);
            const int after = axis.Update(0.001, true, true);
            Check(side * motor * before < 0 && inward && side * motor * after < 0,
                  "a brief five-rpm feedback pulse does not reverse the low-speed inward drive");
            Check(std::abs(pulse - before) < 1000,
                  "low-speed feedback does not switch a full friction-current step");
        }
    }
}

void TestArmLowSpeedMotion() {
    for (int side : {-1, 1}) {
        for (double start : {18.0, 28.0, 33.0}) {
            for (double rate : {Radians(1.86), Radians(3.81)}) {
                ArmPlant model{0.013, 0.95, 0.45, 0.35};
                model.angle = side * Radians(start);
                ArmRig rig(model);
                rig.Run(2, -side * rate);
                const double from = rig.plant.angle;
                int largest_step = 0, previous = rig.applied;
                for (int i = 0; i < 6000; ++i) {
                    rig.Tick(-side * rate);
                    largest_step = std::max(largest_step, std::abs(rig.applied - previous));
                    previous = rig.applied;
                }
                const double travel = side * (from - rig.plant.angle);
                Check(!rig.plant.hit && travel > rate * 6 * 0.5 && travel < rate * 6 * 1.5,
                      "small inward stick inputs keep moving through the reported jitter bands");
                if (largest_step >= kArmJogFrictionCurrent / 2) {
                    std::fprintf(stderr, "Low-speed model side=%d start=%.0f rate=%.2f current_step=%d\n",
                                 side, start, rate * 180 / kPi, largest_step);
                }
                Check(largest_step < kArmJogFrictionCurrent / 2,
                      "steady low-speed inward motion avoids abrupt friction-current steps");
            }
        }
    }
}

void TestArmInwardRelease() {
    // 11-14：从取镖位向中间点动后松手，自行移动会把镖带进发射路径。
    // 复用既有受力模型，包含编码器/rpm 量化、1 ms 电流延迟、静摩擦和堵转保护。
    for (double inertia : {0.01, 0.03}) {
        for (int direction : {-1, 1}) {
            for (double rate : {0.05, 0.1, 0.2}) {
                ArmRig rig(ArmPlant{inertia, 0.95, 0.45, 0.35});
                rig.stick = false;
                const double seat = direction > 0 ? kFetchRight : kFetchLeft;
                Check(rig.axis.MoveTo(seat, kArmAutoSpeed), "release regression starts an automatic fetch move");
                for (int i = 0; i < 6000 && rig.axis.Moving(); ++i) { rig.Tick(0); }
                Check(!rig.axis.Moving() && std::fabs(rig.plant.angle - seat) < Radians(1),
                      "release regression reaches the fetch seat");
                rig.stick = true;
                rig.Run(2, 0);
                rig.Run(1, -direction * rate);
                const double released = rig.plant.angle;
                double drift = 0;
                for (int i = 0; i < 12000; ++i) {
                    rig.Tick(0);
                    drift = std::fmax(drift, std::fabs(rig.plant.angle - released));
                }
                Check(!rig.plant.hit && drift < Radians(0.5),
                      "an inward jog released after fetching holds within half a degree for twelve seconds");
            }
        }
    }
}

void TestArmPlantModel() {
    for (double inertia : {0.01, 0.03}) {
        for (int direction : {-1, 1}) {
            const double limit = direction > 0 ? kArmMax : kArmMin;
            const double seat = direction > 0 ? kFetchRight : kFetchLeft;
            // 无力时停在 21°~31°、1.0 A 推不到镖座，都说明重力和摩擦都不小。
            const ArmPlant loaded{inertia, 1.0, 0.55, 0.47};
            ArmRig push(loaded);
            push.Run(0.3, 0);
            double peak = -1, beyond = -1, swing = 0;
            for (int i = 0; i < 4000; ++i) {
                push.Tick(direction * kArmSpeed);
                const double outward = direction * push.plant.angle;
                peak = std::fmax(peak, outward);
                beyond = std::fmax(beyond, outward - direction * limit);
                if (peak >= direction * limit - Radians(2)) { swing = std::fmax(swing, peak - outward); }
            }
            Check(!push.plant.hit && beyond < Radians(1.5) && swing < Radians(0.5),
                  "a loaded arm pushed into the soft limit stays near it without swinging back");
            const double released = push.plant.angle;
            double drift = 0;
            for (int i = 0; i < 5000; ++i) {
                push.Tick(0);
                drift = std::fmax(drift, std::fabs(push.plant.angle - released));
            }
            Check(drift < Radians(0.5), "a loaded arm released at the soft limit stays put for 5 s");

            ArmRig fetch(loaded);
            fetch.Run(0.3, 0);
            for (int i = 0; i < 8000 && direction * fetch.plant.angle < direction * seat; ++i) {
                fetch.Tick(direction * 0.67);
            }
            const double at_seat = fetch.plant.angle;
            fetch.Run(0.5, 0);
            const double settled = fetch.plant.angle;
            drift = 0;
            for (int i = 0; i < 5000; ++i) {
                fetch.Tick(0);
                drift = std::fmax(drift, std::fabs(fetch.plant.angle - settled));
            }
            Check(std::fabs(settled - at_seat) < Radians(0.5) && drift < Radians(0.5),
                  "a loaded arm released at its fetch seat does not slide back");

            ArmRig blocked(loaded);
            blocked.plant.obstacle = direction * Radians(30);
            blocked.plant.obstacle_side = direction;
            blocked.Run(3.3, direction * kArmSpeed);
            blocked.plant.obstacle_side = 0;
            beyond = -1;
            for (int i = 0; i < 3000; ++i) {
                blocked.Tick(direction * kArmSpeed);
                beyond = std::fmax(beyond, direction * (blocked.plant.angle - limit));
            }
            Check(!blocked.plant.hit && beyond < Radians(2),
                  "an arm freed from an obstacle while pushed does not overrun the soft limit");

            // 在镖座松手后用约 0.3 的摇杆量（约 8°/s）往回拉 2 s：不该突然一冲，也要真的动。
            // 重力和摩擦按 2026-11-12 22:34 实车追踪拟合（重力约 0.95 N·m，静/动摩擦约 0.45/0.35 N·m）。
            const ArmPlant sticky{inertia, 0.95, 0.45, 0.35};
            ArmRig back(sticky);
            back.Run(0.3, 0);
            for (int i = 0; i < 8000 && direction * back.plant.angle < direction * seat; ++i) {
                back.Tick(direction * 0.67);
            }
            back.Run(2.0, 0);
            const double rate = std::pow((0.3 - kStickDeadband) / (1 - kStickDeadband), 2) * kArmSpeed;
            const double from = back.plant.angle;
            double fastest = 0;
            for (int i = 0; i < 2000; ++i) {
                back.Tick(-direction * rate);
                fastest = std::fmax(fastest, std::fabs(back.plant.speed));
            }
            const double moved = direction * (from - back.plant.angle);
            Check(!back.plant.hit && fastest < 4 * rate && moved > 0.5 * rate * 2.0 && moved < 1.5 * rate * 2.0,
                  "pulling a loaded arm back from its seat with a small stick moves steadily without a lurch");
        }
    }
    // 带镖时重力可能比 2026-11-12 22:34 追踪拟合的约 0.95 N·m 大：按 1.3 N·m、摩擦按实测 0.45/0.35 N·m，
    // 推过镖座松手以后要停在镖座外侧。
    for (double inertia : {0.01, 0.03}) {
        for (int direction : {-1, 1}) {
            const double seat = direction > 0 ? kFetchRight : kFetchLeft;
            ArmRig heavy(ArmPlant{inertia, 1.3, 0.45, 0.35});
            heavy.Run(0.3, 0);
            for (int i = 0; i < 8000 && direction * heavy.plant.angle < direction * seat + Radians(1.5); ++i) {
                heavy.Tick(direction * 0.67);
            }
            heavy.Run(0.5, 0);
            const double settled = heavy.plant.angle;
            double drift = 0;
            for (int i = 0; i < 5000; ++i) {
                heavy.Tick(0);
                drift = std::fmax(drift, std::fabs(heavy.plant.angle - settled));
            }
            Check(!heavy.plant.hit && drift < Radians(0.5) && direction * settled > direction * seat,
                  "a heavy arm released past its fetch seat stays there with the cool-motor limit");
        }
    }
    // 无摩擦、惯量偏大的悲观假设：只要求不碰硬挡、越过软限位不超过 2°。
    // 右镖座 48° 要约 1.06 A 才托得住，就算没有摩擦，重力也不小于约 1.06 N·m。
    for (int direction : {-1, 1}) {
        const double limit = direction > 0 ? kArmMax : kArmMin;
        ArmRig loose(ArmPlant{0.06, 1.1, 0, 0});
        loose.Run(0.3, 0);
        double beyond = -1;
        for (int i = 0; i < 9000; ++i) {
            loose.Tick(i < 4000 ? direction * kArmSpeed : 0);
            beyond = std::fmax(beyond, direction * (loose.plant.angle - limit));
        }
        Check(!loose.plant.hit && beyond < Radians(2),
              "a heavy frictionless arm never reaches the hard stop from a full-stick push");
    }
}

void TestHomeAtAndRebase() {
    auto hold_config = SliderConfig();
    hold_config.hold_on_release = true;
    const double p0 = 0.1, p1 = 0.2;
    MotorAxis without_stop(hold_config);
    without_stop.Feedback(12345, 0);
    const bool first_homed = without_stop.HomeAt(p0);
    without_stop.Feedback(without_stop.PositionToCounts(p1), 0);
    const bool first_jogged = without_stop.Jog(0);
    Check(first_homed && first_jogged && Near(without_stop.Target(), p0),
          "absolute home without stop keeps the startup target after feedback moves");

    MotorAxis with_stop(hold_config);
    with_stop.Feedback(12345, 0);
    const bool second_homed = with_stop.HomeAt(p0);
    with_stop.Stop();
    with_stop.Feedback(with_stop.PositionToCounts(p1), 0);
    const bool second_jogged = with_stop.Jog(0);
    const bool holds_current = Near(with_stop.Target(), p1);
    with_stop.Stop();
    with_stop.Feedback(with_stop.PositionToCounts(kSliderTop + 0.05), 0);
    const bool outside_jogged = with_stop.Jog(0);
    Check(second_homed && second_jogged && holds_current && outside_jogged &&
              Near(with_stop.Target(), kSliderTop),
          "stop after absolute home holds the current position, clamped to the soft limit");

    MotorAxis axis(SliderConfig());
    Check(!axis.HomeAt(0.1) && !axis.Homed(), "absolute home needs feedback");
    axis.Feedback(12345, 0);
    Check(!axis.HomeAt(kNan) && !axis.Homed(), "absolute home rejects invalid position");
    for (double position : {-0.2, 0.1}) {
        Check(axis.HomeAt(position) && axis.Homed() && Near(axis.Position(), position) &&
              Near(axis.Target(), position), "absolute home sets position and target");
        Check(axis.Update(0.001, true, true) == 0, "absolute home creates no motion");
        Check(axis.SetTarget(position + 0.01) && axis.Update(0.001, true, true) > 0,
              "absolute home enables position targets");
    }
    axis.MarkFault();
    Check(!axis.HomeAt(0.1) && axis.State() == HomeState::Failed, "failed axis rejects absolute home");
    axis.ClearFault();
    Check(!axis.HomeAt(0.1), "fault recovery requires fresh feedback");
    axis.Feedback(23456, 0);
    Check(axis.HomeAt(-0.1), "absolute home recovers after clearing fault");
    Check(axis.Update(0.001, true, false) == 0 && !axis.Homed(), "absolute axis loses home offline");
    Check(!axis.HomeAt(0.2), "offline absolute axis cannot use stale feedback");
    axis.Feedback(34567, 0);
    Check(axis.HomeAt(0.2) && Near(axis.Position(), 0.2) && axis.Update(0.001, true, true) == 0,
          "reconnected absolute axis homes without movement");

    MotorAxis below(SliderConfig());
    below.Feedback(12345, 0);
    const bool below_homed = below.HomeAt(kSliderBottom - 0.05);
    below.Jog(0);
    Check(below_homed && below.Update(0.001, true, true) == 0,
          "jogging outside the soft limit does not drag the axis back");
    below.Jog(-0.05);
    Check(below.Update(0.001, true, true) == 0,
          "jogging further outside the soft limit is refused");
    below.Jog(0.05);
    Check(below.Update(0.001, true, true) > 0,
          "jogging back toward the soft limit range is allowed");

    MotorAxis above(SliderConfig());
    above.Feedback(12345, 0);
    const bool above_homed = above.HomeAt(kSliderTop + 0.05);
    above.Jog(0);
    const int above_stopped = above.Update(0.001, true, true);
    above.Jog(0.05);
    const int above_outward = above.Update(0.001, true, true);
    above.Jog(-0.05);
    Check(above_homed && above_stopped == 0 && above_outward == 0 &&
              above.Update(0.001, true, true) < 0,
          "above the upper soft limit jogging only moves back down");

    // 接近软限位时按距离减速，不再按一拍走到限位（那样等于增益 1/dt 的位置环，摆臂在限位上猛抖）。
    MotorAxis far(SliderConfig());
    far.Feedback(12345, 0);
    const bool far_homed = far.HomeAt(kSliderTop - 0.1);
    far.Jog(0.05);
    const int far_out = far.Update(0.001, true, true);
    MotorAxis near(SliderConfig());
    near.Feedback(12345, 0);
    const bool near_homed = near.HomeAt(kSliderTop - 0.001);
    near.Jog(0.05);
    const int near_out = near.Update(0.001, true, true);
    Check(far_homed && near_homed && near_out > 0 && near_out * 5 < far_out,
          "jogging slows down close to the soft limit");
    MotorAxis at_limit(SliderConfig());
    at_limit.Feedback(12345, 0);
    const bool at_limit_homed = at_limit.HomeAt(kSliderTop);
    at_limit.Jog(0.05);
    Check(at_limit_homed && at_limit.Update(0.001, true, true) == 0,
          "jogging stops at the soft limit");

    SliderPair pair;
    Check(!pair.Update(0.1, 0.12) && pair.Fault(), "slider sync fault before rebase");
    Check(!pair.Rebase(kNan, 0.12) && !pair.Rebase(0.1, kNan) && pair.Fault(),
          "invalid rebase preserves sync fault");
    Check(pair.Rebase(0.1, 0.12) && !pair.Fault() && pair.Update(0.1, 0.12),
          "rebase accepts current mismatch and clears fault");
    Check(pair.Update(0.2, 0.22) && pair.Update(0.2, 0.2249), "rebased pair tracks relative mismatch");
    Check(!pair.Update(0.2, 0.2251) && pair.Fault(), "rebased pair still detects increasing mismatch");
    Check(pair.Rebase(0.2, 0.22) && !pair.Update(0.2, 0.2149), "rebased pair detects opposite drift");
    Check(pair.Rebase(kSliderTop, kSliderTop) && pair.Update(kSliderTop, kSliderTop),
          "both homed sliders rebase to their new coordinates");

    const double return_time = (kSliderPark - kCockPosition) / kSliderMoveSpeed + 1;
    Check(return_time < kReturnTimeout && return_time < kPressTimeout,
          "default slider speed fits return and press timeouts with margin");
    Check((kSliderPark - kProbePosition) / kSliderMoveSpeed + 1 < kReturnTimeout,
          "default slider speed fits probe return timeout with margin");
}
}

// ---- 阶段 3c：比赛流程 ----
RefereeInput MatchReferee(Stage stage, double remaining = 300, bool gate_open = false) {
    RefereeInput referee;
    referee.online = true;
    referee.stage = stage;
    referee.stage_remaining = static_cast<std::uint16_t>(remaining);
    referee.gate_open = gate_open;
    return referee;
}

MatchMechanism ReadyMechanism() {
    MatchMechanism mechanism;
    mechanism.slider_homed = mechanism.aim_homed = mechanism.all_homed = true;
    mechanism.motors_online = true;
    mechanism.seated = true;
    mechanism.darts = 1;
    return mechanism;
}

RemoteInput MatchRemote(bool online, Switch right = Switch::Middle) {
    RemoteInput remote;
    remote.online = online;
    remote.right = right;
    return remote;
}

// 一个控制周期：遥控器先映射出命令，比赛流程再改写，顺序和适配层一致。
ModeCommand MatchTick(MatchFlow &flow, ModeLogic &mode, const RemoteInput &remote,
                      const RefereeInput &referee, const MatchMechanism &mechanism) {
    ModeCommand command = mode.Update(remote, referee, 0.01);
    flow.Update(command, remote, referee, mechanism, true, 0.01);
    return command;
}

const std::array<Stage, 7> kAllStages{{Stage::Unknown, Stage::NotStarted, Stage::Preparing,
                                       Stage::SelfCheck, Stage::Countdown, Stage::InMatch,
                                       Stage::Settlement}};

void TestMatchPredicates() {
    RefereeInput referee = MatchReferee(Stage::InMatch);
    Check(!MatchFlow::RemoteOptional(false, referee), "remote stays required when no referee was ever seen");
    for (Stage stage : kAllStages) {
        referee = MatchReferee(stage);
        const bool match = stage == Stage::InMatch || stage == Stage::Settlement;
        Check(MatchFlow::RemoteOptional(true, referee) == match,
              "remote is optional only during the match and settlement");
        const bool allowed = stage == Stage::NotStarted || stage == Stage::InMatch;
        Check(MatchFlow::LaunchAllowed(false, referee), "launch is allowed when no referee was ever seen");
        Check(MatchFlow::LaunchAllowed(true, referee) == allowed,
              "launch is allowed only in the not-started and match stages");
        referee.online = false;
        Check(!MatchFlow::RemoteOptional(true, referee) && !MatchFlow::LaunchAllowed(true, referee),
              "an offline referee neither frees the remote nor allows launch");
    }
    for (SafetyFault fault : {SafetyFault::Timeout, SafetyFault::ProbeFailed, SafetyFault::LatchFailed,
                              SafetyFault::PressBlocked, SafetyFault::NoBottom, SafetyFault::GateClosed,
                              SafetyFault::NotAimed, SafetyFault::NotParked, SafetyFault::RemoteOffline,
                              SafetyFault::MotorOffline, SafetyFault::RefereeOffline}) {
        Check(MatchFlow::Recoverable(fault), "transient cocking and firing faults may auto reset");
    }
    for (SafetyFault fault : {SafetyFault::None, SafetyFault::NotHomed, SafetyFault::NotLatched,
                              SafetyFault::CrankNotRetracted, SafetyFault::ArmInPath, SafetyFault::SliderSync,
                              SafetyFault::NoDarts, SafetyFault::QueueOverflow, SafetyFault::InvalidInput,
                              SafetyFault::HomeOverrun, SafetyFault::OutOfMatch, SafetyFault::ReleaseInterrupted}) {
        Check(!MatchFlow::Recoverable(fault), "faults that need a person never auto reset");
    }
}

void TestMatchOutsideMatch() {
    MatchMechanism idle = ReadyMechanism();
    MatchMechanism cocked = idle;
    cocked.latch = LatchState::Latched;
    {
        // 没收到过裁判系统：什么都不改，保持调试行为。
        MatchFlow flow;
        ModeCommand command;
        command.mode = Mode::Manual;
        command.slider_rate = 0.05;
        command.cock_request = command.fire = command.stop_sequence = true;
        flow.Update(command, MatchRemote(true), MatchReferee(Stage::InMatch), idle, false, 0.01);
        Check(command.mode == Mode::Manual && Near(command.slider_rate, 0.05) && command.cock_request &&
                  command.fire && command.stop_sequence && !command.match_override && !command.cock_refused &&
                  !command.fire_refused && !command.estop && !flow.Overriding(),
              "without a referee the match flow changes nothing");
    }
    {
        // “未开始”阶段：留给调试，不限制。
        MatchFlow flow;
        ModeCommand command;
        command.mode = Mode::Manual;
        command.cock_request = command.fire = command.salvo = true;
        flow.Update(command, MatchRemote(true), MatchReferee(Stage::NotStarted), idle, true, 0.01);
        Check(command.cock_request && command.fire && command.salvo && !command.cock_refused &&
                  !command.fire_refused && !command.match_override,
              "the not-started stage stays unrestricted for debugging");
    }
    for (Stage stage : {Stage::Preparing, Stage::SelfCheck, Stage::Countdown, Stage::Unknown}) {
        MatchFlow flow;
        ModeCommand command;
        command.mode = Mode::Manual;
        command.home_request = command.cock_request = command.fire = command.salvo = true;
        flow.Update(command, MatchRemote(true), MatchReferee(stage), idle, true, 0.01);
        Check(!command.cock_request && command.cock_refused && !command.fire && !command.salvo &&
                  command.fire_refused,
              "cocking and firing are refused before the match and during settlement checks");
        Check(command.mode == Mode::Manual && command.home_request && !command.match_override &&
                  !command.estop && !command.cocked_warning,
              "before the match the remote keeps its debug behaviour, homing included");
        ModeCommand decock;
        decock.cock_request = true;
        flow.Update(decock, MatchRemote(true), MatchReferee(stage), cocked, true, 0.01);
        Check(decock.cock_request && !decock.cock_refused && decock.cocked_warning && !decock.auto_decock &&
                  !decock.auto_cock,
              "a launcher found cocked before the match keeps the decock gesture and only warns");
        MatchMechanism running = cocked;
        running.busy = true;
        ModeCommand cancel;
        flow.Update(cancel, MatchRemote(true), MatchReferee(stage), running, true, 0.01);
        Check(cancel.stop_sequence, "a running cock or fire sequence is cancelled before the match");
        running.decocking = true;
        ModeCommand keep;
        flow.Update(keep, MatchRemote(true), MatchReferee(stage), running, true, 0.01);
        Check(!keep.stop_sequence, "a running decock is left alone before the match");
    }
}

void TestMatchTakeover() {
    MatchMechanism cocked = ReadyMechanism();
    cocked.latch = LatchState::Latched;
    const RefereeInput match = MatchReferee(Stage::InMatch, 300);
    {
        // 遥控器不在线：比赛流程照常，不会停流程。
        MatchFlow flow;
        ModeLogic mode(true);
        ModeCommand command = MatchTick(flow, mode, MatchRemote(false), match, cocked);
        Check(command.match_override && command.mode == Mode::Automatic && !command.stop_sequence &&
                  command.leave_manual && !command.estop && flow.Overriding(),
              "an offline remote neither stops the match flow nor powers it down");
        command = MatchTick(flow, mode, MatchRemote(false), match, cocked);
        Check(!command.leave_manual, "leave_manual is sent only on the first taken-over tick");
    }
    {
        // 遥控器在线、手动档、满杆、拨轮：全部不理。
        MatchFlow flow;
        ModeLogic mode(true);
        RemoteInput remote = MatchRemote(true, Switch::Middle);
        remote.left = Switch::Middle;
        remote.left_x = 0.8;
        remote.right_y = 0.8;
        remote.wheel = 1;
        bool ignored = true;
        for (int tick = 0; tick < 300; ++tick) {
            const ModeCommand command = MatchTick(flow, mode, remote, match, cocked);
            ignored = ignored && command.mode == Mode::Automatic && command.slider_rate == 0 &&
                command.lock_rate == 0 && command.pitch_rate == 0 && command.yaw_rate == 0 &&
                !command.home_request && !command.cock_request && !command.magnet_toggle &&
                !command.fire && !command.salvo && !command.estop && !command.stop_sequence;
        }
        Check(ignored, "sticks wheel and switches are ignored during the match");
    }
    {
        MatchFlow flow;
        ModeLogic mode(true);
        RemoteInput remote = MatchRemote(true, Switch::Middle);
        remote.left = Switch::Down;
        remote.wheel = 1;
        bool ignored = true;
        for (int tick = 0; tick < 300; ++tick) {
            ignored = ignored && !MatchTick(flow, mode, remote, match, cocked).fetch_request;
        }
        Check(ignored, "the fetch request is ignored during the match");
    }
    {
        // 开赛时右拨杆本来就在下，不算急停；离开再拨到下才算。
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Down), MatchReferee(Stage::Countdown), cocked);
        ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(!command.estop && command.mode == Mode::Automatic,
              "a switch already down at the start is not an emergency stop");
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(command.estop && command.mode == Mode::Powerless && command.stop_sequence &&
                  !command.auto_decock && !command.fire && flow.EmergencyStop(),
              "moving the switch down after the start is an emergency stop");
        bool still = true;
        for (int tick = 0; tick < 110; ++tick) {
            command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
            still = still && command.estop && command.mode == Mode::Powerless;
        }
        Check(still, "the emergency stop stays latched while the switch is held down");
        command = MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        Check(command.fault_reset && !command.estop && command.mode == Mode::Automatic,
              "holding down one second then back to middle clears the emergency stop");
    }
    {
        // 遥控器比赛中途才上线、拨杆就在下：按急停处理。
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(false), match, cocked);
        const ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(command.estop && command.mode == Mode::Powerless,
              "a remote that comes online mid-match with the switch down is an emergency stop");
    }
    {
        // 开赛前拨到下不是急停，也不会带进比赛。
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), MatchReferee(Stage::Preparing), cocked);
        ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), MatchReferee(Stage::Preparing), cocked);
        Check(!command.estop && command.mode == Mode::Powerless && !command.match_override,
              "switching down before the match is plain powerless, not an emergency stop");
        command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(!command.estop && command.mode == Mode::Automatic,
              "the match flow runs even though the switch was left down before the start");
    }
    {
        // 结算时接管只为退膛：没有要退的膛，遥控器照常。
        MatchFlow flow;
        MatchMechanism idle = ReadyMechanism();
        ModeCommand command;
        command.mode = Mode::Manual;
        command.slider_rate = 0.05;
        command.fire = command.cock_request = true;
        flow.Update(command, MatchRemote(true), MatchReferee(Stage::Settlement), idle, true, 0.01);
        Check(!command.match_override && command.mode == Mode::Manual && Near(command.slider_rate, 0.05) &&
                  !command.fire && command.fire_refused && !command.cock_request && command.cock_refused,
              "settlement without a cocked launcher leaves the remote alone but refuses cocking and firing");
        // 上着膛：接管并退膛，遥控器不在线也一样。
        ModeCommand decock;
        decock.stop_sequence = true;
        flow.Update(decock, MatchRemote(false), MatchReferee(Stage::Settlement), cocked, true, 0.01);
        Check(decock.match_override && decock.mode == Mode::Automatic && decock.auto_decock && !decock.stop_sequence,
              "settlement with a cocked launcher decocks immediately even with the remote offline");
        MatchMechanism running = cocked;
        running.busy = true;
        ModeCommand cancel;
        flow.Update(cancel, MatchRemote(false), MatchReferee(Stage::Settlement), running, true, 0.01);
        Check(cancel.stop_sequence && !cancel.auto_decock, "settlement cancels a running cock or fire sequence");
        running.decocking = true;
        ModeCommand keep;
        flow.Update(keep, MatchRemote(false), MatchReferee(Stage::Settlement), running, true, 0.01);
        Check(!keep.stop_sequence && keep.match_override && !keep.auto_decock,
              "settlement lets a running decock finish");
    }
}

// 在 ticks 个周期里统计比赛流程发出某个请求的次数；每个周期 0.01 s。
template <typename Pick>
int CountRequests(MatchFlow &flow, const RemoteInput &remote, const RefereeInput &referee,
                  const MatchMechanism &mechanism, int ticks, Pick pick) {
    int count = 0;
    for (int tick = 0; tick < ticks; ++tick) {
        ModeCommand command;
        flow.Update(command, remote, referee, mechanism, true, 0.01);
        if (pick(command)) { ++count; }
    }
    return count;
}

void TestMatchRequests() {
    const RemoteInput offline = MatchRemote(false);
    const auto home_slider = [](const ModeCommand &c) { return c.auto_home_slider; };
    const auto home_aim = [](const ModeCommand &c) { return c.auto_home_aim; };
    const auto cock = [](const ModeCommand &c) { return c.auto_cock; };
    const auto decock = [](const ModeCommand &c) { return c.auto_decock; };
    const auto fire = [](const ModeCommand &c) { return c.fire; };
    const auto anything = [](const ModeCommand &c) {
        return c.auto_home_slider || c.auto_home_aim || c.auto_cock || c.auto_decock || c.fire || c.auto_reset;
    };
    {
        // 没回零：自动回零，被拒绝才隔 2 s 重发；正在回零或流程在跑就不发；回零前不上膛。
        MatchMechanism m = ReadyMechanism();
        m.slider_homed = m.aim_homed = m.all_homed = false;
        MatchFlow flow;
        ModeCommand first;
        flow.Update(first, offline, MatchReferee(Stage::InMatch), m, true, 0.01);
        Check(first.auto_home_slider && first.auto_home_aim && !first.auto_cock,
              "the match start homes axes that are not homed yet");
        MatchFlow again;
        Check(CountRequests(again, offline, MatchReferee(Stage::InMatch), m, 250, home_slider) == 2,
              "a refused homing request repeats only every two seconds");
        m.aim_homed = true;
        MatchFlow slider_only;
        ModeCommand partial;
        slider_only.Update(partial, offline, MatchReferee(Stage::InMatch), m, true, 0.01);
        Check(partial.auto_home_slider && !partial.auto_home_aim, "only the axes that are not homed are homed");
        m.homing = true;
        MatchFlow homing;
        Check(CountRequests(homing, offline, MatchReferee(Stage::InMatch), m, 300, anything) == 0,
              "nothing is requested while an axis is homing");
        m = ReadyMechanism();
        m.slider_homed = m.aim_homed = false;
        m.busy = true;
        MatchFlow busy;
        Check(CountRequests(busy, offline, MatchReferee(Stage::InMatch), m, 300, anything) == 0,
              "nothing is requested while a sequence runs");
        m = ReadyMechanism();
        m.slider_homed = m.aim_homed = false;
        MatchFlow aim;
        Check(CountRequests(aim, offline, MatchReferee(Stage::InMatch), m, 10, home_aim) == 1,
              "the aim axes are homed too");
    }
    {
        // 上膛：回零好、有镖、剩余时间够。
        MatchMechanism m = ReadyMechanism();
        MatchFlow flow;
        Check(CountRequests(flow, offline, MatchReferee(Stage::InMatch, 300), m, 250, cock) == 2,
              "an uncocked launcher with a dart is cocked, and a refused request repeats after two seconds");
        const double cutoff = kMatchCockCutoff;
        MatchFlow late;
        Check(CountRequests(late, offline, MatchReferee(Stage::InMatch, cutoff), m, 10, cock) == 0,
              "no cocking is started once less than the cutoff time remains");
        MatchFlow just;
        Check(CountRequests(just, offline, MatchReferee(Stage::InMatch, cutoff + 1), m, 10, cock) == 1,
              "cocking is still started just above the cutoff time");
        m.darts = 0;
        MatchFlow empty;
        Check(CountRequests(empty, offline, MatchReferee(Stage::InMatch, 300), m, 10, anything) == 0,
              "an empty launcher is never cocked again");
        m = ReadyMechanism();
        m.latch = LatchState::Released;
        MatchFlow released;
        Check(CountRequests(released, offline, MatchReferee(Stage::InMatch, 300), m, 10, cock) == 1,
              "a released launcher with a dart is cocked");
    }
    {
        // 发射：上了膛、闸门全开、镖在位。
        MatchMechanism m = ReadyMechanism();
        m.latch = LatchState::Latched;
        MatchFlow closed;
        Check(CountRequests(closed, offline, MatchReferee(Stage::InMatch, 300, false), m, 300, anything) == 0,
              "a cocked launcher waits while the gate is closed");
        MatchFlow open;
        Check(CountRequests(open, offline, MatchReferee(Stage::InMatch, 300, true), m, 250, fire) == 2,
              "a cocked launcher fires when the gate is open and a refused request repeats after two seconds");
        MatchFlow last;
        const int last_requests = CountRequests(last, offline, MatchReferee(Stage::InMatch, 5, true), m, 10, fire);
        Check(last_requests == 1, "firing wins over decocking in the last seconds when the gate is open");
        MatchMechanism unseated = m;
        unseated.seated = false;
        MatchFlow no_dart;
        Check(CountRequests(no_dart, offline, MatchReferee(Stage::InMatch, 300, true), unseated, 10, anything) == 0,
              "no dart on the launcher means no shot");
        MatchMechanism busy = m;
        busy.busy = true;
        MatchFlow running;
        Check(CountRequests(running, offline, MatchReferee(Stage::InMatch, 300, true), busy, 10, anything) == 0,
              "no shot is requested while a sequence runs");
    }
    {
        // 退膛：快结束了还上着膛、闸门没开。
        MatchMechanism m = ReadyMechanism();
        m.latch = LatchState::Latched;
        const double lead = kMatchDecockLead;
        MatchFlow edge;
        Check(CountRequests(edge, offline, MatchReferee(Stage::InMatch, lead), m, 10, decock) == 1,
              "a cocked launcher is decocked once the lead time is reached");
        MatchFlow early;
        Check(CountRequests(early, offline, MatchReferee(Stage::InMatch, lead + 1), m, 10, decock) == 0,
              "a cocked launcher keeps waiting just before the lead time");
        MatchMechanism unseated = m;
        unseated.seated = false;
        MatchFlow stuck;
        Check(CountRequests(stuck, offline, MatchReferee(Stage::InMatch, 10, true), unseated, 10, decock) == 1,
              "a cocked launcher that cannot fire is decocked even with the gate open");
    }
}

void TestMatchFaults() {
    const RefereeInput match = MatchReferee(Stage::InMatch, 300);
    const auto reset = [](const ModeCommand &c) { return c.auto_reset; };
    const auto anything = [](const ModeCommand &c) {
        return c.auto_home_slider || c.auto_home_aim || c.auto_cock || c.auto_decock || c.fire;
    };
    MatchMechanism faulted = ReadyMechanism();
    faulted.fault = SafetyFault::Timeout;
    {
        MatchFlow flow;
        Check(CountRequests(flow, MatchRemote(false), match, faulted, 1000, reset) == kMatchAutoRetries &&
                  flow.Retries() == kMatchAutoRetries,
              "with the remote offline a recoverable fault is reset a limited number of times");
        MatchFlow wait;
        Check(CountRequests(wait, MatchRemote(false), match, faulted, static_cast<int>(kMatchRetryDelay * 100) - 5, reset) == 0,
              "the automatic reset waits for the machine to settle");
        MatchFlow silent;
        Check(CountRequests(silent, MatchRemote(false), match, faulted, 1000, anything) == 0,
              "a faulted machine requests nothing else");
        // 新一场开始（回到准备阶段）次数重置。
        ModeCommand command;
        flow.Update(command, MatchRemote(false), MatchReferee(Stage::Preparing), faulted, true, 0.01);
        Check(flow.Retries() == 0 &&
                  CountRequests(flow, MatchRemote(false), match, faulted, 1000, reset) == kMatchAutoRetries,
              "the retry budget comes back for the next match");
    }
    {
        MatchFlow online;
        Check(CountRequests(online, MatchRemote(true), match, faulted, 1000, reset) == 0,
              "with the remote online the commander resets faults, not the machine");
        for (SafetyFault fault : {SafetyFault::SliderSync, SafetyFault::InvalidInput, SafetyFault::HomeOverrun,
                                  SafetyFault::NoDarts, SafetyFault::NotLatched, SafetyFault::OutOfMatch}) {
            MatchMechanism hard = faulted;
            hard.fault = fault;
            MatchFlow flow;
            Check(CountRequests(flow, MatchRemote(false), match, hard, 1000, reset) == 0,
                  "faults that need a person are never reset automatically");
        }
        MatchMechanism dead = faulted;
        dead.motors_online = false;
        MatchFlow motors;
        Check(CountRequests(motors, MatchRemote(false), match, dead, 1000, reset) == 0,
              "no automatic reset while motors are offline");
        RefereeInput silent = match;
        silent.online = false;
        MatchFlow referee_gone;
        Check(CountRequests(referee_gone, MatchRemote(false), silent, faulted, 1000, reset) == 0,
              "no automatic reset while the referee is offline");
    }
}

// 把 FakeIo、库存、发射流程、模式映射和比赛流程接成一圈，按适配层 Tick 的顺序执行。
struct MatchRig {
    MatchRig() : sequence(io, inventory), mode(true) {
        inventory.ResetManual();
        referee.online = true;
    }
    void Step() {
        io.Advance();
        io.state.remote_online = remote.online;
        io.state.competition = true;
        io.state.referee_online = referee.online;
        io.state.gate_open = referee.gate_open;
        io.state.remote_required = !MatchFlow::RemoteOptional(true, referee);
        io.state.launch_allowed = MatchFlow::LaunchAllowed(true, referee);
        MatchMechanism mechanism;
        mechanism.slider_homed = io.state.slider_homed;
        mechanism.aim_homed = true;
        mechanism.all_homed = io.state.all_homed;
        mechanism.motors_online = io.state.motors_online;
        mechanism.busy = sequence.Busy();
        mechanism.decocking = sequence.Decocking();
        mechanism.cocking = sequence.Cocking();
        mechanism.seated = inventory.Seated();
        mechanism.latch = io.state.latch_state;
        mechanism.fault = sequence.Fault();
        mechanism.darts = inventory.Remaining();
        ModeCommand command = mode.Update(remote, referee, 0.01);
        flow.Update(command, remote, referee, mechanism, true, 0.01);
        if (command.estop) { ++estop_ticks; }
        if ((command.fault_reset || command.auto_reset) && sequence.Fault() != SafetyFault::None) {
            sequence.Reset();
            ++resets;
        }
        if (command.stop_sequence && sequence.Busy()) { sequence.Cancel(); }
        if (sequence.Fault() == SafetyFault::None) {
            if (command.auto_cock && !sequence.Busy()) {
                ++cock_requests;
                sequence.CommandCock();
            }
            if (command.auto_decock && !sequence.Busy()) {
                ++decock_requests;
                sequence.CommandDecock();
            }
            if (command.fire && !sequence.Busy() && io.state.launch_allowed) {
                ++fire_requests;
                sequence.CommandFire();
            }
        }
        if (command.cocked_warning) { ++warnings; }
        if (sequence.Busy()) { sequence.Tick(now, command.mode != Mode::Powerless); }
        now += 0.01;
    }
    void Run(int ticks) {
        for (int tick = 0; tick < ticks; ++tick) { Step(); }
    }
    FakeIo io;
    DartInventory inventory;
    LaunchSequence sequence;
    ModeLogic mode;
    MatchFlow flow;
    RemoteInput remote = MatchRemote(true, Switch::Middle);
    RefereeInput referee;
    double now = 0;
    int cock_requests = 0, decock_requests = 0, fire_requests = 0, resets = 0, warnings = 0, estop_ticks = 0;
};

void TestMatchRig() {
    {
        // 一整场：准备阶段不上膛；开赛自动上膛；闸门没开等着；闸门全开自动发射；没镖了不再上膛。
        MatchRig rig;
        rig.io.state.latch_state = LatchState::Released;
        rig.io.state.slider = kCockPosition;
        rig.referee.stage = Stage::Preparing;
        rig.referee.stage_remaining = 120;
        rig.Run(500);
        Check(rig.cock_requests == 0 && !rig.sequence.Busy() && rig.io.state.latch_state == LatchState::Released,
              "nothing is cocked during preparation");
        rig.referee.stage = Stage::InMatch;
        rig.referee.stage_remaining = 400;
        rig.remote.online = false;
        rig.Run(3000);
        Check(rig.cock_requests == 1 && rig.sequence.Fault() == SafetyFault::None && !rig.sequence.Busy() &&
                  rig.io.state.latch_state == LatchState::Latched && rig.io.unlocks == 0,
              "the match start cocks the launcher once, with the remote offline");
        rig.Run(1000);
        Check(rig.io.unlocks == 0 && rig.fire_requests == 0 && rig.cock_requests == 1,
              "a cocked launcher waits for the gate");
        rig.referee.gate_open = true;
        rig.Run(1500);
        Check(rig.fire_requests == 1 && rig.io.unlocks == 1 && rig.sequence.Fault() == SafetyFault::None &&
                  rig.inventory.Remaining() == 0 && rig.io.state.latch_state == LatchState::Released,
              "the open gate fires exactly one shot");
        rig.Run(3000);
        Check(rig.cock_requests == 1 && rig.fire_requests == 1 && rig.decock_requests == 0,
              "an empty launcher is neither cocked nor decocked again");
    }
    {
        // 快结束：上着膛、闸门没开，自动退膛，不发射。
        MatchRig rig;
        rig.referee.stage = Stage::InMatch;
        rig.referee.stage_remaining = 15;
        rig.remote.online = false;
        rig.Run(3000);
        Check(rig.decock_requests == 1 && rig.io.unlocks == 0 && rig.fire_requests == 0 &&
                  rig.sequence.Fault() == SafetyFault::None && rig.io.state.latch_state == LatchState::Released &&
                  Near(rig.io.state.slider, kSliderPark),
              "a launcher still cocked near the end is decocked without firing");
    }
    {
        // 结算：上着膛，遥控器不在线也退膛，不会因为遥控器离线报故障。
        MatchRig rig;
        rig.referee.stage = Stage::Settlement;
        rig.remote.online = false;
        rig.Run(3000);
        Check(rig.decock_requests == 1 && rig.sequence.Fault() == SafetyFault::None &&
                  rig.io.state.latch_state == LatchState::Released && rig.io.unlocks == 0,
              "settlement decocks a cocked launcher even with the remote offline");
    }
    {
        // 倒计时里发现上着膛：只报警，不动。
        MatchRig rig;
        rig.referee.stage = Stage::Countdown;
        rig.Run(2000);
        Check(rig.decock_requests == 0 && rig.cock_requests == 0 && !rig.sequence.Busy() && rig.warnings > 0 &&
                  rig.io.state.latch_state == LatchState::Latched,
              "a launcher found cocked before the start is only warned about");
    }
    {
        // 遥控器急停：停掉正在跑的上膛流程。
        MatchRig rig;
        rig.io.state.latch_state = LatchState::Released;
        rig.io.state.slider = kCockPosition;
        rig.referee.stage = Stage::InMatch;
        rig.referee.stage_remaining = 400;
        rig.Run(50);
        Check(rig.sequence.Busy() && rig.cock_requests == 1, "the cocking sequence is running");
        rig.remote.right = Switch::Down;
        rig.Run(5);
        Check(!rig.sequence.Busy() && rig.estop_ticks > 0, "the emergency stop cancels the running sequence");
        const int requests = rig.cock_requests;
        rig.Run(500);
        Check(rig.cock_requests == requests && !rig.sequence.Busy(), "nothing restarts while the emergency stop is held");
    }
    {
        // 上膛卡住超时：遥控器不在线时自动复位重试，次数有限；遥控器在线时不自动复位。
        for (bool remote_online : {false, true}) {
            MatchRig rig;
            rig.io.state.latch_state = LatchState::Released;
            rig.io.state.slider = kCockPosition;
            rig.io.press_blocked = true;
            rig.referee.stage = Stage::InMatch;
            rig.referee.stage_remaining = 400;
            rig.remote.online = remote_online;
            rig.Run(6000);
            if (remote_online) {
                Check(rig.resets == 0 && rig.cock_requests == 1 && rig.sequence.Fault() == SafetyFault::Timeout,
                      "with the remote online a stuck cocking stays faulted for the commander to reset");
            } else {
                Check(rig.resets == kMatchAutoRetries && rig.cock_requests == kMatchAutoRetries + 1 &&
                          rig.sequence.Fault() == SafetyFault::Timeout,
                      "with the remote offline a stuck cocking is retried a limited number of times");
            }
        }
    }
}

void TestMatchSafety() {
    Safety safety;
    for (int index = 0; index < 5; ++index) { safety.MotorFeedback(index, 1.0); }
    safety.RemoteFeedback(0.5);
    Check(safety.Check(1.0, false) == SafetyFault::RemoteOffline, "a stale remote faults by default");
    Check(safety.Check(1.0, false, false) == SafetyFault::None, "a stale remote is ignored when it is not required");
    Safety never;
    for (int index = 0; index < 5; ++index) { never.MotorFeedback(index, 1.0); }
    Check(never.Check(1.0, false, false) == SafetyFault::None, "a remote that never connected is ignored when not required");
    Check(never.Check(1.0, false, true) == SafetyFault::RemoteOffline, "a remote that never connected faults when required");

    SafetyState good;
    good.remote_online = true;
    good.motors_online = true;
    good.all_homed = true;
    good.latch_state = LatchState::Latched;
    good.seated = true;
    good.arm = kStandby;
    good.aimed = true;
    good.slider = kSliderPark;
    good.competition = true;
    good.referee_online = true;
    good.gate_open = true;
    Check(Safety::FireBlocker(good) == SafetyFault::None, "a match launch snapshot passes");
    good.remote_online = false;
    Check(Safety::FireBlocker(good) == SafetyFault::RemoteOffline, "an offline remote blocks launch by default");
    good.remote_required = false;
    Check(Safety::FireBlocker(good) == SafetyFault::None, "an offline remote does not block launch when not required");
    good.launch_allowed = false;
    Check(Safety::FireBlocker(good) == SafetyFault::OutOfMatch, "a stage that forbids launching blocks the shot");
    good.referee_online = false;
    Check(Safety::FireBlocker(good) == SafetyFault::RefereeOffline, "an offline referee is reported before the stage");
    good.referee_online = true;
    good.launch_allowed = true;
    good.gate_open = false;
    Check(Safety::FireBlocker(good) == SafetyFault::GateClosed, "a closed gate still blocks launch");
    Check(std::string(Safety::Message(SafetyFault::OutOfMatch)) == "当前比赛阶段不许发射", "the stage fault has a message");

    // 发射流程：阶段不对，CommandFire 直接拒绝。
    FakeIo io;
    io.state.competition = true;
    io.state.referee_online = true;
    io.state.gate_open = true;
    io.state.launch_allowed = false;
    DartInventory inventory;
    inventory.ResetManual();
    LaunchSequence sequence(io, inventory);
    Check(!sequence.CommandFire() && sequence.Fault() == SafetyFault::OutOfMatch, "firing outside the allowed stages is refused");

    // 遥控器不在线、也不要求在线：流程照走；要求在线则报遥控器离线。
    for (bool required : {false, true}) {
        FakeIo remote_io;
        remote_io.state.remote_online = false;
        remote_io.state.remote_required = required;
        DartInventory remote_inventory;
        LaunchSequence decock(remote_io, remote_inventory);
        Check(decock.CommandDecock(), "decock is accepted with the remote offline");
        RunSequence(decock, remote_io);
        if (required) {
            Check(decock.Fault() == SafetyFault::RemoteOffline, "a required remote that is offline fails the sequence");
        } else {
            Check(decock.Fault() == SafetyFault::None && remote_io.state.latch_state == LatchState::Released,
                  "a sequence runs to the end with the remote offline when it is not required");
        }
    }

    // Decocking 只在退膛流程里为真，取消、完成、故障以后都清掉。
    FakeIo flag_io;
    DartInventory flag_inventory;
    LaunchSequence flag(flag_io, flag_inventory);
    Check(!flag.Decocking(), "an idle sequence is not decocking");
    Check(flag.CommandDecock() && flag.Decocking(), "a decock command marks the sequence as decocking");
    flag.Cancel();
    Check(!flag.Decocking(), "cancelling clears the decocking mark");
    Check(flag.CommandDecock(), "decock is accepted again");
    RunSequence(flag, flag_io);
    Check(!flag.Decocking() && !flag.Busy(), "a finished decock is no longer decocking");
    FakeIo cock_io;
    cock_io.state.latch_state = LatchState::Released;
    cock_io.state.slider = kCockPosition;
    DartInventory cock_inventory;
    LaunchSequence cock(cock_io, cock_inventory);
    Check(cock.CommandCock() && !cock.Decocking(), "a cock command is not a decock");
    RunSequence(cock, cock_io);
    Check(!cock.Decocking() && cock_io.state.latch_state == LatchState::Latched, "cocking never reports decocking");
}

void TestRefereeTracker() {
    RefereeInput out;
    RefereeTracker tracker;
    RefereeRaw raw; // 模块里还什么都没收到：全是初始值 0
    tracker.Update(raw, 0.5, out);
    Check(!out.gate_open && out.stage == Stage::Unknown && out.stage_remaining == 0 && !out.dart_valid &&
              out.launch_time == 0,
          "nothing received yet: the zero gate status must not read as open");
    // 0x0001 先到，0x020A 还没到：阶段有效，闸门仍不算开，因为初始值 0 就是“已开启”。
    raw.gs_seq = 1;
    raw.game_progress = 4;
    raw.stage_remain_time = 380;
    tracker.Update(raw, 1.0, out);
    Check(out.stage == Stage::InMatch && out.stage_remaining == 380 && !out.gate_open,
          "the stage is valid before any 0x020A and the gate still does not read as open");
    // 0x020A 到了，状态 0：全开；超过 2 s 没新帧就不算。
    raw.dc_seq = 1;
    raw.opening_status = 0;
    tracker.Update(raw, 1.0, out);
    Check(out.gate_open, "a received 0x020A with status 0 is an open gate");
    tracker.Update(raw, 1.0 + kRefereeGateStale, out);
    Check(out.gate_open, "the gate stays open exactly up to the stale limit");
    tracker.Update(raw, 1.0 + kRefereeGateStale + 0.01, out);
    Check(!out.gate_open, "a gate not refreshed for more than the stale limit is not open");
    raw.dc_seq = 2;
    tracker.Update(raw, 10.0, out);
    Check(out.gate_open, "a new frame refreshes the gate");
    // 状态 1（关闭）、2（正在开关）都不算开；发射指令时间照传。
    for (int status : {1, 2}) {
        raw.dc_seq = static_cast<std::uint8_t>(raw.dc_seq + 1);
        raw.opening_status = static_cast<std::uint8_t>(status);
        raw.launch_time = 123;
        tracker.Update(raw, 10.0, out);
        Check(!out.gate_open && out.launch_time == 123,
              "a closed or moving gate is not open and the launch time passes through");
    }
    // 序号回绕（255 到 0）也是新帧。
    RefereeTracker wrap;
    raw = RefereeRaw{};
    raw.dc_seq = 255;
    wrap.Update(raw, 1.0, out);
    Check(out.gate_open, "sequence 255 counts as received");
    raw.dc_seq = 0;
    wrap.Update(raw, 10.0, out);
    Check(out.gate_open, "a sequence that wraps to 0 is a new frame");
    // 比赛阶段：超过 5 s 没新帧就是未知，0~5 以外也是未知。
    RefereeTracker stage;
    raw = RefereeRaw{};
    raw.gs_seq = 7;
    raw.game_progress = 5;
    raw.stage_remain_time = 40;
    stage.Update(raw, 100.0, out);
    Check(out.stage == Stage::Settlement && out.stage_remaining == 40, "settlement maps from 5");
    stage.Update(raw, 100.0 + kRefereeStageStale, out);
    Check(out.stage == Stage::Settlement, "the stage stays valid up to the stale limit");
    stage.Update(raw, 100.0 + kRefereeStageStale + 0.01, out);
    Check(out.stage == Stage::Unknown && out.stage_remaining == 0, "a stage not refreshed for too long is unknown");
    for (int progress : {6, 9, 15}) {
        raw.gs_seq = static_cast<std::uint8_t>(raw.gs_seq + 1);
        raw.game_progress = static_cast<std::uint8_t>(progress);
        stage.Update(raw, 200.0, out);
        Check(out.stage == Stage::Unknown, "a stage code outside 0..5 is unknown");
    }
    for (int progress = 0; progress <= 5; ++progress) {
        raw.gs_seq = static_cast<std::uint8_t>(raw.gs_seq + 1);
        raw.game_progress = static_cast<std::uint8_t>(progress);
        stage.Update(raw, 201.0, out);
        Check(static_cast<int>(out.stage) == progress, "stage numbers follow the protocol");
    }
    // 0x0105：显示用，同样靠序号和新旧。
    RefereeTracker dart;
    raw = RefereeRaw{};
    dart.Update(raw, 1.0, out);
    Check(!out.dart_valid && out.dart_target == 0, "0x0105 is not valid before it is received");
    raw.dart_seq = 1;
    raw.dart_countdown = 30;
    raw.dart_target = 4;
    raw.dart_last_target = 6;
    raw.dart_hits = 4;
    dart.Update(raw, 2.0, out);
    Check(out.dart_valid && out.dart_countdown == 30 && out.dart_target == 4 && out.dart_last_target == 6 &&
              out.dart_hits == 4,
          "0x0105 fields pass through, including the terminal moving target 4");
    dart.Update(raw, 2.0 + kRefereeStageStale + 0.01, out);
    Check(!out.dart_valid && out.dart_countdown == 0 && out.dart_target == 0, "stale 0x0105 data is dropped");
}

// 独立审查（2026-10-31）指出的问题，每条一个回归检查。
void TestMatchReviewFixes() {
    MatchMechanism cocked = ReadyMechanism();
    cocked.latch = LatchState::Latched;
    const RefereeInput match = MatchReferee(Stage::InMatch, 300);
    RefereeInput offline_referee = match;
    offline_referee.online = false; // 裁判系统掉线
    RefereeInput stale_stage = match;
    stale_stage.stage = Stage::Unknown; // 在线，但 0x0001 太久没更新

    // 1. 急停锁存不能被裁判系统掉线、阶段变未知清掉。
    for (const RefereeInput &gap : {offline_referee, stale_stage}) {
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        Check(MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked).estop,
              "moving the switch down during the match latches the emergency stop");
        for (int tick = 0; tick < 20; ++tick) {
            MatchTick(flow, mode, MatchRemote(true, Switch::Down), gap, cocked);
        }
        ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(command.estop && command.mode == Mode::Powerless && flow.EmergencyStop() && command.stop_sequence,
              "an emergency stop survives the referee dropping out and coming back with the switch still down");
        for (int tick = 0; tick < 110; ++tick) {
            MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        }
        command = MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        Check(command.fault_reset && !command.estop && command.mode == Mode::Automatic,
              "the reset gesture clears an emergency stop that lived through a referee dropout");
    }
    {
        // 阶段未知时拨到下，也要记下来。
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), offline_referee, cocked);
        MatchTick(flow, mode, MatchRemote(true, Switch::Down), offline_referee, cocked);
        const ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(command.estop && command.mode == Mode::Powerless,
              "a switch moved down while the stage was unknown still counts once the match is back");
    }
    {
        // 下一场开始（准备阶段）才清掉；新一场开赛时拨杆一直在下不算急停。
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        Check(MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked).estop, "emergency stop latched");
        ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), MatchReferee(Stage::Preparing), cocked);
        Check(!command.estop && !flow.EmergencyStop(), "the next match starts without the emergency stop");
        command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(!command.estop && command.mode == Mode::Automatic,
              "a switch left down into the next match is not an emergency stop");
    }

    // 2. 遥控器断线重连：拨杆还在下不算急停；断线期间拨到了下才算。
    {
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Down), MatchReferee(Stage::Countdown), cocked);
        MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        for (int tick = 0; tick < 15; ++tick) {
            MatchTick(flow, mode, MatchRemote(false, Switch::Down), match, cocked);
        }
        const ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(!command.estop && command.mode == Mode::Automatic,
              "a remote that drops and returns with the switch still down is not an emergency stop");
    }
    {
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        for (int tick = 0; tick < 15; ++tick) {
            MatchTick(flow, mode, MatchRemote(false, Switch::Middle), match, cocked);
        }
        const ModeCommand command = MatchTick(flow, mode, MatchRemote(true, Switch::Down), match, cocked);
        Check(command.estop, "a switch that was moved down while the remote was offline is an emergency stop");
    }

    // 3. 结算时正在跑的上膛不取消，做完再退膛；退膛前轴没回零先回零。
    {
        MatchFlow flow;
        MatchMechanism cocking = ReadyMechanism();
        cocking.busy = true;
        cocking.cocking = true;
        ModeCommand command;
        command.stop_sequence = true; // 遥控器不在线时 ModeLogic 会这样要求
        flow.Update(command, MatchRemote(false), MatchReferee(Stage::Settlement), cocking, true, 0.01);
        Check(command.match_override && !command.stop_sequence && !command.auto_decock,
              "a cock still running at settlement is left to finish");
        ModeCommand next;
        flow.Update(next, MatchRemote(false), MatchReferee(Stage::Settlement), cocked, true, 0.01);
        Check(next.auto_decock, "once the cock finished at settlement the launcher is decocked");
    }
    {
        MatchFlow flow;
        MatchMechanism lost = cocked;
        lost.slider_homed = false;
        lost.all_homed = false;
        ModeCommand command;
        flow.Update(command, MatchRemote(false), MatchReferee(Stage::Settlement), lost, true, 0.01);
        Check(command.auto_home_slider && !command.auto_home_aim && !command.auto_decock,
              "settlement homes a lost axis before decocking");
    }

    // 4. leave_manual 只在接管的第一拍发；ModeLogic 自己的不能带进来，否则会打断自动回零。
    {
        MatchFlow flow;
        ModeLogic mode(true);
        Check(MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked).leave_manual,
              "the first taken-over tick stops the manual jogs");
        Check(!MatchTick(flow, mode, MatchRemote(false, Switch::Middle), match, cocked).leave_manual,
              "the remote dropping out of manual mode does not send leave_manual again");
    }
    {
        MatchFlow flow;
        ModeLogic mode(true);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        MatchTick(flow, mode, MatchRemote(true, Switch::Middle), match, cocked);
        Check(!MatchTick(flow, mode, MatchRemote(true, Switch::Up), match, cocked).leave_manual,
              "switching the remote from manual to automatic does not send leave_manual again");
    }

    // 5. 发射流程自己也拒绝在不许上膛的阶段开始上膛。
    {
        FakeIo io;
        io.state.latch_state = LatchState::Released;
        io.state.slider = kCockPosition;
        io.state.launch_allowed = false;
        DartInventory inventory;
        LaunchSequence sequence(io, inventory);
        Check(!sequence.CommandCock() && !sequence.CommandSalvo(1) && !sequence.CommandLoad() && !sequence.Busy() &&
                  sequence.Fault() == SafetyFault::None && !sequence.Cocking(),
              "the sequence refuses to start cocking, salvo or load where launching is not allowed");
        io.state.launch_allowed = true;
        Check(sequence.CommandCock() && sequence.Cocking() && !sequence.Decocking(),
              "an allowed cock command marks the sequence as cocking");
        sequence.Cancel();
        Check(!sequence.Cocking(), "cancelling clears the cocking mark");
        Check(sequence.CommandCock(), "cock is accepted again");
        RunSequence(sequence, io);
        Check(!sequence.Cocking() && !sequence.Busy() && io.state.latch_state == LatchState::Latched,
              "a finished cock is no longer cocking");
    }

    // 6. 整圈：结算时上膛还在跑，做完以后自动退膛，不发射。
    {
        MatchRig rig;
        rig.io.state.latch_state = LatchState::Released;
        rig.io.state.slider = kCockPosition;
        rig.referee.stage = Stage::InMatch;
        rig.referee.stage_remaining = 60;
        rig.remote.online = false;
        rig.Run(50);
        Check(rig.sequence.Cocking(), "the cocking sequence is running");
        rig.referee.stage = Stage::Settlement;
        rig.referee.stage_remaining = 30;
        rig.Run(4000);
        Check(!rig.sequence.Busy() && rig.sequence.Fault() == SafetyFault::None && rig.decock_requests == 1 &&
                  rig.io.unlocks == 0 && rig.io.state.latch_state == LatchState::Released &&
                  Near(rig.io.state.slider, kSliderPark),
              "a cock still running when settlement starts finishes and is then decocked, without firing");
    }
}

void TestDelayedPrefetchRegression() {
    for (int side : {-1, 1}) {
        FakeIo io;
        DartInventory inventory;
        if (side < 0) {
            inventory.ResetSeatedLeft();
        } else {
            inventory.ResetFull();
            Check(inventory.Fire() && inventory.Release(), "right prefetch starts with an empty magnet");
        }
        LaunchSequence sequence(io, inventory);
        Check(sequence.CommandSalvo(2), "delayed prefetch starts");
        bool lowered = false, lifted_after_shot = false;
        for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
            io.Advance();
            sequence.Tick(tick * 0.01);
            if (std::string(sequence.StepName()) == "取镖：下探") { lowered = true; }
            if (lowered && sequence.Shots() == 0) {
                Check(Near(io.crank_target, side < 0 ? kCrankFetchLeft : kCrankFetchRight) &&
                          Near(io.arm_target, side < 0 ? kFetchLeft : kFetchRight),
                      "prefetched dart stays supported until the first flight completes");
            }
            if (lowered && sequence.Shots() > 0 && Near(io.crank_target, kCrankRetracted) &&
                !io.crank_speed_log.empty() && Near(io.crank_speed_log.back(), kCrankLiftSpeed)) {
                lifted_after_shot = true;
            }
        }
        Check(lifted_after_shot && !sequence.Busy() && sequence.Fault() == SafetyFault::None &&
                  sequence.Shots() == 2 && io.unlocks == 2 && inventory.Remaining() == (side < 0 ? 0 : 1),
              "one start lifts after flight and automatically fires both darts");
    }
}

void TestPrefetchInterruption() {
    for (int side : {-1, 1}) {
        for (int phase = 0; phase < 3; ++phase) {
            for (int interruption = 0; interruption < 3; ++interruption) {
                FakeIo io;
                DartInventory inventory;
                if (side < 0) {
                    inventory.ResetSeatedLeft();
                } else {
                    inventory.ResetFull();
                    Check(inventory.Fire() && inventory.Release(), "right interruption starts with an empty magnet");
                }
                if (phase == 0) { io.state.aimed = false; }
                LaunchSequence sequence(io, inventory);
                Check(sequence.CommandSalvo(2), "prefetch interruption starts");
                double now = 0;
                bool reached = false;
                for (int tick = 0; tick < 20000 && sequence.Busy(); ++tick) {
                    now = tick * 0.01;
                    io.Advance();
                    sequence.Tick(now);
                    const std::string step = sequence.StepName();
                    reached = phase == 0 ? step == "瞄准：等 yaw / pitch 到位" :
                        phase == 1 ? step == "飞行" : step == "取镖：抬起";
                    if (reached) { break; }
                }
                Check(reached && inventory.Held() && inventory.HeldSide() == side,
                      "interruption reaches its selected prefetch phase with its source");
                const int shots = sequence.Shots(), unlocks = io.unlocks;
                if (interruption == 0) {
                    sequence.Cancel();
                } else if (interruption == 1) {
                    io.state.fault = SafetyFault::MotorOffline;
                    sequence.Tick(now + 0.01);
                } else {
                    sequence.Tick(now + 0.01, false);
                }
                const double held_crank = io.state.crank;
                Check(!sequence.Busy() && inventory.Held() && inventory.HeldSide() == side &&
                          Near(io.crank_target, held_crank),
                      "interruption keeps the source and stops the crank at its current command");
                io.state.fault = SafetyFault::None;
                for (int tick = 0; tick < 10; ++tick) {
                    io.Advance();
                    sequence.Tick(now + 0.02 + tick * 0.01);
                }
                Check(!sequence.Busy() && sequence.Shots() == shots && io.unlocks == unlocks &&
                          Near(io.state.crank, held_crank),
                      "power or feedback recovery cannot lift or fire without a new request");
                if (interruption == 1) { sequence.Reset(); }
                io.state.aimed = true;
                const std::size_t commands = io.crank_log.size();
                Check(sequence.CommandSalvo(1), "an explicit request restarts interrupted prefetch");
                for (int tick = 0; tick < 20000 && sequence.Busy() && io.crank_log.size() == commands; ++tick) {
                    io.Advance();
                    sequence.Tick(now + 0.13 + tick * 0.01);
                }
                Check(io.crank_log.size() > commands && Near(io.crank_log[commands], kCrankRetracted) &&
                          Near(io.crank_speed_log[commands], kCrankLiftSpeed) && io.unlocks == unlocks &&
                          inventory.Held() && inventory.HeldSide() == side,
                      "restarting first lifts the retained dart under control without firing or re-gripping");
            }
        }
    }
}

void TestSingleShotPrefetchCompletion() {
    for (bool holder : {false, true}) {
        FakeIo io;
        DartInventory inventory;
        if (holder) { inventory.ResetSeatedLeft(); } else { inventory.ResetManual(); }
        LaunchSequence sequence(io, inventory);
        Check(sequence.CommandSalvo(1), "single shot starts");
        RunSequence(sequence, io);
        Check(!sequence.Busy() && sequence.Fault() == SafetyFault::None && sequence.Shots() == 1,
              "single shot completes after its optional prefetch");
        if (holder) {
            Check(inventory.Held() && inventory.HeldSide() == -1 && Near(io.state.crank, 0) &&
                      Near(io.state.arm, -kStandby),
                  "single shot lifts and parks its prefetched dart after flight");
        } else {
            Check(!inventory.Held() && io.crank_log.empty() && io.arm_log.empty(),
                  "single shot without a holder adds no fetch motion");
        }
    }
}

void TestTensionModbus() {
    const std::uint8_t vector[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    Check(ModbusCrc16(vector, sizeof(vector)) == 0x4B37, "tension CRC standard vector");
    Check(ModbusCrc16(nullptr, 0) == 0xFFFF && ModbusCrc16(vector, 0) == 0xFFFF,
          "tension CRC empty inputs");

    Check(BuildTensionReadRequest(kTensionRightAddr) ==
              std::array<std::uint8_t, kTensionRequestSize>{0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xCB},
          "tension right request frame");
    Check(BuildTensionReadRequest(kTensionLeftAddr) ==
              std::array<std::uint8_t, kTensionRequestSize>{0x02, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xF8},
          "tension left request frame");

    auto response = [](std::uint8_t addr, std::uint16_t sign, std::uint16_t magnitude) {
        std::array<std::uint8_t, kTensionResponseSize> frame{
            addr, kTensionFuncRead, kTensionResponseDataBytes,
            static_cast<std::uint8_t>(sign >> 8U), static_cast<std::uint8_t>(sign),
            static_cast<std::uint8_t>(magnitude >> 8U), static_cast<std::uint8_t>(magnitude), 0, 0};
        const std::uint16_t crc = ModbusCrc16(frame.data(), 7);
        frame[7] = static_cast<std::uint8_t>(crc);
        frame[8] = static_cast<std::uint8_t>(crc >> 8U);
        return frame;
    };
    auto exception = [](std::uint8_t addr, std::uint8_t code) {
        std::array<std::uint8_t, kTensionExceptionSize> frame{addr, kTensionFuncException, code, 0, 0};
        const std::uint16_t crc = ModbusCrc16(frame.data(), 3);
        frame[3] = static_cast<std::uint8_t>(crc);
        frame[4] = static_cast<std::uint8_t>(crc >> 8U);
        return frame;
    };
    auto check_value = [](const std::array<std::uint8_t, kTensionResponseSize> &frame,
                          std::uint8_t addr, std::int32_t expected, const char *label) {
        std::int32_t value = 0;
        Check(ParseTensionResponse(frame.data(), frame.size(), addr, value) == ModbusParseStatus::Ok &&
                  value == expected, label);
    };

    check_value(response(1, 0, 1000), 1, 1000, "tension right positive response");
    check_value(response(2, 1, 500), 2, -500, "tension left negative response");
    check_value(response(1, 0, 0), 1, 0, "tension positive zero response");
    check_value(response(2, 1, 0), 2, 0, "tension negative zero response");
    check_value(response(1, 0, 65000), 1, 65000, "tension positive full scale");
    check_value(response(2, 1, 65535), 2, -65535, "tension negative full scale");

    for (std::uint8_t addr : {kTensionRightAddr, kTensionLeftAddr}) {
        const auto frame = exception(addr, 2);
        std::int32_t value = 123456;
        Check(ParseTensionResponse(frame.data(), frame.size(), addr, value) == ModbusParseStatus::Exception &&
                  value == 123456, "tension exception response preserves value");
    }

    const auto valid = response(1, 0, 1000);
    const auto error = exception(1, 2);
    for (std::size_t length = 0; length <= 1; ++length) {
        std::int32_t value = 123456;
        Check(ParseTensionResponse(nullptr, length, 1, value) == ModbusParseStatus::Incomplete &&
                  value == 123456, "tension null short frame is incomplete");
    }
    for (std::size_t length = 2; length < kTensionResponseSize; ++length) {
        std::int32_t value = 123456;
        Check(ParseTensionResponse(valid.data(), length, 1, value) == ModbusParseStatus::Incomplete &&
                  value == 123456, "tension response prefix is incomplete");
    }
    for (std::size_t length = 2; length < kTensionExceptionSize; ++length) {
        std::int32_t value = 123456;
        Check(ParseTensionResponse(error.data(), length, 1, value) == ModbusParseStatus::Incomplete &&
                  value == 123456, "tension exception prefix is incomplete");
    }

    auto damaged = valid;
    damaged[7] ^= 1;
    std::int32_t value = 123456;
    Check(ParseTensionResponse(damaged.data(), damaged.size(), 1, value) == ModbusParseStatus::Invalid &&
              value == 123456, "tension response CRC error");
    auto damaged_exception = error;
    damaged_exception[3] ^= 1;
    Check(ParseTensionResponse(damaged_exception.data(), damaged_exception.size(), 1, value) ==
              ModbusParseStatus::Invalid && value == 123456, "tension exception CRC error");
    Check(ParseTensionResponse(valid.data(), valid.size(), 2, value) == ModbusParseStatus::Invalid &&
              value == 123456, "tension address mismatch");
    auto bad_function = valid;
    bad_function[1] = 0x06;
    Check(ParseTensionResponse(bad_function.data(), bad_function.size(), 1, value) == ModbusParseStatus::Invalid &&
              value == 123456, "tension invalid function");
    auto bad_bytes = valid;
    bad_bytes[2] = 2;
    Check(ParseTensionResponse(bad_bytes.data(), bad_bytes.size(), 1, value) == ModbusParseStatus::Invalid &&
              value == 123456, "tension invalid byte count");
    auto bad_sign = response(1, 2, 1000);
    Check(ParseTensionResponse(bad_sign.data(), bad_sign.size(), 1, value) == ModbusParseStatus::Invalid &&
              value == 123456, "tension invalid sign register");
}

void TestAutomaticHomeStartRegression() {
    auto update_tick = [](MatchFlow &flow, ModeLogic &mode, const RemoteInput &remote,
                          const RefereeInput &referee, const MatchMechanism &mechanism,
                          bool competition) {
        ModeCommand command = mode.Update(remote, referee, 0.01);
        flow.Update(command, remote, referee, mechanism, competition, 0.01);
        return command;
    };

    // 1. 全未回零完整串联（!competition 与 competition && Stage::NotStarted）
    for (bool competition : {false, true}) {
        const RefereeInput referee = competition ? MatchReferee(Stage::NotStarted) : RefereeInput{};
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        MatchMechanism mechanism;
        mechanism.motors_online = true;

        ModeCommand idle = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!idle.auto_home_aim && !idle.auto_home_slider && !idle.salvo && !idle.auto_home_cancel,
              "automatic idle does not start homing before a salvo edge");

        remote.left = Switch::Down;
        ModeCommand start = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(start.auto_home_aim && !start.auto_home_slider && !start.salvo && !start.auto_home_cancel,
              "automatic start with both axes unhomed requests aim homing and consumes salvo");

        mechanism.homing = true;
        ModeCommand aim_seeking = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!aim_seeking.auto_home_aim && !aim_seeking.auto_home_slider && !aim_seeking.salvo &&
                  !aim_seeking.auto_home_cancel,
              "aim seeking does not re-send aim home or start slider home");

        mechanism.aim_homed = true;
        mechanism.homing = true;
        ModeCommand aim_retreating = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!aim_retreating.auto_home_aim && !aim_retreating.auto_home_slider && !aim_retreating.salvo &&
                  !aim_retreating.auto_home_cancel,
              "aim retreat with aim_homed=true and homing=true still waits before starting slider");

        mechanism.homing = false;
        ModeCommand slider_start = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!slider_start.auto_home_aim && slider_start.auto_home_slider && !slider_start.salvo &&
                  !slider_start.auto_home_cancel,
              "once aim homing finishes, slider homing is requested");

        mechanism.homing = true;
        ModeCommand slider_seeking = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!slider_seeking.auto_home_aim && !slider_seeking.auto_home_slider && !slider_seeking.salvo &&
                  !slider_seeking.auto_home_cancel,
              "slider seeking does not re-send slider home");

        mechanism.slider_homed = true;
        mechanism.homing = true;
        ModeCommand slider_retreating = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!slider_retreating.auto_home_aim && !slider_retreating.auto_home_slider &&
                  !slider_retreating.salvo && !slider_retreating.auto_home_cancel,
              "slider retreat waits before issuing salvo");

        mechanism.homing = false;
        mechanism.all_homed = false;
        ModeCommand not_all_homed = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!not_all_homed.auto_home_aim && !not_all_homed.auto_home_slider && !not_all_homed.salvo &&
                  !not_all_homed.auto_home_cancel,
              "both axes homed with all_homed=false keeps waiting without firing");

        mechanism.all_homed = true;
        ModeCommand launch = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(launch.salvo && !launch.auto_home_aim && !launch.auto_home_slider &&
                  !launch.auto_home_cancel,
              "all_homed and !homing emits salvo once");

        ModeCommand after = update_tick(flow, mode, remote, referee, mechanism, competition);
        Check(!after.salvo && !after.auto_home_aim && !after.auto_home_slider && !after.auto_home_cancel,
              "salvo is emitted only once after auto-home completes");
    }

    // 2. 部分未回零
    {
        // 仅瞄准未回零：完成后直接发 salvo，不发 slider
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        mechanism.slider_homed = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand start = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(start.auto_home_aim && !start.auto_home_slider && !start.salvo,
              "aim-only unhomed requests aim home without slider home");

        mechanism.aim_homed = true;
        mechanism.all_homed = true;
        ModeCommand done = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(done.salvo && !done.auto_home_slider && !done.auto_home_aim,
              "aim-only auto-home emits salvo directly once aim finishes");
    }
    {
        // 仅滑块未回零：直接发 auto_home_slider，不发 aim，完成后发 salvo
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        mechanism.aim_homed = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand start = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!start.auto_home_aim && start.auto_home_slider && !start.salvo,
              "slider-only unhomed requests slider home directly without aim home");

        mechanism.slider_homed = true;
        mechanism.all_homed = true;
        ModeCommand done = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(done.salvo && !done.auto_home_slider && !done.auto_home_aim,
              "slider-only auto-home emits salvo once slider finishes");
    }

    // 3. 已全部回零直通 & 退让中等待 & 非本流程回零取消不发 auto_home_cancel
    {
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism = ReadyMechanism();
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand direct = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(direct.salvo && !direct.auto_home_aim && !direct.auto_home_slider &&
                  !direct.auto_home_cancel,
              "already homed and idle passes salvo through on the same tick");
    }
    {
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism = ReadyMechanism();
        mechanism.homing = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand wait = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!wait.salvo && !wait.auto_home_aim && !wait.auto_home_slider && !wait.auto_home_cancel,
              "all_homed while retreating consumes salvo without re-issuing StartHome");

        mechanism.homing = false;
        ModeCommand launch = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(launch.salvo && !launch.auto_home_aim && !launch.auto_home_slider,
              "once retreat completes, the waiting salvo is emitted");
        ModeCommand after = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!after.salvo, "retreat-waiting salvo is emitted only once");
    }
    {
        // 非本流程启动的回零期间从 Pending 直接切 Manual / Powerless 时 auto_home_cancel 保持 false 且恢复后不补发 salvo
        for (Switch cancel_right : {Switch::Middle, Switch::Down}) {
            ModeLogic mode;
            MatchFlow flow;
            RemoteInput remote;
            remote.online = true;
            remote.right = Switch::Up;
            remote.left = Switch::Middle;
            RefereeInput referee;
            MatchMechanism mechanism;
            mechanism.motors_online = true;
            mechanism.homing = true;
            update_tick(flow, mode, remote, referee, mechanism, false);

            remote.left = Switch::Down;
            ModeCommand pending = update_tick(flow, mode, remote, referee, mechanism, false);
            Check(!pending.salvo && !pending.auto_home_aim && !pending.auto_home_slider &&
                      !pending.auto_home_cancel,
                  "salvo during externally started homing enters Pending without issuing StartHome");

            remote.right = cancel_right;
            ModeCommand cancelled = update_tick(flow, mode, remote, referee, mechanism, false);
            Check(!cancelled.auto_home_cancel && !cancelled.salvo,
                  "cancelling externally started homing from Pending keeps auto_home_cancel false");

            remote.right = Switch::Up;
            remote.left = Switch::Middle;
            mechanism = ReadyMechanism();
            ModeCommand recovered = update_tick(flow, mode, remote, referee, mechanism, false);
            Check(!recovered.salvo && !recovered.auto_home_cancel && !recovered.auto_home_aim &&
                      !recovered.auto_home_slider,
                  "recovering after cancelled external homing never replays salvo");
        }
    }
    {
        // 自动归零等待态与完成同拍消费 fire，Idle 正常单发 fire 不受影响
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand start = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(start.auto_home_aim && !start.salvo && !start.fire,
              "auto-home starts aim before fire consumption checks");

        mechanism.homing = true;
        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        remote.left = Switch::Up;
        ModeCommand wait_aim_fire = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!wait_aim_fire.fire && !wait_aim_fire.salvo,
              "fire edge during WaitAim is consumed");

        remote.left = Switch::Middle;
        mechanism.aim_homed = true;
        mechanism.homing = false;
        ModeCommand slider_req = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(slider_req.auto_home_slider && !slider_req.salvo && !slider_req.fire,
              "auto-home advances to WaitSlider");

        mechanism.homing = true;
        remote.left = Switch::Up;
        ModeCommand wait_slider_fire = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!wait_slider_fire.fire && !wait_slider_fire.salvo,
              "fire edge during WaitSlider is consumed");

        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        mechanism.slider_homed = true;
        mechanism.all_homed = true;
        mechanism.homing = false;
        remote.left = Switch::Up;
        ModeCommand done = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(done.salvo && !done.fire && !done.auto_home_aim && !done.auto_home_slider,
              "auto-home completion emits salvo without leaking same-tick fire");

        // 退让等待中也消费 fire，完成同拍仍只发 salvo
        remote.left = Switch::Middle;
        mechanism.homing = true;
        update_tick(flow, mode, remote, referee, mechanism, false);
        remote.left = Switch::Down;
        ModeCommand retreat_pending = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!retreat_pending.salvo && !retreat_pending.fire,
              "retreat wait consumes salvo into Pending");
        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        remote.left = Switch::Up;
        ModeCommand retreat_fire = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!retreat_fire.fire && !retreat_fire.salvo,
              "fire edge while waiting for retreat is consumed");
        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        mechanism.homing = false;
        remote.left = Switch::Up;
        ModeCommand retreat_done = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(retreat_done.salvo && !retreat_done.fire,
              "retreat completion emits salvo without leaking same-tick fire");

        // Idle 状态（已回零及未回零且未触发 salvo）正常单发 fire 仍放行
        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, ReadyMechanism(), false);
        remote.left = Switch::Up;
        ModeCommand idle_homed_fire = update_tick(flow, mode, remote, referee, ReadyMechanism(), false);
        Check(idle_homed_fire.fire && !idle_homed_fire.salvo,
              "idle homed state preserves normal single-shot fire edge");

        remote.left = Switch::Middle;
        MatchMechanism unhomed_mech;
        unhomed_mech.motors_online = true;
        update_tick(flow, mode, remote, referee, unhomed_mech, false);
        remote.left = Switch::Up;
        ModeCommand idle_unhomed_fire = update_tick(flow, mode, remote, referee, unhomed_mech, false);
        Check(idle_unhomed_fire.fire && !idle_unhomed_fire.salvo,
              "idle unhomed state without salvo does not suppress fire");
    }

    // 4. 重复启动与忙时不排队
    {
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand first = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(first.auto_home_aim && !first.salvo, "initial salvo edge starts aim homing");

        mechanism.homing = true;
        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        remote.left = Switch::Down;
        ModeCommand dup = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!dup.salvo && !dup.auto_home_aim && !dup.auto_home_slider,
              "duplicate salvo edge during auto-home is consumed without restarting or skipping stage");

        mechanism.aim_homed = true;
        mechanism.homing = false;
        ModeCommand slider = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(slider.auto_home_slider && !slider.auto_home_aim && !slider.salvo,
              "auto-home continues to slider after duplicate salvo edge");

        mechanism.slider_homed = true;
        mechanism.all_homed = true;
        ModeCommand done = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(done.salvo, "auto-home completes normally after duplicate salvo edge");
    }
    {
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        mechanism.busy = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand busy_tick = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!busy_tick.auto_home_aim && !busy_tick.auto_home_slider,
              "salvo edge while busy does not start auto-home");

        mechanism.busy = false;
        ModeCommand idle_after = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!idle_after.auto_home_aim && !idle_after.auto_home_slider && !idle_after.salvo,
              "busy salvo edge is not queued after mechanism becomes idle");

        mechanism.slider_homed = mechanism.aim_homed = mechanism.all_homed = true;
        ModeCommand homed_after = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!homed_after.salvo, "busy salvo edge is never replayed even once homed");
    }

    // 5. 6 类取消/打断：无力、切手动、遥控离线、机构故障、故障复位、比赛接管（Preparing 与 InMatch）
    {
        enum class InterruptKind {
            Powerless,
            Manual,
            RemoteOffline,
            Fault,
            FaultReset,
            MatchPreparing,
            MatchInMatch,
        };
        for (InterruptKind kind : {InterruptKind::Powerless, InterruptKind::Manual,
                                   InterruptKind::RemoteOffline, InterruptKind::Fault,
                                   InterruptKind::FaultReset, InterruptKind::MatchPreparing,
                                   InterruptKind::MatchInMatch}) {
            ModeLogic mode;
            MatchFlow flow;
            RemoteInput remote;
            remote.online = true;
            remote.right = Switch::Up;
            remote.left = Switch::Middle;
            RefereeInput referee;
            MatchMechanism mechanism;
            mechanism.motors_online = true;
            update_tick(flow, mode, remote, referee, mechanism, false);

            remote.left = Switch::Down;
            ModeCommand start = update_tick(flow, mode, remote, referee, mechanism, false);
            Check(start.auto_home_aim && !start.salvo, "auto-home starts before interruption");

            mechanism.homing = true;
            update_tick(flow, mode, remote, referee, mechanism, false);

            ModeCommand interrupted;
            bool competition = false;
            switch (kind) {
                case InterruptKind::Powerless:
                    remote.right = Switch::Down;
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, false);
                    break;
                case InterruptKind::Manual:
                    remote.right = Switch::Middle;
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, false);
                    break;
                case InterruptKind::RemoteOffline:
                    remote.online = false;
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, false);
                    break;
                case InterruptKind::Fault:
                    mechanism.fault = SafetyFault::Timeout;
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, false);
                    break;
                case InterruptKind::FaultReset:
                    interrupted = mode.Update(remote, referee, 0.01);
                    interrupted.fault_reset = true;
                    flow.Update(interrupted, remote, referee, mechanism, false, 0.01);
                    break;
                case InterruptKind::MatchPreparing:
                    competition = true;
                    referee = MatchReferee(Stage::Preparing);
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, true);
                    break;
                case InterruptKind::MatchInMatch:
                    competition = true;
                    referee = MatchReferee(Stage::InMatch);
                    interrupted = update_tick(flow, mode, remote, referee, mechanism, true);
                    break;
            }
            Check(interrupted.auto_home_cancel && !interrupted.salvo,
                  "interruption during active auto-home emits auto_home_cancel without salvo");

            // 恢复条件后不补发 salvo，且 auto_home_cancel 不粘滞
            remote.online = true;
            remote.right = Switch::Up;
            remote.left = Switch::Middle;
            referee = competition ? MatchReferee(Stage::NotStarted) : RefereeInput{};
            mechanism.fault = SafetyFault::None;
            mechanism.homing = false;
            mechanism.aim_homed = mechanism.slider_homed = mechanism.all_homed = true;
            ModeCommand recovered = update_tick(flow, mode, remote, referee, mechanism, competition);
            Check(!recovered.salvo && !recovered.auto_home_cancel && !recovered.auto_home_aim &&
                      !recovered.auto_home_slider,
                  "recovering after interrupted auto-home never replays salvo");
        }
    }

    // 6. SliderMustHold 拒绝终止
    {
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Up;
        remote.left = Switch::Middle;
        RefereeInput referee;
        MatchMechanism mechanism;
        mechanism.motors_online = true;
        mechanism.aim_homed = true;
        update_tick(flow, mode, remote, referee, mechanism, false);

        remote.left = Switch::Down;
        ModeCommand slider_req = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(slider_req.auto_home_slider && !slider_req.salvo,
              "slider home requested before hardware rejection");

        flow.RejectAutoHome();

        for (int i = 0; i < 5; ++i) {
            ModeCommand retry = update_tick(flow, mode, remote, referee, mechanism, false);
            Check(!retry.auto_home_slider && !retry.salvo,
                  "rejected auto-home does not retry auto_home_slider");
        }

        mechanism.slider_homed = true;
        mechanism.all_homed = true;
        ModeCommand homed_later = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(!homed_later.salvo, "rejected auto-home never fires salvo even if all_homed later becomes true");

        remote.left = Switch::Middle;
        update_tick(flow, mode, remote, referee, mechanism, false);
        remote.left = Switch::Down;
        ModeCommand fresh_edge = update_tick(flow, mode, remote, referee, mechanism, false);
        Check(fresh_edge.salvo, "a new salvo edge can restart after RejectAutoHome");
    }

    // 7. 结合生产 MotorAxis 反馈验证停机交接与 StartHome() 时序
    {
        auto axis_homing = [](const MotorAxis &axis) {
            return axis.State() == HomeState::Seeking || axis.State() == HomeState::Retreating;
        };
        auto hw_axis_step = [](MotorAxis &slider, const ModeCommand &cmd) {
            if (cmd.leave_manual || cmd.auto_home_cancel) {
                slider.Stop();
            }
            if (cmd.auto_home_slider) {
                Check(slider.StartHome(), "slider axis accepts StartHome after stop handoff");
            }
        };

        // 7a. 手动切自动（同拍 leave_manual=true 停机）+ salvo 启动 StartHome，后续周期不打断寻零/退让
        MotorAxis slider(SliderConfig());
        slider.Feedback(12345, 0);
        ModeLogic mode;
        MatchFlow flow;
        RemoteInput remote;
        remote.online = true;
        remote.right = Switch::Middle;
        remote.left = Switch::Middle;
        RefereeInput referee;

        auto mech_from_axis = [&slider, &axis_homing]() {
            MatchMechanism m;
            m.motors_online = true;
            m.aim_homed = true;
            m.slider_homed = slider.Homed();
            m.all_homed = slider.Homed();
            m.homing = axis_homing(slider);
            return m;
        };

        // 手动模式下先建立点动速度，验证切自动先发 leave_manual 停机，下一拍 salvo 启动 StartHome，后续周期不打断寻零/退让
        update_tick(flow, mode, remote, referee, mech_from_axis(), false);
        Check(slider.Jog(0.05), "manual jog starts before switching to automatic");
        slider.Update(0.001, true, true);

        remote.right = Switch::Up;
        ModeCommand handoff = update_tick(flow, mode, remote, referee, mech_from_axis(), false);
        Check(handoff.leave_manual && !handoff.auto_home_slider && !handoff.salvo,
              "manual-to-automatic sends leave_manual to stop manual jog before salvo");
        hw_axis_step(slider, handoff);
        Check(slider.Update(0.001, true, true) == 0 && !axis_homing(slider),
              "Stop on leave_manual halts manual jog before StartHome");

        remote.left = Switch::Down;
        ModeCommand start_home = update_tick(flow, mode, remote, referee, mech_from_axis(), false);
        Check(!start_home.leave_manual && start_home.auto_home_slider && !start_home.salvo,
              "salvo edge after entering automatic emits auto_home_slider without leave_manual");
        hw_axis_step(slider, start_home);
        Check(slider.State() == HomeState::Seeking && axis_homing(slider),
              "slider enters Seeking once StartHome runs after leave_manual stop");

        // 同拍同时出现 leave_manual 与 auto_home_slider 时，先 Stop 再 StartHome 仍能进入 Seeking
        {
            MotorAxis same_tick(SliderConfig());
            same_tick.Feedback(12345, 0);
            Check(same_tick.Jog(0.05), "jog starts before same-tick handoff");
            same_tick.Update(0.001, true, true);
            ModeCommand cmd;
            cmd.mode = Mode::Automatic;
            cmd.leave_manual = true;
            cmd.salvo = true;
            RemoteInput auto_r;
            auto_r.online = true;
            auto_r.right = Switch::Up;
            auto_r.left = Switch::Down;
            MatchFlow same_flow;
            MatchMechanism same_mech;
            same_mech.motors_online = true;
            same_mech.aim_homed = true;
            same_flow.Update(cmd, auto_r, referee, same_mech, false, 0.01);
            Check(cmd.leave_manual && cmd.auto_home_slider && !cmd.salvo,
                  "same-tick leave_manual and salvo produces auto_home_slider");
            hw_axis_step(same_tick, cmd);
            Check(same_tick.State() == HomeState::Seeking && axis_homing(same_tick),
                  "same-tick Stop before StartHome leaves slider in Seeking");
        }

        // 驱动到堵转进入 Retreating，再完成退让直至发射 salvo
        bool saw_retreating = false;
        bool saw_salvo = false;
        for (int tick = 0; tick < 1000 && !saw_salvo; ++tick) {
            for (int sub = 0; sub < 10; ++sub) {
                if (slider.State() == HomeState::Retreating) {
                    saw_retreating = true;
                    slider.Feedback(slider.PositionToCounts(slider.Target()), 0);
                } else {
                    slider.Feedback(12345, 0);
                }
                slider.Update(0.001, true, true);
            }
            ModeCommand cmd = update_tick(flow, mode, remote, referee, mech_from_axis(), false);
            Check(!cmd.leave_manual && !cmd.auto_home_cancel,
                  "subsequent automatic ticks do not emit leave_manual or cancel during homing");
            hw_axis_step(slider, cmd);
            if (cmd.salvo) {
                saw_salvo = true;
            }
        }
        Check(saw_retreating && slider.Homed() && !axis_homing(slider) && saw_salvo,
              "production MotorAxis completes seek and retreat and MatchFlow emits salvo");

        // 7b. Seeking 与 Retreating 期间 auto_home_cancel=true 调用 Stop() 后轴 Homing 状态立即为 false
        for (HomeState target_state : {HomeState::Seeking, HomeState::Retreating}) {
            MotorAxis cancel_axis(SliderConfig());
            cancel_axis.Feedback(12345, 0);
            ModeLogic cancel_mode;
            MatchFlow cancel_flow;
            RemoteInput r;
            r.online = true;
            r.right = Switch::Up;
            r.left = Switch::Middle;
            auto make_mech = [&cancel_axis, &axis_homing]() {
                MatchMechanism m;
                m.motors_online = true;
                m.aim_homed = true;
                m.slider_homed = cancel_axis.Homed();
                m.all_homed = cancel_axis.Homed();
                m.homing = axis_homing(cancel_axis);
                return m;
            };
            update_tick(cancel_flow, cancel_mode, r, referee, make_mech(), false);
            r.left = Switch::Down;
            ModeCommand start_cmd = update_tick(cancel_flow, cancel_mode, r, referee, make_mech(), false);
            hw_axis_step(cancel_axis, start_cmd);
            Check(cancel_axis.State() == HomeState::Seeking && axis_homing(cancel_axis),
                  "axis starts seeking before cancel test");

            if (target_state == HomeState::Retreating) {
                for (int i = 0; i < 1000 && cancel_axis.State() == HomeState::Seeking; ++i) {
                    cancel_axis.Feedback(12345, 0);
                    cancel_axis.Update(0.001, true, true);
                }
                Check(cancel_axis.State() == HomeState::Retreating && axis_homing(cancel_axis),
                      "axis enters retreating before cancel test");
            }

            r.right = Switch::Down; // 无力打断
            ModeCommand cancel_cmd = update_tick(cancel_flow, cancel_mode, r, referee, make_mech(), false);
            Check(cancel_cmd.auto_home_cancel && !cancel_cmd.salvo,
                  "powerless during MotorAxis homing emits auto_home_cancel");
            hw_axis_step(cancel_axis, cancel_cmd);
            Check(!axis_homing(cancel_axis),
                  "calling Stop on auto_home_cancel immediately clears MotorAxis homing state");
        }

        // PitchConfig 处于 Retreating 时执行 LauncherHardware::StopMotors 同款逻辑，验证 homing 立即清除
        {
            AxisConfig pitch_config{
                kM3508Ratio * kEncoderRange / kPitchLead, kPitchBottom, kPitchTopLimit,
                kPitchTop, kPitchHomeSpeed, kHomeRetreat, 0.001, 1,
                kPitchCurrentCap, 16384, kPitchPos, kPitchSpeed};
            pitch_config.home_current_fraction = kPitchHomeCurrentFraction;
            pitch_config.home_stall_time = kPitchHomeStallTime;
            pitch_config.home_seek_limit = kPitchHomeSeekLimit;
            pitch_config.hold_on_release = true;

            MotorAxis pitch(pitch_config);
            pitch.Feedback(12345, 0);
            Check(pitch.StartHome() && pitch.State() == HomeState::Seeking && axis_homing(pitch),
                  "pitch starts seeking before retreating stop test");
            for (int i = 0; i < 2000 && pitch.State() == HomeState::Seeking; ++i) {
                pitch.Feedback(12345, 0);
                pitch.Update(0.001, true, true);
            }
            Check(pitch.State() == HomeState::Retreating && axis_homing(pitch),
                  "pitch enters retreating before StopMotors handoff");
            if (!pitch.HoldPosition()) {
                pitch.Stop();
            }
            Check(!axis_homing(pitch) && pitch.State() == HomeState::Idle && !pitch.Homed(),
                  "StopMotors fallback Stop() during pitch retreating immediately clears homing state");
        }
    }
}

int main() {
    TestDelayedPrefetchRegression();
    TestPrefetchInterruption();
    TestSingleShotPrefetchCompletion();
    TestPidAndParams();
    TestGeometryAndServos();
    TestAxis();
    TestAxisHolding();
    TestSafety();
    TestMode();
    TestManualInventory();
    TestLeaveManual();
    TestManualSingleShot();
    TestDecock();
    TestBottomFinding();
    TestDbus();
    TestDbusFramer();
    TestWheelAndManualCancel();
    TestMagnetToggle();
    TestMagnetGuard();
    TestStallGuard();
    TestFaultReset();
    TestStickDeadband();
    TestArmTargetJog();
    TestArmStallTracking();
    TestArmLowSpeedFeedback();
    TestArmLowSpeedMotion();
    TestArmInwardRelease();
    TestArmPlantModel();
    TestArmMoveTo();
    TestAutoFetchTiming();
    TestFetchTest();
    TestLoadFromFetch();
    TestHomeAtAndRebase();
    TestAutomaticTwoShot();
    TestManualTwoShotSwitch();
    TestSequence();
    TestReleasePause();
    TestLockSettle();
    TestCrankSettle();
    TestProbeAndLatch();
    TestShotRecord();
    TestMatchPredicates();
    TestMatchOutsideMatch();
    TestMatchTakeover();
    TestMatchRequests();
    TestMatchFaults();
    TestMatchRig();
    TestMatchSafety();
    TestRefereeTracker();
    TestMatchReviewFixes();
    TestAutomaticHomeStartRegression();
    TestTensionModbus();
    std::printf("PASS: %d checks\n", checks);
    return 0;
}
