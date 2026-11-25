#pragma once
#include <cstdint>

struct GPIO_TypeDef {};
extern GPIO_TypeDef hx711_test_gpio;
inline auto *const GPIOB = &hx711_test_gpio;
constexpr std::uint16_t GPIO_PIN_12 = 1U << 12, GPIO_PIN_13 = 1U << 13;
constexpr std::uint32_t GPIO_MODE_INPUT = 0, GPIO_MODE_OUTPUT_PP = 1;
constexpr std::uint32_t GPIO_NOPULL = 0, GPIO_PULLUP = 1, GPIO_SPEED_FREQ_LOW = 0;
enum GPIO_PinState { GPIO_PIN_RESET, GPIO_PIN_SET };
struct GPIO_InitTypeDef {
    std::uint32_t Pin = 0, Mode = 0, Pull = 0, Speed = 0;
};

void __HAL_RCC_GPIOB_CLK_ENABLE();
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void HAL_GPIO_WritePin(GPIO_TypeDef *, std::uint16_t, GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *, std::uint16_t);
std::uint32_t __get_PRIMASK();
void __disable_irq();
void __set_PRIMASK(std::uint32_t);
