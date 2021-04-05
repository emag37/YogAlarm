#pragma once

#include <chrono>
#include <memory>

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "DataBinding.hpp"


class Alarm : public DataBinding<std::pair<double,double>> {
public:
 enum Alarm_T {
        NONE,
        LOW,
        HIGH
    };
private:
    mutable std::mutex _critical_section;
    double _low_threshold;
    double _high_threshold;
    std::unique_ptr<nvs::NVSHandle> _nvs_handle;
    Alarm_T _last_alarm;

    double GetEememValueOrDefault(const char* name, double default_value);
    void UpdateEememValue(const char* name, double new_value);
public:
   
    Alarm();
    std::pair<double, double> GetLowHighThresholds() const;

    void SetValue(std::pair<double, double> new_value) override;
    std::pair<double, double> GetValue() const override;
    [[nodiscard]] Alarm_T Evaluate(double new_measurement);
    
};