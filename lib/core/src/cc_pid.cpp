#include "cc_pid.h"

namespace cc {

namespace {
constexpr uint32_t kHoldPreserveMs = 5u * 60u * 1000u;  // 5 dk'dan kısa kesintide I korunur
}

float Pid::kt() const {
  if (prm_.kt >= 0) return prm_.kt;
  return prm_.kp > 0 ? prm_.ki / prm_.kp : 0.0f;
}

void Pid::reset() {
  i_ = 0;
  d_ = 0;
  pv_prev_ = kNaN;
  u_raw_prev_ = 0;
  onoff_on_ = false;
  hold_ms_ = 0;
  last_ = PidOutput();
}

void Pid::resetAfterFault() {
  float pv = pv_prev_;
  reset();
  pv_prev_ = pv;
}

void Pid::setParams(const PidParams& p, float sp, float pv) {
  const float u_prev = last_.output;
  if (p.mode == PidMode::PID && prm_.mode == PidMode::PID && prm_.kd > 0 && p.kd > 0)
    d_ *= p.kd / prm_.kd;
  else
    d_ = 0;
  prm_ = p;
  if (p.mode == PidMode::ONOFF) {
    onoff_on_ = u_prev > 0;
  } else if (isValid(pv)) {
    i_ = clampf(u_prev - pTerm(sp, pv) - d_, -prm_.out_max, prm_.out_max);
  }
  u_raw_prev_ = u_prev;
}

void Pid::bumplessTo(float u, float sp, float pv) {
  u = clampf(u, prm_.out_min, prm_.out_max);
  if (isValid(pv)) i_ = clampf(u - pTerm(sp, pv) - d_, -prm_.out_max, prm_.out_max);
  onoff_on_ = u > 0;
  u_raw_prev_ = u;
  last_.output = u;
}

PidOutput Pid::step(const PidInput& in) {
  PidOutput o;
  const float outMax = isValid(in.out_max) ? (in.out_max < prm_.out_max ? in.out_max : prm_.out_max) : prm_.out_max;
  const float outMin = prm_.out_min;
  const float ts_min = (in.dt_s > 0 ? in.dt_s : 0.001f) / 60.0f;

  if (!isValid(in.pv)) {
    // Geçersiz ölçüm: çıktı 0, integratör dondurulur (Safety ayrıca trip eder).
    o.output = 0;
    o.raw = 0;
    o.i = i_;
    o.error = kNaN;
    o.tracking = in.tracking;
    last_ = o;
    u_raw_prev_ = 0;
    return o;
  }

  const float e = in.sp - in.pv;
  o.error = e;
  const float P = pTerm(in.sp, in.pv);

  // Türev (yalnız PID modu), ölçüm üzerinden, birinci derece filtre
  if (prm_.mode == PidMode::PID && prm_.kd > 0 && prm_.kp > 0 && isValid(pv_prev_)) {
    const float tf = prm_.kd / (prm_.kp * prm_.n_filter);
    const float a = tf / (tf + ts_min);
    d_ = a * d_ - (1.0f - a) * prm_.kd * (in.pv - pv_prev_) / ts_min;
  } else if (prm_.mode != PidMode::PID) {
    d_ = 0;
  }
  pv_prev_ = in.pv;

  if (prm_.mode == PidMode::ONOFF) {
    const float h = prm_.onoff_hyst * 0.5f;
    if (in.pv <= in.sp - h) onoff_on_ = true;
    else if (in.pv >= in.sp + h) onoff_on_ = false;
    float u = onoff_on_ ? prm_.onoff_demand : 0.0f;
    o.raw = u;
    o.output = clampf(u, outMin, outMax);
    o.sat = u > outMax ? Saturation::HIGH : Saturation::NONE;
    o.p = o.output;
    o.tracking = in.tracking;
    if (in.tracking) {
      o.output = clampf(in.track_value, outMin, outMax);
      onoff_on_ = o.output > 0;
    }
    u_raw_prev_ = o.output;
    last_ = o;
    return o;
  }

  if (in.tracking) {
    const float u = clampf(in.track_value, outMin, outMax);
    i_ = clampf(u - P - d_, -prm_.out_max, prm_.out_max);
    hold_ms_ = 0;
    o.p = P;
    o.i = i_;
    o.d = d_;
    o.raw = P + i_ + d_;
    o.output = u;
    o.tracking = true;
    u_raw_prev_ = u;
    last_ = o;
    return o;
  }

  // Geçici aşağı akış gecikmesi: integratör korunur (≤ 5 dk)
  bool frozen = false;
  if (in.hold) {
    const uint32_t dt_ms = (uint32_t)(in.dt_s * 1000.0f);
    hold_ms_ = (UINT32_MAX - hold_ms_ < dt_ms) ? UINT32_MAX : hold_ms_ + dt_ms;
    frozen = hold_ms_ < kHoldPreserveMs;
  } else {
    hold_ms_ = 0;
  }

  // Geri hesaplama: uygulanan talep ≠ önceki ham çıktı
  if (!frozen && isValid(in.applied) && prm_.mode != PidMode::P)
    i_ += kt() * (in.applied - u_raw_prev_) * ts_min;

  // Koşullu entegrasyon
  const float e_db = (e > -prm_.deadband && e < prm_.deadband) ? 0.0f : e;
  const float u_pre = P + i_ + d_;
  if ((u_pre >= outMax && e > 0) || (u_pre <= outMin && e < 0)) {
    o.anti_windup = true;
  } else if (!frozen && prm_.mode != PidMode::P) {
    i_ += prm_.ki * e_db * ts_min;
  }
  if (frozen) o.anti_windup = true;
  i_ = clampf(i_, -prm_.out_max, prm_.out_max);

  const float u = P + i_ + d_;
  o.p = P;
  o.i = i_;
  o.d = d_;
  o.raw = u;
  o.output = clampf(u, outMin, outMax);
  o.sat = u > outMax ? Saturation::HIGH : (u < outMin ? Saturation::LOW : Saturation::NONE);
  u_raw_prev_ = u;
  last_ = o;
  return o;
}

}  // namespace cc
