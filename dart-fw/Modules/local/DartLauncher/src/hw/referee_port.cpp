// 裁判数据只在收到新 Topic 时更新；各包是否太旧按收帧序号变化的时刻判断（RefereeTracker），控制线程以 1 s 超时判离线。
#include "referee_port.hpp"
namespace dart {
RefereePort::RefereePort(Referee &ref) : sub_("launcher_ref") {
    static_cast<void>(ref);
    sub_.StartWaiting();
}
bool RefereePort::Update(RefereeInput &out, double now) {
    bool fresh = false;
    if (sub_.Available()) {
        const auto &p = sub_.GetData();
        raw_.gs_seq = p.gs_seq;
        raw_.dart_seq = p.dart_seq;
        raw_.dc_seq = p.dc_seq;
        raw_.game_progress = p.gs.game_progress;
        raw_.stage_remain_time = p.gs.stage_remain_time;
        raw_.dart_countdown = p.dart.countdown;
        raw_.dart_target = p.dart.dart_target;
        raw_.dart_last_target = p.dart.dart_last_target;
        raw_.dart_hits = p.dart.attack_count;
        raw_.opening_status = p.dc.opening_status;
        raw_.launch_time = p.dc.operator_cmd_launch_time;
        sub_.StartWaiting();
        fresh = true;
    }
    tracker_.Update(raw_, now, out);
    return fresh;
}
}
