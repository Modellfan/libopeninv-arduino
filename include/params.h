#pragma once

#include <stdint.h>
#include <stddef.h>
#include <type_traits>
#include <string.h>

#if defined(ARDUINO)
#include <Arduino.h>
#else
// Fallback declaration so non-Arduino builds can provide a millis() implementation
extern uint32_t millis();
#endif

// Provide Arduino-style byte alias when not available
#ifndef byte
using byte = uint8_t;
#endif

namespace openinv {

// Supported parameter types for runtime introspection
enum class ParameterType : uint8_t {
    Unknown = 0,
    Float,
    Int,
    Byte,
    Bool,
    Enum,
    String,
};

// Status/health flags for parameters
enum class ParamFlag : uint8_t {
    None    = 0,
    Initial = 1 << 0,
    Updated = 1 << 1,
    Timeout = 1 << 2,
    Error   = 1 << 3,
};

inline ParamFlag operator|(ParamFlag lhs, ParamFlag rhs) {
    return static_cast<ParamFlag>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}
inline ParamFlag operator&(ParamFlag lhs, ParamFlag rhs) {
    return static_cast<ParamFlag>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}
inline ParamFlag& operator|=(ParamFlag& lhs, ParamFlag rhs) {
    lhs = lhs | rhs;
    return lhs;
}
inline ParamFlag& operator&=(ParamFlag& lhs, ParamFlag rhs) {
    lhs = lhs & rhs;
    return lhs;
}

// Forward declaration
class ParameterBase;

// Compile-time descriptor that keeps metadata in flash
// All fields are intentionally public to keep the type POD-like for embedded targets
// and allow aggregate initialization.
template <typename T>
struct ParamDesc {
    uint16_t id;
    const char* name;
    const char* unit;
    const char* category;
    T minVal;
    T maxVal;
    T defaultVal;
    uint32_t timeoutBudgetMs;
    const char** enumNames = nullptr;
    bool persistent = false;
};

// Base class for runtime-agnostic parameter handling
class ParameterManager;
class ParameterBase {
public:
    virtual ~ParameterBase() = default;

    virtual uint16_t getID() const = 0;
    virtual const char* getName() const = 0;
    virtual ParameterType getType() const = 0;
    virtual size_t getSize() const = 0;
    virtual const void* getRawBytes() const = 0;
    virtual bool setRawBytes(const void* data, size_t len) = 0;
    virtual ParamFlag getFlags() = 0;
    virtual bool isValid() const = 0;
    virtual const char* getUnit() const = 0;
    virtual const char* getCategory() const = 0;
    virtual uint32_t getTimeoutBudget() const = 0;
    virtual uint32_t getLastUpdateTimestamp() const = 0;
    virtual bool isPersistent() const = 0;
    virtual void checkTimeout(uint32_t nowMs) = 0;

protected:
    friend class ParameterManager;
    virtual void setFlag(ParamFlag flag) = 0;
    virtual void clearFlag(ParamFlag flag) = 0;
};

// Utility to map C++ types to the runtime parameter type
template <typename T, typename Enable = void>
struct TypeResolver { static constexpr ParameterType value = ParameterType::Unknown; };

template <typename T>
struct TypeResolver<T, typename std::enable_if<std::is_enum<T>::value>::type> {
    static constexpr ParameterType value = ParameterType::Enum;
};

template <> struct TypeResolver<float, void> { static constexpr ParameterType value = ParameterType::Float; };
template <> struct TypeResolver<int, void> { static constexpr ParameterType value = ParameterType::Int; };
template <> struct TypeResolver<byte, void> { static constexpr ParameterType value = ParameterType::Byte; };
template <> struct TypeResolver<bool, void> { static constexpr ParameterType value = ParameterType::Bool; };
template <> struct TypeResolver<const char*, void> { static constexpr ParameterType value = ParameterType::String; };
template <> struct TypeResolver<char*, void> { static constexpr ParameterType value = ParameterType::String; };

template <typename T>
constexpr ParameterType resolveType() {
    return TypeResolver<T>::value;
}

// Forward declaration of ParameterManager registry
class ParameterManager {
public:
    static ParameterManager& instance();

    bool registerParameter(ParameterBase* parameter);
    ParameterBase* getByID(uint16_t id) const;
    ParameterBase* getByName(const char* name) const;

    void checkTimeouts(uint32_t nowMs);

    template <typename Callback>
    void forEach(Callback&& cb) const {
        for (size_t i = 0; i < count_; ++i) {
            cb(*registry_[i]);
        }
    }

    size_t size() const { return count_; }

private:
    ParameterManager();

#ifdef PARAMS_MAX_PARAMS
    static constexpr size_t MAX_PARAMS = PARAMS_MAX_PARAMS;
#else
    static constexpr size_t MAX_PARAMS = 64;
#endif

    ParameterBase* registry_[MAX_PARAMS];
    size_t count_ = 0;
};

// Parameter implementation
template <typename T, bool IsString>
struct RangeValidator;

template <typename T>
class Parameter : public ParameterBase {
public:
    explicit Parameter(const ParamDesc<T>& descriptor)
        : desc_(descriptor), value_(descriptor.defaultVal) {
        flags_ = ParamFlag::Initial;
        ParameterManager::instance().registerParameter(this);
    }

