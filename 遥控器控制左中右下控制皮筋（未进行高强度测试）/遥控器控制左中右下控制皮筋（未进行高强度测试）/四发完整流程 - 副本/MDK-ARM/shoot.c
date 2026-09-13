#include "shoot.h"
#include <stdlib.h>  // 包含标准库头文件，提供abs()函数声明
#include "calibrate_task.h"
#include "string.h"
#include "cmsis_os.h"

#include "bsp_buzzer.h"
#include "bsp_flash.h"
//#include "chassis_behaviour.h"
#include "cmsis_os.h"
#include "arm_math.h"

#include "can_receive.h"
#include "remote_control.h"
#include "INS_task.h"

#include "cmsis_os.h"
#include "arm_math.h"
#include "shoot.h"
#include "bsp_can.h"
// 定义限幅宏
#define rc_deadband_limit(input, output, dealine)    \
  {                                                  \
    if ((input) > (dealine) || (input) < -(dealine)) \
    {                                                \
      (output) = (input);                            \
    }                                                \
    else                                             \
    {                                                \
      (output) = 0;                                  \
    }                                                \
  }
int dart_pop_up_flat=0;	
int dart_launch_flat=0;	

// 电机PID控制器定义（新增扳机位置PID）
pid_type_def trigger_motor_speed_pid;
pid_type_def magazine_motor_speed_pid;
pid_type_def Fire_motor_pid[6];
pid_type_def trigger_motor_pos_pid;  // 扳机位置环PID

/************                        ************/
dart_date_t Dart_Data;
/*
 *飞镖系统初始化
 *
 */
void shoot_init(void)
{
	//初始化飞镖模式为飞镖待机模式
	Dart_Data .dart_mode =DART_STANDBY  ;
	//初始遥控器数据
	Dart_Data .RcDate =get_remote_control_point();
	//电机PID数据初始化
	const static fp32 motor3508_speed_pid[3] = {M3505_MOTOR_SPEED_PID_KP, M3505_MOTOR_SPEED_PID_KI, M3505_MOTOR_SPEED_PID_KD};
	const static fp32 motor2006_speed_pid[3] = {M2006_MOTOR_SPEED_PID_KP, M2006_MOTOR_SPEED_PID_KI, M2006_MOTOR_SPEED_PID_KD};
	const static fp32 motor2006_position_pid[3] = {M2006_MOTOR_POSITION_PID_KP, M2006_MOTOR_POSITION_PID_KI, M2006_MOTOR_POSITION_PID_KD};

	//电机PID初始化
	for (int i = 0; i < 2; i++)
  {
    Dart_Data .G3508_motor[i] .chassis_motor_measure   = get_chassis_motor_measure_point(i);
    PID_init(&Dart_Data .motor3508_speed_pid[i], PID_POSITION, motor3508_speed_pid, M3505_MOTOR_SPEED_PID_MAX_OUT, M3505_MOTOR_SPEED_PID_MAX_IOUT);
  }
	for (int i = 0; i < 2; i++)
  {
    Dart_Data .G2006_motor[i] .chassis_motor_measure   = get_chassis_motor_measure_point(i);
    PID_init(&Dart_Data .motor2006_speed_pid[i], PID_POSITION, motor2006_speed_pid, M3505_MOTOR_SPEED_PID_MAX_OUT, M3505_MOTOR_SPEED_PID_MAX_IOUT);
    PID_init(&Dart_Data .motor2006_position_pid[i], PID_POSITION, motor2006_position_pid, M2006_MOTOR_POSITION_PID_MAX_OUT, M2006_MOTOR_POSITION_PID_MAX_IOUT);
	}
}
/*
 *执行G3508电机运动功能
 *通过速度环PID控制G3508电机实现
 */
void G3508_Pid_move(int diretion)
{
	if(diretion == 1/*表示向上移动*/)
	{
		Dart_Data .G3508_motor [0].speed_set=4;
		Dart_Data .G3508_motor [1].speed_set=4;
	}
	if(diretion == -1/*表示向下移动*/)
	{
		Dart_Data .G3508_motor [0].speed_set=-4;
		Dart_Data .G3508_motor [1].speed_set=-4;
	}
	for (uint8_t i = 0; i < 2; i++)
  {
  	PID_calc (&Dart_Data .motor3508_speed_pid [i],Dart_Data .G3508_motor->chassis_motor_measure->speed_rpm ,Dart_Data .G3508_motor [i].speed_set);
	}
	for (uint8_t i = 0; i < 2; i++)
	{		
		Dart_Data .G3508_motor [i].give_current =(int16_t)(Dart_Data .motor3508_speed_pid [i].out );
	}
	CAN_cmd_chassis(Dart_Data .G3508_motor [0].give_current, Dart_Data .G3508_motor [1].give_current, 0, 0);
}

void G3508_Position_move(void)
{
	
}
/*
 *执行调整射程功能
 *通过2006电机实现
 */
void range_adjustment(void)
{
	
}
/*
 *执行换弹功能
 *通过步进电机实现
 */
void reload(void)
{
	
}

/*
 *执行飞镖遥控器控制发射
 *
 */
