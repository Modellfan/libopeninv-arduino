# libopeninv-arduino

Arduino port of the libopeninv library, providing CANOpen and parameter management functionality for Arduino and Teensy projects.

## Features

- **CANOpen SDO (Service Data Objects)**: Read and write parameters over CAN bus
- **CAN Message Mapping**: Automatically map parameters to/from CAN messages
- **Parameter System**: Type-safe parameter storage with min/max bounds
- **EEPROM Persistence**: Save parameters and CAN mappings to EEPROM
- **Arduino Compatible**: Works with Arduino framework on common microcontrollers, including Teensy 4.1

## Components

### Core Modules

- **params**: Parameter storage system with fixed-point and floating-point support
- **param_save**: EEPROM storage for parameters with CRC verification
- **cansdo**: CANOpen SDO protocol implementation for parameter access
- **canmap**: Bidirectional mapping between CAN messages and parameters
- **canhardware**: Abstract CAN hardware interface
- **canhardware_teensy41**: Teensy 4.1 wrapper for ACAN_T4 CAN driver

## Usage

### 1. Define Your Parameters

Edit `include/param_prj.h` to define your application parameters:

```cpp
#define PARAM_LIST \
    PARAM_ENTRY("Battery", canNodeId, "", 1, 127, 22, 1) \
    PARAM_ENTRY("Battery", cellVoltMin, "mV", 2500, 3000, 2800, 2) \
    PARAM_ENTRY("Battery", cellVoltMax, "mV", 4000, 4300, 4200, 3) \
    VALUE_ENTRY(packVoltage, "V", 1000) \
```

- `PARAM_ENTRY`: Writable parameters with min/max bounds
- `VALUE_ENTRY`: Read-only values (spot values)

### 2. Initialize in Your Code

```cpp
#include <ACAN_T4.h>
#include "canhardware_teensy41.h"
#include "params.h"
#include "param_save.h"
#include "cansdo.h"
#include "canmap.h"

// Create CAN and libopeninv objects
CanHardwareTeensy41 canHardware(CanHardwareTeensy41::Can1);
CanMap canMap(&canHardware);
CanSdo canSdo(&canHardware, &canMap);

void setup() {
    // Load defaults
    Param::LoadDefaults();

    // Load saved parameters from EEPROM
    parm_load();

    // Initialize CAN
    ACAN_T4_Settings settings(500000);
    ACAN_T4::can1.begin(settings);

    // Set CANOpen node ID
    canSdo.SetNodeId(Param::GetInt(Param::canNodeId));
}
```

### 3. Map CAN Messages to Parameters

```cpp
// Receive pack voltage from CAN ID 0x100, bytes 0-1 (16-bit little-endian)
// Scale by 0.1 (received value is in 0.1V units)
canMap.AddRecv(Param::packVoltage, 0x100, 0, 16, 0.1f);

// Send current on CAN ID 0x200, bytes 0-1 (16-bit little-endian)
// Scale by 10 (send in 0.1A units)
canMap.AddSend(Param::packCurrent, 0x200, 0, 16, 10.0f);

// Send all mapped messages periodically
canMap.SendAll();
```

### 4. Access Parameters via CANOpen SDO

Parameters can be accessed via CANOpen SDO:
- **SDO Request**: `0x600 + nodeId`
- **SDO Response**: `0x580 + nodeId`
- **Parameter Index**: `0x2000` (by parameter number) or `0x21XX` (by unique ID)

Example SDO read of parameter 0 (canNodeId):
```
Request:  [0x40, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00]
Response: [0x43, 0x00, 0x20, 0x00, <value>, 0x00, 0x00, 0x00]
```

### 5. Save Parameters to EEPROM

```cpp
// Save parameters
parm_save();

// Save CAN mappings
canMap.Save();
```

## Parameter Access Methods

```cpp
// Get parameter values
int val = Param::GetInt(Param::canNodeId);
float fval = Param::GetFloat(Param::packVoltage);
bool bval = Param::GetBool(Param::someFlag);

// Set parameter values
Param::Set(Param::canNodeId, FP_FROMINT(22));  // With range check
Param::SetInt(Param::canNodeId, 22);           // Direct set
Param::SetFloat(Param::packVoltage, 350.5f);   // Float set
```

## Parameter Change Callback

Override the parameter change callback to respond to changes:

```cpp
void Param::Change(Param::PARAM_NUM param) {
    switch(param) {
        case Param::canNodeId:
            canSdo.SetNodeId(Param::GetInt(param));
            break;

        case Param::cellVoltMax:
            // Update protection limits
            updateVoltageProtection();
            break;
    }
}
```

## CAN Message Mapping Details

### Bit Positioning
- **offsetBits**: Bit offset in 64-bit CAN message (0-63)
- **numBits**: Number of bits (1-32)
  - Positive: Little-endian
  - Negative: Big-endian

### Scaling
- **gain**: Multiply parameter by this before sending (divide after receiving)
- **offset**: Add this offset before/after scaling

Example:
```cpp
// Map temperature: CAN value = (param * 10) + 40
// Receives 0-255 representing -40°C to +215°C in 1°C steps
canMap.AddRecv(Param::temperature, 0x123, 8, 8, 0.1f, -40);
```

## EEPROM Usage

Parameters and CAN mappings are stored in EEPROM for persistence.

## Example Project

See `examples/canopen_basic/canopen_basic.ino` for a complete working example.

## Hardware Requirements

- Teensy 4.1 (tested) with CAN transceiver (e.g., TJA1050, MCP2551)
- PlatformIO or Teensyduino

## License

GPL v3 - Same as original libopeninv project

## Credits

Based on libopeninv by Johannes Huebner
Reference project: https://github.com/outlandnish/subaru-brz-hv-ecu/tree/main
Ported to Arduino by [Your Name]
