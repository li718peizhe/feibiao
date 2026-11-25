// DBUS 接收线程：有效帧以快照和递增计数交给控制线程。
#pragma once
#include "dbus_frame.hpp"
#include "dbus_framer.hpp"
#include "uart.hpp"
#include "thread.hpp"
#include "semaphore.hpp"
#include "mutex.hpp"
#include "timebase.hpp"
#include <array>
namespace dart {
// 接收线程独占 DBUS 串口，控制线程通过互斥快照读取。
class RemotePort {
public:
    // uart 为 DBUS 接收串口，构造时启动 1536 字节高优先级接收线程。
    explicit RemotePort(LibXR::UART &uart);
    // 复制有效帧、帧计数、接收时刻 ms 和收帧计数；尚无有效帧时返回 false。
    bool Read(RemoteInput &input, std::uint32_t &sequence, std::uint32_t &received_ms,
              std::uint32_t &frames_ok, std::uint32_t &frames_cleared, std::uint32_t &frames_bad);
private:
    static void ThreadMain(RemotePort *self);
    LibXR::UART &uart_;
    LibXR::Thread thread_;
    LibXR::Semaphore sem_{0};
    LibXR::ReadOperation op_;
    LibXR::Mutex mutex_;
    DbusFramer framer_;
    RemoteInput input_{};
    std::uint32_t sequence_ = 0, received_ms_ = 0;
    // 有效帧、清空和解析失败计数，快照随帧数据一起取走。
    std::uint32_t frames_ok_ = 0, frames_cleared_ = 0, frames_bad_ = 0;
};
}
