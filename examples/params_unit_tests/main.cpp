#include <Arduino.h>
#include "params.h"

using oi::ParamFlag;
using oi::ParameterBase;
using oi::ParameterManager;
using oi::ParameterType;

// Example enum parameter
enum class SystemMode { Off, Idle, Run };
static const char* SystemModeNames[] = {"Off", "Idle", "Run"};

// Parameter definitions used by the tests
PARAM(float, engineTemp, 1, "EngineTemp", "°C", "Engine", -40.0f, 125.0f, 0.0f, 5000);
PARAM(int, rpm, 2, "RPM", "rpm", "Engine", 0, 8000, 0, 1000);
PARAM_BOOL(systemReady, 3, "SystemReady", "", "System", false, 0);
static const oi::ParamDesc<SystemMode> desc_systemMode {4, "SystemMode", "", "System", SystemMode::Off, SystemMode::Run, SystemMode::Off, 0, SystemModeNames, true};
namespace params { static oi::Parameter<SystemMode> systemMode { desc_systemMode }; }

struct TestRunner {
    size_t passed = 0;
    size_t failed = 0;
    size_t total = 0;

    void begin() {
        Serial.println();
        Serial.println(F("---- BEGIN PARAM TEST RESULTS ----"));
    }

    void assertTrue(bool condition, const char* testName, const char* details = "") {
        ++total;
        if (condition) {
            ++passed;
            Serial.print(F("[PASS] "));
        } else {
            ++failed;
            Serial.print(F("[FAIL] "));
        }
        Serial.print(testName);
        if (details != nullptr && details[0] != '\0') {
            Serial.print(F(" - "));
            Serial.print(details);
        }
        Serial.println();
    }

    void summary() {
        Serial.println();
        Serial.print(F("RESULT: "));
        Serial.print(passed);
        Serial.print(F("/"));
        Serial.print(total);
        Serial.print(F(" tests passed; "));
        Serial.print(failed);
        Serial.println(F(" failed"));
        Serial.println(F("---- END PARAM TEST RESULTS ----"));
    }
};

static TestRunner runner;
static bool testsRun = false;

bool hasFlag(ParameterBase& param, ParamFlag flag) {
    return (param.getFlags() & flag) != ParamFlag::None;
}

void testRegistration() {
    runner.assertTrue(ParameterManager::instance().size() == 4, "Registry contains all parameters");
    runner.assertTrue(ParameterManager::instance().getByID(1) == &params::engineTemp, "Lookup by ID works", "EngineTemp");
    runner.assertTrue(ParameterManager::instance().getByName("RPM") == &params::rpm, "Lookup by name works", "RPM");
}

void testTypesAndDefaults() {
    runner.assertTrue(params::engineTemp.getType() == ParameterType::Float, "Type resolver", "float -> Float");
    runner.assertTrue(params::rpm.getType() == ParameterType::Int, "Type resolver", "int -> Int");
    runner.assertTrue(params::systemReady.getType() == ParameterType::Bool, "Type resolver", "bool -> Bool");
    runner.assertTrue(params::systemMode.getType() == ParameterType::Enum, "Type resolver", "enum -> Enum");

    runner.assertTrue(params::engineTemp.getValue() == params::engineTemp.getDefault(), "Defaults initialized", "engineTemp");
    runner.assertTrue(params::systemMode.getValue() == SystemMode::Off, "Defaults initialized", "systemMode");
}

void testFlagsAndUpdates() {
    runner.assertTrue(hasFlag(params::rpm, ParamFlag::Initial), "Initial flag set before updates");

    const uint32_t start = millis();
    const int outOfRangeValue = params::rpm.getMax() + 1;
    bool outOfRangeRejected = !params::rpm.setValue(outOfRangeValue, start) && !params::rpm.isValid();
    runner.assertTrue(outOfRangeRejected && hasFlag(params::rpm, ParamFlag::Error), "Out-of-range values rejected");

    bool validUpdate = params::rpm.setValue(params::rpm.getMax() / 2, millis());
    runner.assertTrue(validUpdate && hasFlag(params::rpm, ParamFlag::Updated), "Valid update clears error and sets Updated");
    runner.assertTrue(params::rpm.isValid(), "Parameter valid after good update");
}

void testTimeouts() {
    params::rpm.setValue(params::rpm.getMin(), millis());
    delay(params::rpm.getTimeoutBudget() + 10); // ensure budget is exceeded using real time
    runner.assertTrue(hasFlag(params::rpm, ParamFlag::Timeout), "Timeout flag set when budget exceeded");

    params::rpm.setValue(params::rpm.getMin(), millis()); // new update clears timeout
    runner.assertTrue(!hasFlag(params::rpm, ParamFlag::Timeout), "Timeout cleared after fresh update");
}

void testRawBytesAndSize() {
    params::engineTemp.setValue(37.5f, 500);
    const void* raw = params::engineTemp.getRawBytes();
    float stored = *static_cast<const float*>(raw);
    runner.assertTrue(stored == 37.5f, "Raw bytes reflect latest value", "engineTemp");

    runner.assertTrue(params::systemMode.getSize() == sizeof(SystemMode), "Size matches underlying type", "SystemMode");
}

void testPersistenceAndEnums() {
    runner.assertTrue(params::systemMode.isPersistent(), "Persistent flag propagated", "systemMode");
    runner.assertTrue(params::systemMode.getEnumNames() == SystemModeNames, "Enum names retained");
}

void runAllTests() {
    runner.begin();
    testRegistration();
    testTypesAndDefaults();
    testFlagsAndUpdates();
    testTimeouts();
    testRawBytesAndSize();
    testPersistenceAndEnums();
    runner.summary();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect on boards that need it
    }
}

void loop() {
    if (!testsRun) {
        runAllTests();
        testsRun = true;
    }
    delay(1000);
}
