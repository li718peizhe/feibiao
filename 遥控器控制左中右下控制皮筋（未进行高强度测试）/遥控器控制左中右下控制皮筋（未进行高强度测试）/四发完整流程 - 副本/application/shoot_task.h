#ifdef Gimbal
#ifndef SHOOT_TASK_H
#define SHOOT_TASK_H
#include "main.h"
#include "cmsis_os.h"
#include "CAN_receive.h"

#define SHOOT_OPEN_FIRE KEY_PRESSED_OFFSET_E
#define SHOOT_VISION_FIRE KEY_PRESSED_OFFSET_R

#define  Single_Data  36864
#define SHOOT_FULL_TIME 100
#define SHOOT_CONTROL_TIME 100	

/*摩擦轮电机最大输出限制*/
#define FIRE_left_SPEED_PID_MAX_OUT 15000.0f
#define FIRE_left_SPEED_PID_MAX_IOUT 1000.0f

#define FIRE_right_SPEED_PID_MAX_OUT 15000.0f
#define FIRE_right_SPEED_PID_MAX_IOUT 1000.0f

#define FIRE_MOMENT_PID_MAX_OUT 15000.0f
#define FIRE_MOMENT_PID_MAX_IOUT 1000.0f

//拨弹轮电机PID
#define TRIGGER_ANGLE_PID_KP 0.3f
#define TRIGGER_ANGLE_PID_KI 0.0f
#define TRIGGER_ANGLE_PID_KD 0.9f

#define TRIGGER_SPEED_PID_KP 9.0f
#define TRIGGER_SPEED_PID_KI 0.0f
#define TRIGGER_SPEED_PID_KD 1.0f
/*拨弹盘双环PID输出限制*/
#define BULLET_ANG_PID_MAX_OUT 10000.0f
#define BULLET_ANG_PID_MAX_IOUT 0.0f
#define BULLET_SPEED_PID_MAX_OUT 10000.0f
#define BUFFET_SPEED_PID_MAX_IOUT 1000.0f


extern void shoot_task(void const *pvParameters);


typedef enum
{
    SHOOT_STOP = 0,
    SHOOT_STAR,
}ShootMode_t;

typedef struct
{
    const motor_measure_t *LeftFireData;
    const motor_measure_t *RightFireData;
    const motor_measure_t *TriggerData;
    const RC_ctrl_t *RC_data;
    ShootMode_t ShootMode;
    int8_t last_rc_s;
    int8_t last_rc_T;
		int8_t LAST_PRESS_L;
    fp32 Fire_left_speed_set;
    fp32 Fire_right_speed_set;
    int TriggerMotor_pos_set;
    int16_t give_current[3];

} ShootControl_t;
void Shoot_Init(void);
void Set_ShootMode(void);
void Set_ShootVal(void);
void Shootdata_updata();

#endif
#endif
