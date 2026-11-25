// 闸门超过 kRefereeGateStale 没收到新的 0x020A 就不算全开；比赛阶段和飞镖数据超过 kRefereeStageStale 没更新就不算数。
#include "referee_tracker.hpp"
#include "dart_params.hpp"

namespace dart {
namespace {
// 0x0001 bit 4-7 的比赛阶段，0~5 以外都当作未知。
Stage ToStage(std::uint8_t progress) {
    return progress <= 5 ? static_cast<Stage>(progress) : Stage::Unknown;
}
}

void RefereeTracker::Update(const RefereeRaw &raw, double now, RefereeInput &out) {
    if (raw.gs_seq != gs_seq_) {
        gs_seq_ = raw.gs_seq;
        gs_time_ = now;
        gs_seen_ = true;
    }
    if (raw.dart_seq != dart_seq_) {
        dart_seq_ = raw.dart_seq;
        dart_time_ = now;
        dart_seen_ = true;
    }
    if (raw.dc_seq != dc_seq_) {
        dc_seq_ = raw.dc_seq;
        dc_time_ = now;
        dc_seen_ = true;
    }
    // 没收到过 0x020A 就没有闸门状态：初始值 0 是“已开启”，不能当真。
    out.gate_open = dc_seen_ && now - dc_time_ <= kRefereeGateStale && raw.opening_status == 0;
    out.launch_time = dc_seen_ ? raw.launch_time : 0;
    const bool stage_fresh = gs_seen_ && now - gs_time_ <= kRefereeStageStale;
    out.stage = stage_fresh ? ToStage(raw.game_progress) : Stage::Unknown;
    out.stage_remaining = stage_fresh ? raw.stage_remain_time : 0;
    out.dart_valid = dart_seen_ && now - dart_time_ <= kRefereeStageStale;
    out.dart_countdown = out.dart_valid ? raw.dart_countdown : 0;
    out.dart_target = out.dart_valid ? raw.dart_target : 0;
    out.dart_last_target = out.dart_valid ? raw.dart_last_target : 0;
    out.dart_hits = out.dart_valid ? raw.dart_hits : 0;
}
}
