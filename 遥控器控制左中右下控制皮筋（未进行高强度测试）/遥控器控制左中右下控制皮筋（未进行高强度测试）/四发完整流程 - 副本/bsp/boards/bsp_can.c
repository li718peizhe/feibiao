#include "bsp_can.h"
#include "main.h"
#include "can.h"      

extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
//static CAN_TxHeaderTypeDef chassis_tx_message;
//static uint8_t chassis_GM3508_send_data[8];

void can_filter_init(void)
{

    CAN_FilterTypeDef can_filter_st;
    can_filter_st.FilterActivation = ENABLE;
    can_filter_st.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter_st.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter_st.FilterIdHigh = 0x0000;
    can_filter_st.FilterIdLow = 0x0000;
    can_filter_st.FilterMaskIdHigh = 0x0000;
    can_filter_st.FilterMaskIdLow = 0x0000;
    can_filter_st.FilterBank = 0;
    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter_st);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);


    can_filter_st.SlaveStartFilterBank = 14;
    can_filter_st.FilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan2, &can_filter_st);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING);
}


//void CAN_Cmd3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
//{
//  uint32_t send_mail_box;
//  chassis_tx_message.StdId = CAN_Wheel_ALL_ID;
//  chassis_tx_message.IDE = CAN_ID_STD;
//  chassis_tx_message.RTR = CAN_RTR_DATA;
//  chassis_tx_message.DLC = 0x08;
//  chassis_GM3508_send_data[0] = motor1 >> 8;
//  chassis_GM3508_send_data[1] = motor1;
//  chassis_GM3508_send_data[2] = motor2 >> 8;
//  chassis_GM3508_send_data[3] = motor2;
//  chassis_GM3508_send_data[4] = motor3 >> 8;
//  chassis_GM3508_send_data[5] = motor3;
//  chassis_GM3508_send_data[6] = motor4 >> 8;
//  chassis_GM3508_send_data[7] = motor4;

//  HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_GM3508_send_data, &send_mail_box);
//}


