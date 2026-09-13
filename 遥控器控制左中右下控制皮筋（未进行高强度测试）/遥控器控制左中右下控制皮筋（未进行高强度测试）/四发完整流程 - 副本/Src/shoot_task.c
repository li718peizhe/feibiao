#include "shoot_task.h"
#include "Filter.h"
#include "bsp_laser.h"
//#include "Vision_Task.h"

#define shoot_laser_on() laser_on()   // ���⿪���궨��
#define shoot_laser_off() laser_off() // ����رպ궨��
ShootControl_t ShootControl;
extern shoot_data_t shoot_data;
Low_Pass_Filter_t ShootMotor_Left, ShootMotor_Right;

int ShotTime = 0;
int firetime = 0;
pid_type_def trigger_speed_motor_pid, trigger_Angle_motor_pid;
pid_type_def Fire_left_motor_pid;
pid_type_def Fire_right_motor_pid;
pid_type_def FireLeftMoment;
pid_type_def FireRightMoment;



fp32 Fire_left_speed_pid[3] = {13.0, 0.0, 10.0};
fp32 Fire_right_speed_pid[3] = {13.0, 0.0, 10.0};
fp32 FireLeftMomentPID[3] = {0.5, 0., 0.3};
fp32 FireRightMomentPID[3] = {0.5, 0., 0.3};
const fp32 TriggerAnglePID[3] = {TRIGGER_ANGLE_PID_KP, TRIGGER_ANGLE_PID_KI, TRIGGER_ANGLE_PID_KD};
const fp32 TriggerSpeedPID[3] = {TRIGGER_SPEED_PID_KP, TRIGGER_SPEED_PID_KI, TRIGGER_SPEED_PID_KD};

void ShootInit(void)
{
    ShootControl.ShootMode = SHOOT_STOP;
    ShootControl.last_rc_s = RC_SW_UP;
    ShootControl.last_rc_T = RC_SW_DOWN;

    ShootControl.LeftFireData = get_shoot_fire_left_point();
    ShootControl.RightFireData = get_shoot_fire_right_point();
    ShootControl.TriggerData = get_shoot_trigger_measure_point();
    ShootControl.RC_data = get_remote_control_point();

    PID_init(&trigger_speed_motor_pid, PID_POSITION, TriggerSpeedPID, BULLET_ANG_PID_MAX_OUT, BULLET_ANG_PID_MAX_IOUT,0);
    PID_init(&trigger_Angle_motor_pid, PID_POSITION, TriggerAnglePID, BULLET_SPEED_PID_MAX_OUT, BUFFET_SPEED_PID_MAX_IOUT,0);

    PID_init(&Fire_left_motor_pid, PID_POSITION, Fire_left_speed_pid, FIRE_left_SPEED_PID_MAX_OUT, FIRE_left_SPEED_PID_MAX_IOUT,0);
    PID_init(&Fire_right_motor_pid, PID_POSITION, Fire_right_speed_pid, FIRE_right_SPEED_PID_MAX_OUT, FIRE_right_SPEED_PID_MAX_IOUT,0);
    PID_init(&FireLeftMoment, PID_POSITION, FireLeftMomentPID, FIRE_MOMENT_PID_MAX_OUT, FIRE_MOMENT_PID_MAX_IOUT,0);
    PID_init(&FireRightMoment, PID_POSITION, FireRightMomentPID, FIRE_MOMENT_PID_MAX_OUT, FIRE_MOMENT_PID_MAX_IOUT,0);

    Low_Pass_Filter_Init(&ShootMotor_Left, 2, 1.6);
    Low_Pass_Filter_Init(&ShootMotor_Right, 2, 1.6);
}

void ShootDataUpdata(void)
{
    ShootControl.LeftFireData = get_shoot_fire_left_point();
    ShootControl.RightFireData = get_shoot_fire_right_point();
    ShootControl.TriggerData = get_shoot_trigger_measure_point();
    ShootControl.RC_data = get_remote_control_point();
}

void ShootTask(void const *pvParameters)
{
    vTaskDelay(2);
    ShootInit();
    while (1)
    {
        Set_ShootMode();
        ShootDataUpdata();
        Set_ShootVal();

        Shoot_Heatlimit();
        vTaskDelay(1);
    }
}

int8_t LAST_SHOOT_OFF_KEYBOARD = 0;

void Set_ShootMode(void)
{
    if ((switch_is_up(ShootControl.RC_data->rc.s[RC_Left]) && switch_is_up(ShootControl.RC_data->rc.s[RC_Right]) && !switch_is_up(ShootControl.last_rc_s) && ShootControl.ShootMode == SHOOT_STOP) || ((ShootControl.RC_data->key.v & SHOOT_OPEN_FIRE) && (!(LAST_SHOOT_OFF_KEYBOARD & SHOOT_OPEN_FIRE)) && (ShootControl.ShootMode == SHOOT_STOP)))

	{
        ShootControl.ShootMode = SHOOT_STAR;
    }
    else if ((switch_is_up(ShootControl.RC_data->rc.s[RC_Left]) && !switch_is_up(ShootControl.last_rc_s) && ShootControl.ShootMode != SHOOT_STOP && switch_is_up(ShootControl.RC_data->rc.s[RC_Right])) || ((ShootControl.RC_data->key.v & SHOOT_OPEN_FIRE) && !(LAST_SHOOT_OFF_KEYBOARD & SHOOT_OPEN_FIRE) && (ShootControl.ShootMode != SHOOT_STOP)))
    {
        ShootControl.ShootMode = SHOOT_STOP;
        ShotTime = 0;
    }
    ShootControl.last_rc_s = ShootControl.RC_data->rc.s[RC_Left];
    LAST_SHOOT_OFF_KEYBOARD = ShootControl.RC_data->key.v;
//  static int8_t last_s = RC_SW_UP;
//	static uint16_t last_BUZZER = 0;


//  if ((switch_is_up(ShootControl.RC_data->rc.s[RC_Left]) && !switch_is_up(last_s) && ShootControl.ShootMode == SHOOT_STOP) || ((ShootControl.RC_data->key.v & SHOOT_OPEN_FIRE) && (!(LAST_SHOOT_OFF_KEYBOARD & SHOOT_OPEN_FIRE)) && (ShootControl.ShootMode == SHOOT_STOP)))
//  {
//   ShootControl.ShootMode = SHOOT_STAR;

//  }
//  else if ((switch_is_up(ShootControl.RC_data->rc.s[RC_Left]) && !switch_is_up(last_s) && ShootControl.ShootMode != SHOOT_STOP) || ((ShootControl.RC_data->key.v & SHOOT_OPEN_FIRE) && !(LAST_SHOOT_OFF_KEYBOARD & SHOOT_OPEN_FIRE) && ShootControl.ShootMode != SHOOT_STOP))
//  {
// ShootControl.ShootMode = SHOOT_STOP;

//  }
}

