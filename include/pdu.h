#pragma once

#include <array>
#include <stdint.h>
#include <stddef.h>
#include <tuple>
#include <type_traits>

#include "params.h"

namespace oi {

namespace detail {
uint8_t computeCRC8(const uint8_t* data, size_t len, uint8_t init, uint8_t polynomial);
}

struct Scaling {
    float factor = 1.0f;
    float offset = 0.0f;
};

template <typename T>
struct FieldDescriptor {
    Parameter<T>* param;
    uint16_t startBit;
    uint8_t bitLength;
    Scaling scaling;

    using param_type = T;
};

template <typename T>
constexpr FieldDescriptor<T> field(Parameter<T>& parameter, uint16_t startBit, uint8_t bitLength, Scaling scaling = {}) {
    return {&parameter, startBit, bitLength, scaling};
}

struct Counter {
    uint16_t startBit;
    uint8_t bitLength;
    uint32_t modulus;
};

struct CRC8 {
    using ComputeFn = uint8_t (*)(const uint8_t* data, size_t len, uint8_t init, uint8_t polynomial);

    uint16_t startBit;
    uint8_t init;
    uint8_t polynomial;
    uint8_t bitLength = 8;
    ComputeFn compute = detail::computeCRC8;
};

constexpr CRC8 crc8(uint16_t startBit, uint8_t init, uint8_t polynomial, uint8_t bitLength = 8, CRC8::ComputeFn compute = detail::computeCRC8) {
    return {startBit, init, polynomial, bitLength, compute};
}

namespace detail {

template <typename T>
constexpr int64_t encodeValue(const T& value, const Scaling& scaling) {
    const float numerator = static_cast<float>(value) - scaling.offset;
    const float scaled = numerator / scaling.factor;
    return static_cast<int64_t>(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
}

template <typename T>
constexpr T decodeValue(int64_t raw, const Scaling& scaling) {
    const float physical = static_cast<float>(raw) * scaling.factor + scaling.offset;
    return static_cast<T>(physical);
}

inline void setBits(uint8_t* buffer, size_t len, uint16_t startBit, uint8_t bitLength, uint64_t value) {
    for (uint8_t i = 0; i < bitLength; ++i) {
        const uint16_t bitIndex = startBit + i;
        const size_t byteIndex = bitIndex / 8;
        if (byteIndex >= len) {
            continue;
        }
        const uint8_t bitPos = bitIndex % 8;
        const uint8_t mask = static_cast<uint8_t>(1U << bitPos);
        const bool bit = (value >> i) & 0x1U;
        if (bit) {
            buffer[byteIndex] |= mask;
        } else {
            buffer[byteIndex] &= static_cast<uint8_t>(~mask);
        }
    }
}

inline uint64_t getBits(const uint8_t* buffer, size_t len, uint16_t startBit, uint8_t bitLength) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < bitLength; ++i) {
        const uint16_t bitIndex = startBit + i;
        const size_t byteIndex = bitIndex / 8;
        if (byteIndex >= len) {
            continue;
        }
        const uint8_t bitPos = bitIndex % 8;
        const uint8_t bit = (buffer[byteIndex] >> bitPos) & 0x1U;
        value |= static_cast<uint64_t>(bit) << i;
    }
    return value;
}

inline uint8_t computeCRC8(const uint8_t* data, size_t len, uint8_t init, uint8_t polynomial) {
    uint8_t crc = init;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const bool msb = (crc & 0x80U) != 0;
            crc <<= 1U;
            if (msb) {
                crc ^= polynomial;
            }
        }
    }
    return crc;
}

template <typename Tuple, typename Func>
constexpr void forEachInTuple(Tuple& tuple, Func&& func) {
    std::apply([&](auto&... elems) { (func(elems), ...); }, tuple);
}

template <typename Tuple, typename Func>
constexpr void forEachInTuple(const Tuple& tuple, Func&& func) {
    std::apply([&](const auto&... elems) { (func(elems), ...); }, tuple);
}

} // namespace detail

template <typename... Elements>
class PDU {
public:
    explicit constexpr PDU(uint32_t canId, Elements... elems)
        : canId_(canId), elements_(elems...) {}

    uint32_t id() const { return canId_; }

