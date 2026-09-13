#include <string.h>
#include "ui_interface.h"
#include "ui_g.h"
#include "CAN_receive.h"  
#include "remote_control.h"
#include "chassis_task.h"
#include "INS_task.h"

uint8_t capEnergy_total = 100;  //超电最大容量（百分比）
extern cap_data_t  cap_data;
UI_t Angle_UI;
extern Rcdata_t RCData;
extern ChasisData_t ChassisData;
/*动态*/
ui_1_frame_t ui_g_dynamic_CD_0;
ui_interface_line_t *ui_g_dynamic_CD_CD_surplus_capacity = (ui_interface_line_t*)&(ui_g_dynamic_CD_0.data[0]);

//超电剩余容量
void _ui_init_g_dynamic_CD_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_CD_0.data[i].figure_name[0] = 0;
        ui_g_dynamic_CD_0.data[i].figure_name[1] = 0;
        ui_g_dynamic_CD_0.data[i].figure_name[2] = i + 0;
        ui_g_dynamic_CD_0.data[i].operate_type = 1;
    }
    for (int i = 1; i < 1; i++) {
        ui_g_dynamic_CD_0.data[i].operate_type = 0;
    }

    ui_g_dynamic_CD_CD_surplus_capacity->figure_type = 0;
    ui_g_dynamic_CD_CD_surplus_capacity->operate_type = 1;
    ui_g_dynamic_CD_CD_surplus_capacity->layer = 0;
    ui_g_dynamic_CD_CD_surplus_capacity->color = 2;
    ui_g_dynamic_CD_CD_surplus_capacity->start_x = 660;
    ui_g_dynamic_CD_CD_surplus_capacity->start_y = 111;
    ui_g_dynamic_CD_CD_surplus_capacity->width = 39;
    ui_g_dynamic_CD_CD_surplus_capacity->end_x = 1263;
    ui_g_dynamic_CD_CD_surplus_capacity->end_y = 111;

    ui_proc_1_frame(&ui_g_dynamic_CD_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_CD_0, sizeof(ui_g_dynamic_CD_0));
}

void _ui_update_g_dynamic_CD_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_CD_0.data[i].operate_type = 2;
    }
	//实时超电容量更新 
	ui_g_dynamic_CD_CD_surplus_capacity->end_x = 660 + 603 * cap_data.capEnergy;
	
    ui_proc_1_frame(&ui_g_dynamic_CD_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_CD_0, sizeof(ui_g_dynamic_CD_0));
}

void _ui_remove_g_dynamic_CD_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_CD_0.data[i].operate_type = 3;
    }
	
    ui_proc_1_frame(&ui_g_dynamic_CD_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_CD_0, sizeof(ui_g_dynamic_CD_0));
}


void ui_init_g_dynamic_CD() {
    _ui_init_g_dynamic_CD_0();
}

void ui_update_g_dynamic_CD() {
    _ui_update_g_dynamic_CD_0();
}

void ui_remove_g_dynamic_CD() {
    _ui_remove_g_dynamic_CD_0();
}


ui_string_frame_t ui_g_dynamic_Fire_0;
ui_interface_string_t* ui_g_dynamic_Fire_Fire = &(ui_g_dynamic_Fire_0.option);

//开火
void _ui_init_g_dynamic_Fire_0() {
    ui_g_dynamic_Fire_0.option.figure_name[0] = 0;
    ui_g_dynamic_Fire_0.option.figure_name[1] = 1;
    ui_g_dynamic_Fire_0.option.figure_name[2] = 0;
    ui_g_dynamic_Fire_0.option.operate_type = 1;

    ui_g_dynamic_Fire_Fire->figure_type = 7;
    ui_g_dynamic_Fire_Fire->operate_type = 1;
    ui_g_dynamic_Fire_Fire->layer = 1;
    ui_g_dynamic_Fire_Fire->color = 8;
    ui_g_dynamic_Fire_Fire->start_x = 1581;
    ui_g_dynamic_Fire_Fire->start_y = 725;
    ui_g_dynamic_Fire_Fire->width = 3;
    ui_g_dynamic_Fire_Fire->font_size = 24;
    ui_g_dynamic_Fire_Fire->str_length = 4;
    strcpy(ui_g_dynamic_Fire_Fire->string, "Fire");

    ui_proc_string_frame(&ui_g_dynamic_Fire_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_Fire_0, sizeof(ui_g_dynamic_Fire_0));
}

