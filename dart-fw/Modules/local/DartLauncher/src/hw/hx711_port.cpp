#include "hx711_port.hpp"
#include "main.h"
#include "timebase.hpp"

namespace dart {
namespace {
constexpr auto kClockPin = GPIO_PIN_12;
constexpr auto kDataPin = GPIO_PIN_13;
constexpr std::uint32_t kResetTimeUs = 100;
constexpr std::uint32_t kClockTimeUs = 1;
}

Hx711Port::Hx711Port() {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_RESET);
    GPIO_InitTypeDef config{};
    config.Pin = kClockPin;
    config.Mode = GPIO_MODE_OUTPUT_PP;
    config.Pull = GPIO_NOPULL;
    config.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &config);
    config.Pin = kDataPin;
    config.Mode = GPIO_MODE_INPUT;
    config.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &config);
    // SCK 高于 60 µs 再拉低复位，默认选择 A 通道、128 倍增益。
    HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_SET);
    LibXR::Timebase::DelayMicroseconds(kResetTimeUs);
    HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_RESET);
    received_ms_ = LibXR::Timebase::GetMilliseconds();
}

Hx711Reading Hx711Port::Read() {
    const bool updated = HAL_GPIO_ReadPin(GPIOB, kDataPin) == GPIO_PIN_RESET;
    if (updated) {
        std::uint32_t bits = 0;
        // 避免抢占把 SCK 高电平拉长到 60 µs，导致 HX711 进入休眠。
        const auto interrupt_mask = __get_PRIMASK();
        __disable_irq();
        LibXR::Timebase::DelayMicroseconds(kClockTimeUs);
        for (int bit = 0; bit < 24; ++bit) {
            HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_SET);
            LibXR::Timebase::DelayMicroseconds(kClockTimeUs);
            bits = (bits << 1) | static_cast<std::uint32_t>(
                HAL_GPIO_ReadPin(GPIOB, kDataPin) == GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_RESET);
            LibXR::Timebase::DelayMicroseconds(kClockTimeUs);
        }
        // 第 25 个时钟指定下一次仍读取 A 通道、128 倍增益。
        HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_SET);
        LibXR::Timebase::DelayMicroseconds(kClockTimeUs);
        HAL_GPIO_WritePin(GPIOB, kClockPin, GPIO_PIN_RESET);
        LibXR::Timebase::DelayMicroseconds(kClockTimeUs);
        __set_PRIMASK(interrupt_mask);
        raw_ = static_cast<std::int32_t>(bits);
        if ((bits & 0x800000U) != 0) { raw_ -= 0x1000000; }
        ++samples_;
        received_ms_ = LibXR::Timebase::GetMilliseconds();
    }
    const std::uint32_t now = LibXR::Timebase::GetMilliseconds();
    return {raw_, samples_, now - received_ms_, updated};
}
}
