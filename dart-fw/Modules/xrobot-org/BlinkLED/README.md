# BlinkLED

LED 周期闪烁与致命错误指示模块 / Module that blinks an LED periodically and indicates fatal errors

## 1. 模块作用 / Purpose

构造后，BlinkLED 创建一个 LibXR 定时器任务，每隔 `blink_cycle` 毫秒翻转一次 LED 的输出电平。同时向 LibXR 注册致命错误回调：发生致命错误时，回调停止该定时器并进入无限循环，依次向 LED 写入 `false` 125 ms、`true` 125 ms、`false` 500 ms、`true` 500 ms，用与正常闪烁不同的节奏指示故障。

After construction, BlinkLED creates a LibXR timer task that toggles the LED output every `blink_cycle` milliseconds. It also registers a LibXR fatal-error callback. On a fatal error the callback stops the timer and loops forever writing `false` for 125 ms, `true` for 125 ms, `false` for 500 ms and `true` for 500 ms to the LED, a pattern that differs from the normal blinking and indicates the fault.

## 2. 构造接口 / Constructor

```cpp
BlinkLED(LibXR::GPIO& led, uint32_t blink_cycle = 250);
```

依赖：

- `led`：驱动 LED 的 `LibXR::GPIO`，取自 BSP 的硬件注册（`XR_REGISTER`）。

配置参数：

- `blink_cycle`：翻转周期，单位 ms，默认 250。

Dependencies:

- `led`: the `LibXR::GPIO` that drives the LED, taken from the BSP's Registration (`XR_REGISTER`).

Configuration parameters:

- `blink_cycle`: toggle period in ms, default 250.

## 3. Topic

无 / None

## 4. 配置示例 / Configuration Example

`xrobot instance add xrobot-org/BlinkLED` 写入的实例，`led` 填写为 BSP 中注册的 GPIO 名称：

An instance written by `xrobot instance add xrobot-org/BlinkLED`, with `led` set to a GPIO name registered by the BSP:

```yaml
modules:
  - module: xrobot-org/BlinkLED
    id: blink_led
    args:
      - led: LED_B
      - blink_cycle: 250
```

## 5. 依赖与硬件 / Dependencies and Hardware

依赖：LibXR。

硬件：一个由 `LibXR::GPIO` 驱动、在 BSP 中配置为输出的 LED 引脚，并通过 `XR_REGISTER` 注册。

Dependencies: LibXR.

Hardware: one LED pin driven through `LibXR::GPIO`, configured as an output in the BSP and registered with `XR_REGISTER`.
