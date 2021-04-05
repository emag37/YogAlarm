#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "CriticalSection.hpp"

class OneWireBus {
    gpio_num_t _pin;
    bool _is_init = false;
    CriticalSection _critical_section;

    void Release(); // Release the bus - open drain with pull-up
    void Hold(); // Hold the bus low

    class RetryCounter {
        int _retries = 0;
        const int _max_retries;
    public:
        RetryCounter(int max_retries) : _max_retries(max_retries) {

        }
        bool Tick() {
            ++_retries;
            return _retries <= _max_retries; 
        }

        void Reset() {
            _retries = 0;
        }

        bool IsValid() {
            return _retries <= _max_retries;
        }
    };
public:
    explicit OneWireBus(gpio_num_t pin);

    // Sends a reset pulse and waits for a presence pulse
    bool Reset();

    void WriteBit(bool to_write);
    bool ReadBit();

    //Read/Writes are LSB first
    void WriteByte(uint8_t to_write);
    uint8_t ReadByte();
    CriticalSection::Lock AcquireLock();
};