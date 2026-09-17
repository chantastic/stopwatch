#pragma once
#include <Arduino.h>
inline uint32_t testMillis=0;
inline uint32_t millis(){return testMillis;}
inline void esp_fill_random(void *data,size_t length){static uint8_t counter=0;for(size_t i=0;i<length;i++)static_cast<uint8_t*>(data)[i]=counter++;}
struct FakeESP{uint64_t getEfuseMac(){return 0x123456789abcULL;}};
inline FakeESP ESP;