    // Set value with optional timestamp and validation
    bool setValue(const T& newVal, uint32_t timestampMs = 0) {
        if (!validateRange(newVal)) {
            setFlag(ParamFlag::Error);
            return false;
        }
        value_ = newVal;
        clearFlag(ParamFlag::Error | ParamFlag::Timeout | ParamFlag::Initial);
        setFlag(ParamFlag::Updated);
        lastUpdateTimestamp_ = timestampMs;
        return true;
    }

    Parameter<T>& operator=(const T& newVal) {
        setValue(newVal);
        return *this;
    }

    operator T() const { return value_; }

    const T& getValue() const { return value_; }
    const T& getMin() const { return desc_.minVal; }
    const T& getMax() const { return desc_.maxVal; }
    const T& getDefault() const { return desc_.defaultVal; }
    const char* const* getEnumNames() const { return desc_.enumNames; }

    // ParameterBase overrides
    uint16_t getID() const override { return desc_.id; }
    const char* getName() const override { return desc_.name; }
    ParameterType getType() const override { return resolveType<T>(); }
    size_t getSize() const override { return sizeof(T); }
    const void* getRawBytes() const override { return static_cast<const void*>(&value_); }
    bool setRawBytes(const void* data, size_t len) override {
        if (data == nullptr || len != sizeof(T)) {
            return false;
        }
        if constexpr (resolveType<T>() == ParameterType::String) {
            return false;
        } else {
            T candidate;
            memcpy(&candidate, data, sizeof(T));
            return setValue(candidate);
        }
    }
    ParamFlag getFlags() override {
        checkTimeout(static_cast<uint32_t>(millis()));
        return flags_;
    }
    bool isValid() const override { return (flags_ & (ParamFlag::Error | ParamFlag::Timeout)) == ParamFlag::None; }
    const char* getUnit() const override { return desc_.unit; }
    const char* getCategory() const override { return desc_.category; }
    uint32_t getTimeoutBudget() const override { return desc_.timeoutBudgetMs; }
    uint32_t getLastUpdateTimestamp() const override { return lastUpdateTimestamp_; }
    bool isPersistent() const override { return desc_.persistent; }

    void checkTimeout(uint32_t nowMs) override {
        if (desc_.timeoutBudgetMs == 0) {
            return;
        }
        if (lastUpdateTimestamp_ == 0) {
            return;
        }
        if (nowMs - lastUpdateTimestamp_ > desc_.timeoutBudgetMs) {
            setFlag(ParamFlag::Timeout);
        } else {
            clearFlag(ParamFlag::Timeout);
        }
    }

protected:
    void setFlag(ParamFlag flag) override { flags_ |= flag; }
    void clearFlag(ParamFlag flag) override {
        flags_ = static_cast<ParamFlag>(static_cast<uint8_t>(flags_) & ~static_cast<uint8_t>(flag));
    }

private:
    bool validateRange(const T& candidate) const {
        return RangeValidator<T, resolveType<T>() == ParameterType::String>::check(candidate, desc_);
    }

    const ParamDesc<T>& desc_;
    T value_;
    ParamFlag flags_ = ParamFlag::None;
    uint32_t lastUpdateTimestamp_ = 0;
};

template <typename T, bool IsString>
struct RangeValidator {
    static bool check(const T& candidate, const ParamDesc<T>& desc) {
        return candidate >= desc.minVal && candidate <= desc.maxVal;
    }
};

// Booleans do not need range validation
template <>
struct RangeValidator<bool, false> {
    static bool check(const bool&, const ParamDesc<bool>&) {
        return true;
    }
};

template <typename T>
struct RangeValidator<T, true> {
    static bool check(const T&, const ParamDesc<T>&) {
        return true;
    }
};

// Range validation helper for enums when min/max are provided
// (already handled by default comparison operators)

// Utility to check uniqueness of IDs at compile time
constexpr bool checkUnique(const uint16_t* values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            if (values[i] == values[j]) {
                return false;
            }
        }
    }
    return true;
}

template <size_t N>
constexpr bool checkUnique(const uint16_t (&values)[N]) {
    return checkUnique(values, N);
}

// Helper macro to define parameters in a single line with metadata in Flash
#define PARAM_EXT(type, varname, id, name, unit, category, minv, maxv, def, timeoutMs, enumNames, persistent) \
    static const ::openinv::ParamDesc<type> desc_##varname { id, name, unit, category, minv, maxv, def, timeoutMs, enumNames, persistent }; \
    namespace params { static ::openinv::Parameter<type> varname { desc_##varname }; }

// Convenience macro for booleans where range checks are unnecessary
#define PARAM_BOOL(varname, id, name, unit, category, def, timeoutMs) \
    PARAM_EXT(bool, varname, id, name, unit, category, false, true, def, timeoutMs, nullptr, false)

// Default macro keeps optional enum names and persistence off
#define PARAM(type, varname, id, name, unit, category, minv, maxv, def, timeoutMs) \
    PARAM_EXT(type, varname, id, name, unit, category, minv, maxv, def, timeoutMs, nullptr, false)

} // namespace openinv

