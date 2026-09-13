#ifdef Gimbal
#include "gimbal_calibrate.h"


void set_cali_gimbal_hook(const uint16_t yaw_offset, const uint16_t pitch_offset, const fp32 max_yaw, const fp32 min_yaw, const fp32 max_pitch, const fp32 min_pitch)
{
    gimbal_data.gimbal_yaw_motor.offset_ecd = yaw_offset;
    gimbal_data.gimbal_yaw_motor.max_relative_angle = max_yaw;
     gimbal_data.gimbal_yaw_motor.min_relative_angle = min_yaw;

    gimbal_data.gimbal_pitch_motor.offset_ecd = pitch_offset;
    gimbal_data.gimbal_pitch_motor.max_relative_angle = max_pitch;
    gimbal_data.gimbal_pitch_motor.min_relative_angle = min_pitch;
}

bool_t cmd_cali_gimbal_hook(uint16_t *yaw_offset, uint16_t *pitch_offset, fp32 *max_yaw, fp32 *min_yaw, fp32 *max_pitch, fp32 *min_pitch)
{
    if (gimbal_data.gimbal_calibrate.step == 0)
    {
        gimbal_data.gimbal_calibrate.step = GIMBAL_CALI_START_STEP;
       gimbal_data.gimbal_calibrate.max_pitch =gimbal_data.gimbal_pitch_motor.absolute_angle;
        gimbal_data.gimbal_calibrate.max_pitch_ecd = gimbal_data.gimbal_pitch_motor.gimbal_motor_measure->ecd;
       gimbal_data.gimbal_calibrate.max_yaw = gimbal_data.gimbal_yaw_motor.absolute_angle;
        gimbal_data.gimbal_calibrate.max_yaw_ecd = gimbal_data.gimbal_yaw_motor.gimbal_motor_measure->ecd;
       gimbal_data.gimbal_calibrate.min_pitch = gimbal_data.gimbal_pitch_motor.absolute_angle;
        gimbal_data.gimbal_calibrate.min_pitch_ecd = gimbal_data.gimbal_pitch_motor.gimbal_motor_measure->ecd;
        gimbal_data.gimbal_calibrate.min_yaw = gimbal_data.gimbal_yaw_motor.absolute_angle;
       gimbal_data.gimbal_calibrate.min_yaw_ecd = gimbal_data.gimbal_yaw_motor.gimbal_motor_measure->ecd;
        return 0;
    }
    else if (gimbal_data.gimbal_calibrate.step == GIMBAL_CALI_END_STEP)
    {
        calc_gimbal_cali(&gimbal_data.gimbal_calibrate, yaw_offset, pitch_offset, max_yaw, min_yaw, max_pitch, min_pitch);
        (*max_yaw) -= GIMBAL_CALI_REDUNDANT_ANGLE;
        (*min_yaw) += GIMBAL_CALI_REDUNDANT_ANGLE;
        (*max_pitch) -= GIMBAL_CALI_REDUNDANT_ANGLE;
        (*min_pitch) += GIMBAL_CALI_REDUNDANT_ANGLE;
        gimbal_data.gimbal_yaw_motor.offset_ecd = *yaw_offset;
       gimbal_data.gimbal_yaw_motor.max_relative_angle = *max_yaw;
        gimbal_data.gimbal_yaw_motor.min_relative_angle = *min_yaw;
        gimbal_data.gimbal_yaw_motor.offset_ecd = *pitch_offset;
        gimbal_data.gimbal_yaw_motor.max_relative_angle = *max_pitch;
        gimbal_data.gimbal_yaw_motor.min_relative_angle = *min_pitch;
        gimbal_data.gimbal_calibrate.step = 0;
        return 1;
    }
    else
    {
        return 0;
    }
}
static void calc_gimbal_cali(const gimbal_step_cali_t *gimbal_cali, uint16_t *yaw_offset, uint16_t *pitch_offset, fp32 *max_yaw, fp32 *min_yaw, fp32 *max_pitch, fp32 *min_pitch)
{
    if (gimbal_cali == NULL || yaw_offset == NULL || pitch_offset == NULL || max_yaw == NULL || min_yaw == NULL || max_pitch == NULL || min_pitch == NULL)
    {
        return;
    }

    int16_t temp_max_ecd = 0, temp_min_ecd = 0, temp_ecd = 0;
	
#if YAW_TURN
    temp_ecd = gimbal_cali->min_yaw_ecd - gimbal_cali->max_yaw_ecd;

    if (temp_ecd < 0)
    {
        temp_ecd += ecd_range;
    }
    temp_ecd = gimbal_cali->max_yaw_ecd + (temp_ecd / 2);

    ecd_format(temp_ecd);
    *yaw_offset = temp_ecd;
    *max_yaw = -motor_ecd_to_angle_change(gimbal_cali->max_yaw_ecd, *yaw_offset);
    *min_yaw = -motor_ecd_to_angle_change(gimbal_cali->min_yaw_ecd, *yaw_offset);

#else

    temp_ecd = gimbal_cali->max_yaw_ecd - gimbal_cali->min_yaw_ecd;

    if (temp_ecd < 0)
    {
        temp_ecd += ECD_RANGE;
    }
    temp_ecd = gimbal_cali->max_yaw_ecd - (temp_ecd / 2);

    ecd_format(temp_ecd);
    *yaw_offset = temp_ecd;
    *max_yaw = motor_ecd_to_angle_change(gimbal_cali->max_yaw_ecd, *yaw_offset);
    *min_yaw = motor_ecd_to_angle_change(gimbal_cali->min_yaw_ecd, *yaw_offset);

#endif

#if PITCH_TURN

    temp_ecd = (int16_t)(gimbal_cali->max_pitch / MOTOR_ECD_TO_RAD);
    temp_max_ecd = gimbal_cali->max_pitch_ecd + temp_ecd;
    temp_ecd = (int16_t)(gimbal_cali->min_pitch / MOTOR_ECD_TO_RAD);
    temp_min_ecd = gimbal_cali->min_pitch_ecd + temp_ecd;

    ecd_format(temp_max_ecd);
    ecd_format(temp_min_ecd);

    temp_ecd = temp_max_ecd - temp_min_ecd;

    if (temp_ecd > HALF_ECD_RANGE)
    {
        temp_ecd -= ECD_RANGE;
    }
    else if (temp_ecd < -HALF_ECD_RANGE)
    {
        temp_ecd += ECD_RANGE;
    }

    if (temp_max_ecd > temp_min_ecd)
    {
        temp_min_ecd += ECD_RANGE;
    }

    temp_ecd = temp_max_ecd - temp_ecd / 2;

    ecd_format(temp_ecd);

    *pitch_offset = temp_ecd;

    *max_pitch = -motor_ecd_to_angle_change(gimbal_cali->max_pitch_ecd, *pitch_offset);
    *min_pitch = -motor_ecd_to_angle_change(gimbal_cali->min_pitch_ecd, *pitch_offset);

#else
    temp_ecd = (int16_t)(gimbal_cali->max_pitch / MOTOR_ECD_TO_RAD);
    temp_max_ecd = gimbal_cali->max_pitch_ecd - temp_ecd;
    temp_ecd = (int16_t)(gimbal_cali->min_pitch / MOTOR_ECD_TO_RAD);
    temp_min_ecd = gimbal_cali->min_pitch_ecd - temp_ecd;

    ecd_format(temp_max_ecd);
    ecd_format(temp_min_ecd);

    temp_ecd = temp_max_ecd - temp_min_ecd;

    if (temp_ecd > HALF_ECD_RANGE)
    {
        temp_ecd -= ECD_RANGE;
    }
    else if (temp_ecd < -HALF_ECD_RANGE)
    {
        temp_ecd += ECD_RANGE;
    }

    temp_ecd = temp_max_ecd - temp_ecd / 2;

    ecd_format(temp_ecd);

    *pitch_offset = temp_ecd;

    *max_pitch = motor_ecd_to_angle_change(gimbal_cali->max_pitch_ecd, *pitch_offset);
    *min_pitch = motor_ecd_to_angle_change(gimbal_cali->min_pitch_ecd, *pitch_offset);
#endif
	}

#endif
	