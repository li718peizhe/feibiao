#include "bsp_servo_pwm.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim8;

// ������Ĳ������ã���������180�������ɸ���ʵ�ʶ��������
#define SERVO_MIN_ANGLE    0       // �����С�Ƕ�
#define SERVO_MAX_ANGLE    180     // ������Ƕ�
#define SERVO_MIN_PWM      500     // 0���ӦPWMֵ��500us��50Hz���ڣ�
#define SERVO_MAX_PWM      2500    // 180���ӦPWMֵ��2500us��50Hz���ڣ�
#define SERVO_PWM_RANGE    (SERVO_MAX_PWM - SERVO_MIN_PWM)
#define SERVO_ANGLE_RANGE  (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE)

void servo_pwm_set(uint16_t pwm, uint8_t i)
{
    switch(i)
    {
        case 0:
        {
            __HAL_TIM_SetCompare(&htim8, TIM_CHANNEL_2, pwm);
        }break;
        case 1:
        {
            __HAL_TIM_SetCompare(&htim8, TIM_CHANNEL_1, pwm);
        }break;
        case 2:
        {
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, pwm);
        }break;
        case 3:
        {
            __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_3, pwm); 
        }break;
    }
}

// ����1��angle - Ŀ��Ƕȣ�0-180�㣩������2��servo_id - �����ţ�0-3��
void servo_set_angle(uint8_t angle, uint8_t servo_id)
{
    // 1. ����У�飺�������޷���0-3��
    if(servo_id > 3)
    {
        return; // �Ƿ����ֱ�ӷ��أ������������ͨ��
    }

    // 2. �Ƕ��޷���0-180�㣩�����ⳬ�������е��Χ
    if(angle < SERVO_MIN_ANGLE)
    {
        angle = SERVO_MIN_ANGLE;
    }
    else if(angle > SERVO_MAX_ANGLE)
    {
        angle = SERVO_MAX_ANGLE;
    }

    // 3. �Ƕ�תPWMֵ������ӳ�䣩
    uint16_t target_pwm = SERVO_MIN_PWM + (uint16_t)((float)angle / SERVO_ANGLE_RANGE * SERVO_PWM_RANGE);

    // 4. ����ԭ�к�������PWM�����ƶ����Ŀ��Ƕ�
    servo_pwm_set(target_pwm, servo_id);
}

// ������к��������ٽ����ת��90�㣩
void servo_set_mid(uint8_t servo_id)
{
    servo_set_angle(90, servo_id);
}
