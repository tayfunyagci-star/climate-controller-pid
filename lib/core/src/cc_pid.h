// PID / PI / P / ONOFF — PID_DESIGN.md.
// Paralel form, türev ölçüm üzerinden, oransal terimde setpoint ağırlığı b.
// Birimler: Kp %/°C, Ki %/(°C·dk), Kd %·dk/°C; Ts dakikaya çevrilir.
// GPIO/sürücü bağımlılığı yoktur; çıktı yalnız pid_output (%).
#pragma once
#include "cc_types.h"

namespace cc {

struct PidParams {
  PidMode mode = PidMode::PI;
  float kp = 20.0f;
  float ki = 1.0f;
  float kd = 0.0f;
  float b = 1.0f;             // setpoint ağırlığı
  float deadband = 0.1f;      // °C, yalnız integratöre
  float out_min = 0.0f;
  float out_max = 100.0f;
  float onoff_hyst = 0.5f;    // °C
  float onoff_demand = 100.0f;
  float n_filter = 8.0f;      // türev filtresi N
  float kt = -1.0f;           // geri hesaplama kazancı (dk⁻¹); <0 → Ki/Kp
};

struct PidInput {
  float sp = 0;               // SP_eff
  float pv = kNaN;            // PV_f (filtrelenmiş T1)
  float dt_s = 2.0f;          // Ts
  float applied = kNaN;       // aşağı akışta uygulanan talep (önceki çıktıya karşılık); NaN = yok
  bool hold = false;          // geçici aşağı akış gecikmesi (prestart/post-cool): entegrasyon durur
  bool tracking = false;      // MANUAL/FAILSAFE: çıktı track_value'yu izler
  float track_value = 0;
  float out_max = kNaN;       // dinamik üst sınır (max_heat_demand, vent cap); NaN = params.out_max
};

struct PidOutput {
  float output = 0;           // pid_output (kıstırılmış)
  float raw = 0;              // u_raw
  float p = 0, i = 0, d = 0;
  float error = 0;            // SP_eff − PV_f
  Saturation sat = Saturation::NONE;
  bool anti_windup = false;
  bool tracking = false;
};

class Pid {
 public:
  explicit Pid(const PidParams& p = PidParams()) : prm_(p) {}

  const PidParams& params() const { return prm_; }
  // Kazanç / mod değişimi — bumpless: çıktı anında sabit kalacak şekilde I yeniden hesaplanır.
  void setParams(const PidParams& p, float sp, float pv);
  // MANUAL → AUTO: I = u − P − D (sınırlara kıstırılmış)
  void bumplessTo(float u, float sp, float pv);
  // FAILSAFE/kilit dönüşü: temkinli başlangıç, I = 0 (PID_DESIGN §6)
  void resetAfterFault();
  void reset();

  PidOutput step(const PidInput& in);
  const PidOutput& last() const { return last_; }
  float integrator() const { return i_; }

 private:
  float pTerm(float sp, float pv) const { return prm_.kp * (prm_.b * sp - pv); }
  float kt() const;

  PidParams prm_;
  float i_ = 0;
  float d_ = 0;
  float pv_prev_ = kNaN;
  float u_raw_prev_ = 0;
  bool onoff_on_ = false;
  uint32_t hold_ms_ = 0;
  PidOutput last_;
};

}  // namespace cc
