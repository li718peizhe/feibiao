// 假设积分先因外部阻挡累积，再在限位前 1° 突然解除阻挡；不是实车模型。
#include "motor_axis.hpp"
#include "stall_guard.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace dart;

int main() {
    for (int direction : {-1, 1}) {
        for (double inertia : {0.00923, 0.05}) {
            for (double load : {0.0, 0.17, 0.55}) {
                for (bool early_brake : {false, true}) {
                    AxisConfig config{kEncoderRange / (2 * kPi), kArmMin, kArmMax, kArmMin,
                                      Radians(1), kHomeRetreat, Radians(0.1), -1, kArmCurrentCap,
                                      16384, kArmPos, kArmSpeedGains};
                    config.hold_on_release = true;
                    config.hold_integral_limit = kArmStallCurrent;
                    config.hold_integral_rpm = kArmStallRpm;
                    MotorAxis axis(config);
                    StallGuard guard(kArmStallCurrent, kArmStallRpm, kArmStallTime);
                    const double limit = direction > 0 ? kArmMax : kArmMin;
                    const double initial = limit - direction * Radians(1);
                    axis.Feedback(0, 0);
                    axis.HomeAt(direction * Radians(35));
                    axis.Jog(direction * 0.2);
                    for (int i = 0; i < 8000; ++i) {
                        guard.Update(axis.Update(0.001, true, true), 0, 0.001);
                    }
                    axis.Feedback(axis.PositionToCounts(initial), 0);
                    guard.Update(axis.Update(0.001, true, true), 0, 0.001);
                    axis.Jog(0);
                    double angle = initial, velocity = 0, max_outward = 0, max_inward = 0;
                    int sign_changes = 0, last_sign = 0;
                    for (int i = 0; i < 5000; ++i) {
                        // 黏性阻尼 0.05 N·m/(rad/s)，转速反馈四舍五入到 1 rpm。
                        const double rpm = std::round(velocity * 60 / (2 * kPi));
                        axis.Feedback(axis.PositionToCounts(angle), rpm);
                        const int request = axis.Update(0.001, true, true);
                        int command = axis.LimitAtSoftLimits(guard.Update(request, rpm, 0.001));
                        if (early_brake && direction * (angle - initial) > Radians(0.2) &&
                            direction * command > 0 && direction * rpm >= 0) {
                            command = rpm == 0 ? 0 : -direction * kArmStallCurrent;
                        }
                        const int sign = (command > 0) - (command < 0);
                        if (sign != 0 && last_sign != 0 && sign != last_sign) { ++sign_changes; }
                        if (sign != 0) { last_sign = sign; }
                        const double torque = command * 3.0 / 16384 * 0.741;
                        velocity += (torque - direction * load - 0.05 * velocity) / inertia * 0.001;
                        angle += velocity * 0.001;
                        max_outward = std::max(max_outward, direction * (angle - initial));
                        max_inward = std::max(max_inward, -direction * (angle - initial));
                    }
                    std::printf("direction=%+d inertia=%.5f kgm2 load=%.2f Nm brake=%d "
                                "outward=%.2f deg inward=%.2f deg beyond_limit=%.2f deg "
                                "reversals=%d\n", direction, inertia, load,
                                static_cast<int>(early_brake), max_outward * 180 / kPi,
                                max_inward * 180 / kPi,
                                std::max(0.0, max_outward * 180 / kPi - 1), sign_changes);
                }
            }
        }
    }
}
