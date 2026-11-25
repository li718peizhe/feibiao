// 遥控器拨杆、摇杆、回零手势和裁判发射指令的纯逻辑映射。
#pragma once
#include "dart_params.hpp"
#include <cstdint>

namespace dart {
enum class Mode { Powerless, Manual, Automatic };
// 比赛阶段，数值同 0x0001 的 bit 4-7；Unknown 表示没收到过或太久没更新。
enum class Stage : std::int8_t { Unknown = -1, NotStarted = 0, Preparing = 1, SelfCheck = 2,
                                 Countdown = 3, InMatch = 4, Settlement = 5 };
enum class Switch { Down = -1, Middle = 0, Up = 1 };
enum class ManualGroup { Aim, Slider, Loading };
// 摇杆和拨轮归一到 [-1,1]，online 由 100 ms 超时判定。
struct RemoteInput {
    Switch right = Switch::Down, left = Switch::Middle;
    double left_x = 0, right_y = 0, wheel = 0;
    double right_x = 0, left_y = 0;
    bool online = false;
};
struct RefereeInput {
    // online：1 s 内收到过任何裁判帧。gate_open：收到过 0x020A、不太旧，而且发射站状态为 0（已开启）。
    bool online = false, gate_open = false;
    std::uint16_t launch_time = 0;
    // 阶段 3c：0x0001 的比赛阶段和本阶段剩余秒数；收到过但太久没更新，阶段按 Unknown。
    Stage stage = Stage::Unknown;
    std::uint16_t stage_remaining = 0;
    // 0x0105，只用来显示，不参与控制；dart_valid 为假时下面几个数无意义。
    bool dart_valid = false;
    std::uint8_t dart_countdown = 0, dart_target = 0, dart_last_target = 0, dart_hits = 0;
};
// 速度单位与对应机构一致，瞄准/滑块回零（home_aim/home_slider）、上膛、取镖、电磁铁切换、故障复位和发射命令均为单拍事件。
struct ModeCommand {
    Mode mode = Mode::Powerless;
    ManualGroup group = ManualGroup::Aim;
    double yaw_rate = 0, pitch_rate = 0, slider_rate = 0, lock_rate = 0;
    double arm_rate = 0, crank_rate = 0;
    bool home_aim = false, home_slider = false, cock_request = false, fire = false, salvo = false;
    bool magnet_toggle = false; // 装弹组拨轮负向保持 2 s：手动切换电磁铁通断，台架测试用。
    bool fetch_request = false; // 装弹组拨轮正向保持 2 s：空载取镖、持镖独立装填。
    bool fault_reset = false;
    bool stop_sequence = false;
    bool leave_manual = false; // 这一次从手动切到别的模式，适配层据此停掉手动点动。
    // 阶段 3c：由 MatchFlow 填。match_override 为真时遥控器只剩急停和故障复位，上面的模式、速度、请求已被改写。
    bool match_override = false, estop = false;
    // 自动请求：适配层按手动手势同样的前提条件执行，不满足就丢弃。
    bool auto_home_slider = false, auto_home_aim = false, auto_home_cancel = false;
    bool auto_cock = false, auto_decock = false, auto_reset = false;
    // 当前阶段不许上膛或发射，请求已被丢弃；cocked_warning 是准备阶段发现上着膛。适配层限频后打日志。
    bool cock_refused = false, fire_refused = false, cocked_warning = false;
};
// 自动许可由配置控制，也可在构造时显式指定。
class ModeLogic {
public:
    explicit ModeLogic(bool auto_enabled = kAutoEnabled);
    // dt 为 s；离线或无效遥控输入立即无力并停止流程。
    ModeCommand Update(const RemoteInput &remote, const RefereeInput &referee, double dt);
private:
    bool auto_enabled_, initialized_ = false, wheel_sent_ = false;
    bool referee_initialized_ = false, previous_referee_online_ = false;
    Switch previous_left_ = Switch::Middle;
    Mode previous_mode_ = Mode::Powerless;
    ManualGroup previous_group_ = ManualGroup::Aim;
    double wheel_time_ = 0, fault_reset_time_ = 0;
    int wheel_direction_ = 0;
    std::uint16_t previous_launch_time_ = 0;
};
}
