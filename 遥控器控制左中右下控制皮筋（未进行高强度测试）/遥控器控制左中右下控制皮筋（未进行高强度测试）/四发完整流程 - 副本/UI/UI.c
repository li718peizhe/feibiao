//#include "UI.h"
//#include "usart.h"
//#include "referee_info.h"
//#include "usart.h"
//#include "chassis.h"
//#include "Chassis_Task.h"
//#include "bsp_supercap.h"

//#define Robot_receiver_ID  (game_robot_state.robot_id | 0x100)

//UI_Info_t UI;
//int update_supercapacitor_flag,update_int_flag;
//int update_shoot_flag,update_spin_flag,update_auto_flag,update_supcap_flag,update_unlimit_flag;
//float global_supercapacitor_remain = 77.3;//[0,100]

//uint32_t global_sight_bead_x = 960,global_sight_bead_y = 720,global_supercapacitor_point=20;//[0,100]

//uint8_t update_draw_flag;

//bool IF_Init_Over = false;
//void UI_Init(void)
//{
//	if(!IF_Init_Over)
//	{
//	update_supercapacitor_flag = ADD;
//	update_int_flag = ADD;
//		update_draw_flag = ADD;
//	IF_Init_Over = true;
//	}
//}
///**
//  * @brief  ??????¦Ç?????????????
//  * @param  ?????????????????§Ö?????
//  * @retval void
//  * @attention  ??????¦Ë????
//  */
//void UART8_SendChar(uint8_t cData)
//{
//	while (USART_GetFlagStatus( UART6, USART_FLAG_TC ) == RESET);
//	
//	USART_SendData( UART6, cData );   
//}

//ext_charstring_data_t tx_client_char;
//uint8_t CliendTxBuffer[200];
//uint8_t state_first_graphic = 0 ;//0~5???
//char first_line[30]  = {"SHOOT:"};//?????????,????30?????????bool
//char second_line[30] = {"SPIN:"};//§³????
//char third_line[30]  = {"AUTO:"};//????
//char forth_line[30]  = {"UNLIMIT:"};//???
//char fifth_line[30]  = {"SUPCAP:"};//????
//char empty_line[30] = {"         "};

//void Char_Graphic(ext_client_string_t* graphic,//?????????????????§Ö??????????
//									const char* name,						 
//									uint32_t operate_tpye,			 //??¦Â????????????????????
//									
//									uint32_t layer,							 
//									uint32_t color,
//									uint32_t size,
//									uint32_t length,
//									uint32_t width,
//									uint32_t start_x,
//									uint32_t start_y,

//									const char *character)//???????????
//{
//	graphic_data_struct_t *data_struct = &graphic->grapic_data_struct;
//	for(char i=0;i<3;i++)
//		data_struct->graphic_name[i] = name[i];	//???????
//	data_struct->operate_tpye = operate_tpye; //??????
//	data_struct->graphic_tpye = CHAR;         //Char??
//	data_struct->layer = layer;//????????
//	data_struct->color = color;//??????
//	data_struct->start_angle = size;
//	data_struct->end_angle = length;	
//	data_struct->width = width;
//	data_struct->start_x = start_x;
//	data_struct->start_y = start_y;	
//	
//	data_struct->radius = 0;
//	data_struct->end_x = 0;
//	data_struct->end_y = 0;
//	memcpy(graphic->data,empty_line,19);
//  memcpy(graphic->data,character,length);
//}

//void Client_graphic_Init(void)
//{
//	if(state_first_graphic>=5)
//	{
//		state_first_graphic = 0;
//	}
//		//??
//		tx_client_char.txFrameHeader.SOF = 0xA5;
//		tx_client_char.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(ext_client_string_t);
//		tx_client_char.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&tx_client_char.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??
//	
//		//??????
//		tx_client_char.CmdID = 0x0301;
//		
//		//????????
//		tx_client_char.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_char_graphic;
//		tx_client_char.dataFrameHeader.send_ID     = game_robot_state.robot_id;
//		tx_client_char.dataFrameHeader.receiver_ID = Robot_receiver_ID;


