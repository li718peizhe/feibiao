// DBUS 的四路摇杆按 11 位打包，拨轮可缺省为零。
#include "dbus_frame.hpp"

namespace dart {
bool ParseDbusFrame(const std::array<std::uint8_t, 18> &f, RemoteInput &output) {
    const std::array<int, 4> channel{{
        (f[0] | f[1] << 8) & 0x7ff,
        (f[1] >> 3 | f[2] << 5) & 0x7ff,
        (f[2] >> 6 | f[3] << 2 | f[4] << 10) & 0x7ff,
        (f[4] >> 1 | f[5] << 7) & 0x7ff}};
    const int right = (f[5] >> 4) & 3, left = (f[5] >> 6) & 3;
    for (int value : channel) {
        if (value < 364 || value > 1684) { return false; }
    }
    if (right == 0 || left == 0) { return false; }
    RemoteInput parsed;
    parsed.right = right == 1 ? Switch::Up : right == 3 ? Switch::Middle : Switch::Down;
    parsed.left = left == 1 ? Switch::Up : left == 3 ? Switch::Middle : Switch::Down;
    parsed.right_x = (channel[0] - 1024) / 660.0;
    parsed.right_y = (channel[1] - 1024) / 660.0;
    parsed.left_x = (channel[2] - 1024) / 660.0;
    parsed.left_y = (channel[3] - 1024) / 660.0;
    const int wheel = f[16] | f[17] << 8;
    parsed.wheel = wheel >= 364 && wheel <= 1684 ? (wheel - 1024) / 660.0 : 0;
    parsed.online = true;
    output = parsed;
    return true;
}
}
