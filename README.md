📘 Parameter System Architecture for Embedded Devices (Arduino / STM32 Compatible)
🧩 Overview

This project defines a lightweight and extensible parameter management system for embedded devices such as Arduino and STM32.
It provides a unified interface for compile-time and runtime parameter handling, allowing efficient data management, serialization, and communication between modules.

The architecture is designed to be:

💡 Type-safe

⚡ Optimized for low RAM/CPU

🧱 Modular and scalable

🌍 Portable across embedded targets

🏗️ Software Architecture
File Structure
File	Description
params.h	Defines the core Parameter template class, the Parameters manager class, and associated interfaces.
params_proj.h	Defines all project-specific parameters using a single-line, constexpr-based syntax.
⚙️ Design Goals

✅ Arduino C++ & STM32 compatible

⚡ Minimal footprint (RAM/CPU)

🧠 Compile-time definition of most parameters (values, names, limits)

🔁 Optional runtime parameter creation (e.g., from JSON configuration)

🧩 Type safety across supported types:

float, int, byte, bool, enum, string

🔍 Self-descriptive parameters — each knows its name, type, and metadata at runtime

🧱 Constexpr and inline usage wherever possible

🚫 No redundant definitions (no duplicated names or IDs)

🧮 Simple, unified access syntax

params.name1

or params::name1

💾 Interfaces prepared for persistence and serialization

🧩 ID-based lookup for all parameters through an overarching Parameters manager

🧱 Core Classes
Parameter Template Class

Represents a single configuration or runtime variable.

Data Members
Member	Type	Description
id	uint16_t	Unique ID
value	templated	Current value
minVal	same type	Minimum allowed value
maxVal	same type	Maximum allowed value
defaultVal	same type	Default value
name	const char*	Name of the parameter
unit	const char*	Unit or description
category	const char*	Logical grouping
timeoutBudget	uint32_t	Timeout threshold in ms
lastUpdateTimestamp	uint32_t	Last update timestamp
enumNames	const char** (optional)	Enum name table for runtime introspection
flags	bitfield	Status flags
persistent	bool	Whether to persist the value
Supported Flags
Flag	Description
INITIAL	Parameter initialized but not yet updated
UPDATED	Updated since last read
TIMEOUT	No update within timeout budget
ERROR	CRC or range validation failed
VALID	True if none of the above flags are set
Parameter Functions
Function	Description
setValue(T newVal) / operator=	Sets value and triggers validation
getValue() / operator T()	Returns current value
getName()	Returns name string
getType()	Returns data type enum
getSize()	Returns data size
getMin() / getMax()	Returns min/max
getRawBytes()	Returns pointer to raw data
getFlags()	Returns current flags
isValid()	Returns validity state
🧩 Parameters Manager Class
Class: Parameters

Acts as the central registry for all parameters — both compile-time and runtime-defined.
Provides lookup, iteration, serialization, and persistence access points.

Responsibilities

Hold references/pointers to all parameters

Enable ID-based and name-based access

Provide serialization interface (to/from JSON, binary, etc.)

Manage optional persistent storage

Track update timestamps and validity

Core Functions
Function	Description
registerParameter(ParameterBase* p)	Add new parameter to internal list (runtime use)
getByID(uint16_t id)	Retrieve parameter by unique ID
getByName(const char* name)	Retrieve parameter by name
serializeAll()	Serialize all parameters (to JSON, etc.)
deserializeAll(const char* json)	Load parameter values from serialized data
forEach(callback)	Iterate over all registered parameters
saveAll() / loadAll()	Forward to persistence layer (optional)
🧮 Parameter Definition Syntax
Example — Compile-Time Definition

All compile-time parameters are defined in params_proj.h.
Each line defines one parameter with all metadata as constexpr expressions.

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


Access example:

if (params::engineTemp.isValid()) {
    float temp = params::engineTemp;
}

params::systemActive = true;
const char* server = params::mqttServer.getValue();

🧠 Dynamic / Runtime Parameters (Optional)

For advanced setups, parameters can also be defined and registered at runtime,
for example, from a configuration file or JSON string.

Example — Runtime JSON Definition
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
            auto* param = new Parameter<float>(id, atof(value), 0, 0, name, unit, category, 0);
            paramsManager.registerParameter(param);
        } else if (strcmp(type, "string") == 0) {
            auto* param = new Parameter<String>(id, String(value), "", "", name, unit, category, 0);
            paramsManager.registerParameter(param);
        }
        // Add other types as needed
    }
}


Example JSON input:

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

🧰 Key Principles

Compile-time optimization for static parameters

Runtime extensibility via manager class

No redundant data definitions

Portable embedded C++ code (no STL dependency if not needed)

Constexpr and templates ensure efficiency

🔮 Future Integration Points (placeholders)

PersistenceInterface → EEPROM / Flash storage

SerializationInterface → JSON / Binary encoding

PDUInterface → CAN / UART / MQTT communication

(not yet included in this version)

🧾 Summary
Aspect	Goal
Performance	Lightweight & efficient
Compatibility	Arduino + STM32
Scalability	Static + dynamic parameter definition
Safety	Validation & timeout logic
Ease of Use	One-line parameter definitions
Extensibility	Interfaces for storage & communication
