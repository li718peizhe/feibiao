#pragma once

#include <cstdint>

/**
 * @brief 裁判模块发布的数据类型。
 *        Data types published by the referee Module.
 */
namespace RefereeTypes
{
/**
 * @brief 0x0001 比赛状态数据。
 *        0x0001 game status data.
 */
struct [[gnu::packed]] GameStatus
{
  uint8_t game_type : 4;      /* 比赛类型 */
  uint8_t game_progress : 4;  /* 当前比赛阶段 */
  uint16_t stage_remain_time; /* 当前阶段剩余时间 */
  uint64_t sync_time_stamp;   /* 时间戳 */
};

/**
 * @brief 0x0105 飞镖发射相关数据，固定以 1 Hz 发送。
 *        0x0105 dart launch data, sent at a fixed 1 Hz.
 *
 * 2027 通信协议 V1.0.0（20261029）：选中的目标是 bit 6-8 共 3 位（旧版只有 2 位，末端移动目标 4 会被读成 0）。
 * Per the 2027 protocol V1.0.0 the selected target is the 3 bits 6-8; it used to be 2 bits,
 * which read the terminal moving target (4) as 0.
 */
struct [[gnu::packed]] DartCountdown
{
  uint8_t countdown; /*己方飞镖发射剩余时间，单位：秒*/

  uint16_t dart_last_target : 3; /*bit0-2最近一次己方飞镖击中的目标：1 R 前哨，2 基地固定，3 随机固定，4 随机移动，5 末端移动，6 B 前哨*/
  uint16_t attack_count : 3;     /*bit3-5对方最近被击中的目标累计被击中次数，至多 4*/
  uint16_t dart_target : 3;      /*bit6-8飞镖此时选定的击打目标：0 前哨站或未选，1 基地固定，2 随机固定，3 随机移动，4 末端移动*/
  uint16_t res : 7;              /* bit9-15保留位 */
};

/**
 * @brief 0x0201 机器人性能体系数据。
 *        0x0201 robot performance data.
 */
struct [[gnu::packed]] RobotStatus
{
  uint8_t robot_id;                  /* 本机器人 ID */
  uint8_t robot_level;               /* 机器人等级 */
  uint16_t remain_hp;                /* 机器人当前血量 */
  uint16_t max_hp;                   /* 机器人血量上限 */
  uint16_t shooter_cooling_value;    /* 机器人射击热量每秒冷却值 */
  uint16_t shooter_heat_limit;       /* 机器人射击热量上限 */
  uint16_t chassis_power_limit;      /* 机器人底盘功率上限 */
  uint8_t power_gimbal_output : 1;   /* gimbal输出，0为无输出，1为24V输出 */
  uint8_t power_chassis_output : 1;  /* chassis输出，0为无输出，1为24V输出*/
  uint8_t power_launcher_output : 1; /* shooter输出，0为无输出，1为24V 输出 */
};

/**
 * @brief 0x0203 机器人位置数据。
 *        0x0203 robot position data.
 */
struct [[gnu::packed]] RobotPOS
{
  float x;     /* 本机器人位置x坐标，单位m */
  float y;     /* 本机器人位置y坐标，单位m */
  float angle; /* 本机器人测速模块的朝向 单位：度 正北为0度 */
};

/**
 * @brief 0x0209 机器人 RFID 模块状态。
 *        0x0209 robot RFID module status.
 */
struct [[gnu::packed]] RFID
{
  uint32_t own_base : 1;                          /*己方基地增益点*/
  uint32_t own_highland_center : 1;               /*己方中央高地增益点*/
  uint32_t enemy_highland_center : 1;             /*对方中央高地增益点*/
  uint32_t own_trapezium : 1;                     /*己方梯形高地增益点*/
  uint32_t enemy_trapezium : 1;                   /*对方梯形高地增益点*/
  uint32_t own_slope_before_R1B1 : 1;             /*己方飞坡点（靠近己方一侧飞坡前*/
  uint32_t own_slope_after_R1B1 : 1;              /*己方飞坡点（靠近己方一侧飞坡后*/
  uint32_t enemy_slope_before_R4B4 : 1;           /*对方飞坡点（靠近己方一侧飞坡前*/
  uint32_t enemy_slope_after_R4B4 : 1;            /*对方飞坡点（靠近己方一侧飞坡后*/
  uint32_t own_terrain_crossing_up_R2B2 : 1;      /*己方地形增益(中央高地下方*/
  uint32_t own_terrain_crossing_down_R2B2 : 1;    /*己方地形增益(中央高地上方*/
  uint32_t enemy_terrain_corrssing_up_R2B2 : 1;   /*对方地形增益(中央高地下方*/
  uint32_t enemy_terrain_corrssing_down_R2B2 : 1; /*对方地形增益(中央高地上*/
  uint32_t own_terrain_crossing_up_R3B3 : 1;      /*己方地形增益点(公路下方*/
  uint32_t own_terrain_crossing_down_R3B3 : 1;    /*己方地形增益点(公路上方*/
  uint32_t enemy_terrain_corrssing_up_R3B5 : 1;   /*对方地形增益点(公路下方*/
  uint32_t enemy_terrain_corrssing_down_R3B3 : 1; /*对方地形增益(公路上方*/
  uint32_t own_fortress : 1;                      /*己方堡垒增益点*/
  uint32_t own_outpost : 1;                       /*己方前哨站增益点*/
  uint32_t own_blood_supply_unoverlapping : 1;    /*与资源区不重叠的/UL补给区*/
  uint32_t own_blood_supply_overlapping : 1;      /*己方与资源区重叠的补给区*/
  uint32_t own_assemble : 1;                      /*己方装配增益点*/
  uint32_t enemy_assemble : 1;                    /*对方装配增益点*/
  uint32_t center_resource_RMUL : 1;              /*中心增益点（仅 RMUL 适用）*/
  uint32_t enemy_fortress : 1;                    /*对方堡垒增益点*/
  uint32_t enemy_outpost : 1;                     /*对方前哨站增益点*/
  uint32_t own_tunnel_cross_down : 1;             /*己方隧道增益点（己方一侧公路区下方）*/
  uint32_t own_tunnel_cross_up : 1;               /*己方隧道增益点（己方一侧公路区上方）*/
  uint32_t own_tunnel_zrapezium_down : 1;         /*己方隧道增益(己方梯形高地较低处*/
  uint32_t own_tunnel_zrapezium_up : 1;           /*己方隧道增益(己方梯形高地较高处*/
  uint32_t enemy_tunnel_cross_down : 1;           /*对方隧道增益（对方一侧公路区下方*/
  uint32_t enemy_tunnel_cross_up : 1;             /*对方隧道增益（对方一侧公路区上方*/