//		//?????
//		if(state_first_graphic == 0)//?????????????????????????????
//	{
//		Char_Graphic(&tx_client_char.clientData,"CL1",ADD,0,RED_BLUE,20,strlen(first_line),2,1620,900,first_line);
//		state_first_graphic = 1;
//	}
//	else	if(state_first_graphic == 1)//?????????????????????????????
//	{
//		Char_Graphic(&tx_client_char.clientData,"CL2",ADD,0,RED_BLUE,20,strlen(second_line),2,1620,830,second_line);
//		state_first_graphic = 2;
//	}
//	else	if(state_first_graphic == 2)//?????????????????????????????
//	{
//		Char_Graphic(&tx_client_char.clientData,"CL3",ADD,0,RED_BLUE,20,strlen(third_line),2,1620,760,third_line);
//		state_first_graphic = 3;
//	}
//	else	if(state_first_graphic == 3)//?????????????????????????????
//	{
//		Char_Graphic(&tx_client_char.clientData,"CL4",ADD,0,RED_BLUE,20,strlen(forth_line),2,1620,690,forth_line);
//		state_first_graphic = 4;
//	}
//		else	if(state_first_graphic == 4)//?????????????????????????????
//	{
//		Char_Graphic(&tx_client_char.clientData,"CL5",ADD,0,RED_BLUE,20,strlen(fifth_line),2,1620,620,fifth_line);
//		state_first_graphic = 5;
//	}
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&tx_client_char.CmdID, LEN_CMD_ID+tx_client_char.txFrameHeader.DataLength);//????????????2
//		
//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(tx_client_char));
//		
//    for(int i = 0; i < sizeof(tx_client_char); i++) {
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}
////????????????
//void Figure_INT(Int_data_struct_t *Int,
//					  const char* name,
//					  uint32_t operate_tpye,
//					  uint32_t graphic_tpye, 	//???????
//					  uint32_t layer,					//???
//					  uint32_t color,
//					  uint32_t start_angle,
//					  uint32_t end_angle,
//					  uint32_t width,					//????????
//					  uint32_t start_x,
//					  uint32_t start_y,
//					  int number
//								)
//{
//	for(char i=0;i<3;i++)
//	Int->graphic_name[i] = name[i];
//	Int->operate_tpye = operate_tpye;
//	Int->graphic_tpye = graphic_tpye;
//	Int->layer = layer;
//	Int->color = color;
//	Int->start_angle = start_angle;
//	Int->end_angle = end_angle;
//	Int->width = width;
//	Int->start_x = start_x;
//	Int->start_y = start_y;
//	Int->number = number;
//}
//ext_int_seven_data_t tx_gimbal_data_figure;
//void Client_Gimbal_Data_update()//?????????
//{
//		//??
//		tx_gimbal_data_figure.txFrameHeader.SOF = JUDGE_FRAME_HEADER;
//		tx_gimbal_data_figure.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(Int_data_struct_t)*7;
//		tx_gimbal_data_figure.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&tx_gimbal_data_figure.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??

//		//??????
//		tx_gimbal_data_figure.CmdID = 0x301;

//		//????????
//		tx_gimbal_data_figure.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_seven_graphic;//two
//		tx_gimbal_data_figure.dataFrameHeader.send_ID     = game_robot_state.robot_id;;
//		tx_gimbal_data_figure.dataFrameHeader.receiver_ID = Robot_receiver_ID;
//	
//		//?????
//		Figure_INT(&tx_gimbal_data_figure.clientData[0],"AG1",update_supercapacitor_flag,INT,3,YELLOW,20,0,2,930,750 ,CapQuantity);
//		Figure_INT(&tx_gimbal_data_figure.clientData[2],"AG3",update_supercapacitor_flag,INT,3,WHITE,20,0,2,930,850 ,(Chassis.pit_angle * 100));
////		Figure_INT(&tx_gimbal_data_figure.clientData[3],"AG6",update_supercapacitor_flag,INT,3,WHITE,20,0,2,930,850 ,(int)((Chassis.pit_angle * 100))%100);

