#include "DS18B20.hpp"

#include <array>
#include <cstring>
#include <numeric>

#include "esp_log.h"
#include "freertos/task.h"

const char* TAG = "DS18B20";

static constexpr double DEG_C_PER_BIT_12 = 0.0625;

namespace {
    void PrintRomCode(const DS18B20::RomCode& code) {
        ESP_LOGI(TAG, "Family code: %x", code.family_code);
        ESP_LOGI(TAG, "Serial Number: %x %x %x %x %x %x  end", code.bytes[0], code.bytes[1], code.bytes[2], code.bytes[3], code.bytes[4], code.bytes[5]);
        ESP_LOGI(TAG, "CRC: %x", code.crc);
    }
}

bool DS18B20::InitRomCode() 
{

    if (_rom_code.IsValid()) {
        return true;
    }

    auto lock = _bus.AcquireLock();

    if (_bus.Reset()) {
        RomCode new_code;

        ESP_LOGI(TAG, "Reading ROM Code...");
        _bus.WriteByte(0x33);

        new_code.family_code = _bus.ReadByte();

        for (int i = 0; i < 6; i++) {
            new_code.bytes[i] = _bus.ReadByte();
        }
        new_code.crc = _bus.ReadByte();
        
        PrintRomCode(new_code);

        if (!CheckCrc(new_code)){
            ESP_LOGE(TAG, "Failed to match CRC for ROM code!");
            return false;
        }
        _rom_code = std::move(new_code);
        return _rom_code.IsValid();
    }
    return false;
}

bool DS18B20::SendSkipRom() {
    auto lock = _bus.AcquireLock();
    if (!_bus.Reset()) {
        return false;
    }
    _bus.WriteByte(0xCC);
    return true;
}

uint8_t DS18B20::GetCrcByte(uint8_t current_crc, uint8_t byte) 
{
    const std::array<uint8_t,256> crc_table = 
    {0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53};

    return crc_table[current_crc ^ byte];
}

bool DS18B20::CheckCrc(const Scratchpad& scratchpad) 
{
    const auto crc_calc = std::accumulate(scratchpad.data.begin(), std::prev(scratchpad.data.end()), 0, [&](const uint8_t accum, const uint8_t next_byte) { return GetCrcByte(accum, next_byte);});

    return crc_calc == *scratchpad.data.rbegin();
}

bool DS18B20::CheckCrc(const RomCode& rom_code) 
{
    const auto crc_calc =  std::accumulate(rom_code.bytes.begin(), rom_code.bytes.end(), GetCrcByte(0, rom_code.family_code), [&](const uint8_t accum, const uint8_t next_byte) { return GetCrcByte(accum, next_byte);});
    return crc_calc == rom_code.crc;
}

bool DS18B20::SendMatchRom() 
{
    auto lock = _bus.AcquireLock();
    if (!_bus.Reset()) {
        return false;
    }
    _bus.WriteByte(0x55);

    _bus.WriteByte(_rom_code.family_code);
    for (auto byte : _rom_code.bytes) {
        _bus.WriteByte(byte);
    }
    _bus.WriteByte(_rom_code.crc);
    return true;
}


DS18B20::DS18B20(gpio_num_t pin) : _bus(pin)
{
    InitRomCode();
}

DS18B20::Scratchpad DS18B20::ReadScratchpad() 
{
   DS18B20::Scratchpad scratchpad;

   auto lock = _bus.AcquireLock();
   if(!SendMatchRom()) {
       return scratchpad;
   }
  
   _bus.WriteByte(0xBE);

   for (int i = 0; i < scratchpad.data.size(); i++) {
       scratchpad.data[i] = _bus.ReadByte();
   }
   if (!CheckCrc(scratchpad)) {
       return scratchpad;
   }
   
   scratchpad.is_valid = true;
   return scratchpad;
}

double DS18B20::ReadTemperature() 
{
    if (!InitRomCode()) {
        ESP_LOGE(TAG, "Cannot get ROM code - cannot read temperature");
        return INVALID_TEMP;
    }
    
    if (!SendMatchRom()) {
        ESP_LOGE(TAG, "Could not send match ROM!");
        return INVALID_TEMP;
    }

    _bus.WriteByte(0x44);

    while(!_bus.ReadBit()) {
        // Conversion takes 750ms max at 12-bit resolution, let other tasks run in the meantime
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    auto scratchpad = ReadScratchpad();

    if (!scratchpad.is_valid) {
        return INVALID_TEMP;
    }
    double temp = static_cast<double>(((static_cast<int>(scratchpad.temp_msb())  << 8 ) | static_cast<int>(scratchpad.temp_lsb()))) * DEG_C_PER_BIT_12;

    return temp;
}