  uint32_t enemy_tunnel_zrapezium_down : 1; /*对方隧道增益(对方梯形高地低处*/
  uint32_t enemy_tunnel_zrapezium_up : 1;   /*对方隧道增益(对方梯形高地较高处*/
};

/**
 * @brief 0x020B 地面机器人位置数据。
 *        0x020B ground robot position data.
 */
struct [[gnu::packed]] RobotPosForSentry
{
  float hero_x;       /*己方英雄机器人位置 x 轴坐标，单位：m*/
  float hero_y;       /*己方英雄机器人位置 y 轴坐标，单位：m*/
  float engineer_x;   /*己方工程机器人位置 x 轴坐标，单位：m*/
  float engineer_y;   /*己方工程机器人位置 y 轴坐标，单位：m*/
  float standard_3_x; /*己方 3 号步兵机器人位置 x 轴坐标，单位：m*/
  float standard_3_y; /*己方 3 号步兵机器人位置 y 轴坐标，单位：m*/
  float standard_4_x; /*己方 4 号步兵机器人位置 x 轴坐标，单位：m*/
  float standard_4_y; /*己方 4 号步兵机器人位置 y 轴坐标，单位：m*/
  float res_1;        /*保留位*/
  float res_2;        /*保留位*/
};

/**
 * @brief 0x020D 哨兵自主决策相关信息同步。
 *        0x020D sentry autonomous decision information.
 */
struct [[gnu::packed]] SentryInfo
{
  uint32_t exchanged_bullet_num : 11;  /*允许发弹量*/
  uint32_t exchanged_bullet_times : 4; /*成功远程兑换允许发弹量的次数*/
  uint32_t exchanged_blood_times : 4;  /*哨兵机器人成功远程兑换血量的次数*/
  uint32_t could_risen_free : 1;       /*当前是否可以确认免费复活*/
  uint32_t could_risen_exchanged : 1;  /*哨兵机器人当前是否可以兑换立即复活*/
  uint32_t risen_cost : 10;            /*哨兵机器人当前若兑换立即复活需要花费的金币数*/
  uint32_t res1 : 1;                   /*保留位*/

