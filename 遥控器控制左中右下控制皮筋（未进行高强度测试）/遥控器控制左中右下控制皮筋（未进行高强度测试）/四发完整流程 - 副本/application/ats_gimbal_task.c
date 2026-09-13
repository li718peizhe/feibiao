#ifdef Gimbal
#include "ats_gimbal_task.h"
#include "gimbal_calibrate.h"

#define rc_deadband_limit(input, output, dealine)        \
    {                                                    \
        if ((input) > (dealine) || (input) < -(dealine)) \
        {                                                \
            (output) = (input);                          \
        }                                                \
        else                                             \
        {                                                \
            (output) = 0;                                \
        }                                                \
    }

#define gimbal_total_pid_clear(gimbal_clear)                                                   \
    {                                                                                          \
        gimbal_PID_clear(&(gimbal_clear)->gimbal_yaw_motor.gimbal_motor_absolute_angle_pid);   \
        gimbal_PID_clear(&(gimbal_clear)->gimbal_yaw_motor.gimbal_motor_relative_angle_pid);   \
        PID_clear(&(gimbal_clear)->gimbal_yaw_motor.gimbal_motor_gyro_pid);                    \
                                                                                               \
        gimbal_PID_clear(&(gimbal_clear)->gimbal_pitch_motor.gimbal_motor_absolute_angle_pid); \
        gimbal_PID_clear(&(gimbal_clear)->gimbal_pitch_motor.gimbal_motor_relative_angle_pid); \
        PID_clear(&(gimbal_clear)->gimbal_pitch_motor.gimbal_motor_gyro_pid);                  \
    }

 gimbal_data_t gimbal_data;
gimbal_motor_t gimbal_motor_data;
fp32 FSH2;
fp32 HG;
void ats_gimbal_task(void const *pvParameters)
{
	vTaskDelay(2);
	gimbal_init(&gimbal_data);
	while(1)
	{
		gimbal_feedback_update(&gimbal_data);
		gimbal_mode_set_control(&gimbal_data);
		switch(gimbal_data.mode)
		{
			
			case ATS_GIMBAL_ABSOLUTE_ANGLE:
				gimbal_absolute_angle_control();
				break;
			case ATS_GIMBAL_INIT:gimbal_init_control(&gimbal_data);
				break;
			case ATS_GIMBAL_CALI:gimbal_cali_control(&gimbal_data);
				break;
			case ATS_GIMBAL_STOP:
				CAN_cmd_gimbal(0,0,0,0);
				break;
			default:				
				CAN_cmd_gimbal(0,0,0,0);
				break;
		}
			
	}
	
}

