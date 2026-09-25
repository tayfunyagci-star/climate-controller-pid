#include "cc_profile.h"

namespace cc {

float ProfileResolver::setpointFor(const Config& c, ProfileActive p) {
  switch (p) {
    case ProfileActive::DAY: return c.temperature_setpoint;
    case ProfileActive::NIGHT: return c.setpoint_night;
    case ProfileActive::AWAY: return c.setpoint_away;
    case ProfileActive::FROST: return c.setpoint_frost;
    case ProfileActive::BOOST: return c.setpoint_boost;
  }
  return c.temperature_setpoint;
}

void ProfileResolver::setBoost(bool on, const Config&) {
  if (on && !boost_) boost_t_.reset();
  boost_ = on;
}
void ProfileResolver::setSchedNight(bool on) {
  if (on && !sched_night_) night_t_.reset();
  sched_night_ = on;
}
void ProfileResolver::setSchedAway(bool on) {
  if (on && !sched_away_) away_t_.reset();
  sched_away_ = on;
}

ProfileOutput ProfileResolver::step(const Config& c, const ProfileInput& in) {
  ProfileOutput o;

  // --- Süreli istekler ---
  if (boost_) {
    boost_t_.add(in.dt_ms);
    const uint32_t lim = minToMs((float)c.boost_minutes);
    if (boost_t_.atLeast(lim)) {
      boost_ = false;
      o.ev_boost_ended = true;
    } else {
      o.boost_remaining_min = (lim - boost_t_.ms + 59999u) / 60000u;
    }
  }
  const uint32_t schedLim = (uint32_t)c.sched_timeout_h * 3600000u;
  if (sched_night_) {
    night_t_.add(in.dt_ms);
    if (c.sched_timeout_h > 0 && night_t_.atLeast(schedLim)) { sched_night_ = false; o.ev_sched_night_expired = true; }
  }
  if (sched_away_) {
    away_t_.add(in.dt_ms);
    if (c.sched_timeout_h > 0 && away_t_.atLeast(schedLim)) { sched_away_ = false; o.ev_sched_away_expired = true; }
  }

  // --- Profil önceliği: BOOST › açık seçim › sched_away › sched_night › DAY ---
  ProfileActive act = ProfileActive::DAY;
  if (boost_) act = ProfileActive::BOOST;
  else if (c.profile != ProfileSel::DAY) act = (ProfileActive)(uint8_t)c.profile;
  else if (sched_away_ && !in.local_lock) act = ProfileActive::AWAY;
  else if (sched_night_ && !in.local_lock) act = ProfileActive::NIGHT;
  o.active = act;
  float target = setpointFor(c, act);
  o.source = (SetpointSource)(uint8_t)act;  // DAY..BOOST aynı sırada
  if (in.mode == OpMode::MANUAL) o.source = SetpointSource::MANUAL;

  // --- Antifreeze bekçisi ---
  const bool eligible = c.antifreeze_enabled && in.controller_enable && !in.service &&
                        in.t1_quality == Quality::GOOD && isValid(in.t1);
  if (!eligible) {
    if (antifreeze_) o.ev_antifreeze_off = true;
    antifreeze_ = false;
  } else if (!antifreeze_ && in.t1 < c.frost_guard_temperature) {
    antifreeze_ = true;
    o.ev_antifreeze_on = true;
  } else if (antifreeze_ && in.t1 >= c.setpoint_frost + c.frost_exit_hysteresis) {
    antifreeze_ = false;
    o.ev_antifreeze_off = true;
  }
  o.antifreeze = antifreeze_;
  bool immediate = false;
  if (antifreeze_) {
    const float af = c.setpoint_frost;
    if (in.mode != OpMode::AUTO || af >= target) {
      target = af;
      o.source = SetpointSource::ANTIFREEZE;
      immediate = true;
    }
  }
  if (act == ProfileActive::BOOST) immediate = true;
  o.target = target;

  // --- Rampa: yalnız yükselen setpoint; düşüş anında ---
  if (!init_) {
    eff_ = target;  // boot: önceki etkin setpoint yok → doğrudan hedef
    init_ = true;
  }
  if (immediate || c.setpoint_ramp_c_per_min <= 0 || target <= eff_) {
    eff_ = target;
  } else {
    eff_ += c.setpoint_ramp_c_per_min * (in.dt_ms / 60000.0f);
    if (isValid(in.t1) && in.t1 > eff_) eff_ = in.t1 < target ? in.t1 : target;  // T1 ≫ SP: beklemeden
    if (eff_ >= target) eff_ = target;
  }
  o.effective = eff_;
  o.ramping = eff_ < target;
  prev_active_ = act;
  last_ = o;
  return o;
}

}  // namespace cc
