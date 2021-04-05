#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>

class CriticalSection {
    portMUX_TYPE _critical_section;
    std::atomic<uint32_t> _ref_count;
public:
    CriticalSection();
    class Lock {
        CriticalSection& _outer;
        public:
        Lock(CriticalSection& outer);
        ~Lock();
    };

    Lock Acquire();
};