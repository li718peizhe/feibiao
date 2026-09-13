//
// Created by RM UI Designer
// Static Edition
//

#ifndef UI_g_H
#define UI_g_H

#include "ui_interface.h"
#include "INS_task.h"
#include "user_lib.h"
#include "arm_math.h"
typedef struct
{
 const fp32 * UI_INS_angle;
 fp32 YAW;
}UI_t;

extern ui_interface_line_t *ui_g_dynamic_CD_CD_surplus_capacity;

void ui_init_g_dynamic_CD();
void ui_update_g_dynamic_CD();
void ui_remove_g_dynamic_CD();

extern ui_interface_string_t *ui_g_dynamic_Fire_Fire;

void ui_init_g_dynamic_Fire();
void ui_update_g_dynamic_Fire();
void ui_remove_g_dynamic_Fire();

extern ui_interface_arc_t *ui_g_dynamic_chassis_condition_chassis_gimbal_angle;

void ui_init_g_dynamic_chassis_condition();
void ui_update_g_dynamic_chassis_condition();
void ui_remove_g_dynamic_chassis_condition();

extern ui_interface_rect_t *ui_g_dynamic_recognize_armour_recognize_armour_box;

void ui_init_g_dynamic_recognize_armour();
void ui_update_g_dynamic_recognize_armour();
void ui_remove_g_dynamic_recognize_armour();

extern ui_interface_line_t *ui_g_static_the_box_below_l;
extern ui_interface_line_t *ui_g_static_the_box_below_r;
extern ui_interface_round_t *ui_g_static_Aiming_circle;
extern ui_interface_line_t *ui_g_static_auxiliary_line_a;
extern ui_interface_line_t *ui_g_static_auxiliary_line_b;
extern ui_interface_rect_t *ui_g_static_CD_box;
extern ui_interface_line_t *ui_g_static_auxiliary_line_c;
extern ui_interface_line_t *ui_g_static_auxiliary_line_d;
extern ui_interface_line_t *ui_g_static_auxiliary_line_e;
extern ui_interface_line_t *ui_g_static_auxiliary_line_v;

void ui_init_g_static();
void ui_update_g_static();
void ui_remove_g_static();


#endif // UI_g_H
