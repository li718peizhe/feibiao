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
#define DART_RUN_MODE 1  // 0:测试模式 1:比赛模式
#define DART_REFEREE_TEST_BYPASS 0  // 1:调试时认为裁判系统比赛/闸门数据成立
#define RC_SW_UP1      2
#define RC_SW_MID1     1
#define RC_SW_DOWN1    0
#define MOTOR_LEFT    100
#define MOTOR_RIGHT   -100
#define switch_is_up1(state)  ((state) == RC_SW_UP1)
#define switch_is_down1(state) ((state) == RC_SW_DOWN1)
#define switch_is_mid1(state)  ((state) == RC_SW_MID1)

#define M3505_MOTOR_SPEED_PID_KP 4500.0f
#define M3505_MOTOR_SPEED_PID_KI 0.0f
#define M3505_MOTOR_SPEED_PID_KD 85.0f
#define M3505_MOTOR_SPEED_PID_MAX_OUT 16000.0f
#define M3505_MOTOR_SPEED_PID_MAX_IOUT 1500.0f

#define M3508_MOTOR_CURRENT_PID_KP 100.0f
#define M3508_MOTOR_CURRENT_PID_KI 0.0f
#define M3508_MOTOR_CURRENT_PID_KD 85.0f
#define M3508_MOTOR_CURRENT_PID_MAX_OUT 16000.0f
#define M3508_MOTOR_CURRENT_PID_MAX_IOUT 1500.0f






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

#define M2006_AUX_RC_CHANNEL 2
#define M2006_AUX_RC_DEADBAND 10
#define M2006_AUX_RC_TARGET_STEP 0.5f
#define M2006_AUX_POSITION_PID_KP 0.5f
#define M2006_AUX_POSITION_PID_KI 0.0f
#define M2006_AUX_POSITION_PID_KD 0.0f
#define M2006_AUX_POSITION_PID_MAX_OUT 3000.0f
#define M2006_AUX_POSITION_PID_MAX_IOUT 0.0f
#define M2006_AUX_SPEED_PID_KP 8.0f
#define M2006_AUX_SPEED_PID_KI 0.1f
#define M2006_AUX_SPEED_PID_KD 0.0f
#define M2006_AUX_SPEED_PID_MAX_OUT 5000.0f
#define M2006_AUX_SPEED_PID_MAX_IOUT 1000.0f
//飞镖模式

							#define GM3508_MOTOR_SPEED_PID_MAX_OUT 10000.0f
							#define GM3508_MOTOR_SPEED_PID_MAX_IOUT 2000.0f
   
							#define GM3508_MOTOR_CURRENT_PID_MAX_OUT 10000.0f
							#define GM3508_MOTOR_CURRENT_PID_MAX_IOUT 2000.0f
	 
typedef enum 
{
	DART_STANDBY=0,
	DART_PREPARE,
	DART_SHOOT,
	YAW_M2006_CHANGE,
	//YAW_M2006_CHANGE,
}dart_mode_e;

//飞镖发射状态
typedef enum 
{
	SHOOT_STOP=0,
	SHOOT_READY,
	SHOOT_STARE,
}shoot_state_e;
//飞镖发射次数

typedef enum 
{
	ZERO_SHOOT,
	FRIST_SHOOT,
	SECOND_SHOOT,
	THIRD_SHOOT,
  FORTH_SHOOT,
	REVERSE_SHOOT,
	
	FIT_2006_CHANGE,
}shoot_count_e;

//3508结构体
typedef struct
{
	const motor_measure_t *chassis_motor_measure;   //电机反馈数据指针
	fp32 speed_set;    //目标速度rpm
  int16_t give_current;//输出电流
	
	uint16_t ecd;
  int16_t speed_rpm;
  //int16_t given_current;
  uint8_t temperate;
  int16_t last_ecd;
  int32_t total_angle;
  int32_t round_cnt;
	int32_t current_set;
}G3508_motor_t;

//2006结构体
typedef struct
{
//	const motor_measure_t *get_trigger_motor_measure_point;
	const motor_measure_t *trigger_motor_measure;   //电机反馈数据指针
	fp32 speed_set;    //目标速度rpm
  int16_t give_current;//输出电流
	
	uint16_t ecd;
  int16_t speed_rpm;
  //int16_t given_current;
  uint8_t temperate;
  int16_t last_ecd;
  int32_t total_angle;
  int32_t round_cnt;
	int32_t current_set;
}G2006_motor_t;


typedef enum 
{
	DART_STATE_IDLE = 0,
	DART_STATE_DOWN,
	DART_STATE_DOWN_HOLD,
	DART_STATE_UP,
	DART_STATE_WAIT_GATE,
	DART_STATE_LAUNCH,
	DART_STATE_DOWN_AFTER_LAUNCH,
	DART_STATE_STEPPER_MOVE,
	DART_STATE_RELEASE_UP,
} dart_state_e;

typedef struct
{
	dart_mode_e dart_mode;
	shoot_state_e shoot_state;
	shoot_count_e shoot_count;
	
	G3508_motor_t G3508_motor[2];
	G2006_motor_t G2006_motor[2];
	
	pid_type_def motor3508_current_pid[2];
	pid_type_def motor3508_speed_pid[2];
	pid_type_def motor2006_speed_pid[2];
	pid_type_def motor2006_position_pid[2];
	
	const RC_ctrl_t *RcDate;
	
	dart_state_e dart_state;
	uint8_t current_dart;
	uint32_t state_timer;
}dart_date_t;



void shoot_task(void const *pvParameters);

void Shoot_Init(void);

extern void Shoot_PID_Init(void);

#endif
