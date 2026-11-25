#pragma once
#include <cstdint>

namespace dart {
struct Hx711Reading {
    std::int32_t raw = 0;
    std::uint32_t samples = 0, age_ms = 0;
    bool updated = false;
};

// 右侧 HX711：8 针口第 1 针 PB12 为 SCK，第 3 针 PB13 为 DT。
class Hx711Port {
public:
    Hx711Port();
    // 仅由监控线程调用；未就绪立即返回，raw 保留上次读数。
    Hx711Reading Read();
private:
    std::int32_t raw_ = 0;
    std::uint32_t samples_ = 0, received_ms_ = 0;
};
}
