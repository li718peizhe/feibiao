#ifndef __CHASSIS_TASK_H
#define __CHASSIS_TASK_H

#include "main.h"
#include "struct_typedef.h"
#include "cmsis_os.h"
#include "CAN_receive.h"
#include "remote_control.h"
#include "arm_math.h"
#include "user_lib.h"

/***************************************新老车*********************************************/
#define NEW_CHASSIS 0
/***************************************新老车*********************************************/
/***************************************哨兵*********************************************/
#define AUTO_MODE 1



/***************************************哨兵*********************************************/
extern uint32_t  Cup_control;
extern uint32_t  frictiongear;
/*底盘任务控制间隔 0.002s*/
#define CHASSIS_CONTROL_TIME 0.002f
/***************************************算法参数START*********************************************/
/*斜坡函数*/
#define CHASSIS_ACCEL_X_NUM 0.3333333333f
#define CHASSIS_ACCEL_Y_NUM 0.3333333333f 
#define CHASSIS_ACCEL_W_NUM 0.3333333333f 
/***************************************END**********************************************************/

#define CHASSIS_CONTROL_TIME 0.002f
/*遥控器值缩放*/
#define CHASSIS_RC_SEN 0.02f
/*遥控器前进摇杆（max 660）转化成车体前进速度（m/s）的比例*/ 
 #define CHASSIS_VX_RC_SEN 0.01f
/*遥控器左右摇杆（max 660）转化成车体左右速度（m/s）的比例*/ 
#define CHASSIS_VY_RC_SEN 0.003f
/*遥控器死区*/
#define CHASSIS_RC_DEADLINE 10
/*遥控器通道控制世界坐标系下X、Y轴运动*/
#define CHASSIS_X_CHANNEL 1
#define CHASSIS_Y_CHANNEL 0
#define CHASSIS_WZ_CHANNEL 2

/*车辆底盘信息*/
#define Motor_Ecd_to_Rad 0.000766990394f // 2 * PI / 8192
#define WHEEL_PERIMETER 483.856f          // 车轮周长 (轮子直径 * PI 再转换成mm)
#define M3508_RATIO 10                   // 电机减速比
#define Radius 60                        // 轮径mm




#define GM3508_MOTOR_SPEED_PID_MAX_OUT 10000.0f
#define GM3508_MOTOR_SPEED_PID_MAX_IOUT 2000.0f



#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT 50.0f
#define CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT 0.0f
/*键盘控制时最大速度*/ 
#define KEYBOARD_TOP_SPEED_MAX 30.0f
//#define KEYBOARD_FLLOW_GIMBAL_SPEED_MAX 30.0f

#define KEYBOARD_SPEED_MAX 0.2f
#define KEYBOARD_SPEED_MIN 1.0f
/*W,A,S,D,Z,Q*/ 
#define CHASSIS_FRONT_KEY KEY_PRESSED_OFFSET_W
#define CHASSIS_BACK_KEY KEY_PRESSED_OFFSET_S
#define CHASSIS_LEFT_KEY KEY_PRESSED_OFFSET_A
#define CHASSIS_RIGHT_KEY KEY_PRESSED_OFFSET_D
#define TURN_HEARD KEY_PRESSED_OFFSET_C//掉头
#define CHASSIS_GIMBAL KEY_PRESSED_OFFSET_Q//底盘跟随云台
#define CLIMBING KEY_PRESSED_OFFSET_CTRL	
/*键盘控制时最大速度*/ 
//m3508转化成底盘速度(m/s)的比例，
#define M3508_MOTOR_RPM_TO_VECTOR 0.000415809748903494517209f
#define CHASSIS_MOTOR_RPM_TO_VECTOR_SEN M3508_MOTOR_RPM_TO_VECTOR

extern uint32_t CHASSIS_CHASSIS_T;
typedef enum
{
    CHASSIS_STOP = 0,
    CHASSIS_TOP,
    CHASSIS_NO_FOLLOW_YAW,  
    CHASSIS_FOLLOW_GIMBAL_YAW,
    CHASSIS_FOLLOW_CHASSIS_YAW,
		CHASSIS_AUTO,
} Chassis_mode_e;

typedef struct
{
  const motor_measure_t *chassis_motor_measure;
  fp32 accel;
  fp32 speed;
  fp32 speed_set;
  int16_t give_current;
} chassis_motor_t;

typedef struct
{
		fp32 chassis_yaw;                          //底盘陀螺仪反馈的当前yaw角度
  fp32 chassis_pitch;                        //底盘陀螺仪反馈的当前pitch角度
  fp32 chassis_roll;                         //底盘陀螺仪反馈的当前roll角度
fp32 chassis_yaw_speed;
}chassis_imu_date_t;

