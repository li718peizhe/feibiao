/**
  ******************************************************************************
  * @file    shoot_task.c
  * @brief   飞镖发射任务实现文件
  *          包含飞镖发射状态机、电机控制、遥控器输入处理等功能
  ******************************************************************************
  */

#include "shoot_task.h"
#include <stdlib.h>
#include "calibrate_task.h"
#include "string.h"
#include "cmsis_os.h"

#include "bsp_buzzer.h"
#include "bsp_flash.h"
#include "cmsis_os.h"
#include "arm_math.h"

#include "can_receive.h"
#include "remote_control.h"
#include "INS_task.h"

#include "cmsis_os.h"
#include "arm_math.h"

#include "bsp_servo_pwm.h"
#include "pid.h"

#include "Emm_V5.h"

#include "visionv_task.h"
#include "usbv_task.h"
#include "referee.h"

void yaw_init(void);
static void M2006_Aux_Control(void);
static void M2006_Aux_ResetHold(void);
static uint8_t dart_rc_control_enabled(void);
static uint8_t dart_rc_safe_stop_requested(void);
static uint8_t dart_aux2006_rc_enabled(void);
static uint8_t referee_data_time_valid(uint32_t update_time);
static uint8_t referee_game_state_valid(void);
static uint8_t referee_dart_cmd_valid(void);
static uint8_t dart_game_started(void);
static uint8_t dart_system_enabled(void);
static void dart_enable_outputs(void);
static void dart_all_motor_disable(void);
static void dart_enter_safe_stop(void);
static void reset_dart_window_control(void);
static void reset_motion_arrive_counter(void);

/**
  * @brief  目标值死区限制宏
  * @param  out: 输出值
  * @param  max: 最大值限制
  * @param  min: 最小值限制
  */
#define target_deadband_limit(out, max , min)        \
  {                                                  \
    if ((out) > (max))              \
      (out) = max;                                  \
    else if ((out) < (min))         \
      (out) = min;                                  \
  }

/**
  * @brief  裁判系统数据结构
  */
typedef struct {
    uint8_t game_type;                    // 比赛类型
    uint8_t game_progress;                // 比赛阶段
    uint16_t stage_remain_time;           // 阶段剩余时间
	
	  uint8_t dart_remaining_time;          // 飞镖剩余时间
    uint16_t dart_info;                   // 飞镖信息
    uint8_t latest_dart_hit_target;       // 最近一次己方飞镖击中目标
    uint8_t dart_hit_count;               // 对方被击中目标累计被击中次数
    uint8_t selected_dart_target;         // 飞镖当前选定击打目标
    uint16_t enemy_dart_hit_time;         // 对方飞镖最近命中时间
    uint8_t enemy_dart_hit_target;        // 对方飞镖最近命中目标
    uint8_t robot_id;                     // 机器人ID
    uint16_t current_hp;                  // 当前血量
    uint16_t shooter_barrel_cooling_value; // 枪管冷却值
    uint16_t shooter_barrel_heat_limit;    // 枪管热量限制
    uint16_t chassis_power_limit;         // 底盘功率限制
    uint8_t power_management_shooter_output; // 功率管理射击输出
    uint8_t dart_launch_opening_status;   // 飞镖发射站状态：1关闭，2正在开启或关闭中，0已开启
    uint16_t dart_target_change_time;     // 切换击打目标时的比赛剩余时间
    uint16_t dart_latest_launch_cmd_time; // 最后一次确认发射指令时的比赛剩余时间
    uint32_t game_state_update_time;
    uint32_t dart_info_update_time;
    uint32_t dart_client_cmd_update_time;
} RefereeData_t;

RefereeData_t referee_data;

/**
  * @brief  读取裁判系统数据
  *         从全局裁判系统变量中更新本地数据结构
  */
void read_referee_data(void) {
    referee_data.dart_remaining_time = dart_info.dart_remaining_time;
    referee_data.dart_info = dart_info.dart_info;
    referee_data.latest_dart_hit_target = (uint8_t)(dart_info.dart_info & 0x0007);
    referee_data.dart_hit_count = (uint8_t)((dart_info.dart_info >> 3) & 0x0007);
    referee_data.selected_dart_target = (uint8_t)((dart_info.dart_info >> 6) & 0x0007);
    referee_data.enemy_dart_hit_time = (uint16_t)((field_event.event_data >> 11) & 0x01FF);
    referee_data.enemy_dart_hit_target = (uint8_t)((field_event.event_data >> 20) & 0x0007);
    referee_data.game_type = game_state.game_type;
    referee_data.game_progress = game_state.game_progress;
    referee_data.stage_remain_time = game_state.stage_remain_time;
    referee_data.robot_id = robot_state.robot_id;
    referee_data.current_hp = robot_state.current_HP;
    referee_data.shooter_barrel_cooling_value = robot_state.shooter_barrel_cooling_value;
    referee_data.shooter_barrel_heat_limit = robot_state.shooter_barrel_heat_limit;
    referee_data.chassis_power_limit = robot_state.chassis_power_limit;
    referee_data.power_management_shooter_output = robot_state.power_management_shooter_output;
    referee_data.dart_launch_opening_status = dart_client_cmd.dart_launch_opening_status;
    referee_data.dart_target_change_time = dart_client_cmd.target_change_time;
    referee_data.dart_latest_launch_cmd_time = dart_client_cmd.latest_launch_cmd_time;
    referee_data.game_state_update_time = game_state_last_update_time;
    referee_data.dart_info_update_time = dart_info_last_update_time;
    referee_data.dart_client_cmd_update_time = dart_client_cmd_last_update_time;
}

/* 飞镖弹出和发射标志位 */
int dart_pop_up_flat=0;
int dart_launch_flat=0;
	
/* 遥控器拨杆状态记录 */
static uint8_t last_left_switch  = 0;
static uint8_t last_right_switch = 0;
static uint8_t last_prepare_start_allowed = 0;
static uint8_t last_game_progress = 0;
static uint8_t stepper_disabled_by_rc = 0;
static uint16_t up_hold_stable_count = 0;
static uint16_t up_hold_push_count = 0;
static uint16_t up_hold_elapsed_count = 0;
static uint8_t up_hold_current_released = 0;
static uint8_t aux2006_hold_initialized = 0;
static fp32 aux2006_position_set = 0.0f;
int u=0;

/* PID控制器定义 */
pid_type_def trigger_motor_speed_pid;      // 拨弹电机速度PID
pid_type_def magazine_motor_speed_pid;     // 供弹电机速度PID
pid_type_def Fire_motor_pid[6];            // 发射电机电PID数组
pid_type_def trigger_motor_pos_pid;        // 拨弹电机位置PID

/* GM3508电机电流环PID参数 */
const fp32 GM3508_Current_PID[3] = {15, 0, 0.3};
pid_type_def GM3508_Current[2];            // GM3508电流PID实例

/* 斜坡函数实例，用于电机平滑加速/减速 */
ramp_function_source_t fire_left,fire_right,fire_left_down,fire_right_down;
static ramp_function_source_t pick_down_left, pick_down_right;

/* 飞镖系统全局数据 */
dart_date_t Dart_Data;
static uint8_t reload_bottom_ref_valid = 0;

/**
  * @brief  飞镖系统初始化
  *         初始化飞镖状态机、PID参数、斜坡函数等
  */
