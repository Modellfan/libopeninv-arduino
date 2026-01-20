/*
 * Arduino port of libopeninv CanHardware
 * Wraps the existing CANBus class to provide the CanHardware interface
 */
#include "canhardware_teensy41.h"

CanHardwareTeensy41::CanHardwareTeensy41(Bus bus)
    : CanHardware(), can(ResolveBus(bus))
{
}

CanHardwareTeensy41::CanHardwareTeensy41(Bus bus, enum baudrates baudrate)
    : CanHardware(), can(ResolveBus(bus))
{
    SetBaudrate(baudrate);
}

CanHardwareTeensy41::CanHardwareTeensy41(ACAN_T4* canBus)
    : CanHardware(), can(canBus)
{
}

void CanHardwareTeensy41::SetBaudrate(enum baudrates baudrate)
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
    if (can != nullptr)
        can->begin(settings);
}

void CanHardwareTeensy41::Send(uint32_t canId, uint32_t data[2], uint8_t len)
{
    if (can == nullptr)
        return;

    CANMessage frame;
    convertToCanFrame(canId, data, len, frame);

    can->tryToSend(frame);
}

void CanHardwareTeensy41::ConfigureFilters()
{
    // Accept all frames; filtering can be added if needed.
}

void CanHardwareTeensy41::Poll()
{
    if (can == nullptr)
        return;

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

void CanHardwareTeensy41::convertToCanFrame(uint32_t canId, uint32_t data[2], uint8_t len, CANMessage& frame)
{
    frame.id = canId;
    frame.ext = (canId > 0x7FF);
    frame.rtr = false;
    frame.len = len;
    memcpy(frame.data, data, len);
}

ACAN_T4* CanHardwareTeensy41::ResolveBus(Bus bus)
{
    switch (bus)
    {
        case Can1: return &ACAN_T4::can1;
        case Can2: return &ACAN_T4::can2;
        case Can3: return &ACAN_T4::can3;
        default: return nullptr;
    }
}
