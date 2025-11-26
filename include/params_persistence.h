#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <limits>

#include "params.h"

namespace oi {

// ParameterPersistence serializes registered parameters into EEPROM using a
// fixed-size ring buffer of slots. Each slot contains a RecordHeader followed
// by a compact list of entries describing the parameters that opted into
// persistence. The class intentionally keeps logic header-only so it can be
// used in small Arduino builds without requiring an additional translation
// unit.
class ParameterPersistence {
public:
    static constexpr uint8_t kDefaultSlotCount = 4;
    static constexpr uint16_t kMaxValueSize = 16;

    explicit ParameterPersistence(uint8_t slotCount = kDefaultSlotCount)
        : slotCount_(slotCount == 0 ? 1 : slotCount),
          initialized_(false),
          hasValidData_(false),
          lastSequence_(0U),
          lastSlot_(0U),
          slotSize_(0U) {}

    // Initializes slot bookkeeping and discovers the latest valid slot. Safe
    // to call multiple times; subsequent invocations are no-ops.
    void begin() {
        if (initialized_) {
            return;
        }

        slotSize_ = static_cast<size_t>(EEPROM.length()) / slotCount_;
        initialized_ = true;
        scanSlots();
    }

    // Serializes all eligible parameters into the next slot in the ring.
    // Returns false if there is insufficient capacity or if no valid payload
    // can be written (for example when EEPROM is smaller than a header).
    bool save() {
        if (!initialized_) {
            begin();
        }

        const size_t capacity = payloadCapacity();
        if (capacity < sizeof(uint16_t)) {
            return false;
        }

        uint16_t persistentCount = 0;
        size_t payloadSize = sizeof(uint16_t); // number of entries

        ParameterManager::instance().forEach([&](ParameterBase& param) {
            if (!canPersist(param)) {
                return;
            }
            ++persistentCount;
            payloadSize += sizeof(ParameterEntryHeader) + param.getSize();
        });

        if (payloadSize > capacity || payloadSize > std::numeric_limits<uint16_t>::max()) {
            return false;
        }

        RecordHeader header{};
        header.magic = kMagic;
        header.version = kVersion;
        header.payload_size = static_cast<uint16_t>(payloadSize);
        header.sequence = hasValidData_ ? (lastSequence_ + 1U) : 1U;
        header.crc = 0U;
        header.crc = computeParameterCRC(header, persistentCount);

        const size_t nextSlot = hasValidData_ ? ((lastSlot_ + 1U) % slotCount_) : 0U;
        if (!writeSlot(nextSlot, header, persistentCount)) {
            return false;
        }

        hasValidData_ = true;
        lastSequence_ = header.sequence;
        lastSlot_ = nextSlot;
        return true;
    }

    // Loads the most recent valid slot and applies parameter values. Returns
    // false when no valid slot was found or when initialization fails.
    bool load() {
        if (!initialized_) {
            begin();
        }

        if (!hasValidData_) {
            return false;
        }

        return applySlot(lastSlot_);
    }

private:
    // RecordHeader prefixes each slot and describes the payload that follows.
    // The CRC covers the header (with crc cleared), entry count, and every
    // persisted parameter entry.
    struct RecordHeader {
        uint32_t magic;
        uint16_t version;
        uint16_t payload_size;
        uint32_t sequence;
        uint32_t crc;
    };

    // ParameterEntryHeader precedes each parameter blob in the payload. Type
    // and size are persisted so records stay self-describing: loaders can skip
    // unknown IDs safely and detect schema drift even if firmware definitions
    // change between saves.
    struct ParameterEntryHeader {
        uint16_t id;
        uint8_t type;
        uint16_t size;
    };

    static constexpr uint32_t kMagic = 0x4F495053UL; // 'OIPS'
    static constexpr uint16_t kVersion = 1U;

    uint8_t slotCount_;
    bool initialized_;
    bool hasValidData_;
    uint32_t lastSequence_;
    size_t lastSlot_;
    size_t slotSize_;

    // Maximum payload bytes that fit after the RecordHeader in a single slot.
    size_t payloadCapacity() const {
        if (slotSize_ <= sizeof(RecordHeader)) {
            return 0U;
        }
        return slotSize_ - sizeof(RecordHeader);
    }

    // Filters out parameters that cannot be serialized safely. Strings and
    // unknown types are skipped because their sizes are variable or
    // unsupported, and large buffers are rejected to keep slot payloads
    // bounded.
    bool canPersist(const ParameterBase& param) const {
        if (!param.isPersistent()) {
            return false;
        }
        const ParameterType type = param.getType();
        if (type == ParameterType::Unknown) {
            return false;
        }
        return param.getSize() <= kMaxValueSize;
    }

    void scanSlots() {
        hasValidData_ = false;
        lastSequence_ = 0U;
        lastSlot_ = 0U;

        RecordHeader candidate{};
        uint16_t count = 0;

        for (size_t i = 0; i < slotCount_; ++i) {
            if (!validateSlot(i, candidate, count)) {
                continue;
            }

            if (!hasValidData_ || candidate.sequence > lastSequence_) {
                hasValidData_ = true;
                lastSequence_ = candidate.sequence;
                lastSlot_ = i;
            }
        }
    }

    template <typename UpdateFn>
    static void updateBytes(UpdateFn&& update, const void* ptr, size_t len) {
        const auto* bytes = static_cast<const uint8_t*>(ptr);
        for (size_t i = 0; i < len; ++i) {
            update(bytes[i]);
        }
    }

