#include "chassis_power_control.h"
#include "CAN_receive.h"
#include "remote_control.h"
#include "chassis_task.h"
#include "cap_control.h"
#include "referee.h"
#include "referee_lib.h"
extern float ChassisPower;
extern float ChassisPowerBuff;
extern float Chassis_PowerLimit;
 fp32 output_power_limit=300;
fp32 input_power_limit=150;
extern cap_measure_t cap_measure; //�������ݽṹ��
extern Rcdata_t RCData;
extern RC_ctrl_t rc_ctrl ;
extern 	uint16_t power_cap;

float int16_to_float(int16_t a, int16_t a_max, int16_t a_min, float b_max, float b_min)
{
 float b = (float)(a - a_min) / (float)(a_max - a_min) * (b_max - b_min) + b_min;
 return b;
}


void cap_state_init(void)
{
		get_chassis_power_and_buffer(&ChassisPower,&ChassisPowerBuff, &Chassis_PowerLimit);
		cap_measure.cap_control.bit.cap_record=1;
		cap_measure.cap_control.bit.cap_switch=0;
	  cap_measure.state=0;
				CAN_cmd_cap_control(cap_measure.state,robot_state.chassis_power_limit,power_heat_data.buffer_energy);

//		CAN_cmd_cap_control(Chassis_PowerLimit,output_power_limit,input_power_limit,cap_measure.cap_control.all);
//	  CAN_cmd_cap_powerbuff(ChassisPowerBuff);
}

void cap_feedback_update(void)
{
	fp32 a=cap_measure.voltage;
	fp32 b=cap_measure.current;
	cap_measure.cap_voltage=int16_to_float(a,32000,-32000,30,0);
	cap_measure.cap_current=int16_to_float(b,32000,-32000,20,-20);
	get_chassis_power_and_buffer(&ChassisPower,&ChassisPowerBuff, &Chassis_PowerLimit);

}
//int Cup_control;
int8_t LAST_CAP_KEYBOARD=0;
void cap_control_loop(void)
{
			 static int8_t last_s = RC_SW_UP;
				LAST_CAP_KEYBOARD = RCData.key.v;
	
		if(RCData.key.v&KEY_PRESSED_OFFSET_F)
		{
			power_cap= 200;
		}
else 
{
	power_cap=30;
}

		if (((RCData.key.v & CAP_OPEN_KEY) && (cap_measure.state==0)))
	{
		cap_measure.cap_control.bit.cap_switch=1;
		cap_measure.state  = 1;
//		CAN_cmd_cap_control(Chassis_PowerLimit,output_power_limit,input_power_limit,cap_measure.cap_control.all);
//		CAN_cmd_cap_powerbuff(ChassisPowerBuff);
	}
	else if (((RCData.key.v & CAP_OFF_KEY) && (cap_measure.state==1))||robot_state.power_management_chassis_output==0)
	{
		cap_measure.cap_control.bit.cap_switch=0;
		cap_measure.state = 0;
//	  CAN_cmd_cap_control(Chassis_PowerLimit,output_power_limit,input_power_limit,cap_measure.cap_control.all);
//    CAN_cmd_cap_powerbuff(ChassisPowerBuff);

	}
			last_s=RCData.rc.s[1];

}


