#include "bsp_tim.h"
#include "referee.h"

int8_t Vision_ID;
uint16_t frequent = 0;
SystemCtrl_t SystemData;
extern uint8_t add_YN;
extern float ChassisPower;
extern float ChassisPowerBuff;
extern float Chassis_PowerLimit;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

    if (htim->Instance == TIM2)
    {
			frequent++;
			SystemData.ChassisData = chassis_data();
			if(frequent % 2  == 0)
			{
//						CAN_cmd_cap_control(Chassis_PowerLimit,output_power_limit,input_power_limit,cap_measure.cap_control.all); 
			CAN_cmd_cap_control(cap_measure.state,robot_state.chassis_power_limit,power_heat_data.buffer_energy);
//			CAN_cmd_cap_powerbuff(ChassisPowerBuff);		
			
//				CAN_CmdBoard(power_heat_data.shooter_17mm_1_barrel_heat, 
//											robot_state.shooter_barrel_heat_limit,Vision_ID,
//											robot_state.power_management_shooter_output,
//											game_state.game_progress,
//											shoot_data.initial_speed);
				CAN_CmdBoard(shoot_data.initial_speed);
//				CAN_chassis_speed(SystemData.ChassisData->chassis_imu_date.chassis_yaw_speed);
		//	CAN_Cmd3508(SystemData.ChassisData->give_wheelCurrent[0], SystemData.ChassisData->give_wheelCurrent[1], SystemData.ChassisData->give_wheelCurrent[2], SystemData.ChassisData->give_wheelCurrent[3]);
			}
			if(frequent%2 == 1)
			{
	
			if(ChassisData.GetMotorWheel[0]->temperate <15.f||ChassisData.GetMotorWheel[1]->temperate <15.f||ChassisData.GetMotorWheel[2]->temperate <15.f||ChassisData.GetMotorWheel[3]->temperate <15.f)
			{
	    // ����ָֹͣ������̵��
    //CAN_Cmd3508(0, 0, 0,0);
				CAN_Cmd3508(ChassisData.give_wheelCurrent[0], ChassisData.give_wheelCurrent[1], ChassisData.give_wheelCurrent[2], ChassisData.give_wheelCurrent[3]);
			}
			else{
					CAN_Cmd3508(ChassisData.give_wheelCurrent[0], ChassisData.give_wheelCurrent[1], ChassisData.give_wheelCurrent[2], ChassisData.give_wheelCurrent[3]);
//					CAN_CmdBoard(power_heat_data.shooter_17mm_1_barrel_heat, Vision_ID, game_robot_HP.red_outpost_HP, game_robot_HP.blue_outpost_HP);
			}
			}
			if(frequent >= 10)
			{
				frequent = 0;
			}
			
    }
}