    uint32_t computeParameterCRC(const RecordHeader& header, uint16_t count) const {
        uint32_t hash = 2166136261UL;

        auto step = [&hash](uint8_t byte) {
            hash ^= byte;
            hash *= 16777619UL;
        };

        RecordHeader headerCopy = header;
        headerCopy.crc = 0U;
        updateBytes(step, &headerCopy, sizeof(headerCopy));
        updateBytes(step, &count, sizeof(count));

        ParameterManager::instance().forEach([&](ParameterBase& param) {
            if (!canPersist(param)) {
                return;
            }

            ParameterEntryHeader entry{};
            entry.id = param.getID();
            entry.type = static_cast<uint8_t>(param.getType());
            entry.size = static_cast<uint16_t>(param.getSize());

            updateBytes(step, &entry, sizeof(entry));
            updateBytes(step, param.getRawBytes(), entry.size);
        });

        return hash;
    }

    uint32_t computeSlotCRC(const RecordHeader& header, uint16_t count, size_t dataBase, size_t payloadSize) const {
        uint32_t hash = 2166136261UL;

        auto step = [&hash](uint8_t byte) {
            hash ^= byte;
            hash *= 16777619UL;
        };

        RecordHeader headerCopy = header;
        headerCopy.crc = 0U;
        updateBytes(step, &headerCopy, sizeof(headerCopy));
        updateBytes(step, &count, sizeof(count));

        size_t offset = dataBase;
        size_t consumed = 0;

        for (uint16_t i = 0; i < count && consumed < payloadSize; ++i) {
            ParameterEntryHeader entry{};
            EEPROM.get(static_cast<int>(offset), entry);
            offset += sizeof(entry);
            consumed += sizeof(entry);

            updateBytes(step, &entry, sizeof(entry));

            for (uint16_t b = 0; b < entry.size && consumed < payloadSize; ++b) {
                const uint8_t value = EEPROM.read(static_cast<int>(offset + b));
                step(value);
            }

            offset += entry.size;
            consumed += entry.size;
        }

        return hash;
    }

    bool validateSlot(size_t index, RecordHeader& header, uint16_t& count) const {
        if (slotSize_ == 0U) {
            return false;
        }

        const size_t base = index * slotSize_;
        EEPROM.get(static_cast<int>(base), header);

        if (header.magic != kMagic || header.version != kVersion) {
            return false;
        }

        const size_t capacity = payloadCapacity();
        if (header.payload_size > capacity || header.payload_size < sizeof(uint16_t)) {
            return false;
        }

        size_t offset = base + sizeof(RecordHeader);
        EEPROM.get(static_cast<int>(offset), count);
        offset += sizeof(count);

        size_t consumed = sizeof(count);
        for (uint16_t i = 0; i < count; ++i) {
            ParameterEntryHeader entry{};

            if (consumed + sizeof(entry) > header.payload_size) {
                return false;
            }

            EEPROM.get(static_cast<int>(offset), entry);
            offset += sizeof(entry);
            consumed += sizeof(entry);

            if (entry.size > kMaxValueSize) {
                return false;
            }

            if (consumed + entry.size > header.payload_size) {
                return false;
            }

            offset += entry.size;
            consumed += entry.size;
        }

        const uint32_t storedCrc = header.crc;
        const uint32_t calculated = computeSlotCRC(header, count, base + sizeof(RecordHeader) + sizeof(count), header.payload_size - sizeof(count));
        return storedCrc == calculated;
    }

    bool applySlot(size_t index) {
        RecordHeader header{};
        uint16_t count = 0;

        if (!validateSlot(index, header, count)) {
            return false;
        }

        const size_t base = index * slotSize_;
        size_t offset = base + sizeof(RecordHeader);
        EEPROM.get(static_cast<int>(offset), count);
        offset += sizeof(count);

        bool applied = false;

        for (uint16_t i = 0; i < count; ++i) {
            ParameterEntryHeader entry{};
            EEPROM.get(static_cast<int>(offset), entry);
            offset += sizeof(entry);

            if (entry.size > kMaxValueSize) {
                offset += entry.size;
                continue;
            }

            uint8_t value[kMaxValueSize] = {};
            for (uint16_t b = 0; b < entry.size; ++b) {
                value[b] = EEPROM.read(static_cast<int>(offset + b));
            }
            offset += entry.size;

            ParameterBase* param = ParameterManager::instance().getByID(entry.id);
            if (param == nullptr) {
                continue;
            }
            if (!canPersist(*param)) {
                continue;
            }
            if (param->getType() != static_cast<ParameterType>(entry.type)) {
                continue;
            }

            applied = param->setRawBytes(value, entry.size) || applied;
        }

        return applied;
    }

    bool writeSlot(size_t index, const RecordHeader& header, uint16_t count) {
        if (slotSize_ == 0U) {
            return false;
        }

        const size_t base = index * slotSize_;
        size_t offset = base;

        EEPROM.put(static_cast<int>(offset), header);
        offset += sizeof(header);

        EEPROM.put(static_cast<int>(offset), count);
        offset += sizeof(count);

        ParameterManager::instance().forEach([&](ParameterBase& param) {
            if (!canPersist(param)) {
                return;
            }

            ParameterEntryHeader entry{};
            entry.id = param.getID();
            entry.type = static_cast<uint8_t>(param.getType());
            entry.size = static_cast<uint16_t>(param.getSize());

            EEPROM.put(static_cast<int>(offset), entry);
            offset += sizeof(entry);

            const uint8_t* bytes = static_cast<const uint8_t*>(param.getRawBytes());
            for (uint16_t i = 0; i < entry.size; ++i) {
                EEPROM.write(static_cast<int>(offset + i), bytes[i]);
            }
            offset += entry.size;
        });

        return true;
    }
};

} // namespace oi

