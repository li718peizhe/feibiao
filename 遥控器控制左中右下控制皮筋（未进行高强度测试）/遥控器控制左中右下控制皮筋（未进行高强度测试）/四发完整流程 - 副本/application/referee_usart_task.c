/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       referee_usart_task.c/h
  * @brief      RM referee system data solve. RM����ϵͳ���ݴ���
  * @note       
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. done
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "referee_usart_task.h"
#include "main.h"
#include "cmsis_os.h"
#include "RM_Cilent_UI.h"
#include "bsp_usart.h"
#include "remote_control.h"
#include "CAN_receive.h"
#include "CRC8_CRC16.h"
#include "fifo.h"
#include "protocol.h"
#include "referee.h"
#include "ui_g.h"

/*USART6*/


/**
  * @brief          single byte upacked 
  * @param[in]      void
  * @retval         none
  */
/**
  * @brief          ���ֽڽ��
  * @param[in]      void
  * @retval         none
  */
static void referee_unpack_fifo_data(void);
extern UART_HandleTypeDef huart6;

uint8_t usart6_buf[2][USART_RX_BUF_LENGHT];

fifo_s_t referee_fifo;
uint8_t referee_fifo_buf[REFEREE_FIFO_BUF_LENGTH];
unpack_data_t referee_unpack_obj;
USART_Data_t USART_Data;

// 调试变量
volatile uint32_t referee_task_counter = 0;
volatile uint32_t referee_fifo_data_count = 0;
volatile uint32_t fifo_total_count = 0;
volatile uint8_t fifo_raw_data[64] = {0};
volatile uint8_t fifo_raw_index = 0;
volatile uint32_t fifo_sof_count = 0;
void UI_Init(void)
{
	_ui_init_g_static_0();osDelay(30);
    _ui_init_g_static_1();osDelay(30);

	
	_ui_init_g_dynamic_CD_0();osDelay(30);
	_ui_init_g_dynamic_Fire_0();osDelay(30);
	_ui_init_g_dynamic_chassis_condition_0();osDelay(1000);
	_ui_init_g_dynamic_recognize_armour_0();osDelay(30);
}

void UI_updata(void)
{
	_ui_update_g_dynamic_CD_0();  //����ʣ������
	osDelay(25);
	_ui_update_g_dynamic_Fire_0();  //����
	osDelay(25);
	_ui_update_g_dynamic_chassis_condition_0();  //��̨-����״̬
	osDelay(25);
	_ui_update_g_dynamic_recognize_armour_0();  //ʶ��װ�װ�
	osDelay(25);
}

/**
  * @brief          referee task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
/**
  * @brief          ����ϵͳ����
  * @param[in]      pvParameters: NULL
  * @retval         none
  */

///////////////////////////
//Graph_Data G1,G2,G3,G4,G5,G6,G7,G8,G9,G10,G11,G12;
//Graph_Data Graph_Data_t;
//USART_Data_t usart_RC;
//char shoot_arr[5]="shoot";
//char flrb_arr[4]="FRBL";
//draw_fang cap1;
//draw_Yu2 cap2;
//draw_shoot shoot;
//draw_Yu YU;
//draw_fire fire;
/**
  * @brief          referee task
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
/**
  * @brief          ����ϵͳ����
  * @param[in]      pvParameters: NULL
  * @retval         none
  */
//	//��������ͼ����ٹ۲��Ƿ������ˢ�±�죡����
//	uint16_t UI_UPDATE=0;
//    extern uint32_t COLOR;

void referee_usart_task(void const * argument)
{
	//		uint8_t updata=0;
	
    init_referee_struct_data();
    fifo_s_init(&referee_fifo, referee_fifo_buf, REFEREE_FIFO_BUF_LENGTH);
    usart6_init(usart6_buf[0], usart6_buf[1], USART_RX_BUF_LENGHT);
			UI_Init();
//			shoot_speed(&shoot);
////		shoot_left(&shoot);

    while(1)
    {
		referee_task_counter++;
		referee_fifo_data_count = fifo_s_used(&referee_fifo);
		referee_unpack_fifo_data();
			USART_Data.RCData= Get_Rcdata();
		UI_updata();//UI����
		robot_id();
			if(USART_Data.RCData->key.v&KEY_PRESSED_OFFSET_B)
			{
			UI_Init();
			}
		osDelay(2);
		
		
//		updata++;
//		usart_RC.RCData = Get_Rcdata();
//		FANG_UI(&cap1);
//		YU_Lucky3_UI(&cap2);
//		fire_right(&fire);
//	if(usart_RC.RCData->key.v& KEY_PRESSED_OFFSET_V)
//	{
//		shoot_speed(&shoot);				
//		shoot_left(&shoot);
//		shoot_right(&shoot);
//	}				
//	if(updata%2==0)
//	{
//		YU_Lucky_UI(&YU);
//		vision_state(&fire);

//	}
	}
}
	
 int robot_ID;
 int robot_man_ID;
