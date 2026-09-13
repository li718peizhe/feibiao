#ifndef __RM_CILENT_UI__
#define __RM_CILENT_UI__

#define Robot_ID UI_Data_RobotID_RStandard2
#define Cilent_ID UI_Data_CilentID_RStandard2        //机器人角色设置
#include "chassis_task.h"
#include "stm32f4xx.h"
#include "stdarg.h"
#include "usart.h"
#include "struct_typedef.h"

#pragma pack(1)                           //按1字节对齐

#define NULL 0
#define __FALSE 100

/****************************开始标志*********************/
#define UI_SOF 0xA5
/****************************CMD_ID数据********************/
#define UI_CMD_Robo_Exchange 0x0301    
/****************************内容ID数据********************/
#define UI_Data_ID_Del 0x0100 
#define UI_Data_ID_Draw1 0x0101
#define UI_Data_ID_Draw2 0x0102
#define UI_Data_ID_Draw5 0x0103
#define UI_Data_ID_Draw7 0x0104
#define UI_Data_ID_DrawChar 0x0110
/****************************红方机器人ID********************/
#define UI_Data_RobotID_RHero 1         
#define UI_Data_RobotID_REngineer 2
#define UI_Data_RobotID_RStandard1 3
#define UI_Data_RobotID_RStandard2 4
#define UI_Data_RobotID_RStandard3 5
#define UI_Data_RobotID_RAerial 6
#define UI_Data_RobotID_RSentry 7
#define UI_Data_RobotID_RRadar 9
/****************************蓝方机器人ID********************/
#define UI_Data_RobotID_BHero 101
#define UI_Data_RobotID_BEngineer 102
#define UI_Data_RobotID_BStandard1 103
#define UI_Data_RobotID_BStandard2 104
#define UI_Data_RobotID_BStandard3 105
#define UI_Data_RobotID_BAerial 106
#define UI_Data_RobotID_BSentry 107
#define UI_Data_RobotID_BRadar 109
/**************************红方操作手ID************************/
#define UI_Data_CilentID_RHero 0x0101
#define UI_Data_CilentID_REngineer 0x0102
#define UI_Data_CilentID_RStandard1 0x0103
#define UI_Data_CilentID_RStandard2 0x0104
#define UI_Data_CilentID_RStandard3 0x0105
#define UI_Data_CilentID_RAerial 0x0106
/***************************蓝方操作手ID***********************/
#define UI_Data_CilentID_BHero 0x0165
#define UI_Data_CilentID_BEngineer 0x0166
#define UI_Data_CilentID_BStandard1 0x0167
#define UI_Data_CilentID_BStandard2 0x0168
#define UI_Data_CilentID_BStandard3 0x0169
#define UI_Data_CilentID_BAerial 0x016A
/***************************删除操作***************************/
#define UI_Data_Del_NoOperate 0
#define UI_Data_Del_Layer 1
#define UI_Data_Del_ALL 2
/***************************图形配置参数__图形操作********************/
#define UI_Graph_ADD 1
#define UI_Graph_Change 2
#define UI_Graph_Del 3
/***************************图形配置参数__图形类型********************/
#define UI_Graph_Line 0         //直线
#define UI_Graph_Rectangle 1    //矩形
#define UI_Graph_Circle 2       //整圆
#define UI_Graph_Ellipse 3      //椭圆
#define UI_Graph_Arc 4          //圆弧
#define UI_Graph_Float 5        //浮点型
#define UI_Graph_Int 6          //整形
#define UI_Graph_Char 7         //字符型
/***************************图形配置参数__图形颜色********************/
#define UI_Color_Main 0         //红蓝主色
#define UI_Color_Yellow 1
#define UI_Color_Green 2
#define UI_Color_Orange 3
#define UI_Color_Purplish_red 4 //紫红色
#define UI_Color_Pink 5
#define UI_Color_Cyan 6         //青色
#define UI_Color_Black 7
#define UI_Color_White 8



typedef unsigned char Uint8_t;
typedef unsigned char U8;



extern  int MINTOP;

typedef  struct
{
  uint8_t SOF;//起始字节,固定0
  uint16_t data_length;//帧数据长度
  uint8_t seq;//包序号
  uint8_t CRC8;//CRC8校验值
} UI_Packhead;//帧头
         

typedef __packed struct
{
 uint16_t data_cmd_id;
 uint16_t sender_id;
 uint16_t receiver_id;
// uint8_t user_data[113];
}UI_Data_Operate;
typedef struct
{
   uint8_t Delete_Operate;         //删除操作
   uint8_t Layer;                  //删除图层
} UI_Data_Delete;          //删除图层帧


