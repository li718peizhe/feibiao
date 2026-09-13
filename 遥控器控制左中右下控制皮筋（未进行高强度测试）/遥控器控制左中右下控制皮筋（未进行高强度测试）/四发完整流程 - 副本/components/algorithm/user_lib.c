#include "user_lib.h"
#include "arm_math.h"

fp32 invSqrt(fp32 num)
{
    fp32 halfnum = 0.5f * num;
    fp32 y = num;
    long i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(fp32 *)&i;
    y = y * (1.5f - (halfnum * y * y));
    return y;
}

void ramp_init(ramp_function_source_t *ramp_source_type, fp32 frame_period, fp32 max, fp32 min)
{
    ramp_source_type->frame_period = frame_period;
    ramp_source_type->max_value = max;
    ramp_source_type->min_value = min;
    ramp_source_type->input = 0.0f;
    ramp_source_type->out = 0.0f;
}

void ramp_calc(ramp_function_source_t *ramp_source_type, fp32 input)
{
    ramp_source_type->input = input;
    ramp_source_type->out += ramp_source_type->input * ramp_source_type->frame_period;
    if (ramp_source_type->out > ramp_source_type->max_value)
    {
        ramp_source_type->out = ramp_source_type->max_value;
    }
    else if (ramp_source_type->out < ramp_source_type->min_value)
    {
        ramp_source_type->out = ramp_source_type->min_value;
    }
}
void ramp_calc1(ramp_function_source_t *ramp_source_type, fp32 input)
{
	 // 将目标输入值赋值给结构体的 input
    ramp_source_type->input = input;

    // 判断 max_value 是否大于零，决定增速还是减速
    if (ramp_source_type->max_value > 0)
    {
        // 如果目标值大于当前值，逐步增加 output 值
        ramp_source_type->out += ramp_source_type->input * ramp_source_type->frame_period;

        // 如果 output 超过了最大值 max_value，限制其为 max_value
        if (ramp_source_type->out > ramp_source_type->max_value)
        {
            ramp_source_type->out = ramp_source_type->max_value;
        }
        // 如果 output 小于最小值 min_value，限制其为 min_value
        else if (ramp_source_type->out < ramp_source_type->min_value)
        {
            ramp_source_type->out = ramp_source_type->min_value;
        }
    }
    // 如果 max_value 小于零，执行减速操作
    else if (ramp_source_type->max_value < 0)
    {
        // 如果目标值小于当前值，逐步减少 output 值
        ramp_source_type->out += ramp_source_type->input * ramp_source_type->frame_period;

        // 如果 output 小于最小值 min_value，限制其为 min_value
        if (ramp_source_type->out < ramp_source_type->max_value)
        {
            ramp_source_type->out = ramp_source_type->max_value;
        }
        // 如果 output 超过了最大值 max_value，限制其为 max_value
        else if (ramp_source_type->out > ramp_source_type->min_value)
        {
            ramp_source_type->out = ramp_source_type->min_value;
        }
    }
    // 如果 max_value == 0，直接将 output 设置为 0
    else if (ramp_source_type->max_value == 0)
    {
        ramp_source_type->out = 0;
    }
}

void ramp_calc_min(ramp_function_source_t *ramp_source_type, fp32 input)
{
    ramp_source_type->input = input;
    ramp_source_type->out -= ramp_source_type->input * ramp_source_type->frame_period;
    if (ramp_source_type->out > ramp_source_type->max_value)
    {
        ramp_source_type->out = ramp_source_type->max_value;
    }
    else if (ramp_source_type->out < ramp_source_type->min_value)
    {
        ramp_source_type->out = ramp_source_type->min_value;
    }
}

void first_order_filter_init(first_order_filter_type_t *first_order_filter_type, fp32 frame_period, const fp32 num[1])
{
    first_order_filter_type->frame_period = frame_period;
    first_order_filter_type->num[0] = num[0];
    first_order_filter_type->input = 0.0f;
    first_order_filter_type->out = 0.0f;
}
void first_order_filter_cali(first_order_filter_type_t *first_order_filter_type, fp32 input)
{
    first_order_filter_type->input = input;
    first_order_filter_type->out =
    first_order_filter_type->num[0] / (first_order_filter_type->num[0] + first_order_filter_type->frame_period) * first_order_filter_type->out + first_order_filter_type->frame_period / (first_order_filter_type->num[0] + first_order_filter_type->frame_period) * first_order_filter_type->input;
}

