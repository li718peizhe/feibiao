#ifndef CHASSIS_POWER_CONTROL_H
#define CHASSIS_POWER_CONTROL_H
#include "chassis_task.h"
#include "main.h"
#include "remote_control.h"

typedef struct
{
    const motor_measure_t *ChassisMotorData[4];
    const RC_ctrl_t* RCData;
     ChasisData_t *ChassisData_L;

}ChassisPowerControl_t;
#define M3508_MOTOR_RPM_TO_VECTOR 0.000415809748903494517209f
#define CHASSIS_MOTOR_RPM_TO_VECTOR_SEN M3508_MOTOR_RPM_TO_VECTOR
#define error_powerDistribution_set 37000.0f
#define prop_powerDistribution_set 11000.0f


void LowPass_SetChassis(float* output,float In);
extern void Chassis_PowerLimit_T(void);
extern float int16_to_float(int16_t a, int16_t a_max, int16_t a_min, float b_max, float b_min);
extern uint32_t Power_UI;
#endif