extern uint8_t Vision_ID,red_outpost,blue_outpost;
void robot_id(void)
{
    switch(robot_state.robot_id)
    {
    case 1:
        robot_ID = 1;
        robot_man_ID = 0X0101;
        Vision_ID = 1;

        break;
    case 2:
        robot_ID = 2;
        robot_man_ID = 0X0102;
        Vision_ID = 1;
        break;
    case 3:
        robot_ID = 3;
        robot_man_ID = 0X0103;
        Vision_ID = 1;
        break;
    case 4:
        robot_ID = 4;
        robot_man_ID = 0X0104;
        Vision_ID = 1;
        break;
    case 5:
        robot_ID = 5;
        robot_man_ID = 0X0105;
        Vision_ID = 1;
        break;
    case 7:
        robot_ID = 7;
        robot_man_ID = 0X0106;
        Vision_ID = 1;
		    red_outpost=1;
				blue_outpost=0;
        break;
    case 8:
        robot_ID = 8;
        robot_man_ID = 0;
        Vision_ID = 0;
        break;
    case 101:
        robot_ID = 101;
        robot_man_ID = 0X0165;
        Vision_ID = 1;
        break;
    case 102:
        robot_ID = 102;
        robot_man_ID = 0X0166;
        Vision_ID = 0;
        break;
    case 103:
        robot_ID = 103;
        robot_man_ID = 0X0167;
        Vision_ID = 0;
        break;
    case 104:
        robot_ID = 104;
        robot_man_ID = 0X0168;
        Vision_ID = 0;
        break;
    case 105:
        robot_ID = 105;
        robot_man_ID = 0X0169;
        Vision_ID = 0;
        break;
    case 107:
        robot_ID = 107;
        robot_man_ID = 0X016A;
        Vision_ID = 0;
				red_outpost=0;
				blue_outpost=1;
        break;
    case 108:
        robot_ID = 108;
        robot_man_ID = 0;
        Vision_ID = 0;
        break;
    }


}
/**
  * @brief          single byte upacked 
  * @param[in]      void
  * @retval         none
  */
/**
  * @brief          ���ֽڽ��
  * @param[in]      void
  * @retval         none
  */
void referee_unpack_fifo_data(void)
{
  uint8_t byte = 0;
  uint8_t sof = HEADER_SOF;
  unpack_data_t *p_obj = &referee_unpack_obj;

  while ( fifo_s_used(&referee_fifo) )
  {
    byte = fifo_s_get(&referee_fifo);
    
    // 调试：记录所有接收到的字节
    fifo_total_count++;
    fifo_raw_data[fifo_raw_index] = byte;
    fifo_raw_index = (fifo_raw_index + 1) % 64;
    
    if(byte == sof)
    {
        fifo_sof_count++;
    }
    switch(p_obj->unpack_step) 
    {
      case STEP_HEADER_SOF:
      {
        if(byte == sof)
        {
          p_obj->unpack_step = STEP_LENGTH_LOW;
          p_obj->protocol_packet[p_obj->index++] = byte;
        }
        else
        {
          p_obj->index = 0;
        }
      }break;
      
      case STEP_LENGTH_LOW:
      {
        p_obj->data_len = byte;
        p_obj->protocol_packet[p_obj->index++] = byte;
        p_obj->unpack_step = STEP_LENGTH_HIGH;
      }break;
      
      case STEP_LENGTH_HIGH:
      {
        p_obj->data_len |= (byte << 8);
        p_obj->protocol_packet[p_obj->index++] = byte;

        if(p_obj->data_len < (REF_PROTOCOL_FRAME_MAX_SIZE - REF_HEADER_CRC_CMDID_LEN))
        {
          p_obj->unpack_step = STEP_FRAME_SEQ;
        }
        else
        {
          p_obj->unpack_step = STEP_HEADER_SOF;
          p_obj->index = 0;
        }
      }break;
      case STEP_FRAME_SEQ:
      {
        p_obj->protocol_packet[p_obj->index++] = byte;
        p_obj->unpack_step = STEP_HEADER_CRC8;
      }break;

      case STEP_HEADER_CRC8:
      {
        p_obj->protocol_packet[p_obj->index++] = byte;

        if (p_obj->index == REF_PROTOCOL_HEADER_SIZE)
        {
          if ( verify_CRC8_check_sum(p_obj->protocol_packet, REF_PROTOCOL_HEADER_SIZE) )
          {
            p_obj->unpack_step = STEP_DATA_CRC16;
          }
          else
          {
            p_obj->unpack_step = STEP_HEADER_SOF;
            p_obj->index = 0;
          }
        }
      }break;  
      
      case STEP_DATA_CRC16:
      {
        if (p_obj->index < (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
        {
           p_obj->protocol_packet[p_obj->index++] = byte;  
        }
        if (p_obj->index >= (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
        {
          p_obj->unpack_step = STEP_HEADER_SOF;
          p_obj->index = 0;

          if ( verify_CRC16_check_sum(p_obj->protocol_packet, REF_HEADER_CRC_CMDID_LEN + p_obj->data_len) )
          {
             referee_data_solve(p_obj->protocol_packet);
          }
        }
      }break;

      default:
      {
        p_obj->unpack_step = STEP_HEADER_SOF;
        p_obj->index = 0;
      }break;
    }
  }
}


void USART6_IRQHandler(void)
{
    static volatile uint8_t res;

	if(USART6->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart6);

        static uint16_t this_time_rx_len = 0;

        if ((huart6.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char*)usart6_buf[0], this_time_rx_len);

        }
        else
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char*)usart6_buf[1], this_time_rx_len);
        }
    }
	HAL_UART_IRQHandler(&huart6);
}

