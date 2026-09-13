#include "visionv_task.h"
#include "usbv_task.h"
#include "cmsis_os.h"
#include "shoot_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>

// 外部变量声明
extern dart_date_t Dart_Data;

// 全局视觉控制变量
vision_control_t vision_control;

// 视觉位置PID参数 (外环) - 误差单位为内围转子角度(度)
// 去除Ki防止到达目标后积分导致抖动(0.15度偏移)，加入Kd作为阻尼，越接近目标减速越明显
static const fp32 vision_yaw_pid[3] = {1.5f, 0.0f, 1.0f};
static const float VISION_YAW_PID_MAX_OUT = 3000.0f; // 限制最大速度 (rpm)
static const float VISION_YAW_PID_MAX_IOUT = 800.0f;

// 视觉速度PID参数 (内环)
static const fp32 vision_yaw_speed_pid[3] = {5.0f, 0.1f, 0.0f};
static const float VISION_YAW_SPEED_PID_MAX_OUT = 10000.0f; // 限制最大电流 (2006电机最大发送10000)
static const float VISION_YAW_SPEED_PID_MAX_IOUT = 3000.0f;

// 目标更新超时时间（ms）
static const uint32_t TARGET_UPDATE_TIMEOUT = 500;

static uint8_t vision_target_active(void)
{
    uint32_t now = xTaskGetTickCount();

    return (vision_control.target.target_found &&
            (now - vision_control.target.last_update_time) <= TARGET_UPDATE_TIMEOUT);
}

/**
 * @brief 视觉控制初始化
 */
void vision_control_init(void)
{
    // 初始化视觉控制结构体
    vision_control.mode = VISION_MODE_DISABLED;
    vision_control.enable = 0;
    
    // 初始化目标信息
    vision_control.target.target_angle = 0.0f;
    vision_control.target.target_distance = 0.0f;
    vision_control.target.target_found = 0;
    vision_control.target.last_update_time = 0;
    vision_control.target.target_total_angle = 0.0f;
    
    // 初始化PID控制器 (外环)
    PID_init(&vision_control.yaw_pid, PID_POSITION, vision_yaw_pid, 
             VISION_YAW_PID_MAX_OUT, VISION_YAW_PID_MAX_IOUT);
             
    // 初始化PID控制器 (内环)
    PID_init(&vision_control.yaw_speed_pid, PID_POSITION, vision_yaw_speed_pid, 
             VISION_YAW_SPEED_PID_MAX_OUT, VISION_YAW_SPEED_PID_MAX_IOUT);
}

/**
 * @brief 设置视觉控制模式
 * @param mode 视觉控制模式
 */
void vision_set_mode(vision_mode_t mode)
{
    if (vision_control.mode == mode) {
        return;
    }

    vision_control.mode = mode;
    if (mode == VISION_MODE_DISABLED) {
        vision_control.enable = 0;
        PID_clear(&vision_control.yaw_pid);
        PID_clear(&vision_control.yaw_speed_pid);
    } else {
        vision_control.enable = 1;
    }
}

/**
 * @brief 更新视觉目标信息
 * @param angle 目标角度（度）
 * @param distance 目标距离（米）
 * @param found 目标是否找到
 */
void vision_update_target(float angle, float distance, uint8_t found)
{
    vision_control.target.target_distance = distance;
    vision_control.target.target_found = found;
    vision_control.target.last_update_time = xTaskGetTickCount();
    
    if (found && Dart_Data.G2006_motor[0].trigger_motor_measure != NULL) {
        // 根据用户给定的比例：内围转子转1圈(360度) = target_angle 0.01
        int32_t current_ecd = Dart_Data.G2006_motor[0].trigger_motor_measure->total_angle;
        float current_total_angle = (float)current_ecd * 360.0f / 8192.0f;
        
        if (angle == 0.0f) {
            // 如果刚从非0变成0，说明到达目标，锁定目标为当前绝对位置
            // 如果已经是0，则不更新，保持原目标锁定！防止漂移和抖动
            if (vision_control.target.target_angle != 0.0f) {
                vision_control.target.target_total_angle = current_total_angle;
                // 到达目标时清除PID积分，防止积分风饱和导致的超调和0.15度抖动
                PID_clear(&vision_control.yaw_pid);
                PID_clear(&vision_control.yaw_speed_pid);
            }
        } else {
            // 目标非0时，计算新的绝对目标角度
            vision_control.target.target_total_angle = current_total_angle + angle * 36000.0f;
        }
        
        // 记录最新角度
        vision_control.target.target_angle = angle;
    }
}

