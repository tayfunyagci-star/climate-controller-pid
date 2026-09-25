#include "cc_dht.h"

namespace cc {

DhtFrame decodeDht22(const DhtPulse* p, uint16_t n) {
  DhtFrame f;
  // Kayıt sonundaki sıfır süreli girdileri at
  while (n > 0 && p[n - 1].us == 0) --n;
  // Sondan geriye 40 HIGH darbe topla (son darbe genelde bitiş LOW'udur)
  uint16_t idx[40];
  uint8_t found = 0;
  uint16_t highs = 0;
  for (uint16_t i = 0; i < n; ++i) if (p[i].level) ++highs;
  if (highs == 0) { f.status = DrvStatus::TIMEOUT; return f; }
  for (int32_t i = (int32_t)n - 1; i >= 0 && found < 40; --i) {
    if (p[i].level) idx[39 - found++] = (uint16_t)i;
  }
  if (found < 40) { f.status = DrvStatus::BUS_ERROR; return f; }
  for (uint8_t b = 0; b < 40; ++b) {
    const uint16_t i = idx[b];
    const uint16_t us = p[i].us;
    uint8_t bit;
    if (us >= kDhtZeroMin && us <= kDhtZeroMax) bit = 0;
    else if (us >= kDhtOneMin && us <= kDhtOneMax) bit = 1;
    else { f.status = DrvStatus::BUS_ERROR; return f; }
    // Her veri bitinden önce ≈ 50 µs LOW gelir
    if (i == 0 || p[i - 1].level != 0 || p[i - 1].us < kDhtLowMin || p[i - 1].us > kDhtLowMax) {
      f.status = DrvStatus::BUS_ERROR;
      return f;
    }
    f.bytes[b / 8] = (uint8_t)((f.bytes[b / 8] << 1) | bit);
  }
  const uint8_t sum = (uint8_t)(f.bytes[0] + f.bytes[1] + f.bytes[2] + f.bytes[3]);
  if (sum != f.bytes[4]) { f.status = DrvStatus::CRC_ERROR; return f; }
  const float rh = (float)(((uint16_t)f.bytes[0] << 8) | f.bytes[1]) / 10.0f;
  float t = (float)((((uint16_t)f.bytes[2] & 0x7Fu) << 8) | f.bytes[3]) / 10.0f;
  if (f.bytes[2] & 0x80u) t = -t;
  if (!(rh >= 0.0f && rh <= 100.0f) || !(t >= -40.0f && t <= 80.0f)) { f.status = DrvStatus::BUS_ERROR; return f; }
  f.humidity = rh;
  f.temperature = t;
  f.status = DrvStatus::OK;
  return f;
}

uint16_t encodeDht22(const uint8_t bytes[5], DhtPulse* out, uint16_t cap) {
  uint16_t k = 0;
  auto put = [&](uint8_t lv, uint16_t us) { if (k < cap) out[k++] = DhtPulse{lv, us}; };
  put(0, 80);
  put(1, 80);
  for (uint8_t b = 0; b < 40; ++b) {
    const bool one = (bytes[b / 8] >> (7 - (b % 8))) & 1u;
    put(0, 50);
    put(1, one ? 70 : 27);
  }
  put(0, 50);
  put(1, 0);
  return k;
}

}  // namespace cc