void shoot_rc(void)
{
	//左拨杆（下）右拨杆（下）->飞镖待机状态
	if(switch_is_down(Dart_Data .RcDate ->rc .s [1])&&switch_is_down(Dart_Data .RcDate ->rc .s [1]))
	{
		Dart_Data .dart_mode =DART_STANDBY ;
	}
	//左拨杆（下）右拨杆（中）&&飞镖待机状态 ->第一次发射
	if(switch_is_down(Dart_Data .RcDate ->rc .s [1])&&switch_is_mid(Dart_Data .RcDate ->rc .s [1]&&Dart_Data .dart_mode ==DART_STANDBY))
	{
		Dart_Data .shoot_count =FRIST_SHOOT  ;
	}
	//检测飞镖成功上弹 ->飞镖准备状态
	if(dart_pop_up_flat ==1)
	{
		Dart_Data .dart_mode =DART_PREPARE ;
	}
	//左拨杆（下）右拨杆（上）&&飞镖待机状态 &&上次（左拨杆（下）右拨杆（中））->飞镖发射状态 	
	if(switch_is_down(Dart_Data .RcDate ->rc .s [1])&&switch_is_up(Dart_Data .RcDate ->rc .s [1]&&Dart_Data .dart_mode ==DART_PREPARE))
	{
		Dart_Data .shoot_count =FRIST_SHOOT  ;
	}
	//飞镖发射标志->飞镖待机状态
	if(dart_launch_flat  ==1)
	{
		Dart_Data .dart_mode =DART_STANDBY ;
	}
	/******************/
	//左拨杆（下）右拨杆（中）&&飞镖待机状态 &&上次（左拨杆（下）右拨杆（中））->第二，三，四次发射
	if(switch_is_down(Dart_Data .RcDate ->rc .s [1])&&switch_is_mid(Dart_Data .RcDate ->rc .s [1]))
	{
		Dart_Data .shoot_count =FRIST_SHOOT  ;
	}
	//检测飞镖成功上弹 ->飞镖准备状态
	if(dart_pop_up_flat ==1)
	{
		Dart_Data .dart_mode =DART_PREPARE ;
	}
	//左拨杆（下）右拨杆（上）&&飞镖待机状态 &&上次（左拨杆（下）右拨杆（中））->飞镖发射状态 	
	if(switch_is_down(Dart_Data .RcDate ->rc .s [1])&&switch_is_mid(Dart_Data .RcDate ->rc .s [1]))
	{
		Dart_Data .shoot_count =FRIST_SHOOT  ;
	}
	//飞镖发射标志->飞镖待机状态
	if(dart_launch_flat  ==1)
	{
		Dart_Data .dart_mode =DART_STANDBY ;
	}
}

/*
 *执行飞镖发射任务
 *
 */
void shoot()
{
	//1.第一次发射
	if(Dart_Data .shoot_count == FRIST_SHOOT )
	{
		//3508向下转	
		G3508_Pid_move(-1);
		//舵机控制开关
		
		//碰撞停止（等待）
		//通过检测电机电流判断电机是否过载
		if(Dart_Data .G3508_motor [0].chassis_motor_measure ->given_current  >= 2500||Dart_Data .G3508_motor [1].chassis_motor_measure ->given_current >= 2500 )
		{
			//等待（确保飞镖成功上弹）
			
			//停止3508电机
			CAN_cmd_chassis (0,0,0,0);
			//飞镖成功上弹标志
			dart_pop_up_flat=1;
		}
		//3508向上转
		G3508_Pid_move(1);
		//碰撞停止（等待）
		if(Dart_Data .G3508_motor [0].chassis_motor_measure ->given_current  >= 2500||Dart_Data .G3508_motor [1].chassis_motor_measure ->given_current >= 2500 )
		{
			//停止3508电机
			CAN_cmd_chassis (0,0,0,0);
			//飞镖准备状态
			
			//舵机控制开关
			
			//飞镖发射标志
			dart_launch_flat=1;
		}
	}
	
	//2.第二次发射（与三，四次发射相同）
	if(Dart_Data .shoot_count ==SECOND_SHOOT||Dart_Data .shoot_count ==THIRD_SHOOT||Dart_Data .shoot_count ==FORTH_SHOOT )
	{
		//3508向下转
		G3508_Pid_move(-1);		
		//碰撞停止（等待）
		//通过检测电机电流判断电机是否过载
		if(Dart_Data .G3508_motor [0].chassis_motor_measure ->given_current  >= 2500||Dart_Data .G3508_motor [1].chassis_motor_measure ->given_current >= 2500 )
		{
			//停止3508电机
			CAN_cmd_chassis (0,0,0,0);
		}
		
		//换弹
		reload();
		//3508向上移动固定距离
		G3508_Position_move();
		//3508向下转
		G3508_Pid_move(-1);
		//舵机控制开关
		
		//碰撞停止（等待）
		if(Dart_Data .G3508_motor [0].chassis_motor_measure ->given_current  >= 2500||Dart_Data .G3508_motor [1].chassis_motor_measure ->given_current >= 2500 )
		{
			HAL_Delay (1000);
			//停止3508电机
			CAN_cmd_chassis (0,0,0,0);
			//飞镖成功上弹标志
			dart_pop_up_flat=1;
		}
		//3508向上转
		G3508_Pid_move(1);
		//碰撞停止（等待）
		if(Dart_Data .G3508_motor [0].chassis_motor_measure ->given_current  >= 2500||Dart_Data .G3508_motor [1].chassis_motor_measure ->given_current >= 2500 )
		{
			//停止3508电机
			CAN_cmd_chassis (0,0,0,0);	
			//舵机控制开关	
		
			//飞镖发射标志
			dart_launch_flat=1;
		}
	}
}
/*
 *执行飞镖系统任务
 *
 */
void shoot_task(void const *pvParameters)
{
	vTaskDelay(100);
	shoot_init ();
	while(1)
	{
		shoot_rc();
		shoot();
		
	}
}

//// 辅助函数：目标值范围限制
//static void target_deadband_limit1(int32_t *target, int32_t max_val, int32_t min_val)
//{
//    if (*target > max_val) *target = max_val;
//    else if (*target < min_val) *target = min_val;
//}

