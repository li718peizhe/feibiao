#include "chassis_task.h"
#include "referee_lib.h"
#include "cmsis_os.h"
#include "user_lib.h"
#include "arm_math.h"
#include "pid.h"
#include "remote_control.h"
#include "CAN_receive.h"
#include "INS_task.h"
#include "chassis_power_control.h"
#include "bsp_buzzer.h"
#include "cap_control.h"
#include "RM_Cilent_UI.h"
#include "referee.h"
/**
 * @brief �������ź�Ӧ���������Ƶĺ궨�塣
 *
 * ��������źŵľ���ֵ���ڸ�����������ֵ��dealine����������źŵ��������źţ�
 * ��������ź�Ϊ�㡣���ڷ�ֹС���ȵ����벨����ϵͳ��������Ҫ����Ӧ��
 *
 * @param input �����źš�
 * @param output ����źţ���������������Ľ����
 * @param dealine ������ֵ��
 */
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

float Change_TOP = 0.1, YU1 = 0, YU3 = 0.5;
		fp32 v_k;
#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t chassis_high_water;
#endif


// GM6020����ٶȿ��Ƶ�PID�������飬����4�����
pid_type_def GM6020_Speed[4];

// GM3508����ٶȿ��Ƶ�PID�������飬����4�����
pid_type_def GM3508_Speed[4];
pid_type_def GM3508_Speed_ALL;

// ���̸�����Ƶ�PID����
pid_type_def ChassisFollow_PID;
pid_type_def Chassis_yaw_speed;
extern pid_type_def givecurrent_limt[4];

// �������ݽṹ�����ڴ洢����״̬��ģʽ����Ϣ
ChasisData_t ChassisData;
ramp_function_source_t wx_set;
ramp_function_source_t wy_set;
ramp_function_source_t wz_set;
ramp_function_source_t chassis_rmp;
ramp_function_source_t chassis_wz_set;

const fp32 GM3508_Speed_PID[3] = {15, 0, 0.3};

const fp32 Chassis_Power[3] = {1.0f, 0.0f, 0.0f};

extern fp32 INS_accel[3];
 fp32 FollowGaible_PID[3] = {3.5f, 0.0f, 0.0f};
 fp32 Chassis_yaw_speed_PID[3]={1,0,0};
 //{0.40f, 0.0f, 10.0f}
uint8_t add_YN = 0;
fp32 angle_set = 0;
uint16_t AUTO = 0;
fp32 SPEED_TOP=150;
/**
 * @brief ������������
 *
 * �˺�����ʾһ�� FreeRTOS ���񣬸�����Ƶ��̵���Ϊ��
 * �����ȳ�ʼ�����̣�Ȼ�����һ��ѭ������ѭ���и��µ������ݲ����ݵ�ǰ����ģʽִ����Ӧ�Ĳ�����
 *
 * ѭ������һ���ӳ٣����ڿ�������ִ�е�Ƶ�ʣ�������������� INCLUDE_uxTaskGetStackHighWaterMark��
 * �����һ����ѡ�Ĳ��֣����ڸ�������Ķ�ջ��ˮλ��ǡ�
 *
 * @param pvParameters ��������������������δʹ�ã���
 */
void chassis_task(void const *pvParameters)
{
    // �ӳ� 10 ���룬�Ա����������ʼ��
    vTaskDelay(10);
    //B_Music();
    // ��ʼ������
    ChassisInit();
    cap_state_init();
    // ������ѭ��
    while (1)
    {
				//buzzer_off();
        // ���µ�������
        ChassisDataUpdate();
        cap_feedback_update();
        cap_control_loop();
        // ���ݵ�ǰ����ģʽִ����Ӧ�Ĳ���
        switch (ChassisData.ChassisMode)
        {
        // ���̸�����̨����ģʽ
        case CHASSIS_FOLLOW_GIMBAL_YAW:
            ChassisFollowGimbal();
            break;

        // ������תģʽ
        case CHASSIS_TOP:
            ChassisTop();
            break;
        // Ĭ�������ֹͣ����
        default:
            ChassisStop();
            break;
        }
				

        // ����һ��С�ӳ٣��Կ�������ִ�е�Ƶ��
        vTaskDelay(1);

        // ��ѡ����������Ķ�ջ��ˮλ���
#if INCLUDE_uxTaskGetStackHighWaterMark
        chassis_high_water = uxTaskGetStackHighWaterMark(NULL);
#endif
    }
}