  uint32_t current_state : 2;  /*哨兵当前姿态*/
  uint32_t own_mech_state : 1; /* 己方能量机关是否能进入正在激活状态 */
  uint32_t res2 : 1;           /*保留位*/
};

/**
 * @brief 0x0204 机器人增益和底盘能量数据。
 *        0x0204 robot buff and chassis energy data.
 */
struct [[gnu::packed]] RobotBuff
{
  uint8_t healing_buff;                  /*机器人回血增益,百分比*/
  uint16_t cooling_acc;                  /*机器人射击热量冷却增益 单位是s^{-1}*/
  uint8_t defense_buff;                  /*机器人防御增益 百分比*/
  uint8_t vulnerability_buff;            /*机器人负防御增益 百分比*/
  uint16_t attack_buff;                  /*机器人攻击增益 百分比*/
  uint8_t percent_125_remain_energy : 1; /* 在剩余能量≥125%时为 1 */
  uint8_t percent_100_remain_energy : 1; /* 在剩余能量≥100%时为 1 */
  uint8_t percent_50_remain_energy : 1;  /* 在剩余能量≥50%时为 1 */
  uint8_t percent_30_remain_energy : 1;  /* 在剩余能量≥30%时为 1 */
  uint8_t percent_15_remain_energy : 1;  /* 在剩余能量≥15%时为 1 */
  uint8_t percent_5_remain_energy : 1;   /* 在剩余能量≥5%时为 1 */
  uint8_t percent_1_remain_energy : 1;   /* 在剩余能量≥1%时为 1 */
};

/**
 * @brief 0x0206 伤害状态数据。
 *        0x0206 damage data.
 */
struct [[gnu::packed]] RobotDamage
{
  uint8_t armor_id : 4;    /* 受击打的装甲板id */
  uint8_t damage_type : 4; /* 血量变化类型 */
};

/**
 * @brief 0x0207 实时射击数据。
 *        0x0207 real-time shooting data.
 */
struct [[gnu::packed]] LauncherData
{
  uint8_t bullet_type;   /* 弹丸类型 */
  uint8_t launcherer_id; /* 发射机构 ID */
  uint8_t bullet_freq;   /* 弹丸射速 Hz */
  float bullet_speed;    /* 弹丸初速度 m/s */
};

/**
 * @brief 0x020C 雷达标记进度数据。
 *        0x020C radar marking progress data.
 */
struct [[gnu::packed]] RadarMarkProgress
{
  uint8_t mark_enemy_hero_state : 1;       /*对方 1 号英雄机器人易伤情况*/
  uint8_t mark_enemy_engineer_state : 1;   /*对方 2 号工程机器人易伤情况*/
  uint8_t mark_enemy_standard_3_state : 1; /*对方 3 号步兵机器人易伤情况*/
  uint8_t mark_enemy_standard_4_state : 1; /*对方 4 号步兵机器人易伤情况*/
  uint8_t mark_enemy_sentry_state : 1;     /*对方哨兵机器人易伤情况*/
  uint8_t mark_own_hero_state : 1;         /*己方 1 号英雄机器人特殊标识情况*/
  uint8_t mark_own_engineer_state : 1;     /*己方 2 号工程机器人特殊标识情况*/
  uint8_t mark_own_standard_3_state : 1;   /*己方 3 号步兵机器人特殊标识情况*/
  uint8_t mark_own_standard_4_state : 1;   /*己方 4 号步兵机器人特殊标识情况*/
  uint8_t mark_own_sentry_state : 1;       /*己方哨兵机器人特殊标识情况*/
};

/**
 * @brief 机器人、比赛与发射相关的裁判系统摘要，紧凑布局共 117 字节。
 *        Referee summary of robot, game and launcher data, 117 bytes in packed layout.
 *
 * 前 92 字节的布局保持不变，之后的字段依次追加在末尾。
 * The layout of the first 92 bytes is unchanged; later fields are appended at the end.
 */
struct [[gnu::packed]] RobotGameRefereePack
{
  RobotStatus robot_status;     /* 机器人状态 */
  GameStatus game_status;       /* 比赛信息 */
  SentryInfo sentry_info;       /*哨兵数据*/
  RFID rfid;                    /*机器人RFID模块状态*/
  uint16_t bullet_17_remain;    /*  17mm 弹丸允许发弹量 */
  uint16_t our_outpose;         /* 己方前哨站 */
  uint16_t red_base;            /* 己方基地 */
  RobotPosForSentry sentry_pos; /* 0x020B */
  RobotPOS robot_pos;
  uint16_t launcher_17_heat;    /* 0x0202 17mm 发射机构的射击热量 */
  uint16_t launcher_42_heat;    /* 0x0202 42mm 发射机构的射击热量 */
  LauncherData launcher_data;   /* 0x0207 最近一发的射击数据 */
  uint16_t shot_seq;            /* 0x0207 计数，每收到一帧加 1 */
  RobotBuff robot_buff;         /* 0x0204 */
  RadarMarkProgress radar_mark; /* 0x020C */
  RobotDamage robot_damage;     /* 0x0206 最近一次的伤害数据 */
  uint8_t hurt_seq;             /* 0x0206 计数，每收到一帧加 1 */
};
}  // namespace RefereeTypes