void _ui_update_g_dynamic_Fire_0() {
    ui_g_dynamic_Fire_0.option.operate_type = 2;

    static uint8_t e_key_toggle_flag = 0;  //当前
	static uint8_t e_key_last_state = 0;  //上次
	
	uint8_t e_key_current_state = (RCData.key.v & SHOOT_OPEN_FIRE) ? 1 : 0;
	
	if (e_key_current_state && !e_key_last_state)  // 检测按键从未按下 -> 按下的边沿
	{
    e_key_toggle_flag = !e_key_toggle_flag;  // 反转开关状态

    if (e_key_toggle_flag)
        ui_g_dynamic_Fire_Fire->color = 2;  // green
    else
        ui_g_dynamic_Fire_Fire->color = 8;  // white
	}
	e_key_last_state = e_key_current_state;  // 更新上次状态
	
    ui_proc_string_frame(&ui_g_dynamic_Fire_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_Fire_0, sizeof(ui_g_dynamic_Fire_0));
}

void _ui_remove_g_dynamic_Fire_0() {
    ui_g_dynamic_Fire_0.option.operate_type = 3;

    ui_proc_string_frame(&ui_g_dynamic_Fire_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_Fire_0, sizeof(ui_g_dynamic_Fire_0));
}

void ui_init_g_dynamic_Fire() {
    _ui_init_g_dynamic_Fire_0();
}

void ui_update_g_dynamic_Fire() {
    _ui_update_g_dynamic_Fire_0();
}

void ui_remove_g_dynamic_Fire() {
    _ui_remove_g_dynamic_Fire_0();
}

ui_1_frame_t ui_g_dynamic_chassis_condition_0;
ui_interface_arc_t *ui_g_dynamic_chassis_condition_chassis_gimbal_angle = (ui_interface_arc_t*)&(ui_g_dynamic_chassis_condition_0.data[0]);

//云台-底盘状态
void _ui_init_g_dynamic_chassis_condition_0() {
	Angle_UI.UI_INS_angle =get_INS_angle_point();
	
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_chassis_condition_0.data[i].figure_name[0] = 0;
        ui_g_dynamic_chassis_condition_0.data[i].figure_name[1] = 2;
        ui_g_dynamic_chassis_condition_0.data[i].figure_name[2] = i + 0;
        ui_g_dynamic_chassis_condition_0.data[i].operate_type = 1;
    }
    for (int i = 1; i < 1; i++) {
        ui_g_dynamic_chassis_condition_0.data[i].operate_type = 0;
    }

    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->figure_type = 4;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->operate_type = 1;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->layer = 0;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->color = 6;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_x = 1623;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_y = 704;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->width = 21;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = 30;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle = 330;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->rx = 88;
    ui_g_dynamic_chassis_condition_chassis_gimbal_angle->ry = 88;


    ui_proc_1_frame(&ui_g_dynamic_chassis_condition_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_chassis_condition_0, sizeof(ui_g_dynamic_chassis_condition_0));
}

void rotato_arc(fp32 angle)
{
	if(angle < 0)  //顺时针
	{
		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = 30 - angle *RAD_TO_ANGLE;
		if(-angle *RAD_TO_ANGLE <= 30 && -angle *RAD_TO_ANGLE >0)
			ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle = 330 - angle *RAD_TO_ANGLE;
		else
			ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle =  - angle *RAD_TO_ANGLE - 30;
	}
	else if(angle > 0)  //逆时针
	{
		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle = 330 - angle *RAD_TO_ANGLE;
		if(angle *RAD_TO_ANGLE <= 30 && angle *RAD_TO_ANGLE >0)
			ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = 30 - angle *RAD_TO_ANGLE;
		else
			ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = 390 - angle *RAD_TO_ANGLE;
	}
	else if(angle == 0)  //正向
	{
		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = 30;
        ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle = 330;
	}
}

