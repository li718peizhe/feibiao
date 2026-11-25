// DBUS 使用 100000、偶校验、8 位、1 停止位，与 DR16 串口设置一致。
#include "remote_port.hpp"
namespace dart {
RemotePort::RemotePort(LibXR::UART &uart) : uart_(uart), op_(sem_, 4) {
    uart_.SetConfig({100000, LibXR::UART::Parity::EVEN, 8, 1});
    thread_.Create(this, ThreadMain, "dart_dbus", 1536, LibXR::Thread::Priority::HIGH);
}
void RemotePort::ThreadMain(RemotePort *self) {
    std::array<std::uint8_t, kDbusFrameSize> frame{};
    RemoteInput parsed;
    auto last = LibXR::Timebase::GetMilliseconds();
    while (true) {
        const auto action = self->framer_.Poll(LibXR::Timebase::GetMilliseconds(),
                                               self->uart_.read_port_->Size());
        if (action == DbusAction::ReadFrame) {
            if (self->uart_.Read({frame.data(), frame.size()}, self->op_) == LibXR::ErrorCode::OK) {
                if (ParseDbusFrame(frame, parsed)) {
                    LibXR::Mutex::LockGuard lock(self->mutex_);
                    self->input_ = parsed;
                    self->received_ms_ = LibXR::Timebase::GetMilliseconds();
                    ++self->sequence_;
                    ++self->frames_ok_;
                } else {
                    static_cast<void>(self->uart_.read_port_->ClearQueuedData());
                    self->framer_.Reset();
                    LibXR::Mutex::LockGuard lock(self->mutex_);
                    ++self->frames_bad_;
                }
            } else {
                static_cast<void>(self->uart_.read_port_->ClearQueuedData());
                self->framer_.Reset();
            }
        } else if (action == DbusAction::Clear) {
            static_cast<void>(self->uart_.read_port_->ClearQueuedData());
            LibXR::Mutex::LockGuard lock(self->mutex_);
            ++self->frames_cleared_;
        }
        LibXR::Thread::SleepUntil(last, 1);
    }
}
bool RemotePort::Read(RemoteInput &input, std::uint32_t &sequence, std::uint32_t &received_ms,
                      std::uint32_t &frames_ok, std::uint32_t &frames_cleared, std::uint32_t &frames_bad) {
    LibXR::Mutex::LockGuard lock(mutex_);
    input = input_;
    sequence = sequence_;
    received_ms = received_ms_;
    frames_ok = frames_ok_;
    frames_cleared = frames_cleared_;
    frames_bad = frames_bad_;
    return input_.online;
}
}
