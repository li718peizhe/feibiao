# 破阵天矛系统 - 电控通信协议

## 📋 文档信息

- **版本**: v1.0
- **协议名称**: RM Missile Protocol
- **创建日期**: 2026-10-31
- **适用系统**: RoboMaster 2027 迫击炮/飞镖视觉系统

---

## 🔌 串口配置

### 硬件参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 设备名 | `/dev/ttyACM0` | USB串口设备 |
| 波特率 | `115200` | 标准串口速率 |
| 数据位 | `8` | 固定 |
| 停止位 | `1` | 固定 |
| 奇偶校验 | `None` | 无校验 |
| 流控制 | `None` | 无流控 |

### 配置文件路径

```yaml
# config/system_config.yaml
serial:
  enable: true                      # 是否启用串口
  device_name: "/dev/ttyACM0"       # 串口设备
  baud_rate: 115200                 # 波特率
  flow_control: "none"              # 流控制
  parity: "none"                    # 奇偶校验
  stop_bits: 1                      # 停止位
  debug: false                      # 是否打印调试信息
  format: "binary"                  # 发送格式: binary|json
  send_mode: "detected"             # 发送模式: detected|always
  protocol: "rm_missile"            # 协议类型
```

---

## 📤 数据发送协议（视觉 → 电控）

### 数据包结构

**总长度**: 16 字节

| 字节偏移 | 字段名 | 类型 | 长度 | 说明 |
|---------|--------|------|------|------|
| 0 | `header` | `uint8_t` | 1 | 帧头，固定值 `0xA5` |
| 1 | `state` | `uint8_t` | 1 | 目标状态 (0=无目标, 1=有目标) |
| 2-5 | `yaw` | `float` | 4 | yaw轴增量角度（弧度，小端序） |
| 6 | `direction` | `uint8_t` | 1 | 目标方向 (0=正对, 1=左侧, 2=右侧) |
| 7 | `distance_mode` | `uint8_t` | 1 | 距离偏差 (0=无偏差, 1=近偏差, 2=远偏差) |
| 8-11 | `timestamp_ms` | `uint32_t` | 4 | 图像捕获时间戳（毫秒，小端序） |
| 12-13 | `t_offset` | `uint16_t` | 2 | 时间补偿（毫秒，小端序） |
| 14-15 | `checksum` | `uint16_t` | 2 | CRC16-Modbus校验（前14字节） |

### 字段详细说明

#### 1. header（帧头）
- **值**: `0xA5`
- **说明**: 固定帧头，用于数据包同步识别

#### 2. state（目标状态）
- **值**: 
  - `0` = 无目标检测
  - `1` = 检测到目标
- **说明**: 指示当前是否检测到绿色引导灯

#### 3. yaw（yaw轴增量角度）
- **类型**: 32位浮点数（IEEE 754，小端序）
- **单位**: 弧度（rad）
- **范围**: `[-π, π]`
- **说明**: 
  - 炮筒需要旋转的yaw角度
  - **正值** = 向左转（逆时针）
  - **负值** = 向右转（顺时针）
  - **0** = 已对准目标

#### 4. direction（目标方向）
- **值**:
  - `0` = 正对（目标在正前方，±2°以内）
  - `1` = 左侧（目标在左侧）
  - `2` = 右侧（目标在右侧）
- **说明**: 粗略指示目标相对于炮筒的方位

#### 5. distance_mode（距离偏差模式）
- **值**:
  - `0` = 无偏差（默认）
  - `1` = 近偏差（目标比预期近）
  - `2` = 远偏差（目标比预期远）
- **说明**: 当前版本使用固定距离（16米），此字段固定为 `0`

#### 6. timestamp_ms（时间戳）
- **类型**: 32位无符号整数（小端序）
- **单位**: 毫秒（ms）
- **说明**: 图像捕获时的系统时间戳，用于延迟补偿

#### 7. t_offset（时间补偿）
- **类型**: 16位无符号整数（小端序）
- **单位**: 毫秒（ms）
- **说明**: 图像处理耗时，用于预测补偿

#### 8. checksum（校验和）
- **类型**: 16位无符号整数（小端序）
- **算法**: CRC16-Modbus
- **计算范围**: 数据包的前14字节（不包含checksum字段本身）
- **说明**: 用于验证数据包完整性

---

## 📥 数据接收协议（电控 → 视觉）

### 当前实现状态

**暂未实现电控下行数据接收**

未来可能接收的数据：
- 炮筒当前yaw角度（用于闭环控制）
- 系统状态反馈
- 命令确认

---

## 🔄 发送模式

### detected 模式（推荐）
- **说明**: 仅在检测到目标时发送数据
- **优点**: 减少无效通信，降低电控负载
- **适用场景**: 正常比赛模式

### always 模式
- **说明**: 每帧都发送数据（无目标时 `state=0`）
- **优点**: 电控可实时监控视觉系统状态
- **适用场景**: 调试模式

---

## 📊 数据示例

### 示例1：检测到目标（向左2.5°）

```
十六进制数据（16字节）：
A5 01 6D E0 07 3F 01 00 00 12 34 56 00 64 XX XX
     ↑  ↑---------↑  ↑  ↑  ↑--------↑  ↑--↑  ↑--↑
     |      |       |  |  |     |       |   |
  state   yaw   dir dist  timestamp  t_off checksum
```

**字段解析**：
- `header` = `0xA5` ✓
- `state` = `0x01` (有目标)
- `yaw` = `0x3F 07 E0 6D` = `0.04363` rad ≈ 2.5°
- `direction` = `0x01` (左侧)
- `distance_mode` = `0x00` (无偏差)
- `timestamp_ms` = `0x56341200` = 1446048256 ms
- `t_offset` = `0x0064` = 100 ms
- `checksum` = 待计算

