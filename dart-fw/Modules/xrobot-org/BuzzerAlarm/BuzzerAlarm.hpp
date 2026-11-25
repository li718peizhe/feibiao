#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: 无源蜂鸣器开机提示音与致命错误报警模块 / Passive buzzer Module for a start-up tune and fatal-error alarms
depends: []
=== END MANIFEST === */
// clang-format on

#include <cmath>
#include <memory>

#include "libxr_def.hpp"
#include "pwm.hpp"
#include "thread.hpp"

/**
 * @brief 无源蜂鸣器模块，播放开机提示音并在致命错误时报警。
 *        Passive buzzer Module that plays a start-up tune and sounds an alarm on fatal
 *        errors.
 */
class BuzzerAlarm
{
 public:
  /**
   * @brief 十二平均律的音名，Cs 表示 C 升号，其余同理。
   *        Note names in twelve-tone equal temperament; Cs is C sharp, and so on.
   */
  // NOLINTNEXTLINE
  enum class NoteName
  {
    C = 0,  ///< C
    Cs,     ///< C#
    D,      ///< D
    Ds,     ///< D#
    E,      ///< E
    F,      ///< F
    Fs,     ///< F#
    G,      ///< G
    Gs,     ///< G#
    A,      ///< A
    As,     ///< A#
    B       ///< B
  };

  /**
   * @brief 构造 BuzzerAlarm，注册致命错误回调并播放开机提示音（约 800 ms，阻塞）。
   *        Construct BuzzerAlarm, register the fatal-error callback and play the start-up
   *        tune (about 800 ms, blocking).
   *
   * @param pwm 驱动蜂鸣器的 PWM。
   *            PWM that drives the buzzer.
   * @param alarm_freq 致命错误报警频率，单位 Hz。
   *                   Fatal-error alarm frequency in Hz.
   * @param alarm_duration 每次报警的鸣叫时长，单位 ms。
   *                       Beep length per alarm in ms.
   * @param alarm_delay 每次报警鸣叫后的等待时间，单位 ms。
   *                    Pause after each alarm beep in ms.
   */
  BuzzerAlarm(
      LibXR::PWM& pwm,
      uint32_t alarm_freq = 1500,
      uint32_t alarm_duration = 300,
      uint32_t alarm_delay = 300)
      : alarm_freq_(alarm_freq),
        alarm_duration_(alarm_duration),
        alarm_delay_(alarm_delay),
        pwm_(std::addressof(pwm))
  {
    LibXR::Assert::RegisterFatalErrorCallback(
        LibXR::Callback<const char*, uint32_t>::Create(
            [](bool in_isr, BuzzerAlarm* alarm, const char* file, uint32_t line)
            {
              UNUSED(file);
              UNUSED(line);

              alarm->Play(alarm->alarm_freq_, alarm->alarm_duration_);
              if (!in_isr)
              {
                LibXR::Thread::Sleep(alarm->alarm_delay_);
              }
            },
            this));
    PlayNote(NoteName::B, 4, 200);
    PlayNote(NoteName::G, 3, 200);
    PlayNote(NoteName::B, 4, 400);
  }

  /**
   * @brief 以指定频率鸣叫（0.5% 占空比），阻塞 duration 毫秒后关闭 PWM。
   *        Beep at the given frequency (0.5% duty), block for duration ms, then disable
   *        the PWM.
   *
   * @param freq 频率，单位 Hz。
   *             Frequency in Hz.
   * @param duration 鸣叫时长，单位 ms。
   *                 Beep length in ms.
   */
  void Play(uint32_t freq, uint32_t duration)
  {
    pwm_->SetConfig({.frequency = freq});
    pwm_->Enable();
    pwm_->SetDutyCycle(0.005);
    LibXR::Thread::Sleep(duration);
    pwm_->Disable();
  }

  /**
   * @brief 按音名和八度（A4 = 440 Hz）换算频率并鸣叫。
   *        Convert the note name and octave (A4 = 440 Hz) to a frequency and beep.
   *
   * @param note 音名。
   *             Note name.
   * @param octave 八度。
   *               Octave.
   * @param duration 鸣叫时长，单位 ms。
   *                 Beep length in ms.
   */
  void PlayNote(NoteName note, uint32_t octave, uint32_t duration)
  {
    int midi_num = static_cast<int>(note) + static_cast<int>((octave + 1) * 12);
    float freq = 440.0f * std::pow(2.0f, (static_cast<float>(midi_num) - 69.0f) / 12.0f);
    Play(static_cast<uint32_t>(freq), static_cast<uint32_t>(duration));
  }

 private:
  uint32_t alarm_freq_;
  uint32_t alarm_duration_;
  uint32_t alarm_delay_;

  LibXR::PWM* pwm_;
};
