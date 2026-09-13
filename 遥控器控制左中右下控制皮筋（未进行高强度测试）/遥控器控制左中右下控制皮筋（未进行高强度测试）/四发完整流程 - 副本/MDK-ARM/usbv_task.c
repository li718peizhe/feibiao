#include "usbv_task.h"
#include <string.h>
#include "FreeRTOS.h"
#include "cmsis_os.h"

#include "usb_device.h"
#include "usbd_cdc_if.h"

// 视觉控制头文件
#include "visionv_task.h"
#include "remote_control.h"
#include "referee.h"
#include "shoot_task.h"

// 外部变量声明
extern USBD_HandleTypeDef hUsbDeviceFS;

// 全局变量
static uint8_t usb_rx_buffer[2048];
static uint32_t usb_rx_length = 0;
static uint32_t vision_feedback_timestamp = 0;
VisionPacket vision_packet;
ControlFeedbackPacket feedback_packet;

#define USBV_REFEREE_OFFLINE_TIMEOUT 500

static uint8_t usbv_rc_control_enabled(void)
{
    const RC_ctrl_t *rc = get_remote_control_point();

#if DART_REFEREE_TEST_BYPASS
    return (rc != NULL &&
            switch_is_down(rc->rc.s[1]) &&
            switch_is_mid(rc->rc.s[0]));
#else
    uint32_t now = xTaskGetTickCount();

    return (rc != NULL &&
            switch_is_down(rc->rc.s[1]) &&
            switch_is_mid(rc->rc.s[0]) &&
            referee_last_update_time != 0 &&
            (now - referee_last_update_time) <= USBV_REFEREE_OFFLINE_TIMEOUT &&
            game_state.game_progress == PROGRESS_BATTLE);
#endif
}

/*
*初始值 0xFFFF，对每个字节 LSB 优先处理
*返回值即为校验和，接收方对包含校验和的前N-2字节重新计算，结果应为 0
*
*/
uint16_t crc16_modbus(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

int parse_vision_packet(const uint8_t* buf, size_t len, VisionPacket* out) {
    const size_t PKT_LEN = 16;
	if (len < PKT_LEN) return -1;    //长度不足
    if (buf[0] != 0xA5) return -2;        //帧头.错误

    memcpy(out, buf, PKT_LEN);
    uint16_t crc = crc16_modbus(buf, 14);
    if (crc != out->checksum) return -3;    //CRC.校验.失败
    return 0;
}

void build_feedback_packet(ControlFeedbackPacket* pkt, float yaw_current, uint8_t accept_yaw, uint32_t timestamp_ms) {
    pkt->header = 0x5A;
    pkt->yaw_current = yaw_current;
    pkt->accept_yaw = accept_yaw ? 1 : 0;
    pkt->timestamp_ms = timestamp_ms;
    pkt->checksum = 0;
    pkt->checksum = crc16_modbus((const uint8_t*)pkt, 10);
}

// USB接收回调函数
int8_t CDC_Receive_Vision_FS(uint8_t* Buf, uint32_t *Len)
{
    // 保存接收到的数据
    memcpy(usb_rx_buffer, Buf, *Len);
    usb_rx_length = *Len;
    
    // 重新设置接收缓冲区
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    
    return (USBD_OK);
}
float target_angle;
void usbv_task(void const *argument)
{
    // 初始化USB设备
    MX_USB_DEVICE_Init();
    
    // 重新设置接收回调
    USBD_Interface_fops_FS.Receive = CDC_Receive_Vision_FS;
    
    while(1)
    {
        // 处理接收到的数据
        if (usb_rx_length > 0) {
            int result = parse_vision_packet(usb_rx_buffer, usb_rx_length, &vision_packet);
            if (result == 0) {
                uint8_t target_found = (vision_packet.state != 0);
                uint8_t accept_yaw = (usbv_rc_control_enabled() || target_found);

                // 先取 yaw 的绝对值（只把它当做偏角大小）
                float yaw_abs = vision_packet.yaw > 0 ? vision_packet.yaw : -vision_packet.yaw;
                target_angle = yaw_abs * 57.2957795f;
                
                switch (vision_packet.direction) {
                    case 0:
                        target_angle = 0.0f; // 0=正对，不动
                        break;
                    case 1:
                        target_angle = target_angle; // 1=向右动（左正右负，故取负）
                        break;
                    case 2:
                        target_angle = -target_angle;  // 2=向左动（保持正值）
                        break;
                    default:
                        target_angle = 0.0f;
                        break;
                }
                
                float target_distance = 0.0f;
                
                vision_update_target(target_angle, target_distance, target_found);
                
                vision_feedback_timestamp++;
                build_feedback_packet(&feedback_packet, 0.0f, accept_yaw, vision_feedback_timestamp);
                
                CDC_Transmit_FS((uint8_t*)&feedback_packet, sizeof(ControlFeedbackPacket));
            }
            
            usb_rx_length = 0;
        }
        
        // 延时1ms
        vTaskDelay(1);
    }
}