    void pack(uint8_t* buffer, size_t len = 8) {
        if (buffer == nullptr || len == 0) {
            return;
        }
        for (size_t i = 0; i < len; ++i) {
            buffer[i] = 0;
        }

        uint32_t nextCounter = counterValue_;
        detail::forEachInTuple(elements_, [&](auto& elem) {
            using ElemType = std::decay_t<decltype(elem)>;
            if constexpr (std::is_same<ElemType, Counter>::value) {
                if (elem.modulus > 0) {
                    nextCounter = (nextCounter + 1U) % elem.modulus;
                }
                detail::setBits(buffer, len, elem.startBit, elem.bitLength, nextCounter);
            } else if constexpr (std::is_same<ElemType, CRC8>::value) {
                // CRC handled after all fields are packed
            } else {
                using ValueType = typename std::remove_reference<decltype(elem)>::type;
                using ParamType = typename ValueType::param_type;
                if (elem.param != nullptr) {
                    const int64_t raw = detail::encodeValue<ParamType>(elem.param->getValue(), elem.scaling);
                    detail::setBits(buffer, len, elem.startBit, elem.bitLength, static_cast<uint64_t>(raw));
                }
            }
        });

        counterValue_ = nextCounter;
        applyCRC(buffer, len);
    }

    bool unpack(const uint8_t* buffer, size_t len = 8) {
        if (buffer == nullptr || len == 0) {
            return false;
        }
        const bool crcOk = validateCRC(buffer, len);

        detail::forEachInTuple(elements_, [&](auto& elem) {
            using ElemType = std::decay_t<decltype(elem)>;
            if constexpr (std::is_same<ElemType, Counter>::value) {
                const uint64_t value = detail::getBits(buffer, len, elem.startBit, elem.bitLength);
                rxCounter_ = static_cast<uint32_t>(value);
            } else if constexpr (std::is_same<ElemType, CRC8>::value) {
                // CRC already validated
            } else {
                using ValueType = typename std::remove_reference<decltype(elem)>::type;
                using ParamType = typename ValueType::param_type;
                const uint64_t raw = detail::getBits(buffer, len, elem.startBit, elem.bitLength);
                if (elem.param != nullptr) {
                    elem.param->setValue(detail::decodeValue<ParamType>(static_cast<int64_t>(raw), elem.scaling));
                }
            }
        });

        return crcOk;
    }

    uint32_t counter() const { return counterValue_; }
    uint32_t receivedCounter() const { return rxCounter_; }

private:
    void applyCRC(uint8_t* buffer, size_t len) {
        CRC8* crcDesc = nullptr;
        detail::forEachInTuple(elements_, [&](auto& elem) {
            using ElemType = std::decay_t<decltype(elem)>;
            if constexpr (std::is_same<ElemType, CRC8>::value) {
                crcDesc = &elem;
            }
        });

        if (crcDesc == nullptr) {
            return;
        }

        std::array<uint8_t, 8> temp{};
        for (size_t i = 0; i < len && i < temp.size(); ++i) {
            temp[i] = buffer[i];
        }
        detail::setBits(temp.data(), len, crcDesc->startBit, crcDesc->bitLength, 0);
        const auto computeFn = crcDesc->compute != nullptr ? crcDesc->compute : detail::computeCRC8;
        const uint8_t crc = computeFn(temp.data(), len, crcDesc->init, crcDesc->polynomial);
        detail::setBits(buffer, len, crcDesc->startBit, crcDesc->bitLength, crc);
    }

    bool validateCRC(const uint8_t* buffer, size_t len) const {
        const CRC8* crcDesc = nullptr;
        detail::forEachInTuple(elements_, [&](const auto& elem) {
            using ElemType = std::decay_t<decltype(elem)>;
            if constexpr (std::is_same<ElemType, CRC8>::value) {
                crcDesc = &elem;
            }
        });

        if (crcDesc == nullptr) {
            return true;
        }

        std::array<uint8_t, 8> temp{};
        for (size_t i = 0; i < len && i < temp.size(); ++i) {
            temp[i] = buffer[i];
        }
        detail::setBits(temp.data(), len, crcDesc->startBit, crcDesc->bitLength, 0);
        const auto computeFn = crcDesc->compute != nullptr ? crcDesc->compute : detail::computeCRC8;
        const uint8_t computed = computeFn(temp.data(), len, crcDesc->init, crcDesc->polynomial);
        const uint64_t received = detail::getBits(buffer, len, crcDesc->startBit, crcDesc->bitLength);
        return computed == received;
    }

    uint32_t canId_;
    std::tuple<Elements...> elements_;
    uint32_t counterValue_ = 0;
    uint32_t rxCounter_ = 0;
};

} // namespace oi