uint16_t CHASSIS_S_T = 0;
uint16_t CHASSIS_GIMBAL_T = 1;
uint32_t CHASSIS_CHASSIS_T = 0;
uint16_t CHASSIS_T_T = 0;
uint16_t LAST_CHASSIS_S_T = 0;
uint16_t LAST_CHASSIS_GIMBAL_T = 0;
uint16_t LAST_CHASSIS_CHASSIS_T = 0;
uint16_t LAST_CHASSIS_T_T = 0;
uint8_t red_outpost,blue_outpost;
int TOP=1;
uint8_t TOP_mode=0;
/**
 * @brief ����ģʽ�л�������
 *
 * �ú���ͨ��ң��������İ���״̬���жϵ���Ӧ���ڵĹ���ģʽ��
 * ���ݰ�����ϣ��л�����ģʽΪֹͣ�����̸�����̺��򡢵��̸�����̨����͵��̶�����
 *
 * @return ����ģʽ��CHASSIS_STOP, CHASSIS_FOLLOW_CHASSIS_YAW, CHASSIS_FOLLOW_GIMBAL_YAW, CHASSIS_TOP����
 */
uint8_t GetChassisMode(void)
{
    uint8_t mode = 0;

			if(ChassisData.RC_data->key.v&CHASSIS_GIMBAL)
			{
				TOP_mode=1;
				TOP=-1;
				 v_k=0.085;

			}
			else if(ChassisData.RC_data->key.v&KEY_PRESSED_OFFSET_SHIFT)
			{
				TOP_mode=1;
				TOP=1;
				 v_k=0.095;
			}
			else
			{
				TOP_mode=0;
			}
//			if(ChassisData.RC_data->rc.ch[4]>600)
//			{
//				TOP=-1;
//			}
//			else
//			{
//				TOP=1;
//			}
    // �жϾ������ģʽ
     if ((switch_is_down(ChassisData.RC_data->rc.s[RC_Right]) && switch_is_down(ChassisData.RC_data->rc.s[RC_Left])) /*|| (switch_is_mid(ChassisData.RC_data->rc.s[RC_Right]) && !CHASSIS_S_T && !CHASSIS_GIMBAL_T && !CHASSIS_T_T && CHASSIS_CHASSIS_T)*/)
    {
        mode = CHASSIS_STOP;
    }
    else if (((switch_is_mid(ChassisData.RC_data->rc.s[RC_Right]) && switch_is_mid(ChassisData.RC_data->rc.s[RC_Left]))&&TOP_mode!=1)||(switch_is_mid(ChassisData.RC_data->rc.s[RC_Right])&& switch_is_up(ChassisData.RC_data->rc.s[RC_Left]))/*&& !CHASSIS_S_T && !CHASSIS_T_T && !CHASSIS_CHASSIS_T && CHASSIS_GIMBAL_T*/)
    {
        mode = CHASSIS_FOLLOW_GIMBAL_YAW;
    }
    else if ((switch_is_mid(ChassisData.RC_data->rc.s[RC_Left]) && switch_is_up(ChassisData.RC_data->rc.s[RC_Right])) || (switch_is_mid(ChassisData.RC_data->rc.s[RC_Right]) &&TOP_mode==1/*&& !CHASSIS_S_T && !CHASSIS_GIMBAL_T && !CHASSIS_CHASSIS_T && CHASSIS_T_T*/))
    {
        mode = CHASSIS_TOP;
		v_k=0.085;
    }
    else
    {
        mode = CHASSIS_STOP;
    }
    return mode;
}

/**
 * @brief ���̳�ʼ��������
 *
 * �ú������ڳ�ʼ���������ݽṹ��ң�������ݡ��Լ�������صĴ������͵����
 */
