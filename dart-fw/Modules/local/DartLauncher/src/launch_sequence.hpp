// 固定容量队列移植 sequence.py：执行时按当前库存展开，故障保持舵机；磁铁由适配层限时，无力或故障时断电吸持。
#pragma once
#include "dart_inventory.hpp"
#include "launcher_io.hpp"
#include <array>
#include <cstdint>

namespace dart {
// 锁住是怎么确认的：没记录、慢压找底后锁止确认、探测判成已锁住。
enum class LatchSource : std::uint8_t { None, Press, Probe };
// 一发的发射记录，在“发射：解锁”打开夹爪那一拍写下；位置 m，电流是电调原始值，时间 s。
struct ShotRecord {
    std::uint32_t number = 0; // 开机以来第几发，故障复位不清零；0 表示还没发过
    double pitch = 0; // 解锁时的 pitch 位置
    LatchSource source = LatchSource::None;
    double bottom = 0; // 锁住那次慢压找到的底，只有 Press 有
    double hold_current = 0; // 合上夹爪那一步，滑块在锁止位托着发射机构时电流的平均值，只有 Press 有
    double check_current = 0; // 锁止确认保持窗口里滑块电流的最大值；Probe 时是探测保持窗口里的
    double wait = 0; // 确认锁住到解锁隔了多久，None 时为 0
};
// 名称、进入动作、完成条件和超时由枚举步骤定义，不在控制循环分配内存。
class LaunchSequence {
public:
    LaunchSequence(LauncherIo &io, DartInventory &inventory);
    // 清流程故障和队列，按安全规则停止电机，保留库存及执行器角度。
    void Reset();
    // 上膛、装填、直接发射或展开一至四发；返回命令是否接受。
    bool CommandCock();
    // 已锁住时退膛：滑块压住发射机构，张开夹爪，再带着它慢慢升回停靠位；没锁住或不能接受命令时返回 false，不报故障。
    bool CommandDecock();
    bool CommandLoad();
    // 调用方保证 FetchHolding() 或库存 Held()；交接取镖库存，完整装填后锁止回位。
    bool CommandLoadFromFetch();
    bool CommandFire();
    bool CommandSalvo(int count);
    // 自动取镖试运行（任务 3q）：取一发，第一发左边、以后左右轮流；推杆伸一次吸住、按 kCrankLiftSpeed 慢慢抬起，
    // 停在镖座上方举着。只要摆臂建了零点，不要求滑块和 pitch 回零。
    // 忙、有故障、库存记着磁铁上有镖、或者上一发取上来还没放镖（ReleaseFetch）时返回 false。
    bool CommandFetchTest();
    // 取镖试运行下探到镖上以后、操作手还没让磁铁通电放镖：磁铁上可能吸着镖，不能再去镖座下探。
    bool FetchHolding() const { return fetch_holding_; }
    // 下一发取哪边：-1 左、1 右。做完一发换另一边，中途取消或报故障不换。
    int NextFetchSide() const { return fetch_side_; }
    // 操作手让磁铁通电，放掉了取上来的镖。
    void ReleaseFetch() { fetch_holding_ = false; }
    // 手动操作取消自动流程，保持库存和执行器角度。
    void Cancel();
    // 单调时刻 s；powered=false 暂停普通流程，取消取镖试运行。
    void Tick(double now, bool powered = true);
    // 当前流程状态和最近进入的中文步骤；Revision 每进入一步递增。
    bool Busy() const { return has_current_ || size_ != 0; }
    // 正在跑的是退膛流程。比赛流程在结算和准备阶段只留退膛，其余流程要取消。
    bool Decocking() const { return decocking_ && Busy(); }
    // 正在跑的是单独的上膛流程（CommandCock）。结算时它不取消，让它做完再退膛，免得停在半路托着发射机构。
    bool Cocking() const { return cocking_ && Busy(); }
    SafetyFault Fault() const { return fault_; }
    const char *StepName() const { return label_; }
    std::uint32_t Revision() const { return revision_; }
    int Shots() const { return shots_; }
    // 最近一发的发射记录；number 为 0 表示开机以来还没发过。
    const ShotRecord &LastShot() const { return last_shot_; }
    // 这次慢压找到了底、滑块正压在底上；给测试和以后的状态显示用。
    bool AtBottom() const { return Snapshot().at_bottom; }
private:
    enum class Action : std::uint8_t {
        One, Cock, Load, Prefetch, Standby, Arm, Crank, LiftPrefetch, Lock, LatchCheck, Probe, Slider,
        Rise, Grip, PrefetchGrip, Release, Aim, Unlock, Flight, Press, Magnet, Wait, FetchGrip
    };
    enum class Condition : std::uint8_t {
        Immediate, Arm, Crank, Lock, LatchCheck, Probe, Pressed, Parked, Loaded,
        MagnetWait, Aimed, FlightWait, Approach, Wait
    };
    struct Step {
        const char *name = "空闲";
        Action action = Action::One;
        Condition condition = Condition::Immediate;
        double target = 0, timeout = 1;
        int side = 0;
        double speed = 0; // 推杆步骤的角速度 rad/s，0 表示按 kServoSpeed。
    };
    bool Accept();
    bool Append(const Step *steps, int count);
    bool Prepend(const Step *steps, int count);
    void ExpandCock();
    void ProbeSteps();
    void LoadSteps();
    void FetchSteps(int side, double park);
    void LiftPrefetch(int side, double park);
    bool FetchTestSteps(int side);
    void FireSteps(bool front);
    void ExpandOne();
    void ExpandLoad();
    void ExpandPrefetch();
    void ExpandStandby();
    void Enter();
    bool Done();
    void Fail(SafetyFault fault);
    SafetyState Snapshot() const;
    LauncherIo &io_;
    DartInventory &inventory_;
    std::array<Step, 64> queue_{};
    Step current_{};
    int size_ = 0, shots_ = 0;
    bool has_current_ = false, paused_ = false, decocking_ = false, cocking_ = false;
    bool fetch_test_ = false, fetch_holding_ = false;
    int fetch_side_ = -1;
    int fetch_held_side_ = 0; // 吸住时记来源，取镖完成后 fetch_side_ 已切到下一侧。
    int prefetch_held_side_ = 0, prefetch_supported_side_ = 0;
    // 装填放镖通电以后、推杆收起以前为真：这期间无力会让磁铁断电、重新吸住镖。
    bool release_window_ = false;
    double elapsed_ = 0, last_time_ = -1;
    double latch_check_target_ = 0;
    double latch_hold_start_ = -1;
    double probe_hold_start_ = -1;
    double push_start_ = -1;
    double bottom_window_start_ = -1;
    double bottom_window_position_ = 0;
    double bottom_position_ = 0;
    double lock_position_ = 0;
    bool push_started_ = false, bottom_found_ = false;
    LatchState probe_result_ = LatchState::Unknown;
    // 发射记录：确认锁住时把来源、底和电流记进 latch_record_（number、pitch、wait 不用），解锁时拷进 last_shot_。
    ShotRecord last_shot_{}, latch_record_{};
    double latch_time_ = -1;
    double hold_current_sum_ = 0;
    int hold_current_count_ = 0;
    double check_current_max_ = 0;
    std::uint32_t revision_ = 0;
    SafetyFault fault_ = SafetyFault::None;
    const char *label_ = "空闲";
};
}
