/*
 * Arduino port of libopeninv CanHardware
 * Wraps the existing CANBus class to provide the CanHardware interface
 */
#include "canhardware_arduino.h"

CanHardwareArduino::CanHardwareArduino(ACAN_T4* canBus)
    : CanHardware(), can(canBus)
{
}

void CanHardwareArduino::SetBaudrate(enum baudrates baudrate)
{
    uint32_t baud;

    switch(baudrate)
    {
        case Baud125:  baud = 125000; break;
        case Baud250:  baud = 250000; break;
        case Baud500:  baud = 500000; break;
        case Baud800:  baud = 800000; break;
        case Baud1000: baud = 1000000; break;
        default:       baud = 500000; break;
    }

    ACAN_T4_Settings settings(baud);
    can->begin(settings);
}

void CanHardwareArduino::Send(uint32_t canId, uint32_t data[2], uint8_t len)
{
    CANMessage frame;
    convertToCanFrame(canId, data, len, frame);

    can->tryToSend(frame);
}

void CanHardwareArduino::ConfigureFilters()
{
    // Accept all frames; filtering can be added if needed.
}

void CanHardwareArduino::Poll()
{
    CANMessage frame;
    while (can->receive(frame))
    {
        uint32_t data32[2] = { 0, 0 };
        memcpy(data32, frame.data, frame.len);

        // Update timestamp
        lastRxTimestamp = millis();

        // Call the CanHardware HandleRx which will dispatch to registered callbacks
        HandleRx(frame.id, data32, frame.len);
    }
}

void CanHardwareArduino::convertToCanFrame(uint32_t canId, uint32_t data[2], uint8_t len, CANMessage& frame)
{
    frame.id = canId;
    frame.ext = (canId > 0x7FF);
    frame.rtr = false;
    frame.len = len;
    memcpy(frame.data, data, len);
}
