#include "referee.h"
#include "string.h"
#include "stdio.h"
#include "CRC8_CRC16.h"
#include "protocol.h"
#include "cmsis_os.h"

// 调试变量
volatile uint32_t game_state_count = 0;
volatile uint32_t robot_state_count = 0;
volatile uint32_t dart_info_count = 0;
volatile uint32_t other_cmd_count = 0;
volatile uint32_t referee_last_update_time = 0;
volatile uint32_t game_state_last_update_time = 0;
volatile uint32_t dart_info_last_update_time = 0;
volatile uint32_t dart_client_cmd_last_update_time = 0;

// 调试用的原始数据缓冲区
volatile uint16_t debug_cmd_id = 0;
volatile uint16_t debug_data_length = 0;
volatile uint8_t debug_dart_raw_data[8] = {0};
volatile uint8_t debug_dart_copy_data[8] = {0};
volatile uint32_t referee_length_error_count = 0;

frame_header_struct_t referee_receive_header;
frame_header_struct_t referee_send_header;

game_status_t game_state;																		/*0X0001*/
ext_game_result_t game_result;															/*0X0002*/
game_robot_HP_t game_robot_HP;															/*0X0003*/
event_data_t field_event;																		/*0X0101*/
ext_supply_projectile_action_t supply_projectile_action_t;	/*0X0102*/
ext_supply_projectile_booking_t supply_projectile_booking_t;/*0X0103*/
referee_warning_t referee_warning;													/*0x0104*/
dart_info_t dart_info;																			/*0X0105*/
robot_status_t robot_state;															/*0X0201*/
power_heat_data_t power_heat_data;													/*0X0202*/
game_robot_pos_t game_robot_pos;														/*0X0203*/
buff_t buff;																								/*0X0204*/
air_support_data_t robot_energy;														/*0X0205*/
hurt_data_t robot_hurt;																			/*0X0206*/
shoot_data_t shoot_data;																		/*0X0207*/
projectile_allowance_t projectile_allowance;								/*0X0208*/
rfid_status_t robot_rfid;																		/*0x0209*/
dart_client_cmd_t dart_client_cmd;													/*0X020A*/
ground_robot_position_t ground_robot_position;							/*0X020B*/
radar_mark_data_t radar_mark_data;													/*0X020C*/
sentry_info_t SentryInfo;																		/*0X020D*/
radar_info_t RadarInfo;																			/*0X020E*/
interaction_layer_delete_t interaction_layer_delete;				/*0X0100*/
interaction_figure_t interaction_figure;										/*0X0101*/
interaction_figure_2_t interaction_figure_2;								/*0X0102*/
interaction_figure_3_t interaction_figure_3;								/*0X0103*/
interaction_figure_4_t interaction_figure_4;								/*0X0104*/
sentry_cmd_t sentry_cmd;																		/*0X0120*/
radar_cmd_t radar_cmd;																			/*0X0121*/
robot_interaction_data_t robot_interaction_data;						/*0X0301*/
custom_robot_data_t custom_robot_data;											/*0X0302*/
map_command_t map_command;																	/*0X0303*/
remote_control_t remote_control;														/*0X0304*/
map_robot_data_t map_robot_data;														/*0X0305*/
custom_client_data_t custom_client_data;										/*0X0306*/
map_data_t map_data;																				/*0X0307*/
custom_info_t custom_info;																	/*0X0308*/
robot_custom_data_t robot_custom_data;                      /*0X0309*/

static uint8_t copy_referee_payload(void *dst, uint16_t dst_size, const uint8_t *src)
{
    if (referee_receive_header.data_length != dst_size)
    {
        referee_length_error_count++;
        return 0;
    }

    memcpy(dst, src, dst_size);
    return 1;
}

static uint8_t copy_referee_payload_max(void *dst, uint16_t dst_size, const uint8_t *src)
{
    if (referee_receive_header.data_length > dst_size)
    {
        referee_length_error_count++;
        return 0;
    }

    memset(dst, 0, dst_size);
    memcpy(dst, src, referee_receive_header.data_length);
    return 1;
}