void shoot_init(void)
{
	Dart_Data.dart_mode = DART_STANDBY;
	Dart_Data.dart_state = DART_STATE_IDLE;
	Dart_Data.current_dart = 0;
	Dart_Data.state_timer = 0;
	reload_bottom_ref_valid = 0;
	reset_dart_window_control();
	Dart_Data.RcDate = get_remote_control_point();
	
	/* 初始化PID参数数组 */
	const static fp32 motor3508_current_pid[3] = {M3508_MOTOR_CURRENT_PID_KP, M3508_MOTOR_CURRENT_PID_KI, M3508_MOTOR_CURRENT_PID_KD};
	const static fp32 motor3508_speed_pid[3] = {M3505_MOTOR_SPEED_PID_KP, M3505_MOTOR_SPEED_PID_KI, M3505_MOTOR_SPEED_PID_KD};
	const static fp32 motor2006_speed_pid[3] = {M2006_MOTOR_SPEED_PID_KP, M2006_MOTOR_SPEED_PID_KI, M2006_MOTOR_SPEED_PID_KD};
	const static fp32 motor2006_position_pid[3] = {M2006_MOTOR_POSITION_PID_KP, M2006_MOTOR_POSITION_PID_KI, M2006_MOTOR_POSITION_PID_KD};
	
	/* 初始化斜坡函数 */
	ramp_init(&fire_left, 0.02, 16000.0f, 0.0f);
	ramp_init(&fire_right, 0.02, 0.0f, -16000.0f);
	ramp_init(&fire_left_down, 0.02, 2000.0f, 0.0f);
	ramp_init(&fire_right_down, 0.02, 0.0f, -2000.0f);
	ramp_init(&pick_down_left, 0.02, 8000.0f, 4700.0f);
	ramp_init(&pick_down_right, 0.02, -4700.0f, -8000.0f);
	
	/* 初始化GM3508电机 */
	for (int i = 0; i < 2; i++)
    {
		Dart_Data.G3508_motor[i].chassis_motor_measure = get_chassis_motor_measure_point(i);
		PID_init(&Dart_Data.motor3508_current_pid[i], PID_POSITION, motor3508_current_pid, GM3508_MOTOR_CURRENT_PID_MAX_OUT, GM3508_MOTOR_CURRENT_PID_MAX_IOUT);
		PID_init(&Dart_Data.motor3508_speed_pid[i], PID_POSITION, motor3508_speed_pid, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
		Dart_Data.G3508_motor[i].speed_set = 0.0f;
		Dart_Data.G3508_motor[i].current_set = 0.0f;
		Dart_Data.G3508_motor[i].give_current = 0;
	}
	
	/* 初始化云台偏航轴 */
	yaw_init();
	vision_control_init();

	/* 以当前步进电机位置作为换弹原点，后续送弹后回到该0点 */
	Emm_V5_Reset_CurPos_To_Zero(1);
}

/**
  * @brief  云台偏航轴初始化
  *         初始化M2006拨弹电机的速度和位置PID
  */
void yaw_init(void)
{	
	const static fp32 motor2006_speed_pid[3] = {M2006_MOTOR_SPEED_PID_KP, M2006_MOTOR_SPEED_PID_KI, M2006_MOTOR_SPEED_PID_KD};
	const static fp32 motor2006_position_pid[3] = {M2006_MOTOR_POSITION_PID_KP, M2006_MOTOR_POSITION_PID_KI, M2006_MOTOR_POSITION_PID_KD};
	const static fp32 aux2006_speed_pid[3] = {M2006_AUX_SPEED_PID_KP, M2006_AUX_SPEED_PID_KI, M2006_AUX_SPEED_PID_KD};
	const static fp32 aux2006_position_pid[3] = {M2006_AUX_POSITION_PID_KP, M2006_AUX_POSITION_PID_KI, M2006_AUX_POSITION_PID_KD};
	
	for (int i = 0; i < 2; i++)
    {
		Dart_Data.G2006_motor[i].trigger_motor_measure = get_trigger_motor_measure_point(i);
		if (i == 0)
		{
			PID_init(&Dart_Data.motor2006_speed_pid[i], PID_POSITION, motor2006_speed_pid, M2006_MOTOR_POSITION_PID_MAX_OUT, M2006_MOTOR_POSITION_PID_MAX_IOUT);
			PID_init(&Dart_Data.motor2006_position_pid[i], PID_POSITION, motor2006_position_pid, M2006_MOTOR_POSITION_PID_MAX_OUT, M2006_MOTOR_POSITION_PID_MAX_IOUT);
		}
		else
		{
			PID_init(&Dart_Data.motor2006_speed_pid[i], PID_POSITION, aux2006_speed_pid, M2006_AUX_SPEED_PID_MAX_OUT, M2006_AUX_SPEED_PID_MAX_IOUT);
			PID_init(&Dart_Data.motor2006_position_pid[i], PID_POSITION, aux2006_position_pid, M2006_AUX_POSITION_PID_MAX_OUT, M2006_AUX_POSITION_PID_MAX_IOUT);
		}
		Dart_Data.G2006_motor[i].speed_set = 0.0f;
		Dart_Data.G2006_motor[i].current_set = 0;
		Dart_Data.G2006_motor[i].give_current = 0;
	}
	M2006_Aux_ResetHold();
}

/**
  * @brief  射程调整函数（预留）
  */
void range_adjustment(void)
{
	
}

/**
  * @brief  M2006拨弹电机控制
  *         根据遥控器摇杆输入控制云台偏航角度
  */
static void M2006_Control(void)
{
    int16_t ch0 = Dart_Data.RcDate->rc.ch[0];
    fp32 target_pos = (fp32)ch0 / 660.0f * 45.0f;
    target_deadband_limit(target_pos, 45.0f, -45.0f);
 
    uint16_t ecd = Dart_Data.G2006_motor[0].trigger_motor_measure->ecd;
    fp32 current_angle = (fp32)ecd * 360.0f / 8192.0f;
	
    if (current_angle > 180.0f)
        current_angle -= 360.0f;

    if (abs(ch0) < 10) {
        CAN_Cmd2006(0);
        return;
    }

    PID_Calc_Angle(&Dart_Data.motor2006_position_pid[0], current_angle, target_pos);
    fp32 speed_set = Dart_Data.motor2006_position_pid[0].out;

    fp32 current_speed = Dart_Data.G2006_motor[0].trigger_motor_measure->speed_rpm;
    PID_calc(&Dart_Data.motor2006_speed_pid[0], current_speed, speed_set);
    int16_t current_give = (int16_t)Dart_Data.motor2006_speed_pid[0].out;

    target_deadband_limit(current_give, 2000, -2000);
	CAN_Cmd2006(current_give);
}

static void M2006_Aux_ResetHold(void)
{
	aux2006_hold_initialized = 0;
	aux2006_position_set = 0.0f;
	PID_clear(&Dart_Data.motor2006_position_pid[1]);
	PID_clear(&Dart_Data.motor2006_speed_pid[1]);
	Dart_Data.G2006_motor[1].speed_set = 0.0f;
	Dart_Data.G2006_motor[1].current_set = 0;
	Dart_Data.G2006_motor[1].give_current = 0;
}

static uint8_t dart_aux2006_rc_enabled(void)
{
	return (Dart_Data.RcDate != NULL &&
			switch_is_mid(Dart_Data.RcDate->rc.s[1]) &&
			switch_is_down(Dart_Data.RcDate->rc.s[0]));
}

static void M2006_Aux_Control(void)
{
	const motor_measure_t *aux_measure = Dart_Data.G2006_motor[1].trigger_motor_measure;
	int16_t rc_ch = 0;
	fp32 speed_set = 0.0f;
	int16_t current_give = 0;

	Dart_Data.RcDate = get_remote_control_point();

	if (Dart_Data.RcDate != NULL && dart_rc_safe_stop_requested())
	{
		M2006_Aux_ResetHold();
		CAN_Cmd2006_Aux(0);
		return;
	}

	if (aux_measure == NULL)
	{
		M2006_Aux_ResetHold();
		CAN_Cmd2006_Aux(0);
		return;
	}

	if (!aux2006_hold_initialized)
	{
		aux2006_position_set = (fp32)aux_measure->total_angle;
		PID_clear(&Dart_Data.motor2006_position_pid[1]);
		PID_clear(&Dart_Data.motor2006_speed_pid[1]);
		aux2006_hold_initialized = 1;
	}

	if (dart_aux2006_rc_enabled())
	{
		rc_ch = Dart_Data.RcDate->rc.ch[M2006_AUX_RC_CHANNEL];
		if (rc_ch > M2006_AUX_RC_DEADBAND || rc_ch < -M2006_AUX_RC_DEADBAND)
		{
			aux2006_position_set += (fp32)rc_ch * M2006_AUX_RC_TARGET_STEP;
		}
	}

	speed_set = PID_calc(&Dart_Data.motor2006_position_pid[1],
						 (fp32)aux_measure->total_angle,
						 aux2006_position_set);
	target_deadband_limit(speed_set, M2006_AUX_POSITION_PID_MAX_OUT, -M2006_AUX_POSITION_PID_MAX_OUT);

	Dart_Data.G2006_motor[1].speed_set = speed_set;

	current_give = (int16_t)PID_calc(&Dart_Data.motor2006_speed_pid[1],
									 (fp32)aux_measure->speed_rpm,
									 speed_set);
	target_deadband_limit(current_give, M2006_AUX_SPEED_PID_MAX_OUT, -M2006_AUX_SPEED_PID_MAX_OUT);

	Dart_Data.G2006_motor[1].current_set = current_give;
	Dart_Data.G2006_motor[1].give_current = current_give;
	CAN_Cmd2006_Aux(current_give);
}

static uint8_t dart_rc_control_enabled(void)
{
	return (Dart_Data.RcDate != NULL &&
			switch_is_down(Dart_Data.RcDate->rc.s[1]) &&
			switch_is_mid(Dart_Data.RcDate->rc.s[0]));
}

static uint8_t dart_rc_safe_stop_requested(void)
{
	return (Dart_Data.RcDate != NULL &&
			switch_is_up(Dart_Data.RcDate->rc.s[1]));
}

/**
  * @brief  遥控器输入处理
  *         根据遥控器拨杆状态设置飞镖发射模式和计数
  */
void shoot_rc(void)
{
	Dart_Data.RcDate = get_remote_control_point();
	
	if (Dart_Data.RcDate == NULL)
		return;

	switch (Dart_Data.current_dart)
	{
		case 0:
			Dart_Data.shoot_count = FRIST_SHOOT;
			break;
		case 1:
			Dart_Data.shoot_count = SECOND_SHOOT;
			break;
		case 2:
			Dart_Data.shoot_count = THIRD_SHOOT;
			break;
		case 3:
			Dart_Data.shoot_count = FORTH_SHOOT;
			break;
		default:
			Dart_Data.shoot_count = ZERO_SHOOT;
			break;
	}
	
	uint8_t left_switch = Dart_Data.RcDate->rc.s[1];
	uint8_t right_switch = Dart_Data.RcDate->rc.s[0];

	if (dart_rc_safe_stop_requested())
	{
		dart_enter_safe_stop();
		last_left_switch = left_switch;
		last_right_switch = right_switch;
		return;
	}
	
	if (!dart_system_enabled())
	{
		last_left_switch = left_switch;
		last_right_switch = right_switch;
		return;
	}

	dart_enable_outputs();
	Dart_Data.dart_mode = DART_STANDBY;
	dart_pop_up_flat = 0;
	dart_launch_flat = 0;
	
	last_left_switch = left_switch;
	last_right_switch = right_switch;
}

/* 调试变量 */
int cout=12000;
int i=0;
uint16_t time_out_up,time_out_down;

/* 电机位置记录 */
static int32_t motor1_move_start = 0;
static int32_t motor2_move_start = 0;
static int32_t motor1_initial_pos = 0;
static int32_t motor2_initial_pos = 0;
static int32_t reload_bottom_pos_1 = 0;
static int32_t reload_bottom_pos_2 = 0;
static uint8_t is_moving = 0;
static uint8_t last_shoot_count = ZERO_SHOOT;
static uint8_t reload_returned_bottom = 0;
static uint8_t reload_stepper_returned_origin = 0;
static uint8_t dart_match_fire_count = 0;
static uint8_t gate_open_last = 0;
static uint8_t gate_fire_count = 0;
static uint8_t gate_opening_count = 0;
static uint8_t gate_opening_edge_latched = 0;
static uint8_t first_dart_chamber_allowed = 0;
static uint8_t last_dart_launch_opening_status = 0;
static uint8_t last_referee_online = 0;
static uint32_t gate_status_open_tick = 0;
static uint32_t wait_gate_enter_tick = 0;
static uint8_t gate_fire_ready_latched = 0;
static uint8_t dart_auto_run_latched = 0;
static uint32_t dart_auto_run_start_tick = 0;
static uint16_t vision_auto_start_count = 0;
static uint8_t vision_found_last = 0;
static uint32_t vision_gate_open_tick = 0;
static uint32_t last_gate_open_event_tick = 0;
static uint32_t vision_align_start_tick = 0;
static uint8_t vision_align_candidate_last = 0;
static uint8_t gate_window_active = 0;
static uint32_t gate_window_open_tick = 0;
static uint8_t gate_window_from_vision = 0;
static uint8_t launch_servo_released = 0;
static uint8_t debug_run_enabled_last = 0;
volatile uint8_t pick_debug_reload_index = 0;
volatile uint8_t pick_debug_curve_phase = 0;
volatile uint32_t pick_debug_elapsed_ms = 0;
volatile int16_t pick_debug_start_current = 0;
volatile int16_t pick_debug_end_current = 0;
volatile int16_t pick_debug_output_current = 0;
volatile int16_t pick_debug_motor1_speed = 0;
volatile int16_t pick_debug_motor2_speed = 0;
volatile int32_t pick_debug_motor1_angle = 0;
volatile int32_t pick_debug_motor2_angle = 0;
volatile uint8_t launch_debug_block_reason = 0;
volatile uint8_t launch_debug_state = 0;
volatile uint8_t launch_debug_current_dart = 0;
volatile uint8_t launch_debug_up_hold_released = 0;
volatile uint8_t launch_debug_gate_fire_count = 0;
volatile uint8_t launch_debug_match_fire_count = 0;
volatile uint8_t launch_debug_vision_raw = 0;
volatile uint8_t launch_debug_vision_aligned = 0;
volatile uint8_t launch_debug_rc_enabled = 0;
volatile uint16_t launch_debug_up_hold_stable_count = 0;
volatile uint16_t launch_debug_up_hold_push_count = 0;
volatile uint16_t launch_debug_up_hold_elapsed_count = 0;
volatile int16_t launch_debug_motor1_speed = 0;
volatile int16_t launch_debug_motor2_speed = 0;
volatile uint8_t launch_debug_vision_packet_state = 0;
volatile uint8_t launch_debug_vision_direction = 0;
volatile uint8_t launch_debug_vision_target_found = 0;
volatile uint16_t launch_debug_vision_age_ms = 0;
volatile int16_t launch_debug_vision_yaw_mrad = 0;

/* 电机行程定义（脉冲数） */
//#define TRAVEL_DOWN 900000
#define TRAVEL_DOWN 848000
#define TRAVEL_UP 609000
#define TRAVEL_RELOAD_DOWN TRAVEL_DOWN


/* 保持电流（用于底部锁定位置） */
#define HOLD_CURRENT 7000
#define DOWN_3508_MIN_CURRENT 6000
#define RELOAD_DOWN_3508_MAX_CURRENT 14000.0f
#define DOWN_3508_RAMP_STEP 200.0f
#define DOWN_3508_FIRST_RAMP_STEP 300.0f
#define UP_3508_FAST_CURRENT 6500
#define UP_3508_SLOW_CURRENT 3500
#define UP_3508_RAMP_STEP 80.0f
#define UP_3508_SLOW_RAMP_STEP 40.0f
#define UP_3508_SLOWDOWN_PERCENT 80
#define UP_3508_FIRST_TARGET_PERCENT 100
#define UP_3508_TARGET_PERCENT 110
#define UP_3508_STOP_CHECK_PERCENT 95
#define UP_3508_HOLD_CURRENT 500
#define UP_3508_HOLD_STABLE_SPEED_RPM 35
#define UP_3508_HOLD_STABLE_MS 300
#define UP_3508_HOLD_FORCE_PUSH_MS 100
#define UP_3508_TOP_PUSH_CURRENT HOLD_CURRENT
#define UP_3508_TOP_PUSH_MS 700

/* 延时定义（毫秒） */
#define SERVO_LOCK_ANGLE 72
#define SERVO_RELEASE_ANGLE 100
#define FIRST_SHOOT_DOWN_TIMEOUT_MS 2800
#define FIRST_SHOOT_LOCK_DELAY_MS 3200
#define BOTTOM_SERVO_LOCK_WAIT_MS 1300
#define FIRST_SHOOT_UP_DELAY_MS 2000
#define RELOAD_DOWN_DELAY_MS 5500
#define SERVO_LOCK_DELAY_MS 200
#define LAUNCH_READY_DELAY_MS 100
#define LAUNCH_RELEASE_DELAY_MS 500
#define PICK_DART_PRE_UP_WAIT_MS 200
#define PICK_DART_UP_DELAY_MS 2000
#define PICK_DART_DOWN_DELAY_MS 600
#define PICK_DART_LOCK_DELAY_MS 1800
#define PICK_DART_RETURN_DOWN_TIMEOUT_MS 4500

/* 步进电机目标位置（脉冲数）：上电当前位置为原点，每次取镖后回原点 */
#define STEPPER_ORIGIN_POS       0
#define STEPPER_RELOAD_POS_1  7000
#define STEPPER_RELOAD_POS_2 12000
#define STEPPER_RELOAD_POS_3 16500
#define STEPPER_MOVE_WAIT_MS  3200
#define STEPPER_RETURN_WAIT_MS 0

/* 对抗力矩电流（mA）：第一次换弹和后续换弹分别调参 */
#define PICK_COUNTER_FIRST_START_CURRENT 1800
#define PICK_COUNTER_FIRST_END_CURRENT 1600
#define PICK_COUNTER_RELOAD_START_CURRENT 1600
#define PICK_COUNTER_RELOAD_END_CURRENT 1400
#define PICK_COUNTER_START_HOLD_MS 300
#define PICK_COUNTER_RAMP_END_MS 1500
#define PICK_DART_DOWN_START_CURRENT 7000
#define PICK_DART_DOWN_TARGET_CURRENT 9000
#define PICK_DART_DOWN_LATE_START_CURRENT 7000
#define PICK_DART_DOWN_LATE_TARGET_CURRENT 9500
#define PICK_DART_DOWN_RAMP_STEP 60.0f
#define PICK_DART_HOLD_CURRENT 7000
#define RELOAD_BOTTOM_TOLERANCE 2000
#define RELOAD_DOWN_CONFIRM_PERCENT 98
#define REFEREE_OFFLINE_TIMEOUT_MS 500
#define DART_MAX_COUNT 4
#define DART_GATE_MAX_FIRE_COUNT 2
#define DART_GATE_OPEN_STATUS 0
#define DART_GATE_CLOSED_STATUS 1
#define DART_GATE_MOVING_STATUS 2
#define VISION_TARGET_FOUND 1
#define VISION_TARGET_ALIGNED 0
#define VISION_TARGET_TIMEOUT_MS 500
#define VISION_ALIGN_YAW_DEADBAND_RAD 0.0175f
#define VISION_AUTO_START_CONFIRM_MS 200
#define VISION_GATE_OPEN_DELAY_MS 1000
#define VISION_GATE_EVENT_COOLDOWN_MS 1500
#define VISION_GATE_WINDOW_TIMEOUT_MS 6000
#define VISION_ALIGN_STABLE_MS VISION_GATE_OPEN_DELAY_MS
#define DART_AUTO_TOTAL_TIMEOUT_MS 300000
#define LAUNCH_BLOCK_NONE 0
#define LAUNCH_BLOCK_SYSTEM_DISABLED 1
#define LAUNCH_BLOCK_DART_MAX 2
#define LAUNCH_BLOCK_UP_NOT_RELEASED 3
#define LAUNCH_BLOCK_MATCH_FIRE_MAX 4
#define LAUNCH_BLOCK_GATE_FIRE_MAX 5
#define LAUNCH_BLOCK_VISION_NOT_ALIGNED 6
/* 3508到位判定：相对编码器行程 + 低速持续确认 + 超时兜底 */
#define MOTOR_STOP_SPEED_RPM 35
#define MOTOR_ARRIVE_STABLE_MS 120
#define TRAVEL_CONFIRM_PERCENT 85
#define LEARNED_TRAVEL_MIN 100000
#define DOWN_TIMEOUT_MS 6500
#define UP_TIMEOUT_MS 3500
#define FIRST_UP_TIMEOUT_MS 8000
#define RELOAD_DOWN_TIMEOUT_MS 6500

/* 状态机处理函数声明 */
static int32_t abs_i32(int32_t value);
static uint8_t tick_after(uint32_t tick, uint32_t base);
static uint8_t vision_target_raw_valid(void);
static uint8_t vision_target_wait_valid(void);
static uint8_t vision_target_launch_valid(void);
static uint8_t vision_target_aligned(void);
static uint8_t dart_gate_status_open(void);
static uint8_t dart_gate_fire_ready(void);
static uint8_t dart_prepare_start_allowed(void);
static uint8_t dart_referee_allow_launch(void);
static uint8_t dart_system_enabled(void);
static uint8_t dart_auto_run_timed_out(void);
static void update_dart_match_control(void);
static void update_dart_auto_control(void);
static void update_dart_gate_window(void);
static void reset_dart_window_control(void);
static void register_gate_open_event(uint32_t now, uint8_t from_vision);
static void reset_vision_align_confirm(void);
static void reset_current_dart_vision_wait(void);
static void enter_wait_gate_state(void);
static void stop_and_wait_gate(void);
static void update_yaw_aim_control(void);
static void hold_3508_up(void);
static void reset_3508_up_hold(void);
static int32_t get_down_travel_ref(void);
static int32_t get_up_travel_ref(void);
static int32_t get_travel_confirm_value(int32_t travel_ref);
static int16_t get_pick_counter_current(uint32_t elapsed_ms);
static void reset_motion_arrive_counter(void);
static uint8_t both_3508_speed_low(void);
static uint8_t motion_arrive_confirmed(uint8_t condition);
static void learn_bottom_position(void);
static void learn_top_position(void);
static void handle_down_movement(void);
static void handle_down_hold(void);
static void handle_up_movement(void);
static void handle_launch(void);
static void handle_down_after_launch(void);
static void handle_stepper_move(void);
static void handle_release_up(void);

static int32_t abs_i32(int32_t value)
{
	return (value >= 0) ? value : -value;
}

static uint8_t tick_after(uint32_t tick, uint32_t base)
{
	return ((int32_t)(tick - base) > 0);
}

static uint8_t referee_is_online(void)
{
#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
		return 1;
#endif

	uint32_t now = xTaskGetTickCount();

	return (referee_last_update_time != 0 &&
			(now - referee_last_update_time) <= REFEREE_OFFLINE_TIMEOUT_MS);
}

static uint8_t referee_data_time_valid(uint32_t update_time)
{
	uint32_t now = xTaskGetTickCount();

	return (update_time != 0 &&
			(now - update_time) <= REFEREE_OFFLINE_TIMEOUT_MS);
}

static uint8_t referee_game_state_valid(void)
{
#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
		return 1;
#endif

	return referee_data_time_valid(referee_data.game_state_update_time);
}

static uint8_t referee_dart_cmd_valid(void)
{
#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
		return 1;
#endif

	return referee_data_time_valid(referee_data.dart_client_cmd_update_time);
}

static uint8_t dart_game_started(void)
{
#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
		return 1;
#endif

	return (dart_auto_run_latched ||
			(referee_game_state_valid() &&
			referee_data.game_progress == PROGRESS_BATTLE));
}

static void dart_enable_outputs(void)
{
	if (stepper_disabled_by_rc)
	{
		Emm_V5_En_Control(1, 1, 0);
		stepper_disabled_by_rc = 0;
	}
}

static void dart_all_motor_disable(void)
{
	CAN_CmdDart3508(0, 0, 0, 0);
	CAN_Cmd2006_All(0, 0);
	M2006_Aux_ResetHold();
	servo_set_angle(SERVO_LOCK_ANGLE, 0);
	vision_set_mode(VISION_MODE_DISABLED);
	vision_packet.state = 0;
	vision_control.target.target_found = 0;

	if (!stepper_disabled_by_rc)
	{
		Emm_V5_Stop_Now(1, 0);
		Emm_V5_En_Control(1, 0, 0);
		stepper_disabled_by_rc = 1;
	}
}

static void dart_enter_safe_stop(void)
{
	Dart_Data.dart_mode = DART_STANDBY;
	Dart_Data.dart_state = DART_STATE_IDLE;
	Dart_Data.state_timer = 0;
	Dart_Data.shoot_count = ZERO_SHOOT;
	dart_pop_up_flat = 0;
	dart_launch_flat = 0;
	dart_auto_run_latched = 0;
	dart_auto_run_start_tick = 0;
	reset_dart_window_control();
#if DART_REFEREE_TEST_BYPASS
	gate_open_last = 0;
	gate_fire_count = 0;
#endif
	reset_motion_arrive_counter();
	dart_all_motor_disable();
}

static uint8_t vision_target_raw_valid(void)
{
	uint32_t now = xTaskGetTickCount();

	return (vision_control.target.target_found != 0 &&
			vision_packet.state != 0 &&
			(now - vision_control.target.last_update_time) <= VISION_TARGET_TIMEOUT_MS);
}

static uint8_t vision_target_wait_valid(void)
{
	if (!vision_target_raw_valid())
		return 0;
	if (wait_gate_enter_tick != 0 &&
		!tick_after(vision_control.target.last_update_time, wait_gate_enter_tick))
		return 0;

	return 1;
}

static uint8_t vision_target_launch_valid(void)
{
	if (!vision_target_wait_valid())
		return 0;

	return 1;
}

static uint8_t vision_target_aligned(void)
{
	uint32_t now = xTaskGetTickCount();
	uint32_t target_age = now - vision_control.target.last_update_time;
	float yaw_abs = vision_packet.yaw;
	int32_t yaw_mrad = (int32_t)(vision_packet.yaw * 1000.0f);
	uint8_t align_candidate = 0;

	if (yaw_abs < 0.0f)
		yaw_abs = -yaw_abs;
	if (yaw_mrad > 32767)
		yaw_mrad = 32767;
	else if (yaw_mrad < -32768)
		yaw_mrad = -32768;

	launch_debug_vision_packet_state = vision_packet.state;
	launch_debug_vision_direction = vision_packet.direction;
	launch_debug_vision_target_found = vision_control.target.target_found;
	launch_debug_vision_age_ms = (target_age > 65535) ? 65535 : (uint16_t)target_age;
	launch_debug_vision_yaw_mrad = (int16_t)yaw_mrad;

	align_candidate = (vision_target_launch_valid() &&
		vision_packet.direction == VISION_TARGET_ALIGNED &&
		yaw_abs <= VISION_ALIGN_YAW_DEADBAND_RAD);

	if (!align_candidate)
	{
		vision_align_candidate_last = 0;
		vision_align_start_tick = 0;
		return 0;
	}

	if (!vision_align_candidate_last)
	{
		vision_align_candidate_last = 1;
		vision_align_start_tick = now;
		return 0;
	}

	return ((now - vision_align_start_tick) >= VISION_ALIGN_STABLE_MS);
}

static uint8_t dart_gate_status_open(void)
{
#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
		return 1;
#endif

	return (referee_dart_cmd_valid() &&
			dart_game_started() &&
			referee_data.dart_launch_opening_status == DART_GATE_OPEN_STATUS);
}

static uint8_t dart_gate_fire_ready(void)
{
	uint32_t now = xTaskGetTickCount();
	uint8_t vision_gate_ready = 0;
	uint8_t dart_cmd_valid = referee_dart_cmd_valid();

	if (gate_window_active &&
		(now - gate_window_open_tick) > VISION_GATE_WINDOW_TIMEOUT_MS &&
		gate_fire_count == 0)
	{
		gate_window_active = 0;
		gate_window_from_vision = 0;
	}

	if (dart_cmd_valid)
	{
		vision_gate_open_tick = 0;
		if (dart_gate_status_open())
		{
			if (gate_status_open_tick == 0)
				gate_status_open_tick = now;
			if ((now - gate_status_open_tick) >= VISION_GATE_OPEN_DELAY_MS)
				return 1;
		}
		else
		{
			gate_status_open_tick = 0;
		}

		return 0;
	}

	if (vision_gate_open_tick != 0 &&
		vision_target_raw_valid() &&
		(now - vision_gate_open_tick) >= VISION_GATE_OPEN_DELAY_MS &&
		(now - vision_gate_open_tick) <= VISION_GATE_WINDOW_TIMEOUT_MS)
	{
		vision_gate_ready = 1;
	}

	return vision_gate_ready;
}

static uint8_t dart_auto_run_timed_out(void)
{
	uint32_t now = xTaskGetTickCount();

	return (dart_auto_run_latched &&
			dart_auto_run_start_tick != 0 &&
			(now - dart_auto_run_start_tick) > DART_AUTO_TOTAL_TIMEOUT_MS);
}

static uint8_t dart_system_enabled(void)
{
	if (Dart_Data.current_dart >= DART_MAX_COUNT)
		return 0;
	if (dart_auto_run_timed_out())
		return 0;

	return dart_game_started();
}

static uint8_t dart_prepare_start_allowed(void)
{
#if DART_REFEREE_TEST_BYPASS
	return (dart_rc_control_enabled() &&
			Dart_Data.current_dart < DART_MAX_COUNT);
#else
	return (dart_system_enabled() &&
			Dart_Data.current_dart < DART_MAX_COUNT &&
			(gate_opening_count > 0 ||
			 (Dart_Data.current_dart == 0 && first_dart_chamber_allowed)));
#endif
}

static uint8_t dart_referee_allow_launch(void)
{
	uint8_t vision_aligned = 0;

	launch_debug_state = Dart_Data.dart_state;
	launch_debug_current_dart = Dart_Data.current_dart;
	launch_debug_up_hold_released = up_hold_current_released;
	launch_debug_gate_fire_count = gate_fire_count;
	launch_debug_match_fire_count = dart_match_fire_count;
	launch_debug_vision_raw = vision_target_raw_valid();
	launch_debug_vision_aligned = 0;
	launch_debug_rc_enabled = dart_rc_control_enabled();

	if (!dart_system_enabled())
	{
		launch_debug_block_reason = LAUNCH_BLOCK_SYSTEM_DISABLED;
		return 0;
	}

#if DART_REFEREE_TEST_BYPASS
	if (Dart_Data.current_dart >= DART_MAX_COUNT)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_DART_MAX;
		return 0;
	}
	if (!up_hold_current_released)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_UP_NOT_RELEASED;
		return 0;
	}
	if (dart_match_fire_count >= DART_MAX_COUNT)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_MATCH_FIRE_MAX;
		return 0;
	}
	vision_aligned = vision_target_aligned();
	launch_debug_vision_aligned = vision_aligned;
	if (!vision_aligned)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_VISION_NOT_ALIGNED;
		return 0;
	}

	launch_debug_block_reason = LAUNCH_BLOCK_NONE;
	return 1;
