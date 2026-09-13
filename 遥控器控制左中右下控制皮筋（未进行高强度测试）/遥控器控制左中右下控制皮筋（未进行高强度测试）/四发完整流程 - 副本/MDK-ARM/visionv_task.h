#ifndef VISIONV_TASK_H
#define VISIONV_TASK_H

#include "cmsis_os.h"
#include "pid.h"
#include "remote_control.h"

// 视觉控制模式
typedef enum {
    VISION_MODE_DISABLED = 0,  // 视觉控制禁用
    VISION_MODE_AUTO_AIM,      // 自动瞄准模式
    VISION_MODE_TRACKING        // 目标跟踪模式
} vision_mode_t;

// 视觉目标信息结构体
typedef struct {
    float target_angle;         // 视觉下发的偏角 (0.01 = 1圈内围转子)
    float target_distance;      // 目标距离（米）
    uint8_t target_found;       // 目标是否找到（1:找到，0:未找到）
    uint32_t last_update_time;  // 最后更新时间（系统时间）
    float target_total_angle;   // 结合电机当前位置计算的目标绝对总角度 (度)
} vision_target_t;

// 视觉控制结构体
typedef struct {
    vision_mode_t mode;         // 视觉控制模式
    vision_target_t target;     // 目标信息
    pid_type_def yaw_pid;       // yaw轴位置PID控制器 (外环)
    pid_type_def yaw_speed_pid; // yaw轴速度PID控制器 (内环)
    uint8_t enable;             // 视觉控制使能
} vision_control_t;

// 全局视觉控制变量
extern vision_control_t vision_control;

// 函数声明
void visionv_task(void const *pvParameters);
void vision_control_init(void);
void vision_set_mode(vision_mode_t mode);
void vision_update_target(float angle, float distance, uint8_t found);
float vision_calculate_yaw_output(void);

#endif // VISIONV_TASK_H
