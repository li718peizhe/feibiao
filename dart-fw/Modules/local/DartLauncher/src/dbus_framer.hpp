// 按线路空闲对 DBUS 串口接收队列分帧，不依赖串口或框架。
#pragma once
#include <cstddef>
#include <cstdint>

namespace dart {
enum class DbusAction : std::uint8_t { None, ReadFrame, Clear };

// 用字节数的变化判断线路空闲：空闲 3 ms 且积攒 18 字节时读一帧，
// 其他空闲情况清空队列，避免错位帧驱动执行器。
class DbusFramer {
public:
    // now_ms 单调毫秒（允许 32 位回绕），queued 为串口接收队列当前字节数。
    DbusAction Poll(std::uint32_t now_ms, std::size_t queued);
    // 读帧失败等异常时调用，回到未对齐状态。
    void Reset();

private:
    bool started_ = false, aligned_ = false;
    std::size_t last_queued_ = 0;
    std::uint32_t last_poll_ms_ = 0, last_change_ms_ = 0;
};
}
