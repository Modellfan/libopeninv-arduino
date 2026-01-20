#include <Arduino.h>
#include <ACAN_T4.h>
#include "canhardware_teensy41.h"
#include "isa_shunt.h"
#include "params.h"

static const uint32_t kPrintPeriodMs = 1000;

CanHardwareTeensy41 canHardware(CanHardwareTeensy41::Can1, CanHardware::Baud500);

static bool Can1Callback(uint32_t id, uint32_t *data, uint8_t dlc)
{
    (void)dlc;
    ISA::DecodeCAN(id, data);
    return true;
}

static void SetCanFilters()
{
    ISA::RegisterCanMessages(&canHardware);
}

static FunctionPointerCallback isaCallback(Can1Callback, SetCanFilters);

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Serial.println("\n=== ISA IVT Test (Teensy 4.1) ===");

    canHardware.AddCallback(&isaCallback);
    ISA::RegisterCanMessages(&canHardware);

    Serial.println("Listening for ISA IVT frames 0x521-0x528");
    Serial.println("Send 'i' to initialize, 'c' to init current, 'r' to restart");
}

void loop()
{
    canHardware.Poll();

    if (Serial.available())
    {
        const char cmd = static_cast<char>(Serial.read());
        if (cmd == 'i' || cmd == 'I')
        {
            ISA::initialize(&canHardware);
            Serial.println("ISA initialize sequence sent");
        }
        else if (cmd == 'c' || cmd == 'C')
        {
            ISA::initCurrent(&canHardware);
            Serial.println("ISA current calibration sent");
        }
        else if (cmd == 'r' || cmd == 'R')
        {
            ISA::RESTART(&canHardware);
            Serial.println("ISA restart sent");
        }
    }

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= kPrintPeriodMs)
    {
        lastPrint = millis();
        Serial.print("A=");
        Serial.print(Param::GetFloat(Param::isaCurrent));
        Serial.print(" V1=");
        Serial.print(Param::GetFloat(Param::isaVoltage1));
        Serial.print(" V2=");
        Serial.print(Param::GetFloat(Param::isaVoltage2));
        Serial.print(" V3=");
        Serial.print(Param::GetFloat(Param::isaVoltage3));
        Serial.print(" T=");
        Serial.print(Param::GetFloat(Param::isaTemperature));
        Serial.print(" Ah=");
        Serial.print(Param::GetFloat(Param::isaAh));
        Serial.print(" kW=");
        Serial.print(Param::GetFloat(Param::isaKW));
        Serial.print(" kWh=");
        Serial.println(Param::GetFloat(Param::isaKWh));
    }
}
