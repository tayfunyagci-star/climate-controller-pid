#include "cc_power.h"

namespace cc {

PowerParams PowerParams::fromConfig(const Config& c) {
  PowerParams p;
  p.driver = c.output_driver_r;
  p.window_ms = sToMs((float)c.tp_window_s);
  p.min_on_ms = sToMs((float)c.heater_min_on_s);
  p.min_off_ms = sToMs((float)c.heater_min_off_s);
  p.stage2_on = c.stage2_on;
  p.stage2_off = c.stage2_off;
  p.dwell_ms = sToMs((float)c.stage_min_dwell_s);
  p.power_w[0] = (float)c.heater_power_w_r1;
  p.power_w[1] = (float)c.heater_power_w_r2;
  p.rotation = c.lead_rotation;
  return p;
}

PowerManager::PowerManager(const PowerParams& p) : p_(p) {
  ch_[1].pos = p_.window_ms / 2;  // R2 penceresi yarım pencere kaydırılmış
  autoLead();
}

void PowerManager::setParams(const PowerParams& p) {
  p_ = p;
  autoLead();
}

static bool unequal(const PowerParams& p) {
  return p.power_w[0] > 0 && p.power_w[1] > 0 && p.power_w[0] != p.power_w[1];
}

void PowerManager::autoLead() {
  // Farklı güçte önce küçük rezistans modüle edilir (daha iyi çözünürlük)
  if (unequal(p_)) lead_ = p_.power_w[0] <= p_.power_w[1] ? 0 : 1;
}

float PowerManager::stage1Capacity() const {
  if (!unequal(p_)) return 0.5f;
  const float tot = p_.power_w[0] + p_.power_w[1];
  return p_.power_w[lead_] / tot;
}

uint32_t PowerManager::quantize(float duty) const {
  if (duty <= 0) return 0;
  const uint32_t W = p_.window_ms;
  uint32_t on = (uint32_t)(duty / 100.0f * (float)W + 0.5f);
  if (on > W) on = W;
  if (on < p_.min_on_ms) return 0;                 // kısa darbe yok
  if (W - on < p_.min_off_ms) return W;            // kısa boşluk yok
  return on;
}

PowerOutput PowerManager::step(const PowerInput& in) {
  PowerOutput o;
  const float d = in.enabled ? clampf(in.demand, 0.0f, 100.0f) : 0.0f;
  if (in.rotate && p_.rotation == LeadRotation::DAILY && !unequal(p_)) rotate_pending_ = true;

  stage_t_.add(in.dt_ms);
  const float cap1 = stage1Capacity();
  const float boundary = cap1 * 100.0f;
  const float onTh = boundary + (p_.stage2_on - 50.0f);
  const float offTh = boundary - (50.0f - p_.stage2_off);

  const uint8_t prev = stage_;
  if (d <= 0) {
    stage_ = 0;
  } else if (stage_ == 0) {
    stage_ = 1;
  } else if (stage_ == 1 && d >= onTh && stage_t_.atLeast(p_.dwell_ms)) {
    stage_ = 2;
  } else if (stage_ == 2 && d <= offTh && stage_t_.atLeast(p_.dwell_ms)) {
    stage_ = 1;
  }
  if (stage_ != prev) {
    stage_t_.reset();
    o.ev_stage_up = stage_ > prev;
    o.ev_stage_down = stage_ < prev;
  }
  // Lider değişimi yalnız talep 0 anında
  if (stage_ == 0 && rotate_pending_) {
    lead_ ^= 1u;
    rotate_pending_ = false;
    o.ev_rotated = true;
  }

  const uint8_t lead = lead_, fol = lead_ ^ 1u;
  float dutyLead = 0, dutyFol = 0;
  if (stage_ == 1) {
    dutyLead = d / cap1;
    if (dutyLead > 100) dutyLead = 100;  // histerezis bandında kıstırma
  } else if (stage_ == 2) {
    dutyLead = 100;
    dutyFol = clampf((d - boundary) / (1.0f - cap1), 0.0f, 100.0f);
  }
  float duty[2];
  duty[lead] = dutyLead;
  duty[fol] = dutyFol;

  // Zaman-oransal modülatör
  const uint32_t W = p_.window_ms ? p_.window_ms : 1;
  float frac[2];
  for (int i = 0; i < 2; ++i) {
    Channel& c = ch_[i];
    c.duty = duty[i];
    if (duty[i] <= 0) {
      c.on_ms = 0;            // azalış anında
      c.active = false;
    } else if (!c.active) {
      c.active = true;
      // Devreye girişte pencere başlar; diğer kanal modüle ediliyorsa yarım pencere kaydırılır (§3.5)
      const Channel& other = ch_[i ^ 1];
      c.pos = other.active ? (other.pos + W / 2) % W : 0;
      c.on_ms = quantize(duty[i]);
    } else {
      c.pos += in.dt_ms;
      if (c.pos >= W) {
        c.pos %= W;
        c.on_ms = quantize(duty[i]);  // pencere başında mandallanır
      } else if (duty[i] >= 100.0f && c.on_ms < W) {
        c.on_ms = quantize(duty[i]);  // tam güce geçiş beklemez
      }
    }
    o.req[i] = c.on_ms > 0 && c.pos < c.on_ms;
    frac[i] = (float)c.on_ms / (float)W;
    o.duty[i] = duty[i];
  }
  // İki büyük yük aynı tikte devreye girmez (flicker / ani akım): takipçinin açılışı bir tik ertelenir
  if (o.req[0] && o.req[1] && !prev_req_[0] && !prev_req_[1]) o.req[fol] = false;
  prev_req_[0] = o.req[0];
  prev_req_[1] = o.req[1];
  o.stage = stage_;
  o.lead = lead_;
  o.applied = (frac[lead] * cap1 + frac[fol] * (1.0f - cap1)) * 100.0f;
  last_ = o;
  return o;
}

}  // namespace cc