void Shoot_PIDCalc(void)
{

    Low_Pass_Filter_OUT(&ShootMotor_Left, ShootControl.LeftFireData->speed_rpm);
    Low_Pass_Filter_OUT(&ShootMotor_Right, ShootControl.RightFireData->speed_rpm);
#if Normal_Mode
    ShootControl.give_current[0] = PID_calc(&Fire_right_motor_pid, ShootControl.RightFireData->speed_rpm, ShootControl.Fire_right_speed_set);
    ShootControl.give_current[1] = PID_calc(&Fire_left_motor_pid, ShootControl.LeftFireData->speed_rpm, ShootControl.Fire_left_speed_set);
#else
    PID_calc(&Fire_right_motor_pid, ShootControl.RightFireData->speed_rpm, ShootControl.Fire_right_speed_set);
    PID_calc(&Fire_left_motor_pid, ShootControl.LeftFireData->speed_rpm, ShootControl.Fire_left_speed_set);

    ShootControl.give_current[0] = PID_calc(&FireRightMoment, ShootControl.RightFireData->given_current, Fire_right_motor_pid.out);
    ShootControl.give_current[1] = PID_calc(&FireLeftMoment, ShootControl.LeftFireData->given_current, Fire_left_motor_pid.out);

#endif

    PID_calc(&trigger_Angle_motor_pid, (fp32)ShootControl.TriggerData->total_angle, (fp32)ShootControl.TriggerMotor_pos_set);
    ShootControl.give_current[2] = PID_calc(&trigger_speed_motor_pid, ShootControl.TriggerData->speed_rpm, trigger_Angle_motor_pid.out);


}
int8_t  LAST_VISION_SHOOT;
uint16_t MAX=40;
int16_t OPEN_LIMIT;
void Set_ShootVal(void)
{
    switch (ShootControl.ShootMode)
    {
    case SHOOT_STOP:
        ShootControl.Fire_left_speed_set = 0;
        ShootControl.Fire_right_speed_set = 0;
        shoot_laser_off(); // ���⿪��
        ShootControl.TriggerMotor_pos_set = ShootControl.TriggerData->total_angle;
        Shoot_PIDCalc();
        break;
    case SHOOT_STAR:
        ShootControl.Fire_left_speed_set = -7200.0f;
        ShootControl.Fire_right_speed_set = 7200.0f;
        shoot_laser_on(); // ���⿪��
        LAST_VISION_SHOOT = ShootControl.RC_data->key.v;
        if ((!switch_is_down(ShootControl.last_rc_T) && switch_is_down(ShootControl.RC_data->rc.s[RC_Left]))||(ShootControl.RC_data->mouse.press_l&&ShootControl.LAST_PRESS_L==0))
        {
            ShootControl.TriggerMotor_pos_set += Single_Data;
        }
//        if (ShootControl.RC_data->rc.ch[4] > 600 ) /*||(shoot_control == 0)*/
//        {
//            ShotTime++;
//        }

        if ((ShotTime >= SHOOT_FULL_TIME && (shoot_data.shooter_17mm_1_barrel_heat + MAX <= shoot_data.shooter_barrel_heat_limit&&OPEN_LIMIT==1))|| (ShotTime >= SHOOT_FULL_TIME &&OPEN_LIMIT==0))
        {
           ShootControl.TriggerMotor_pos_set += Single_Data;
            ShotTime = 0;
        }
        if (UseVision())
        {
            firetime++;
        }
        if((shoot_control == 1) && (firetime >= SHOOT_CONTROL_TIME) &&  ((ShootControl.RC_data->key.v & SHOOT_VISION_FIRE) && (!(LAST_VISION_SHOOT & SHOOT_VISION_FIRE)) ) )
        {
            ShootControl.TriggerMotor_pos_set += Single_Data;
            firetime = 0;

        }
        ShootControl.last_rc_T = ShootControl.RC_data->rc.s[RC_Left];
				ShootControl.LAST_PRESS_L = ShootControl.RC_data->mouse.press_l;

        Shoot_PIDCalc();
        break;
    default:
        break;
    }
}

void Shoot_Heatlimit()
{
	if(ShootControl.RC_data->key.v&KEY_PRESSED_OFFSET_G)
	{
	OPEN_LIMIT=0;
	}
	else
	{
	 OPEN_LIMIT=1;
	}

}

