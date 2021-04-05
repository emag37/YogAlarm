#pragma once
#include <chrono>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

class Audio {
public:
    struct Beep {
        std::chrono::steady_clock::duration duration;
        int frequency_hz;

        [[nodiscard]] bool IsSilence() const {return frequency_hz <= 0;}
    };
private:
    gpio_num_t _pin;
    bool _is_running;
    std::thread _beep_worker;

    std::queue<Beep> _pending_beeps;

    std::condition_variable _beep_queue_cv;
    std::mutex _beep_queue_lock;
    void TaskWorker();
    void Play(const Audio::Beep& beep);
public:

    void PlayTune(const std::vector<Audio::Beep>& to_play);
    Audio(gpio_num_t pin);
    ~Audio();
};