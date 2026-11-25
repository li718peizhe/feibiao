// 累计 13 位转子角，要求相邻反馈位移小于半圈。
#pragma once
#include <cstdint>

namespace dart {
// 正方向为编码器计数增加方向，累计值为转子计数。
class RotorCounter {
public:
    // 首帧建立连续角基准；非法角返回 false 且保留已有计数。
    bool Update(std::uint16_t angle);
    // 清空基准，等待下一帧。
    void Reset();
    // 返回连续转子计数，首帧保留其单圈绝对值。
    std::int64_t Counts() const { return counts_; }
private:
    bool initialized_ = false;
    std::uint16_t last_ = 0;
    std::int64_t counts_ = 0;
};
}