void ChassisInit(void)
{
    // ��ʼ���������ݽṹ
    ChassisData.ChassisMode = CHASSIS_STOP;
    ChassisData.RC_data=get_remote_control_point();
    ChassisData.chassis_INS_angle = get_INS_angle_point();
	ChassisData.chassis_gyro_angle=get_gyro_data_point();
    ChassisData.GetMotorYaw = get_gimbal_yaw__point();
    ramp_init(&wx_set, 0.008, 0.0f, 0.0f);
    ramp_init(&wy_set, 0.008, 0.0f, 0.0f);
    ramp_init(&wz_set, 1, 2.5f, 0.0f);
	ramp_init(&chassis_rmp, 0.002, 1.0f, 0.0f);
	ramp_init(&chassis_wz_set, 0.001, 2.5f, 0.0f);

    // ��ʼ�����̵��
    for (uint8_t i = 0; i < 4; i++)
    {
        ChassisData.GetMotorWheel[i] = get_chassis_Wheelmotor_point(i);
    }

    // ��ʼ������PID������
    ChassisPIDInit();
}

/**
 * @brief ����PID��ʼ��������
 *
 * �ú������ڳ�ʼ��������ʹ�õ�PID��������
 */
void ChassisPIDInit(void)
{
    // GM3508����ٶ�PID��ʼ��
    PID_init(&GM3508_Speed[0], PID_POSITION, GM3508_Speed_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT); 
    PID_init(&GM3508_Speed[1], PID_POSITION, GM3508_Speed_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
    PID_init(&GM3508_Speed[2], PID_POSITION, GM3508_Speed_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
    PID_init(&GM3508_Speed[3], PID_POSITION, GM3508_Speed_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);

    // ���̸�����̨PID��ʼ��
    PID_init(&ChassisFollow_PID, PID_POSITION, FollowGaible_PID, CHASSIS_FOLLOW_GIMBAL_PID_MAX_OUT, CHASSIS_FOLLOW_GIMBAL_PID_MAX_IOUT);
	PID_init(&Chassis_yaw_speed,PID_POSITION,Chassis_yaw_speed_PID,1,1);
}

/**
 * @brief ���µ������ݺ�����
 *
 * �ú������ڸ��µ��̵Ĵ��������ݡ�ң���������Լ���ǰ����ģʽ��
 */
int Change_power;

float Change_AUTO_TOP;
uint32_t Cup_control = 0;
uint32_t mini_top = 0;
uint32_t frictiongear = 0;
uint32_t AAAA = 0;
extern uint32_t Changer_UI;

void ChassisDataUpdate(void)
{
    // ���µ��̵������
    for (uint8_t i = 0; i < 4; i++)
    {
        ChassisData.GetMotorWheel[i] = get_chassis_Wheelmotor_point(i);
    }

    // ���µ�����̬�����ݺ͵�ǰ����ģʽ
    ChassisData.chassis_INS_angle = get_INS_angle_point();
		ChassisData.chassis_gyro_angle=get_gyro_data_point();
    ChassisData.ChassisMode = GetChassisMode();
    ChassisData.GetMotorYaw = get_gimbal_yaw__point();
    ChassisData.RC_data=get_remote_control_point();
	    ChassisData.chassis_imu_date.chassis_yaw = -(rad_format(*(ChassisData.chassis_INS_angle +
        INS_YAW_ADDRESS_OFFSET)));
    ChassisData.chassis_imu_date.chassis_pitch = (rad_format(*(ChassisData.chassis_INS_angle +
        INS_PITCH_ADDRESS_OFFSET)));	 //̧ͷΪ��
    ChassisData.chassis_imu_date.chassis_roll  = -(*(ChassisData.chassis_INS_angle +
        INS_ROLL_ADDRESS_OFFSET));			 //����Ϊ��
		    ChassisData.chassis_imu_date.chassis_yaw_speed = (*(ChassisData.chassis_gyro_angle + INS_YAW_ADDRESS_OFFSET));


	}
fp32 WheelSpeed[4];
/**
 * @brief ����PID���㺯����
 *
 * �ú������ڼ�����̵���Ͷ����PID����������������µ������ֵ��
 */
void ChassisPIDcalc(void)
{
    // ������̵������
    for (uint8_t i = 0; i < 4; i++)
    {
        ChassisData.give_wheelCurrent[i] = PID_calc(&GM3508_Speed[i], MotorWheel[i].speed_rpm, WheelSpeed[i]);
    }

}

/**
 * @brief ����ֹͣ������
 *
 * �ú������ڽ������ٶȺͽ��ٶ�����Ϊ�㣬������ָֹͣ������̵����
 */
void ChassisStop(void)
{
    // �������ٶȺͽ��ٶ�����Ϊ��
    ChassisData.VxSet = 0;
    ChassisData.Vyset = 0;
    ChassisData.Wzset = 0;
    // ����ָֹͣ������̵��
    CAN_Cmd3508(0, 0, 0, 0);
}


/**
 * @brief ���Ƶ����˶�����ת������
 *
 * �ú�������ң��������ͼ��̰���״̬�����Ƶ��̵��˶���
 *
 * @note �ú���������ң�����ͼ��̵����룬�Լ���������̨֮���ͬ����ϵ��
 */

void ChassisTop(void)
{
    int16_t vx_channel, vy_channel;
    fp32 sin_yaw = 0., cos_yaw = 0;
	fp32 Vx_rot;fp32 Vy_rot;

    // ��ȡң����ͨ�����룬Ӧ����������
    rc_deadband_limit(ChassisData.RC_data->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    rc_deadband_limit(ChassisData.RC_data->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);

	

    // ����ң����������������ٶ�
    vx_channel = vx_channel * CHASSIS_RC_SEN*2.0;
    vy_channel = -vy_channel * CHASSIS_RC_SEN*2.0;

	

    // ���ݼ���������������ٶ�
    if (ChassisData.RC_data->key.v & CHASSIS_FRONT_KEY)
    {
        vx_channel = KEYBOARD_TOP_SPEED_MAX;
    }
    else if (ChassisData.RC_data->key.v & CHASSIS_BACK_KEY)
    {
        vx_channel = -KEYBOARD_TOP_SPEED_MAX;
    }
    if (ChassisData.RC_data->key.v & CHASSIS_LEFT_KEY)
    {
        vy_channel = KEYBOARD_TOP_SPEED_MAX;
    }
    else if (ChassisData.RC_data->key.v & CHASSIS_RIGHT_KEY)
    {
        vy_channel = -KEYBOARD_TOP_SPEED_MAX;
    }

    // ������̨ƫ���Ƕ�Ӧ�����Һ�����ֵ
    ChassisData.ChassisFollowGimbalAngle = 0; // 直接使用遥控器数据，不需要从双板通信获取yaw_ecd和gimbal_angle
    sin_yaw = arm_sin_f32(-ChassisData.ChassisFollowGimbalAngle+ChassisData.Wzset*v_k);
    cos_yaw = arm_cos_f32(-ChassisData.ChassisFollowGimbalAngle+ChassisData.Wzset*v_k);
   wx_set.max_value = TOP*SPEED_TOP;
	    ramp_calc(&wx_set, 0.5f);
	ChassisData.Wzset = TOP*SPEED_TOP;
    ChassisData.VxSet = cos_yaw * vx_channel + sin_yaw * vy_channel;
    ChassisData.Vyset = -sin_yaw * vx_channel + cos_yaw * vy_channel;
    Chassis_PowerLimit_T();
}
fp32 k;
void chassis_rc_key()
{
//int16_t vx_channel, vy_channel, vz_channel;
    fp32 sin_yaw = 0., cos_yaw = 0.;
    
//    // ��ȡң����ͨ�����룬Ӧ����������
//    rc_deadband_limit(ChassisData.RCData->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
//    rc_deadband_limit(ChassisData.RCData->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);
//    
//    // ����ң����������������ٶ�
//    vx_channel = vx_channel * CHASSIS_VX_RC_SEN;
//    vy_channel = -vy_channel * CHASSIS_VX_RC_SEN;

    // ǰ��
if (ChassisData.RCData->key.v & CHASSIS_FRONT_KEY) {
    wx_set.max_value = 3.0;
    ChassisData.vx_target = KEYBOARD_SPEED_MAX; 	// ����ǰ��Ŀ���ٶ�
	
	if(ChassisData.RCData->key.v & CHASSIS_LEFT_KEY)
	{
		  wy_set.max_value = 3.0;
    ChassisData.vy_target = KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
	}
	
	else if (ChassisData.RCData->key.v & CHASSIS_RIGHT_KEY) {
	    wy_set.max_value = -3.0;
    ChassisData.vy_target = -KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
	
}
	else 
	{
		 wy_set.max_value = 0;
		ChassisData.vy_target=0;
	}
}
// ����
else if (ChassisData.RCData->key.v & CHASSIS_BACK_KEY) {
    wx_set.max_value = -3.0;
    ChassisData.vx_target = -KEYBOARD_SPEED_MAX;  // ���ú���Ŀ���ٶ�

	if(ChassisData.RCData->key.v & CHASSIS_LEFT_KEY)
	{
		  wy_set.max_value = 3.0;
    ChassisData.vy_target = KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
	}
	
	else if (ChassisData.RCData->key.v & CHASSIS_RIGHT_KEY) {
	    wy_set.max_value = -3.0;
    ChassisData.vy_target = -KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
	
}
}
// ����
else if (ChassisData.RCData->key.v & CHASSIS_LEFT_KEY) {
	    wy_set.max_value = 3.0;
    ChassisData.vy_target = KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
}
// ����
else if (ChassisData.RCData->key.v & CHASSIS_RIGHT_KEY) {
	    wy_set.max_value = -3.0;
    ChassisData.vy_target = -KEYBOARD_SPEED_MAX;  // ��������Ŀ���ٶ�
	
}
//else if (vx_channel != 0 || vy_channel != 0)

//{
//	if(vx_channel>0)
//	{
//		wx_set.max_value = 30.0;
//		ChassisData.vx_target=0.5;
//	}
//	else if(vx_channel<0)
//	{
//		wx_set.max_value = -30;
//		ChassisData.vx_target=-0.5;
//	}
//	else if(vy_channel>0)
//	{
//		wy_set.max_value = 30.0;
//		ChassisData.vy_target=0.5;
//	}
//	else if(vy_channel<0)
//	{
//		wy_set.max_value = -30.0;
//		ChassisData.vy_target=-0.5;
//	}
	else {
    wx_set.max_value = 0;  // �ް���ʱ��ǰ���ٶ�Ϊ��
    wy_set.max_value = 0;  // �ް���ʱ�������ٶ�Ϊ��
	ChassisData.vy_target=0;
		ChassisData.vx_target=0;
	k=0;
}

}

// �ް�������ʱ���ٶ�����Ϊ��
 

/**
 * @brief ���̸�������˶����ƺ�����
 *
 * �ú�������ң��������ͼ��̰���״̬�����Ƶ��̸�������˶���
 *
 * @note �ú���������̺͵���֮�����ͬ����ϵ���������˶���
 */
#define DEGREE_2_RAD PI/180
#define SQRT2 1
#define SQRT4 sqrt(2)/4
#define chassis_current 16864/20.0
fp32 hao1,hao2,hao3,hao4 ,hao5;
extern fp32 wheel_speed_yu[4];

 fp32 wheel_rpm_ratio = Radius_wheel;
void chassis_vector_to_mecanum_wheel_speed(const fp32 vx_set, const fp32 vy_set, const fp32 wz_set, fp32 wheel_speed[4])
{
		wheel_speed[0] =10*(-SQRT2*vx_set + SQRT2*vy_set + wz_set * L1_wheel)/wheel_rpm_ratio;
		wheel_speed[1] =10*( -SQRT2*vx_set - SQRT2*vy_set + wz_set * L2_wheel)/wheel_rpm_ratio;
    wheel_speed[2] = 10*(+SQRT2*vx_set - SQRT2*vy_set + wz_set * L3_wheel)/wheel_rpm_ratio;
    wheel_speed[3] = 10*(+SQRT2*vx_set+ SQRT2*vy_set + wz_set * L4_wheel)/wheel_rpm_ratio;	
}
//����Ť��ǰ��
void chassis_moment_feedforward(fp32 wheel_fn[4],fp32 wheel_FN[4])
{
	fp32 THEDA[4];
	fp32 FN[4];
	fp32 fsh=arm_sin_f32(ChassisData.chassis_imu_date.chassis_pitch)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_pitch)+pow(-sin(ChassisData.chassis_imu_date.chassis_roll)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_pitch),2);
	float BS=atan2(-sqrt(fsh),arm_cos_f32(ChassisData.chassis_imu_date.chassis_pitch)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_roll));//������б��
	float chassis_angle=arm_sin_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_roll)+arm_sin_f32(ChassisData.chassis_imu_date.chassis_pitch)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_yaw);
	float AS=acos(chassis_angle/arm_sin_f32(BS));
	float set_angle=fabs(ChassisData.chassis_imu_date.chassis_yaw)-AS;
	float FN_X=robot_weight*GRAVITY*arm_sin_f32(BS)*arm_cos_f32(set_angle);
	float FN_Y=robot_weight*GRAVITY*arm_sin_f32(BS)*arm_sin_f32(set_angle);
	fp32 fn=robot_weight*GRAVITY*arm_sin_f32(BS);
	fp32 high=0.163024/arm_cos_f32(BS);
	fp32 P_x=high*(arm_sin_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_roll)+arm_cos_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_pitch)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_roll));
	fp32 P_y=high*(-arm_cos_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_roll)+arm_sin_f32(ChassisData.chassis_imu_date.chassis_yaw)*arm_sin_f32(ChassisData.chassis_imu_date.chassis_pitch)*arm_cos_f32(ChassisData.chassis_imu_date.chassis_roll));
	if(P_x>=0.14744)
	{
		P_x=0.14;
	}
	if(P_x<=-0.14744)
	{
		P_x=-0.14;
	}
		if(P_y<=-0.14744)
	{
		P_y=-0.14;
	}
	if(P_y>=0.14744)
	{
		P_y=0.14;
	}	

	fp32 r0_x=0.14744;
	fp32 r1_x=-0.14744;
	fp32 r2_x=-0.14744;
	fp32 r3_x=0.14744;
	fp32 r0_y=0.14744;
	fp32 r1_y=0.14744;
	fp32 r2_y=-0.14744;
	fp32 r3_y=-0.14744;
	fp32 L0=sqrt(pow(r0_x-P_x,2)+pow(r0_y-P_y,2));
	fp32 L1=sqrt(pow(r1_x-P_x,2)+pow(r1_y-P_y,2));
	fp32 L2=sqrt(pow(r2_x-P_x,2)+pow(r2_y-P_y,2));
	fp32 L3=sqrt(pow(r3_x-P_x,2)+pow(r3_y-P_y,2));
	fp32 N0=(robot_weight*GRAVITY*arm_cos_f32(BS)*L1*L2*L3)/(L1*L2*L3+L0*L2*L3+L0*L1*L3+L0*L1*L2);
	fp32 N1=N0*L0/L1;
	fp32 N2=N0*L0/L2;
	fp32 N3=N0*L0/L3;
		
	 THEDA[0]=N0/(N0+N1+N2+N3);
	 THEDA[1]=N1/(N0+N1+N2+N3);
	 THEDA[2]=N2/(N0+N1+N2+N3);
	 THEDA[3]=N3/(N0+N1+N2+N3);
	 for(uint8_t i=0;i<4;i++)
		{
		FN[i]=robot_weight*GRAVITY*arm_sin_f32(BS)*THEDA[i];
		}
	fp32 FN0_x=(FN[0])*arm_cos_f32(set_angle);
	fp32 FN0_y=(FN[0])*arm_sin_f32(set_angle);
	fp32 FN1_x=(FN[1])*arm_cos_f32(set_angle);
	fp32 FN1_y=(FN[1])*arm_sin_f32(set_angle);
	fp32 FN2_x=(FN[2])*arm_cos_f32(set_angle);
	fp32 FN2_y=(FN[2])*arm_sin_f32(set_angle);
	fp32 FN3_x=(FN[3])*arm_cos_f32(set_angle);
	fp32 FN3_y=(FN[3])*arm_sin_f32(set_angle);
	
	wheel_FN[0]=(((-SQRT4*(FN_X))*Radius_wheel+(SQRT4*(FN_Y))*Radius_wheel)/0.3)*chassis_current;
  wheel_FN[1]=(((-SQRT4*(FN_X))*Radius_wheel+(SQRT4*(FN_Y))*Radius_wheel)/0.3)*chassis_current;
  wheel_FN[2]=(((SQRT4*(FN_X))*Radius_wheel+(-SQRT4*(FN_Y))*Radius_wheel)/0.3)*chassis_current;
  wheel_FN[3]=(((SQRT4*(FN_X))*Radius_wheel+(-SQRT4*(FN_Y))*Radius_wheel)/0.3)*chassis_current;

	wheel_fn[0]=(((-SQRT4*(FN0_x))*Radius_wheel+(SQRT4*(FN0_y))*Radius_wheel)/0.3)*chassis_current;
  wheel_fn[1]=(((-SQRT4*(FN1_x))*Radius_wheel+(SQRT4*(FN1_y))*Radius_wheel)/0.3)*chassis_current;
  wheel_fn[2]=(((SQRT4*(FN2_x))*Radius_wheel+(-SQRT4*(FN2_y))*Radius_wheel)/0.3)*chassis_current;
  wheel_fn[3]=(((SQRT4*(FN3_x))*Radius_wheel+(-SQRT4*(FN3_y))*Radius_wheel)/0.3)*chassis_current;
	for(uint8_t i=0;i<4;i++)
	{
		if(fabs(wheel_FN[i])>=5000)
		{
			wheel_FN[0]=5000;
			wheel_FN[3]=5000;
			wheel_FN[1]=-5000;
			wheel_FN[2]=-5000;
		}
		if(wheel_fn[3]>=900)
		{
			wheel_fn[3]=900;
		}
	}

}
/**
 * @brief ���̸�����̨�˶����ƺ�����
 *
 * �ú�������ң��������ͼ��̰���״̬�����Ƶ��̸�����̨�˶���
 *
 * @note �ú���������̺���̨֮�����ͬ����ϵ������̨�Ƕȶ�Ӧ�����˶���
 */
	fp32 Key_ramp_x;
