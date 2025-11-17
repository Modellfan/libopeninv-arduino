# 📘 Parameter System Architecture for Embedded Devices (Arduino / STM32 Compatible)

## 🧩 Overview
This project defines a **lightweight and extensible parameter management system** for embedded devices such as Arduino and STM32.  
It provides a unified interface for **compile-time and runtime parameter handling**, allowing efficient data management, serialization, and communication between modules.

The architecture is designed to be:
- 💡 **Type-safe**
- ⚡ **Optimized for low RAM/CPU**
- 🧱 **Modular and scalable**
- 🌍 **Portable across embedded targets**

---

## 🏗️ Software Architecture

### File Structure

| File | Description |
|------|--------------|
| `params.h` | Defines the core `Parameter` template class, the `Parameters` manager class, and associated interfaces. |
| `params_proj.h` | Defines all project-specific parameters using a single-line, constexpr-based syntax. |

---

## ⚙️ Design Goals

- ✅ **Arduino C++ & STM32 compatible**
- ⚡ **Minimal footprint** (RAM/CPU)
- 🧠 **Compile-time definition** of most parameters (values, names, limits)
- 🔁 **Optional runtime parameter creation** (e.g., from JSON configuration)
- 🧩 **Type safety** across supported types:
  - `float`, `int`, `byte`, `bool`, `enum`, `string`
- 🔍 **Self-descriptive parameters** — each knows its name, type, and metadata at runtime
- 🚫 **No redundant definitions** (no duplicated names or IDs)
- 🧮 **Simple, unified access syntax**
  - `params.name1`
  - or `params::name1`
- 💾 **Interfaces prepared** for persistence and serialization
- 🧩 **ID-based lookup** for all parameters through an overarching `Parameters` manager

---

## 🧱 Core Classes

### `Parameter` Template Class

Represents a single configuration or runtime variable.

#### **Data Members**
| Member | Type | Description |
|---------|------|-------------|
| `id` | `uint16_t` | Unique ID |
| `value` | templated | Current value |
| `minVal` | same type | Minimum allowed value |
| `maxVal` | same type | Maximum allowed value |
| `defaultVal` | same type | Default value |
| `name` | `const char*` | Name of the parameter |
| `unit` | `const char*` | Unit or description |
| `category` | `const char*` | Logical grouping |
| `timeoutBudget` | `uint32_t` | Timeout threshold in ms |
| `lastUpdateTimestamp` | `uint32_t` | Last update timestamp |
| `enumNames` | `const char**` *(optional)* | Enum name table for runtime introspection |
| `flags` | bitfield | Status flags |
| `persistent` | `bool` | Whether to persist the value |

---

### Supported Flags

| Flag | Description |
|------|--------------|
| `INITIAL` | Parameter initialized but not yet updated |
| `UPDATED` | Updated since last read |
| `TIMEOUT` | No update within timeout budget |
| `ERROR` | CRC or range validation failed |
| `VALID` | True if none of the above flags are set |

---

### **Parameter Functions**

| Function | Description |
|-----------|--------------|
| `setValue(T newVal)` / `operator=` | Sets value and triggers validation |
| `getValue()` / `operator T()` | Returns current value |
| `getName()` | Returns name string |
| `getType()` | Returns data type enum |
| `getSize()` | Returns data size |
| `getMin()` / `getMax()` | Returns min/max |
| `getRawBytes()` | Returns pointer to raw data |
| `getFlags()` | Returns current flags |
| `isValid()` | Returns validity state |

---

## 🧩 Parameters Manager Class

### Class: `Parameters`

Acts as the **central registry** for all parameters — both compile-time and runtime-defined.  
Provides lookup, iteration, serialization, and persistence access points.

#### **Responsibilities**
- Hold references/pointers to all parameters
- Enable **ID-based and name-based access**
- Provide **serialization** interface (to/from JSON, binary, etc.)
- Manage optional **persistent storage**
- Track **update timestamps and validity**

#### **Core Functions**
| Function | Description |
|-----------|--------------|
| `registerParameter(ParameterBase* p)` | Add new parameter to internal list (runtime use) |
| `getByID(uint16_t id)` | Retrieve parameter by unique ID |
| `getByName(const char* name)` | Retrieve parameter by name |
| `serializeAll()` | Serialize all parameters (to JSON, etc.) |
| `deserializeAll(const char* json)` | Load parameter values from serialized data |
| `forEach(callback)` | Iterate over all registered parameters |
| `saveAll()` / `loadAll()` | Forward to persistence layer (optional) |

---

## 🧮 Parameter Definition Syntax

### Example — **Compile-Time Definition**

All compile-time parameters are defined in `params_proj.h`.  
Each line defines one parameter with all metadata as constexpr expressions.

