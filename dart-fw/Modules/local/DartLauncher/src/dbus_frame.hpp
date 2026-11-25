// 解析遥控器 18 字节 DBUS 帧，不依赖串口或框架。
#pragma once
#include "mode_logic.hpp"
#include <array>
#include <cstdint>

namespace dart {
// 检查摇杆和拨杆，成功时输出 [-1,1] 输入；失败保留输出原值。
bool ParseDbusFrame(const std::array<std::uint8_t, 18> &frame, RemoteInput &output);
}
