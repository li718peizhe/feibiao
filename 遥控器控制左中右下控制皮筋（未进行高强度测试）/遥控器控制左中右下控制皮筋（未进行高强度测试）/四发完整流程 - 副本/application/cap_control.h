#ifndef CAP_CONTROL_H
#define CAP_CONTROL_H
#include "struct_typedef.h"
#include "remote_control.h"
//#include "CAN_receive.h"
#define CAP_OPEN_KEY KEY_PRESSED_OFFSET_Z
#define CAP_OFF_KEY KEY_PRESSED_OFFSET_X
typedef  union
{
    uint16_t state;
    struct
    {
        uint16_t warning : 1;   //����
        uint16_t cap_v_over : 1;    //���ݹ�ѹ
        uint16_t cap_i_over : 1;    //���ݹ���
        uint16_t cap_v_low : 1;     //����Ƿѹ
        uint16_t bat_v_low : 1;     //����ϵͳǷѹ
        uint16_t can_receive_miss : 1;    //δ����CANͨ������
    }bit;
}cap_state_t;

typedef struct
{
		fp32 chassis_max_power;
    int16_t chassis_power_buffer;  //���̹��ʻ���
    uint16_t chassis_power_limit;   //�����˵��̹�����������
    int16_t output_power_limit;     //���ݷŵ繦������
    int16_t input_power_limit;     //���ݳ�繦������
			float power_limit;
	fp32 power_cap;
	  fp32 voltage;
	  fp32 current;
	  int16_t cap_voltage;
	  int16_t cap_current;
	  cap_state_t  state_code;
    int16_t state;
	  uint16_t cap_buff;
    union{
        uint16_t all;
        struct
        {
            uint16_t cap_switch : 1;    //���ݿ���
            uint16_t cap_record : 1;    //��¼���ܿ���
        }bit;
				}cap_control;
}cap_measure_t;
//typedef struct
//{
//		fp32 chassis_max_power;
//    int16_t chassis_power_buffer;  //���̹��ʻ���
//    uint16_t chassis_power_limit;   //�����˵��̹�����������
//	  fp32 voltage;
//	  fp32 current;
//	  int16_t cap_voltage;
//	  int16_t cap_current;
//	  cap_state_t  state_code;
//    int16_t state;
//	  uint16_t cap_buff;
//    union{
//        uint16_t all;
//        struct
//        {
//            uint16_t cap_switch : 1;    //���ݿ���
//            uint16_t cap_record : 1;    //��¼���ܿ���
//        }bit;
//				}cap_control;
//}cap_measure_t;
extern fp32 output_power_limit;
extern fp32 input_power_limit;
extern void cap_state_init(void);

extern void cap_feedback_update(void);

extern void cap_control_loop(void);

extern float int16_to_float(int16_t a, int16_t a_max, int16_t a_min, float b_max, float b_min);

#endif

