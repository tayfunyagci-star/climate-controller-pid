// Heating Performance Monitor — SAFETY_DESIGN §6. Kontrolü değiştirmez; gözlemler ve uyarır.
#pragma once
#include "cc_rate.h"

namespace cc {

struct HpmParams {
  bool relay_profile = false;
  uint32_t long_heating_ms = 120u * 60000u;   // max_continuous_heating_min / 2
  static HpmParams fromConfig(bool relay, int32_t max_continuous_min) {
    HpmParams p;
    p.relay_profile = relay;
    p.long_heating_ms = (uint32_t)max_continuous_min * 60000u / 2u;
    return p;
  }
};

struct HpmInput {
  uint32_t dt_ms = 1000;
  float t1 = kNaN;            // PV_f (GOOD değilse NaN)
  float sp = 21;
  float demand = 0;           // heat_demand %
  bool heating = false;       // zincir PREPURGE/ACTIVE
  uint8_t stage = 0;
  bool day_rollover = false;  // gün devri (saat geçerliyse)
  bool time_valid = false;
};

struct HpmOutput {
  float temperature_rate = kNaN;       // °C/sa, 10 dk regresyon
  float duty_1h = 0, duty_24h = 0;     // %
  uint16_t cycles_1h = 0;
  uint32_t heating_minutes_today = 0;
  float stage2_ratio_24h = 0;          // %
  float efficiency_index = kNaN;       // °C / tam güç dakikası (son ısıtma dönemi)
  float efficiency_ref = kNaN, efficiency_recent = kNaN;
  // Kurallar
  bool performance_low = false;        // WARNING
  bool excessive_duty = false;         // INFO
  bool long_heating = false;           // WARNING
  bool frequent_cycling = false;       // WARNING
  bool capacity_degradation = false;   // INFO
  bool ev_cycle_start = false;
};

class HeatingPerformanceMonitor {
 public:
  explicit HeatingPerformanceMonitor(const HpmParams& p = HpmParams()) : p_(p), rate_(10000) {}
  void setParams(const HpmParams& p) { p_ = p; }
  HpmOutput step(const HpmInput& in);
  const HpmOutput& last() const { return last_; }

 private:
  HpmParams p_;
  RateWindow<61> rate_;                 // 10 dk @ 10 s
  // 1 sa: 60 × 1 dk kova; 24 sa: 24 × 1 sa kova
  float min_demand_acc_ = 0; uint32_t min_ms_ = 0;
  float min_heat_acc_ = 0, min_s2_acc_ = 0;
  float m_demand_[60] = {}; uint16_t m_cycles_[60] = {}; uint8_t m_i_ = 0, m_n_ = 0;
  float h_demand_[24] = {}, h_heat_[24] = {}, h_s2_[24] = {}; uint8_t h_i_ = 0, h_n_ = 0;
  float hour_demand_acc_ = 0, hour_heat_acc_ = 0, hour_s2_acc_ = 0; uint32_t hour_ms_ = 0;
  uint16_t cycles_this_min_ = 0;
  bool prev_heating_ = false;
  Timer period_t_;
  float period_t1_start_ = kNaN, period_fullpower_min_ = 0;
  uint32_t today_ms_ = 0;
  // Performans düşük: talep ≥ 80 % süresi ve başlangıç T1
  Timer hi_t_;
  // Günlük verim (21 gün): ilk 14 gün referans, son 7 gün güncel
  float day_dt_ = 0, day_fp_ = 0;
  float days_[21] = {}; uint8_t d_n_ = 0;
  float ref_ = kNaN;
  HpmOutput last_;
};

}  // namespace cc
