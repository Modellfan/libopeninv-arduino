/*
 * Arduino port of libopeninv CanHardware
 * Wraps the existing CANBus class to provide the CanHardware interface
 */
#ifndef CANHARDWARE_TEENSY41_H
#define CANHARDWARE_TEENSY41_H

#include <Arduino.h>
#include <ACAN_T4.h>
#include "canhardware.h"

class CanHardwareTeensy41 : public CanHardware
{
public:
    enum Bus
    {
        Can1 = 1,
        Can2 = 2,
        Can3 = 3
    };

    explicit CanHardwareTeensy41(Bus bus);
    CanHardwareTeensy41(Bus bus, enum baudrates baudrate);
    explicit CanHardwareTeensy41(ACAN_T4* canBus);

    void SetBaudrate(enum baudrates baudrate) override;
    void Send(uint32_t canId, uint32_t data[2], uint8_t len) override;
    void Poll();

protected:
    void ConfigureFilters() override;

private:
    ACAN_T4* can;

    // Convert between data formats
    void convertToCanFrame(uint32_t canId, uint32_t data[2], uint8_t len, CANMessage& frame);
    static ACAN_T4* ResolveBus(Bus bus);
};

#endif // CANHARDWARE_TEENSY41_H
