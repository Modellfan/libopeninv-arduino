#include <Arduino.h>
#include "params.h"

using namespace openinv;

// Enum parameter example
enum class Mode { OFF, IDLE, RUN, ERROR };
static const char* ModeNames[] = {"OFF", "IDLE", "RUN", "ERROR"};

// Parameters defined with the single-line macro keep metadata in Flash
PARAM(float, engineTemp, 1, "EngineTemp", "°C", "Engine", -40.0f, 125.0f, 0.0f, 1000);
PARAM(int, rpm, 2, "RPM", "rpm", "Engine", 0, 10000, 0, 1000);
PARAM(bool, systemActive, 3, "SystemActive", "", "System", false, true, false, 0);
PARAM_EXT(Mode, systemMode, 4, "Mode", "", "System", Mode::OFF, Mode::ERROR, Mode::OFF, 0, ModeNames, true);

void setup() {
    Serial.begin(115200);

    params::engineTemp = 42.5f;
    params::rpm = 1500;
    params::systemActive = true;
    params::systemMode = Mode::RUN;

    Serial.println(F("Registered parameters:"));
    ParameterManager::instance().forEach([](ParameterBase& param) {
        Serial.println(param.getName());
    });
}

void loop() {
    ParameterManager::instance().checkTimeouts(millis());
    delay(1000);
}