/**
 * @brief 计算yaw轴输出
 * @return yaw轴控制输出
 */
float vision_calculate_yaw_output(void)
{
    uint32_t current_time = xTaskGetTickCount();
    if ((current_time - vision_control.target.last_update_time) > TARGET_UPDATE_TIMEOUT) {
        vision_control.target.target_found = 0;
    }
    
    if (!vision_control.target.target_found) {
        PID_clear(&vision_control.yaw_pid);
        PID_clear(&vision_control.yaw_speed_pid);
        return 0.0f;
    }
    
    if (Dart_Data.G2006_motor[0].trigger_motor_measure == NULL) {
        PID_clear(&vision_control.yaw_pid);
        PID_clear(&vision_control.yaw_speed_pid);
        return 0.0f;
    }
    
    int32_t current_ecd = Dart_Data.G2006_motor[0].trigger_motor_measure->total_angle;
    int16_t speed_rpm = Dart_Data.G2006_motor[0].trigger_motor_measure->speed_rpm;
    float current_total_angle = (float)current_ecd * 360.0f / 8192.0f;
    
    float target_total_angle = vision_control.target.target_total_angle;
    
    // 1. 位置环计算 (使用绝对角度进行PID)
    PID_calc(&vision_control.yaw_pid, current_total_angle, target_total_angle);
    
    // 根据用户要求："越接近目标值，速度越慢，并且加入前馈补偿"
    // 计算位置误差，使用平方根曲线（类时间最优）动态限制输出速度
    float pos_error = target_total_angle - current_total_angle;
    float abs_error = pos_error > 0.0f ? pos_error : -pos_error;
    
    float target_speed = vision_control.yaw_pid.out;
    
    // 前馈速度限制曲线：距离越近，最大允许速度以抛物线形态快速下降，保证停靠极其平滑
    float max_speed_ff = VISION_YAW_PID_MAX_OUT;
    if (abs_error < 1.0f) {
        max_speed_ff = 0.0f; // 极小误差直接死区，彻底消除抖动
    } else if (abs_error < 180.0f) {
        // sqrtf 前馈阻尼曲线：常数系数可调，当前设置为 30，使接近时速度快速收敛
        max_speed_ff = 30.0f * sqrtf(abs_error); 
    }
    
    // 限制外环输出（速度），执行前馈减速
    if (target_speed > max_speed_ff) target_speed = max_speed_ff;
    if (target_speed < -max_speed_ff) target_speed = -max_speed_ff;
    
    // 2. 速度环计算
    PID_calc(&vision_control.yaw_speed_pid, (float)speed_rpm, target_speed);
    
    return vision_control.yaw_speed_pid.out;
}

/**
 * @brief 视觉控制任务
 * @param pvParameters 任务参数
 */
void visionv_task(void const *pvParameters)
{
    vTaskDelay(500);
    
    vision_control_init();
    
    while(1)
    {
        if (Dart_Data.G2006_motor[0].trigger_motor_measure == NULL) {
            vTaskDelay(10);
            continue;
        }
        
        if (vision_target_active()) {
            vision_set_mode(VISION_MODE_AUTO_AIM);
        } else {
            vision_set_mode(VISION_MODE_DISABLED);
            CAN_Cmd2006(0);
        }
        
        if (vision_control.enable) {
            float yaw_output = vision_calculate_yaw_output();
            int16_t current_give = (int16_t)yaw_output;
            
            // 2006电机发送范围限制 [-10000, 10000]
            if (current_give > 10000) current_give = 10000;
            if (current_give < -10000) current_give = -10000;
            
            CAN_Cmd2006(current_give);
        }
        
        vTaskDelay(1);
    }
}
