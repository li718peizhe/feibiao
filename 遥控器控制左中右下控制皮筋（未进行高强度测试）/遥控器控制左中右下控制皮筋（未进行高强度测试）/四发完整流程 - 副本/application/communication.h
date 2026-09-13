#ifndef COMMUNICATION_H
#define COMMUNICATION_H
#include "struct_typedef.h"

extern void CAN_CmdBoard(int16_t RC_CH1, int16_t RC_CH2, int16_t RC_CH3, uint8_t S_Left, uint8_t S_Right);
extern void CAN_CmdBoard1(int16_t RC_CH5, int16_t key, uint16_t gimbal_angle, uint16_t yaw_ecd);

#endif