//		Figure_INT(&tx_gimbal_data_figure.clientData[1],"AG2",update_supercapacitor_flag,INT,3,WHITE,20,0,2,930,800 ,Chassis_Motor[YAW].Data.angle*100);
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&tx_gimbal_data_figure.CmdID, LEN_CMD_ID+tx_gimbal_data_figure.txFrameHeader.DataLength);//????????????2

//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(tx_gimbal_data_figure));
//		
//    for(int i = 0; i < sizeof(tx_gimbal_data_figure); i++) {
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}


//void UI_DataUpdate(void)
//{
//		UI.User.Vcap_show =        SuperCap[0];
//}
//void Figure_Graphic(graphic_data_struct_t* graphic,//??????????????????????????
//									const char* name,
//									uint32_t operate_tpye,
//									uint32_t graphic_tpye,//?????????
//									uint32_t layer,
//									uint32_t color,
//									uint32_t start_angle,
//									uint32_t end_angle,
//									uint32_t width,
//									uint32_t start_x,
//									uint32_t start_y,
//									uint32_t radius,
//									uint32_t end_x,
//									uint32_t end_y)
//{
//	for(char i=0;i<3;i++)
//		graphic->graphic_name[i] = name[i];	//???????
//	graphic->operate_tpye = operate_tpye; //??????
//	graphic->graphic_tpye = graphic_tpye;         //Char??
//	graphic->layer        = layer;//????????
//	graphic->color        = color;//???
//	graphic->start_angle  = start_angle;
//	graphic->end_angle    = end_angle;	
//	graphic->width        = width;
//	graphic->start_x      = start_x;
//	graphic->start_y      = start_y;	
//	graphic->radius = radius;
//	graphic->end_x  = end_x;
//	graphic->end_y  = end_y;
//}
////************************************????????*******************************/
//ext_graphic_seven_data_t tx_client_graphic_figure;
//static void fri_figure(void)
//{
//	if(update_shoot_flag==0)
//	{
//		Figure_Graphic(&tx_client_graphic_figure.clientData[0],"GL1",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,890, 20,0,0);//?????¦Ä??,????
//		update_shoot_flag=1;
//	}
//	else if(Chassis.shoot_ready == 0 && update_shoot_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[0],"GL1",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,890, 20,0,0);//?????¦Ä??,????
//	else if(Chassis.shoot_ready == 1 && update_shoot_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[0],"GL1",update_draw_flag,CIRCLE,1,GREEN,0,0,5,1780,890, 20,0,0);//????????????
//}
//static void spin_second_figure(void)
//{
//	if(update_spin_flag==0)
//	{
//		Figure_Graphic(&tx_client_graphic_figure.clientData[1],"GL2",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,830, 20,0,0);//???§³?????????
//		update_spin_flag=1;
//	}
//	else if(Chassis.act == FOLO && update_spin_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[1],"GL2",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,830, 20,0,0);//???§³?????????
//	else if(Chassis.act == SPIN && update_spin_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[1],"GL2",update_draw_flag,CIRCLE,1,GREEN,0,0,5,1780,830, 20,0,0);//??§³????????
//}
//static void auto_aim_third_figure(void)
//{
//	if(update_auto_flag==0)
//	{
//		Figure_Graphic(&tx_client_graphic_figure.clientData[2],"GL3",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,760, 20,0,0);//¦Ä???????????
//		update_auto_flag=1;
//	}
//	else if(Chassis.auto_ready == Auto_Off && update_auto_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[2],"GL3",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,760, 20,0,0);//¦Ä???????????
//	else if(Chassis.auto_ready == Auto_On && update_auto_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[2],"GL3",update_draw_flag,CIRCLE,1,GREEN,0,0,5,1780,760, 20,0,0);//??????????
//}
//static void unlimit_figure(void)
//{
//	if(update_unlimit_flag==0)
//	{
//		Figure_Graphic(&tx_client_graphic_figure.clientData[3],"GL4",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,690, 20,0,0);//¦Ä????????????
//		update_unlimit_flag=1;
//	}	
//	else if(Chassis.unlimited_ready == false &&update_unlimit_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[3],"GL4",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,690, 20,0,0);//¦Ä????????????
//	else if(Chassis.unlimited_ready == true &&update_unlimit_flag==1)
//		Figure_Graphic(&tx_client_graphic_figure.clientData[3],"GL4",update_draw_flag,CIRCLE,1,GREEN,0,0,5,1780,690, 20,0,0);//???????????
//}
//static void stuck_figure(void)
//{
//	if(supcap_flag==0)
//	{
//		Figure_Graphic(&tx_client_graphic_figure.clientData[4],"GL5",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,620, 20,0,0);//¦Ä????????????
//		supcap_flag=1;
//	}
//	else if(supcap_flag==1  && Chassis.stuck_ready == false) 
//		Figure_Graphic(&tx_client_graphic_figure.clientData[4],"GL5",update_draw_flag,CIRCLE,1,FUCHSIA,0,0,5,1780,620, 20,0,0);//¦Ä????????????
//		else if(supcap_flag==1  && Chassis.stuck_ready == true) 
//		Figure_Graphic(&tx_client_graphic_figure.clientData[4],"GL5",update_draw_flag,CIRCLE,1,GREEN,0,0,5,1780,620, 20,0,0);//¦Ä????????????

