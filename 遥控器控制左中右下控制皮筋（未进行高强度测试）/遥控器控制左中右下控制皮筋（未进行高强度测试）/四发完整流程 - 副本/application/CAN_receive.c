#include "CAN_receive.h"
#include "cmsis_os.h"
#include "main.h"
#include "bsp_rng.h"
#include "cap_control.h"
#include "string.h"
#include "chassis_task.h"
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;

static CAN_TxHeaderTypeDef shoot_tx_message;
static uint8_t shoot_can_send_trigger_data[8];
/* Dart uses one 0x200 frame for 3508[0..1], yaw2006, and aux2006. */
static int16_t dart_motor_current[4] = {0};

static void CAN_CmdDartMotor(void)
{
  uint32_t send_mail_box;

  shoot_tx_message.StdId = CAN_Wheel_ALL_ID;
  shoot_tx_message.IDE = CAN_ID_STD;
  shoot_tx_message.RTR = CAN_RTR_DATA;
  shoot_tx_message.DLC = 0x08;

  shoot_can_send_trigger_data[0] = dart_motor_current[0] >> 8;
  shoot_can_send_trigger_data[1] = dart_motor_current[0];
  shoot_can_send_trigger_data[2] = dart_motor_current[1] >> 8;
  shoot_can_send_trigger_data[3] = dart_motor_current[1];
  shoot_can_send_trigger_data[4] = dart_motor_current[2] >> 8;
  shoot_can_send_trigger_data[5] = dart_motor_current[2];
  shoot_can_send_trigger_data[6] = dart_motor_current[3] >> 8;
  shoot_can_send_trigger_data[7] = dart_motor_current[3];

  HAL_CAN_AddTxMessage(&hcan1, &shoot_tx_message, shoot_can_send_trigger_data, &send_mail_box);
}

#define get_motor_measure(ptr, data)                                 \
  {                                                                  \
    (ptr)->last_ecd = (ptr)->ecd;                                    \
    (ptr)->ecd = (uint16_t)((data)[0] << 8 | (data)[1]);             \
    (ptr)->speed_rpm = (uint16_t)((data)[2] << 8 | (data)[3]);       \
    (ptr)->given_current = (uint16_t)((data)[4] << 8 | (data)[5]);   \
    (ptr)->temperate = (data)[6];                                    \
    if ((ptr)->ecd - (ptr)->last_ecd > 4096)                         \
      (ptr)->round_cnt--;                                            \
    else if ((ptr)->ecd - (ptr)->last_ecd < -4096)                   \
      (ptr)->round_cnt++;                                            \
    (ptr)->total_angle = ((ptr)->round_cnt + 1) * 8192 + (ptr)->ecd; \
  }

#define get_board_data(ptr, data)                             \
  {                                                           \
    (ptr)->rc.ch[0] = (uint16_t)((data)[0] << 8 | (data)[1]); \
    (ptr)->rc.ch[1] = (uint16_t)((data)[2] << 8 | (data)[3]); \
    (ptr)->rc.ch[2] = (uint16_t)((data)[4] << 8 | (data)[5]); \
    (ptr)->rc.s[1] = (data)[6];                               \
    (ptr)->rc.s[0] = (data)[7];                               \
  }

#define get_board2_data(ptr, data)                                \
  {                                                               \
    (ptr)->rc.ch[4] = (uint16_t)((data)[0] << 8 | (data)[1]);     \
    (ptr)->key.v = (uint16_t)((data)[2] << 8 | (data)[3]);        \
    (ptr)->gimbal_angle = (uint16_t)((data)[4] << 8 | (data)[5]); \
    (ptr)->yaw_ecd = (uint16_t)((data)[6] << 8 | (data)[7]);      \
  }
	

