#ifndef REFEREE_H
#define REFEREE_H

#include "main.h"
#include "protocol.h"

typedef enum
{
    RED_HERO = 1,
    RED_ENGINEER = 2,
    RED_STANDARD_1 = 3,
    RED_STANDARD_2 = 4,
    RED_STANDARD_3 = 5,
    RED_AERIAL = 6,
    RED_SENTRY = 7,
    RED_DART = 8,
    RED_RADAR = 9,
    RED_OUTPOST = 10,
    RED_BASE = 11,
    BLUE_HERO = 101,
    BLUE_ENGINEER = 102,
    BLUE_STANDARD_1 = 103,
    BLUE_STANDARD_2 = 104,
    BLUE_STANDARD_3 = 105,
    BLUE_AERIAL = 106,
    BLUE_SENTRY = 107,
    BLUE_DART = 108,
    BLUE_RADAR = 109,
    BLUE_OUTPOST = 110,
    BLUE_BASE = 111,
} robot_id_t;
typedef enum
{
    PROGRESS_UNSTART = 0,
    PROGRESS_PREPARE = 1,
    PROGRESS_SELFCHECK = 2,
    PROGRESS_5sCOUNTDOWN = 3,
    PROGRESS_BATTLE = 4,
    PROGRESS_CALCULATING = 5,
} game_progress_t;


typedef __packed struct/*0X0001*/
{
    uint8_t game_type : 4;
    uint8_t game_progress : 4;
    uint16_t stage_remain_time;
    uint64_t SyncTimeStamp;
} game_status_t;

typedef __packed struct/*0X0002*/
{
    uint8_t winner;
} ext_game_result_t;


typedef __packed struct/*0X0003*/
{
  uint16_t ally_1_robot_HP;
  uint16_t ally_2_robot_HP;
  uint16_t ally_3_robot_HP;
  uint16_t ally_4_robot_HP;
  uint16_t reserved;
  uint16_t ally_7_robot_HP;
  uint16_t ally_outpost_HP;
  uint16_t ally_base_HP;
} game_robot_HP_t;

typedef __packed struct /*0X0101*/
{
    uint32_t event_data;
} event_data_t;

typedef __packed struct /*0X0102*/
{
    uint8_t reserved;
    uint8_t supply_robot_id;
    uint8_t supply_projectile_step;
    uint8_t supply_projectile_num;
} ext_supply_projectile_action_t;

typedef __packed struct /*0X0103*/
{
    uint8_t supply_projectile_id;
    uint8_t supply_robot_id;
    uint8_t supply_num;
} ext_supply_projectile_booking_t;


typedef __packed struct/*0x0104*/
{
    uint8_t level;
    uint8_t offending_robot_id;
    uint8_t count;
} referee_warning_t;

typedef __packed struct/*0X0105*/
{
    uint8_t dart_remaining_time;
    uint16_t dart_info;
} dart_info_t;


typedef __packed struct/*0X0201*/
{
 uint8_t robot_id;
uint8_t robot_level;
uint16_t current_HP;
 uint16_t maximum_HP;
uint16_t shooter_barrel_cooling_value;
uint16_t shooter_barrel_heat_limit;
uint16_t chassis_power_limit;
 uint8_t power_management_gimbal_output : 1;
uint8_t power_management_chassis_output : 1;
 uint8_t power_management_shooter_output :1;
}robot_status_t;
//typedef __packed struct
//{
// uint8_t robot_id;
// uint8_t robot_level;
// uint16_t current_HP;
// uint16_t maximum_HP;
// uint16_t shooter_id1_17mm_barrel_cooling_value;
// uint16_t shooter_id1_17mm_barrel_heat_limit;
// uint16_t shooter_id1_17mm_initial_launching_speed_limit;
// uint16_t shooter_id2_17mm_barrel_cooling_valuecooling_rate;
//uint16_t shooter_id2_17mm_barrel_heatcooling_limit;
//uint16_t shooter_id2_17mm_initial_launching_speed_limit;
//uint16_t shooter_id1_42mm_barrel_cooling_value;
// uint16_t shooter_id1_42mm_barrel_heat_cooling_limit;
// uint16_t shooter_id1_42mm_initial_launching_speed_limit;
// uint16_t chassis_power_limit;
// uint8_t power_management_gimbal_output : 1;
// uint8_t power_management_chassis_output : 1;
// uint8_t power_management_shooter_output : 1;
//}robot_status_t;

typedef __packed struct/*0X0202*/
{
    uint16_t chassis_voltage;
    uint16_t chassis_current;
    float chassis_power;
    uint16_t buffer_energy;
    uint16_t shooter_17mm_1_barrel_heat;
    uint16_t shooter_42mm_barrel_heat;
} power_heat_data_t;

#define shooter_17mm_2_barrel_heat shooter_17mm_1_barrel_heat

typedef __packed struct /*0X0203*/
{
    float x;
    float y;
    float angle;
} game_robot_pos_t;


typedef __packed struct/*0X0204*/
{
    uint8_t recovery_buff;
    uint16_t cooling_buff;
    uint8_t defence_buff;
    uint8_t vulnerability_buff;
    uint16_t attack_buff;
	uint8_t remaining_energy; 
} buff_t;


typedef __packed struct/*0X0205*/
{
    uint8_t airforce_status;
    uint8_t time_remain;
} air_support_data_t;

typedef __packed struct/*0X0206*/
{
    uint8_t armor_id : 4;
    uint8_t HP_deduction_reason : 4;
} hurt_data_t;


typedef __packed struct/*0X0207*/
{
    uint8_t bullet_type;
    uint8_t shooter_number;
    uint8_t launching_frequency;
    float initial_speed;
} shoot_data_t;


