#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace dart {
constexpr std::uint8_t kTensionRightAddr = 1;
constexpr std::uint8_t kTensionLeftAddr = 2;
constexpr std::uint16_t kTensionRegStart = 0x0002;
constexpr std::uint16_t kTensionRegCount = 2;
constexpr std::size_t kTensionRequestSize = 8;
constexpr std::size_t kTensionResponseSize = 9;
constexpr std::size_t kTensionExceptionSize = 5;
constexpr std::uint8_t kTensionFuncRead = 0x03;
constexpr std::uint8_t kTensionFuncException = 0x83;
constexpr std::uint8_t kTensionResponseDataBytes = 4;
enum class ModbusParseStatus : std::uint8_t { Incomplete, Ok, Exception, Invalid };
std::uint16_t ModbusCrc16(const std::uint8_t *data, std::size_t length);
std::array<std::uint8_t, kTensionRequestSize> BuildTensionReadRequest(std::uint8_t addr);
ModbusParseStatus ParseTensionResponse(const std::uint8_t *data, std::size_t length,
                                       std::uint8_t expected_addr, std::int32_t &value);
}
