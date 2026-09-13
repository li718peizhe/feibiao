//
// Created by bismarckkk on 2025/3/22.
// Dynamic Edition
//

#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include "ui_types.h"
#include "bsp_usart.h"

extern int ui_self_id;
extern UART_HandleTypeDef huart6;

#define SEND_MESSAGE(message, len) HAL_UART_Transmit_DMA(&huart6, message, len);  //需替换成所使用RTOS的延时函数

//定义 弧度 转换到 角度的比例
#define RAD_TO_ANGLE 57.295779513082320876798154814105f

#define SHOOT_OPEN_FIRE KEY_PRESSED_OFFSET_E

void ui_proc_1_frame(ui_1_frame_t *msg);
void ui_proc_2_frame(ui_2_frame_t *msg);
void ui_proc_5_frame(ui_5_frame_t *msg);
void ui_proc_7_frame(ui_7_frame_t *msg);
void ui_proc_string_frame(ui_string_frame_t *msg);
void ui_proc_delete_frame(ui_delete_frame_t *msg);

void ui_delete_layer(const uint8_t delete_type, const uint8_t layer);

void ui_scan_and_send(const ui_interface_figure_t* ui_now_figures, uint8_t* ui_dirty_figure, const ui_interface_string_t* ui_now_strings, uint8_t* ui_dirty_string, int total_figures, int total_strings);

#endif //UI_INTERFACE_H
