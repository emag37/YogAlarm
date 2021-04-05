#include <stdio.h>
#include <memory>

#include "wifi_credentials_do_not_commit.hpp"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#error "WIFI_SSID or WIFI_PASSWORD must be defined!"
#endif

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "WebUI.hpp"

#include "DS18B20.hpp"
#include "mDns.hpp"
#include "DataBinding.hpp"
#include "WifiStation.hpp"
#include "Audio.hpp"
#include "Alarm.hpp"

static const char* TAG = "main";

#define AUDIO_GPIO_PIN GPIO_NUM_21
#define TEMP_SENSOR_GPIO_PIN GPIO_NUM_12

extern "C" {
	void app_main(void);
}

struct TemperatureTaskData {
  std::shared_ptr<DS18B20> temp_sensor;
  std::shared_ptr<DataBinding<double>> data_source;
};

void TemperatureTaskWorker(void * param)
{
  std::unique_ptr<TemperatureTaskData> data = std::unique_ptr<TemperatureTaskData>(static_cast<TemperatureTaskData*>(param));

  while (true) {
    auto temp = data->temp_sensor->ReadTemperature();
    if (temp != DS18B20::INVALID_TEMP) {
      data->data_source->SetValue(temp);
    }
  }
}

void app_main(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Init the speaker at low level so if something happens we don't fry it
  gpio_set_direction(AUDIO_GPIO_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(AUDIO_GPIO_PIN, 0);

  WifiStation station(WIFI_SSID, WIFI_PASSWORD);
  mDns::AddHttpService("yogalarm", "Yogurt Alarm");

  auto temp_sensor = std::make_shared<DS18B20>(TEMP_SENSOR_GPIO_PIN);
  auto temperature_source = std::make_shared<DataSourceSingleValue<double>>(DS18B20::INVALID_TEMP);
  auto alarm = std::make_shared<Alarm>();

  Audio audio(AUDIO_GPIO_PIN);


  WebUI _web_ui(temperature_source, alarm);

  TaskHandle_t temperature_task;
  xTaskCreate(TemperatureTaskWorker, "Temperature Task", 3072, new TemperatureTaskData {temp_sensor, temperature_source}, tskIDLE_PRIORITY + 3, &temperature_task);
 

  while(true) {
    if (!station.IsConnected()) {
      ESP_LOGI(TAG, "WiFi not connected, reconnect");
      station.Connect();
    }

    switch (alarm->Evaluate(temperature_source->GetValue())){ 
      case Alarm::Alarm_T::HIGH:
        audio.PlayTune({
        Audio::Beep{std::chrono::seconds(2), 440},
        Audio::Beep{std::chrono::seconds(2), -1},
        Audio::Beep{std::chrono::seconds(2), 440},
        Audio::Beep{std::chrono::seconds(2), -1},
        Audio::Beep{std::chrono::seconds(2), 440}
        });
        break;
      case Alarm::Alarm_T::LOW:
        audio.PlayTune({
        Audio::Beep{std::chrono::seconds(2), 392},
        Audio::Beep{std::chrono::seconds(2), -1},
        Audio::Beep{std::chrono::seconds(2), 392},
        Audio::Beep{std::chrono::seconds(2), -1},
        Audio::Beep{std::chrono::seconds(2), 392}
        });
        break;
        default:
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}