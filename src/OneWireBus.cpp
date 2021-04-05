
#include "OneWireBus.hpp"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

static constexpr int WAIT_RETRIES = 100;
static const char* TAG = "OneWire";

OneWireBus::OneWireBus(gpio_num_t pin) : _pin(pin)
{
    gpio_reset_pin(pin);
    Release();
}


bool OneWireBus::Reset() 
{
    auto lock = AcquireLock();
    Hold();
    ets_delay_us(500);
    Release();
    
    ets_delay_us(50);
    auto retry = RetryCounter(WAIT_RETRIES);
    while (gpio_get_level(_pin) && retry.Tick()) {
        ets_delay_us(5);
    }
    
    _is_init = retry.IsValid();
    if (_is_init) {
        while(!gpio_get_level(_pin));
    }
  
    return _is_init;
}


void OneWireBus::WriteBit(bool to_write) 
{
    auto lock = AcquireLock();
    Hold();

    if (!to_write) { // Generate a 0 slot - hold the bus a bit longer
        ets_delay_us(80);   
        Release();
    } else {
        ets_delay_us(1);
        Release();
        ets_delay_us(80);
    }
}

bool OneWireBus::ReadBit() 
{
    auto lock = AcquireLock();

    Hold();
    ets_delay_us(3);
    Release();
    const bool value = gpio_get_level(_pin);
    ets_delay_us(80);

    return value;
}

void OneWireBus::WriteByte(uint8_t to_write) 
{
    auto lock = AcquireLock();

    for (int i = 0; i < 8; i++) {
        WriteBit(((to_write) & (1 << i)));
        ets_delay_us(1);
    }
}

uint8_t OneWireBus::ReadByte() 
{
    uint8_t ret_byte = 0;

    auto lock = AcquireLock();
    for(int i = 0; i< 8; i++) {
        const uint8_t next_bit = ReadBit();
        ret_byte |= (next_bit << i);
        ets_delay_us(1);
    }
    return ret_byte;
}

CriticalSection::Lock OneWireBus::AcquireLock() 
{
    return _critical_section.Acquire();
}

void OneWireBus::Hold() 
{
    gpio_set_direction(_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(_pin, 0);
}

void OneWireBus::Release() 
{
    gpio_set_direction(_pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(_pin, GPIO_PULLUP_ONLY);
}