#else
	if (Dart_Data.current_dart >= DART_MAX_COUNT)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_DART_MAX;
		return 0;
	}
	if (!dart_gate_fire_ready())
	{
		gate_fire_ready_latched = 0;
		reset_vision_align_confirm();
		launch_debug_block_reason = LAUNCH_BLOCK_GATE_FIRE_MAX;
		return 0;
	}
	if (!gate_fire_ready_latched)
	{
		gate_fire_ready_latched = 1;
		reset_vision_align_confirm();
		launch_debug_block_reason = LAUNCH_BLOCK_VISION_NOT_ALIGNED;
		return 0;
	}
	if (Dart_Data.current_dart == 2 && gate_opening_count < 2)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_GATE_FIRE_MAX;
		return 0;
	}
	if (!up_hold_current_released)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_UP_NOT_RELEASED;
		return 0;
	}
	if (dart_match_fire_count >= DART_MAX_COUNT)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_MATCH_FIRE_MAX;
		return 0;
	}
	if (gate_fire_count >= DART_GATE_MAX_FIRE_COUNT)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_GATE_FIRE_MAX;
		return 0;
	}
	vision_aligned = vision_target_aligned();
	launch_debug_vision_aligned = vision_aligned;
	if (!vision_aligned)
	{
		launch_debug_block_reason = LAUNCH_BLOCK_VISION_NOT_ALIGNED;
		return 0;
	}

	launch_debug_block_reason = LAUNCH_BLOCK_NONE;
	return 1;
