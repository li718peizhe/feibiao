#ifndef SHOOT_H
#define SHOOT_H

#include <stdint.h>

#include "struct_typedef.h"
#include "bsp_can.h"
#include "remote_control.h"
#include "main.h"
#include "bsp_can.h"
#include "pid.h"
#include "FreeRTOS.h"
#include "task.h"
//#include "chassis_behaviour.h"
#include "CAN_receive.h"

#include "struct_typedef.h"
#include "bsp_rc.h"

#define FRICTION_STOP 0
#define FRICTION_RUN  1
#define RC_SW_UP1      2
#define RC_SW_MID1     1
#define RC_SW_DOWN1    0
#define MOTOR_LEFT    100
#define MOTOR_RIGHT   -100
#define switch_is_up1(state)  ((state) == RC_SW_UP1)
#define switch_is_down1(state) ((state) == RC_SW_DOWN1)
#define switch_is_mid1(state)  ((state) == RC_SW_MID1)

#define M3508_MOTOR_CURRENT_PID_KP 15.0f
#define M3508_MOTOR_CURRENT_PID_KI 0.0f
#define M3508_MOTOR_CURRENT_PID_KD 0.3f
#define M3508_MOTOR_CURRENT_PID_MAX_OUT 16000.0f
#define M3508_MOTOR_CURRENT_PID_MAX_IOUT 1500.0f

#define M3505_MOTOR_SPEED_PID_KP 3500.0f
#define M3505_MOTOR_SPEED_PID_KI 0.0f
#define M3505_MOTOR_SPEED_PID_KD 85.0f
#define M3505_MOTOR_SPEED_PID_MAX_OUT 16000.0f
#define M3505_MOTOR_SPEED_PID_MAX_IOUT 1500.0f

#define M2006_MOTOR_SPEED_PID_KP 3500.0f
#define M2006_MOTOR_SPEED_PID_KI 0.0f
#define M2006_MOTOR_SPEED_PID_KD 85.0f
#define M2006_MOTOR_SPEED_PID_MAX_OUT 16000.0f
#define M2006_MOTOR_SPEED_PID_MAX_IOUT 1500.0f

#define M2006_MOTOR_POSITION_PID_KP 3500.0f
#define M2006_MOTOR_POSITION_PID_KI 0.0f
#define M2006_MOTOR_POSITION_PID_KD 85.0f
#define M2006_MOTOR_POSITION_PID_MAX_OUT 16000.0f
#define M2006_MOTOR_POSITION_PID_MAX_IOUT 1500.0f
//����ģʽ
typedef enum 
{
	DART_STANDBY=0,
	DART_PREPARE,
	DART_SHOOT,
	YAW_M2006_CHANGE,
}dart_mode_e;

//���ڷ���״̬
typedef enum 
{
	SHOOT_STOP=0,
	SHOOT_READY,
	SHOOT_STARE,
}shoot_state_e;
//���ڷ������

typedef enum 
{
	FRIST_SHOOT=0,
	SECOND_SHOOT,
	THIRD_SHOOT,
  FORTH_SHOOT,
}shoot_count_e;

typedef struct
{
	const motor_measure_t *chassis_motor_measure;
	fp32 speed_set;
  int16_t give_current;
}G3508_motor_t;

typedef struct
{
	const motor_measure_t *chassis_motor_measure;
	const motor_measure_t *trigger_motor_measure;
	
}G2006_motor_t;

//typedef struct
//{
//	fp32 chassis_yaw;
//  fp32 chassis_pitch;
//  fp32 chassis_roll;
//	fp32 chassis_yaw_speed;
//}chassis_imu_date_t;

typedef struct
{
	dart_mode_e dart_mode;
	shoot_state_e shoot_state;
	shoot_count_e shoot_count;
	
	G3508_motor_t G3508_motor[2];
	G2006_motor_t G2006_motor[2];
	
	pid_type_def motor3508_speed_pid[4];
	pid_type_def motor2006_speed_pid[4];
	pid_type_def motor2006_position_pid[4];
	
	const RC_ctrl_t *RcDate;

//	chassis_imu_date_t chassis_imu_date;
}dart_date_t;

void shoot_task(void const *pvParameters);

void Shoot_Init(void);

extern void Shoot_PID_Init(void);

#endif

