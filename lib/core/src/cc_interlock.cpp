#include "cc_interlock.h"

namespace cc {

InterlockParams InterlockParams::fromConfig(const Config& c) {
  InterlockParams p;
  p.prestart_ms = sToMs((float)c.fan_prestart_s);
  p.heater_min_on_ms = sToMs((float)c.heater_min_on_s);
  p.heater_min_off_ms = sToMs((float)c.heater_min_off_s);
  p.vf_min_on_ms = sToMs((float)c.vent_min_on_s);
  p.vf_min_off_ms = sToMs((float)c.vent_min_off_s);
  p.pc_mode = c.post_cool_mode;
  p.pc_ms = sToMs((float)c.post_cool_seconds);
  p.pc_min_ms = sToMs((float)c.post_cool_min_s);
  p.pc_max_ms = sToMs((float)c.post_cool_max_s);
  p.pc_safe_temp = c.post_cool_safe_temp;
  return p;
}

void InterlockEngine::startBootPostCool() {
  post_cool_ = true;
  boot_pc_ = true;
  pc_t_.reset();
}

bool InterlockEngine::postCoolDone(const InterlockInput& in) const {
  const bool timeOk = pc_t_.atLeast(p_.pc_ms);
  if (boot_pc_) return timeOk;  // boot sonrası T2 henüz hazır olmayabilir → TIME
  const bool t2Ok = in.t2_q == Quality::GOOD && isValid(in.t2) && in.t2 < p_.pc_safe_temp &&
                    pc_t_.atLeast(p_.pc_min_ms);
  switch (p_.pc_mode) {
    case PostCoolMode::TIME: return timeOk;
    case PostCoolMode::TEMPERATURE: return t2Ok;
    case PostCoolMode::HYBRID: return timeOk && t2Ok;
  }
  return timeOk;
}

InterlockOutput InterlockEngine::step(const InterlockInput& in) {
  InterlockOutput o;
  bool next[OUT_COUNT] = {false, false, false, false};
  Reason rsn[OUT_COUNT] = {Reason::NONE, Reason::NONE, Reason::NONE, Reason::NONE};
  const bool hfPrev = eff_[HF];
  const bool blocked = in.heater_lockout || !in.arm;
  const bool fanReady = hfPrev && on_t_[HF].atLeast(p_.prestart_ms);

  // ---------------- R1 / R2 ----------------
  for (uint8_t i = R1; i <= R2; ++i) {
    const bool want = in.r_req[i] && in.heat_chain;
    bool r = eff_[i];
    if (blocked) {                                   // I-1 (min ON beklenmez)
      r = false;
      if (want) rsn[i] = in.lockout_reason;
    } else if (want && !eff_[i]) {
      if (!fanReady) { r = false; rsn[i] = Reason::FAN_PRESTART; }                        // I-3
      else if (!off_t_[i].atLeast(p_.heater_min_off_ms)) { r = false; rsn[i] = Reason::MIN_OFF_TIME; }  // I-8
      else r = true;
    } else if (!want && eff_[i]) {
      if (!on_t_[i].atLeast(p_.heater_min_on_ms)) { r = true; rsn[i] = Reason::MIN_ON_TIME; }  // I-7
      else r = false;
    }
    next[i] = r;
  }
  const bool anyR = next[R1] || next[R2];
  const bool chain = in.heat_chain && !blocked;

  // ---------------- Post-cool ----------------
  if (anyR || chain) {
    if (post_cool_) { post_cool_ = false; boot_pc_ = false; }  // yeni talep: prepurge olmadan aktife dön
    was_heating_ = true;
  } else if (was_heating_) {
    was_heating_ = false;
    post_cool_ = true;
    boot_pc_ = false;
    pc_t_.reset();
    o.ev_post_cool_start = true;
  }
  if (post_cool_) {
    if (!o.ev_post_cool_start) pc_t_.add(in.dt_ms);  // süre R'nin kapandığı andan sayılır
    if (postCoolDone(in)) {
      post_cool_ = false;
      boot_pc_ = false;
      o.ev_post_cool_end = true;
    } else if (!boot_pc_ && p_.pc_mode != PostCoolMode::TIME && pc_t_.atLeast(p_.pc_max_ms)) {
      o.post_cool_timeout = true;  // güvenli yön: fan açık kalır
    }
  }
  const bool prepurge = chain && !anyR && !fanReady;

  // ---------------- Heater Fan (I-2) ----------------
  const bool hfForce = anyR || chain || post_cool_ || in.hf_force_on;
  if (hfForce) {
    next[HF] = true;
    if (!in.hf_req) {
      if (anyR) rsn[HF] = Reason::HEATER_INTERLOCK;
      else if (prepurge) rsn[HF] = Reason::PREPURGE;
      else if (chain) rsn[HF] = Reason::HEATER_INTERLOCK;
      else if (post_cool_) rsn[HF] = boot_pc_ ? Reason::BOOT_POST_COOL : Reason::POST_COOL;
      else rsn[HF] = Reason::OVERTEMPERATURE;
    }
  } else if (in.hf_req && !hfPrev) {
    if (!off_t_[HF].atLeast(p_.hf_min_off_ms)) { next[HF] = false; rsn[HF] = Reason::MIN_OFF_TIME; }
    else next[HF] = true;
  } else if (!in.hf_req && hfPrev) {
    if (!on_t_[HF].atLeast(p_.hf_min_on_ms)) { next[HF] = true; rsn[HF] = Reason::MIN_ON_TIME; }
    else next[HF] = false;
  } else {
    next[HF] = hfPrev;
  }

  // ---------------- Ventilation Fan ----------------
  const bool vfPrev = eff_[VF];
  if (in.vf_force_on) {                                           // I-4
    next[VF] = true;
    if (!in.vf_req) rsn[VF] = Reason::OVERTEMPERATURE;
  } else if (in.antifreeze) {                                      // I-5
    next[VF] = false;
    if (in.vf_req) rsn[VF] = Reason::ANTIFREEZE_INHIBIT;
  } else if (in.vf_inhibit != Reason::NONE) {                      // I-6 / I-9
    next[VF] = false;
    if (in.vf_req) rsn[VF] = in.vf_inhibit;
  } else if (in.vf_req && !vfPrev) {
    if (!off_t_[VF].atLeast(p_.vf_min_off_ms)) { next[VF] = false; rsn[VF] = Reason::MIN_OFF_TIME; }  // I-8
    else next[VF] = true;
  } else if (!in.vf_req && vfPrev) {
    if (!on_t_[VF].atLeast(p_.vf_min_on_ms)) { next[VF] = true; rsn[VF] = Reason::MIN_ON_TIME; }      // I-7
    else next[VF] = false;
  } else {
    next[VF] = vfPrev;
  }

  // ---------------- Değişmez (ikinci kontrol) ----------------
  if ((next[R1] || next[R2]) && !next[HF]) {  // yapısal olarak oluşamaz; oluşursa güvenli yön
    next[R1] = next[R2] = false;
  }

  // ---------------- Zamanlayıcılar ----------------
  for (uint8_t k = 0; k < OUT_COUNT; ++k) {
    if (next[k] != eff_[k]) {
      if (next[k]) { on_t_[k].reset(); o.ev_on[k] = true; }
      else { off_t_[k].reset(); o.ev_off[k] = true; }
    }
    if (next[k]) on_t_[k].add(in.dt_ms);
    else off_t_[k].add(in.dt_ms);
    eff_[k] = next[k];
    o.eff[k] = next[k];
    o.reason[k] = rsn[k];
  }
  o.post_cool = post_cool_;
  o.boot_post_cool = post_cool_ && boot_pc_;
  o.prepurge = prepurge;
  o.post_cool_elapsed_ms = post_cool_ ? pc_t_.ms : 0;
  o.post_cool_remaining_ms = (post_cool_ && pc_t_.ms < p_.pc_ms) ? p_.pc_ms - pc_t_.ms : 0;
  o.hf_on_ms = eff_[HF] ? on_t_[HF].ms : 0;
  last_ = o;
  return o;
}

OutputGuard::Result OutputGuard::apply(const bool d[OUT_COUNT], uint32_t dt_ms) {
  Result r{};
  const bool prevHF = applied_[HF];
  const bool prevAnyR = applied_[R1] || applied_[R2];
  const bool wantAnyR = d[R1] || d[R2];
  r.violation = wantAnyR && !d[HF];
  // HF: istenen ya da R önceki tikte açıksa açık kalır (kapanışta önce R)
  r.out[HF] = d[HF] || prevAnyR || (wantAnyR && r.violation);
  // R: yalnız HF önceki tikte zaten açık ve bu tikte de açıksa
  for (uint8_t i = R1; i <= R2; ++i) r.out[i] = d[i] && d[HF] && prevHF && r.out[HF];
  r.out[VF] = d[VF];
  for (uint8_t k = 0; k < OUT_COUNT; ++k) {
    if (r.out[k] && !applied_[k]) ++switches_[k];
    if (r.out[k]) on_ms_[k] += dt_ms;
    applied_[k] = r.out[k];
  }
  return r;
}

}  // namespace cc
