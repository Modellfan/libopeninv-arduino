#include <Arduino.h>
#include "params.h"
#include "pdu.h"

// Define Parameters used by the PDU (metadata is illustrative)
PARAM(byte, mode, 1, "Mode", "", "Engine", 0, 255, 0, 0);
PARAM(int, rpm, 2, "RPM", "rpm", "Engine", 0, 8000, 0, 0);
PARAM(float, tempC, 3, "TempC", "degC", "Engine", -40.0f, 215.0f, 0.0f, 0);

// Simple CRC handler that mirrors CRC8 descriptor signature
static uint8_t customCRC8(const uint8_t* data, size_t len, uint8_t init, uint8_t polynomial) {
    uint8_t crc = init;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            const bool msb = (crc & 0x80U) != 0;
            crc <<= 1U;
            if (msb) {
                crc ^= polynomial;
            }
        }
    }
    return crc;
}

// Define the PDU with compile-time descriptors
static auto pduEngine = openinv::PDU(
    0x123,
    openinv::field(openinv::params::mode, 0, 8),
    openinv::field(openinv::params::rpm, 8, 16),
    openinv::field(openinv::params::tempC, 24, 16, {0.1f, 0.0f}),
    openinv::Counter{56, 4, 16},
    openinv::crc8(7, 0xFF, 0x1D, 8, &customCRC8));

static void printBuffer(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (i) Serial.print(" ");
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ;
    }

    // Populate signal values
    openinv::params::mode.setValue(3);
    openinv::params::rpm.setValue(1500);
    openinv::params::tempC.setValue(85.0f);

    uint8_t txPayload[8] = {};
    pduEngine.pack(txPayload);

    Serial.println(F("TX payload with counter + CRC:"));
    printBuffer(txPayload, sizeof(txPayload));

    // Pretend we received the same frame back
    uint8_t rxPayload[8] = {};
    memcpy(rxPayload, txPayload, sizeof(rxPayload));

    bool crcOk = pduEngine.unpack(rxPayload);
    Serial.print(F("CRC valid: "));
    Serial.println(crcOk ? F("yes") : F("no"));

    Serial.print(F("Decoded mode: "));
    Serial.println(openinv::params::mode.getValue());
    Serial.print(F("Decoded rpm: "));
    Serial.println(openinv::params::rpm.getValue());
    Serial.print(F("Decoded tempC: "));
    Serial.println(openinv::params::tempC.getValue(), 1);
    Serial.print(F("Counter value: "));
    Serial.println(pduEngine.counter());
}

void loop() {
    // Nothing to do in loop for this demonstration
}

