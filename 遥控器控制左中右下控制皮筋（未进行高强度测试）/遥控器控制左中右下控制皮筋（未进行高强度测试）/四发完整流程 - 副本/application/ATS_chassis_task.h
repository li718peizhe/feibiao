#ifndef ATS_CHASSIS_TASK
#define ATS_CHASSIS_TASK
#include "remote_control.h"
#include "CAN_receive.h"
#include "user_lib.h"
#include "DJI_motor.h"
#include "main.h"
#include "cmsis_os.h"
#include "INS_task.h"
#include "pid.h"
#include "arm_math.h"




#define RC_left 1
#define RC_right 0
#define CHASSIS_X_CHANNEL 1
#define CHASSIS_Y_CHANNEL 0
#define CHASSIS_WZ_CHANNEL 2
#define CHASSIS_RC_DEADLINE 10
#define CHASSIS_CONTROL_TIME 0.002f
/*遥控器值缩放*/
#define CHASSIS_RC_SEN 0.02f
#define CHASSIS_FRONT_KEY KEY_PRESSED_OFFSET_W
#define CHASSIS_BACK_KEY KEY_PRESSED_OFFSET_S
#define CHASSIS_LEFT_KEY KEY_PRESSED_OFFSET_A
#define CHASSIS_RIGHT_KEY KEY_PRESSED_OFFSET_D

#define KEYBOARD_TOP_SPEED_MAX 15.0f
#define KEYBOARD_SPEED_MAX 0.2f

/*遥控器前进摇杆（max 660）转化成车体前进速度（m/s）的比例*/ 
 #define CHASSIS_VX_RC_SEN 0.01f
/*遥控器左右摇杆（max 660）转化成车体左右速度（m/s）的比例*/ 
#define CHASSIS_VY_RC_SEN 0.003f
#define DEGREE_2_RAD 0.01745329252f // pi/180
/***************************************全向轮底盘解算参数START*********************************************/
/* 机器人底盘修改的参数,单位为mm(毫米)*/
#define WHEEL_BASE 600.509f          		 // 纵向轴距(前进后退方向)
#define TRACK_WIDTH 600.509f              // 横向轮距(左右平移方向)
#define CENTER_GIMBAL_OFFSET_X 0.0f    							 // 云台旋转中心距底盘几何中心的距离,前后方向,云台位于正中心时默认设为0
#define CENTER_GIMBAL_OFFSET_Y 0   								 // 云台旋转中心距底盘几何中心的距离,左右方向,云台位于正中心时默认设为0
#define RADIUS_WHEEL 76.2f         						     // 轮子半径
#define REDUCTION_RATIO_WHEEL 14.0f 							 // 电机减速比,因为编码器量测的是转子的速度而不是输出轴的速度故需进行转换
/* 根据robot_def.h中的macro自动计算的参数 */
#define HALF_WHEEL_BASE (WHEEL_BASE / 2.0f)     	 // 半轴距
#define HALF_TRACK_WIDTH (TRACK_WIDTH / 2.0f)  		 // 半轮距
#define PERIMETER_WHEEL (RADIUS_WHEEL * 2 * PI)		 // 轮子周长
#define M3508_RATIO 14                   // 电机减速比

#define LF_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RF_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define LB_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define RB_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
/***************************************END***********************************************************************/
#define M3508_MOTOR_SPEED_PID_KP 10.0f  //5.0f
#define M3508_MOTOR_SPEED_PID_KI 0.0f
#define M3508_MOTOR_SPEED_PID_KD 1.0f
#define GM3508_MOTOR_SPEED_PID_MAX_OUT 10000.0f
#define GM3508_MOTOR_SPEED_PID_MAX_IOUT 2000.0f

#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 1.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 0.0f

void ats_chassis_task(void const *pvParameters);

typedef struct
{
	const RC_ctrl_t*chassis_RC;
	const Receive_board_data_t*chassis_board;
	const fp32*chassis_INS_angle;
	const fp32*chassis_INS_angle_speed;
	const fp32*accel_fliter;
	const motor_measure_t *GetMotorWheel[4];
	fp32 ChassisFollowGimbalAngle;
	 int16_t give_wheelCurrent[4];
	uint16_t mode;
	fp32 x_set;
	fp32 y_set;
	fp32 vx_target;
	fp32 vy_target;
	fp32 Vx_set;
	fp32 Vy_set;
	fp32 Wz_set;
	fp32 ats_chassis_angle_set;
}chassis_data_t;

typedef enum
{
		ATS_CHASSIS_STOP=0,
		ATS_CHASSIS_FOLLOW_GIMBAL_YAW,
		ATS_CHASSIS_TOP,
}chassis_mode_t;
extern void ats_chassis_init(chassis_data_t *chassis_init);
extern void mode_set_control(chassis_data_t*chassis_mode_set);
extern void ats_chassis_updata(chassis_data_t*chassis_updata);
extern void chassis_stop(chassis_data_t*chassis_stop);
extern void chassis_control(chassis_data_t*control_data);
 extern void chassis_rc_key();
extern chassis_data_t *GetChassisdata(void);
extern chassis_data_t chassis_DATA;
extern void chassis_vector_to_mecanum_wheel_speed(const fp32 vx_set, const fp32 vy_set, const fp32 wz_set, fp32 wheel_speed[4]);
extern void ChassisTop(void);

#endif