//}

//static void Draw_Figure_bool()
//{
//	fri_figure();
//	spin_second_figure();
//	auto_aim_third_figure ();
//	unlimit_figure();
//	stuck_figure();
//}
//void Client_graphic_Info_update()
//{
//		//??
//		tx_client_graphic_figure.txFrameHeader.SOF = JUDGE_FRAME_HEADER;
//		tx_client_graphic_figure.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(graphic_data_struct_t)*7;
//		tx_client_graphic_figure.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&tx_client_graphic_figure.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??

//		//??????
//		tx_client_graphic_figure.CmdID = 0x0301;

//		//????????
//		tx_client_graphic_figure.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_seven_graphic;
//		tx_client_graphic_figure.dataFrameHeader.send_ID     = game_robot_state.robot_id;
//		tx_client_graphic_figure.dataFrameHeader.receiver_ID = Robot_receiver_ID;
//	
//		//?????
//		Draw_Figure_bool();
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&tx_client_graphic_figure.CmdID, LEN_CMD_ID+tx_client_graphic_figure.txFrameHeader.DataLength);//????????????2

//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(tx_client_graphic_figure));
//		
//    for(int i = 0; i < sizeof(tx_client_graphic_figure); i++) 
//		{
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}
////?????????
//float depth=40.5f;
//ext_graphic_seven_data_t Center_aim_line1;
//ext_graphic_seven_data_t Center_aim_line2;
//static void Horizontal_and_vertical_aiming_line1_Int(void)
//{
////	Figure_Graphic(&Center_aim_line1.clientData[0],"HV2",ADD,LINE,2,WHITE,0,0,2,960-depth*2,397,0,960-depth*2,500);
//		Figure_Graphic(&Center_aim_line1.clientData[1],"HV3",ADD,LINE,2,WHITE,0,0,2,960-depth,356,0,960-depth,500);
//		Figure_Graphic(&Center_aim_line1.clientData[2],"HV4",ADD,LINE,2,WHITE,0,0,2,960,315,0,960,500);
//		Figure_Graphic(&Center_aim_line1.clientData[3],"HV5",ADD,LINE,2,WHITE,0,0,2,960+depth,356,0,960+depth,500);
////	Figure_Graphic(&Center_aim_line1.clientData[4],"HV1",ADD,LINE,2,WHITE,0,0,2,960+depth*2,397,0,960+depth*2,500);
//}
//void Horizontal_and_vertical_aiming_line1_Draw()
//{
//			//??
//		Center_aim_line1.txFrameHeader.SOF = JUDGE_FRAME_HEADER;
//		Center_aim_line1.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(graphic_data_struct_t)*7;
//		Center_aim_line1.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&Center_aim_line1.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??

