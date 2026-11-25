# MadgwickAHRS

基于 Madgwick 梯度下降算法的姿态和航向参考系统（AHRS）模块 / Attitude and Heading Reference System (AHRS) Module based on the Madgwick gradient-descent filter

## 1. 模块作用 / Purpose

构造后，MadgwickAHRS 创建线程 `ahrs`（`HIGH` 优先级，栈深 `param.task_stack_depth`）。线程订阅陀螺仪与加速度计 Topic，每收到一条陀螺仪消息执行一次更新，使用最近收到的加速度计消息。加速度计向量为零时仅积分陀螺仪，不做校正。更新结果以 `LibXR::Quaternion<float>` 和 `LibXR::EulerAngle<float>`（单位 rad）发布。

`OnMonitor()` 在四元数含有 NaN 或 Inf 时输出警告。

Upon construction, MadgwickAHRS creates the thread `ahrs` (`HIGH` priority, stack depth `param.task_stack_depth`). The thread subscribes to the gyroscope and accelerometer Topics and runs one update per gyroscope message, using the most recently received accelerometer message. When the accelerometer vector is zero, only the gyroscope is integrated, without correction. The result is published as `LibXR::Quaternion<float>` and `LibXR::EulerAngle<float>` (rad).

`OnMonitor()` logs a warning when the quaternion contains NaN or Inf.

## 2. 时间戳约定 / Timestamp Convention

陀螺仪 Topic 是时间基准。每次更新用相邻两条陀螺仪消息的 Topic 消息时间戳计算 `dt`（第一次更新 `dt = 0`），并以同一个时间戳发布 `ahrs_quaternion` 与 `ahrs_euler`。

The gyroscope Topic is the time base. Each update computes `dt` from the Topic message timestamps of consecutive gyroscope messages (`dt = 0` on the first update) and publishes `ahrs_quaternion` and `ahrs_euler` with that same timestamp.

## 3. RamFS 命令 / RamFS Command

模块向 `ramfs` 添加命令 `ahrs`，`interval_ms` 限制在 2 到 1000 ms：

- `ahrs`：打印用法。
- `ahrs show <time_ms> <interval_ms>`：周期打印欧拉角、四元数和 `dt`。
- `ahrs print_quat <time_ms> <interval_ms>`：以 VOFA+ 格式打印 `w,x,y,z`。
- `ahrs test`：静止 3 s 后测量 10 s 的航向漂移，以 °/min 输出。

The Module adds the command `ahrs` to `ramfs`; `interval_ms` is clamped to 2 to 1000 ms:

- `ahrs`: print the usage.
- `ahrs show <time_ms> <interval_ms>`: print the Euler angles, the quaternion and `dt` periodically.
- `ahrs print_quat <time_ms> <interval_ms>`: print `w,x,y,z` in VOFA+ format.
- `ahrs test`: after 3 s of standing still, measure the yaw drift over 10 s and print it in °/min.

## 4. 构造接口 / Constructor

```cpp
struct Param
{
  float beta;
  const char* gyro_topic_name;
  const char* accl_topic_name;
  const char* quaternion_topic_name;
  const char* euler_topic_name;
  uint32_t task_stack_depth;
};

MadgwickAHRS(LibXR::RamFS& ramfs,
             const Param& param = {.beta = 0.05f,
                                   .gyro_topic_name = "imu_gyro",
                                   .accl_topic_name = "imu_accl",
                                   .quaternion_topic_name = "ahrs_quaternion",
                                   .euler_topic_name = "ahrs_euler",
                                   .task_stack_depth = 2048});
```

依赖：

- `ramfs`：接收 `ahrs` 命令的 RamFS。

配置参数（`Param`）：

- `beta`：Madgwick 滤波增益，默认 0.05。
- `gyro_topic_name`、`accl_topic_name`：订阅的陀螺仪与加速度计 Topic 名称，默认 `imu_gyro`、`imu_accl`。
- `quaternion_topic_name`、`euler_topic_name`：发布的四元数与欧拉角 Topic 名称，默认 `ahrs_quaternion`、`ahrs_euler`。
- `task_stack_depth`：线程栈深，单位字节，默认 2048。