#endif
}

static void update_dart_gate_window(void)
{
	uint8_t gate_open_now = dart_gate_status_open();
	uint32_t now = xTaskGetTickCount();

	if (gate_open_now && !gate_open_last)
	{
		gate_status_open_tick = now;
		reset_vision_align_confirm();
	}
	else if (!gate_open_now)
	{
		gate_status_open_tick = 0;
	}

	gate_open_last = gate_open_now;
}

static void register_gate_open_event(uint32_t now, uint8_t from_vision)
{
	if (gate_window_active &&
		gate_fire_count < DART_GATE_MAX_FIRE_COUNT &&
		((now - gate_window_open_tick) <= VISION_GATE_WINDOW_TIMEOUT_MS ||
		 gate_fire_count > 0))
	{
		last_gate_open_event_tick = now;
		return;
	}

	gate_window_active = 1;
	gate_window_open_tick = now;
	gate_window_from_vision = from_vision;
	gate_opening_count++;
	gate_fire_count = 0;
	last_gate_open_event_tick = now;
}

static void reset_vision_align_confirm(void)
{
	vision_align_start_tick = 0;
	vision_align_candidate_last = 0;
}

static void start_current_dart_vision_wait(uint32_t now)
{
	vision_gate_open_tick = now;
	reset_vision_align_confirm();
	if (!referee_dart_cmd_valid())
	{
		register_gate_open_event(now, 1);
	}
}

