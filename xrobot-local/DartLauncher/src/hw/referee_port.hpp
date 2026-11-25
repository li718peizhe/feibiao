// 订阅 Referee 的 launcher_ref Topic：0x020A 闸门和发射指令、0x0001 比赛阶段、0x0105 飞镖数据。
#pragma once
#include "Referee.hpp"
#include "mode_logic.hpp"
#include "referee_tracker.hpp"
#include "message.hpp"
namespace dart {
// Topic 由先构造的 Referee 实例提供；名称与配置保持一致。
class RefereePort {
public:
    // ref 须先构造且配置 launcher_ref Topic，构造后等待下一次发布。
    explicit RefereePort(Referee &ref);
    // 每个控制周期都要调用，now 为单调启动时间 s：“太久没更新”随时间变化，没有新数据也要重新算。
    // 返回本次是否收到了新发布，避免控制循环不断延长旧数据的在线时间；online 由调用方按收到时刻自己算。
    bool Update(RefereeInput &out, double now);
private:
    LibXR::Topic::ASyncSubscriber<Referee::LauncherPack> sub_;
    // 最近一次发布的原始字段和判断“收到过没有、太不太旧”的跟踪器（纯逻辑，在 src/ 里有主机测试）。
    RefereeRaw raw_{};
    RefereeTracker tracker_{};
};
}
