#include "tension_port.hpp"
#include "timebase.hpp"
#include <algorithm>
#include <array>

namespace dart {
namespace {
constexpr std::uint32_t kTensionBaudRate = 9600;
constexpr std::uint32_t kTxTimeoutMs = 30;
constexpr std::uint32_t kRxChunkTimeoutMs = 10;
constexpr std::uint32_t kResponseTimeoutMs = 80;
constexpr std::uint32_t kTurnaroundMs = 15;
constexpr std::uint32_t kRxPollStepMs = 3;
constexpr std::size_t kThreadStackBytes = 1024;
}

TensionPort::TensionPort(LibXR::UART &uart)
    : uart_(uart), tx_op_(tx_sem_, kTxTimeoutMs), rx_op_(rx_sem_, kRxChunkTimeoutMs) {
    uart_.SetConfig({kTensionBaudRate, LibXR::UART::Parity::NO_PARITY, 8, 1});
    thread_.Create(this, ThreadMain, "dart_tension", kThreadStackBytes, LibXR::Thread::Priority::LOW);
}

void TensionPort::ThreadMain(TensionPort *self) {
    while (true) {
        self->PollStation(kTensionRightAddr, self->right_);
        LibXR::Thread::Sleep(kTurnaroundMs);
        self->PollStation(kTensionLeftAddr, self->left_);
        LibXR::Thread::Sleep(kTurnaroundMs);
    }
}

void TensionPort::PollStation(std::uint8_t addr, ChannelState &channel) {
    static_cast<void>(uart_.read_port_->ClearQueuedData());
    const auto request = BuildTensionReadRequest(addr);
    if (uart_.Write({request.data(), request.size()}, tx_op_) != LibXR::ErrorCode::OK) {
        LibXR::Mutex::LockGuard lock(mutex_);
        ++channel.timeouts;
        return;
    }

    std::array<std::uint8_t, kTensionResponseSize> buffer{};
    std::size_t received = 0;
    const std::uint32_t start = LibXR::Timebase::GetMilliseconds();
    while (true) {
        const std::uint32_t now = LibXR::Timebase::GetMilliseconds();
        if (static_cast<std::uint32_t>(now - start) >= kResponseTimeoutMs) {
            break;
        }
        const std::size_t available = uart_.read_port_->Size();
        if (available > 0 && received < buffer.size()) {
            const std::size_t count = std::min(available, buffer.size() - received);
            if (uart_.Read({buffer.data() + received, count}, rx_op_) != LibXR::ErrorCode::OK) {
                static_cast<void>(uart_.read_port_->ClearQueuedData());
                LibXR::Mutex::LockGuard lock(mutex_);
                ++channel.errors;
                return;
            }
            received += count;
            std::int32_t parsed = 0;
            const auto status = ParseTensionResponse(buffer.data(), received, addr, parsed);
            if (status == ModbusParseStatus::Ok) {
                LibXR::Mutex::LockGuard lock(mutex_);
                channel.raw = parsed;
                ++channel.samples;
                channel.received_ms = LibXR::Timebase::GetMilliseconds();
                channel.valid = true;
                return;
            }
            if (status == ModbusParseStatus::Exception) {
                LibXR::Mutex::LockGuard lock(mutex_);
                ++channel.exceptions;
                return;
            }
            if (status == ModbusParseStatus::Invalid) {
                static_cast<void>(uart_.read_port_->ClearQueuedData());
                LibXR::Mutex::LockGuard lock(mutex_);
                ++channel.errors;
                return;
            }
        } else {
            LibXR::Thread::Sleep(kRxPollStepMs);
        }
    }
    static_cast<void>(uart_.read_port_->ClearQueuedData());
    LibXR::Mutex::LockGuard lock(mutex_);
    ++channel.timeouts;
}

TensionSnapshot TensionPort::Read() {
    LibXR::Mutex::LockGuard lock(mutex_);
    TensionSnapshot snapshot;
    snapshot.left.raw = left_.raw;
    snapshot.left.samples = left_.samples;
    snapshot.left.timeouts = left_.timeouts;
    snapshot.left.exceptions = left_.exceptions;
    snapshot.left.errors = left_.errors;
    snapshot.left.valid = left_.valid;
    snapshot.right.raw = right_.raw;
    snapshot.right.samples = right_.samples;
    snapshot.right.timeouts = right_.timeouts;
    snapshot.right.exceptions = right_.exceptions;
    snapshot.right.errors = right_.errors;
    snapshot.right.valid = right_.valid;
    const std::uint32_t now = LibXR::Timebase::GetMilliseconds();
    snapshot.left.age_ms = left_.samples == 0 ? 0 : now - left_.received_ms;
    snapshot.right.age_ms = right_.samples == 0 ? 0 : now - right_.received_ms;
    return snapshot;
}
}
