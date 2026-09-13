#ifndef PID_H
#define PID_H
#include "struct_typedef.h"
#include "user_lib.h"
#include "arm_math.h"
#define LIMIT(x,min,max) (((x)<=(min))?(min):(((x)>=(max))?(max):(x)))
#define ABS(x) (((x)>0)?(x):(-(x)))

enum PID_MODE
{
    PID_POSITION = 0,
    PID_DELTA
};

typedef struct
{
    uint8_t mode;

    fp32 Kp;
    fp32 Ki;
    fp32 Kd;

    fp32 max_out;  
    fp32 max_iout; 

    fp32 set;
    fp32 fdb;

    fp32 out;
    fp32 Pout;
    fp32 Iout;
    fp32 Dout;
    fp32 Dbuf[3];  
    fp32 error[3]; 

} pid_type_def;

typedef struct
{
    fp32 kp;
    fp32 ki;
    fp32 kd;

    fp32 set;
    fp32 get;
    fp32 err;

    fp32 max_out;
    fp32 max_iout;
    
      fp32 error_set;
      fp32 last_set;
    
        fp32 feedforward;     // Ç°À¡ÔöÒæ

    fp32 Pout;
    fp32 Iout;
    fp32 Dout;

    fp32 out;
} gimbal_PID_t;


extern void PID_init(pid_type_def *pid, uint8_t mode, const fp32 PID[3], fp32 max_out, fp32 max_iout);
extern fp32 PID_calc(pid_type_def *pid, fp32 ref, fp32 set);
extern void gimbal_PID_init(gimbal_PID_t *pid, fp32 maxout, fp32 max_iout, fp32 kp, fp32 ki, fp32 kd , fp32 feedforward);
extern fp32 gimbal_PID_calc(gimbal_PID_t *pid, fp32 get, fp32 set, fp32 error_delta);
extern void PID_clear(pid_type_def *pid);
extern void PID_init(pid_type_def *pid, uint8_t mode, const fp32 PID[3], fp32 max_out, fp32 max_iout);
extern fp32 PID_Calc_Ecd(pid_type_def *pid, fp32 ref, fp32 set, uint16_t ecd_range);
extern fp32 PID_Calc_Angle(pid_type_def *pid, fp32 ref, fp32 set);
#endif
