// Trend halkaları (WEB_SCADA_UI trend sayfası): 1 sa × 5 s ve 24 sa × 60 s, yalnız RAM (ADR-007).
// Örnek: T1, etkin hedef, nem, ısı talebi, çıkış bitleri (R1, R2, HF, VF). 60 s halkasında bitler dakika
// boyunca VEYA'lanır (kısa çalışma kaybolmaz). Halkalar yığından bir kez ayrılır (≈ 17 KB).
#pragma once
#include <cstddef>
#include <cstdint>

namespace trend {

struct Sample {
  int16_t t10;      // °C × 10; INT16_MIN = geçersiz
  int16_t sp10;
  uint16_t rh10;    // % × 10; 0xFFFF = geçersiz
  uint8_t d2;       // talep % × 2
  uint8_t b;        // bit0 R1, bit1 R2, bit2 HF, bit3 VF
};
constexpr int16_t kNoT = INT16_MIN;
constexpr uint16_t kNoRh = 0xFFFF;

void begin();
void tick(uint32_t now_ms);               // loop görevinden; kendi 5 s temposu
// Pencereye uyan örnekleri eskiden yeniye kopyalar. res_s: 5 veya 60. age_last_s: son örneğin yaşı
size_t read(uint32_t win_s, Sample* out, size_t cap, uint32_t& res_s, uint32_t& age_last_s);
size_t maxSamples();

}  // namespace trend