void init_referee_struct_data(void)
{
    referee_last_update_time = 0;
    game_state_last_update_time = 0;
    dart_info_last_update_time = 0;
    dart_client_cmd_last_update_time = 0;
    memset(&referee_receive_header, 0, sizeof(frame_header_struct_t));
    memset(&referee_send_header, 0, sizeof(frame_header_struct_t));
    memset(&game_state, 0, sizeof(game_status_t));
    memset(&game_result, 0, sizeof(ext_game_result_t));
    memset(&game_robot_HP, 0, sizeof(game_robot_HP_t));
    memset(&field_event, 0, sizeof(event_data_t));
    memset(&supply_projectile_action_t, 0, sizeof(ext_supply_projectile_action_t));
    memset(&supply_projectile_booking_t, 0, sizeof(ext_supply_projectile_booking_t));
    memset(&referee_warning, 0, sizeof(referee_warning_t));
    memset(&robot_state, 0, sizeof(robot_status_t));
    memset(&dart_info, 0, sizeof(dart_info_t));
    memset(&power_heat_data, 0, sizeof(power_heat_data_t));
    memset(&game_robot_pos, 0, sizeof(game_robot_pos_t));
    memset(&buff, 0, sizeof(buff_t));
    memset(&robot_energy, 0, sizeof(air_support_data_t));
    memset(&robot_hurt, 0, sizeof(hurt_data_t));
    memset(&shoot_data, 0, sizeof(shoot_data_t));
    memset(&projectile_allowance, 0, sizeof(projectile_allowance_t));
    memset(&robot_rfid, 0, sizeof(rfid_status_t));
    memset(&dart_client_cmd, 0, sizeof(dart_client_cmd_t));
    memset(&ground_robot_position, 0, sizeof(ground_robot_position_t));
    memset(&radar_mark_data, 0, sizeof(radar_mark_data_t));
    memset(&SentryInfo, 0, sizeof(sentry_info_t));
    memset(&RadarInfo, 0, sizeof(radar_info_t));
    memset(&robot_interaction_data, 0, sizeof(robot_interaction_data_t));
    memset(&interaction_layer_delete, 0, sizeof(interaction_layer_delete_t));
    memset(&interaction_figure, 0, sizeof(interaction_figure_t));
    memset(&interaction_figure_2, 0, sizeof(interaction_figure_2_t));
    memset(&interaction_figure_3, 0, sizeof(interaction_figure_3_t));
    memset(&interaction_figure_2, 0, sizeof(interaction_figure_2_t));
    memset(&interaction_figure_4, 0, sizeof(interaction_figure_4_t));
    memset(&sentry_cmd, 0, sizeof(sentry_cmd_t));
    memset(&radar_cmd, 0, sizeof(radar_cmd_t));
    memset(&custom_robot_data, 0, sizeof(custom_robot_data_t));
    memset(&robot_custom_data,0,sizeof(robot_custom_data_t));
}

