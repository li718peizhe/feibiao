#pragma once
#include <cstdint>

namespace LibXR {
class Timebase {
public:
    static void DelayMicroseconds(std::uint32_t us);
    static std::uint32_t GetMilliseconds();
};
}
