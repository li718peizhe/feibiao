#include "dbus_framer.hpp"
#include "dart_params.hpp"

namespace dart {
DbusAction DbusFramer::Poll(std::uint32_t now_ms, std::size_t queued) {
    // 无符号减法处理 32 位毫秒计数回绕。
    if (!started_) {
        started_ = true;
        aligned_ = false;
        last_queued_ = queued;
        last_poll_ms_ = now_ms;
        last_change_ms_ = now_ms;
        return DbusAction::None;
    }
    const std::uint32_t since_poll = now_ms - last_poll_ms_;
    const std::uint32_t since_change = now_ms - last_change_ms_;
    last_poll_ms_ = now_ms;
    if (since_poll > kDbusStallMs) {
        // 轮询中断期间队列可能溢出丢字节，回到未对齐状态。
        aligned_ = false;
        last_queued_ = queued;
        last_change_ms_ = now_ms;
        return DbusAction::None;
    }
    if (queued != last_queued_) {
        last_queued_ = queued;
        last_change_ms_ = now_ms;
        return DbusAction::None;
    }
    if (since_change < kDbusQuietMs) {
        return DbusAction::None;
    }
    if (queued == 0) {
        aligned_ = true;
        return DbusAction::None;
    }
    if (queued == kDbusFrameSize && aligned_) {
        last_queued_ = 0;
        last_change_ms_ = now_ms;
        return DbusAction::ReadFrame;
    }
    // 清空发生在空闲期，下一个字节就是下一帧的开头。
    last_queued_ = 0;
    last_change_ms_ = now_ms;
    aligned_ = true;
    return DbusAction::Clear;
}

void DbusFramer::Reset() { started_ = false; }
}