static void reset_current_dart_vision_wait(void)
{
	uint32_t now = xTaskGetTickCount();
	uint8_t vision_found_now = vision_target_wait_valid();

	vision_gate_open_tick = 0;
	vision_found_last = vision_found_now;
	reset_vision_align_confirm();

	if (vision_found_now)
	{
		start_current_dart_vision_wait(now);
	}
}

static void enter_wait_gate_state(void)
{
	wait_gate_enter_tick = xTaskGetTickCount();
	gate_status_open_tick = 0;
	gate_fire_ready_latched = 0;
	Dart_Data.dart_state = DART_STATE_WAIT_GATE;
	Dart_Data.state_timer = 0;
	launch_servo_released = 0;
	reset_current_dart_vision_wait();
}

static void update_dart_match_control(void)
{
	uint8_t referee_online = referee_is_online();
	uint8_t game_state_valid = referee_game_state_valid();
	uint8_t dart_cmd_valid = referee_dart_cmd_valid();
	uint8_t first_dart_chamber_edge = (dart_cmd_valid &&
		Dart_Data.current_dart == 0 &&
		last_dart_launch_opening_status == DART_GATE_CLOSED_STATUS &&
		referee_data.dart_launch_opening_status == DART_GATE_MOVING_STATUS);

#if DART_REFEREE_TEST_BYPASS
	if (dart_rc_control_enabled())
	{
		uint32_t now = xTaskGetTickCount();
		uint8_t debug_run_start_edge = !debug_run_enabled_last;

		debug_run_enabled_last = 1;

		if (debug_run_start_edge)
		{
			Dart_Data.current_dart = 0;
			dart_match_fire_count = 0;
			gate_fire_count = 0;
			gate_opening_count = 0;
			first_dart_chamber_allowed = 0;
			gate_status_open_tick = 0;
			wait_gate_enter_tick = 0;
			gate_fire_ready_latched = 0;
			gate_opening_edge_latched = 0;
			gate_window_active = 0;
			gate_window_open_tick = 0;
			gate_window_from_vision = 0;
			launch_servo_released = 0;
			reset_vision_align_confirm();
		}

		if (!dart_auto_run_latched)
		{
			dart_auto_run_latched = 1;
			dart_auto_run_start_tick = now;
		}

		if (!gate_window_active)
		{
			register_gate_open_event(now, 0);
		}

		gate_open_last = 1;
		last_game_progress = PROGRESS_BATTLE;
		last_dart_launch_opening_status = DART_GATE_OPEN_STATUS;
		last_referee_online = 1;
		return;
	}
#endif

	if (!referee_online)
	{
		gate_open_last = 0;
		gate_status_open_tick = 0;
		gate_fire_ready_latched = 0;
		gate_opening_edge_latched = 0;
		last_referee_online = 0;
		return;
	}

	if (!last_referee_online)
	{
		gate_opening_edge_latched = 0;
		if (dart_cmd_valid)
		{
			last_dart_launch_opening_status = referee_data.dart_launch_opening_status;
		}
	}
	last_referee_online = 1;

	if (game_state_valid &&
		last_game_progress != PROGRESS_BATTLE &&
		referee_data.game_progress == PROGRESS_BATTLE)
	{
		dart_auto_run_latched = 1;
		dart_auto_run_start_tick = xTaskGetTickCount();
		vision_auto_start_count = 0;
		Dart_Data.current_dart = 0;
		dart_match_fire_count = 0;
		gate_open_last = 0;
		gate_fire_count = 0;
		gate_opening_count = 0;
		first_dart_chamber_allowed = 0;
		gate_status_open_tick = 0;
		wait_gate_enter_tick = 0;
		gate_fire_ready_latched = 0;
		gate_opening_edge_latched = 0;
		gate_window_active = 0;
		gate_window_open_tick = 0;
		gate_window_from_vision = 0;
		launch_servo_released = 0;
		if (dart_cmd_valid)
		{
			last_dart_launch_opening_status = referee_data.dart_launch_opening_status;
		}
	}

	if (game_state_valid &&
		referee_data.game_progress != PROGRESS_BATTLE)
	{
		dart_auto_run_latched = 0;
		dart_auto_run_start_tick = 0;
		vision_auto_start_count = 0;
		dart_match_fire_count = 0;
		gate_open_last = 0;
		gate_fire_count = 0;
		gate_opening_count = 0;
		first_dart_chamber_allowed = 0;
		gate_status_open_tick = 0;
		wait_gate_enter_tick = 0;
		gate_fire_ready_latched = 0;
		gate_opening_edge_latched = 0;
		gate_window_active = 0;
		gate_window_open_tick = 0;
		gate_window_from_vision = 0;
		launch_servo_released = 0;
		if (dart_cmd_valid)
		{
			last_dart_launch_opening_status = referee_data.dart_launch_opening_status;
		}
		last_game_progress = referee_data.game_progress;
		return;
	}

	if (first_dart_chamber_edge)
	{
		first_dart_chamber_allowed = 1;
	}

	gate_opening_edge_latched = (dart_cmd_valid &&
		last_dart_launch_opening_status != DART_GATE_OPEN_STATUS &&
		referee_data.dart_launch_opening_status == DART_GATE_OPEN_STATUS);
	if (gate_opening_edge_latched)
	{
		uint32_t now = xTaskGetTickCount();

		register_gate_open_event(now, 0);
	}
	if (dart_cmd_valid)
	{
		last_dart_launch_opening_status = referee_data.dart_launch_opening_status;
	}
	if (game_state_valid)
	{
		last_game_progress = referee_data.game_progress;
	}
	update_dart_gate_window();
}

static void update_dart_auto_control(void)
{
	uint32_t now = xTaskGetTickCount();
	uint8_t vision_found_now = vision_target_raw_valid();
	uint8_t vision_wait_valid = vision_target_wait_valid();
	uint8_t dart_cmd_valid = referee_dart_cmd_valid();
	uint8_t referee_allows_auto = (!referee_game_state_valid() ||
		referee_data.game_progress == PROGRESS_BATTLE);

	if (Dart_Data.dart_state == DART_STATE_WAIT_GATE)
	{
		if (!dart_cmd_valid && vision_wait_valid)
		{
			if (vision_gate_open_tick == 0)
			{
				start_current_dart_vision_wait(now);
			}
		}
		else
		{
			vision_gate_open_tick = 0;
			if (!vision_wait_valid)
			{
				reset_vision_align_confirm();
			}
		}
	}

	if (!dart_cmd_valid &&
		vision_found_now && !vision_found_last &&
		Dart_Data.dart_state == DART_STATE_IDLE)
	{
		if (last_gate_open_event_tick == 0 ||
			(now - last_gate_open_event_tick) >= VISION_GATE_EVENT_COOLDOWN_MS)
		{
			register_gate_open_event(now, 1);
		}
	}
	else if (!vision_found_now &&
		Dart_Data.dart_state != DART_STATE_WAIT_GATE)
	{
		vision_gate_open_tick = 0;
	}
	vision_found_last = (Dart_Data.dart_state == DART_STATE_WAIT_GATE) ?
		vision_wait_valid : vision_found_now;

	if (!dart_auto_run_latched &&
		referee_allows_auto &&
		Dart_Data.dart_state == DART_STATE_IDLE &&
		Dart_Data.current_dart == 0 &&
		vision_found_now)
	{
		if (vision_auto_start_count < VISION_AUTO_START_CONFIRM_MS)
		{
			vision_auto_start_count++;
		}

		if (vision_auto_start_count >= VISION_AUTO_START_CONFIRM_MS)
		{
			dart_auto_run_latched = 1;
			dart_auto_run_start_tick = now;
		}
	}
	else if (!vision_found_now || dart_auto_run_latched || !referee_allows_auto)
	{
		vision_auto_start_count = 0;
	}
}

static void reset_dart_window_control(void)
{
	last_prepare_start_allowed = 0;
	gate_open_last = dart_gate_status_open();
	gate_status_open_tick = gate_open_last ? xTaskGetTickCount() : 0;
	wait_gate_enter_tick = 0;
	gate_fire_ready_latched = 0;
	gate_opening_count = 0;
	gate_opening_edge_latched = 0;
	first_dart_chamber_allowed = 0;
	last_dart_launch_opening_status = referee_dart_cmd_valid() ? referee_data.dart_launch_opening_status : 0;
	last_referee_online = referee_is_online();
	vision_found_last = 0;
	vision_gate_open_tick = 0;
	reset_vision_align_confirm();
	last_gate_open_event_tick = 0;
	gate_window_active = 0;
	gate_window_open_tick = 0;
	gate_window_from_vision = 0;
	launch_servo_released = 0;
	vision_auto_start_count = 0;
}