```cpp
// params_proj.h

#include "params.h"

namespace params {

// Float parameter
constexpr Parameter<float> engineTemp {
    1, 0.0f, -40.0f, 125.0f, "EngineTemp", "°C", "Engine", 1000
};

// Integer parameter
constexpr Parameter<int> rpm {
    2, 0, 0, 10000, "RPM", "rpm", "Engine", 1000
};

// Boolean parameter
constexpr Parameter<bool> systemActive {
    3, false, false, true, "SystemActive", "", "System", 0
};

// Enum parameter
enum class Mode { OFF, IDLE, RUN, ERROR };
constexpr const char* ModeNames[] = {"OFF", "IDLE", "RUN", "ERROR"};
constexpr Parameter<Mode> systemMode {
    4, Mode::OFF, Mode::OFF, Mode::ERROR, "Mode", "", "System", 0, ModeNames
};

// String parameter (e.g. MQTT server)
constexpr Parameter<const char*> mqttServer {
    5, "mqtt.local", "", "", "MQTTServer", "", "Network", 0
};

} // namespace params
```

# 📘 Parameter Usage Examples

## 🧩 Example — Parameter Access

```cpp
if (params::engineTemp.isValid()) {
    float temp = params::engineTemp;
    Serial.println(temp);
}

params::systemActive = true;

const char* server = params::mqttServer.getValue();
Serial.println(server);
```

🧠 Dynamic / Runtime Parameters (Optional)

To enable flexible configuration, parameters can also be created at runtime,
for example loaded from a JSON configuration file or network message.

🧩 Example — Runtime JSON Definition
#include "params.h"
#include "ArduinoJson.h"

Parameters paramsManager;

void setupRuntimeParams(const char* jsonConfig) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, jsonConfig);

    for (JsonObject p : doc["parameters"].as<JsonArray>()) {
        uint16_t id = p["id"];
        const char* name = p["name"];
        const char* category = p["category"];
        const char* unit = p["unit"];
        const char* type = p["type"];
        const char* value = p["value"];

        if (strcmp(type, "float") == 0) {
            auto* param = new Parameter<float>(
                id, atof(value), 0, 0, name, unit, category, 0
            );
            paramsManager.registerParameter(param);
        } else if (strcmp(type, "string") == 0) {
            auto* param = new Parameter<String>(
                id, String(value), "", "", name, unit, category, 0
            );
            paramsManager.registerParameter(param);
        }
        // Add more type cases as needed
    }
}

Example JSON input
{
  "parameters": [
    { "id": 10, "name": "wifiSSID", "type": "string", "value": "MyNetwork", "category": "Network", "unit": "" },
    { "id": 11, "name": "updateRate", "type": "float", "value": "0.5", "category": "System", "unit": "s" }
  ]
}

🧩 Access by ID or Name
auto* param = paramsManager.getByID(5);
if (param && param->isValid()) {
    Serial.println(param->getName());
}

auto* mqttParam = paramsManager.getByName("MQTTServer");
if (mqttParam) mqttParam->setValue("mqtt.remote.net");

🧠 Compile-Time vs Runtime Overview
Aspect	Compile-Time	Runtime
Definition	constexpr / static in headers	JSON, network, or user input
Storage	Flash memory (program)	Dynamic allocation
Access	params::name	paramsManager.getByName() / getByID()
Performance	Very fast	Slightly slower
Typical Use	System constants, calibration	User config, network parameters
🧰 Key Principles

Compile-time optimization

Clean, minimal parameter definitions

Dynamic extensibility via ParameterManager

Unified API for static + runtime parameters

No redundant definitions

Cross-platform (Arduino / STM32)

Minimal footprint

Flash-based metadata for static parameters

🔮 Future Integration Points

Planned future interfaces for extended functionality:

PersistenceInterface → EEPROM / Flash storage

SerializationInterface → JSON / binary encoding

PDUInterface → CAN / UART / MQTT communication

(Interfaces not yet implemented in this version.)


9. Namespace and Multiple Header Support

You can define parameters across multiple headers:

// params_engine.h
namespace params { PARAM(float, engineTemp, ...); }

// params_network.h
namespace params { PARAM(string, mqttServer, ...); }


Everything still works because:

Each parameter self-registers

Namespace keeps API clean

No linker conflicts if static is used

8. Unique ID Checking

C++ cannot automatically detect all parameters in a namespace.

Options:

✔ Central enum
enum class ParamID : uint16_t { EngineTemp = 1, RPM = 2 };


Compile-time prevention of duplicate names.

✔ ID list + static_assert(checkUnique())
constexpr uint16_t PARAM_IDS[] = {1, 2, 3};
static_assert(checkUnique(PARAM_IDS));

✔ Macros auto-generate ID list

More automation but less transparency.