void _ui_update_g_dynamic_chassis_condition_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_chassis_condition_0.data[i].operate_type = 2;
    }
	Angle_UI.YAW = (rad_format(*(Angle_UI.UI_INS_angle +INS_YAW_ADDRESS_OFFSET)));
	//实时底盘正方向角度更新
    rotato_arc(ChassisData.ChassisFollowGimbalAngle);
	if(ChassisData.ChassisMode == CHASSIS_TOP) //小陀螺判断
		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->color = 5;  //-->pink
	else
		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->color = 6;  //-->cyan
 
    ui_proc_1_frame(&ui_g_dynamic_chassis_condition_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_chassis_condition_0, sizeof(ui_g_dynamic_chassis_condition_0));
}

void _ui_remove_g_dynamic_chassis_condition_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_chassis_condition_0.data[i].operate_type = 3;
    }

    ui_proc_1_frame(&ui_g_dynamic_chassis_condition_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_chassis_condition_0, sizeof(ui_g_dynamic_chassis_condition_0));
}


void ui_init_g_dynamic_chassis_condition() {
    _ui_init_g_dynamic_chassis_condition_0();
}

void ui_update_g_dynamic_chassis_condition() {
    _ui_update_g_dynamic_chassis_condition_0();
}

void ui_remove_g_dynamic_chassis_condition() {
    _ui_remove_g_dynamic_chassis_condition_0();
}

ui_1_frame_t ui_g_dynamic_recognize_armour_0;
ui_interface_rect_t *ui_g_dynamic_recognize_armour_recognize_armour_box = (ui_interface_rect_t*)&(ui_g_dynamic_recognize_armour_0.data[0]);

//识别装甲板
void _ui_init_g_dynamic_recognize_armour_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_recognize_armour_0.data[i].figure_name[0] = 0;
        ui_g_dynamic_recognize_armour_0.data[i].figure_name[1] = 3;
        ui_g_dynamic_recognize_armour_0.data[i].figure_name[2] = i + 0;
        ui_g_dynamic_recognize_armour_0.data[i].operate_type = 1;
    }
    for (int i = 1; i < 1; i++) {
        ui_g_dynamic_recognize_armour_0.data[i].operate_type = 0;
    }

    ui_g_dynamic_recognize_armour_recognize_armour_box->figure_type = 1;
    ui_g_dynamic_recognize_armour_recognize_armour_box->operate_type = 1;
    ui_g_dynamic_recognize_armour_recognize_armour_box->layer = 1;
    ui_g_dynamic_recognize_armour_recognize_armour_box->color = 7;
    ui_g_dynamic_recognize_armour_recognize_armour_box->start_x = 566;
    ui_g_dynamic_recognize_armour_recognize_armour_box->start_y = 260;
    ui_g_dynamic_recognize_armour_recognize_armour_box->width = 4;
    ui_g_dynamic_recognize_armour_recognize_armour_box->end_x = 1354;
    ui_g_dynamic_recognize_armour_recognize_armour_box->end_y = 835;
	
    ui_proc_1_frame(&ui_g_dynamic_recognize_armour_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_recognize_armour_0, sizeof(ui_g_dynamic_recognize_armour_0));
}

void _ui_update_g_dynamic_recognize_armour_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_recognize_armour_0.data[i].operate_type = 2;
    }
	
	if(RCData.vision_state == 1)
		ui_g_dynamic_recognize_armour_recognize_armour_box->color = 5;  //-->green
	else
		ui_g_dynamic_recognize_armour_recognize_armour_box->color = 7;  //-->white

    ui_proc_1_frame(&ui_g_dynamic_recognize_armour_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_recognize_armour_0, sizeof(ui_g_dynamic_recognize_armour_0));
}

