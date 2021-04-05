#pragma once

#include <string>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

class WifiStation {
    std::string _ssid;
    std::string _password;
    wifi_config_t _config;
    EventGroupHandle_t _wifi_event_group;
    unsigned int _retry_count = 0;
    bool _is_connected = false;
    esp_event_handler_instance_t _instance_any_id;
    esp_event_handler_instance_t _instance_got_ip;

    static void HandleEvent(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    void HandleConnected();
    bool IncrementRetryIsMax();

public:
    WifiStation(const std::string& ssid, const std::string& password);
    bool Connect();

    [[nodiscard]] bool IsConnected() const { return _is_connected;};

    ~WifiStation();
};