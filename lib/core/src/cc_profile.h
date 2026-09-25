// Profil çözümü, setpoint rampası ve antifreeze bekçisi — CONTROL_ARCHITECTURE §3, ADR-005, PID_DESIGN §7.
#pragma once
#include "cc_config.h"

namespace cc {

struct ProfileInput {
  OpMode mode = OpMode::AUTO;
  bool service = false;           // sistem SERVICE durumunda (antifreeze devre dışı)
  bool controller_enable = true;  // OFF → antifreeze dahil tüm otomatik kontrol durur
  bool local_lock = false;        // sched_* istekleri kaydedilir ama uygulanmaz
  float t1 = kNaN;                // kontrol değeri (PV_f)
  Quality t1_quality = Quality::MISSING;
  uint32_t dt_ms = 0;
};

struct ProfileOutput {
  ProfileActive active = ProfileActive::DAY;
  SetpointSource source = SetpointSource::DAY;
  float target = 21.0f;           // rampasız hedef
  float effective = 21.0f;        // setpoint_effective
  bool ramping = false;
  bool antifreeze = false;        // antifreeze bekçisi etkin
  uint32_t boost_remaining_min = 0;
  // Bu adımda oluşan olaylar
  bool ev_boost_ended = false;
  bool ev_sched_night_expired = false;
  bool ev_sched_away_expired = false;
  bool ev_antifreeze_on = false;
  bool ev_antifreeze_off = false;
};

class ProfileResolver {
 public:
  // İstek kanalları (switch state = istek)
  void setBoost(bool on, const Config& c);
  void setSchedNight(bool on);
  void setSchedAway(bool on);
  bool boost() const { return boost_; }
  bool schedNight() const { return sched_night_; }
  bool schedAway() const { return sched_away_; }

  ProfileOutput step(const Config& c, const ProfileInput& in);
  const ProfileOutput& last() const { return last_; }

  // Profil setpoint'i (rampasız)
  static float setpointFor(const Config& c, ProfileActive p);

 private:
  bool boost_ = false, sched_night_ = false, sched_away_ = false;
  Timer boost_t_, night_t_, away_t_;
  bool antifreeze_ = false;
  bool init_ = false;
  float eff_ = 0;
  ProfileActive prev_active_ = ProfileActive::DAY;
  ProfileOutput last_;
};

}  // namespace cc
