#include "Audio.hpp"

#include "driver/ledc.h"

#include "esp_log.h"

static const char* TAG = "Audio";

Audio::Audio(gpio_num_t pin) : _pin(pin), _is_running(true)
{
    ledc_timer_config_t ledc_timer;
     
    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer.freq_hz = 2000;
    ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.duty = 0;
    ledc_channel.gpio_num = static_cast<int>(_pin);
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.hpoint = 0;
    ledc_channel.timer_sel = LEDC_TIMER_0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    _beep_worker = std::thread([this] {
        this->TaskWorker();
    });
}

Audio::~Audio() 
{
    {
        std::lock_guard<decltype(_beep_queue_lock)> lock(_beep_queue_lock);
        _is_running = false;
    }
    _beep_queue_cv.notify_all();

    _beep_worker.join();
}

void Audio::Play(const Audio::Beep& beep) 
{
    if (beep.IsSilence()){
        std::this_thread::sleep_for(beep.duration);
    } else {
        ESP_ERROR_CHECK(ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, beep.frequency_hz));
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 50));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
        std::this_thread::sleep_for(beep.duration);
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0));
    }
}

void Audio::PlayTune(const std::vector<Audio::Beep>& to_play) 
{
    {
        std::lock_guard<decltype(_beep_queue_lock)> lock(_beep_queue_lock);
        for (const auto& beep : to_play) {
            _pending_beeps.push(beep);
        }
    }
    _beep_queue_cv.notify_all();
}

void Audio::TaskWorker() 
{
    while(true) {
        Audio::Beep next_beep;
        {
            std::unique_lock<decltype(_beep_queue_lock)> lock(_beep_queue_lock);
            
            if (_pending_beeps.empty()) {
                _beep_queue_cv.wait_for(lock, std::chrono::seconds(5));
                if (_pending_beeps.empty()) {
                    continue;
                }
            }
            if (!_is_running) {
                return;
            }
            
            next_beep = std::move(_pending_beeps.front());
            _pending_beeps.pop();
        }

        Play(next_beep);
    }
}
