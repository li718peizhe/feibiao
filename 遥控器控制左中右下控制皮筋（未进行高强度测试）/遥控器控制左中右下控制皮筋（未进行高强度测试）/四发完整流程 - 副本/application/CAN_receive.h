#ifndef CAN_RECEIVE_H
#define CAN_RECEIVE_H
#include "referee.h"
#include "struct_typedef.h"
#include "cap_control.h"
#define CHASSIS_CAN hcan1
#define Board_CAN hcan1
#define CAP_CAN  hcan1

/* ----------------------- RC Switch Definition----------------------------- */
#define RC_Left 1
#define RC_Right 0
#define RC_SW_UP                ((uint16_t)1)
#define RC_SW_MID               ((uint16_t)3)
#define RC_SW_DOWN              ((uint16_t)2)
#define switch_is_down(s)       (s == RC_SW_DOWN)
#define switch_is_mid(s)        (s == RC_SW_MID)
#define switch_is_up(s)         (s == RC_SW_UP)

/* CAN send and receive ID */
typedef enum
{
	 CAN_GIMBAL_ALL_ID = 0x1FF,
  CAN_SHOOT_ALL_ID = 0x200,
	
	 CAN_GM2006_AMMUNITION_BOOSTER_ID = 0x207,
  CAN_Wheel_ALL_ID = 0x200,
  CAN_Rudder_ALL_ID = 0x1FF,
  /*can1*/
  CAN_3508_M1_ID = 0x201,
  CAN_3508_M2_ID = 0x202,
//  CAN_3508_M3_ID = 0x203,
//  CAN_3508_M4_ID = 0x204,

	CAN_2006_M3_ID = 0x203,
	CAN_2006_M4_ID = 0x204,
	
//  CAN_6020_M1_ID = 0x208,
//  CAN_6020_M2_ID = 0x209,
//  CAN_6020_M3_ID = 0x200,
  CAN_BoardRef = 0x208, 
	CAN_Board3_ID=0x209,


  /*can2*/
  CAN_6020_M4_ID = 0x208,
//  CAN_GIMBAL_YAW_ID = 0x20A,
  CAN_Board_ID = 0x201,
  CAN_Board2_ID = 0x202,

	//CAN_BoardDart=0x205,
  	CAN_CAP_ID=0X051,
//		CAN_CAP_ID=0x30,
} can_msg_id_e;

// rm motor data
typedef struct
{
  uint16_t ecd;
  int16_t speed_rpm;
  int16_t given_current;
  uint8_t temperate;
  int16_t last_ecd;
  int32_t total_angle;
  int32_t round_cnt;
	int32_t current_angle;
} motor_measure_t;

typedef struct
{
  // int16_t ch[5];
  // char s[2];
  // uint16_t key;
  int16_t gimbal_angle;
  uint16_t yaw_ecd;
	uint8_t vision_state;
	int16_t right_fire_speed;
	int16_t left_fire_speed;
  struct
  {
    int16_t ch[5];
    char s[2];
  } rc;
  struct
  {
    uint16_t v;
  } key;

} Rcdata_t;

typedef union
{
  uint8_t byte[4];
  float f_data;
} CharUFloat_t;
typedef __packed struct
{
	uint8_t errorCode;
	float chassisPower;
	uint16_t chassisPowerLimit;
	uint8_t capEnergy;
}cap_data_receiv_t;

typedef struct
{
	uint8_t errorCode;
	uint8_t outputenable;
	float chassisPower;
	uint16_t chassisPowerLimit;
	float capEnergy;
}cap_data_t;


extern void CAN_cmd_chassis_reset_ID(void);
extern void CAN2_Cmd6020(int16_t motor4);
extern void CAN_Cmd6020(int16_t motor1, int16_t motor2, int16_t motor3);
extern void CAN_Cmd3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
extern void CAN_CmdDart3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
extern void CAN2_Cmd6020_3(int16_t motor3);
extern void CAN_CmdBoard(float initial_speed);
extern void CAN_Cmd2006(int16_t Trigger);
extern void CAN_Cmd2006_Aux(int16_t Aux);
extern void CAN_Cmd2006_All(int16_t Trigger, int16_t Aux);

extern const Rcdata_t *Get_Rcdata(void);
extern	Rcdata_t RCData;

/**
  * @brief          返回底盘电机 3508电机数据指针
  * @param[in]      i: 电机编号,范围[0,3]
  * @retval         电机数据指针
  */extern const motor_measure_t *get_chassis_motor_measure_point(uint8_t i);   

extern const motor_measure_t *get_trigger_motor_measure_point(uint8_t i);
extern const motor_measure_t *get_chassis_Wheelmotor_point(uint8_t i);
extern const motor_measure_t *get_chassis_Ruddermotor__point(uint8_t i);
extern motor_measure_t MotorWheel[4];
extern motor_measure_t MotorRudder[4];
extern const motor_measure_t *get_gimbal_yaw__point(void);

extern cap_measure_t cap_measure;
extern void CAN_cmd_cap_powerbuff(uint16_t chassis_power_buffer);
extern void CAN_cmd_cap_control( uint8_t enableDCDC , uint16_t chassis_power_limit, uint16_t chassis_power_buffer);
//extern void CAN_cmd_cap_control( uint16_t chassis_power_limit, int16_t output_power_limit, int16_t input_power_limit, int16_t cap_control);

extern	void CAN_chassis_speed(uint16_t data1);

extern const motor_measure_t *get_chassis_motor_measure_point(uint8_t i);
#endif