void abs_limit(fp32 *num, fp32 Limit)
{
    if (*num > Limit)
    {
        *num = Limit;
    }
    else if (*num < -Limit)
    {
        *num = -Limit;
    }
}

fp32 sign(fp32 value)
{
    if (value >= 0.0f)
    {
        return 1.0f;
    }
    else
    {
        return -1.0f;
    }
}

fp32 fp32_deadline(fp32 Value, fp32 minValue, fp32 maxValue)
{
    if (Value < maxValue && Value > minValue)
    {
        Value = 0.0f;
    }
    return Value;
}

int16_t int16_deadline(int16_t Value, int16_t minValue, int16_t maxValue)
{
    if (Value < maxValue && Value > minValue)
    {
        Value = 0;
    }
    return Value;
}

fp32 fp32_constrain(fp32 Value, fp32 minValue, fp32 maxValue)
{
    if (Value < minValue)
        return minValue;
    else if (Value > maxValue)
        return maxValue;
    else
        return Value;
}

void VAL_LIMIT(float *Value, float minValue, float maxValue)
{
    if (*Value < minValue)
        *Value = minValue;
    else if (*Value > maxValue)
        *Value = maxValue;
    else
        *Value = *Value;
}

int16_t int16_constrain(int16_t Value, int16_t minValue, int16_t maxValue)
{
    if (Value < minValue)
        return minValue;
    else if (Value > maxValue)
        return maxValue;
    else
        return Value;
}

fp32 loop_fp32_constrain(fp32 Input, fp32 minValue, fp32 maxValue)
{
    if (maxValue < minValue)
    {
        return Input;
    }

    if (Input > maxValue)
    {
        fp32 len = maxValue - minValue;
        while (Input > maxValue)
        {
            Input -= len;
        }
    }
    else if (Input < minValue)
    {
        fp32 len = maxValue - minValue;
        while (Input < minValue)
        {
            Input += len;
        }
    }
    return Input;
}

fp32 theta_format(fp32 Ang)
{
    return loop_fp32_constrain(Ang, -180.0f, 180.0f);
}

void InitQueue(Queue *Q)
{
    Q->base = (ElemType *)malloc(sizeof(ElemType) * MAXSIZE);
    // assert(Q->base != NULL);
    Q->front = Q->rear = 0;
}

// 入队操作

void EnQueue(Queue *Q, ElemType x)

{
    if (((Q->rear + 1) % MAXSIZE) == Q->front)

        return;
    Q->base[Q->rear] = x;

    // 更改尾指针的指向
    Q->rear = (Q->rear + 1) % MAXSIZE;
}

// 出队操作
void DeQueue(Queue *Q)

{
    if (Q->front == Q->rear)
        return;
    Q->front = (Q->front + 1) % MAXSIZE;
}

// 获取队头元素

void GetHdad(Queue *Q, ElemType *v)
{
    if (Q->front == Q->rear)

        return;

    // 如果队列不为空，获取队头元素
    *v = Q->base[Q->front];
}

// 获取队列长度（元素个数）
int Length(Queue *Q)
{
    int len = Q->rear - Q->front;
    len = (len > 0) ? len : MAXSIZE + len;

    return len;
}

// 清空队列

void ClearQueue(Queue *Q)

{
    Q->front = Q->rear = 0;
}

void DestroyQueue(Queue *Q)

{
    free(Q->base);
    Q->base = NULL;
}


fp32 motor_ecd_to_angle_change(uint16_t ecd, uint16_t offset_ecd)
{
    int32_t relative_ecd = ecd - offset_ecd;
    if (relative_ecd > HALF_ECD_RANGE)
    {
        relative_ecd -= ECD_RANGE;
    }
    else if (relative_ecd < -HALF_ECD_RANGE)
    {
        relative_ecd += ECD_RANGE;
    }

    return relative_ecd * MOTOR_ECD_TO_RAD;
}





