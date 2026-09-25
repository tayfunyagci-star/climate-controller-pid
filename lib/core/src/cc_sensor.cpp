#include "cc_sensor.h"

namespace cc {

RoleParams RoleParams::t1(float offset, uint32_t interval_ms, uint32_t stale_ms, float tau_s, uint32_t stuck_ms) {
  RoleParams p;
  p.phys_min = -40; p.phys_max = 85; p.plaus_min = -30; p.plaus_max = 60; p.max_rate_per_s = 2.0f;
  p.offset = offset; p.interval_ms = interval_ms; p.stale_ms = stale_ms; p.tau_s = tau_s; p.stuck_ms = stuck_ms;
  return p;
}
RoleParams RoleParams::rh1(float offset, uint32_t interval_ms, uint32_t stale_ms, float tau_s, uint32_t stuck_ms) {
  RoleParams p;
  p.phys_min = 0; p.phys_max = 100; p.plaus_min = 1; p.plaus_max = 99.9f; p.max_rate_per_s = 10.0f;
  p.offset = offset; p.interval_ms = interval_ms; p.stale_ms = stale_ms; p.tau_s = tau_s; p.stuck_ms = stuck_ms;
  p.humidity = true;
  return p;
}
RoleParams RoleParams::t2(float offset, uint32_t interval_ms, uint32_t stale_ms) {
  RoleParams p;
  p.phys_min = -40; p.phys_max = 150; p.plaus_min = -30; p.plaus_max = 130; p.max_rate_per_s = 10.0f;
  p.ds18b20_sentinels = true; p.offset = offset; p.interval_ms = interval_ms; p.stale_ms = stale_ms;
  p.tau_s = 0; p.stuck_ms = UINT32_MAX;
  return p;
}

void RoleFilter::pushBucket(bool error) {
  if (b_total_[b_i_] < 0xFFFF) ++b_total_[b_i_];
  if (error && b_err_[b_i_] < 0xFFFF) ++b_err_[b_i_];
}

void RoleFilter::accountError(DrvStatus st) {
  ++r_.errors;
  if (st == DrvStatus::CRC_ERROR) ++r_.crc_errors;
  if (st == DrvStatus::TIMEOUT) ++r_.timeouts;
  if (consecutive_err_ < 255) ++consecutive_err_;
  if (consecutive_err_ >= 3) bad_ = true;
  pushBucket(true);
}

static float median3(const float* v, uint8_t n) {
  if (n == 1) return v[0];
  if (n == 2) return (v[0] + v[1]) * 0.5f;
  float a = v[0], b = v[1], c = v[2];
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;
}

const RoleReading& RoleFilter::tick(uint32_t dt_ms) {
  age_.add(dt_ms);
  since_sample_.add(dt_ms);
  b_t_.add(dt_ms);
  while (b_t_.ms >= 60000u) {
    b_t_.ms -= 60000u;
    b_i_ = (uint8_t)((b_i_ + 1) % 10);
    b_total_[b_i_] = 0;
    b_err_[b_i_] = 0;
  }
  finish();
  return r_;
}

const RoleReading& RoleFilter::sample(DrvStatus st, float raw, uint32_t dt_ms, bool heating_active) {
  // zamanı ilerlet (finish sonra tekrar çağrılır)
  age_.add(dt_ms);
  b_t_.add(dt_ms);
  while (b_t_.ms >= 60000u) {
    b_t_.ms -= 60000u;
    b_i_ = (uint8_t)((b_i_ + 1) % 10);
    b_total_[b_i_] = 0;
    b_err_[b_i_] = 0;
  }
  const float dt_s = (since_sample_.ms + dt_ms) / 1000.0f;
  since_sample_.reset();
  if (disabled_ || missing_) { finish(); return r_; }

  if (st != DrvStatus::OK || !isValid(raw)) {
    accountError(st);
    finish();
    return r_;
  }
  if (p_.ds18b20_sentinels && (raw == 85.0f || raw <= -127.0f)) {  // güç-açılış / bağlantı yok
    accountError(DrvStatus::OK);
    finish();
    return r_;
  }
  const float v = raw + p_.offset;
  if (v < p_.phys_min || v > p_.phys_max) {  // fiziksel aralık dışı → BAD örnek
    accountError(DrvStatus::OK);
    finish();
    return r_;
  }
  consecutive_err_ = 0;
  bad_ = false;
  pushBucket(false);

  bool sampleUncertain = false;
  const bool implausible = v < p_.plaus_min || v > p_.plaus_max;
  if (implausible) sampleUncertain = true;
  if (isValid(last_accepted_) && dt_s > 0 && std::fabs(v - last_accepted_) / dt_s > p_.max_rate_per_s)
    sampleUncertain = true;

  // takılı değer ve yoğuşma
  if (isValid(last_raw_) && v == last_raw_ && heating_active) stuck_t_.add((uint32_t)(dt_s * 1000.0f));
  else stuck_t_.reset();
  if (p_.humidity && v >= 99.5f) cond_t_.add((uint32_t)(dt_s * 1000.0f));
  else cond_t_.reset();
  last_raw_ = v;
  r_.raw = v;

  if (!implausible) {
    med_[medI_] = v;
    medI_ = (uint8_t)((medI_ + 1) % 3);
    if (medN_ < 3) ++medN_;
    float buf[3];
    for (uint8_t i = 0; i < medN_; ++i) buf[i] = med_[i];
    const float m = median3(buf, medN_);
    r_.safety = m;
    if (p_.tau_s <= 0 || !isValid(ema_)) ema_ = m;
    else ema_ += (m - ema_) * (dt_s / (p_.tau_s + dt_s));
    last_accepted_ = v;
  }
  last_uncertain_ = sampleUncertain;
  if (!sampleUncertain) {
    age_.reset();
    ever_good_ = true;
  }
  finish();
  return r_;
}

void RoleFilter::finish() {
  uint32_t tot = 0, err = 0;
  for (int i = 0; i < 10; ++i) { tot += b_total_[i]; err += b_err_[i]; }
  r_.error_rate_10m = tot ? 100.0f * (float)err / (float)tot : 0.0f;
  r_.intermittent = r_.error_rate_10m > 20.0f;
  r_.stuck = stuck_t_.atLeast(p_.stuck_ms);
  r_.condensation = p_.humidity && cond_t_.atLeast(30u * 60u * 1000u);
  r_.age_ms = age_.ms;

  Quality q;
  if (disabled_) q = Quality::DISABLED;
  else if (missing_) q = Quality::MISSING;
  else if (bad_) q = Quality::BAD;
  else if (age_.ms > p_.stale_ms) q = Quality::STALE;
  else if (!ever_good_) q = Quality::UNCERTAIN;  // boot: ilk geçerli örnek bekleniyor (değer yok)
  else if (last_uncertain_ || r_.stuck || r_.condensation || r_.error_rate_10m > 50.0f ||
           age_.ms > 3u * p_.interval_ms)
    q = Quality::UNCERTAIN;
  else q = Quality::GOOD;
  r_.quality = q;
  const bool usable = (q == Quality::GOOD || q == Quality::UNCERTAIN) && ever_good_;
  r_.value = usable ? ema_ : kNaN;
  if (!usable) r_.safety = kNaN;
  else if (!isValid(r_.safety)) r_.safety = ema_;
}

}  // namespace cc
