#include <Arduino.h>
#include <ACAN_T4.h>
#include "canhardware_teensy41.h"

static const uint32_t kTestCanIds[] = { 0x120, 0x121, 0x122 };
static const uint32_t kSendPeriodMs = 1000;

CanHardwareTeensy41 CAN1(CanHardwareTeensy41::Can1, CanHardware::Baud500);
CanHardwareTeensy41 CAN2(CanHardwareTeensy41::Can2, CanHardware::Baud500);
CanHardwareTeensy41 CAN3(CanHardwareTeensy41::Can3, CanHardware::Baud500);

static CanHardware* canInterface[] = { &CAN1, &CAN2, &CAN3 };

static void PrintCanFrame(uint32_t canId, const uint32_t* data, uint8_t dlc)
{
    uint8_t bytes[8] = {0};
    memcpy(bytes, data, dlc);

    Serial.print("RX 0x");
    Serial.print(canId, HEX);
    Serial.print(" [");
    Serial.print(dlc);
    Serial.print("] ");
    for (uint8_t i = 0; i < dlc; i++)
    {
        if (bytes[i] < 0x10) Serial.print('0');
        Serial.print(bytes[i], HEX);
        if (i + 1 < dlc) Serial.print(' ');
    }
    Serial.println();
}

static void SetCanFilters()
{
    canInterface[0]->RegisterUserMessage(kTestCanIds[0]);
    canInterface[1]->RegisterUserMessage(kTestCanIds[1]);
    canInterface[2]->RegisterUserMessage(kTestCanIds[2]);
}

static bool CanCallback(uint32_t id, uint32_t data[2], uint8_t dlc)
{
    PrintCanFrame(id, data, dlc);
    return true;
}

static bool Can1Callback(uint32_t canId, uint32_t* data, uint8_t dlc)
{
    return CanCallback(canId, data, dlc);
}

static bool Can2Callback(uint32_t canId, uint32_t* data, uint8_t dlc)
{
    return CanCallback(canId, data, dlc);
}

static bool Can3Callback(uint32_t canId, uint32_t* data, uint8_t dlc)
{
    return CanCallback(canId, data, dlc);
}

static void OnCanClear()
{
    Serial.println("CAN filters cleared");
}

static FunctionPointerCallback can1Callback(Can1Callback, OnCanClear);
static FunctionPointerCallback can2Callback(Can2Callback, OnCanClear);
static FunctionPointerCallback can3Callback(Can3Callback, OnCanClear);

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    Serial.println("\n=== libopeninv-arduino CanHardwareTeensy41 Test ===");

    CAN1.AddCallback(&can1Callback);
    CAN2.AddCallback(&can2Callback);
    CAN3.AddCallback(&can3Callback);

    SetCanFilters();

    Serial.println("CAN bus initialized at 500 kbps");
    Serial.println("Listening for CAN IDs:");
    for (uint8_t i = 0; i < 3; i++)
    {
        Serial.print("  CAN");
        Serial.print(i + 1);
        Serial.print(": 0x");
        Serial.println(kTestCanIds[i], HEX);
    }
    Serial.println("Sending test frame every 1s on all buses");
}

void loop()
{
    CAN1.Poll();
    CAN2.Poll();
    CAN3.Poll();

    static uint32_t lastSend = 0;
    static uint32_t counters[3] = { 0, 0, 0 };
    if (millis() - lastSend >= kSendPeriodMs)
    {
        lastSend = millis();

        counters[0]++;
        counters[1]++;
        counters[2]++;

        uint32_t data1[2] = { counters[0], 0xA5A5A5A5 };
        uint32_t data2[2] = { counters[1], 0xA5A5A5A5 };
        uint32_t data3[2] = { counters[2], 0xA5A5A5A5 };

        CAN1.Send(kTestCanIds[0], data1, 8);
        CAN2.Send(kTestCanIds[1], data2, 8);
        CAN3.Send(kTestCanIds[2], data3, 8);

        Serial.print("TX CAN1 0x");
        Serial.print(kTestCanIds[0], HEX);
        Serial.print(" counter=");
        Serial.println(counters[0]);

        Serial.print("TX CAN2 0x");
        Serial.print(kTestCanIds[1], HEX);
        Serial.print(" counter=");
        Serial.println(counters[1]);

        Serial.print("TX CAN3 0x");
        Serial.print(kTestCanIds[2], HEX);
        Serial.print(" counter=");
        Serial.println(counters[2]);
    }
}