Dependencies:

- `ramfs`: the RamFS that receives the `ahrs` command.

Configuration parameters (`Param`):

- `beta`: Madgwick filter gain, default 0.05.
- `gyro_topic_name`, `accl_topic_name`: names of the subscribed gyroscope and accelerometer Topics, defaults `imu_gyro`, `imu_accl`.
- `quaternion_topic_name`, `euler_topic_name`: names of the published quaternion and Euler angle Topics, defaults `ahrs_quaternion`, `ahrs_euler`.
- `task_stack_depth`: thread stack depth in bytes, default 2048.

## 5. Topic

| Topic | 方向 | 类型 | 说明 |
| --- | --- | --- | --- |
| `gyro_topic_name`（默认 `imu_gyro`） | 订阅 | `Eigen::Matrix<float, 3, 1>` | 角速度，单位 rad/s |
| `accl_topic_name`（默认 `imu_accl`） | 订阅 | `Eigen::Matrix<float, 3, 1>` | 加速度，归一化后使用，单位任意 |
| `quaternion_topic_name`（默认 `ahrs_quaternion`） | 发布 | `LibXR::Quaternion<float>` | 姿态四元数 |
| `euler_topic_name`（默认 `ahrs_euler`） | 发布 | `LibXR::EulerAngle<float>` | 欧拉角，单位 rad |

订阅的 Topic 名称须设为 IMU 实例发布的名称，例如 BMI088 的 `bmi088_gyro` 与 `bmi088_accl`；订阅的 Topic 尚未创建时，`ahrs` 线程等待它被创建。

| Topic | Direction | Type | Meaning |
| --- | --- | --- | --- |
| `gyro_topic_name` (default `imu_gyro`) | Subscribe | `Eigen::Matrix<float, 3, 1>` | Angular rate in rad/s |
| `accl_topic_name` (default `imu_accl`) | Subscribe | `Eigen::Matrix<float, 3, 1>` | Acceleration, normalized before use, any unit |
| `quaternion_topic_name` (default `ahrs_quaternion`) | Publish | `LibXR::Quaternion<float>` | Attitude quaternion |
| `euler_topic_name` (default `ahrs_euler`) | Publish | `LibXR::EulerAngle<float>` | Euler angles in rad |

The subscribed Topic names are set to the names published by the IMU instance, for example `bmi088_gyro` and `bmi088_accl` of BMI088; while a subscribed Topic does not exist yet, the `ahrs` thread waits for it to be created.

## 6. 配置示例 / Configuration Example

`xrobot instance add xrobot-org/MadgwickAHRS` 写入的实例，`ramfs` 填写为 BSP 通过 `XR_REGISTER`（硬件注册）注册的 RamFS 名称，`beta` 取 BSP 配置中的 0.033，Topic 名称按 IMU 实例的发布名称填写：

An instance written by `xrobot instance add xrobot-org/MadgwickAHRS`, with `ramfs` set to the RamFS name registered by the BSP with `XR_REGISTER` (Registration), `beta` set to 0.033 as in the BSP configuration and the Topic names set to the names published by the IMU instance:

```yaml
modules:
  - module: xrobot-org/MadgwickAHRS
    id: ahrs
    args:
      - ramfs: ramfs
      - param:
          beta: 0.033f
          gyro_topic_name: "bmi088_gyro"
          accl_topic_name: "bmi088_accl"
          quaternion_topic_name: "ahrs_quaternion"
          euler_topic_name: "ahrs_euler"
          task_stack_depth: 2048
```

## 7. 依赖与硬件 / Dependencies and Hardware

依赖：LibXR。陀螺仪与加速度计数据来自发布对应 Topic 的 IMU 模块，例如 `xrobot-org/BMI088`。

硬件：无。

Dependencies: LibXR. The gyroscope and accelerometer data come from an IMU Module that publishes the Topics, for example `xrobot-org/BMI088`.

Hardware: none.