### 示例2：无目标

```
十六进制数据（16字节）：
A5 00 00 00 00 00 00 00 00 12 34 56 00 00 XX XX
```

**字段解析**：
- `state` = `0x00` (无目标)
- `yaw` = `0.0` rad
- 其他字段为默认值

---

## 🛠️ CRC16-Modbus 校验算法

### 算法特点
- **多项式**: `0xA001`
- **初始值**: `0xFFFF`
- **计算顺序**: 从低位到高位
- **结果格式**: 小端序

### C/C++ 参考实现

```cpp
uint16_t crc16_modbus(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}
```

### 使用方法

```cpp
// 1. 准备数据包（前14字节）
uint8_t packet[16];
// ... 填充字段 ...

// 2. 计算CRC（前14字节）
uint16_t crc = crc16_modbus(packet, 14);

// 3. 填充校验和（小端序）
packet[14] = crc & 0xFF;        // 低字节
packet[15] = (crc >> 8) & 0xFF; // 高字节
```

---

## 🔍 数据解析流程（电控侧）

### 1. 接收数据

```cpp
uint8_t buffer[16];
int bytes_read = read(serial_fd, buffer, 16);
if (bytes_read != 16) {
    // 数据不完整
    return;
}
```

### 2. 验证帧头

```cpp
if (buffer[0] != 0xA5) {
    // 帧头错误，丢弃数据
    return;
}
```

### 3. 校验CRC

```cpp
uint16_t received_crc = buffer[14] | (buffer[15] << 8);
uint16_t calculated_crc = crc16_modbus(buffer, 14);

if (received_crc != calculated_crc) {
    // 校验失败，数据损坏
    return;
}
```

### 4. 解析字段

```cpp
uint8_t state = buffer[1];
float yaw;
memcpy(&yaw, &buffer[2], 4);
uint8_t direction = buffer[6];
uint8_t distance_mode = buffer[7];
uint32_t timestamp_ms;
memcpy(&timestamp_ms, &buffer[8], 4);
uint16_t t_offset;
memcpy(&t_offset, &buffer[12], 2);
```

### 5. 执行控制

```cpp
if (state == 1) {
    // 有目标，执行旋转
    float yaw_deg = yaw * 180.0 / M_PI;
    motor_rotate_yaw(yaw_deg);
} else {
    // 无目标，保持当前姿态或搜索
}
```

---

## ⚙️ 配置与调试

### 启用串口调试日志

```yaml
# config/system_config.yaml
serial:
  debug: true  # 启用串口调试日志（每0.5秒输出一次）
```

**日志输出示例**：
```
[TX] state=1 yaw=+0.0436 rad (+2.50°) dir=左 ts=1446048256ms
```

### 测试串口连接

```bash
# 1. 查看串口设备
ls -l /dev/ttyACM*

# 2. 检查权限
sudo chmod 666 /dev/ttyACM0

# 3. 使用minicom测试
sudo minicom -D /dev/ttyACM0 -b 115200

# 4. 或使用Python测试
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

---

## 📌 注意事项

### 1. 字节序
- **所有多字节数据使用小端序（Little Endian）**
- 这是x86/ARM架构的标准字节序

### 2. 数据对齐
- 使用 `__attribute__((packed))` 确保结构体无填充字节
- 总长度必须严格为16字节

### 3. 实时性
- 视觉系统以 **30-31 FPS** 运行
- 检测延迟 < 33ms
- 串口发送为异步非阻塞

### 4. 错误处理
- 串口断开会自动尝试重连（每秒一次）
- CRC校验失败应丢弃数据包
- 建议实现超时检测（>100ms无数据视为异常）

### 5. 坐标系定义
- **yaw正方向**: 逆时针（从上往下看）
- **yaw=0**: 炮筒正对目标
- **yaw>0**: 目标在左侧，需要逆时针旋转
- **yaw<0**: 目标在右侧，需要顺时针旋转

---

## 🧪 测试工具

### Python测试脚本

```python
#!/usr/bin/env python3
import serial
import struct
import time

def crc16_modbus(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

# 打开串口
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

while True:
    # 接收16字节
    data = ser.read(16)
    if len(data) != 16:
        continue
    
    # 验证帧头
    if data[0] != 0xA5:
        print("帧头错误")
        continue
    
    # 校验CRC
    received_crc = struct.unpack('<H', data[14:16])[0]
    calculated_crc = crc16_modbus(data[:14])
    if received_crc != calculated_crc:
        print("CRC校验失败")
        continue
    
    # 解析数据
    state = data[1]
    yaw = struct.unpack('<f', data[2:6])[0]
    direction = data[6]
    distance_mode = data[7]
    timestamp_ms = struct.unpack('<I', data[8:12])[0]
    t_offset = struct.unpack('<H', data[12:14])[0]
    
    # 打印
    dir_str = ["正对", "左侧", "右侧"][direction]
    print(f"[RX] state={state} yaw={yaw:.4f}rad({yaw*57.2958:.2f}°) dir={dir_str} ts={timestamp_ms}ms")
    
ser.close()
```

---

## 📞 技术支持

如有疑问，请联系视觉组或参考代码：
- 发送实现: `src/mortar_vision_node.cpp` (第1320-1398行)
- 配置文件: `config/system_config.yaml`
- 测试脚本: `scripts/serial_monitor.sh`

---

**文档结束**