void _ui_remove_g_dynamic_recognize_armour_0() {
    for (int i = 0; i < 1; i++) {
        ui_g_dynamic_recognize_armour_0.data[i].operate_type = 3;
    }

    ui_proc_1_frame(&ui_g_dynamic_recognize_armour_0);
    SEND_MESSAGE((uint8_t *) &ui_g_dynamic_recognize_armour_0, sizeof(ui_g_dynamic_recognize_armour_0));
}


void ui_init_g_dynamic_recognize_armour() {
    _ui_init_g_dynamic_recognize_armour_0();
}

void ui_update_g_dynamic_recognize_armour() {
    _ui_update_g_dynamic_recognize_armour_0();
}

void ui_remove_g_dynamic_recognize_armour() {
    _ui_remove_g_dynamic_recognize_armour_0();
}


/*静态（正常不需要修改）*/
ui_7_frame_t ui_g_static_0;

ui_interface_line_t *ui_g_static_the_box_below_l = (ui_interface_line_t*)&(ui_g_static_0.data[0]);
ui_interface_line_t *ui_g_static_the_box_below_r = (ui_interface_line_t*)&(ui_g_static_0.data[1]);
ui_interface_round_t *ui_g_static_Aiming_circle = (ui_interface_round_t*)&(ui_g_static_0.data[2]);
ui_interface_line_t *ui_g_static_auxiliary_line_a = (ui_interface_line_t*)&(ui_g_static_0.data[3]);
ui_interface_line_t *ui_g_static_auxiliary_line_b = (ui_interface_line_t*)&(ui_g_static_0.data[4]);
ui_interface_rect_t *ui_g_static_CD_box = (ui_interface_rect_t*)&(ui_g_static_0.data[5]);
ui_interface_line_t *ui_g_static_auxiliary_line_c = (ui_interface_line_t*)&(ui_g_static_0.data[6]);

