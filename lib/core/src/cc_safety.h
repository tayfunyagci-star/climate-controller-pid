// SafetyManager değerlendiricisi — SAFETY_DESIGN §3 (S1–S19), STATE_MACHINE §4.
// Kontrolden bağımsız: kendi limit kopyası, medyan-3 (EMA'sız) ölçüm. Yalnız güvenli yönde zorlar.
#pragma once
#include "cc_config.h"
#include "cc_rate.h"

namespace cc {

enum SafetyLatch : uint8_t {
  SL_OVERTEMP = 1u << 0,
  SL_FAN_FAULT = 1u << 1,
  SL_HEATING_TIMEOUT = 1u << 2,
  SL_TEMP_RISE = 1u << 3,
  SL_INTERNAL = 1u << 4,
  SL_OUTPUT = 1u << 5,
};

struct SafetyParams {
  float overtemp_limit = 40, reset_hyst = 3, outlet_limit = 80;
  uint32_t overtemp_delay_ms = 10000, outlet_delay_ms = 3000;
  uint32_t max_continuous_ms = 240u * 60000u;
  float unexpected_rise = 1.5f, max_rise = 5.0f;   // °C / 10 dk
  float frost_guard = 4.0f;
  float hf_fault_rise_per_min = 15.0f;
  bool t2_enabled = false;
  uint32_t sensor_good_clear_ms = 30000;
  uint32_t control_hb_limit_ms = 6000, sensor_hb_limit_ms = 5000, output_hb_limit_ms = 1000;
  static SafetyParams fromConfig(const Config& c);
};

struct SafetyInput {
  uint32_t dt_ms = 250;
  float t1 = kNaN;                  // medyan-3, EMA'sız
  Quality t1_q = Quality::MISSING;
  float t2 = kNaN;
  Quality t2_q = Quality::DISABLED;
  bool any_r_on = false;            // effective
  bool hf_on = false;
  bool heating_active = false;      // zincir PREPURGE/ACTIVE
  float heat_demand = 0;
  float max_demand = 100;
  bool config_error = false;
  uint32_t control_hb_age_ms = 0, sensor_hb_age_ms = 0, output_hb_age_ms = 0;
  bool guard_violation = false;     // OutputGuard / iç tutarlılık (R ON ∧ HF OFF gözlemi)
  bool reset_request = false;       // yetkili kilit sıfırlama
};

struct SafetyOutput {
  bool lockout = false;             // rezistanslar kilitli (FAILSAFE)
  FailsafeReason reason = FailsafeReason::NONE;
  Reason lockout_reason = Reason::NONE;  // InterlockEngine I-1 nedeni
  bool arm_allowed = true;          // OutputTask heartbeat kaybında HEATER_ARM=0
  bool vf_force = false;            // I-4
  bool hf_force = false;            // S2
  uint8_t latched = 0;              // SafetyLatch maskesi
  // Alarm koşulları
  bool overtemp_condition = false;  // T1/T2 limit üstünde (ham koşul)
  bool sensor_fault = false;        // S3
  bool sensor_stale = false;        // S4
  bool rise_warning = false;        // S9 (R OFF)
  bool frost_risk = false;          // S19
  bool config_error = false;        // S13
  bool reset_done = false;          // bu adımda en az bir kilit kalktı
  bool reset_refused = false;       // reset istendi ama koşul sürüyor
  float rise_10min = kNaN;          // T1 10 dk değişimi
  bool ev_trip = false;
};

class SafetyEvaluator {
 public:
  explicit SafetyEvaluator(const SafetyParams& p = SafetyParams()) : p_(p), t1w_(10000), t2w_(5000) {}
  void setParams(const SafetyParams& p) { p_ = p; }  // yalnız doğrulanmış tam config commit'inde
  // Kalıcı kilitlerin boot'ta geri yüklenmesi (INTERNAL hariç: başarılı self-test ile kalkar)
  void restoreLatched(uint8_t mask) { latched_ |= (uint8_t)(mask & ~SL_INTERNAL); }
  SafetyOutput step(const SafetyInput& in);
  const SafetyOutput& last() const { return last_; }
  uint8_t latched() const { return latched_; }

 private:
  bool overtempCleared(const SafetyInput& in) const;
  SafetyParams p_;
  uint8_t latched_ = 0;
  bool ot_src_t2_ = false;
  Timer ot_t_, t2ot_t_, heat_t_, r_off_t_, good_t_;
  bool sensor_fault_ = false, sensor_stale_ = false;
  float last_good_t1_ = kNaN;
  RateWindow<61> t1w_;  // 10 dk @ 10 s
  RateWindow<13> t2w_;  // 60 s @ 5 s
  SafetyOutput last_;
};

}  // namespace cc
