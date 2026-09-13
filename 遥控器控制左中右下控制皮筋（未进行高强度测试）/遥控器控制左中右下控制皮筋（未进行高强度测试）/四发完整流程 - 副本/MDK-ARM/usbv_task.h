#ifndef USBV_TASK_H
#define USBV_TASK_H

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  header;        // 0xA5
    uint8_t  state;         // 0=未发现目标, 1=发现目标
    float    yaw;           // yaw_cmd，单位rad
    uint8_t  direction;     // 0=目标在正对, 1=目标在左侧，2=目标再右侧
    uint8_t  distance_mode; // 0=近偏差(修正2°), 1=远偏差(修正4°)
    uint32_t timestamp_ms;  // 图像捕获时间戳(ms)
    uint16_t t_offset;      // 时间补偿(ms)
    uint16_t checksum;      // CRC16-Modbus（前14字节）
} VisionPacket;

#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    uint8_t  header;       // 0x5A
    float    yaw_current;  // 当前炮筒yaw，rad（上电零位，左正右负）
    uint8_t  accept_yaw;   // 0=当前不采纳视觉yaw, 1=采纳视觉yaw
    uint32_t timestamp_ms; // 电控时间戳(ms)
    uint16_t checksum;     // CRC16-Modbus（前10字节）
} ControlFeedbackPacket;

#pragma pack(pop)

extern VisionPacket vision_packet;
extern ControlFeedbackPacket feedback_packet;

uint16_t crc16_modbus(const uint8_t* data, size_t len);
int parse_vision_packet(const uint8_t* buf, size_t len, VisionPacket* out);
void build_feedback_packet(ControlFeedbackPacket* pkt, float yaw_current, uint8_t accept_yaw, uint32_t timestamp_ms);

void usbv_task(void const *pvParameters);

#endif
