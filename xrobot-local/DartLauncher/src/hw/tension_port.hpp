#pragma once
#include "mutex.hpp"
#include "semaphore.hpp"
#include "thread.hpp"
#include "tension_modbus.hpp"
#include "timebase.hpp"
#include "uart.hpp"
#include <cstdint>

namespace dart {
struct TensionChannel {
    std::int32_t raw = 0;
    std::uint32_t samples = 0;
    std::uint32_t timeouts = 0;
    std::uint32_t exceptions = 0;
    std::uint32_t errors = 0;
    std::uint32_t age_ms = 0;
    bool valid = false;
};

struct TensionSnapshot {
    TensionChannel left{};
    TensionChannel right{};
};

class TensionPort {
public:
    explicit TensionPort(LibXR::UART &uart);
    TensionSnapshot Read();

private:
    struct ChannelState {
        std::int32_t raw = 0;
        std::uint32_t samples = 0;
        std::uint32_t timeouts = 0;
        std::uint32_t exceptions = 0;
        std::uint32_t errors = 0;
        std::uint32_t received_ms = 0;
        bool valid = false;
    };

    static void ThreadMain(TensionPort *self);
    void PollStation(std::uint8_t addr, ChannelState &channel);

    LibXR::UART &uart_;
    LibXR::Thread thread_;
    LibXR::Semaphore tx_sem_{0};
    LibXR::Semaphore rx_sem_{0};
    LibXR::WriteOperation tx_op_;
    LibXR::ReadOperation rx_op_;
    LibXR::Mutex mutex_;
    ChannelState left_{};
    ChannelState right_{};
};
}
