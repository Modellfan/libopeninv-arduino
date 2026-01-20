/*
 * Arduino port of libopeninv CanHardware
 * Wraps the existing CANBus class to provide the CanHardware interface
 */
#ifndef CANHARDWARE_ARDUINO_H
#define CANHARDWARE_ARDUINO_H

#include <Arduino.h>
#include <ACAN_T4.h>
#include "canhardware.h"

class CanHardwareArduino : public CanHardware
{
public:
    explicit CanHardwareArduino(ACAN_T4* canBus);

    void SetBaudrate(enum baudrates baudrate) override;
    void Send(uint32_t canId, uint32_t data[2], uint8_t len) override;
    void Poll();

protected:
    void ConfigureFilters() override;

private:
    ACAN_T4* can;

    // Convert between data formats
    void convertToCanFrame(uint32_t canId, uint32_t data[2], uint8_t len, CANMessage& frame);
};

#endif // CANHARDWARE_ARDUINO_H
