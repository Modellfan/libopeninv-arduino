# Parameter Persistence Storage Scheme

This document explains how `ParameterPersistence` stores Arduino parameters in EEPROM. It mirrors the implementation in `include/params_persistence.h` so the layout and constraints are clear when porting or debugging.

## Overview

* EEPROM space is divided into a fixed number of equally sized **slots**. By default, four slots form a ring buffer that rotates on each save.
* Each slot contains a **record header** followed by a compact payload describing every eligible parameter (only parameters flagged as persistent and within supported size/type constraints).
* The most recent valid slot is selected using a monotonically increasing sequence number. Validation uses a CRC over the header, parameter count, and parameter payloads.

## Slot sizing and capacity

* The number of slots is set when constructing `ParameterPersistence` (default: `kDefaultSlotCount = 4`). A value of zero is coerced to one slot.
* Each slot size is `EEPROM.length() / slotCount`. The available payload capacity is `slotSize - sizeof(RecordHeader)`; saves fail if the payload would exceed this space or `uint16_t` limits.
* Saves also fail when total capacity is smaller than `sizeof(uint16_t)` (the count field), ensuring the payload always has room for the entry list.

### EEPROM fragmentation schematic

The EEPROM is fragmented into equally sized slots laid out back-to-back. The sequence number stored in each slot turns this layout into a ring buffer that rotates as new saves occur.

```
<EEPROM address 0>
┌──────────────┬────────────────────────────────────────┬──────────────┬────────────────────────────────────────┬─────┐
│   Slot 0      │              Slot 1                    │    Slot 2    │              Slot 3                    │ ... │
│ (slotSize B)  │             (slotSize B)               │ (slotSize B) │             (slotSize B)               │     │
└──────────────┴────────────────────────────────────────┴──────────────┴────────────────────────────────────────┴─────┘
```

Within a slot, the layout is:

```
Offset 0                  sizeof(RecordHeader)                slotSize
│                         │                                   │
┌─────────────────────────┬───────────────────────────────────┬────────────────────────────────────────────┐
│       RecordHeader      │   uint16_t entry_count            │    Parameter entries and raw bytes         │
│ (magic, version,        │ (at least 2 bytes; aborted saves │ (repeats entry header + value for each     │
│  payload_size, sequence,│ avoid overrunning payload_size)   │  persisted parameter; total size bounded   │
│  crc)                   │                                   │  by payload_size)                          │
└─────────────────────────┴───────────────────────────────────┴────────────────────────────────────────────┘
```

#### Example: 1024-byte EEPROM, four slots

* `slotCount = 4` ⇒ `slotSize = 256` bytes.
* Address ranges: Slot 0 = `0–255`, Slot 1 = `256–511`, Slot 2 = `512–767`, Slot 3 = `768–1023`.
* With `sizeof(RecordHeader) = 16`, the maximum payload (entry count + entries) per slot is `256 - 16 = 240` bytes.
* Saving data advances through slots in sequence order: first valid save lands in Slot 0 (sequence = 1), next in Slot 1 (sequence = 2), and so on; Slot 0 becomes the newest again once Slot 3 has been used.

## Record header

| Field | Type | Meaning |
| --- | --- | --- |
| `magic` | `uint32_t` | Constant `0x4F495053` (`'OIPS'`) that identifies valid records. |
| `version` | `uint16_t` | Current format version (`kVersion = 1`). |
| `payload_size` | `uint16_t` | Size in bytes of the payload immediately after the header. Must be at least `sizeof(uint16_t)` for the entry count. |
| `sequence` | `uint32_t` | Monotonic counter incremented on each successful save to order slots chronologically. |
| `crc` | `uint32_t` | FNV-1a hash over the header (with CRC zeroed), entry count, and all payload bytes. |

## Parameter entries

* Immediately after the header comes a `uint16_t` **entry count** followed by `count` parameter entries.
* Each entry starts with a `ParameterEntryHeader`:
  * `id` (`uint16_t`): Parameter identifier.
  * `type` (`uint8_t`): Encoded `ParameterType` value.
  * `size` (`uint16_t`): Number of data bytes that follow.
* The entry header is followed by the raw bytes of the parameter value. Variable-length strings are excluded because the persistence layer only supports bounded values.

## Eligibility rules

Parameters are persisted only when all conditions are true:

1. The parameter marks itself as persistent (`isPersistent()` returns true).
2. The type is neither `Unknown` nor `String` (variable-sized or unsupported).
3. The value size is less than or equal to `kMaxValueSize` (16 bytes) to keep payloads compact and predictable.

Parameters that fail any rule are skipped during save and ignored when loading, even if present in EEPROM.

## Save workflow

1. Ensure initialization by calling `begin()` if needed; this divides EEPROM into slots and scans existing records for validity and recency.
2. Iterate over all registered parameters, filtering by the eligibility rules to compute the entry count and projected payload size.
3. Abort if the payload would exceed capacity or `uint16_t` limits.
4. Build a `RecordHeader` with updated `sequence` (previous sequence + 1, or 1 if no valid data yet) and compute the CRC across the header, count, and parameter data.
5. Choose the next slot in the ring buffer (incrementing from the last used slot modulo the slot count) and write the header, count, and each entry with its raw bytes.
6. Update in-memory tracking so subsequent loads point to the newly written slot.

## Load workflow

1. Call `begin()` if needed to size slots and locate the most recent valid record by scanning every slot.
2. If no valid records are found, loading fails immediately.
3. Validate the chosen slot again before applying it: check magic/version, confirm payload bounds, and recompute the CRC using data read from EEPROM.
4. For each entry, look up the parameter by ID, confirm it is eligible and the stored type matches, then apply raw bytes via `setRawBytes`.
5. Loading returns true if at least one parameter was successfully applied.

## CRC calculation

* Uses the 32-bit FNV-1a hash starting at offset `2166136261` and multiplying by `16777619` after each XOR step.
* The CRC input order is: header with `crc` zeroed, entry count, each entry header, then each entry value byte as stored in EEPROM.
* Validation reads bytes directly from EEPROM to ensure the hash reflects stored data.

## Failure cases and safeguards

* Slots with bad magic, version mismatch, oversized payloads, or CRC mismatches are ignored during scans.
* Individual entries with values larger than `kMaxValueSize` are skipped while applying data to avoid buffer overruns.
* A failed save never updates the tracked `lastSlot` or `lastSequence`, keeping the previous valid data intact.

## Practical usage tips

* Call `ParameterPersistence::begin()` early in setup to ensure the library locates existing data before parameters are used.
* Use the persistent flag only for small fixed-size types (ints, floats, booleans, small structs) to stay within the 16-byte limit.
* If EEPROM size is small, consider reducing the slot count to maximize payload capacity per slot, or increase it for higher write endurance when capacity allows.
