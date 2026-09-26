// WS2812B şerit sürücüsü (RMT TX, IDF 4.4 legacy sürücü; 800 kHz, GRB). Bloklamaz: önceki çerçeve
// bitmediyse yeni çerçeve atlanır. Yalnız konsol/servis görevi (loop) çağırır.
#pragma once
#include <cstddef>
#include <cstdint>

namespace hal {
bool stripBegin(size_t leds);
bool stripShow(const uint8_t* grb, size_t leds);   // leds × 3 bayt, G,R,B sırası
bool stripOk();
}
