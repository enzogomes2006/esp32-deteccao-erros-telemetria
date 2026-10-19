#pragma once
#include <cstdint>
extern int64_t fakeClock;
inline int64_t esp_timer_get_time(){return fakeClock;}
