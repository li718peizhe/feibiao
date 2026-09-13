
/*
 * **************************************************************************
 * ********************                                  ********************
 * ********************      COPYRIGHT INFORMATION       ********************
 * ********************                                  ********************
 * **************************************************************************
 *                                                                          *
 *                                   _oo8oo_                                *
 *                                  o8888888o                               *
 *                                  88" . "88                               *
 *                                  (| -_- |)                               *
 *                                  0\  =  /0                               *
 *                                ___/'==='\___                             *
 *                              .' \\|     |// '.                           *
 *                             / \\|||  :  |||// \                          *
 *                            / _||||| -:- |||||_ \                         *
 *                           |   | \\\  -  /// |   |                        *
 *                           | \_|  ''\---/''  |_/ |                        *
 *                           \  .-\__  '-'  __/-.  /                        *
 *                         ___'. .'  /--.--\  '. .'___                      *
 *                      ."" '<  '.___\_<|>_/___.'  >' "".                   *
 *                     | | :  `- \`.:`\ _ /`:.`/ -`  : | |                  *
 *                     \  \ `-.   \_ __\ /__ _/   .-` /  /                  *
 *                 =====`-.____`.___ \_____/ ___.`____.-`=====              *
 *                                   `=---=`                                *
 * **************************************************************************
 * ********************                                  ********************
 * ********************      				 										 ********************
 * ********************         佛祖保佑 永远无BUG       ********************
 * ********************                                  ********************
 * **************************************************************************
 *         .............................................
 *                  佛祖镇楼                  BUG辟易
 *          佛曰:
 *                  写字楼里写字间，写字间里程序员；
 *                  程序人员写程序，又拿程序换酒钱。
 *                  酒醒只在网上坐，酒醉还来网下眠；
 *                  酒醉酒醒日复日，网上网下年复年。
 *                  但愿老死电脑间，不愿鞠躬老板前；
 *                  奔驰宝马贵者趣，公交自行程序员。
 *                  别人笑我忒疯癫，我笑自己命太贱；
 *                  不见满街漂亮妹，哪个归得程序员？
 */




#include "chassis_task.h"
#include "struct_typedef.h"
#include "RM_Cilent_UI.h"
#include "crc8_crc16.h"
#include "referee.h"
#include <stdio.h>
#include "string.h"
#include "chassis_task.h"

unsigned char UI_Seq;                      //包序号
draw_Yu Draw_CH;
int robot_ID,robot_man_ID;
 extern Rcdata_t RCData;
extern cap_data_t cap_data;
 uint8_t UI_Seq1=0;
 uint8_t ClienTxBuffer[120];
uint8_t cnt=7;
uint8_t cnt1=0;
uint32_t Power_UI=15;
  int MINTOP=0;