void gimbal_init(gimbal_data_t*init)
{
			if(init==NULL)
			{
				return ;
			}
			    static const fp32 Pitch_speed_pid[3] = {PITCH_SPEED_PID_KP, PITCH_SPEED_PID_KI, PITCH_SPEED_PID_KD};
					static const fp32 Yaw_speed_pid[3] = {YAW_SPEED_PID_KP, YAW_SPEED_PID_KI, YAW_SPEED_PID_KD};
    gimbal_total_pid_clear(init);
			//获取遥控器数据指针
				init->gimbal_RC=get_remote_control_point();
			//获取陀螺仪姿态角数据
				init->gimbal_INT_angle_point=get_INS_angle_point();
			//获取陀螺仪角速度数据
			init->gimbal_INS_angle_speed=get_gyro_data_point();
			//获取云台电机数据
			init->gimbal_yaw_motor.gimbal_motor_measure=get_yaw_gimbal_motor_measure_point();
			init->gimbal_pitch_motor.gimbal_motor_measure=get_pitch_gimbal_motor_measure_point();
			init->gimbal_yaw_motor.absolute_angle_set=init->gimbal_yaw_motor.absolute_angle;
			init->gimbal_yaw_motor.relative_angle_set = init->gimbal_yaw_motor.relative_angle;
      init->gimbal_pitch_motor.relative_angle_set = init->gimbal_pitch_motor.relative_angle;
			init->gimbal_pitch_motor.absolute_angle_set=init->gimbal_pitch_motor.absolute_angle;
			init->gimbal_yaw_motor.motor_gyro_set=init->gimbal_yaw_motor.motor_gyro;
			init->gimbal_pitch_motor.motor_gyro_set=init->gimbal_pitch_motor.motor_gyro;
			gimbal_PID_init(&init->gimbal_yaw_motor.gimbal_motor_absolute_angle_pid,YAW_GYRO_ABSOLUTE_PID_MAX_OUT,YAW_GYRO_ABSOLUTE_PID_MAX_IOUT,
																																			YAW_GYRO_ABSOLUTE_PID_KP,YAW_GYRO_ABSOLUTE_PID_KI,YAW_GYRO_ABSOLUTE_PID_KD,0);
			gimbal_PID_init(&init->gimbal_pitch_motor.gimbal_motor_absolute_angle_pid,PITCH_ENCODE_RELATIVE_PID_MAX_OUT,PITCH_ENCODE_RELATIVE_PID_MAX_IOUT,
																																						PITCH_ENCODE_RELATIVE_PID_KP,PITCH_ENCODE_RELATIVE_PID_KI,PITCH_ENCODE_RELATIVE_PID_KD,0);
			    PID_init(&init->gimbal_yaw_motor.gimbal_motor_gyro_pid, PID_POSITION, Yaw_speed_pid, YAW_SPEED_PID_MAX_OUT, YAW_SPEED_PID_MAX_IOUT);
			PID_init(&init->gimbal_pitch_motor.gimbal_motor_gyro_pid, PID_POSITION, Pitch_speed_pid, PITCH_SPEED_PID_MAX_OUT, PITCH_SPEED_PID_MAX_IOUT);
		
}
void gimbal_mode_set_control(gimbal_data_t*gimbal_mode_set)
{
		if(gimbal_mode_set==NULL)
	{
		return;
	}
	uint8_t left_switch=gimbal_mode_set->gimbal_RC->rc.s[RC_left];
	uint8_t right_switch=gimbal_mode_set->gimbal_RC->rc.s[RC_right];
	    if (gimbal_mode_set->mode == ATS_GIMBAL_CALI && gimbal_mode_set->gimbal_calibrate.step != GIMBAL_CALI_END_STEP)
    {
        return;
    }

    if (gimbal_mode_set->gimbal_calibrate.step == GIMBAL_CALI_START_STEP)
    {
        gimbal_mode_set->mode = ATS_GIMBAL_CALI;
        return;
    }
    if (gimbal_mode_set->mode == ATS_GIMBAL_INIT)
    {
        static uint16_t init_time = 0;
        static uint16_t init_stop_time = 0;
      init_time++;
      
							

        if ((fabs(gimbal_mode_set->gimbal_yaw_motor.relative_angle - INIT_YAW_SET) < GIMBAL_INIT_ANGLE_ERROR &&
            fabs(gimbal_mode_set->gimbal_pitch_motor.absolute_angle - INIT_PITCH_SET) < GIMBAL_INIT_ANGLE_ERROR))
        {
            
            if (init_stop_time < GIMBAL_INIT_STOP_TIME)
            {
                init_stop_time++;
            }
        }
        else
        {
            
            if (init_time < GIMBAL_INIT_TIME)
            {
                init_time++;
            }
        }
        if (init_time < GIMBAL_INIT_TIME && init_stop_time < GIMBAL_INIT_STOP_TIME &&
            !switch_is_down(gimbal_mode_set->gimbal_RC->rc.s[GIMBAL_MODE_CHANNEL]))
        {
            return;
        }
        else
        {
            init_stop_time = 0;
            init_time = 0;
        }
    }
	if(switch_is_down(right_switch))
	{
		gimbal_mode_set->mode=ATS_GIMBAL_STOP;
	}
	else if(switch_is_mid(right_switch)||switch_is_up(right_switch))
	{
		gimbal_mode_set->mode=ATS_GIMBAL_ABSOLUTE_ANGLE;
	}
	else
	{
		gimbal_mode_set->mode=ATS_GIMBAL_STOP;
	}	
	{
	 static uint8_t last_gimbal_mode= ATS_GIMBAL_STOP;
        if (last_gimbal_mode == ATS_GIMBAL_STOP && gimbal_mode_set->mode != ATS_GIMBAL_STOP)
        {
            gimbal_mode_set->mode = ATS_GIMBAL_INIT;
        }
        last_gimbal_mode = gimbal_mode_set->mode;
			}

}
static void gimbal_feedback_update(gimbal_data_t *feedback_update)
{

    if (feedback_update == NULL)
    {
        return;
    }
    feedback_update->gimbal_pitch_motor.absolute_angle = *(feedback_update->gimbal_INT_angle_point + INS_PITCH_ADDRESS_OFFSET);


    feedback_update->gimbal_pitch_motor.relative_angle = motor_ecd_to_angle_change(feedback_update->gimbal_pitch_motor.gimbal_motor_measure->ecd,
                                                                                   feedback_update->gimbal_pitch_motor.offset_ecd);

    feedback_update->gimbal_pitch_motor.motor_gyro = *(feedback_update->gimbal_INS_angle_speed + INS_GYRO_Y_ADDRESS_OFFSET);

    feedback_update->gimbal_yaw_motor.absolute_angle = *(feedback_update->gimbal_INT_angle_point + INS_YAW_ADDRESS_OFFSET);

    feedback_update->gimbal_yaw_motor.relative_angle = motor_ecd_to_angle_change(feedback_update->gimbal_yaw_motor.gimbal_motor_measure->ecd,
                                                                                 feedback_update->gimbal_yaw_motor.offset_ecd);
    feedback_update->gimbal_yaw_motor.motor_gyro = arm_cos_f32(feedback_update->gimbal_pitch_motor.relative_angle) * (*(feedback_update->gimbal_INS_angle_speed + INS_GYRO_Z_ADDRESS_OFFSET)) - arm_sin_f32(feedback_update->gimbal_pitch_motor.relative_angle) * (*(feedback_update->gimbal_INS_angle_speed + INS_GYRO_X_ADDRESS_OFFSET));
}
void gimbal_absolute_angle_control()
{
	  
    static int16_t yaw_channel = 0, pitch_channel = 0;
		fp32 add_yaw;fp32 add_yaw_angle_set;
		fp32 add_pitch;fp32 add_pitch_angle_set;
    rc_deadband_limit(gimbal_data.gimbal_RC->rc.ch[YAW_CHANNEL], yaw_channel, RC_DEADBAND);
    rc_deadband_limit(gimbal_data.gimbal_RC->rc.ch[PITCH_CHANNEL], pitch_channel, RC_DEADBAND);

			
    add_yaw = yaw_channel * YAW_RC_SEN - gimbal_data.gimbal_RC->mouse.x * YAW_MOUSE_SEN;
		add_yaw_angle_set=gimbal_data.gimbal_yaw_motor.absolute_angle_set;
		gimbal_data.gimbal_yaw_motor.absolute_angle_set=rad_format(add_yaw_angle_set+add_yaw);		
	gimbal_motor_absolute_angle_control(&gimbal_data.gimbal_yaw_motor);
		add_pitch_angle_set=gimbal_data.gimbal_pitch_motor.absolute_angle_set;
		 add_pitch= pitch_channel * PITCH_RC_SEN + gimbal_data.gimbal_RC->mouse.y * PITCH_MOUSE_SEN;
		gimbal_data.gimbal_pitch_motor.absolute_angle_set=rad_format(add_pitch_angle_set+add_pitch);
		gimbal_motor_absolute_angle_control(&gimbal_data.gimbal_pitch_motor);
	  FSH2=add_yaw;
			HG=add_pitch;
//		if(gimbal_data.gimbal_pitch_motor.absolute_angle_set>gimbal_data.gimbal_pitch_motor.max_relative_angle)
//		{
//			gimbal_data.gimbal_pitch_motor.absolute_angle_set=gimbal_data.gimbal_pitch_motor.max_relative_angle;
//		}
//		if(gimbal_data.gimbal_pitch_motor.absolute_angle_set<gimbal_data.gimbal_pitch_motor.min_relative_angle)
//		{
//			gimbal_data.gimbal_pitch_motor.absolute_angle_set=gimbal_data.gimbal_pitch_motor.min_relative_angle;
//		}
}
static void gimbal_motor_absolute_angle_control(gimbal_motor_t *gimbal_motor)
{
    if (gimbal_motor == NULL)
    {
        return;
    }
    gimbal_motor->motor_gyro_set = gimbal_PID_calc(&gimbal_motor->gimbal_motor_absolute_angle_pid, gimbal_motor->absolute_angle, gimbal_motor->absolute_angle_set, gimbal_motor->motor_gyro);
    gimbal_motor->current_set = PID_calc(&gimbal_motor->gimbal_motor_gyro_pid, gimbal_motor->motor_gyro, gimbal_motor->motor_gyro_set);

    gimbal_motor->given_current = (int16_t)(gimbal_motor->current_set);
}
static void gimbal_init_control(gimbal_data_t*gimbal_motor_init)
{		
	fp32 pitch_init;fp32 yaw_init;
    if ( gimbal_motor_init== NULL)
    {
        return;
    }

    if (fabs(INIT_PITCH_SET - gimbal_motor_init->gimbal_pitch_motor.absolute_angle) > GIMBAL_INIT_ANGLE_ERROR)
    {
       pitch_init = (INIT_PITCH_SET - gimbal_motor_init->gimbal_pitch_motor.absolute_angle) * GIMBAL_INIT_PITCH_SPEED;
        yaw_init = 0.0f;
    }
    else
    {
        pitch_init= (INIT_PITCH_SET - gimbal_motor_init->gimbal_pitch_motor.absolute_angle) * GIMBAL_INIT_PITCH_SPEED;
        yaw_init = (INIT_YAW_SET - gimbal_motor_init->gimbal_yaw_motor.relative_angle) * GIMBAL_INIT_YAW_SPEED;
    }
		gimbal_motor_init->gimbal_yaw_motor.relative_angle_set+=yaw_init;
		gimbal_motor_init->gimbal_pitch_motor.relative_angle_set+=pitch_init;
				gimbal_motor_absolute_angle_control(&gimbal_data.gimbal_yaw_motor);
		gimbal_motor_absolute_angle_control(&gimbal_data.gimbal_pitch_motor);

}
static void gimbal_cali_control(gimbal_data_t *gimbal_control_set)
{
   
    static uint16_t cali_time = 0;

    if (gimbal_control_set->gimbal_calibrate.step == GIMBAL_CALI_PITCH_MAX_STEP)
    {

        gimbal_control_set->gimbal_pitch_motor.raw_cmd_current = GIMBAL_CALI_MOTOR_SET;
        gimbal_control_set->gimbal_yaw_motor.raw_cmd_current = 0;
        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_pitch_motor.motor_gyro, cali_time, gimbal_control_set->gimbal_calibrate.max_pitch,
                               gimbal_control_set->gimbal_pitch_motor.absolute_angle, gimbal_control_set->gimbal_calibrate.max_pitch_ecd,
                               gimbal_control_set->gimbal_pitch_motor.gimbal_motor_measure->ecd, gimbal_control_set->gimbal_calibrate.step);
    }
    else if (gimbal_control_set->gimbal_calibrate.step == GIMBAL_CALI_PITCH_MIN_STEP)
    {
         gimbal_control_set->gimbal_pitch_motor.raw_cmd_current= -GIMBAL_CALI_MOTOR_SET;
        gimbal_control_set->gimbal_yaw_motor.raw_cmd_current = 0;

        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_pitch_motor.motor_gyro, cali_time, gimbal_control_set->gimbal_calibrate.min_pitch,
                               gimbal_control_set->gimbal_pitch_motor.absolute_angle, gimbal_control_set->gimbal_calibrate.min_pitch_ecd,
                               gimbal_control_set->gimbal_pitch_motor.gimbal_motor_measure->ecd, gimbal_control_set->gimbal_calibrate.step);
    }
    else if (gimbal_control_set->gimbal_calibrate.step == GIMBAL_CALI_YAW_MAX_STEP)
    {
         gimbal_control_set->gimbal_pitch_motor.raw_cmd_current = 0;
        gimbal_control_set->gimbal_yaw_motor.raw_cmd_current= GIMBAL_CALI_MOTOR_SET;

        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_yaw_motor.motor_gyro, cali_time, gimbal_control_set->gimbal_calibrate.max_yaw,
                               gimbal_control_set->gimbal_yaw_motor.absolute_angle, gimbal_control_set->gimbal_calibrate.max_yaw_ecd,
                               gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->ecd, gimbal_control_set->gimbal_calibrate.step);
    }

    else if (gimbal_control_set->gimbal_calibrate.step == GIMBAL_CALI_YAW_MIN_STEP)
    {
         gimbal_control_set->gimbal_pitch_motor.raw_cmd_current = 0;
        gimbal_control_set->gimbal_yaw_motor.raw_cmd_current = -GIMBAL_CALI_MOTOR_SET;

        gimbal_cali_gyro_judge(gimbal_control_set->gimbal_yaw_motor.motor_gyro, cali_time, gimbal_control_set->gimbal_calibrate.min_yaw,
                               gimbal_control_set->gimbal_yaw_motor.absolute_angle, gimbal_control_set->gimbal_calibrate.min_yaw_ecd,
                               gimbal_control_set->gimbal_yaw_motor.gimbal_motor_measure->ecd, gimbal_control_set->gimbal_calibrate.step);
    }
    else if (gimbal_control_set->gimbal_calibrate.step == GIMBAL_CALI_END_STEP)
    {
        cali_time = 0;
    }
}
static void gimbal_PID_clear(gimbal_PID_t *gimbal_pid_clear)
{
    if (gimbal_pid_clear == NULL)
    {
        return;
    }
    gimbal_pid_clear->err = gimbal_pid_clear->set = gimbal_pid_clear->get = 0.0f;
    gimbal_pid_clear->out = gimbal_pid_clear->Pout = gimbal_pid_clear->Iout = gimbal_pid_clear->Dout = 0.0f;
}

gimbal_data_t *GetGimbaldata(void)
{
   
    return &gimbal_data;
}

#endif

