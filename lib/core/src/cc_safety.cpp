#include "cc_safety.h"

namespace cc {

SafetyParams SafetyParams::fromConfig(const Config& c) {
  SafetyParams p;
  p.overtemp_limit = c.cabin_overtemp_limit;
  p.reset_hyst = c.overtemp_reset_hysteresis;
  p.outlet_limit = c.heater_outlet_limit;
  p.max_continuous_ms = minToMs((float)c.max_continuous_heating_min);
  p.unexpected_rise = c.unexpected_rise_c_per_10min;
  p.max_rise = c.max_rise_c_per_10min;
  p.frost_guard = c.frost_guard_temperature;
  p.t2_enabled = c.t2_enabled;
  return p;
}

static bool usable(Quality q) { return q == Quality::GOOD || q == Quality::UNCERTAIN; }

bool SafetyEvaluator::overtempCleared(const SafetyInput& in) const {
  // Koşul temizliği: T1 (ve T2 kaynağıysa T2) limit − histerezisin altında ve ölçüm kullanılabilir
  const bool t1ok = usable(in.t1_q) && isValid(in.t1) && in.t1 < p_.overtemp_limit - p_.reset_hyst;
  if (!ot_src_t2_) return t1ok;
  const bool t2ok = usable(in.t2_q) && isValid(in.t2) && in.t2 < p_.outlet_limit - p_.reset_hyst;
  return t1ok && t2ok;
}

SafetyOutput SafetyEvaluator::step(const SafetyInput& in) {
  SafetyOutput o;
  const uint8_t latchedBefore = latched_;
  const bool t1ok = usable(in.t1_q) && isValid(in.t1);

  // S1 — ortam aşırı sıcaklık (10 s)
  if (t1ok && in.t1 >= p_.overtemp_limit) {
    ot_t_.add(in.dt_ms);
    o.overtemp_condition = true;
    if (ot_t_.atLeast(p_.overtemp_delay_ms) && !(latched_ & SL_OVERTEMP)) {
      latched_ |= SL_OVERTEMP;
      ot_src_t2_ = false;
    }
  } else {
    ot_t_.reset();
  }
  // S2 — hava çıkışı aşırı sıcaklık (3 s)
  const bool t2ok = p_.t2_enabled && usable(in.t2_q) && isValid(in.t2);
  if (t2ok && in.t2 >= p_.outlet_limit) {
    t2ot_t_.add(in.dt_ms);
    o.overtemp_condition = true;
    if (t2ot_t_.atLeast(p_.outlet_delay_ms)) {
      if (!(latched_ & SL_OVERTEMP)) ot_src_t2_ = true;
      latched_ |= SL_OVERTEMP;
    }
  } else {
    t2ot_t_.reset();
  }
  if (latched_ & SL_OVERTEMP) {
    const bool t1Hot = !(t1ok && in.t1 < p_.overtemp_limit - p_.reset_hyst);
    const bool t2Hot = t2ok && in.t2 >= p_.outlet_limit - p_.reset_hyst;
    o.vf_force = !ot_src_t2_ && t1Hot;  // tahliye (I-4)
    o.hf_force = ot_src_t2_ && t2Hot;   // S2: HF ON
  }

  // S3/S4 — sensör yok/bozuk/bayat; otomatik temizlenme: GOOD ≥ 30 s
  const bool hbStale = in.sensor_hb_age_ms > p_.sensor_hb_limit_ms;
  const bool bad = in.t1_q == Quality::BAD || in.t1_q == Quality::MISSING || in.t1_q == Quality::DISABLED;
  const bool stale = in.t1_q == Quality::STALE || hbStale;
  if (bad) sensor_fault_ = true;
  if (stale) sensor_stale_ = true;
  if (in.t1_q == Quality::GOOD && !hbStale) good_t_.add(in.dt_ms);
  else good_t_.reset();
  if ((sensor_fault_ || sensor_stale_) && good_t_.atLeast(p_.sensor_good_clear_ms)) {
    sensor_fault_ = sensor_stale_ = false;
  }
  if (t1ok && in.t1_q == Quality::GOOD) last_good_t1_ = in.t1;
  o.sensor_fault = sensor_fault_;
  o.sensor_stale = sensor_stale_;
  // S19 — donma riski + sensör arızası (ölçümsüz ısıtma yapılmaz; yalnız alarm)
  o.frost_risk = (sensor_fault_ || sensor_stale_) && isValid(last_good_t1_) && last_good_t1_ < p_.frost_guard + 2.0f;

  // S6 — Heater Fan arızası (T2 varsa): HF ON ∧ R ON iken T2 yükselişi > 15 °C/dk
  if (t2ok) t2w_.push(in.t2, in.dt_ms);
  else t2w_.clear();
  if (t2ok && in.hf_on && in.any_r_on && t2w_.full()) {
    const float perMin = t2w_.delta() / t2w_.spanMinutes();
    if (perMin > p_.hf_fault_rise_per_min) latched_ |= SL_FAN_FAULT;
  }

  // S7 — sürekli ısıtma: talep doyumda (≥ etkin üst sınır) kesintisiz süre (F1 yorumu, CHANGELOG)
  if (in.heating_active && in.heat_demand >= in.max_demand - 0.5f) heat_t_.add(in.dt_ms);
  else heat_t_.reset();
  if (heat_t_.atLeast(p_.max_continuous_ms)) latched_ |= SL_HEATING_TIMEOUT;

  // S9 — beklenmeyen sıcaklık artışı (10 dk)
  if (t1ok) t1w_.push(in.t1, in.dt_ms);
  else t1w_.clear();
  if (in.any_r_on) r_off_t_.reset();
  else r_off_t_.add(in.dt_ms);
  if (t1w_.full()) {
    const float rise = t1w_.delta();
    o.rise_10min = rise;
    if (r_off_t_.atLeast(10u * 60000u) && rise > p_.unexpected_rise) o.rise_warning = true;
    if (!r_off_t_.atLeast(10u * 60000u) && rise > p_.max_rise) latched_ |= SL_TEMP_RISE;
  }

  // S13 — konfigürasyon hatası
  o.config_error = in.config_error;

  // S17 — iç hata: heartbeat kaybı, çıkış tutarsızlığı
  const bool outputStall = in.output_hb_age_ms > p_.output_hb_limit_ms;
  if (in.control_hb_age_ms > p_.control_hb_limit_ms || outputStall || in.guard_violation) latched_ |= SL_INTERNAL;
  o.arm_allowed = !outputStall && !(latched_ & SL_INTERNAL);

  // Reset (yetkili): yalnız koşulu temiz kilitler kalkar; INTERNAL yalnız reboot + self-test
  if (in.reset_request) {
    uint8_t clearable = 0;
    if ((latched_ & SL_OVERTEMP) && overtempCleared(in)) clearable |= SL_OVERTEMP;
    if (latched_ & SL_FAN_FAULT) clearable |= SL_FAN_FAULT;
    if ((latched_ & SL_HEATING_TIMEOUT) && !heat_t_.atLeast(p_.max_continuous_ms)) clearable |= SL_HEATING_TIMEOUT;
    if ((latched_ & SL_TEMP_RISE) && !(t1w_.full() && t1w_.delta() > p_.max_rise)) clearable |= SL_TEMP_RISE;
    if (latched_ & SL_OUTPUT) clearable |= SL_OUTPUT;
    if (clearable) {
      latched_ &= (uint8_t)~clearable;
      o.reset_done = true;
      if (clearable & SL_HEATING_TIMEOUT) heat_t_.reset();
    }
    if (latched_ & (uint8_t)~SL_INTERNAL) o.reset_refused = true;
  }

  // Öncelik: INTERNAL › OVERTEMP › FAN › OUTPUT › TEMP_RISE › TIMEOUT › CONFIG › SENSOR
  FailsafeReason r = FailsafeReason::NONE;
  if (latched_ & SL_INTERNAL) r = FailsafeReason::INTERNAL_FAULT;
  else if (latched_ & SL_OVERTEMP) r = FailsafeReason::OVERTEMP;
  else if (latched_ & SL_FAN_FAULT) r = FailsafeReason::HEATER_FAN_FAULT;
  else if (latched_ & SL_OUTPUT) r = FailsafeReason::OUTPUT_FAULT;
  else if (latched_ & SL_TEMP_RISE) r = FailsafeReason::TEMP_RISE;
  else if (latched_ & SL_HEATING_TIMEOUT) r = FailsafeReason::HEATING_TIMEOUT;
  else if (in.config_error) r = FailsafeReason::CONFIG_ERROR;
  else if (sensor_fault_ || sensor_stale_) r = FailsafeReason::SENSOR_FAULT;
  o.reason = r;
  o.lockout = r != FailsafeReason::NONE;
  o.lockout_reason = r == FailsafeReason::NONE      ? Reason::NONE
                     : r == FailsafeReason::OVERTEMP ? Reason::OVERTEMPERATURE_LOCKOUT
                     : r == FailsafeReason::SENSOR_FAULT ? Reason::SENSOR_FAULT
                                                         : Reason::SAFETY_LOCKOUT;
  o.latched = latched_;
  o.ev_trip = (latched_ & ~latchedBefore) != 0 || (o.lockout && !last_.lockout);
  last_ = o;
  return o;
}

}  // namespace cc