static void stop_and_wait_gate(void)
{
	hold_3508_up();
	servo_set_angle(SERVO_LOCK_ANGLE, 0);
	enter_wait_gate_state();
}

static void hold_3508_up(void)
{
	uint8_t speed_low = 0;

	if (up_hold_elapsed_count < 65535)
	{
		up_hold_elapsed_count++;
	}

	if (Dart_Data.G3508_motor[0].chassis_motor_measure != NULL &&
		Dart_Data.G3508_motor[1].chassis_motor_measure != NULL)
	{
		speed_low = (abs_i32(Dart_Data.G3508_motor[0].chassis_motor_measure->speed_rpm) <= UP_3508_HOLD_STABLE_SPEED_RPM &&
			abs_i32(Dart_Data.G3508_motor[1].chassis_motor_measure->speed_rpm) <= UP_3508_HOLD_STABLE_SPEED_RPM);
		launch_debug_motor1_speed = Dart_Data.G3508_motor[0].chassis_motor_measure->speed_rpm;
		launch_debug_motor2_speed = Dart_Data.G3508_motor[1].chassis_motor_measure->speed_rpm;
	}

	launch_debug_up_hold_stable_count = up_hold_stable_count;
	launch_debug_up_hold_push_count = up_hold_push_count;
	launch_debug_up_hold_elapsed_count = up_hold_elapsed_count;

	if (up_hold_current_released)
	{
		CAN_CmdDart3508(0, 0, 0, 0);
		return;
	}

	if (up_hold_stable_count < UP_3508_HOLD_STABLE_MS &&
		up_hold_elapsed_count < UP_3508_HOLD_FORCE_PUSH_MS)
	{
		if (speed_low)
		{
			up_hold_stable_count++;
		}
		else
		{
			up_hold_stable_count = 0;
			up_hold_push_count = 0;
		}

		CAN_CmdDart3508(-UP_3508_HOLD_CURRENT, UP_3508_HOLD_CURRENT, 0, 0);
		return;
	}

	if (up_hold_stable_count < UP_3508_HOLD_STABLE_MS)
	{
		up_hold_stable_count = UP_3508_HOLD_STABLE_MS;
	}

	if (up_hold_push_count < UP_3508_TOP_PUSH_MS)
	{
		up_hold_push_count++;
		CAN_CmdDart3508(-UP_3508_TOP_PUSH_CURRENT, UP_3508_TOP_PUSH_CURRENT, 0, 0);
	}
	else
	{
		up_hold_current_released = 1;
		launch_debug_up_hold_released = 1;
		CAN_CmdDart3508(0, 0, 0, 0);
	}
}

static void reset_3508_up_hold(void)
{
	up_hold_stable_count = 0;
	up_hold_push_count = 0;
	up_hold_elapsed_count = 0;
	up_hold_current_released = 0;
}

static void update_yaw_aim_control(void)
{
	if (!dart_system_enabled())
	{
		CAN_Cmd2006(0);
		return;
	}

	if (vision_control.enable)
	{
		return;
	}

	if (vision_target_raw_valid())
	{
		int16_t current_give = (int16_t)vision_calculate_yaw_output();

		target_deadband_limit(current_give, 10000, -10000);
		CAN_Cmd2006(current_give);
	}
	else if (dart_rc_control_enabled() &&
			 Dart_Data.RcDate != NULL &&
			 Dart_Data.G2006_motor[0].trigger_motor_measure != NULL)
	{
		M2006_Control();
	}
}

static uint16_t motion_arrive_stable_count = 0;
static int32_t first_start_pos_1 = 0;
static int32_t first_start_pos_2 = 0;
static int32_t first_bottom_pos_1 = 0;
static int32_t first_bottom_pos_2 = 0;
static int32_t first_top_pos_1 = 0;
static int32_t first_top_pos_2 = 0;
static int32_t learned_down_travel = TRAVEL_DOWN;
static int32_t learned_up_travel = TRAVEL_UP;
static uint8_t learned_down_valid = 0;
static uint8_t learned_up_valid = 0;

static int32_t get_down_travel_ref(void)
{
	return learned_down_valid ? learned_down_travel : TRAVEL_DOWN;
}

static int32_t get_up_travel_ref(void)
{
	return learned_up_valid ? learned_up_travel : TRAVEL_UP;
}

static int32_t get_travel_confirm_value(int32_t travel_ref)
{
	return travel_ref * TRAVEL_CONFIRM_PERCENT / 100;
}

static int16_t get_pick_counter_current(uint32_t elapsed_ms)
{
	int16_t start_current = PICK_COUNTER_RELOAD_START_CURRENT;
	int16_t end_current = PICK_COUNTER_RELOAD_END_CURRENT;
	int32_t current_range = 0;
	int32_t ramp_elapsed = 0;
	int32_t ramp_time = PICK_COUNTER_RAMP_END_MS - PICK_COUNTER_START_HOLD_MS;
	int16_t output_current = 0;

	if (Dart_Data.current_dart == 1)
	{
		start_current = PICK_COUNTER_FIRST_START_CURRENT;
		end_current = PICK_COUNTER_FIRST_END_CURRENT;
	}

	if (elapsed_ms < PICK_COUNTER_START_HOLD_MS)
	{
		output_current = start_current;
		pick_debug_curve_phase = 0;
	}
	else if (elapsed_ms < PICK_COUNTER_RAMP_END_MS)
	{
		current_range = start_current - end_current;
		ramp_elapsed = elapsed_ms - PICK_COUNTER_START_HOLD_MS;
		output_current = (int16_t)(start_current - current_range * ramp_elapsed / ramp_time);
		pick_debug_curve_phase = 1;
	}
	else
	{
		output_current = end_current;
		pick_debug_curve_phase = 2;
	}

	pick_debug_reload_index = Dart_Data.current_dart;
	pick_debug_elapsed_ms = elapsed_ms;
	pick_debug_start_current = start_current;
	pick_debug_end_current = end_current;
	pick_debug_output_current = output_current;

	return output_current;
}

static void reset_motion_arrive_counter(void)
{
	motion_arrive_stable_count = 0;
}




static uint8_t both_3508_speed_low(void)
{
	return (abs_i32(Dart_Data.G3508_motor[0].chassis_motor_measure->speed_rpm) <= MOTOR_STOP_SPEED_RPM &&
			abs_i32(Dart_Data.G3508_motor[1].chassis_motor_measure->speed_rpm) <= MOTOR_STOP_SPEED_RPM);
}

static uint8_t motion_arrive_confirmed(uint8_t condition)
{
	if (condition)
	{
		if (motion_arrive_stable_count < MOTOR_ARRIVE_STABLE_MS)
		{
			motion_arrive_stable_count++;
		}
	}
	else
	{
		motion_arrive_stable_count = 0;
	}

	return (motion_arrive_stable_count >= MOTOR_ARRIVE_STABLE_MS);
}

static void learn_bottom_position(void)
{
	int32_t down_travel_1 = 0;
	int32_t down_travel_2 = 0;
	int32_t down_travel_avg = 0;

	first_bottom_pos_1 = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	first_bottom_pos_2 = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
	reload_bottom_pos_1 = first_bottom_pos_1;
	reload_bottom_pos_2 = first_bottom_pos_2;
	reload_bottom_ref_valid = 1;

	down_travel_1 = abs_i32(first_bottom_pos_1 - first_start_pos_1);
	down_travel_2 = abs_i32(first_bottom_pos_2 - first_start_pos_2);
	down_travel_avg = (down_travel_1 + down_travel_2) / 2;

	if (down_travel_avg > LEARNED_TRAVEL_MIN)
	{
		learned_down_travel = down_travel_avg;
		learned_down_valid = 1;
	}
}

static void learn_top_position(void)
{
	int32_t up_travel_1 = 0;
	int32_t up_travel_2 = 0;
	int32_t up_travel_avg = 0;

	first_top_pos_1 = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	first_top_pos_2 = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;

	up_travel_1 = abs_i32(first_top_pos_1 - first_bottom_pos_1);
	up_travel_2 = abs_i32(first_top_pos_2 - first_bottom_pos_2);
	up_travel_avg = (up_travel_1 + up_travel_2) / 2;

	if (up_travel_avg > LEARNED_TRAVEL_MIN)
	{
		learned_up_travel = up_travel_avg;
		learned_up_valid = 1;
	}
}

/**
  * @brief  飞镖发射主函数
  *         根据当前状态执行相应的状态机处理
  */
