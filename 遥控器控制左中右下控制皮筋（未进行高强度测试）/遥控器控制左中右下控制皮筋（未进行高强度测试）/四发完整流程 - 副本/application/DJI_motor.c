#include "DJI_motor.h"
#include "cmsis_os.h"
#include "main.h"
#include "bsp_rng.h"
#include "CAN_receive.h"

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static uint8_t chassis_GM3508_send_data[8];
static uint8_t gimbal_can_send_data[8];
static uint8_t shoot_can_send_data[8];
#if defined(Chassis) || defined(Gimbal)
void CAN_cmd_chassis(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
{
	  uint32_t send_mail_box;
	 CAN_TxHeaderTypeDef chassis_tx_message;
  chassis_tx_message.StdId = CAN_Wheel_ALL_ID;
  chassis_tx_message.IDE = CAN_ID_STD;
  chassis_tx_message.RTR = CAN_RTR_DATA;
  chassis_tx_message.DLC = 0x08;
  chassis_GM3508_send_data[0] = motor1 >> 8;
  chassis_GM3508_send_data[1] = motor1;
  chassis_GM3508_send_data[2] = motor2 >> 8;
  chassis_GM3508_send_data[3] = motor2;
  chassis_GM3508_send_data[4] = motor3 >> 8;
  chassis_GM3508_send_data[5] = motor3;
  chassis_GM3508_send_data[6] = motor4 >> 8;
  chassis_GM3508_send_data[7] = motor4;

  HAL_CAN_AddTxMessage(&hcan1, &chassis_tx_message, chassis_GM3508_send_data, &send_mail_box);

}
#endif

#ifdef Gimbal
void CAN_cmd_gimbal(int16_t yaw, int16_t pitch, int16_t NO, int16_t rev)
{
  uint32_t send_mail_box;
	CAN_TxHeaderTypeDef gimbal_tx_message;
  gimbal_tx_message.StdId = CAN_GIMBAL_ALL_ID;
  gimbal_tx_message.IDE = CAN_ID_STD;
  gimbal_tx_message.RTR = CAN_RTR_DATA;
  gimbal_tx_message.DLC = 0x08;
  gimbal_can_send_data[0] = (yaw >> 8);
  gimbal_can_send_data[1] = yaw;
  gimbal_can_send_data[2] = (pitch >> 8);
  gimbal_can_send_data[3] = pitch;
  gimbal_can_send_data[4] = (NO >> 8);
  gimbal_can_send_data[5] = NO;
  gimbal_can_send_data[6] = (rev >> 8);
  gimbal_can_send_data[7] = rev;
  HAL_CAN_AddTxMessage(&hcan2, &gimbal_tx_message, gimbal_can_send_data, &send_mail_box);
}
void CAN_CmdShoot(int16_t right, int16_t left, int16_t Trigger)
{
  uint32_t send_mail_box;
		CAN_TxHeaderTypeDef shoot_tx_message;

  shoot_tx_message.StdId = CAN_SHOOT_ALL_ID;
  shoot_tx_message.IDE = CAN_ID_STD;
  shoot_tx_message.RTR = CAN_RTR_DATA;
  shoot_tx_message.DLC = 0x08;
  shoot_can_send_data[0] = right >> 8;
  shoot_can_send_data[1] = right;
  shoot_can_send_data[2] = left >> 8;
  shoot_can_send_data[3] = left;
  shoot_can_send_data[4] = Trigger >> 8;
  shoot_can_send_data[5] = Trigger;

  HAL_CAN_AddTxMessage(&hcan1, &shoot_tx_message, shoot_can_send_data, &send_mail_box);
}
#endif
