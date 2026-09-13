#ifdef Gimbal
#ifndef ATS_GIMBAL_TASK_H
#define ATS_GIMBAL_TASK_H
#include "main.h"
#include "cmsis_os.h"
#include "struct_typedef.h"
#include "remote_control.h"
#include "INS_task.h"
#include "CAN_receive.h"
#include "user_lib.h"
#include "arm_math.h"
#include "pid.h"
#include "DJI_motor.h"

#define RC_left 1
#define RC_right 0
#define YAW_CHANNEL   2
#define PITCH_CHANNEL 3
#define RC_DEADBAND   20
#define YAW_RC_SEN    -0.000005f
#define PITCH_RC_SEN  -0.000006f //0.005
#define YAW_MOUSE_SEN   0.00005f
#define PITCH_MOUSE_SEN 0.00015f

#define GIMBAL_INIT_ANGLE_ERROR     0.1f
#define INIT_YAW_SET    0.0f
#define INIT_PITCH_SET  0.0f
#define GIMBAL_INIT_PITCH_SPEED     0.004f
#define GIMBAL_INIT_YAW_SPEED       0.005f

#define GIMBAL_CALI_MOTOR_SET   8000
#define GIMBAL_CALI_STEP_TIME   2000
#define GIMBAL_CALI_GYRO_LIMIT  0.1f

#define YAW_GYRO_ABSOLUTE_PID_KP        15.0f
#define YAW_GYRO_ABSOLUTE_PID_KI        0.0f
#define YAW_GYRO_ABSOLUTE_PID_KD        0.0f
#define YAW_GYRO_ABSOLUTE_PID_MAX_OUT   12.0f
#define YAW_GYRO_ABSOLUTE_PID_MAX_IOUT  0.0f

#define PITCH_ENCODE_RELATIVE_PID_KP 25.0f
#define PITCH_ENCODE_RELATIVE_PID_KI 0.00f
#define PITCH_ENCODE_RELATIVE_PID_KD 0.0f

#define PITCH_ENCODE_RELATIVE_PID_MAX_OUT 15.0f
#define PITCH_ENCODE_RELATIVE_PID_MAX_IOUT 0.0f

//pitch speed close-loop PID params, max out and max iout
#define PITCH_SPEED_PID_KP        9000.0f
#define PITCH_SPEED_PID_KI        0.0f
#define PITCH_SPEED_PID_KD        20.0f
#define PITCH_SPEED_PID_MAX_OUT   30000.0f
#define PITCH_SPEED_PID_MAX_IOUT  5000.0f
extern uint16_t AUTO;
//yaw speed close-loop PID params, max out and max iout
#define YAW_SPEED_PID_KP        7500.0f
#define YAW_SPEED_PID_KI        0.0f
#define YAW_SPEED_PID_KD        20.0f
#define YAW_SPEED_PID_MAX_OUT   30000.0f
#define YAW_SPEED_PID_MAX_IOUT  0.0f


extern void ats_gimbal_task(void const *pvParameters);




typedef enum
{
		ATS_GIMBAL_STOP=0,	
	ATS_GIMBAL_ABSOLUTE_ANGLE,
		ATS_GIMBAL_INIT,
		ATS_GIMBAL_CALI,
	ATS_GIMBAL_VISION,
}gimbal_mode_t;





typedef struct
{
    const motor_measure_t *gimbal_motor_measure;
    gimbal_PID_t gimbal_motor_absolute_angle_pid;
    gimbal_PID_t gimbal_motor_relative_angle_pid;
    pid_type_def gimbal_motor_gyro_pid;
    uint16_t offset_ecd;
    fp32 max_relative_angle; //rad
    fp32 min_relative_angle; //rad

    fp32 relative_angle;     //rad
    fp32 relative_angle_set; //rad
    fp32 absolute_angle;     //rad
    fp32 absolute_angle_set; //rad
    fp32 motor_gyro;         //rad/s
    fp32 motor_gyro_set;
    fp32 motor_speed;
    fp32 raw_cmd_current;
    fp32 current_set;
    int16_t given_current;

} gimbal_motor_t;
typedef struct
{
    fp32 max_yaw;
    fp32 min_yaw;
    fp32 max_pitch;
    fp32 min_pitch;
    uint16_t max_yaw_ecd;
    uint16_t min_yaw_ecd;
    uint16_t max_pitch_ecd;
    uint16_t min_pitch_ecd;
    uint8_t step;
} gimbal_step_cali_t;

typedef struct
{
	const RC_ctrl_t*gimbal_RC;
	const fp32*gimbal_INT_angle_point;
	const fp32*gimbal_INS_angle_speed;
	gimbal_motor_t gimbal_yaw_motor;
  gimbal_motor_t gimbal_pitch_motor;
	int16_t mode;
	gimbal_step_cali_t gimbal_calibrate;
}gimbal_data_t;
extern gimbal_data_t gimbal_data;
 void gimbal_init(gimbal_data_t*gimbal_init);
 void ats_gimbal_updata(gimbal_data_t*gimbal_updata);
 void gimbal_mode_set_control(gimbal_data_t*gimbal_mode_set);
extern void gimbal_feedback_update(gimbal_data_t *feedback_update);
static void gimbal_motor_absolute_angle_control(gimbal_motor_t *gimbal_motor);
void gimbal_absolute_angle_control();
static void gimbal_init_control(gimbal_data_t*gimbal_motor_init);
extern gimbal_data_t *GetGimbaldata(void);
static void gimbal_cali_control(gimbal_data_t *gimbal_control_set);
static void gimbal_PID_clear(gimbal_PID_t *pid_clear);

#endif
#endif