//		//??????
//		Center_aim_line1.CmdID = 0x301;

//		//????????
//		Center_aim_line1.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_seven_graphic;
//		Center_aim_line1.dataFrameHeader.send_ID     = game_robot_state.robot_id;
//		Center_aim_line1.dataFrameHeader.receiver_ID = Robot_receiver_ID;
//	
//		//?????
//		Horizontal_and_vertical_aiming_line1_Int();
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&Center_aim_line1.CmdID, LEN_CMD_ID+Center_aim_line1.txFrameHeader.DataLength);//????????????2

//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(Center_aim_line1));
//		
//    for(int i = 0; i < sizeof(Center_aim_line1); i++) {
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}

//static void Horizontal_and_vertical_aiming_line2_Int(void)//????+?????1??
//{	
//		Figure_Graphic(&Center_aim_line2.clientData[0],"RO1",ADD,LINE,3,GREEN,0,0,2,600,0, 0,780,190);//???
//		Figure_Graphic(&Center_aim_line2.clientData[1],"RO2",ADD,LINE,3,GREEN,0,0,2,1380,0, 0,1200,190);//?????
//		Figure_Graphic(&Center_aim_line2.clientData[2],"RO3",ADD,LINE,3,WHITE,0,0,2,960-depth*3,459,0,960+depth*3,459);//????????1??
//		Figure_Graphic(&Center_aim_line2.clientData[3],"RO4",ADD,LINE,2,WHITE,0,0,2,960,500,0,960,200);//??????
//	
//		Figure_Graphic(&Center_aim_line2.clientData[4],"RO5",ADD,LINE,2,WHITE,0,0,2, 960-depth*1.5f, 459-depth*0.75f, 0, 960+depth*1.5f, 459-depth*0.75f);//???
//		Figure_Graphic(&Center_aim_line2.clientData[5],"RO6",ADD,LINE,2,WHITE,0,0,2, 960-depth*1.5f, 459-depth*1.5f, 0, 960+depth*1.5f, 459-depth*1.5f);//???
//		Figure_Graphic(&Center_aim_line2.clientData[6],"RO7",ADD,LINE,2,WHITE,0,0,2, 960-depth*1.5f, 459-depth*2.25f, 0, 960+depth*1.5f, 459-depth*2.25f);//????
//}
//void Horizontal_and_vertical_aiming_line2_Draw()
//{
//			//??
//		Center_aim_line2.txFrameHeader.SOF = JUDGE_FRAME_HEADER;
//		Center_aim_line2.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(graphic_data_struct_t)*7;
//		Center_aim_line2.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&Center_aim_line2.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??

//		//??????
//		Center_aim_line2.CmdID = 0x301;

//		//????????
//		Center_aim_line2.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_seven_graphic;
//		Center_aim_line2.dataFrameHeader.send_ID     = game_robot_state.robot_id;
//		Center_aim_line2.dataFrameHeader.receiver_ID = Robot_receiver_ID;
//	
//		//?????
//		Horizontal_and_vertical_aiming_line2_Int();
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&Center_aim_line2.CmdID, LEN_CMD_ID+Center_aim_line2.txFrameHeader.DataLength);//????????????2

