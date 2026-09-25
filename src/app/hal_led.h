// Kart üstü durum LED'i (DevKit V1: GPIO2, tek renk). Desenler: sürekli, yavaş/hızlı yanıp sönme, çift çakma.
// Yalnız konsol/servis görevi (loop) sürer; bloklamaz.
#pragma once
#include <cstdint>

namespace hal {
enum class LedPattern : uint8_t { OFF, ON, SLOW, FAST, DOUBLE, HEARTBEAT };
void ledBegin();
void ledService(LedPattern p, uint32_t now_ms);
}