void shoot()
{
	static uint32_t shoot_timer = 0;
	uint8_t prepare_start_allowed = 0;
	uint8_t prepare_start_edge = 0;
    shoot_timer++;

	u=0;

	Dart_Data.RcDate = get_remote_control_point();
	if (dart_rc_safe_stop_requested())
	{
		dart_enter_safe_stop();
		return;
	}

#if DART_REFEREE_TEST_BYPASS
	if (!dart_rc_control_enabled())
	{
		debug_run_enabled_last = 0;
		dart_enter_safe_stop();
		return;
	}
#endif

	update_dart_match_control();
	update_dart_auto_control();

	if (dart_auto_run_timed_out())
	{
		dart_enter_safe_stop();
		return;
	}

	if (!dart_system_enabled())
	{
		if (Dart_Data.dart_state != DART_STATE_IDLE)
		{
			dart_enter_safe_stop();
		}
		last_prepare_start_allowed = 0;
		CAN_Cmd2006(0);
		return;
	}

	dart_enable_outputs();

	if (Dart_Data.dart_state != DART_STATE_IDLE ||
		Dart_Data.current_dart < DART_MAX_COUNT)
	{
		update_yaw_aim_control();
	}
	prepare_start_allowed = dart_prepare_start_allowed();
	prepare_start_edge = (prepare_start_allowed && !last_prepare_start_allowed);
	last_prepare_start_allowed = prepare_start_allowed;
		
	/* 飞镖发射状态机 */
	switch (Dart_Data.dart_state)
	{
		case DART_STATE_IDLE:
			if (Dart_Data.current_dart < DART_MAX_COUNT &&
				(prepare_start_allowed || prepare_start_edge))
			{
				Dart_Data.dart_state = DART_STATE_DOWN;
				Dart_Data.state_timer = 0;
				motor1_move_start = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
				motor2_move_start = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
				motor1_initial_pos = motor1_move_start;
				motor2_initial_pos = motor2_move_start;
				first_start_pos_1 = motor1_move_start;
				first_start_pos_2 = motor2_move_start;
				reload_bottom_ref_valid = 0;
				learned_down_valid = 0;
				learned_up_valid = 0;
				ramp_init(&fire_left_down, 0.02, 14000.0f, 0.0f);
				ramp_init(&fire_right_down, 0.02, 0.0f, -14000.0f);
				reset_motion_arrive_counter();
				is_moving = 1;
				i = 0;
			}
			break;
			
		case DART_STATE_DOWN:
			handle_down_movement();
			break;
			
		case DART_STATE_DOWN_HOLD:
			handle_down_hold();
			break;
			
		case DART_STATE_UP:
			handle_up_movement();
			break;

		case DART_STATE_WAIT_GATE:
			hold_3508_up();
			if (DART_RUN_MODE == 1 &&
				dart_referee_allow_launch())
			{
				Dart_Data.dart_state = DART_STATE_LAUNCH;
				Dart_Data.state_timer = 0;
				launch_servo_released = 0;
				dart_auto_run_start_tick = xTaskGetTickCount();
			}
			break;
			
		case DART_STATE_LAUNCH:
			handle_launch();
			break;
			
		case DART_STATE_DOWN_AFTER_LAUNCH:
			handle_down_after_launch();
			break;
			
		case DART_STATE_STEPPER_MOVE:
			handle_stepper_move();
			break;
			
		case DART_STATE_RELEASE_UP:
			handle_release_up();
			break;
			
		default:
			Dart_Data.dart_state = DART_STATE_IDLE;
			break;
	}
}

/**
  * @brief  处理下压动作
  *         控制GM3508电机将飞镖压入发射位置
  */
static void handle_down_movement(void)
{
	int32_t motor1_current = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	int32_t motor2_current = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
	int32_t motor1_moved = motor1_current - motor1_move_start;
	int32_t motor2_moved = motor2_current - motor2_move_start;
	int32_t motor1_moved_abs = abs_i32(motor1_moved);
	int32_t motor2_moved_abs = abs_i32(motor2_moved);
	int32_t avg_moved_abs = (motor1_moved_abs + motor2_moved_abs) / 2;
	int32_t down_travel_ref = get_down_travel_ref();
	uint8_t first_shot_learning = (Dart_Data.current_dart == 0 && !learned_down_valid);
	uint8_t latch_pressed = (avg_moved_abs >= get_travel_confirm_value(down_travel_ref) && both_3508_speed_low());
	uint8_t arrive_confirmed = first_shot_learning ? 0 : motion_arrive_confirmed(latch_pressed);
	uint32_t down_timeout = first_shot_learning ? FIRST_SHOOT_DOWN_TIMEOUT_MS : DOWN_TIMEOUT_MS;
	fp32 down_ramp_step = first_shot_learning ? DOWN_3508_FIRST_RAMP_STEP : DOWN_3508_RAMP_STEP;
	
	if (!arrive_confirmed && Dart_Data.state_timer < down_timeout)
	{
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		ramp_calc(&fire_left_down, down_ramp_step);
		ramp_calc(&fire_right_down, -down_ramp_step);
		CAN_CmdDart3508(fire_left_down.out, fire_right_down.out, 0, 0);
		Dart_Data.state_timer++;
	}
	else
	{
		reset_motion_arrive_counter();
		Dart_Data.dart_state = DART_STATE_DOWN_HOLD;
	}
}

/**
  * @brief  处理下压保持
  *         保持电流锁定飞镖位置，延时后切换到上抬状态
  */
static void handle_down_hold(void)
{
	if (Dart_Data.state_timer < FIRST_SHOOT_LOCK_DELAY_MS)
	{
		CAN_CmdDart3508(HOLD_CURRENT, -HOLD_CURRENT, 0, 0);
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		Dart_Data.state_timer++;
		return;
	}

	if (Dart_Data.state_timer < (FIRST_SHOOT_LOCK_DELAY_MS + BOTTOM_SERVO_LOCK_WAIT_MS))
	{
		CAN_CmdDart3508(HOLD_CURRENT, -HOLD_CURRENT, 0, 0);
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		if (!reload_bottom_ref_valid)
		{
			learn_bottom_position();
		}
		Dart_Data.state_timer++;
		return;
	}

	CAN_CmdDart3508(0, 0, 0, 0);
	servo_set_angle(SERVO_LOCK_ANGLE, 0);
	ramp_init(&fire_left, 0.02, 0.0f, -UP_3508_FAST_CURRENT);
	ramp_init(&fire_right, 0.02, 0.0f, UP_3508_FAST_CURRENT);
	reset_motion_arrive_counter();
	reset_3508_up_hold();
	Dart_Data.dart_state = DART_STATE_UP;
	Dart_Data.state_timer = 0;
	motor1_move_start = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	motor2_move_start = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
}

/**
 * @brief  处理上抬动作
 *         控制GM3508电机将飞镖抬起到发射准备位置
  */
static void handle_up_movement(void)
{
	int32_t motor1_current = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	int32_t motor2_current = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
	
	int32_t motor1_moved = motor1_current - motor1_move_start;
	int32_t motor2_moved = motor2_current - motor2_move_start;
	int32_t motor1_moved_abs = abs_i32(motor1_moved);
	int32_t motor2_moved_abs = abs_i32(motor2_moved);
	int32_t avg_moved_abs = (motor1_moved_abs + motor2_moved_abs) / 2;
	int32_t up_travel_ref = get_up_travel_ref();
	uint8_t first_shot_up_learning = (Dart_Data.current_dart == 0 && !learned_up_valid);
	int32_t up_base_travel = first_shot_up_learning ?
		up_travel_ref * UP_3508_FIRST_TARGET_PERCENT / 100 :
		up_travel_ref;
	int32_t up_target_travel = up_base_travel * UP_3508_TARGET_PERCENT / 100;
	int32_t up_slowdown_travel = up_target_travel * UP_3508_SLOWDOWN_PERCENT / 100;
	int32_t up_stop_check_travel = up_target_travel * UP_3508_STOP_CHECK_PERCENT / 100;
	uint8_t up_slow_zone = (avg_moved_abs >= up_slowdown_travel);
	uint8_t encoder_reached = (avg_moved_abs >= up_target_travel);
	uint8_t upper_stop_reached = (avg_moved_abs >= up_stop_check_travel && both_3508_speed_low());
	uint8_t arrive_confirmed = (encoder_reached || upper_stop_reached);
	uint32_t up_timeout = first_shot_up_learning ? FIRST_UP_TIMEOUT_MS : UP_TIMEOUT_MS;
	
	if (!arrive_confirmed && Dart_Data.state_timer < up_timeout)
	{
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		ramp_calc(&fire_left, up_slow_zone ? -UP_3508_SLOW_RAMP_STEP : -UP_3508_RAMP_STEP);
		ramp_calc(&fire_right, up_slow_zone ? UP_3508_SLOW_RAMP_STEP : UP_3508_RAMP_STEP);
		if (up_slow_zone)
		{
			if (fire_left.out < -UP_3508_SLOW_CURRENT)
				fire_left.out = -UP_3508_SLOW_CURRENT;
			if (fire_right.out > UP_3508_SLOW_CURRENT)
				fire_right.out = UP_3508_SLOW_CURRENT;
		}
		CAN_CmdDart3508(fire_left.out, fire_right.out, 0, 0);
		Dart_Data.state_timer++;
	}
	else
	{
		reset_motion_arrive_counter();
		hold_3508_up();
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		enter_wait_gate_state();
	}
}

/**
  * @brief  处理发射动作
  *         控制舵机解锁并延时等待飞镖发射完成
  */
static void handle_launch(void)
{
	hold_3508_up();

	if (DART_RUN_MODE != 1)
	{
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		stop_and_wait_gate();
		return;
	}

#if DART_REFEREE_TEST_BYPASS
	if (!launch_servo_released &&
		!dart_system_enabled())
	{
		stop_and_wait_gate();
		return;
	}
#else
	if (!launch_servo_released &&
		!dart_referee_allow_launch())
	{
		stop_and_wait_gate();
		return;
	}
#endif

	if (Dart_Data.state_timer < LAUNCH_READY_DELAY_MS)
	{
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
	}
	else
	{
		if (Dart_Data.current_dart == 0 && learned_down_valid && !learned_up_valid)
		{
			learn_top_position();
		}
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		launch_servo_released = 1;
	}

	Dart_Data.state_timer++;

	if (Dart_Data.state_timer > (LAUNCH_READY_DELAY_MS + LAUNCH_RELEASE_DELAY_MS))
	{
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		Dart_Data.current_dart++;
		dart_match_fire_count++;
		gate_fire_count = (Dart_Data.current_dart == 1 ||
			Dart_Data.current_dart == 3) ? 1 : DART_GATE_MAX_FIRE_COUNT;
		dart_auto_run_start_tick = xTaskGetTickCount();
		
		if (Dart_Data.current_dart < DART_MAX_COUNT)
		{
			Dart_Data.dart_state = DART_STATE_DOWN_AFTER_LAUNCH;
			Dart_Data.state_timer = 0;
			motor1_move_start = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
			motor2_move_start = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
			ramp_init(&fire_left_down, 0.02, RELOAD_DOWN_3508_MAX_CURRENT, 0.0f);
			ramp_init(&fire_right_down, 0.02, 0.0f, -RELOAD_DOWN_3508_MAX_CURRENT);
			reset_motion_arrive_counter();
		}
		else
		{
			Dart_Data.dart_state = DART_STATE_IDLE;
			Dart_Data.shoot_count = ZERO_SHOOT;
			last_shoot_count = ZERO_SHOOT;
			dart_auto_run_latched = 0;
			dart_auto_run_start_tick = 0;
			reset_dart_window_control();
		}
	}
}

