#ifdef Chassis
#include "ats_chassis_task.h"
#include "chassis_power_control.h"
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

chassis_data_t chassis_DATA;
 ramp_function_source_t ATS_Wx_set;
 ramp_function_source_t ATS_Wy_set;
 ramp_function_source_t ATS_Wz_set;
pid_type_def ATS_GM3508_Speed[4];
pid_type_def ChassisFollow_PID;
const fp32 GM3508_Speed_PID[3] = {M3508_MOTOR_SPEED_PID_KP, M3508_MOTOR_SPEED_PID_KI, M3508_MOTOR_SPEED_PID_KD};
const fp32 FollowGaible_PID[3] = {0.30f, 0.0f, 7.0f};

void ats_chassis_task(void const *pvParameters)
{
	    vTaskDelay(10);
			 ats_chassis_init(&chassis_DATA);
				while(1)
				{
				ats_chassis_updata(&chassis_DATA);
				mode_set_control(&chassis_DATA);		
					switch(chassis_DATA.mode)
					{
						case ATS_CHASSIS_TOP:
						ChassisTop();							
						break;
						case ATS_CHASSIS_FOLLOW_GIMBAL_YAW:chassis_control(&chassis_DATA);
							break;
						case ATS_CHASSIS_STOP:
						CAN_cmd_chassis(0,0,0,0);
							break;
						default: 
							CAN_cmd_chassis(0,0,0,0);
							break;
					}
					
				}
}