#define get_dart_info(ptr, data)                    \
  {                                                               \
   /* 飞镖剩余时间 */                       \
    (ptr)->dart_remaining_time = (uint8_t)(data)[0];         \
    /* 飞镖信息*/        \
    memcpy((void*)&((ptr)->dart_info), (const void*)&(data)[1], 2); \
  }

	#define get_referee_data2(ptr, data)                    \
   {                                                  \
     memcpy((void*)&((ptr)->game_type), (const void*)data, 4);        \
     (ptr)->game_progress = (uint8_t)(data)[4];         \
     (ptr)->stage_remain_time = (uint8_t)(data)[5];      \
     (ptr)->SyncTimeStamp = (uint8_t)(data)[6]; \
   }
#define get_cap_measure(ptr, data)    							\
{                                 								  \
	(ptr)->voltage = (data)[0] << 8 | (data)[1];  \
	(ptr)->current = (data)[2] << 8 | (data)[3];  \
	(ptr)->state_code.state  =  (data)[4] << 8 | (data)[5]; \
}


 // 解析飞镖信息数据：data为3字节原始数据，ptr为dart_info_t结构体指针
//#define get_dart_info(ptr, data)                    \
//{                                                  \
//    /* 飞镖剩余时间 */                       \
//    (ptr)->dart_remaining_time = (uint8_t)(data)[0];         \
//    /* 飞镖信息*/        \
//    memcpy((void*)&((ptr)->dart_info), (const void*)&(data)[1], 2); \
//}
//	 
	 
	 
// 添加get_referee_data宏，用于解析裁判系统数据
#define get_referee_data(ptr, data)                        \
  {                                                        \
    // 从data中提取shoot_data_t结构体所需的字段 \
    // 解析float类型的initial_speed(4字节) \
    memcpy(&((ptr)->initial_speed), data, 4);               \
  }
	
//	#define get_cap_measure(ptr, data)    							\
//{                                 								  \
//	(ptr)->errorCode = (uint8_t)(data)[0];  \
//	(ptr)->chassisPower = (uint32_t)(data)[1]<<8|(data)[2];  \
//	(ptr)->chassisPowerLimit  = (uint16_t) (data)[5]|(data)[6]>>2; \
//	(ptr)->capEnergy  = (uint8_t)(data)[7]>>1; \
//}
motor_measure_t MotorWheel[4];
		
		motor_measure_t Motortrigger[2];//2006电机
		
motor_measure_t MotorYaw;
 cap_measure_t cap_measure;
Rcdata_t RCData;
static dart_info_t can_dart_info;
cap_data_receiv_t  cap_data_receiv;
cap_data_t cap_data;
CharUFloat_t CharUFloat;
 extern	ChasisData_t ChassisData;
static CAN_TxHeaderTypeDef chassis_tx_message;
static CAN_TxHeaderTypeDef cap_tx_message;
static uint8_t chassis_can_send_data[8];
static uint8_t chassis_GM3508_send_data[8];
static uint8_t cap_can_send_data[8];
uint16_t Chasssis_yaw_speed;
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
Chasssis_yaw_speed=(uint16_t)ChassisData.chassis_imu_date.chassis_yaw_speed;
  CAN_RxHeaderTypeDef rx_header;
  uint8_t rx_data[8];
  HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);
  if (hcan->Instance == CAN1)
  {
    switch (rx_header.StdId)
    {
			
			
    case CAN_3508_M1_ID:
    case CAN_3508_M2_ID:
//    case CAN_3508_M3_ID:
//    case CAN_3508_M4_ID:
    {
      static uint8_t i = 0;
      i = rx_header.StdId - CAN_3508_M1_ID;
      get_motor_measure(&MotorWheel[i], rx_data);
      break;
    }
		case CAN_2006_M3_ID:  
    get_motor_measure(&Motortrigger[0], rx_data);
    break;
		case CAN_2006_M4_ID:
    get_motor_measure(&Motortrigger[1], rx_data);
    break;
		
		case CAN_CAP_ID:
			 memcpy(&cap_data_receiv,rx_data,sizeof(cap_data_receiv));
			cap_data.errorCode=cap_data_receiv.errorCode&0x7F;
		cap_data.outputenable=!(cap_data_receiv.errorCode>>7);
		cap_data.chassisPower=cap_data_receiv.chassisPower;
		cap_data.chassisPowerLimit=cap_data_receiv.chassisPowerLimit;
		cap_data.capEnergy=cap_data_receiv.capEnergy/255.0f;
//	get_cap_measure(&cap_measure,rx_data);
		 break;
    default:
    {
      break;
    }
    }
  }
  else
  {
    switch (rx_header.StdId)
    {			
			
			case CAN_Board_ID:
      get_board_data(&RCData, rx_data);
			break;
			case CAN_Board2_ID:
      get_board2_data(&RCData, rx_data);
			break;
			case CAN_Board3_ID:
			get_dart_info(&can_dart_info,rx_data);
			break;
		case CAN_BoardRef:
			get_referee_data(&shoot_data, rx_data);
			break;
//				case CAN_BoardDart:
//			get_dart_info(&dart_info, rx_data);
//			break;


    }
  }
}
}
void CAN_cmd_chassis_reset_ID(void)
{
  uint32_t send_mail_box;
  chassis_tx_message.StdId = 0x700;
  chassis_tx_message.IDE = CAN_ID_STD;
  chassis_tx_message.RTR = CAN_RTR_DATA;
  chassis_tx_message.DLC = 0x08;
  chassis_can_send_data[0] = 0;
  chassis_can_send_data[1] = 0;
  chassis_can_send_data[2] = 0;
  chassis_can_send_data[3] = 0;
  chassis_can_send_data[4] = 0;
  chassis_can_send_data[5] = 0;
  chassis_can_send_data[6] = 0;
  chassis_can_send_data[7] = 0;

  HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
}

