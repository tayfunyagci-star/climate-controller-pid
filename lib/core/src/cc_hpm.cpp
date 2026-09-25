#include "cc_hpm.h"

namespace cc {

static float median(float* v, uint8_t n) {
  for (uint8_t i = 1; i < n; ++i)
    for (uint8_t j = i; j > 0 && v[j - 1] > v[j]; --j) { float t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
  if (!n) return kNaN;
  return (n & 1) ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

HpmOutput HeatingPerformanceMonitor::step(const HpmInput& in) {
  HpmOutput o = last_;
  o.ev_cycle_start = false;
  const float dt = (float)in.dt_ms;

  // Sıcaklık değişim hızı (10 dk doğrusal regresyon)
  rate_.push(in.t1, in.dt_ms);
  o.temperature_rate = rate_.slopePerHour();

  // Isıtma dönemi
  if (in.heating && !prev_heating_) {
    ++cycles_this_min_;
    o.ev_cycle_start = true;
    period_t_.reset();
    period_t1_start_ = in.t1;
    period_fullpower_min_ = 0;
  }
  if (in.heating) {
    period_t_.add(in.dt_ms);
    period_fullpower_min_ += in.demand / 100.0f * (dt / 60000.0f);
    today_ms_ += in.dt_ms;
  }
  if (!in.heating && prev_heating_) {
    if (period_t_.atLeast(10u * 60000u) && isValid(in.t1) && isValid(period_t1_start_) && period_fullpower_min_ > 0) {
      const float dT = in.t1 - period_t1_start_;
      o.efficiency_index = dT / period_fullpower_min_;
      day_dt_ += dT;
      day_fp_ += period_fullpower_min_;
    }
  }
  prev_heating_ = in.heating;
  o.long_heating = in.heating && period_t_.atLeast(p_.long_heating_ms);

  // Dakika / saat kovaları
  min_demand_acc_ += in.demand * dt;
  if (in.heating) min_heat_acc_ += dt;
  if (in.heating && in.stage == 2) min_s2_acc_ += dt;
  min_ms_ += in.dt_ms;
  if (min_ms_ >= 60000u) {
    m_demand_[m_i_] = min_demand_acc_ / (float)min_ms_;
    m_cycles_[m_i_] = cycles_this_min_;
    m_i_ = (uint8_t)((m_i_ + 1) % 60);
    if (m_n_ < 60) ++m_n_;
    hour_demand_acc_ += min_demand_acc_;
    hour_heat_acc_ += min_heat_acc_;
    hour_s2_acc_ += min_s2_acc_;
    hour_ms_ += min_ms_;
    min_demand_acc_ = min_heat_acc_ = min_s2_acc_ = 0;
    min_ms_ = 0;
    cycles_this_min_ = 0;
    if (hour_ms_ >= 3600000u) {
      h_demand_[h_i_] = hour_demand_acc_ / (float)hour_ms_;
      h_heat_[h_i_] = hour_heat_acc_;
      h_s2_[h_i_] = hour_s2_acc_;
      h_i_ = (uint8_t)((h_i_ + 1) % 24);
      if (h_n_ < 24) ++h_n_;
      hour_demand_acc_ = hour_heat_acc_ = hour_s2_acc_ = 0;
      hour_ms_ = 0;
    }
  }
  float s = 0;
  uint32_t cyc = cycles_this_min_;
  for (uint8_t i = 0; i < m_n_; ++i) { s += m_demand_[i]; cyc += m_cycles_[i]; }
  o.duty_1h = m_n_ ? s / m_n_ : 0;
  o.cycles_1h = (uint16_t)cyc;
  float sd = 0, sh = 0, s2 = 0;
  for (uint8_t i = 0; i < h_n_; ++i) { sd += h_demand_[i]; sh += h_heat_[i]; s2 += h_s2_[i]; }
  o.duty_24h = h_n_ ? sd / h_n_ : o.duty_1h;
  o.stage2_ratio_24h = sh > 0 ? 100.0f * s2 / sh : 0;
  o.heating_minutes_today = today_ms_ / 60000u;

  // Gün devri: günlük verim kaydı
  if (in.day_rollover) {
    if (day_fp_ > 0) {
      if (d_n_ == 21) {
        for (uint8_t i = 14; i < 20; ++i) days_[i] = days_[i + 1];  // ilk 14 gün referans olarak sabit
        days_[20] = day_dt_ / day_fp_;
      } else {
        days_[d_n_++] = day_dt_ / day_fp_;
      }
    }
    day_dt_ = day_fp_ = 0;
    today_ms_ = 0;
    o.heating_minutes_today = 0;
  }
  if (!isValid(ref_) && d_n_ >= 14) {
    float tmp[14];
    for (uint8_t i = 0; i < 14; ++i) tmp[i] = days_[i];
    ref_ = median(tmp, 14);
  }
  o.efficiency_ref = ref_;
  if (d_n_ >= 21) {
    float tmp[7];
    for (uint8_t i = 0; i < 7; ++i) tmp[i] = days_[d_n_ - 7 + i];
    o.efficiency_recent = median(tmp, 7);
  }
  o.capacity_degradation = in.time_valid && isValid(ref_) && ref_ > 0 && isValid(o.efficiency_recent) &&
                           o.efficiency_recent < 0.7f * ref_;

  // Kurallar
  if (in.heating && in.demand >= 80.0f) hi_t_.add(in.dt_ms);
  else hi_t_.reset();
  o.performance_low = hi_t_.atLeast(10u * 60000u) && rate_.full() && rate_.delta() < 0.2f && isValid(in.t1) &&
                      in.t1 < in.sp - 1.0f;
  o.excessive_duty = h_n_ >= 24 && o.duty_24h > 85.0f;
  o.frequent_cycling = o.cycles_1h > (p_.relay_profile ? 3 : 6);
  last_ = o;
  return o;
}

}  // namespace cc