/**
 * @brief  处理发射后下行
 *         发射完成后，3508再次下压到底部，舵机保持松开
  */
static void handle_down_after_launch(void)
{
	int32_t motor1_current = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	int32_t motor2_current = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
	int32_t motor1_moved = motor1_current - motor1_move_start;
	int32_t motor2_moved = motor2_current - motor2_move_start;
	int32_t motor1_moved_abs = abs_i32(motor1_moved);
	int32_t motor2_moved_abs = abs_i32(motor2_moved);
	int32_t avg_moved_abs = (motor1_moved_abs + motor2_moved_abs) / 2;
	int32_t down_travel_ref = get_down_travel_ref();
	uint8_t motor1_at_bottom = reload_bottom_ref_valid &&
		(abs_i32(motor1_current - reload_bottom_pos_1) <= RELOAD_BOTTOM_TOLERANCE);
	uint8_t motor2_at_bottom = reload_bottom_ref_valid &&
		(abs_i32(motor2_current - reload_bottom_pos_2) <= RELOAD_BOTTOM_TOLERANCE);
	uint8_t encoder_reached = (motor1_at_bottom && motor2_at_bottom && both_3508_speed_low());
	uint8_t bottom_pressed = (avg_moved_abs >= down_travel_ref * RELOAD_DOWN_CONFIRM_PERCENT / 100 && both_3508_speed_low());
	uint8_t arrive_confirmed = motion_arrive_confirmed(encoder_reached || bottom_pressed);

	if (!arrive_confirmed && Dart_Data.state_timer < RELOAD_DOWN_TIMEOUT_MS)
	{
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		ramp_calc(&fire_left_down, DOWN_3508_RAMP_STEP);
		ramp_calc(&fire_right_down, -DOWN_3508_RAMP_STEP);
		CAN_CmdDart3508(fire_left_down.out, fire_right_down.out, 0, 0);
		Dart_Data.state_timer++;
	}
	else
	{
		reset_motion_arrive_counter();
		CAN_CmdDart3508(HOLD_CURRENT, -HOLD_CURRENT, 0, 0);
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		reload_returned_bottom = 0;
		reload_stepper_returned_origin = 0;
		Dart_Data.dart_state = DART_STATE_STEPPER_MOVE;
		Dart_Data.state_timer = 0;
	}
}

/**
 * @brief  处理步进电机送弹
 *         3508在底部保持，等待步进电机将下一发送到取镖位
  */
static void handle_stepper_move(void)
{
	uint32_t target_pos = 0;
	
	switch (Dart_Data.current_dart)
	{
		case 1:
			target_pos = STEPPER_RELOAD_POS_1;
			break;
		case 2:
			target_pos = STEPPER_RELOAD_POS_2;
			break;
		case 3:
			target_pos = STEPPER_RELOAD_POS_3;
			break;
		default:
			Dart_Data.dart_state = DART_STATE_DOWN;
			Dart_Data.state_timer = 0;
			return;
	}
	
	if (Dart_Data.state_timer == 0)
	{
		Emm_V5_Pos_Control(1, 0, 1000, 50, target_pos, 1, 0);
	}
	
	servo_set_angle(SERVO_RELEASE_ANGLE, 0);
	CAN_CmdDart3508(HOLD_CURRENT, -HOLD_CURRENT, 0, 0);
	Dart_Data.state_timer++;
	
	if (Dart_Data.state_timer > STEPPER_MOVE_WAIT_MS)
	{
		Dart_Data.dart_state = DART_STATE_RELEASE_UP;
		Dart_Data.state_timer = 0;
	}
}

/**
 * @brief  处理换弹取镖动作
 *         步进到位后，3508带对抗电流上升取镖；取完镖后步进先回原点，3508再下到底部
  */
static void handle_release_up(void)
{
	int32_t motor1_current = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
	int32_t motor2_current = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
	uint8_t motor1_at_bottom = reload_bottom_ref_valid &&
		(abs_i32(motor1_current - reload_bottom_pos_1) <= RELOAD_BOTTOM_TOLERANCE);
	uint8_t motor2_at_bottom = reload_bottom_ref_valid &&
		(abs_i32(motor2_current - reload_bottom_pos_2) <= RELOAD_BOTTOM_TOLERANCE);
	uint8_t bottom_stable = motion_arrive_confirmed(motor1_at_bottom && motor2_at_bottom && both_3508_speed_low());
	uint32_t pick_up_elapsed = 0;
	int16_t counter_current = 0;
	pick_debug_motor1_angle = motor1_current;
	pick_debug_motor2_angle = motor2_current;
	pick_debug_motor1_speed = Dart_Data.G3508_motor[0].chassis_motor_measure->speed_rpm;
	pick_debug_motor2_speed = Dart_Data.G3508_motor[1].chassis_motor_measure->speed_rpm;

	if (!reload_returned_bottom &&
		!reload_stepper_returned_origin &&
		Dart_Data.state_timer < PICK_DART_PRE_UP_WAIT_MS)
	{
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		CAN_CmdDart3508(HOLD_CURRENT, -HOLD_CURRENT, 0, 0);
		Dart_Data.state_timer++;
		return;
	}

	if (Dart_Data.state_timer >= PICK_DART_PRE_UP_WAIT_MS)
	{
		pick_up_elapsed = Dart_Data.state_timer - PICK_DART_PRE_UP_WAIT_MS;
	}
	counter_current = get_pick_counter_current(pick_up_elapsed);

	if (reload_returned_bottom)
	{
		if (Dart_Data.state_timer < PICK_DART_LOCK_DELAY_MS)
		{
			servo_set_angle(SERVO_LOCK_ANGLE, 0);
			CAN_CmdDart3508(PICK_DART_HOLD_CURRENT, -PICK_DART_HOLD_CURRENT, 0, 0);
			Dart_Data.state_timer++;
			return;
		}

		CAN_CmdDart3508(0, 0, 0, 0);
		ramp_init(&fire_left, 0.02, 0.0f, -UP_3508_FAST_CURRENT);
		ramp_init(&fire_right, 0.02, 0.0f, UP_3508_FAST_CURRENT);
		reset_3508_up_hold();
		motor1_move_start = Dart_Data.G3508_motor[0].chassis_motor_measure->total_angle;
		motor2_move_start = Dart_Data.G3508_motor[1].chassis_motor_measure->total_angle;
		reload_returned_bottom = 0;
		reload_stepper_returned_origin = 0;
		Dart_Data.dart_state = DART_STATE_UP;
		Dart_Data.state_timer = 0;
		return;
	}

	if (Dart_Data.state_timer < (PICK_DART_PRE_UP_WAIT_MS + PICK_DART_UP_DELAY_MS))
	{
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		CAN_CmdDart3508(counter_current, -counter_current, 0, 0);
	}
	else if (!reload_stepper_returned_origin)
	{
		if (Dart_Data.state_timer == (PICK_DART_PRE_UP_WAIT_MS + PICK_DART_UP_DELAY_MS))
		{
			Emm_V5_Pos_Control(1, 1, 1000, 50, STEPPER_ORIGIN_POS, 1, 0);
		}

		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		CAN_CmdDart3508(counter_current, -counter_current, 0, 0);

		if (Dart_Data.state_timer >= (PICK_DART_PRE_UP_WAIT_MS + PICK_DART_UP_DELAY_MS + STEPPER_RETURN_WAIT_MS))
		{
			int16_t pick_down_start = PICK_DART_DOWN_START_CURRENT;
			int16_t pick_down_target = PICK_DART_DOWN_TARGET_CURRENT;

			if (Dart_Data.current_dart >= 2)
			{
				pick_down_start = PICK_DART_DOWN_LATE_START_CURRENT;
				pick_down_target = PICK_DART_DOWN_LATE_TARGET_CURRENT;
			}

			reload_stepper_returned_origin = 1;
			Dart_Data.state_timer = 0;
			reset_motion_arrive_counter();
			ramp_init(&pick_down_left, 0.02, pick_down_target, pick_down_start);
			ramp_init(&pick_down_right, 0.02, -pick_down_start, -pick_down_target);
			return;
		}
		}
	else if (!bottom_stable && Dart_Data.state_timer < PICK_DART_RETURN_DOWN_TIMEOUT_MS)
	{
		servo_set_angle(SERVO_RELEASE_ANGLE, 0);
		ramp_calc(&pick_down_left, PICK_DART_DOWN_RAMP_STEP);
		ramp_calc(&pick_down_right, -PICK_DART_DOWN_RAMP_STEP);
		CAN_CmdDart3508((int16_t)pick_down_left.out, (int16_t)pick_down_right.out, 0, 0);
	}
	
	else
	{
		reset_motion_arrive_counter();
		servo_set_angle(SERVO_LOCK_ANGLE, 0);
		CAN_CmdDart3508(PICK_DART_HOLD_CURRENT, -PICK_DART_HOLD_CURRENT, 0, 0);
		reload_returned_bottom = 1;
		Dart_Data.state_timer = 0;
		return;
	}

	Dart_Data.state_timer++;
}

/**
  * @brief  飞镖PID重新初始化
  *         重新配置GM3508电机的速度PID参数
  */
void shootPIDInit(void)
{
	PID_init(&Dart_Data.motor3508_speed_pid[0], PID_POSITION, GM3508_Current_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
	PID_init(&Dart_Data.motor3508_speed_pid[1], PID_POSITION, GM3508_Current_PID, GM3508_MOTOR_SPEED_PID_MAX_OUT, GM3508_MOTOR_SPEED_PID_MAX_IOUT);
}

/**
  * @brief  飞镖发射任务
  *         FreeRTOS任务，周期性地读取裁判数据、处理遥控器输入和执行发射状态机
  * @param  pvParameters: 任务参数
  */
void shoot_task(void const *pvParameters)
{
	vTaskDelay(100);
	shoot_init();
	
	while(1)
	{
		read_referee_data();

//				servo_set_angle(72, 0);

		M2006_Aux_Control();
		shoot_rc();
		shoot();
		vTaskDelay(1);
	}
}
