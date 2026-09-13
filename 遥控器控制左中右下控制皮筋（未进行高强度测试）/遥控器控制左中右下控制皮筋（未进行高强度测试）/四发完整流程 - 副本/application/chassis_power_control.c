#include "chassis_power_control.h"
#include "referee.h"
#include "referee_lib.h"
#include "arm_math.h"
#include "pid.h"
#include "Filter.h"
#include "remote_control.h"
#include "cap_control.h"
#include "CAN_receive.h"
#include "chassis_task.h"
float ChassisPower = 0.0f;
float ChassisPowerBuff = 0.0f;
float Chassis_PowerLimit = 0.0f;
fp32 WheelSet[4];
extern int Change_power;
float Lowpass_set[4];
#define K_Low_setchassis  0.05   //0.05   截止频率f=1000a/2pi=160a
ChassisPowerControl_t ChassisPowerLMT;
pid_type_def Power_Limit;
gimbal_PID_t feedforward[4];
pid_type_def givecurrent_limt[4];
extern cap_measure_t cap_measure;
float Power_Limit_PID[3] = {1.5, 0.0, 0.0};
extern pid_type_def GM3508_Speed[4];
fp32 temp1;
fp32 temp2;
fp32 power_set;
fp32      chassis_max_power = 0;
float     input_power = 0;
float WhichPower = 0.;
float SHUO;
float eee;
float yyy;
float speed[4];
float ddd;
extern cap_data_t cap_data;

//uint32_t Power_UI=15;
	fp32 A=0;
 fp32 wheel_speed_yu[4]; 
	float FSHh,fsh1,mmm;
fp32 wheel_feedward[4];
fp32 wheel_feedward2[4];
fp32 wheel_fsh1;
fp32 wheel_fsh2;
fp32 wheel_fsh3;
fp32 wheel_fsh4;
#define M3508_MOTOR_RPM_TO_VECTOR 0.000415809748903494517209f


  float    	initial_give_power[4];
	uint16_t power_cap;

