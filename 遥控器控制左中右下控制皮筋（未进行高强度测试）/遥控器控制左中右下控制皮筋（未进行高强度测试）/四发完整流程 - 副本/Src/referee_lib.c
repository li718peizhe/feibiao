#include "referee.h"
#include "referee_lib.h"
void get_chassis_power_and_buffer(fp32 *power, fp32 *buffer, fp32 *powermax)
{
    *power = power_heat_data.chassis_power;//¦Ì¡Á?¨¬¨º?3?1|?¨º
    *buffer = power_heat_data.buffer_energy;//¦Ì¡Á?¨¬?o3?1|?¨º
		*powermax = robot_state.chassis_power_limit;//¦Ì¡Á?¨¬1|?¨º?T??

}


uint8_t get_robot_id(void)
{
	return robot_state.robot_id;
}
uint8_t get_robot_level(void)
{
	return robot_state.robot_level;
}
uint16_t get_robot_remain_HP(void)
{
	return robot_state.current_HP;
}

uint16_t get_robot_max_HP(void)
{
	return robot_state.maximum_HP;
}

//uint16_t ID1_cooling_rate(void)
//{
//	return robot_state_YU.shooter_id1_17mm_barrel_cooling_value;
//}c
//uint16_t ID1_cooling_limit(void)
//{
//	return robot_state_YU.shooter_id1_17mm_barrel_heat_limit;
//}

uint16_t ID1CoolingHeat(void)
{
	return power_heat_data.shooter_17mm_1_barrel_heat;
}

uint16_t ID2_cooling_rate(void)
{
	return power_heat_data.shooter_17mm_2_barrel_heat;
}

int MyColour(void)
{
	if(get_robot_id() < 10)
		return 0;//ºì
	else
		return 1;//À¶
}







