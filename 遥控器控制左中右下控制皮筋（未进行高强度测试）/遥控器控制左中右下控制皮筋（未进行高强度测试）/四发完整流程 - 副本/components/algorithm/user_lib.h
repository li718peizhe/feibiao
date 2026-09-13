#ifndef USER_LIB_H
#define USER_LIB_H
#include "struct_typedef.h"
#include <assert.h>
#include "stdlib.h"

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))


#define ElemType float
#define MAXSIZE 10

#define HALF_ECD_RANGE   4096
#define ECD_RANGE       8191
#define MOTOR_ECD_TO_RAD 0.000766990394f //      2*  PI  /8192

typedef struct
{
ElemType *base; 
int front; 
int rear; 
}Queue;


#define QUEUE_SIZE 50
struct queue {
	float  data[QUEUE_SIZE];
	int front;
	int  tail;
	int empty;
};


typedef struct
{
    uint16_t nowLength;
    uint16_t queueLength;
    float queueTotal;

    float queue[100];
    uint8_t full_flag;
} QueueObj;

void Queueinit(struct queue *);
void enqueue(struct queue *, float);
int dequeue(struct queue *);



typedef __packed struct
{
    fp32 input;        
    fp32 out;          
    fp32 min_value;    
    fp32 max_value;    
    fp32 frame_period; 
} ramp_function_source_t;

/**
  * @brief          斜波函数初始化
  * @param[in]      斜波函数结构体
  * @param[in]      间隔的时间，单位 s
  * @param[in]      最大值
  * @param[in]      最小值
  * @retval         返回空
  */
typedef __packed struct
{
    fp32 input;       
    fp32 out;         
    fp32 num[1];    
    fp32 frame_period;
} first_order_filter_type_t;

extern fp32 invSqrt(fp32 num);


void ramp_init(ramp_function_source_t *ramp_source_type, fp32 frame_period, fp32 max, fp32 min);


void ramp_calc(ramp_function_source_t *ramp_source_type, fp32 input);
void ramp_calc_min(ramp_function_source_t *ramp_source_type, fp32 input);
void ramp_calc1(ramp_function_source_t *ramp_source_type, fp32 input);

extern void first_order_filter_init(first_order_filter_type_t *first_order_filter_type, fp32 frame_period, const fp32 num[1]);

extern void first_order_filter_cali(first_order_filter_type_t *first_order_filter_type, fp32 input);

extern void abs_limit(fp32 *num, fp32 Limit);

extern fp32 sign(fp32 value);

extern fp32 fp32_deadline(fp32 Value, fp32 minValue, fp32 maxValue);

extern int16_t int16_deadline(int16_t Value, int16_t minValue, int16_t maxValue);

extern fp32 fp32_constrain(fp32 Value, fp32 minValue, fp32 maxValue);

extern int16_t int16_constrain(int16_t Value, int16_t minValue, int16_t maxValue);

extern fp32 loop_fp32_constrain(fp32 Input, fp32 minValue, fp32 maxValue);

extern fp32 theta_format(fp32 Ang);

extern void VAL_LIMIT(float *Value, float minValue, float maxValue);

void InitQueue(Queue *Q);
void EnQueue(Queue *Q, ElemType x);
void ShowQueue(Queue *Q);
void DeQueue(Queue *Q);
void GetHdad(Queue *Q, ElemType *v);
int Length(Queue *Q);
void ClearQueue(Queue *Q);
void DestroyQueue(Queue *Q);

extern fp32 motor_ecd_to_angle_change(uint16_t ecd, uint16_t offset_ecd);


#define rad_format(Ang) loop_fp32_constrain((Ang), -PI, PI)

#endif