void CAN_Cmd3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
{
  uint32_t send_mail_box;
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

  HAL_CAN_AddTxMessage(&CHASSIS_CAN, &chassis_tx_message, chassis_GM3508_send_data, &send_mail_box);
}

void CAN_CmdDart3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
{
  (void)motor3;
  (void)motor4;

  dart_motor_current[0] = motor1;
  dart_motor_current[1] = motor2;
  CAN_CmdDartMotor();
}

//2006电机
void CAN_Cmd2006(int16_t Trigger)
{
  dart_motor_current[2] = Trigger;
  CAN_CmdDartMotor();
}

void CAN_Cmd2006_Aux(int16_t Aux)
{
  dart_motor_current[3] = Aux;
  CAN_CmdDartMotor();
}

void CAN_Cmd2006_All(int16_t Trigger, int16_t Aux)
{
  dart_motor_current[2] = Trigger;
  dart_motor_current[3] = Aux;
  CAN_CmdDartMotor();
}

//void CAN_CmdBoard(int16_t data1, uint16_t data2, uint8_t data3 ,uint8_t data4,uint8_t data5)
	void CAN_CmdBoard(float initial_speed)

	{
  uint32_t send_mail_box;
  chassis_tx_message.StdId = CAN_BoardRef;
  chassis_tx_message.IDE = CAN_ID_STD;
  chassis_tx_message.RTR = CAN_RTR_DATA;
  chassis_tx_message.DLC = 0x08;
  // 将float类型的initial_speed转换为字节数组并发送
  memcpy(chassis_can_send_data, &initial_speed, 4);
  // 其余字节清零
  chassis_can_send_data[4] = 0;
  chassis_can_send_data[5] = 0;
  chassis_can_send_data[6] = 0;
  chassis_can_send_data[7] = 0;
		
  HAL_CAN_AddTxMessage(&hcan2, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
}

void CAN_chassis_speed(uint16_t data1)
	{
  uint32_t send_mail_box;
  chassis_tx_message.StdId = CAN_BoardRef;
  chassis_tx_message.IDE = CAN_ID_STD;
  chassis_tx_message.RTR = CAN_RTR_DATA;
  chassis_tx_message.DLC = 0x08;
  chassis_can_send_data[0] = data1>>8;
  chassis_can_send_data[1] = data1;
  chassis_can_send_data[2] = 0;
  chassis_can_send_data[3] = 0;
  chassis_can_send_data[4] = 0;
  chassis_can_send_data[5] = 0;
  chassis_can_send_data[6] = 0;
  chassis_can_send_data[7] = 0;

  HAL_CAN_AddTxMessage(&hcan2, &chassis_tx_message, chassis_can_send_data, &send_mail_box);
}

//void CAN_cmd_cap_powerbuff( uint16_t chassis_power_buffer)
//{
//  uint32_t send_mail_box;
//  cap_tx_message.StdId = 0x2E;
//  cap_tx_message.IDE = CAN_ID_STD;
//  cap_tx_message.RTR = CAN_RTR_DATA;
//  cap_tx_message.DLC = 0x08;
//  cap_can_send_data[0] = chassis_power_buffer>>8 ;
//  cap_can_send_data[1] = chassis_power_buffer;
//  cap_can_send_data[2] = 0;
//  cap_can_send_data[3] = 0;
//	cap_can_send_data[4] = 0;
//  cap_can_send_data[5] = 0;
//  cap_can_send_data[6] = 0;
//  cap_can_send_data[7] = 0;

//  HAL_CAN_AddTxMessage(&CAP_CAN, &cap_tx_message, cap_can_send_data, &send_mail_box);
//}
//void CAN_cmd_cap_control( uint16_t chassis_power_limit, int16_t output_power_limit, int16_t input_power_limit, int16_t cap_control)
//{
//  uint32_t send_mail_box;
//  cap_tx_message.StdId = 0x2F;
//  cap_tx_message.IDE = CAN_ID_STD;
//  cap_tx_message.RTR = CAN_RTR_DATA;
//  cap_tx_message.DLC = 0x08;
//  cap_can_send_data[0] = chassis_power_limit>>8 ;
//  cap_can_send_data[1] = chassis_power_limit;
//  cap_can_send_data[2] = output_power_limit>>8;
//  cap_can_send_data[3] = output_power_limit;
//	cap_can_send_data[4] = input_power_limit>>8;
//  cap_can_send_data[5] = input_power_limit;
//  cap_can_send_data[6] = cap_control>>8;
//  cap_can_send_data[7] = cap_control;

//  HAL_CAN_AddTxMessage(&CAP_CAN, &cap_tx_message, cap_can_send_data, &send_mail_box);
//}
void CAN_cmd_cap_control( uint8_t enableDCDC, uint16_t chassis_power_limit, uint16_t chassis_power_buffer)
{
  uint32_t send_mail_box;
  cap_tx_message.StdId = 0x061;
  cap_tx_message.IDE = CAN_ID_STD;
  cap_tx_message.RTR = CAN_RTR_DATA;
  cap_tx_message.DLC = 0x08;
  cap_can_send_data[0] = enableDCDC;
  cap_can_send_data[1] = chassis_power_limit;
  cap_can_send_data[2] =0x00;
  cap_can_send_data[3] = chassis_power_buffer;
	cap_can_send_data[4] = 0;
  cap_can_send_data[5] = 0;
  cap_can_send_data[6] = 0;
  cap_can_send_data[7] = 0;

  HAL_CAN_AddTxMessage(&CAP_CAN, &cap_tx_message, cap_can_send_data, &send_mail_box);
}

const motor_measure_t *get_chassis_Wheelmotor_point(uint8_t i)
{
  return &MotorWheel[i];
}


const Rcdata_t *Get_Rcdata(void)
{
  return &RCData;
}

const motor_measure_t *get_gimbal_yaw__point(void)
{
  return &MotorYaw;
}
/**
  * @brief          返回拨弹电机 2006电机数据指针
  * @param[in]      none
  * @retval         电机数据指针
  */
const motor_measure_t *get_trigger_motor_measure_point(uint8_t i)
{
		return &Motortrigger[(i & 0x01)];}
/**
  * @brief          返回底盘电机 3508电机数据指针
  * @param[in]      i: 电机编号,范围[0,3]
  * @retval         电机数据指针
  */
const motor_measure_t *get_chassis_motor_measure_point(uint8_t i)
{
    return &MotorWheel[(i & 0x03)];
}