typedef struct
{
	
    uint8_t ChassisMode;
    const Rcdata_t *RCData;
		const RC_ctrl_t*RC_data;
    const motor_measure_t *GetMotorWheel[4];
    const motor_measure_t *GetMotorRudder[4];
	  chassis_motor_t motor_chassis[4];    
    const motor_measure_t *GetMotorYaw;
    const fp32 *chassis_INS_angle;
	  const fp32 *chassis_gyro_angle;
    fp32 ChassisFollowGimbalAngle;
    int16_t give_wheelCurrent[4];
    int16_t give_RudderCurrent[4];
    fp32 chassis_angle_set;
		chassis_imu_date_t    chassis_imu_date;			 //惯导数据获取
    fp32 VxSet;
    fp32 Vyset;
    fp32 Wzset;
	fp32 vx_target;
fp32 vy_target;

}ChasisData_t;
 void chassis_vector_to_mecanum_wheel_speed(const fp32 vx_set, const fp32 vy_set, const fp32 wz_set, fp32 wheel_speed[4]);

extern uint16_t LAST_MODE_CHASSIS_MODE;

/***************************************全向轮底盘解算参数START*********************************************/
 /* 机器人底盘修改的参数,单位为mm(毫米)*/
#define WHEEL_BASE 296.282547213942f           		 // 纵向轴距(前进后退方向)
#define TRACK_WIDTH 296.282547213942f              // 横向轮距(左右平移方向)
#define CENTER_GIMBAL_OFFSET_X 0.08909    							 // 云台旋转中心距底盘几何中心的距离,前后方向,云台位于正中心时默认设为0
#define CENTER_GIMBAL_OFFSET_Y 0.00518   								 // 云台旋转中心距底盘几何中心的距离,左右方向,云台位于正中心时默认设为0
#define Radius_wheel 0.077f
#define REDUCTION_RATIO_WHEEL 10.0f 							 // 电机减速比,因为编码器量测的是转子的速度而不是输出轴的速度故需进行转换
 
 /* 根据robot_def.h中的macro自动计算的参数 */
#define HALF_WHEEL_BASE (WHEEL_BASE / 2.0f)     	 // 半轴距
#define HALF_TRACK_WIDTH (TRACK_WIDTH / 2.0f)  		 // 半轮距

#define robot_weight 13
#define  GRAVITY 9.8f   
#define PERIMETER_WHEEL (Radius_wheel * 2 * PI)		 // 轮子周长
 
//#define LF_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
//#define RF_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE - CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
//#define LB_CENTER ((HALF_TRACK_WIDTH + CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
//#define RB_CENTER ((HALF_TRACK_WIDTH - CENTER_GIMBAL_OFFSET_X + HALF_WHEEL_BASE + CENTER_GIMBAL_OFFSET_Y) * DEGREE_2_RAD)
#define L1_wheel 0.20500
#define L2_wheel 0.20500
#define L3_wheel 0.20500
#define L4_wheel 0.20500

/***************************************END***********************************************************************/
/*函数声明*/
/**
 * @brief 停止底盘，将速度和旋转速度设为零。
 */
void ChassisStop(void);

/**
 * @brief 在指定范围内限制输入角度。
 * 
 * @param angle 要限制的输入角度。
 * @param max 角度的最大值。
 * @return 限制后的角度。
 */
float Angle_Limit(float angle, float max);

/**
 * @brief 计算底盘电机的PID控制器输出，并更新电机电流值。
 */
void ChassisPIDcalc(void);

/**
 * @brief 根据指定的速度和旋转速度控制底盘角度。
 * 
 * @param vx_set x方向的目标速度。
 * @param vy_set y方向的目标速度。
 * @param wz_set 目标旋转速度。
 * @param wheel_angle 用于存储计算得到的轮子角度的数组。
 */
void ChassisAngleCtrl(fp32 vx_set, fp32 vy_set, fp32 wz_set, fp32 wheel_angle[4]);

/**
 * @brief 根据指定的速度和旋转速度控制底盘速度。
 * 
 * @param vx_set x方向的目标速度。
 * @param vy_set y方向的目标速度。
 * @param wz_set 目标旋转速度。
 * @param wheel_speed 用于存储计算得到的轮子速度的数组。
 */
void ChassisSpeedCtrl(fp32 vx_set, fp32 vy_set, fp32 wz_set, fp32 wheel_speed[4]);

/**
 * @brief 计算底盘电机的PID控制器输出，并更新电机电流值。
 */
void ChassisPIDcalc(void);

/**
 * @brief 初始化底盘PID控制器。
 */
void ChassisPIDInit(void);

/**
 * @brief 初始化底盘，设置初始值并初始化PID控制器。
 */
void ChassisInit(void);

/**
 * @brief 更新底盘数据，包括电机位置、角度和遥控器数据。
 */
void ChassisDataUpdate(void);

/**
 * @brief 获取当前底盘模式。
 * 
 * @return 当前底盘模式。
 */
uint8_t GetChassisMode(void);

/**
 * @brief 控制底盘以跟随底盘运动。
 */
void ChassisFollowChassis(void);

/**
 * @brief 控制底盘运动到指定位置。
 */
void ChassisTop(void);

/**
 * @brief 控制底盘跟随云台运动。
 */
void ChassisFollowGimbal(void);
void Chassis_auto(void);
void AUTO_Mode(void);
extern void chassis_moment_feedforward(fp32 wheel_fn[4],fp32 wheel_FN[4]);

extern ChasisData_t ChassisData;
/*RTOS*/
extern void chassis_task(void const * argument);
extern ChasisData_t *GetChassisData(void);
extern const ChasisData_t* chassis_data(void);
#endif