typedef struct
{
	const Rcdata_t * RCData;

}UI_DATA_T;
typedef struct
{ 
   uint8_t graphic_name[3]; 
   uint32_t operate_tpye:3; 
   uint32_t graphic_tpye:3; 
   uint32_t layer:4; 
   uint32_t color:4; 
   uint32_t start_angle:9;
   uint32_t end_angle:9;
   uint32_t width:10; 
   uint32_t start_x:11; 
   uint32_t start_y:11;
uint32_t details_c:10;
uint32_t details_d:11;
uint32_t details_e:11; 
} Float_Data;

typedef struct
{ 
uint8_t graphic_name[3]; 
uint32_t operate_tpye:3; 
uint32_t graphic_tpye:3; 
uint32_t layer:4; 
uint32_t color:4; 
uint32_t start_angle:9;
uint32_t end_angle:9;
uint32_t width:10; 
uint32_t start_x:11; 
uint32_t start_y:11;
uint32_t radius:10; 
uint32_t end_x:11; 
uint32_t end_y:11;  
	
} Graph_Data;

typedef struct
{ 
uint8_t graphic_name[3]; 
uint32_t operate_tpye:3; 
uint32_t graphic_tpye:3; 
uint32_t layer:4; 
uint32_t color:4; 
uint32_t start_angle:9;
uint32_t end_angle:9;
uint32_t width:10; 
uint32_t start_x:11; 
uint32_t start_y:11;
uint32_t radius:10; 
uint32_t end_x:11; 
uint32_t end_y:11;  
	char cap_vol_data_string[20];//图形数据
} Graph_Data1;

typedef struct
{
   Graph_Data Graph_Control;
   uint8_t show_Data[30];
} String_Data;                  //打印字符串数据

typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data Graphic_Data[7];
		uint16_t CRC16;
} draw_Yu;

typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data grapic_data_struct[2];

		uint16_t CRC16;
} draw_Yu1;
typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data grapic_data_struct[2];
const cap_data_t cap_data;
		uint16_t CRC16;
} draw_Yu2;

typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data grapic_data_struct[2];
const cap_data_t cap_data;
		uint16_t CRC16;
} draw_Yu3;



typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data1 grapic_data_struct;
		uint16_t CRC16;


} draw_cap;
typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data1 grapic_data_struct;
		uint16_t CRC16;


}draw_gimbal;
typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data1 grapic_data_struct;
		uint16_t CRC16;


}draw_top;

typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data grapic_data_struct[7];
		uint16_t CRC16;


} draw_fang;
typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data1 grapic_data_struct;
		uint16_t CRC16;

}draw_shoot;
typedef struct
{
	 UI_Packhead framehead;		
		uint16_t CMD_ID;
		UI_Data_Operate datahead;
		Graph_Data1 grapic_data_struct[2];
		uint16_t CRC16;

}draw_fire;


extern draw_Yu Draw_CH;
void UI_Delete(uint8_t Del_Operate,uint8_t Del_Layer);
void Line_Draw(Graph_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,uint32_t End_x,uint32_t End_y);
int UI_ReFresh(int cnt,...);
unsigned char Get_CRC8_Check_Sum_UI(unsigned char *pchMessage,unsigned int dwLength,unsigned char ucCRC8);
uint16_t Get_CRC16_Check_Sum_UI(uint8_t *pchMessage,uint32_t dwLength,uint16_t wCRC);
void Circle_Draw(Graph_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,uint32_t Graph_Radius);
void Rectangle_Draw(Graph_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,uint32_t End_x,uint32_t End_y);
void Float_Draw(Float_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_Size,uint32_t Graph_Digit,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,float Graph_Float);
void Char_Draw(String_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_Size,uint32_t Graph_Digit,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,char *Char_Data);
int Char_ReFresh(String_Data string_Data);
void Arc_Draw(Graph_Data *image,char imagename[3],uint32_t Graph_Operate,uint32_t Graph_Layer,uint32_t Graph_Color,uint32_t Graph_StartAngle,uint32_t Graph_EndAngle,uint32_t Graph_Width,uint32_t Start_x,uint32_t Start_y,uint32_t x_Length,uint32_t y_Length);
extern void YU_Lucky_UI(draw_Yu* draw);
extern void YU_Lucky2_UI(draw_Yu2 *draw);;
void YU_Lucky3_UI(draw_Yu2 *round);
void CAP2_UI(draw_cap *draw,int8_t power);
void CAP1_UI(draw_cap *draw,int8_t power);
void gimbal_UI2(draw_gimbal *draw);
void gimbal_UI1(draw_gimbal *draw);
void FANG_UI(draw_fang*line);
void TOP_UI1(draw_top*draw);
void TOP_UI2(draw_top*draw);
extern void shoot_left(draw_shoot*line);
extern void shoot_right(draw_shoot*line);
extern void shoot_speed(draw_shoot *shoot);
extern void fire_right(draw_fire*line);
extern void vision_state(draw_fire*line);

#endif
