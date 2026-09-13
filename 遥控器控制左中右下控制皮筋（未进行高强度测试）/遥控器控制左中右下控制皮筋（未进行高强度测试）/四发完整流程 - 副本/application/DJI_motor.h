#ifndef DJI_MOTOR_H
#define DJI_MOTOR_H
#include "struct_typedef.h"


extern void CAN_cmd_chassis(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
extern void CAN_cmd_gimbal(int16_t yaw, int16_t pitch, int16_t NO, int16_t rev);
extern void CAN_CmdShoot(int16_t right, int16_t left, int16_t Trigger);

#endif