UI_DATA_T UI_Data;
//线
void YU_Lucky_UI(draw_Yu* draw)
{
unsigned char *framepoint;                      //读写指针


	 Graph_Data imageData;
  
 	 uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值

   framepoint=(unsigned char *)&draw->framehead;
   
   draw->framehead.SOF=UI_SOF;
       	 draw->framehead.data_length=111;//
   draw->framehead.seq=UI_Seq1;

	
	
	memcpy(&ClienTxBuffer[0],&draw->framehead,5);
	append_CRC8_check_sum(ClienTxBuffer,5);
			  UI_Seq1++; 
				
	 draw->CMD_ID = cmd_id=0X0301;
	 draw->datahead.data_cmd_id=0X0104;//
   draw->datahead.sender_id=robot_ID;
   draw->datahead.receiver_id=robot_man_ID;                          //填充操作数据


if(UI_Seq1%2==0)
	{	
	draw->Graphic_Data[0].graphic_name[0] = 'h';//图形名
	draw->Graphic_Data[0].graphic_name[1] = 'h';
	draw->Graphic_Data[0].graphic_name[2] = 'h';
	draw->Graphic_Data[0].operate_tpye = 1;//图形操作
	draw->Graphic_Data[0].graphic_tpye = 0;//图形类型
	draw->Graphic_Data[0].layer = 0;//图层
	draw->Graphic_Data[0].color = 8;//颜色
	draw->Graphic_Data[0].start_angle = 0;//起始角度 字体大小
	draw->Graphic_Data[0].end_angle = 0;//终止角度 字符长度
	draw->Graphic_Data[0].width = 3;//宽度
	draw->Graphic_Data[0].start_x = 840;//X起点
	draw->Graphic_Data[0].start_y = 540;//Y起点
	draw->Graphic_Data[0].radius = 0;//半径
	draw->Graphic_Data[0].end_x = 1080;//结束长度
	draw->Graphic_Data[0].end_y = 540;//结束长度

	

	draw->Graphic_Data[1].graphic_name[0] = 'b';
	draw->Graphic_Data[1].graphic_name[1] = 'b';
	draw->Graphic_Data[1].graphic_name[2] = 'b';
		draw->Graphic_Data[1].operate_tpye = 1;
	draw->Graphic_Data[1].graphic_tpye = 0;
	draw->Graphic_Data[1].layer = 0;
	draw->Graphic_Data[1].color = 2;
	draw->Graphic_Data[1].start_angle = 0;
	draw->Graphic_Data[1].end_angle = 0;
	draw->Graphic_Data[1].width = 1;
	draw->Graphic_Data[1].start_x = 860;
	draw->Graphic_Data[1].start_y = 410;
	draw->Graphic_Data[1].radius = 0;
	draw->Graphic_Data[1].end_x = 1060;
	draw->Graphic_Data[1].end_y = 410;

		draw->Graphic_Data[2].graphic_name[0] = 'c';
	draw->Graphic_Data[2].graphic_name[1] = 'c';
	draw->Graphic_Data[2].graphic_name[2] = 'c';
		draw->Graphic_Data[2].operate_tpye = 1;
	draw->Graphic_Data[2].graphic_tpye = 0;
	draw->Graphic_Data[2].layer = 0;
	draw->Graphic_Data[2].color = 2;
	draw->Graphic_Data[2].start_angle = 0;
	draw->Graphic_Data[2].end_angle = 0;
	draw->Graphic_Data[2].width = 1;
	draw->Graphic_Data[2].start_x = 880;
	draw->Graphic_Data[2].start_y = 360;
	draw->Graphic_Data[2].radius = 0;
	draw->Graphic_Data[2].end_x = 1040;
	draw->Graphic_Data[2].end_y = 360;
	
	draw->Graphic_Data[3].graphic_name[0] = 'd';
	draw->Graphic_Data[3].graphic_name[1] = 'd';
	draw->Graphic_Data[3].graphic_name[2] = 'd';
	draw->Graphic_Data[3].operate_tpye = 1;
	draw->Graphic_Data[3].graphic_tpye = 0;
	draw->Graphic_Data[3].layer = 0;
	draw->Graphic_Data[3].color = 2;
	draw->Graphic_Data[3].start_angle = 0;
	draw->Graphic_Data[3].end_angle = 0;
	draw->Graphic_Data[3].width = 1;
	draw->Graphic_Data[3].start_x = 900;
	draw->Graphic_Data[3].start_y = 310;
	draw->Graphic_Data[3].radius = 0;
	draw->Graphic_Data[3].end_x = 1020;
	draw->Graphic_Data[3].end_y = 310;
	
	draw->Graphic_Data[4].graphic_name[0] = 'e';
	draw->Graphic_Data[4].graphic_name[1] = 'e';
	draw->Graphic_Data[4].graphic_name[2] = 'e';
	draw->Graphic_Data[4].operate_tpye = 1;
	draw->Graphic_Data[4].graphic_tpye = 0;
	draw->Graphic_Data[4].layer = 0;
	draw->Graphic_Data[4].color = 2;
	draw->Graphic_Data[4].start_angle = 0;
	draw->Graphic_Data[4].end_angle = 0;
	draw->Graphic_Data[4].width = 1;
	draw->Graphic_Data[4].start_x = 920;
	draw->Graphic_Data[4].start_y = 260;
	draw->Graphic_Data[4].radius = 0;
	draw->Graphic_Data[4].end_x = 1000;
	draw->Graphic_Data[4].end_y = 260;
	
						/*纵线*/
	draw->Graphic_Data[5].graphic_name[0] = 'f';
	draw->Graphic_Data[5].graphic_name[1] = 'f';
	draw->Graphic_Data[5].graphic_name[2] = 'f';
	draw->Graphic_Data[5].operate_tpye = 1;
	draw->Graphic_Data[5].graphic_tpye = 0;
	draw->Graphic_Data[5].layer = 0;
	draw->Graphic_Data[5].color = 5;
	draw->Graphic_Data[5].start_angle = 0;
	draw->Graphic_Data[5].end_angle = 0;
	draw->Graphic_Data[5].width = 3;
	draw->Graphic_Data[5].start_x = 960;
	draw->Graphic_Data[5].start_y = 260;
	draw->Graphic_Data[5].radius = 0;
	draw->Graphic_Data[5].end_x = 960;
	draw->Graphic_Data[5].end_y = 700;
	
						/* 圆 */
		draw->Graphic_Data[6].graphic_name[0] = 'g';
	draw->Graphic_Data[6].graphic_name[1] = 'g';
	draw->Graphic_Data[6].graphic_name[2] = 'g';

		draw->Graphic_Data[6].operate_tpye = 1;
	draw->Graphic_Data[6].graphic_tpye = 2;
	draw->Graphic_Data[6].layer = 0;
	draw->Graphic_Data[6].color = 5;
	
	draw->Graphic_Data[6].start_angle = 0;
	draw->Graphic_Data[6].end_angle = 0;
	draw->Graphic_Data[6].width = 2;
	draw->Graphic_Data[6].start_x = 960;
	draw->Graphic_Data[6].start_y = 520;
	draw->Graphic_Data[6].radius = 5;
	draw->Graphic_Data[6].end_x = 960;
	draw->Graphic_Data[6].end_y = 500;
	
}
//车道线
	if(UI_Seq1%2!=0)
//			if(0)
	{
	draw->Graphic_Data[0].graphic_name[0] = 'c';
	draw->Graphic_Data[0].graphic_name[1] = 'a';
	draw->Graphic_Data[0].graphic_name[2] = 'a';
	draw->Graphic_Data[0].operate_tpye = 1;
	draw->Graphic_Data[0].graphic_tpye = 0;
	draw->Graphic_Data[0].layer = 1;
	draw->Graphic_Data[0].color = 2;
	draw->Graphic_Data[0].start_angle = 0;
	draw->Graphic_Data[0].end_angle = 0;
	draw->Graphic_Data[0].width = 3;
	draw->Graphic_Data[0].start_x = 690-100;
	draw->Graphic_Data[0].start_y = 1080-865-100-65+10;
	draw->Graphic_Data[0].radius = 0;
	draw->Graphic_Data[0].end_x = 1230+100;
	draw->Graphic_Data[0].end_y = 1080-865-100-65+10;
	
	draw->Graphic_Data[1].operate_tpye = 1;
	draw->Graphic_Data[1].graphic_name[0] = 'c';
	draw->Graphic_Data[1].graphic_name[1] = 'b';
	draw->Graphic_Data[1].graphic_name[2] = 'b';
	draw->Graphic_Data[1].operate_tpye = 1;
	draw->Graphic_Data[1].graphic_tpye = 0;
	draw->Graphic_Data[1].layer = 1;
	draw->Graphic_Data[1].color = 2;
	draw->Graphic_Data[1].start_angle = 0;
	draw->Graphic_Data[1].end_angle = 45;
	draw->Graphic_Data[1].width = 1;
	draw->Graphic_Data[1].start_x = 630-100;
	draw->Graphic_Data[1].start_y = 10;
	draw->Graphic_Data[1].radius = 0;
	draw->Graphic_Data[1].end_x = 690-100;
	draw->Graphic_Data[1].end_y = 1080-865-100-65+10;

	draw->Graphic_Data[2].graphic_name[0] = 'c';
	draw->Graphic_Data[2].graphic_name[1] = 'd';
	draw->Graphic_Data[2].graphic_name[2] = 'd';
	draw->Graphic_Data[2].operate_tpye = 1;
	draw->Graphic_Data[2].graphic_tpye = 0;
	draw->Graphic_Data[2].layer = 1;
	draw->Graphic_Data[2].color = 2;
	draw->Graphic_Data[2].start_angle = 0;
	draw->Graphic_Data[2].end_angle = 360-45;
	draw->Graphic_Data[2].width = 1;
	draw->Graphic_Data[2].start_x = 1230+100;
	draw->Graphic_Data[2].start_y = 1080-865-100-65+10;
	draw->Graphic_Data[2].radius = 0;
	draw->Graphic_Data[2].end_x = 1280+100;
	draw->Graphic_Data[2].end_y = 10;
	
			draw->Graphic_Data[3].graphic_name[0] = 'c';
	draw->Graphic_Data[3].graphic_name[1] = 'e';
	draw->Graphic_Data[3].graphic_name[2] = 'e';
		draw->Graphic_Data[3].operate_tpye = 1;
	draw->Graphic_Data[3].graphic_tpye = 0;
	draw->Graphic_Data[3].layer = 1;
	draw->Graphic_Data[3].color = 2;
	draw->Graphic_Data[3].start_angle = 0;
	draw->Graphic_Data[3].end_angle = 0;
	draw->Graphic_Data[3].width = 1;
	draw->Graphic_Data[3].start_x = 960+300;
	draw->Graphic_Data[3].start_y = 540-120;
	draw->Graphic_Data[3].radius = 0;
	draw->Graphic_Data[3].end_x = 960+300;
	draw->Graphic_Data[3].end_y = 540+150;
	
	draw->Graphic_Data[4].graphic_name[0] = 'c';
	draw->Graphic_Data[4].graphic_name[1] = 'f';
	draw->Graphic_Data[4].graphic_name[2] = 'f';
	draw->Graphic_Data[4].operate_tpye = 1;
	draw->Graphic_Data[4].graphic_tpye = 0;
	draw->Graphic_Data[4].layer = 1;
	draw->Graphic_Data[4].color = 2;
	draw->Graphic_Data[4].start_angle = 0;
	draw->Graphic_Data[4].end_angle = 0;
	draw->Graphic_Data[4].width = 1;
	draw->Graphic_Data[4].start_x = 960-300;
	draw->Graphic_Data[4].start_y = 540-120;
	draw->Graphic_Data[4].radius = 0;
	draw->Graphic_Data[4].end_x = 960-300;
	draw->Graphic_Data[4].end_y = 540+150;
	
						/*纵线*/
	draw->Graphic_Data[5].graphic_name[0] = 'c';
	draw->Graphic_Data[5].graphic_name[1] = 'g';
	draw->Graphic_Data[5].graphic_name[2] = 'g';
	draw->Graphic_Data[5].operate_tpye = 1;
	draw->Graphic_Data[5].graphic_tpye = 0;
	draw->Graphic_Data[5].layer = 1;
	draw->Graphic_Data[5].color = 5;
	draw->Graphic_Data[5].start_angle = 0;
	draw->Graphic_Data[5].end_angle = 0;
	draw->Graphic_Data[5].width = 3;
	draw->Graphic_Data[5].start_x = 960;
	draw->Graphic_Data[5].start_y = 260;
	draw->Graphic_Data[5].radius = 0;
	draw->Graphic_Data[5].end_x = 960;
	draw->Graphic_Data[5].end_y = 700;
	
						/* 圆 */
	draw->Graphic_Data[6].graphic_name[0] = 'c';
	draw->Graphic_Data[6].graphic_name[1] = 'h';
	draw->Graphic_Data[6].graphic_name[2] = 'h';	
	draw->Graphic_Data[6].operate_tpye = 1;
	draw->Graphic_Data[6].graphic_tpye = 2;
	draw->Graphic_Data[6].layer = 1;
	draw->Graphic_Data[6].color = 5;
	draw->Graphic_Data[6].start_angle = 0;
	draw->Graphic_Data[6].end_angle = 0;
	draw->Graphic_Data[6].width = 2;
	draw->Graphic_Data[6].start_x = 960;
	draw->Graphic_Data[6].start_y = 520;
	draw->Graphic_Data[6].radius = 10;
	draw->Graphic_Data[6].end_x = 960;
	draw->Graphic_Data[6].end_y = 520;
}

	//填充操作数据


		memcpy(&ClienTxBuffer[5],(uint8_t*)&draw->CMD_ID,2);
	 memcpy(&ClienTxBuffer[7],(uint8_t*)&draw->datahead,6);
	 memcpy(&ClienTxBuffer[13],(uint8_t*)&draw->Graphic_Data,105);

//	memcpy(&ClienTxBuffer[5],(uint8_t*)&draw->CMD_ID,sizeof(UI_Data_Operate)+7*sizeof(Graph_Data)+2*sizeof(uint16_t));
	append_CRC16_check_sum(ClienTxBuffer,120);	
	for(int i=0;i<120;i++)
	{	
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &ClienTxBuffer[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	
	}

}
	
	




//电容能量
void YU_Lucky3_UI(draw_Yu2 *round)
{
	
		 uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值

   round->framehead.SOF=UI_SOF;
	 round->framehead.data_length=21+15;
   round->framehead.seq=UI_Seq1;

	uint8_t Clien_character1[45];

	memcpy(&Clien_character1[0],&round->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 round->CMD_ID = cmd_id=0X0301;
	 round->datahead.data_cmd_id=0X0102;
   round->datahead.sender_id=robot_ID;
   round->datahead.receiver_id=robot_man_ID; 

	round->grapic_data_struct[0].graphic_name[0] = 'g';
	round->grapic_data_struct[0].graphic_name[1] = 'g';
	round->grapic_data_struct[0].graphic_name[2] = 'g';
	round->grapic_data_struct[0].operate_tpye = 1;
	round->grapic_data_struct[0].graphic_tpye = 0;
	round->grapic_data_struct[0].layer = 0;
	round->grapic_data_struct[0].color = 2;
	round->grapic_data_struct[0].start_angle = 0;
	round->grapic_data_struct[0].end_angle = 0;
	round->grapic_data_struct[0].width = 25;
	round->grapic_data_struct[0].start_x = 805;
	round->grapic_data_struct[0].start_y = 115;
	round->grapic_data_struct[0].radius = 0;
	round->grapic_data_struct[0].end_x = 1095;
	round->grapic_data_struct[0].end_y = 115;

	round->grapic_data_struct[1].graphic_name[0] = 'g';
	round->grapic_data_struct[1].graphic_name[1] = 'g';
	round->grapic_data_struct[1].graphic_name[2] = 'g';
	round->grapic_data_struct[1].operate_tpye = 2;
	round->grapic_data_struct[1].graphic_tpye = 0;
	round->grapic_data_struct[1].layer = 0;
	round->grapic_data_struct[1].color = 2;
	round->grapic_data_struct[1].start_angle = 0;
	round->grapic_data_struct[1].end_angle = 0;
	round->grapic_data_struct[1].width = 25;
	round->grapic_data_struct[1].start_x = 805;
	round->grapic_data_struct[1].start_y = 115;
	round->grapic_data_struct[1].radius = 0;
	round->grapic_data_struct[1].end_x = 1095-290*(1-cap_data.capEnergy);
	round->grapic_data_struct[1].end_y = 115;

   memcpy(&Clien_character1[5],(uint8_t*)&round->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&round->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&round->grapic_data_struct,30);
	append_CRC16_check_sum(Clien_character1,45);	
	for(int i=0;i<45;i++)
	{	
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}

//矩形

 uint8_t Clien_character2[30];
void FANG_UI(draw_fang*line)
{
	unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=21;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0101;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 

	line->grapic_data_struct[0].graphic_name[0] = 'm';
	line->grapic_data_struct[0].graphic_name[1] = 'a';
	line->grapic_data_struct[0].graphic_name[2] = 'a';
	line->grapic_data_struct[0].operate_tpye = 1;
	line->grapic_data_struct[0].graphic_tpye = 1;
	line->grapic_data_struct[0].layer = 0;
	line->grapic_data_struct[0].color =8;
	line->grapic_data_struct[0].start_angle = 0;//字体大小
	line->grapic_data_struct[0].end_angle = 0;//字长
	line->grapic_data_struct[0].width = 5;
	line->grapic_data_struct[0].start_x = 800;
	line->grapic_data_struct[0].start_y = 100;
	line->grapic_data_struct[0].radius = 0;
	line->grapic_data_struct[0].end_x = 1100;
	line->grapic_data_struct[0].end_y = 130;


		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,15);


	 
	append_CRC16_check_sum(Clien_character2,30);	
	for(int i=0;i<30;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
//摩擦轮转速
void shoot_speed(draw_shoot *shoot)
{	 
	 uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值

   shoot->framehead.SOF=UI_SOF;
	 shoot->framehead.data_length=21;
   shoot->framehead.seq=UI_Seq1;

	uint8_t Clien_character1[30];

	memcpy(&Clien_character1[0],&shoot->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 shoot->CMD_ID = cmd_id=0X0301;
	 shoot->datahead.data_cmd_id=0X0101;
   shoot->datahead.sender_id=robot_ID;
   shoot->datahead.receiver_id=robot_man_ID; 


	shoot->grapic_data_struct.graphic_name[1] = 'Y';
	shoot->grapic_data_struct.graphic_name[1] = 'G';
	shoot->grapic_data_struct.graphic_name[2] = 'H';
	shoot->grapic_data_struct.operate_tpye = 1;
	shoot->grapic_data_struct.graphic_tpye = 6;
	shoot->grapic_data_struct.layer = 3;
	shoot->grapic_data_struct.color = 8;
	shoot->grapic_data_struct.start_angle = 15;//字体大小
	shoot->grapic_data_struct.end_angle = 0;//字长
	shoot->grapic_data_struct.width = 4;
	shoot->grapic_data_struct.start_x = 1537;
	shoot->grapic_data_struct.start_y = 590;
	shoot->grapic_data_struct.radius = (6&0X3FF);;
	shoot->grapic_data_struct.end_x = (6>>10)&0X7FF;
	shoot->grapic_data_struct.end_y =(6>>21)&0X7FF;
	
	
   memcpy(&Clien_character1[5],(uint8_t*)&shoot->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&shoot->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&shoot->grapic_data_struct,15);
	append_CRC16_check_sum(Clien_character1,30);	
	for(int i=0;i<30;i++)
	{	
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}

void shoot_right(draw_shoot*line)
{
		unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=21;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0101;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 
	
		line->grapic_data_struct.graphic_name[0] = 'i';
	line->grapic_data_struct.graphic_name[1] = 'a';
	line->grapic_data_struct.graphic_name[2] = 'a';
	line->grapic_data_struct.operate_tpye = 1;
	line->grapic_data_struct.graphic_tpye = 1;
	line->grapic_data_struct.layer = 0;
	line->grapic_data_struct.color =8;
	line->grapic_data_struct.start_angle = 0;//字体大小
	line->grapic_data_struct.end_angle = 0;//字长
	line->grapic_data_struct.width = 5;
	line->grapic_data_struct.start_x = 1565;
	line->grapic_data_struct.start_y = 540;
	line->grapic_data_struct.radius = 0;
	line->grapic_data_struct.end_x = 1590;
	line->grapic_data_struct.end_y = 640;

		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,15);


	 
	append_CRC16_check_sum(Clien_character2,30);	
	for(int i=0;i<30;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
void shoot_left(draw_shoot*line)
{
	unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=21;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0101;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 

	line->grapic_data_struct.graphic_name[0] = 'u';
	line->grapic_data_struct.graphic_name[1] = 'a';
	line->grapic_data_struct.graphic_name[2] = 'a';
	line->grapic_data_struct.operate_tpye = 1;
	line->grapic_data_struct.graphic_tpye = 1;
	line->grapic_data_struct.layer = 0;
	line->grapic_data_struct.color =8;
	line->grapic_data_struct.start_angle = 0;//字体大小
	line->grapic_data_struct.end_angle = 0;//字长
	line->grapic_data_struct.width = 5;
	line->grapic_data_struct.start_x = 1500;
	line->grapic_data_struct.start_y = 540;
	line->grapic_data_struct.radius = 0;
	line->grapic_data_struct.end_x = 1525;
	line->grapic_data_struct.end_y = 640;



		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,15);


	 
	append_CRC16_check_sum(Clien_character2,30);	
	for(int i=0;i<30;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
void fire_right(draw_fire*line)
{
		unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=36;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0102;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 

	line->grapic_data_struct[0].graphic_name[0] = 'u';
	line->grapic_data_struct[0].graphic_name[1] = 'a';
	line->grapic_data_struct[0].graphic_name[2] = 'a';
	line->grapic_data_struct[0].operate_tpye = 1;
	line->grapic_data_struct[0].graphic_tpye = 0;
	line->grapic_data_struct[0].layer = 1;
	line->grapic_data_struct[0].color =5;
	line->grapic_data_struct[0].start_angle = 0;//字体大小
	line->grapic_data_struct[0].end_angle = 0;//字长
	line->grapic_data_struct[0].width = 3;
	line->grapic_data_struct[0].start_x = 1565;
	line->grapic_data_struct[0].start_y = 540;
	line->grapic_data_struct[0].radius = 0;
	line->grapic_data_struct[0].end_x = 1590;
	line->grapic_data_struct[0].end_y = 540;

	line->grapic_data_struct[1].graphic_name[0] = 'u';
	line->grapic_data_struct[1].graphic_name[1] = 'a';
	line->grapic_data_struct[1].graphic_name[2] = 'a';
	line->grapic_data_struct[1].operate_tpye = 2;
	line->grapic_data_struct[1].graphic_tpye = 0;
	line->grapic_data_struct[1].layer = 1;
	line->grapic_data_struct[1].color =2;
	line->grapic_data_struct[1].start_angle = 0;//字体大小
	line->grapic_data_struct[1].end_angle = 0;//字长
	line->grapic_data_struct[1].width = 3;
	line->grapic_data_struct[1].start_x = 1565;
	line->grapic_data_struct[1].start_y = 540+(UI_Data.RCData->right_fire_speed)/100;
	line->grapic_data_struct[1].radius = 0;
	line->grapic_data_struct[1].end_x = 1590;
	line->grapic_data_struct[1].end_y = 540+(UI_Data.RCData->right_fire_speed)/100;


		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,30);


	 
	append_CRC16_check_sum(Clien_character2,45);	
	for(int i=0;i<45;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
void fire_left(draw_fire*line)
{
		unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=36;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0102;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 

	line->grapic_data_struct[0].graphic_name[0] = 'u';
	line->grapic_data_struct[0].graphic_name[1] = 'a';
	line->grapic_data_struct[0].graphic_name[2] = 'a';
	line->grapic_data_struct[0].operate_tpye = 1;
	line->grapic_data_struct[0].graphic_tpye = 0;
	line->grapic_data_struct[0].layer = 1;
	line->grapic_data_struct[0].color =5;
	line->grapic_data_struct[0].start_angle = 0;//字体大小
	line->grapic_data_struct[0].end_angle = 0;//字长
	line->grapic_data_struct[0].width = 3;
	line->grapic_data_struct[0].start_x = 1500;
	line->grapic_data_struct[0].start_y = 540;
	line->grapic_data_struct[0].radius = 0;
	line->grapic_data_struct[0].end_x = 1500;
	line->grapic_data_struct[0].end_y = 540;

	line->grapic_data_struct[1].graphic_name[0] = 'u';
	line->grapic_data_struct[1].graphic_name[1] = 'a';
	line->grapic_data_struct[1].graphic_name[2] = 'a';
	line->grapic_data_struct[1].operate_tpye = 2;
	line->grapic_data_struct[1].graphic_tpye = 0;
	line->grapic_data_struct[1].layer = 1;
	line->grapic_data_struct[1].color =2;
	line->grapic_data_struct[1].start_angle = 0;//字体大小
	line->grapic_data_struct[1].end_angle = 0;//字长
	line->grapic_data_struct[1].width = 3;
	line->grapic_data_struct[1].start_x = 1500;
	line->grapic_data_struct[1].start_y = 540+(UI_Data.RCData->left_fire_speed)/100;
	line->grapic_data_struct[1].radius = 0;
	line->grapic_data_struct[1].end_x = 1500;
	line->grapic_data_struct[1].end_y = 540+(UI_Data.RCData->left_fire_speed)/100;


		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,30);


	 
	append_CRC16_check_sum(Clien_character2,45);	
	for(int i=0;i<45;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
void vision_state(draw_fire*line)
{
		unsigned char *framepoint;
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;    	//CRC16校验值
	
	framepoint=(unsigned char *)&line->framehead;
   line->framehead.SOF=UI_SOF;
   line->framehead.data_length=36;//
   line->framehead.seq=UI_Seq1;

	
	memcpy(&Clien_character2[0],&line->framehead,5);
	append_CRC8_check_sum(Clien_character2,5);
			  UI_Seq1++; 
				
	 line->CMD_ID = cmd_id=0X0301;
	 line->datahead.data_cmd_id=0X0102;
   line->datahead.sender_id=robot_ID;
   line->datahead.receiver_id=robot_man_ID; 

	line->grapic_data_struct[0].graphic_name[0] = 'l';
	line->grapic_data_struct[0].graphic_name[1] = 'a';
	line->grapic_data_struct[0].graphic_name[2] = 'a';
	line->grapic_data_struct[0].operate_tpye = 1;
	line->grapic_data_struct[0].graphic_tpye = 1;
	line->grapic_data_struct[0].layer = 1;
	line->grapic_data_struct[0].color =2;
	line->grapic_data_struct[0].start_angle = 0;//字体大小
	line->grapic_data_struct[0].end_angle = 0;//字长
	line->grapic_data_struct[0].width = 3;
	line->grapic_data_struct[0].start_x = 960-300-80;
	line->grapic_data_struct[0].start_y = 540-120-150;
	line->grapic_data_struct[0].radius = 0;
	line->grapic_data_struct[0].end_x = 960+300+80;
	line->grapic_data_struct[0].end_y = 540+120+150;

	line->grapic_data_struct[1].graphic_name[0] = 'l';
	line->grapic_data_struct[1].graphic_name[1] = 'a';
	line->grapic_data_struct[1].graphic_name[2] = 'a';
	line->grapic_data_struct[1].operate_tpye = 2;
	line->grapic_data_struct[1].graphic_tpye = 1;
	line->grapic_data_struct[1].layer = 1;
	line->grapic_data_struct[1].color =4+RCData.vision_state;
	line->grapic_data_struct[1].start_angle = 0;//字体大小
	line->grapic_data_struct[1].end_angle = 0;//字长
	line->grapic_data_struct[1].width = 3;
	line->grapic_data_struct[1].start_x = 960-300-80;
	line->grapic_data_struct[1].start_y = 540-120-150;
	line->grapic_data_struct[1].radius = 0;
	line->grapic_data_struct[1].end_x = 960+300+80;
	line->grapic_data_struct[1].end_y = 540+120+150;


		 memcpy(&Clien_character2[5],(uint8_t*)&line->CMD_ID,2);
	 memcpy(&Clien_character2[7],(uint8_t*)&line->datahead,6);
	 memcpy(&Clien_character2[13],(uint8_t*)&line->grapic_data_struct,30);


	 
	append_CRC16_check_sum(Clien_character2,45);	
	for(int i=0;i<45;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character2[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}

//字符
void CAP2_UI(draw_cap *draw,int8_t power)
{
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值
   draw->framehead.SOF=UI_SOF;
   draw->framehead.data_length=51;//
   draw->framehead.seq=UI_Seq1;

 uint8_t Clien_character1[60];//
	
	memcpy(&Clien_character1[0],&draw->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 draw->CMD_ID = cmd_id=0X0301;
	 draw->datahead.data_cmd_id=0X0110;
   draw->datahead.sender_id=robot_ID;
   draw->datahead.receiver_id=robot_man_ID; 
	
		sprintf(draw->grapic_data_struct.cap_vol_data_string,"CAP:%u",power);

	draw->grapic_data_struct.graphic_name[0] = 'F';
	draw->grapic_data_struct.graphic_name[1] = 'S';
	draw->grapic_data_struct.graphic_name[2] = 'H';
	draw->grapic_data_struct.operate_tpye = 1;
	draw->grapic_data_struct.graphic_tpye = 7;
	draw->grapic_data_struct.layer = 6;
	draw->grapic_data_struct.color = 2;
	draw->grapic_data_struct.start_angle = 24;//字体大小
	draw->grapic_data_struct.end_angle = 10;//字长
	draw->grapic_data_struct.width = 4;
	draw->grapic_data_struct.start_x = 100;
	draw->grapic_data_struct.start_y = 880;
	draw->grapic_data_struct.radius = 0;
	draw->grapic_data_struct.end_x = 40;
	draw->grapic_data_struct.end_y =724;

  		
	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



	 
	append_CRC16_check_sum(Clien_character1,60);	
	for(int i=0;i<60;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}
	
}

void CAP1_UI(draw_cap *draw,int8_t power)
{
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值
   draw->framehead.SOF=UI_SOF;
   draw->framehead.data_length=51;//
   draw->framehead.seq=UI_Seq1;

 uint8_t Clien_character1[60];//
	
	memcpy(&Clien_character1[0],&draw->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 draw->CMD_ID = cmd_id=0X0301;
	 draw->datahead.data_cmd_id=0X0110;
   draw->datahead.sender_id=robot_ID;
   draw->datahead.receiver_id=robot_man_ID;
	
	sprintf(draw->grapic_data_struct.cap_vol_data_string,"CAP:%u",power);

	draw->grapic_data_struct.graphic_name[0] = 'F';
	draw->grapic_data_struct.graphic_name[1] = 'S';
	draw->grapic_data_struct.graphic_name[2] = 'H';
	draw->grapic_data_struct.operate_tpye = 2;
	draw->grapic_data_struct.graphic_tpye = 7;
	draw->grapic_data_struct.layer = 6;
	draw->grapic_data_struct.color = Cup_control+3;
	draw->grapic_data_struct.start_angle = 24;//字体大小
	draw->grapic_data_struct.end_angle = 10;//字长
	draw->grapic_data_struct.width = 4;
	draw->grapic_data_struct.start_x = 100;
	draw->grapic_data_struct.start_y = 880;
	draw->grapic_data_struct.radius = 0;
	draw->grapic_data_struct.end_x = 40;
	draw->grapic_data_struct.end_y =724;

  		
	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



	 
	append_CRC16_check_sum(Clien_character1,60);	
	for(int i=0;i<60;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}
	
}

char str[] = {"SHOOT:%u\n"};
//void gimbal_UI1(draw_gimbal *draw)
//{
//   uint16_t cmd_id=0X0301;	
//   uint16_t frametail=0xFFFF;                        //CRC16校验值
//   draw->framehead.SOF=UI_SOF;
//   draw->framehead.data_length=51;//
//   draw->framehead.seq=UI_Seq1;

// uint8_t Clien_character1[60];//
//	
//	memcpy(&Clien_character1[0],&draw->framehead,5);
//	append_CRC8_check_sum(Clien_character1,5);
//			  UI_Seq1++; 
//				
//	 draw->CMD_ID = cmd_id=0X0301;
//	 draw->datahead.data_cmd_id=0X0110;
//   draw->datahead.sender_id=robot_ID;
//   draw->datahead.receiver_id=robot_man_ID; 
//	
//	sprintf(draw->grapic_data_struct.cap_vol_data_string,"SHOOT:%u\n",projectile_allowance.projectile_allowance_17mm);

//	draw->grapic_data_struct.graphic_name[0] = 'F';
//	draw->grapic_data_struct.graphic_name[1] = 'f';
//	draw->grapic_data_struct.graphic_name[2] = 'H';
//	draw->grapic_data_struct.operate_tpye = 1;
//	draw->grapic_data_struct.graphic_tpye = 7;
//	draw->grapic_data_struct.layer = 6;
//	draw->grapic_data_struct.color = 1;
//	draw->grapic_data_struct.start_angle = 24;//字体大小
//	draw->grapic_data_struct.end_angle = sizeof(str);//字长
//	draw->grapic_data_struct.width = 4;
//	draw->grapic_data_struct.start_x = 100;
//	draw->grapic_data_struct.start_y = 850;
//	draw->grapic_data_struct.radius = 0;
//	draw->grapic_data_struct.end_x = 40;
//	draw->grapic_data_struct.end_y =724;

//  		
//	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
//	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
//	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



//	 
//	append_CRC16_check_sum(Clien_character1,60);	
//	for(int i=0;i<60;i++)
//	{	
//		
//	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
//	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
//	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
//	}
//	
//}

//void gimbal_UI2(draw_gimbal *draw)
//{
//   uint16_t cmd_id=0X0301;	
//   uint16_t frametail=0xFFFF;                        //CRC16校验值
//   draw->framehead.SOF=UI_SOF;
//   draw->framehead.data_length=51;//
//   draw->framehead.seq=UI_Seq1;

// uint8_t Clien_character1[60];//
//	
//	memcpy(&Clien_character1[0],&draw->framehead,5);
//	append_CRC8_check_sum(Clien_character1,5);
//			  UI_Seq1++; 
//				
//	 draw->CMD_ID = cmd_id=0X0301;
//	 draw->datahead.data_cmd_id=0X0110;
//   draw->datahead.sender_id=robot_ID;
//   draw->datahead.receiver_id=robot_man_ID; 
//	
//	sprintf(draw->grapic_data_struct.cap_vol_data_string,"SHOOT:%u",projectile_allowance.projectile_allowance_17mm);

//	draw->grapic_data_struct.graphic_name[0] = 'F';
//	draw->grapic_data_struct.graphic_name[1] = 'f';
//	draw->grapic_data_struct.graphic_name[2] = 'H';
//	draw->grapic_data_struct.operate_tpye = 2;
//	draw->grapic_data_struct.graphic_tpye = 7;
//	draw->grapic_data_struct.layer = 6;
//	draw->grapic_data_struct.color = 1;
//	draw->grapic_data_struct.start_angle = 24;//字体大小
//	draw->grapic_data_struct.end_angle = sizeof(str);;//字长
//	draw->grapic_data_struct.width = 4;
//	draw->grapic_data_struct.start_x = 100;
//	draw->grapic_data_struct.start_y = 850;
//	draw->grapic_data_struct.radius = 0;
//	draw->grapic_data_struct.end_x = 40;
//	draw->grapic_data_struct.end_y =724;

//  		
//	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
//	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
//	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



//	 
//	append_CRC16_check_sum(Clien_character1,60);	
//	for(int i=0;i<60;i++)
//	{	
//		
//	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
//	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
//	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
//	}
//	
//}

void TOP_UI1(draw_top*draw)
{
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值
   draw->framehead.SOF=UI_SOF;
   draw->framehead.data_length=51;//
   draw->framehead.seq=UI_Seq1;

 uint8_t Clien_character1[60];//
	
	memcpy(&Clien_character1[0],&draw->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 draw->CMD_ID = cmd_id=0X0301;
	 draw->datahead.data_cmd_id=0X0110;
   draw->datahead.sender_id=robot_ID;
   draw->datahead.receiver_id=robot_man_ID; 
	sprintf(draw->grapic_data_struct.cap_vol_data_string,"TOP");

	draw->grapic_data_struct.graphic_name[0] = 'F';
	draw->grapic_data_struct.graphic_name[1] = 'f';
	draw->grapic_data_struct.graphic_name[2] = 'f';
	draw->grapic_data_struct.operate_tpye = 1;
	draw->grapic_data_struct.graphic_tpye = 7;
	draw->grapic_data_struct.layer = 6;
	draw->grapic_data_struct.color = 1;
	draw->grapic_data_struct.start_angle = 24;//字体大小
	draw->grapic_data_struct.end_angle = sizeof(str);;//字长
	draw->grapic_data_struct.width = 4;
	draw->grapic_data_struct.start_x = 100;
	draw->grapic_data_struct.start_y = 820;
	draw->grapic_data_struct.radius = 0;
	draw->grapic_data_struct.end_x = 40;
	draw->grapic_data_struct.end_y =724;

  		
	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



	 
	append_CRC16_check_sum(Clien_character1,60);	
	for(int i=0;i<60;i++)
	{	
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}
void TOP_UI2(draw_top*draw)
{
   uint16_t cmd_id=0X0301;	
   uint16_t frametail=0xFFFF;                        //CRC16校验值
   draw->framehead.SOF=UI_SOF;
   draw->framehead.data_length=51;//
   draw->framehead.seq=UI_Seq1;

 uint8_t Clien_character1[60];//
	
	memcpy(&Clien_character1[0],&draw->framehead,5);
	append_CRC8_check_sum(Clien_character1,5);
			  UI_Seq1++; 
				
	 draw->CMD_ID = cmd_id=0X0301;
	 draw->datahead.data_cmd_id=0X0110;
   draw->datahead.sender_id=robot_ID;
   draw->datahead.receiver_id=robot_man_ID; 
	
	sprintf(draw->grapic_data_struct.cap_vol_data_string,"TOP");

	draw->grapic_data_struct.graphic_name[0] = 'F';
	draw->grapic_data_struct.graphic_name[1] = 'f';
	draw->grapic_data_struct.graphic_name[2] = 'f';
	draw->grapic_data_struct.operate_tpye = 2;
	draw->grapic_data_struct.graphic_tpye = 7;
	draw->grapic_data_struct.layer = 6;
	draw->grapic_data_struct.color = 1+MINTOP;
	draw->grapic_data_struct.start_angle = 24;//字体大小
	draw->grapic_data_struct.end_angle = sizeof(str);;//字长
	draw->grapic_data_struct.width = 4;
	draw->grapic_data_struct.start_x = 100;
	draw->grapic_data_struct.start_y = 820;
	draw->grapic_data_struct.radius = 0;
	draw->grapic_data_struct.end_x = 40;
	draw->grapic_data_struct.end_y =724;

  		
	 memcpy(&Clien_character1[5],(uint8_t*)&draw->CMD_ID,2);
	 memcpy(&Clien_character1[7],(uint8_t*)&draw->datahead,6);
	 memcpy(&Clien_character1[13],(uint8_t*)&draw->grapic_data_struct,45);



	 
	append_CRC16_check_sum(Clien_character1,60);	
	for(int i=0;i<60;i++)
	{	
		
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	HAL_UART_Transmit(&huart6, &Clien_character1[i], 1, HAL_MAX_DELAY); // 发送一个字节的数据
	while (HAL_UART_GetState(&huart6) != HAL_UART_STATE_READY);
	}

}

/*****************************************************CRC8校验值计算**********************************************/
const unsigned char CRC8_INIT_UI = 0xff; 
const unsigned char CRC8_TAB_UI[256] = 
{ 
0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41, 
0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc, 
0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62, 
0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff, 
0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07, 
0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a, 
0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6, 0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24, 
0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9, 
0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd, 
0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92, 0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50, 
0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee, 
0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1, 0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73, 
0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b, 
0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16, 
0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8, 
0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35, 
};
unsigned char Get_CRC8_Check_Sum_UI(unsigned char *pchMessage,unsigned int dwLength,unsigned char ucCRC8) 
{ 
unsigned char ucIndex; 
while (dwLength--) 
{ 
ucIndex = ucCRC8^(*pchMessage++); 
ucCRC8 = CRC8_TAB_UI[ucIndex]; 
} 
return(ucCRC8); 
}

uint16_t CRC_INIT_UI = 0xffff; 
const uint16_t wCRC_Table_UI[256] = 
{ 
0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf, 
0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7, 
0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e, 
0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876, 
0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd, 
0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5, 
0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c, 
0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974, 
0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb, 
0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3, 
0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72, 
0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9, 
0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1, 
0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738, 
0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7, 
0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff, 
0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036, 
0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e, 
0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5, 
0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd, 
0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134, 
0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c, 
0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3, 
0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb, 
0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232, 
0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a, 
0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1, 
0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9, 
0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330, 
0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};
/* 
** Descriptions: CRC16 checksum function 
** Input: Data to check,Stream length, initialized checksum 
** Output: CRC checksum 
*/ 
uint16_t Get_CRC16_Check_Sum_UI(uint8_t *pchMessage,uint32_t dwLength,uint16_t wCRC) 
{ 
Uint8_t chData; 
if (pchMessage == NULL) 
{ 
return 0xFFFF; 
} 
while(dwLength--) 
{ 
chData = *pchMessage++;
(wCRC) = ((uint16_t)(wCRC) >> 8) ^ wCRC_Table_UI[((uint16_t)(wCRC) ^ (uint16_t)(chData)) & 
0x00ff]; 
} 
return wCRC; 
}

