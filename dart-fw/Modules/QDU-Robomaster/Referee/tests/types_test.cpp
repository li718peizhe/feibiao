#include <cstddef>
#include <cstring>
#include <type_traits>

#include "RefereeTypes.hpp"

using Packet = RefereeTypes::RobotGameRefereePack;
static_assert(std::is_trivially_copyable_v<Packet>);
static_assert(std::is_standard_layout_v<Packet>);
static_assert(alignof(Packet) == 1);
static_assert(sizeof(RefereeTypes::RobotStatus) == 13);
static_assert(sizeof(RefereeTypes::GameStatus) == 11);
static_assert(sizeof(RefereeTypes::SentryInfo) == 5);
static_assert(sizeof(RefereeTypes::RFID) == 5);
static_assert(sizeof(RefereeTypes::RobotPosForSentry) == 40);
static_assert(sizeof(RefereeTypes::RobotPOS) == 12);
static_assert(sizeof(RefereeTypes::RobotBuff) == 8);
static_assert(sizeof(RefereeTypes::RobotDamage) == 1);
static_assert(sizeof(RefereeTypes::LauncherData) == 7);
static_assert(sizeof(RefereeTypes::RadarMarkProgress) == 2);
static_assert(offsetof(RefereeTypes::LauncherData, bullet_speed) == 3);
static_assert(offsetof(RefereeTypes::RobotBuff, cooling_acc) == 1);
static_assert(offsetof(RefereeTypes::RobotBuff, attack_buff) == 5);
static_assert(sizeof(Packet) == 117);
static_assert(offsetof(Packet, robot_status) == 0);
static_assert(offsetof(Packet, game_status) == 13);
static_assert(offsetof(Packet, sentry_info) == 24);
static_assert(offsetof(Packet, rfid) == 29);
static_assert(offsetof(Packet, bullet_17_remain) == 34);
static_assert(offsetof(Packet, our_outpose) == 36);
static_assert(offsetof(Packet, red_base) == 38);
static_assert(offsetof(Packet, sentry_pos) == 40);
static_assert(offsetof(Packet, robot_pos) == 80);
static_assert(offsetof(Packet, launcher_17_heat) == 92);
static_assert(offsetof(Packet, launcher_42_heat) == 94);
static_assert(offsetof(Packet, launcher_data) == 96);
static_assert(offsetof(Packet, shot_seq) == 103);
static_assert(offsetof(Packet, robot_buff) == 105);
static_assert(offsetof(Packet, radar_mark) == 113);
static_assert(offsetof(Packet, robot_damage) == 115);
static_assert(offsetof(Packet, hurt_seq) == 116);

static_assert(sizeof(RefereeTypes::DartCountdown) == 3);
static_assert(offsetof(RefereeTypes::DartCountdown, countdown) == 0);

int main()
{
  // 2027 通信协议 0x0105：发射剩余 30 s，最近命中 6（B 前哨），对方累计被命中 4 次，选中 4（末端移动）。
  // 选中的目标占 bit 6-8，旧的 2 位定义会把 4 读成 0。
  const unsigned char bytes[3] = {30, 0x26, 0x01};
  RefereeTypes::DartCountdown dart;
  std::memcpy(&dart, bytes, sizeof(dart));
  if (dart.countdown != 30 || dart.dart_last_target != 6 || dart.attack_count != 4 ||
      dart.dart_target != 4 || dart.res != 0)
  {
    return 1;
  }
  // 选中 1~3 和默认 0。
  for (unsigned target = 0; target <= 4; ++target)
  {
    const unsigned value = target << 6;
    const unsigned char encoded[3] = {0, static_cast<unsigned char>(value & 0xFF),
                                      static_cast<unsigned char>(value >> 8)};
    std::memcpy(&dart, encoded, sizeof(dart));
    if (dart.dart_target != target || dart.dart_last_target != 0 || dart.attack_count != 0)
    {
      return 2;
    }
  }
  return 0;
}
