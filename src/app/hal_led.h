// Dahili WS2812 durum LED'i — RMT TX (legacy sürücü). Yalnız konsol/servis görevi yazar.
#pragma once
#include <cstdint>

namespace hal {
bool ledBegin();
void ledSet(uint8_t r, uint8_t g, uint8_t b);  // bloklamaz (DMA'sız RMT, 24 bit ≈ 30 µs)
}
