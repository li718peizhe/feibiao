#include "tension_modbus.hpp"

namespace dart {

std::uint16_t ModbusCrc16(const std::uint8_t *data, std::size_t length) {
    std::uint16_t crc = 0xFFFF;
    if (data == nullptr || length == 0) {
        return crc;
    }
    for (std::size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (std::uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 1U) != 0U) {
                crc = static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U);
            } else {
                crc = static_cast<std::uint16_t>(crc >> 1U);
            }
        }
    }
    return crc;
}

std::array<std::uint8_t, kTensionRequestSize> BuildTensionReadRequest(std::uint8_t addr) {
    std::array<std::uint8_t, kTensionRequestSize> frame{
        addr,
        kTensionFuncRead,
        static_cast<std::uint8_t>(kTensionRegStart >> 8U),
        static_cast<std::uint8_t>(kTensionRegStart & 0xFFU),
        static_cast<std::uint8_t>(kTensionRegCount >> 8U),
        static_cast<std::uint8_t>(kTensionRegCount & 0xFFU),
        0,
        0,
    };
    const std::uint16_t crc = ModbusCrc16(frame.data(), 6);
    frame[6] = static_cast<std::uint8_t>(crc & 0xFFU);
    frame[7] = static_cast<std::uint8_t>(crc >> 8U);
    return frame;
}

ModbusParseStatus ParseTensionResponse(const std::uint8_t *data, std::size_t length,
                                       std::uint8_t expected_addr, std::int32_t &value) {
    if (data == nullptr || length < 2) {
        return ModbusParseStatus::Incomplete;
    }
    if (data[0] != expected_addr) {
        return ModbusParseStatus::Invalid;
    }
    if (data[1] == kTensionFuncException) {
        if (length < kTensionExceptionSize) {
            return ModbusParseStatus::Incomplete;
        }
        const std::uint16_t expected_crc = static_cast<std::uint16_t>(data[3]) |
                                            static_cast<std::uint16_t>(data[4] << 8U);
        return ModbusCrc16(data, 3) == expected_crc ? ModbusParseStatus::Exception
                                                     : ModbusParseStatus::Invalid;
    }
    if (data[1] == kTensionFuncRead) {
        if (length < kTensionResponseSize) {
            return ModbusParseStatus::Incomplete;
        }
        if (data[2] != kTensionResponseDataBytes) {
            return ModbusParseStatus::Invalid;
        }
        const std::uint16_t expected_crc = static_cast<std::uint16_t>(data[7]) |
                                            static_cast<std::uint16_t>(data[8] << 8U);
        if (ModbusCrc16(data, 7) != expected_crc) {
            return ModbusParseStatus::Invalid;
        }
        const std::uint16_t sign = static_cast<std::uint16_t>(data[3] << 8U) | data[4];
        const std::uint16_t magnitude = static_cast<std::uint16_t>(data[5] << 8U) | data[6];
        if (sign > 1) {
            return ModbusParseStatus::Invalid;
        }
        value = sign == 1 ? -static_cast<std::int32_t>(magnitude)
                          : static_cast<std::int32_t>(magnitude);
        return ModbusParseStatus::Ok;
    }
    return ModbusParseStatus::Invalid;
}

}