//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(Center_aim_line2));
//		
//    for(int i = 0; i < sizeof(Center_aim_line2); i++) {
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}


////???????????????
//ext_graphic_one_data_t tx_supercapacitor_figure;
//int update_supercapacitor_flag;
//static void supercapacitor_figure(float remain_energy,uint32_t turning_point)//????????????¦Ë???????????????????
//{
//	uint32_t remaining = (uint32_t)remain_energy;//??????
//	if(remaining >= turning_point)//???????3
//		Figure_Graphic(&tx_supercapacitor_figure.clientData,"SR1",update_supercapacitor_flag,LINE,0,GREEN,0,0,20,1780,330,0, 1780,330+remaining*3);
//	else if(remaining < turning_point)
//		Figure_Graphic(&tx_supercapacitor_figure.clientData,"SR1",update_supercapacitor_flag,LINE,0,FUCHSIA,0,0,20,1780,330,0,1780,330+remaining*3);		
//}
//void Client_supercapacitor_update()//?????????
//{
//		//??
//		tx_supercapacitor_figure.txFrameHeader.SOF = JUDGE_FRAME_HEADER;
//		tx_supercapacitor_figure.txFrameHeader.DataLength = sizeof(ext_student_interactive_header_data_t) + sizeof(graphic_data_struct_t);
//		tx_supercapacitor_figure.txFrameHeader.Seq = 0;//?????
//		memcpy(CliendTxBuffer,&tx_supercapacitor_figure.txFrameHeader,sizeof(xFrameHeader));
//		Append_CRC8_Check_Sum(CliendTxBuffer, sizeof(xFrameHeader));//?§µ??

//		//??????
//		tx_supercapacitor_figure.CmdID = 0x301;

//		//????????
//		tx_supercapacitor_figure.dataFrameHeader.data_cmd_id = INTERACT_ID_draw_one_graphic;
//		tx_supercapacitor_figure.dataFrameHeader.send_ID     = game_robot_state.robot_id;;
//		tx_supercapacitor_figure.dataFrameHeader.receiver_ID = Robot_receiver_ID;
//	
//		//?????
//		supercapacitor_figure(global_supercapacitor_remain,global_supercapacitor_point);
//		memcpy(CliendTxBuffer+LEN_FRAME_HEAD, (uint8_t*)&tx_supercapacitor_figure.CmdID, LEN_CMD_ID+tx_supercapacitor_figure.txFrameHeader.DataLength);//????????????2

//		//?¦Â
//		Append_CRC16_Check_Sum(CliendTxBuffer,sizeof(tx_supercapacitor_figure));
//		
//    for(int i = 0; i < sizeof(tx_supercapacitor_figure); i++) {
//      UART8_SendChar(CliendTxBuffer[i]);
//    }
//}

//void Startjudge_task(void)
//{
//  static int i = -1,J = 0;
//  i++;
//  J++;
//	UI_Init();
//  UI_DataUpdate();
//  
//  switch(i)
//  {
//    case 0:
//        Client_graphic_Init();   //????
//        break;

//    case 1:
//        Client_graphic_Info_update();
//        break;

//    case 2:
//				Horizontal_and_vertical_aiming_line1_Draw();
//				break;
//		
//		case 3:
//				Horizontal_and_vertical_aiming_line2_Draw();
//				break;
//    
//		case 4:
//			Client_Gimbal_Data_update();
//		  update_int_flag = MODIFY;
//		update_draw_flag = MODIFY;
//				break;
//		
//    case 5:
//      global_supercapacitor_remain = (UI.User.Vcap_show - 13)/(23-13) * 100;
//      Client_supercapacitor_update();
//      update_supercapacitor_flag = MODIFY;
//      break;

//    default:
//        i = -1;
//        break;
//  }
//  
//  if(J == 100)
//  {
//    J = 0;
//    UI.IF_Init_Over = false;
//  }
//}
