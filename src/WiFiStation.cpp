#include "WiFiStation.hpp"

#include <cstring>

#include "esp_log.h"


constexpr EventBits_t WIFI_CONNECTED_BIT = (1 << 0);
constexpr EventBits_t WIFI_FAIL_BIT      = (1 << 1);
constexpr unsigned int MAX_RETRY_COUNT = 5;


static const char* TAG = "WiFiStation";

void WifiStation::HandleEvent(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Got wifi event: %d", event_id);
    WifiStation* station = static_cast<WifiStation*>(arg);

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG,"connect to the AP fail");
        if (!station->IncrementRetryIsMax()) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        station->HandleConnected();
    }
}

WifiStation::WifiStation(const std::string& ssid, const std::string& password) : _ssid(ssid), _password(password), _wifi_event_group(xEventGroupCreate()) {
    if(_wifi_event_group == nullptr) {
        ESP_LOGE(TAG, "Error allocating event group");
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    _config = {};
    strcpy(reinterpret_cast<char *>(_config.sta.ssid), _ssid.c_str());
    strcpy(reinterpret_cast<char *>(_config.sta.password), _password.c_str());
    _config.sta.pmf_cfg.capable = true;
    _config.sta.pmf_cfg.required = false;
    _config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &WifiStation::HandleEvent,
                                                        this,
                                                        &_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &WifiStation::HandleEvent,
                                                        this,
                                                        &_instance_got_ip));
   
}

bool WifiStation::IncrementRetryIsMax() {
    if (++_retry_count < MAX_RETRY_COUNT) {
        return false;
    }
    xEventGroupSetBits(_wifi_event_group, WIFI_FAIL_BIT);
    return true;
}

void WifiStation::HandleConnected() {
    ESP_LOGI(TAG, "WiFi Connected");
    xEventGroupSetBits(_wifi_event_group, WIFI_CONNECTED_BIT);
}

bool WifiStation::Connect() {
    _is_connected = false;
    _retry_count = 0;
    xEventGroupClearBits(_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            20000 / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 _ssid.c_str());
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                 _ssid.c_str());
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    _is_connected = bits & WIFI_CONNECTED_BIT;
    return _is_connected;
}

WifiStation::~WifiStation() {
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, _instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _instance_any_id));
    vEventGroupDelete(_wifi_event_group);
}