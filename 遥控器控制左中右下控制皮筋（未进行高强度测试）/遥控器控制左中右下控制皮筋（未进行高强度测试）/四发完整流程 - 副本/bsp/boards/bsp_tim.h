#ifndef __BSP_TIM_H
#define __BSP_TIM_H

#include "main.h"
#include "CAN_receive.h"
#include "chassis_task.h"
#include "referee.h"
typedef struct
{
	const ChasisData_t* ChassisData;
	const projectile_allowance_t *projectile_allowance;	
}SystemCtrl_t;	

#endif
