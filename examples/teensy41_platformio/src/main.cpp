#include <Arduino.h>
#include <ACAN_T4.h>
#include "canhardware_teensy41.h"
#include "params.h"
#include "param_save.h"
#include "cansdo.h"
#include "canmap.h"

static const uint32_t kCanBitrate = 500000;

CanHardwareTeensy41 canHardware(CanHardwareTeensy41::Can1);
CanMap canMap(&canHardware);
CanSdo canSdo(&canHardware, &canMap);

static bool Can1Callback(uint32_t canId, uint32_t data[2], uint8_t dlc)
{
    canMap.HandleRx(canId, data, dlc);
    canSdo.HandleRx(canId, data, dlc);
    return true;
}

static void SetCanFilters()
{
    canMap.HandleClear();
    canSdo.HandleClear();
}

static FunctionPointerCallback canCallback(Can1Callback, SetCanFilters);

void Param::Change(Param::PARAM_NUM param)
{
    switch(param)
    {
        case Param::canNodeId:
            canSdo.SetNodeId(Param::GetInt(param));
            break;
        default:
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Serial.println("\n=== libopeninv-arduino CANOpen Example (Teensy 4.1) ===");

    Param::LoadDefaults();

    if (parm_load() == 0)
        Serial.println("Parameters loaded from EEPROM");
    else
        Serial.println("No saved parameters, using defaults");

    Serial.print("CAN Node ID: ");
    Serial.println(Param::GetInt(Param::canNodeId));

    ACAN_T4_Settings settings(kCanBitrate);
    const uint32_t errorCode = ACAN_T4::can1.begin(settings);
    if (errorCode != 0)
    {
        Serial.print("ERROR: CAN initialization failed (code=0x");
        Serial.print(errorCode, HEX);
        Serial.println(")");
        while (1) delay(100);
    }

    canSdo.SetNodeId(Param::GetInt(Param::canNodeId));

    canHardware.AddCallback(&canCallback);

    canMap.AddRecv(Param::packVoltage, 0x100, 0, 16, 0.1f);
    canMap.AddSend(Param::packCurrent, 0x200, 0, 16, 10.0f);
    canMap.Save();

    Serial.println("CANOpen ready");
}

void loop()
{
    canHardware.Poll();

    static unsigned long lastSend = 0;
    if (millis() - lastSend >= 100)
    {
        lastSend = millis();

        static float voltage = 300.0f;
        static float current = 50.0f;

        voltage += (random(-10, 10) / 10.0f);
        current += (random(-5, 5) / 10.0f);

        Param::SetFloat(Param::packVoltage, voltage);
        Param::SetFloat(Param::packCurrent, current);

        canMap.SendAll();
    }

    if (Serial.available())
    {
        char cmd = Serial.read();

        if (cmd == 's' || cmd == 'S')
        {
            Serial.println("Saving parameters to EEPROM...");
            uint32_t crc = parm_save();
            Serial.print("Saved with CRC: 0x");
            Serial.println(crc, HEX);
        }
        else if (cmd == 'l' || cmd == 'L')
        {
            Serial.println("Loading parameters from EEPROM...");
            if (parm_load() == 0)
                Serial.println("Parameters loaded successfully");
            else
                Serial.println("Failed to load parameters (CRC error)");
        }
        else if (cmd == 'p' || cmd == 'P')
        {
            Serial.println("\nCurrent Parameters:");
            Serial.print("  CAN Node ID: ");
            Serial.println(Param::GetInt(Param::canNodeId));
            Serial.print("  Pack Voltage: ");
            Serial.print(Param::GetFloat(Param::packVoltage));
            Serial.println(" V");
            Serial.print("  Pack Current: ");
            Serial.print(Param::GetFloat(Param::packCurrent));
            Serial.println(" A");
        }
    }
}
