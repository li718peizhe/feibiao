#pragma once
// LibXR 模块入口：构造参数是 XRobot 1.0 注册对象的引用接口，不再划分 hw/app 命名空间。
/* === MODULE MANIFEST V2 ===
module_description: 飞镖发射架 LibXR 适配层 / Dart launcher LibXR adapter
depends:
- id: QDU-Robomaster/RMMotor
  ref: same-or-dev
- id: QDU-Robomaster/Referee
  ref: same-or-dev
=== END MANIFEST === */
#include "launcher_hw.hpp"
#include "thread.hpp"
#include "timebase.hpp"
// 自建 1 kHz 控制线程；监控入口由 XRobot 主循环调用。
class DartLauncher {
public:
    // 接收 XRobot 注册对象引用；CAN/ID 用于逐帧在线计时，方向由 RMMotor 处理。
    DartLauncher(RMMotor &slider_left, RMMotor &slider_right, RMMotor &pitch, RMMotor &yaw, RMMotor &arm,
                 LibXR::CAN &slider_can, LibXR::CAN &pitch_can, LibXR::CAN &yaw_can, LibXR::CAN &arm_can,
                 std::uint32_t slider_left_id, std::uint32_t slider_right_id, std::uint32_t pitch_id,
                 std::uint32_t yaw_id, std::uint32_t arm_id, Referee &referee, LibXR::UART &dbus,
                 LibXR::PWM &lock_pwm, LibXR::PWM &crank_pwm, LibXR::PWM &magnet_pwm,
                 LibXR::UART &tension_uart)
                 : hardware_(slider_left, slider_right, pitch, yaw, arm, slider_can, pitch_can, yaw_can, arm_can,
                    slider_left_id, slider_right_id, pitch_id, yaw_id, arm_id, referee, dbus,
                    lock_pwm, crank_pwm, magnet_pwm, tension_uart) {
        thread_.Create(this, ThreadMain, "dart_control", 4096, LibXR::Thread::Priority::HIGH);
    }
    // 由生成的主循环每秒调用，打印模式、故障、流程与库存。
    void OnMonitor() { hardware_.OnMonitor(); }
private:
    static void ThreadMain(DartLauncher *self) {
        auto wake = LibXR::Timebase::GetMilliseconds();
        std::uint32_t previous = wake;
        std::uint64_t elapsed_ms = 0;
        while (true) {
            const std::uint32_t now = LibXR::Timebase::GetMilliseconds();
            const std::uint32_t delta = now - previous;
            elapsed_ms += delta;
            previous = now;
            // 无符号差值处理 32 位毫秒计数回绕；首拍使用名义周期。
            const auto tick_start = LibXR::Timebase::GetMicroseconds();
            self->hardware_.Tick(elapsed_ms / 1000.0, delta > 0 ? delta / 1000.0 : 0.001);
            // 记录每拍耗时与唤醒间隔，阶段 3 检查 double 运算是否跑得完。
            const auto tick_span = LibXR::Timebase::GetMicroseconds() - tick_start;
            self->hardware_.RecordControlTiming(static_cast<std::uint32_t>(tick_span), delta);
            LibXR::Thread::SleepUntil(wake, 1);
        }
    }
    dart::LauncherHardware hardware_;
    LibXR::Thread thread_;
};
