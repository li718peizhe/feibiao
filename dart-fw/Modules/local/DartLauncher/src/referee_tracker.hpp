// 把 Referee 模块发布的原始字段翻成 RefereeInput：靠收帧序号判断 0x0001、0x0105、0x020A 收到过没有、是不是太旧。
// 纯逻辑，不引用 LibXR，可以在主机上测；适配层把 LibXR 的结构体拷成 RefereeRaw 再交给它。
#pragma once
#include "mode_logic.hpp"
#include <cstdint>

namespace dart {
// 模块发布的原始字段。序号每收到一帧加 1（回绕），开机以来没收到过为 0。
struct RefereeRaw {
    std::uint8_t gs_seq = 0, dart_seq = 0, dc_seq = 0;
    std::uint8_t game_progress = 0;      // 0x0001 bit 4-7：比赛阶段
    std::uint16_t stage_remain_time = 0; // 0x0001：本阶段剩余秒数
    std::uint8_t dart_countdown = 0, dart_target = 0, dart_last_target = 0, dart_hits = 0; // 0x0105
    std::uint8_t opening_status = 0;     // 0x020A 发射站状态：0 已开启，1 关闭，2 正在开启或关闭
    std::uint16_t launch_time = 0;       // 0x020A 最后一次确认发射时的比赛剩余秒数
};
class RefereeTracker {
public:
    // 每个控制周期调用，now 为单调时刻 s；raw 是最近一次发布的内容，没有新发布时传上一次的。
    // 只写比赛流程用到的字段（gate_open、launch_time、stage、stage_remaining、dart_*）；
    // online 由调用方按“任意一帧”的收到时刻自己算。
    // 没收到过的包不能当真：模块里它们的初始值全是 0，而 0x020A 的发射站状态 0 正是“已开启”。
    void Update(const RefereeRaw &raw, double now, RefereeInput &out);
private:
    double gs_time_ = 0, dart_time_ = 0, dc_time_ = 0;
    std::uint8_t gs_seq_ = 0, dart_seq_ = 0, dc_seq_ = 0;
    bool gs_seen_ = false, dart_seen_ = false, dc_seen_ = false;
};
}