void Chassis_PowerLimit_T(void)
{					
	float powerWeight_Error[4];
			fp32 sumError = 0.0f;
    fp32 			scaled_give_power[4];
    float   	initial_total_power = 0;
						float error[4];
	fp32 power_scale[4];

    //Pin=Pm+k1w*w+k2*out*out+a  功率输出公式
    fp32 toque_coefficient = 1.99688994e-6f; // (20/16384)*(0.3)*(187/3591)/9.55  Pm=toque_coefficient*W 转速单位RPM
    fp32 a = 1.23e-07;						 // k2
    fp32 k1 = 1.453e-07;					 // k1
    fp32 constant = 4.081f;


    PID_init(&Power_Limit, PID_POSITION, Power_Limit_PID, 10000, 0);

    ChassisPowerLMT.ChassisData_L  = GetChassisData();
    for (uint8_t i = 0; i < 4; i++)
    {
        ChassisPowerLMT.ChassisData_L->GetMotorWheel[i] = get_chassis_Wheelmotor_point(i);
    }
		chassis_vector_to_mecanum_wheel_speed(ChassisData.VxSet, ChassisData.Vyset, ChassisData.Wzset,  wheel_speed_yu);
			
    get_chassis_power_and_buffer(&ChassisPower,&ChassisPowerBuff, &Chassis_PowerLimit);
		
				   for (uint8_t i = 0; i < 4; i++)
    {
			gimbal_PID_init(&feedforward[i],16000,1000,25,0,10,0);
		}
		   for (uint8_t i = 0; i < 4; i++)
    {
//			gimbal_PID_calc(&feedforward[i],MotorWheel[i].speed_rpm, wheel_speed_yu[i],A);
       PID_calc(&GM3508_Speed[i], MotorWheel[i].speed_rpm, wheel_speed_yu[i]);
							sumError+=fabs(GM3508_Speed[i].error[0]);
    }
//	

    //缓冲能量利用
    PID_calc(&Power_Limit, ChassisPowerBuff, 50);

    input_power = Chassis_PowerLimit - Power_Limit.out-cap_measure.power_limit;

//	if(cap_measure.cap_voltage <11)
//	{
//		cap_measure.power_cap=0;
//		output_power_limit=0;
//	}
//	else
//	{
//		cap_measure.power_cap=150	;
//		output_power_limit=300;
//	}
//	if(robot_state.robot_level==1&&ChassisPowerLMT.RCData->key.v&KEY_PRESSED_OFFSET_CTRL)
//	{
//		power_cap= 30;
//	}
//	else if(robot_state.robot_level==2)
//	{
//		power_cap= 50;
//	}
//	else if(robot_state.robot_level==3)
//	{
//		power_cap= 70;
//	}
//	else if(robot_state.robot_level==4)
//	{
//		power_cap= 80;
//	}
//	else if(robot_state.robot_level==5)
//	{
//		power_cap= 90;
//	}
//		else if(robot_state.robot_level==6)
//	{
//		power_cap= 100;
//	}
//	else if(robot_state.robot_level==7)
//	{
//		power_cap= 130;
//	}
//	else if(robot_state.robot_level==8)
//	{
//		power_cap= 150;
//	}
//	else if(robot_state.robot_level==9)
//	{
//		power_cap= 170;
//	}
//	else if(robot_state.robot_level==10)
//	{
//		power_cap= 200;
//	}
	if (cap_data.capEnergy >=0.25)//最低工作电压
	{
		 if (cap_measure.state == 0)
		  {
			chassis_max_power = input_power + 5; 	//略大于最大功率，避免电容一直充满，提高能量利用率
	  	}
	    else
	   	{
		  chassis_max_power = input_power + power_cap ;//合适的电容伪缓冲能量		
			}
	}
	else
		{
			
          chassis_max_power = input_power + 5;
    
		}

    for (uint8_t i = 0; i < 4; i++)
    {
        initial_give_power[i] = (GM3508_Speed[i].out+wheel_feedward[i]+wheel_feedward2[i]) * toque_coefficient * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm +
                                k1 * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm +
                                a * (GM3508_Speed[i].out+wheel_feedward[i]+wheel_feedward2[i]) * (GM3508_Speed[i].out+wheel_feedward[i]+wheel_feedward2[i]+wheel_feedward[i]+wheel_feedward2[i]) + constant;
        if (initial_give_power < 0) // negative power not included (transitory)不含负电（暂态）
            continue;
        initial_total_power += initial_give_power[i];
    }
eee=sumError;
    //最大功率如何确定的问题，在不添加超级电容的情况下确定最大功率限制的逻辑
    if (initial_total_power > chassis_max_power) // determine if larger than max power确定是否大于最大功率
    {
			 float errorConfidence;

		if(sumError>error_powerDistribution_set)
		{
			  errorConfidence = 6.0f;
		}
		else if(sumError>prop_powerDistribution_set)
		{
			errorConfidence=(sumError-prop_powerDistribution_set)/(error_powerDistribution_set-prop_powerDistribution_set);
		}
		else
		{
			errorConfidence=0.0f;
		}
			
		yyy=errorConfidence;
        for (uint8_t i = 0; i < 4; i++)
        {		
					 powerWeight_Error[i]=fabs(fabs(GM3508_Speed[i].error[0])/sumError);
					 fp32 power =  chassis_max_power / initial_total_power;
            scaled_give_power[i]=power*initial_give_power[i];
					 power_scale[i] = errorConfidence * powerWeight_Error[i]*chassis_max_power + (1.0f - errorConfidence) * scaled_give_power[i];
					
            scaled_give_power[i] = initial_give_power[i] * power; // get scaled power
            if (scaled_give_power[i] < 0)
            {
                continue;
            }
            WhichPower = scaled_give_power[0];

            fp32 b = toque_coefficient * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm ;//Pm
            fp32 c = k1 * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm  * ChassisPowerLMT.ChassisData_L->GetMotorWheel[i]->speed_rpm  - power_scale[i] + constant;
            if (GM3508_Speed[i].out+wheel_feedward[i]+wheel_feedward2[i] > 0) // Selection of the calculation formula according to the direction of the original moment
                //根据原弯矩方向选择计算公式
            {
                temp1 = (-b + sqrt(b * b - 4 * a * c)) / (2 * a);//得到力矩电流
                //广工算出的值为缩放比例系数K
                if (temp1 > 16000)//发送力矩电流控制值比较
                {
                    GM3508_Speed[i].out = 16000;
                }
                else
                    GM3508_Speed[i].out = temp1;
            }
            else
            {
                temp2 = (-b - sqrt(b * b - 4 * a * c)) / (2 * a);
                if (temp2 < -16000)
                {
                    GM3508_Speed[i].out = -16000;
                }
                else
                    GM3508_Speed[i].out = temp2;
            }
        }
    }

//    for (uint8_t i = 0; i < 4; i++)
//    {

  ChassisData.give_wheelCurrent[0] = (int16_t)( GM3508_Speed[0].out)+wheel_fsh1;
			  ChassisData.give_wheelCurrent[1] = (int16_t)( GM3508_Speed[1].out)+wheel_fsh2;
  ChassisData.give_wheelCurrent[2] = (int16_t)( GM3508_Speed[2].out)+wheel_fsh3;
  ChassisData.give_wheelCurrent[3] = (int16_t)( GM3508_Speed[3].out)+wheel_fsh4;

//    }

}
void LowPass_SetChassis(float* output,float In)
{
    *output=(1-K_Low_setchassis)*(*output)+K_Low_setchassis*In;   
}

