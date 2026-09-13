#include "communication.h"
#include "cmsis_os.h"
#include "main.h"
#include "CAN_receive.h"
#include "bsp_rng.h"
#include "can.h"
#include "remote_control.h"

static uint8_t board_send_data[8];
static CAN_TxHeaderTypeDef board_tx_message;
static uint8_t chassis_can_send_data[8];
static uint8_t cap_can_send_data[8];
static CAN_TxHeaderTypeDef cap_tx_message;
#ifdef Gimbal
void CAN_CmdBoard(int16_t RC_CH1, int16_t RC_CH2, int16_t RC_CH3, uint8_t S_Left, uint8_t S_Right)
{
  uint32_t send_mail_box;

  board_tx_message.StdId = CAN_Board_ID;
  board_tx_message.IDE = CAN_ID_STD;
  board_tx_message.RTR = CAN_RTR_DATA;
  board_tx_message.DLC = 0x08;
  board_send_data[0] = RC_CH1 >> 8;
  board_send_data[1] = RC_CH1;
  board_send_data[2] = RC_CH2 >> 8;
  board_send_data[3] = RC_CH2;
  board_send_data[4] = RC_CH3 >> 8;
  board_send_data[5] = RC_CH3;
  board_send_data[6] = S_Left;
  board_send_data[7] = S_Right;
  HAL_CAN_AddTxMessage(&hcan2, &board_tx_message, board_send_data, &send_mail_box);
}

void CAN_CmdBoard1(int16_t RC_CH5, int16_t key, uint16_t gimbal_angle, uint16_t yaw_ecd)
{
  uint32_t send_mail_box;
  board_tx_message.StdId = CAN_Board2_ID;
  board_tx_message.IDE = CAN_ID_STD;
  board_tx_message.RTR = CAN_RTR_DATA;
  board_tx_message.DLC = 0x08;
  board_send_data[0] = RC_CH5 >> 8;
  board_send_data[1] = RC_CH5;
  board_send_data[2] = key >> 8;
  board_send_data[3] = key;
  board_send_data[4] = gimbal_angle >> 8;
  board_send_data[5] = gimbal_angle;
  board_send_data[6] = yaw_ecd >> 8;
  board_send_data[7] = yaw_ecd;
  HAL_CAN_AddTxMessage(&hcan2, &board_tx_message, board_send_data, &send_mail_box);
}
#endif

#ifdef Chassis
void CAN_CmdBoard2(int16_t data1, uint16_t data2, uint8_t data3 ,uint16_t data4)
	{
  uint32_t send_mail_box;
  board_tx_message.StdId = CAN_BoardRef;
  board_tx_message.IDE = CAN_ID_STD;
  board_tx_message.RTR = CAN_RTR_DATA;
  board_tx_message.DLC = 0x08;
  board_send_data[0] = data1 >> 8;
  board_send_data[1] = data1;
  board_send_data[2] = data2;
  board_send_data[3] = data3 >> 8;
  board_send_data[4] = data3;
  board_send_data[5] = data4 >> 8;
  board_send_data[6] = data4;
  board_send_data[7] = 0;

  HAL_CAN_AddTxMessage(&hcan2, &board_tx_message, board_send_data, &send_mail_box);
}
void CAN_cmd_cap_powerbuff( uint16_t chassis_power_buffer)
{
  uint32_t send_mail_box;
  cap_tx_message.StdId = 0x2E;
  cap_tx_message.IDE = CAN_ID_STD;
  cap_tx_message.RTR = CAN_RTR_DATA;
  cap_tx_message.DLC = 0x08;
  cap_can_send_data[0] = chassis_power_buffer>>8 ;
  cap_can_send_data[1] = chassis_power_buffer;
  cap_can_send_data[2] = 0;
  cap_can_send_data[3] = 0;
	cap_can_send_data[4] = 0;
  cap_can_send_data[5] = 0;
  cap_can_send_data[6] = 0;
  cap_can_send_data[7] = 0;

  HAL_CAN_AddTxMessage(&hcan1, &cap_tx_message, cap_can_send_data, &send_mail_box);
}
void CAN_cmd_cap_control( uint16_t chassis_power_limit, int16_t output_power_limit, int16_t input_power_limit, int16_t cap_control)
{
  uint32_t send_mail_box;
  cap_tx_message.StdId = 0x2F;
  cap_tx_message.IDE = CAN_ID_STD;
  cap_tx_message.RTR = CAN_RTR_DATA;
  cap_tx_message.DLC = 0x08;
  cap_can_send_data[0] = chassis_power_limit>>8 ;
  cap_can_send_data[1] = chassis_power_limit;
  cap_can_send_data[2] = output_power_limit>>8;
  cap_can_send_data[3] = output_power_limit;
	cap_can_send_data[4] = input_power_limit>>8;
  cap_can_send_data[5] = input_power_limit;
  cap_can_send_data[6] = cap_control>>8;
  cap_can_send_data[7] = cap_control;

  HAL_CAN_AddTxMessage(&hcan1, &cap_tx_message, cap_can_send_data, &send_mail_box);
}
#endif
