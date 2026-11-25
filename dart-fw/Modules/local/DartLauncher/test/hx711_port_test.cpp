// 运行生产驱动，以 GPIO 和时间替身检查 HX711 线上协议。
#include "hx711_port.hpp"
#include "main.h"
#include "timebase.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

GPIO_TypeDef hx711_test_gpio;
namespace {
int checks = 0;
bool gpio_enabled = false, ready = false;
GPIO_PinState clock_level = GPIO_PIN_RESET;
std::uint32_t interrupt_mask = 0, word = 0;
std::uint64_t time_us = 0, last_edge_us = 0;
std::uint32_t rising_edges = 0, data_reads = 0;
bool reading = false;
std::vector<GPIO_InitTypeDef> configurations;
std::vector<std::uint64_t> high_widths, low_widths;

void Check(bool condition, const char *label) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}

void Prepare(std::uint32_t bits, bool available = true) {
    word = bits;
    ready = available;
    rising_edges = 0;
    data_reads = 0;
    high_widths.clear();
    low_widths.clear();
    last_edge_us = time_us;
    reading = true;
}
}

void __HAL_RCC_GPIOB_CLK_ENABLE() { gpio_enabled = true; }

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config) {
    Check(gpio_enabled && port == GPIOB, "GPIOB clock is enabled before setup");
    configurations.push_back(*config);
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, std::uint16_t pin, GPIO_PinState level) {
    Check(port == GPIOB && pin == GPIO_PIN_12, "only PB12 drives the clock");
    if (level == clock_level) { return; }
    const auto duration = time_us - last_edge_us;
    if (level == GPIO_PIN_SET) {
        low_widths.push_back(duration);
        ++rising_edges;
    } else {
        high_widths.push_back(duration);
    }
    if (reading) {
        Check(interrupt_mask == 1, "clock edges cannot be interrupted");
    }
    clock_level = level;
    last_edge_us = time_us;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, std::uint16_t pin) {
    Check(port == GPIOB && pin == GPIO_PIN_13, "only PB13 is read as data");
    if (clock_level == GPIO_PIN_RESET) {
        Check(rising_edges == 0, "readiness is checked before sending any clock");
        return ready ? GPIO_PIN_RESET : GPIO_PIN_SET;
    }
    Check(rising_edges >= 1 && rising_edges <= 24, "data is read on 24 data clocks");
    Check(time_us - last_edge_us >= 1, "data has settled before sampling");
    ++data_reads;
    return ((word >> (24 - rising_edges)) & 1U) != 0 ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

std::uint32_t __get_PRIMASK() { return interrupt_mask; }
void __disable_irq() { interrupt_mask = 1; }
void __set_PRIMASK(std::uint32_t value) { interrupt_mask = value; }
void LibXR::Timebase::DelayMicroseconds(std::uint32_t us) { time_us += us; }
std::uint32_t LibXR::Timebase::GetMilliseconds() {
    return static_cast<std::uint32_t>(time_us / 1000);
}

int main() {
    dart::Hx711Port port;
    Check(configurations.size() == 2, "only clock and data GPIOs are configured");
    Check(configurations[0].Pin == GPIO_PIN_12 &&
              configurations[0].Mode == GPIO_MODE_OUTPUT_PP &&
              configurations[0].Pull == GPIO_NOPULL,
          "PB12 is a push-pull clock output");
    Check(configurations[1].Pin == GPIO_PIN_13 &&
              configurations[1].Mode == GPIO_MODE_INPUT &&
              configurations[1].Pull == GPIO_PULLUP,
          "PB13 is pulled up so a disconnected module stays not ready");
    Check(high_widths.size() == 1 && high_widths[0] >= 60 && clock_level == GPIO_PIN_RESET,
          "startup resets HX711 and leaves clock low for A/128");

    Prepare(0, false);
    const auto start_us = time_us;
    auto sample = port.Read();
    Check(!sample.updated && sample.samples == 0 && sample.raw == 0,
          "no sample is reported while the module is not ready");
    Check(rising_edges == 0 && data_reads == 0 && time_us == start_us,
          "not-ready polling sends no clocks and does not wait");
    Check(interrupt_mask == 0, "not-ready polling leaves interrupts enabled");

    const std::array<std::int32_t, 8> values{
        0, 1, 0x123456, 8388607, -8388608, -1, -1193046, 0};
    std::uint32_t samples = 0;
    for (const auto value : values) {
        Prepare(static_cast<std::uint32_t>(value) & 0xFFFFFFU);
        sample = port.Read();
        Check(sample.updated && sample.raw == value && sample.samples == ++samples,
              "24-bit MSB-first two's-complement data is preserved, including both rails");
        Check(sample.age_ms == 0, "new data is timestamped at the read");
        Check(rising_edges == 25 && data_reads == 24,
              "one read sends 25 clocks to keep the next conversion on A/128");
        Check(clock_level == GPIO_PIN_RESET && interrupt_mask == 0,
              "read leaves clock low and restores enabled interrupts");
        for (const auto width : high_widths) {
            Check(width >= 1 && width <= 50, "each high pulse meets HX711 timing");
        }
        for (const auto width : low_widths) {
            Check(width >= 1, "each low pulse meets HX711 timing");
        }
    }

    Prepare(0xFFFFFFU);
    interrupt_mask = 1;
    sample = port.Read();
    Check(sample.raw == -1 && sample.samples == ++samples && interrupt_mask == 1,
          "read preserves an existing interrupt mask");
    interrupt_mask = 0;
    const auto received_ms = LibXR::Timebase::GetMilliseconds();
    time_us += 1500000;
    Prepare(0, false);
    sample = port.Read();
    Check(!sample.updated && sample.raw == -1 && sample.samples == samples &&
              sample.age_ms == LibXR::Timebase::GetMilliseconds() - received_ms,
          "not-ready polling retains the last sample and exposes its age");

    time_us = (std::uint64_t{1} << 32) * 1000 - 2000;
    Prepare(0x123456U);
    sample = port.Read();
    const auto before_wrap_ms = LibXR::Timebase::GetMilliseconds();
    time_us += 5000;
    Prepare(0, false);
    sample = port.Read();
    Check(sample.raw == 0x123456 && !sample.updated &&
              sample.age_ms == LibXR::Timebase::GetMilliseconds() - before_wrap_ms &&
              sample.age_ms == 5,
          "sample age survives the 32-bit millisecond wrap");
    std::printf("PASS: %d HX711 checks\n", checks);
    return 0;
}