fp32 Key_ramp_y;
fp32 Chassis_angle;
extern fp32 wheel_feedward,wheel_feedward2;
int KEYBOARD_FLLOW_GIMBAL_SPEED_MAX=0;

void ChassisFollowGimbal(void)
{
    int16_t vx_channel, vy_channel,vz_channel;/*angle_set = 0.0f*/
    fp32 sin_yaw = 0., cos_yaw = 0;

    // ��ȡң����ͨ�����룬Ӧ����������
    rc_deadband_limit(ChassisData.RC_data->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    rc_deadband_limit(ChassisData.RC_data->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);
    

    // ����ң����������������ٶ�
    ChassisData.VxSet = vx_channel * CHASSIS_VX_RC_SEN*5;
    ChassisData.Vyset = -vy_channel * CHASSIS_VX_RC_SEN*5;
   
	if(ChassisData.RC_data->key.v&KEY_PRESSED_OFFSET_F)
	{
		KEYBOARD_FLLOW_GIMBAL_SPEED_MAX=60;
	}
	else
	{
		KEYBOARD_FLLOW_GIMBAL_SPEED_MAX=30;
	}

//	if (ChassisData.RC_data->key.v & CHASSIS_FRONT_KEY)
//    {
//         ChassisData.VxSet = KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
//    }
//    else if (ChassisData.RC_data->key.v & CHASSIS_BACK_KEY)
//    {
//         ChassisData.VxSet = -KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
//    }
//    if (ChassisData.RC_data->key.v & CHASSIS_LEFT_KEY)
//    {
//         ChassisData.Vyset = KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
//    }
//    else if (ChassisData.RC_data->key.v & CHASSIS_RIGHT_KEY)
//    {
//         ChassisData.Vyset = -KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
//    }
  
	    if (ChassisData.RC_data->key.v & CHASSIS_FRONT_KEY)
    {
		ramp_calc(&wx_set,5);
		wx_set.max_value=KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
         ChassisData.VxSet = wx_set.out;
    }
    else if (ChassisData.RC_data->key.v & CHASSIS_BACK_KEY)
    {
		ramp_calc(&wx_set,5);
				wx_set.max_value=KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
         ChassisData.VxSet = -wx_set.out;
    }
    else if (ChassisData.RC_data->key.v & CHASSIS_LEFT_KEY)
    {
		ramp_calc(&wy_set,5);
		wy_set.max_value=KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;

         ChassisData.Vyset = wy_set.out;
    }
    else if (ChassisData.RC_data->key.v & CHASSIS_RIGHT_KEY)
    {
		ramp_calc(&wy_set,5);
				wy_set.max_value=KEYBOARD_FLLOW_GIMBAL_SPEED_MAX;
         ChassisData.Vyset = -wy_set.out;
    }
else 
{
	wx_set.max_value=0;
	wy_set.max_value=0;
	wx_set.out=0;
	wy_set.out=0;
}

    	ChassisData.ChassisFollowGimbalAngle = 0; // 直接使用遥控器数据，不需要从双板通信获取yaw_ecd和gimbal_angle
//    sin_yaw = arm_sin_f32(ChassisData.ChassisFollowGimbalAngle);
//    cos_yaw = arm_cos_f32(ChassisData.ChassisFollowGimbalAngle);

	//chassis_rc_key();
    // ������̨��Ե��̵ĽǶȱ仯������̵��ٶ�����
//			ramp_calc1(&wx_set,ChassisData.vx_target);
//			ramp_calc1(&wy_set,ChassisData.vy_target);
//				wx_set.max_value	= cos_yaw * vx_channel + sin_yaw * vy_channel;
//    wy_set.max_value = -sin_yaw * vx_channel + cos_yaw * vy_channel;            
        // �趨���̽Ƕ�Ŀ��ֵ
	ChassisData.chassis_angle_set = rad_format(angle_set);

//		ChassisData.VxSet=wx_set.out;
//		ChassisData.Vyset=wy_set.out;
			Chassis_angle=fmod(ChassisData.ChassisFollowGimbalAngle,2*PI);

PID_calc(&Chassis_yaw_speed,ChassisData.chassis_imu_date.chassis_yaw_speed,0); 

	if(Chassis_angle<-PI)
	{
		Chassis_angle=Chassis_angle+2*PI;
	}
	else if(Chassis_angle>PI)
	{
		Chassis_angle=Chassis_angle-2*PI;
	}    

//			if(fabs(Chassis_angle)>PI/36)
//	{

		ChassisData.Wzset = -30* PID_calc(&ChassisFollow_PID, Chassis_angle, ChassisData.chassis_angle_set)-5*Chassis_yaw_speed.out;
//				ChassisData.Wzset = -25* PID_calc(&ChassisFollow_PID, Chassis_angle, ChassisData.chassis_angle_set);

//	}
//	else
//	{
//		ChassisData.Wzset=0;
//	}
//	if(fabs(ChassisData.VxSet)>0)
//	{
//		
//	}
//			if(fabs(Chassis_angle)>PI/12)
//	{
//		
//		ChassisData.Wzset = - (PID_calc(&ChassisFollow_PID, Chassis_angle, ChassisData.chassis_angle_set)-Chassis_yaw_speed.out);  
//	}
//	else
//	{
//		ChassisData.Wzset=0-Chassis_yaw_speed.out;
//	}


    Chassis_PowerLimit_T();
						chassis_moment_feedforward(&wheel_feedward,&wheel_feedward2);

}

/**
 * @brief ��ȡ��������ָ��ĺ�����
 *
 * �ú�������ָ��������ݽṹ ChasisData_t ��ָ�룬���ڷ��ʵ��̵ĸ������ݡ�
 *
 * @return ChasisData_t �ṹ��ָ�룬ָ��������ݡ�
 */
ChasisData_t *GetChassisData(void)
{
    // ���ص������ݽṹ��ָ��
    return &ChassisData;
}

const ChasisData_t *chassis_data(void)
{
    return &ChassisData;
}


