# BuzzerAlarm

无源蜂鸣器开机提示音与致命错误报警模块 / Passive buzzer Module for a start-up tune and fatal-error alarms

## 1. 模块作用 / Purpose

构造时，BuzzerAlarm 向 LibXR 注册致命错误回调，并播放一段开机提示音（B4 200 ms、G3 200 ms、B4 400 ms），构造期间阻塞约 800 ms。发生致命错误时，LibXR 的致命错误循环反复调用该回调：每次以 `alarm_freq` 鸣叫 `alarm_duration` 毫秒，随后等待 `alarm_delay` 毫秒；LibXR 在两次调用之间另等待 500 ms，默认参数下约每 1100 ms 报警一次。LibXR 保存一个致命错误回调，后注册的回调替换先注册的，例如后构造的 BlinkLED 注册的回调。

模块提供两个公有方法：

- `void Play(uint32_t freq, uint32_t duration)`：以 `freq` Hz、0.5% 占空比鸣叫 `duration` 毫秒（阻塞），然后关闭 PWM。
- `void PlayNote(NoteName note, uint32_t octave, uint32_t duration)`：按音名和八度（十二平均律，A4 = 440 Hz）换算频率后调用 `Play`。`NoteName` 为 `C`、`Cs`、`D`、`Ds`、`E`、`F`、`Fs`、`G`、`Gs`、`A`、`As`、`B`。

Upon construction, BuzzerAlarm registers a LibXR fatal-error callback and plays a start-up tune (B4 200 ms, G3 200 ms, B4 400 ms); the constructor blocks for about 800 ms. On a fatal error, the LibXR fatal-error loop calls the callback repeatedly: each call beeps at `alarm_freq` for `alarm_duration` ms and then waits `alarm_delay` ms; LibXR waits another 500 ms between two calls, so with the default arguments the alarm repeats about every 1100 ms. LibXR keeps one fatal-error callback, and a callback registered later replaces an earlier one, for example the callback registered by a BlinkLED constructed afterwards.

The Module provides two public methods:

- `void Play(uint32_t freq, uint32_t duration)`: beep at `freq` Hz with 0.5% duty for `duration` ms (blocking), then disable the PWM.
- `void PlayNote(NoteName note, uint32_t octave, uint32_t duration)`: convert the note name and octave (equal temperament, A4 = 440 Hz) to a frequency and call `Play`. `NoteName` is `C`, `Cs`, `D`, `Ds`, `E`, `F`, `Fs`, `G`, `Gs`, `A`, `As`, `B`.

## 2. 构造接口 / Constructor

```cpp
BuzzerAlarm(LibXR::PWM& pwm,
            uint32_t alarm_freq = 1500,
            uint32_t alarm_duration = 300,
            uint32_t alarm_delay = 300);
```

依赖：

- `pwm`：驱动无源蜂鸣器的 `LibXR::PWM` 通道，取自 BSP 的硬件注册（`XR_REGISTER`）。

配置参数：

- `alarm_freq`：致命错误报警频率，单位 Hz，默认 1500。
- `alarm_duration`：每次报警的鸣叫时长，单位 ms，默认 300。
- `alarm_delay`：每次报警鸣叫后的等待时间，单位 ms，默认 300。

Dependencies:

- `pwm`: the `LibXR::PWM` channel that drives the passive buzzer, taken from the BSP's Registration (`XR_REGISTER`).

Configuration parameters:

- `alarm_freq`: fatal-error alarm frequency in Hz, default 1500.
- `alarm_duration`: beep length per alarm in ms, default 300.
- `alarm_delay`: pause after each alarm beep in ms, default 300.

## 3. Topic

无 / None

## 4. 配置示例 / Configuration Example

`xrobot instance add xrobot-org/BuzzerAlarm` 写入的实例，`pwm` 填写为 BSP 中注册的 PWM 名称：

An instance written by `xrobot instance add xrobot-org/BuzzerAlarm`, with `pwm` set to a PWM name registered by the BSP:

```yaml
modules:
  - module: xrobot-org/BuzzerAlarm
    id: buzzer
    args:
      - pwm: pwm_tim12_ch2
      - alarm_freq: 1500
      - alarm_duration: 300
      - alarm_delay: 300
```

## 5. 依赖与硬件 / Dependencies and Hardware

依赖：LibXR。

硬件：一个无源蜂鸣器，由 BSP 中的一路 `LibXR::PWM` 驱动，并通过 `XR_REGISTER` 注册。

Dependencies: LibXR.

Hardware: one passive buzzer driven by a `LibXR::PWM` channel of the BSP and registered with `XR_REGISTER`.
