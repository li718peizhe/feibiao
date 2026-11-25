#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: LED 周期闪烁与致命错误指示模块 / Module that blinks an LED periodically and indicates fatal errors
depends: []
=== END MANIFEST === */
// clang-format on

#include <memory>

#include "gpio.hpp"
#include "libxr_assert.hpp"
#include "libxr_cb.hpp"
#include "libxr_def.hpp"
#include "thread.hpp"
#include "timer.hpp"

/**
 * @brief LED 周期闪烁模块，并在致命错误时以特殊节奏指示故障。
 *        Module that blinks an LED periodically and indicates fatal errors with a
 *        distinct pattern.
 */
class BlinkLED
{
 public:
  /**
   * @brief 构造 BlinkLED，创建并启动闪烁定时器，注册致命错误回调。
   *        Construct BlinkLED, create and start the blink timer, and register the
   *        fatal-error callback.
   *
   * @param led 驱动 LED 的 GPIO。
   *            GPIO that drives the LED.
   * @param blink_cycle 翻转周期，单位 ms。
   *                    Toggle period in ms.
   */
  BlinkLED(
      LibXR::GPIO& led,
      uint32_t blink_cycle = 250)
      : led_(std::addressof(led)),
        timer_handle_(LibXR::Timer::CreateTask(BlinkTaskFun, this, blink_cycle))
  {
    LibXR::Timer::Add(timer_handle_);
    LibXR::Timer::Start(timer_handle_);

    auto error_callback = LibXR::Callback<const char*, uint32_t>::Create(
        [](bool in_isr, BlinkLED* led, const char* file, uint32_t line)
        {
          UNUSED(file);
          UNUSED(line);

          LibXR::Timer::Stop(led->timer_handle_);

          if (!in_isr)
          {
            while (true)
            {
              led->led_->Write(false);
              LibXR::Thread::Sleep(125);
              led->led_->Write(true);
              LibXR::Thread::Sleep(125);
              led->led_->Write(false);
              LibXR::Thread::Sleep(500);
              led->led_->Write(true);
              LibXR::Thread::Sleep(500);
            }
          }
        },
        this);

    LibXR::Assert::RegisterFatalErrorCallback(error_callback);
  }

  /**
   * @brief 定时器任务：翻转 LED 输出电平。
   *        Timer task that toggles the LED output level.
   *
   * @param blink BlinkLED 实例。
   *              BlinkLED instance.
   */
  static void BlinkTaskFun(BlinkLED* blink)
  {
    blink->flag_ = !blink->flag_;
    blink->led_->Write(blink->flag_);
  }

 private:
  bool flag_ = false;
  LibXR::GPIO* led_;
  LibXR::Timer::TimerHandle timer_handle_;
};
