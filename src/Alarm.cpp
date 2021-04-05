#include "Alarm.hpp"

#include <cstring>

#include "esp_log.h"

static const char* TAG = "Alarm";
static const char* LOW_THRESH_KEY = "low_thresh";
static const char* HI_THRESH_KEY = "hi_thresh";

static constexpr double DEFAULT_LOW_THRESH = -55.;
static constexpr double DEFAULT_HI_THRESH = 125.;

Alarm::Alarm() : _last_alarm(Alarm_T::NONE)
{
    esp_err_t result;
    _nvs_handle = nvs::open_nvs_handle("storage", NVS_READWRITE, &result);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS");
        _nvs_handle = nullptr;
    }
    
    _low_threshold = GetEememValueOrDefault(LOW_THRESH_KEY, DEFAULT_LOW_THRESH);
    _high_threshold = GetEememValueOrDefault(HI_THRESH_KEY, DEFAULT_HI_THRESH);
}

std::pair<double, double> Alarm::GetLowHighThresholds() const 
{
    return std::make_pair(_low_threshold, _high_threshold);
}

void Alarm::SetValue(std::pair<double, double> new_value) 
{
    std::lock_guard<decltype(_critical_section)> lock(_critical_section);

    std::tie(_low_threshold, _high_threshold) = new_value;
    UpdateEememValue(LOW_THRESH_KEY, _low_threshold);
    UpdateEememValue(HI_THRESH_KEY, _high_threshold);
    _last_alarm = Alarm_T::NONE;
}

std::pair<double, double> Alarm::GetValue() const 
{
    std::lock_guard<decltype(_critical_section)> lock(_critical_section);
    return GetLowHighThresholds();
}

Alarm::Alarm_T Alarm::Evaluate(double new_measurement) 
{
    std::lock_guard<decltype(_critical_section)> lock(_critical_section);
    Alarm::Alarm_T new_alarm = Alarm_T::NONE;

    
    if (new_measurement <= _low_threshold) {
        new_alarm = Alarm_T::LOW;
    } else if (new_measurement >= _high_threshold) {
        new_alarm = Alarm_T::HIGH;
    }

    if (new_alarm == Alarm_T::NONE || new_alarm == _last_alarm) {
        return Alarm_T::NONE;
    }


    _last_alarm = new_alarm;
    return new_alarm;
}

double Alarm::GetEememValueOrDefault(const char* key, double default_value) 
{
    if (!_nvs_handle) {
        return default_value;
    }

    uint64_t value_as_int;
    auto err = _nvs_handle->get_item(key, value_as_int);
    switch (err) {
        case ESP_OK: {
            double ret_val;
            memcpy(&ret_val, &value_as_int, sizeof(ret_val));
            return ret_val;
        }
        case ESP_ERR_NVS_NOT_FOUND:
        default :
            ESP_LOGI(TAG, "Value for %s not found in EEMEM, return default %lf.", key, default_value);
            return default_value;
    }
}

void Alarm::UpdateEememValue(const char* name, double new_value) 
{
    if (!_nvs_handle) {
        return;
    }

    uint64_t as_int;
    memcpy(&as_int, &new_value, sizeof(as_int));
    auto err = _nvs_handle->set_item(name, as_int);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving value for %s to EEMEM", name);
    }
}