typedef __packed struct/*0X0208*/
{
    uint16_t projectile_allowance_17mm;
    uint16_t projectile_allowance_42mm;
    uint16_t remaining_gold_coin;
    uint16_t projectile_allowance_fortress;
} projectile_allowance_t;


typedef __packed struct/*0x0209*/
{
    uint32_t rfid_status;
    uint8_t rfid_status_2;
} rfid_status_t;


typedef __packed struct/*0X020A*/
{
    uint8_t dart_launch_opening_status;
    uint8_t reserved;
    uint16_t target_change_time;
    uint16_t latest_launch_cmd_time;
} dart_client_cmd_t;


typedef __packed struct/*0X020B*/
{
    float hero_x;
    float hero_y;
    float engineer_x;
    float engineer_y;
    float standard_3_x;
    float standard_3_y;
    float standard_4_x;
    float standard_4_y;
    float reserved1;
    float reserved2;
} ground_robot_position_t;


typedef __packed struct/*0X020C*/
{
uint16_t mark_progress;
} radar_mark_data_t;


typedef __packed struct/*0X020D*/
{
uint32_t sentry_info; 
  uint16_t sentry_info_2;
} sentry_info_t;


typedef __packed struct/*0X020E*/
{
    uint8_t radar_info;
} radar_info_t;




typedef __packed struct/*0X0100*/
{
    uint8_t delete_type;
    uint8_t layer;
} interaction_layer_delete_t;

typedef __packed struct/*0X0101*/
{
    uint8_t figure_name[3];
    uint32_t operate_tpye: 3;
    uint32_t figure_tpye: 3;
    uint32_t layer: 4;
    uint32_t color: 4;
    uint32_t details_a: 9;
    uint32_t details_b: 9;
    uint32_t width: 10;
    uint32_t start_x: 11;
    uint32_t start_y: 11;
    uint32_t details_c: 10;
    uint32_t details_d: 11;
    uint32_t details_e: 11;
} interaction_figure_t;
typedef __packed struct/*0X0102*/
{
    interaction_figure_t interaction_figure[2];
} interaction_figure_2_t;

typedef __packed struct/*0X0103*/
{
    interaction_figure_t interaction_figure[5];
} interaction_figure_3_t;
typedef __packed struct/*0X0104*/
{
    interaction_figure_t interaction_figure[7];
} interaction_figure_4_t;



typedef __packed struct/*0X0120*/
{
    uint32_t sentry_cmd;
} sentry_cmd_t;


typedef __packed struct/*0X0121*/
{
    uint8_t radar_cmd;
} radar_cmd_t;


typedef __packed struct/*0X0301*/
{
    uint16_t data_cmd_id;
    uint16_t sender_id;
    uint16_t receiver_id;
    uint8_t user_data[user_data_max];
} robot_interaction_data_t;


typedef __packed struct/*0X0302*/
{
    uint8_t data[30];
} custom_robot_data_t;





typedef __packed struct/*0X0303*/
{
    float target_position_x;
    float target_position_y;
    uint8_t cmd_keyboard;
    uint8_t target_robot_id;
    uint8_t cmd_source;
} map_command_t;


typedef __packed struct/*0X0304*/
{
    int16_t mouse_x;
    int16_t mouse_y;
    int16_t mouse_z;
    int8_t left_button_down;
    int8_t right_button_down;
    uint16_t keyboard_value;
    uint16_t reserved;
} remote_control_t;


typedef __packed struct/*0X305*/
{
    uint16_t hero_position_x; 
  uint16_t hero_position_y; 
  uint16_t engineer_position_x; 
  uint16_t engineer_position_y; 
  uint16_t infantry_3_position_x; 
  uint16_t infantry_3_position_y; 
  uint16_t infantry_4_position_x; 
  uint16_t infantry_4_position_y; 
  uint16_t infantry_5_position_x; 
  uint16_t infantry_5_position_y; 
  uint16_t sentry_position_x; 
  uint16_t sentry_position_y;
} map_robot_data_t;


typedef __packed struct/*0X0306*/
{
    uint16_t key_value;
    uint16_t x_position: 12;
    uint16_t mouse_left: 4;
    uint16_t y_position: 12;
    uint16_t mouse_right: 4;
    uint16_t reserved;
} custom_client_data_t;


typedef __packed struct/*0X0307*/
{
    uint8_t intention;
    uint16_t start_position_x;
    uint16_t start_position_y;
    int8_t delta_x[49];
    int8_t delta_y[49];
    uint16_t sender_id;
} map_data_t;


typedef __packed struct/*0X0308*/
{
    uint16_t sender_id;
    uint16_t receiver_id;
    uint16_t user_data[30];
} custom_info_t;

typedef __packed struct /*0x0309*/
{ 
uint8_t data[30]; 
}robot_custom_data_t; 

extern game_robot_HP_t game_robot_HP;			
extern game_robot_pos_t game_robot_pos;	
extern game_status_t game_state;		
extern event_data_t field_event;
extern void init_referee_struct_data(void);
extern void referee_data_solve(uint8_t *frame);
extern power_heat_data_t power_heat_data;
extern buff_t buff;
extern robot_status_t robot_state;
extern shoot_data_t shoot_data;
extern projectile_allowance_t projectile_allowance;
extern power_heat_data_t power_heat_data;//0202
extern dart_info_t dart_info;																			/*0X0105*/
extern dart_client_cmd_t dart_client_cmd;													/*0X020A*/
extern volatile uint32_t referee_last_update_time;
extern volatile uint32_t game_state_last_update_time;
extern volatile uint32_t dart_info_last_update_time;
extern volatile uint32_t dart_client_cmd_last_update_time;

#endif
							
