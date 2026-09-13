#ifndef BSP_SERVO_PWM_H
#define BSP_SERVO_PWM_H
#include "struct_typedef.h"

extern void servo_pwm_set(uint16_t pwm, uint8_t i);

extern void servo_set_angle(uint8_t angle, uint8_t servo_id);
extern void servo_set_mid(uint8_t servo_id);
#endif