void referee_data_solve(uint8_t *frame)
{
    uint16_t cmd_id = 0;

    uint8_t index = 0;

    memcpy(&referee_receive_header, frame, sizeof(frame_header_struct_t));

    index += sizeof(frame_header_struct_t);

    memcpy(&cmd_id, frame + index, sizeof(uint16_t));
    index += sizeof(uint16_t);
    
    // 调试：记录命令ID和数据长度
    debug_cmd_id = cmd_id;
    debug_data_length = referee_receive_header.data_length;
    referee_last_update_time = xTaskGetTickCount();

    switch (cmd_id)
    {
    case GAME_STATE_CMD_ID:
    {
        if (copy_referee_payload(&game_state, sizeof(game_state), frame + index))
        {
            game_state_count++;
            game_state_last_update_time = referee_last_update_time;
        }
    }
    break;
    case GAME_RESULT_CMD_ID:
    {
        copy_referee_payload(&game_result, sizeof(game_result), frame + index);
    }
    break;
    case GAME_ROBOT_HP_CMD_ID:
    {
        copy_referee_payload(&game_robot_HP, sizeof(game_robot_HP), frame + index);
    }
    break;
    case FIELD_EVENTS_CMD_ID:
    {
        copy_referee_payload(&field_event, sizeof(field_event), frame + index);
    }
    break;
    case SUPPLY_PROJECTILE_ACTION_CMD_ID:
    {
        copy_referee_payload(&supply_projectile_action_t, sizeof(supply_projectile_action_t), frame + index);
    }
    break;
    case SUPPLY_PROJECTILE_BOOKING_CMD_ID:
    {
        copy_referee_payload(&supply_projectile_booking_t, sizeof(supply_projectile_booking_t), frame + index);
    }
    break;
    case REFEREE_WARNING_CMD_ID:
    {
        copy_referee_payload(&referee_warning, sizeof(referee_warning), frame + index);
    }
    break;

    case ROBOT_STATE_CMD_ID:
    {
        if (copy_referee_payload(&robot_state, sizeof(robot_state), frame + index))
        {
            robot_state_count++;
        }
    }
    break;
    case DART_INFO_CMD_ID:
    {
        if (copy_referee_payload(&dart_info, sizeof(dart_info), frame + index))
        {
            dart_info_count++;
            dart_info_last_update_time = referee_last_update_time;
        }
    }
    break;
    case POWER_HEAT_DATA_CMD_ID:
    {
        copy_referee_payload(&power_heat_data, sizeof(power_heat_data), frame + index);
    }
    break;
    case ROBOT_POS_CMD_ID:
    {
        copy_referee_payload(&game_robot_pos, sizeof(game_robot_pos), frame + index);
    }
    break;
    case BUFF_MUSK_CMD_ID:
    {
        copy_referee_payload(&buff, sizeof(buff), frame + index);
    }
    break;
    case AERIAL_ROBOT_ENERGY_CMD_ID:
    {
        copy_referee_payload(&robot_energy, sizeof(robot_energy), frame + index);
    }
    break;
    case ROBOT_HURT_CMD_ID:
    {
        copy_referee_payload(&robot_hurt, sizeof(robot_hurt), frame + index);
    }
    break;
    case SHOOT_DATA_CMD_ID:
    {
        copy_referee_payload(&shoot_data, sizeof(shoot_data), frame + index);
    }
    break;
    case BULLET_REMAINING_CMD_ID:
    {
        copy_referee_payload(&projectile_allowance, sizeof(projectile_allowance), frame + index);
    }
    break;
    case ROBOT_RFID_STATE:
    {
        copy_referee_payload(&robot_rfid, sizeof(robot_rfid), frame + index);
    }
    break;
    case DART_CLIENT_CMD_ID:
    {
        if (copy_referee_payload(&dart_client_cmd, sizeof(dart_client_cmd), frame + index))
        {
            dart_client_cmd_last_update_time = referee_last_update_time;
        }
    }
    break;
    case GROUND_ROBOT_POSITION_CMD_ID:
    {
        copy_referee_payload(&ground_robot_position, sizeof(ground_robot_position), frame + index);
    }
    break;
    case RADAR_MARK_DATA_CMD_ID:
    {
        copy_referee_payload(&radar_mark_data, sizeof(radar_mark_data), frame + index);
    }
    break;
    case SENTRY_INFO_CMD_ID:
    {
        copy_referee_payload(&SentryInfo, sizeof(SentryInfo), frame + index);
    }
    break;

    case RADAR_INFO_CMD_ID:
    {
        copy_referee_payload(&RadarInfo, sizeof(RadarInfo), frame + index);
    }
    break;
    case ROBOT_INTERACTION_DATA_CMD_ID:
    {
        copy_referee_payload_max(&robot_interaction_data, sizeof(robot_interaction_data), frame + index);
    }
		  break;
		case CUSTOM_ROBOT_DATA_ID:
		{
        copy_referee_payload(&custom_robot_data, sizeof(custom_robot_data), frame + index);
    }
		  break;
		

    default:
    {
        break;
    }
    }
}