void _ui_init_g_static_0() {
    for (int i = 0; i < 7; i++) {
        ui_g_static_0.data[i].figure_name[0] = 0;
        ui_g_static_0.data[i].figure_name[1] = 4;
        ui_g_static_0.data[i].figure_name[2] = i + 0;
        ui_g_static_0.data[i].operate_type = 1;
    }
    for (int i = 7; i < 7; i++) {
        ui_g_static_0.data[i].operate_type = 0;
    }

    ui_g_static_the_box_below_l->figure_type = 0;
    ui_g_static_the_box_below_l->operate_type = 1;
    ui_g_static_the_box_below_l->layer = 0;
    ui_g_static_the_box_below_l->color = 2;
    ui_g_static_the_box_below_l->start_x = 650;
    ui_g_static_the_box_below_l->start_y = 258;
    ui_g_static_the_box_below_l->width = 3;
    ui_g_static_the_box_below_l->end_x = 513;
    ui_g_static_the_box_below_l->end_y = 103;

    ui_g_static_the_box_below_r->figure_type = 0;
    ui_g_static_the_box_below_r->operate_type = 1;
    ui_g_static_the_box_below_r->layer = 0;
    ui_g_static_the_box_below_r->color = 2;
    ui_g_static_the_box_below_r->start_x = 1267;
    ui_g_static_the_box_below_r->start_y = 258;
    ui_g_static_the_box_below_r->width = 3;
    ui_g_static_the_box_below_r->end_x = 1404;
    ui_g_static_the_box_below_r->end_y = 103;

    ui_g_static_Aiming_circle->figure_type = 2;
    ui_g_static_Aiming_circle->operate_type = 1;
    ui_g_static_Aiming_circle->layer = 0;
    ui_g_static_Aiming_circle->color = 8;
    ui_g_static_Aiming_circle->start_x = 959;
    ui_g_static_Aiming_circle->start_y = 538;
    ui_g_static_Aiming_circle->width = 3;
    ui_g_static_Aiming_circle->r = 19;

    ui_g_static_auxiliary_line_a->figure_type = 0;
    ui_g_static_auxiliary_line_a->operate_type = 1;
    ui_g_static_auxiliary_line_a->layer = 0;
    ui_g_static_auxiliary_line_a->color = 8;
    ui_g_static_auxiliary_line_a->start_x = 649;
    ui_g_static_auxiliary_line_a->start_y = 539;
    ui_g_static_auxiliary_line_a->width = 3;
    ui_g_static_auxiliary_line_a->end_x = 1266;
    ui_g_static_auxiliary_line_a->end_y = 539;

    ui_g_static_auxiliary_line_b->figure_type = 0;
    ui_g_static_auxiliary_line_b->operate_type = 1;
    ui_g_static_auxiliary_line_b->layer = 0;
    ui_g_static_auxiliary_line_b->color = 8;
    ui_g_static_auxiliary_line_b->start_x = 919;
    ui_g_static_auxiliary_line_b->start_y = 509;
    ui_g_static_auxiliary_line_b->width = 2;
    ui_g_static_auxiliary_line_b->end_x = 1005;
    ui_g_static_auxiliary_line_b->end_y = 509;

    ui_g_static_CD_box->figure_type = 1;
    ui_g_static_CD_box->operate_type = 1;
    ui_g_static_CD_box->layer = 0;
    ui_g_static_CD_box->color = 8;
    ui_g_static_CD_box->start_x = 659;
    ui_g_static_CD_box->start_y = 91;
    ui_g_static_CD_box->width = 4;
    ui_g_static_CD_box->end_x = 1263;
    ui_g_static_CD_box->end_y = 132;

    ui_g_static_auxiliary_line_c->figure_type = 0;
    ui_g_static_auxiliary_line_c->operate_type = 1;
    ui_g_static_auxiliary_line_c->layer = 0;
    ui_g_static_auxiliary_line_c->color = 8;
    ui_g_static_auxiliary_line_c->start_x = 889;
    ui_g_static_auxiliary_line_c->start_y = 479;
    ui_g_static_auxiliary_line_c->width = 2;
    ui_g_static_auxiliary_line_c->end_x = 1026;
    ui_g_static_auxiliary_line_c->end_y = 479;


    ui_proc_7_frame(&ui_g_static_0);
    SEND_MESSAGE((uint8_t *) &ui_g_static_0, sizeof(ui_g_static_0));
}

void _ui_update_g_static_0() {
    for (int i = 0; i < 7; i++) {
        ui_g_static_0.data[i].operate_type = 2;
    }

    ui_proc_7_frame(&ui_g_static_0);
    SEND_MESSAGE((uint8_t *) &ui_g_static_0, sizeof(ui_g_static_0));
}

void _ui_remove_g_static_0() {
    for (int i = 0; i < 7; i++) {
        ui_g_static_0.data[i].operate_type = 3;
    }

    ui_proc_7_frame(&ui_g_static_0);
    SEND_MESSAGE((uint8_t *) &ui_g_static_0, sizeof(ui_g_static_0));
}
ui_5_frame_t ui_g_static_1;

ui_interface_line_t *ui_g_static_auxiliary_line_d = (ui_interface_line_t*)&(ui_g_static_1.data[0]);
ui_interface_line_t *ui_g_static_auxiliary_line_e = (ui_interface_line_t*)&(ui_g_static_1.data[1]);
ui_interface_line_t *ui_g_static_auxiliary_line_v = (ui_interface_line_t*)&(ui_g_static_1.data[2]);

