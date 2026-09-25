// DHT22 / AM2302 çerçeve çözücü — platformdan bağımsız (RMT yakalaması HAL'de, çözüm burada).
// Girdi: hattın yakalanmış darbe dizisi (seviye + süre µs). Çıktı: T/RH ve sürücü durumu.
// Protokol: sensör yanıtı 80 µs LOW + 80 µs HIGH, ardından 40 bit; her bit ≈ 50 µs LOW +
// HIGH süresi (26–28 µs = 0, ≈ 70 µs = 1). Bayt sırası RH_H RH_L T_H T_L CHK; T_H bit7 = işaret.
#pragma once
#include <cstdint>
#include "cc_sensor.h"

namespace cc {

struct DhtPulse {
  uint8_t level;   // 0 = LOW, 1 = HIGH
  uint16_t us;     // süre (µs); 0 = kayıt sonu (boşta)
};

struct DhtFrame {
  DrvStatus status = DrvStatus::NOT_READY;
  float temperature = kNaN;
  float humidity = kNaN;
  uint8_t bytes[5] = {0, 0, 0, 0, 0};
};

// Eşikler (µs). Bit HIGH ≤ kDhtZeroMax → 0, ≥ kDhtOneMin → 1; aradaki belirsiz süre BUS_ERROR.
constexpr uint16_t kDhtZeroMin = 8, kDhtZeroMax = 45, kDhtOneMin = 55, kDhtOneMax = 100;
constexpr uint16_t kDhtLowMin = 25, kDhtLowMax = 100;

// Darbe dizisinden son 40 veri bitini çözer.
//  - HIGH darbe sayısı < 40 → yanıt yok/eksik: TIMEOUT (hiç HIGH yoksa) veya BUS_ERROR
//  - Bit süresi aralık dışı → BUS_ERROR;  checksum tutmuyor → CRC_ERROR
//  - Fiziksel aralık dışı (T −40…80 °C, RH 0…100 %) → BUS_ERROR
DhtFrame decodeDht22(const DhtPulse* p, uint16_t n);

// Test ve tanı için: 5 baytı darbe dizisine çevirir (yanıt öneki dahil). out ≥ 84 eleman.
uint16_t encodeDht22(const uint8_t bytes[5], DhtPulse* out, uint16_t cap);

}  // namespace cc
