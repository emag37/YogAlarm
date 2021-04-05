
#include "CriticalSection.hpp"

CriticalSection::CriticalSection() : _critical_section(portMUX_INITIALIZER_UNLOCKED)
{
    
}

CriticalSection::Lock CriticalSection::Acquire() 
{
    return CriticalSection::Lock(*this);
}

CriticalSection::Lock::Lock(CriticalSection& outer) : _outer(outer) {
    if (outer._ref_count.fetch_add(1) == 0) {
        portENTER_CRITICAL(&_outer._critical_section);
    }
}

CriticalSection::Lock::~Lock() {
    if (_outer._ref_count.fetch_sub(1) == 1) {
        portEXIT_CRITICAL(&_outer._critical_section);
    }
}