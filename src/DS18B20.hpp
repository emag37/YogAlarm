#pragma once

#include <array>
#include <atomic>

#include "OneWireBus.hpp"

class DS18B20 {
public:
    static constexpr double INVALID_TEMP = -300.0;
    struct RomCode {
        uint8_t family_code = 0;
        std::array<uint8_t,6> bytes;
        uint8_t crc = 0;

        [[nodiscard]] bool IsValid() const {return family_code != 0;}
    };

    struct Scratchpad {
        bool is_valid = false;
        std::array<uint8_t, 9> data;

        uint8_t& temp_lsb(){ return data[0];}
        uint8_t& temp_msb(){ return data[1];}
        uint8_t& temp_alarm_h(){ return data[2];}
        uint8_t& temp_alarm_l(){ return data[3];}
        uint8_t& config(){ return data[4];}
        uint8_t& crc() {return data[8];}
    };

private:
    OneWireBus _bus;
    RomCode _rom_code;
    
    bool InitRomCode();
    bool SendMatchRom();
    bool SendSkipRom();
    
    static uint8_t GetCrcByte(uint8_t current_crc, uint8_t byte);
    static bool CheckCrc(const Scratchpad& scratchpad);
    static bool CheckCrc(const RomCode& rom_code);
public:
   
    explicit DS18B20(gpio_num_t pin);

    Scratchpad ReadScratchpad();

    // Gets the temperature in degrees Celsius
    double ReadTemperature();
};