void ats_chassis_init(chassis_data_t *chassis_init)
{
	if(chassis_init==NULL)
	{
		return ;
	}
	//获取遥控器指针
	chassis_init->chassis_board=Get_Rcdata();
	//获取陀螺仪姿态角指针
	chassis_init->chassis_INS_angle=get_INS_angle_point();
	//获取陀螺仪角速度指针
	chassis_init->chassis_INS_angle_speed=get_gyro_data_point();
	//获取加速度计滤波后的数据
	chassis_init->accel_fliter=get_accel_data_point();
	for(uint8_t i=0;i<=4;i++)
	{
		chassis_init->GetMotorWheel[i]=get_chassis_Wheelmotor_point(i);
	}
		for(uint8_t i=0;i<=4;i++)
	{
	 PID_init(&ATS_GM3508_Speed[i], PID_POSITION, GM3508_Speed_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
	}
		for(uint8_t i=0;i<=4;i++)
	{
	 PID_init(&ChassisFollow_PID, PID_POSITION, FollowGaible_PID, CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT, CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT);
	}

    ramp_init(&ATS_Wx_set, 0.01, 0.0f, 0.0f);
    ramp_init(&ATS_Wy_set, 0.01, 0.0f, 0.0f);
    ramp_init(&ATS_Wz_set, 1, 2.5f, 0.0f);
}

void ats_chassis_updata(chassis_data_t*chassis_updata)
{
	if(chassis_updata==NULL)
	{
		return;
	}
		for(uint8_t i=0;i<=4;i++)
	{
		chassis_updata->GetMotorWheel[i]=get_chassis_Wheelmotor_point(i);
	}

}

void mode_set_control(chassis_data_t*chassis_mode_set)
{
	if(chassis_mode_set==NULL)
	{
		return;
	}
//	uint8_t left_switch=chassis_mode_set->chassis_RC->rc.s[RC_left];
	uint8_t right_switch=chassis_mode_set->chassis_board->rc.s[RC_right];
	if(switch_is_down(right_switch))
	{
		chassis_mode_set->mode=ATS_CHASSIS_STOP;
	}
	else if(switch_is_mid(right_switch))
	{
		chassis_mode_set->mode=ATS_CHASSIS_FOLLOW_GIMBAL_YAW;
	}
	else if(switch_is_up(right_switch))
	{
		chassis_mode_set->mode=ATS_CHASSIS_TOP;
	}
	else
	{
		chassis_mode_set->mode=ATS_CHASSIS_STOP;
	}
	
}
void RC_data_updata(chassis_data_t*rc_updata)
{
	if(rc_updata==NULL)
	{
		return;
	}
	rc_updata->x_set=rc_updata->chassis_board->rc.ch[1]*CHASSIS_VX_RC_SEN;
	rc_updata->y_set=rc_updata->chassis_board->rc.ch[0]*CHASSIS_VY_RC_SEN;
}

	fp32 Key_ramp_x;
fp32 Key_ramp_y;
fp32 angle_set=0;
void ChassisTop(void)
{
    int16_t vx_channel, vy_channel;
    fp32 sin_yaw = 0., cos_yaw = 0;
//    NOW_MUSIC = 4;
//    if(NOW_MUSIC == 4 && LAST_MUSIC != NOW_MUSIC)
//    {
//        B_Music();
//    }
//    LAST_MUSIC = NOW_MUSIC;

    // 读取遥控器通道输入，应用死区限制
    rc_deadband_limit(chassis_DATA.chassis_board->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    rc_deadband_limit(chassis_DATA.chassis_board->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);

		
    // 根据遥控器输入调整底盘速度
    vx_channel = vx_channel * CHASSIS_RC_SEN*2.0 ;
    vy_channel = -vy_channel * CHASSIS_RC_SEN*2.0;

		
    // 根据键盘输入调整底盘速度
    if (chassis_DATA.chassis_board->key.v & CHASSIS_FRONT_KEY)
    {
        vx_channel = KEYBOARD_TOP_SPEED_MAX;
    }
    else if (chassis_DATA.chassis_board->key.v & CHASSIS_BACK_KEY)
    {
        vx_channel = -KEYBOARD_TOP_SPEED_MAX;
    }
    if (chassis_DATA.chassis_board->key.v & CHASSIS_LEFT_KEY)
    {
        vy_channel = KEYBOARD_TOP_SPEED_MAX;
    }
    else if (chassis_DATA.chassis_board->key.v & CHASSIS_RIGHT_KEY)
    {
        vy_channel = -KEYBOARD_TOP_SPEED_MAX;
    }

    // 计算云台偏航角对应的正弦和余弦值
    chassis_DATA.ChassisFollowGimbalAngle = motor_ecd_to_angle_change(chassis_DATA.chassis_board->yaw_ecd, chassis_DATA.chassis_board->gimbal_angle);
    sin_yaw = arm_sin_f32(-chassis_DATA.ChassisFollowGimbalAngle);
    cos_yaw = arm_cos_f32(-chassis_DATA.ChassisFollowGimbalAngle);

   ATS_Wz_set.max_value = 3.14;
		    ramp_calc(&ATS_Wz_set, 0.5f);		
    chassis_DATA.Vx_set = cos_yaw * vx_channel + sin_yaw * vy_channel;
    chassis_DATA.Vy_set = -sin_yaw * vx_channel + cos_yaw * vy_channel;
chassis_DATA.Wz_set = ATS_Wz_set.out;
    Chassis_PowerLimit_T();

}

void chassis_control(chassis_data_t*control_data)
{ 
	    int16_t vx_channel, vy_channel;/*angle_set = 0.0f*/
    fp32 sin_yaw = 0., cos_yaw = 0;

		 chassis_DATA.ChassisFollowGimbalAngle = motor_ecd_to_angle_change(chassis_DATA.chassis_board->yaw_ecd, chassis_DATA.chassis_board->gimbal_angle);
    sin_yaw = arm_sin_f32(chassis_DATA.ChassisFollowGimbalAngle);
    cos_yaw = arm_cos_f32(chassis_DATA.ChassisFollowGimbalAngle);
		chassis_rc_key();
		ramp_calc1(&ATS_Wx_set, control_data->vx_target);  // 对 x 轴的目标速度进行斜坡平滑
    ramp_calc1(&ATS_Wy_set, control_data->vy_target);  // 对 y 轴的目标速度进行斜坡平滑
		ramp_calc(&ATS_Wz_set,0.5);
		control_data->Vx_set=ATS_Wx_set.out;
		control_data->Vy_set=ATS_Wy_set.out;
		control_data->Wz_set=ATS_Wz_set.out;
		Key_ramp_x=control_data->Vx_set;
	Key_ramp_y=control_data->Vy_set;

	    ATS_Wx_set.max_value = cos_yaw * vx_channel + sin_yaw * vy_channel+Key_ramp_x;
    ATS_Wy_set.max_value = -sin_yaw * vx_channel + cos_yaw * vy_channel+Key_ramp_y;
    // 设定底盘角度目标值
    chassis_DATA.ats_chassis_angle_set = rad_format(angle_set);

    // 根据云台相对底盘的角度变化和目标角度计算底盘的角速度设置
    chassis_DATA.Wz_set = - PID_calc(&ChassisFollow_PID, chassis_DATA.ChassisFollowGimbalAngle,  chassis_DATA.ats_chassis_angle_set);

	 Chassis_PowerLimit_T();
}
extern fp32 wheel_speed_yu[4];
 fp32 wheel_rpm_ratio = 60.0f / (PERIMETER_WHEEL) * M3508_RATIO * 1000;
void chassis_vector_to_mecanum_wheel_speed(const fp32 vx_set, const fp32 vy_set, const fp32 wz_set, fp32 wheel_speed[4])
{
		wheel_speed[0] =( -vx_set - vy_set - wz_set * RF_CENTER)*wheel_rpm_ratio;
    wheel_speed[1] = (+vx_set - vy_set - wz_set * LF_CENTER)*wheel_rpm_ratio;
    wheel_speed[2] =( +vx_set + vy_set - wz_set * LB_CENTER)*wheel_rpm_ratio;
    wheel_speed[3] = (-vx_set + vy_set - wz_set * RB_CENTER)*wheel_rpm_ratio;
}


void chassis_rc_key()
{
int16_t vx_channel, vy_channel;
   
    
    // 读取遥控器通道输入，应用死区限制
    rc_deadband_limit(chassis_DATA.chassis_board->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    rc_deadband_limit(chassis_DATA.chassis_board->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);
    
    // 根据遥控器输入调整底盘速度
    vx_channel = vx_channel * CHASSIS_VX_RC_SEN;
    vy_channel = -vy_channel * CHASSIS_VX_RC_SEN;


    // 前进
if (chassis_DATA.chassis_board->key.v & CHASSIS_FRONT_KEY) {
    ATS_Wx_set.max_value = 5.0;
    chassis_DATA.vx_target = KEYBOARD_SPEED_MAX;  // 设置前进目标速度
	if(chassis_DATA.chassis_board->key.v & CHASSIS_LEFT_KEY)
	{
		  ATS_Wy_set.max_value = 5.0;
    chassis_DATA.vy_target = KEYBOARD_SPEED_MAX;  // 设置左移目标速度
	}
	
	else if (chassis_DATA.chassis_board->key.v & CHASSIS_RIGHT_KEY) {
	    ATS_Wy_set.max_value = -5.0;
    chassis_DATA.vy_target = -KEYBOARD_SPEED_MAX;  // 设置右移目标速度
	
}
	else 
	{
		 ATS_Wy_set.max_value = 0;
		chassis_DATA.vy_target=0;
	}
}
// 后退
else if (chassis_DATA.chassis_board->key.v & CHASSIS_BACK_KEY) {
    ATS_Wx_set.max_value = -5.0;
    chassis_DATA.vx_target = -KEYBOARD_SPEED_MAX;  // 设置后退目标速度
	if(chassis_DATA.chassis_board->key.v & CHASSIS_LEFT_KEY)
	{
		  ATS_Wy_set.max_value = 5.0;
    chassis_DATA.vy_target = KEYBOARD_SPEED_MAX;  // 设置左移目标速度
	}
	
	else if (chassis_DATA.chassis_board->key.v & CHASSIS_RIGHT_KEY) {
	    ATS_Wy_set.max_value = -5.0;
    chassis_DATA.vy_target = -KEYBOARD_SPEED_MAX;  // 设置右移目标速度
	
}
}
// 左移
else if (chassis_DATA.chassis_board->key.v & CHASSIS_LEFT_KEY) {
	    ATS_Wy_set.max_value = 5.0;
    chassis_DATA.vy_target = KEYBOARD_SPEED_MAX;  // 设置左移目标速度
}
// 右移
else if (chassis_DATA.chassis_board->key.v & CHASSIS_RIGHT_KEY) {
	    ATS_Wy_set.max_value = -5.0;
    chassis_DATA.vy_target = -KEYBOARD_SPEED_MAX;  // 设置右移目标速度
	
}
else if (vx_channel != 0 || vy_channel != 0)

{
	if(vx_channel>0)
	{
		ATS_Wx_set.max_value = 5.0;
		chassis_DATA.vx_target=0.5;
	}
	else if(vx_channel<0)
	{
		ATS_Wx_set.max_value = -5.0;
		chassis_DATA.vx_target=-0.5;
	}
	else if(vy_channel>0)
	{
		ATS_Wy_set.max_value = 5.0;
		chassis_DATA.vy_target=0.5;
	}
	else if(vy_channel<0)
	{
		ATS_Wy_set.max_value = -5.0;
		chassis_DATA.vy_target=-0.5;
	}
	
}

// 无按键按下时，速度设置为零
else {
    ATS_Wx_set.max_value = 0;  // 无按键时，前进速度为零
    ATS_Wy_set.max_value = 0;  // 无按键时，侧向速度为零
	chassis_DATA.vy_target=0;
		chassis_DATA.vx_target=0;
}
 
}
chassis_data_t *GetChassisdata(void)
{
    // 返回底盘数据结构的指针
    return &chassis_DATA;
}
#endif