void _ui_init_g_static_1() {
    for (int i = 0; i < 3; i++) {
        ui_g_static_1.data[i].figure_name[0] = 0;
        ui_g_static_1.data[i].figure_name[1] = 4;
        ui_g_static_1.data[i].figure_name[2] = i + 7;
        ui_g_static_1.data[i].operate_type = 1;
    }
    for (int i = 3; i < 5; i++) {
        ui_g_static_1.data[i].operate_type = 0;
    }

    ui_g_static_auxiliary_line_d->figure_type = 0;
    ui_g_static_auxiliary_line_d->operate_type = 1;
    ui_g_static_auxiliary_line_d->layer = 0;
    ui_g_static_auxiliary_line_d->color = 8;
    ui_g_static_auxiliary_line_d->start_x = 859;
    ui_g_static_auxiliary_line_d->start_y = 449;
    ui_g_static_auxiliary_line_d->width = 2;
    ui_g_static_auxiliary_line_d->end_x = 1065;
    ui_g_static_auxiliary_line_d->end_y = 449;

    ui_g_static_auxiliary_line_e->figure_type = 0;
    ui_g_static_auxiliary_line_e->operate_type = 1;
    ui_g_static_auxiliary_line_e->layer = 0;
    ui_g_static_auxiliary_line_e->color = 8;
    ui_g_static_auxiliary_line_e->start_x = 779;
    ui_g_static_auxiliary_line_e->start_y = 419;
    ui_g_static_auxiliary_line_e->width = 2;
    ui_g_static_auxiliary_line_e->end_x = 1139;
    ui_g_static_auxiliary_line_e->end_y = 419;

    ui_g_static_auxiliary_line_v->figure_type = 0;
    ui_g_static_auxiliary_line_v->operate_type = 1;
    ui_g_static_auxiliary_line_v->layer = 0;
    ui_g_static_auxiliary_line_v->color = 8;
    ui_g_static_auxiliary_line_v->start_x = 960;
    ui_g_static_auxiliary_line_v->start_y = 666;
    ui_g_static_auxiliary_line_v->width = 2;
    ui_g_static_auxiliary_line_v->end_x = 960;
    ui_g_static_auxiliary_line_v->end_y = 272;


    ui_proc_5_frame(&ui_g_static_1);
    SEND_MESSAGE((uint8_t *) &ui_g_static_1, sizeof(ui_g_static_1));
}

void _ui_update_g_static_1() {
    for (int i = 0; i < 3; i++) {
        ui_g_static_1.data[i].operate_type = 2;
    }

    ui_proc_5_frame(&ui_g_static_1);
    SEND_MESSAGE((uint8_t *) &ui_g_static_1, sizeof(ui_g_static_1));
}

void _ui_remove_g_static_1() {
    for (int i = 0; i < 3; i++) {
        ui_g_static_1.data[i].operate_type = 3;
    }

    ui_proc_5_frame(&ui_g_static_1);
    SEND_MESSAGE((uint8_t *) &ui_g_static_1, sizeof(ui_g_static_1));
}


void ui_init_g_static() {
    _ui_init_g_static_0();
    _ui_init_g_static_1();
}

void ui_update_g_static() {
    _ui_update_g_static_0();
    _ui_update_g_static_1();
}

void ui_remove_g_static() {
    _ui_remove_g_static_0();
    _ui_remove_g_static_1();
}

//		ui_g_dynamic_chassis_condition_chassis_gimbal_angle->color = 5;  //-->pink
//		uint32_t start_angle = (uint32_t)ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle;
//		uint32_t end_angle = (uint32_t)ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle;
//		while(1)
//		{
//			ui_g_dynamic_chassis_condition_chassis_gimbal_angle->start_angle = start_angle++;
//            ui_g_dynamic_chassis_condition_chassis_gimbal_angle->end_angle = end_angle++;
//			if(start_angle ==360) start_angle = 0;
//			if(end_angle ==360) end_angle = 0;
//			if(ChassisData.ChassisMode != CHASSIS_TOP)
//				ui_g_dynamic_chassis_condition_chassis_gimbal_angle->color = 6;  //-->cyan
//				break;
//		}
