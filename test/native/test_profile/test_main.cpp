// Profil çözümü, rampa, antifreeze — CONTROL_ARCHITECTURE §3, ADR-005, PID_DESIGN §7, §9-5.
#include <unity.h>
#include "cc_profile.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static ProfileInput in(float t1, OpMode m = OpMode::AUTO, uint32_t dt = 1000) {
  ProfileInput p;
  p.mode = m;
  p.t1 = t1;
  p.t1_quality = Quality::GOOD;
  p.dt_ms = dt;
  return p;
}

// Öncelik tablosu: BOOST › açık seçim › sched_away › sched_night › DAY
void test_priority_table() {
  struct Row { bool boost; ProfileSel sel; bool away; bool night; ProfileActive exp; };
  const Row rows[] = {
      {false, ProfileSel::DAY, false, false, ProfileActive::DAY},
      {false, ProfileSel::DAY, false, true, ProfileActive::NIGHT},
      {false, ProfileSel::DAY, true, false, ProfileActive::AWAY},
      {false, ProfileSel::DAY, true, true, ProfileActive::AWAY},
      {false, ProfileSel::NIGHT, true, false, ProfileActive::NIGHT},
      {false, ProfileSel::FROST, true, true, ProfileActive::FROST},
      {false, ProfileSel::AWAY, false, true, ProfileActive::AWAY},
      {true, ProfileSel::AWAY, true, true, ProfileActive::BOOST},
      {true, ProfileSel::DAY, false, false, ProfileActive::BOOST},
  };
  for (const Row& r : rows) {
    Config c;
    c.profile = r.sel;
    ProfileResolver pr;
    pr.setBoost(r.boost, c);
    pr.setSchedAway(r.away);
    pr.setSchedNight(r.night);
    ProfileOutput o = pr.step(c, in(20));
    TEST_ASSERT_EQUAL((int)r.exp, (int)o.active);
    TEST_ASSERT_EQUAL_FLOAT(ProfileResolver::setpointFor(c, r.exp), o.target);
  }
}

// A9: sched_night ON → NIGHT 18 °C; OFF → DAY
void test_A9_sched_night() {
  Config c;
  ProfileResolver pr;
  pr.step(c, in(20));
  pr.setSchedNight(true);
  ProfileOutput o = pr.step(c, in(20));
  TEST_ASSERT_EQUAL(ProfileActive::NIGHT, o.active);
  TEST_ASSERT_EQUAL_FLOAT(18.0f, o.effective);  // düşüş anında
  TEST_ASSERT_EQUAL(SetpointSource::NIGHT, o.source);
  pr.setSchedNight(false);
  o = pr.step(c, in(18));
  TEST_ASSERT_EQUAL(ProfileActive::DAY, o.active);
  TEST_ASSERT_EQUAL_FLOAT(21.0f, o.target);
}

// Yerel kilit: sched_* kaydedilir ama çözüme alınmaz
void test_local_lock_ignores_sched() {
  Config c;
  ProfileResolver pr;
  pr.setSchedAway(true);
  ProfileInput p = in(20);
  p.local_lock = true;
  ProfileOutput o = pr.step(c, p);
  TEST_ASSERT_EQUAL(ProfileActive::DAY, o.active);
  TEST_ASSERT_TRUE(pr.schedAway());
  p.local_lock = false;
  o = pr.step(c, p);
  TEST_ASSERT_EQUAL(ProfileActive::AWAY, o.active);
}

// D-19: sched_* sched_timeout_h sonra kendiliğinden düşer
void test_sched_timeout() {
  Config c;
  c.sched_timeout_h = 1;
  ProfileResolver pr;
  pr.setSchedNight(true);
  ProfileOutput o;
  bool ev = false;
  for (int i = 0; i < 3600 + 5; ++i) {
    o = pr.step(c, in(20));
    ev |= o.ev_sched_night_expired;
  }
  TEST_ASSERT_TRUE(ev);
  TEST_ASSERT_FALSE(pr.schedNight());
  TEST_ASSERT_EQUAL(ProfileActive::DAY, o.active);
}

// BOOST süreli; rampasız
void test_boost_timed_no_ramp() {
  Config c;
  c.boost_minutes = 10;
  ProfileResolver pr;
  pr.step(c, in(20));
  pr.setBoost(true, c);
  ProfileOutput o = pr.step(c, in(20));
  TEST_ASSERT_EQUAL_FLOAT(23.0f, o.effective);  // anında
  TEST_ASSERT_EQUAL(10u, o.boost_remaining_min);
  bool ended = false;
  for (int i = 0; i < 601; ++i) { o = pr.step(c, in(20)); ended |= o.ev_boost_ended; }
  TEST_ASSERT_TRUE(ended);
  TEST_ASSERT_EQUAL(ProfileActive::DAY, o.active);
}

// §9-5: SP 18 → 22, etkin SP 0.2 °C/dk yükselir; düşüş anında
void test_ramp() {
  Config c;
  c.temperature_setpoint = 18;
  ProfileResolver pr;
  pr.step(c, in(17));
  c.temperature_setpoint = 22;
  ProfileOutput o;
  for (int i = 0; i < 600; ++i) o = pr.step(c, in(17));  // 10 dk
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 20.0f, o.effective);
  TEST_ASSERT_TRUE(o.ramping);
  for (int i = 0; i < 600; ++i) o = pr.step(c, in(17));
  TEST_ASSERT_EQUAL_FLOAT(22.0f, o.effective);
  c.temperature_setpoint = 19;
  o = pr.step(c, in(17));
  TEST_ASSERT_EQUAL_FLOAT(19.0f, o.effective);
}

// T1 ≫ SP: rampa beklemeden T1'e kadar
void test_ramp_skips_when_t1_above() {
  Config c;
  c.temperature_setpoint = 18;
  ProfileResolver pr;
  pr.step(c, in(21));
  c.temperature_setpoint = 22;
  ProfileOutput o = pr.step(c, in(21));
  TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(21.0f, o.effective);
}

// Antifreeze: T1 < 4 → etkin; T1 ≥ frost + 1 → çıkış; OFF/VENT_ONLY/AUTO/MANUAL'da
void test_antifreeze_enter_exit_all_modes() {
  const OpMode modes[] = {OpMode::OFF, OpMode::VENT_ONLY, OpMode::AUTO, OpMode::MANUAL};
  for (OpMode m : modes) {
    Config c;
    ProfileResolver pr;
    ProfileOutput o = pr.step(c, in(4.5f, m));
    TEST_ASSERT_FALSE(o.antifreeze);
    o = pr.step(c, in(3.9f, m));
    TEST_ASSERT_TRUE(o.antifreeze);
    TEST_ASSERT_TRUE(o.ev_antifreeze_on);
    if (m != OpMode::AUTO) {
      TEST_ASSERT_EQUAL(SetpointSource::ANTIFREEZE, o.source);
      TEST_ASSERT_EQUAL_FLOAT(5.0f, o.effective);
    }
    o = pr.step(c, in(5.9f, m));
    TEST_ASSERT_TRUE(o.antifreeze);
    o = pr.step(c, in(6.0f, m));
    TEST_ASSERT_FALSE(o.antifreeze);
  }
}

// Ölçümsüz antifreeze yok; SERVICE ve controller_enable=OFF'ta devre dışı
void test_antifreeze_requires_good_sensor() {
  Config c;
  ProfileResolver pr;
  ProfileInput p = in(2.0f, OpMode::OFF);
  p.t1_quality = Quality::STALE;
  TEST_ASSERT_FALSE(pr.step(c, p).antifreeze);
  p.t1_quality = Quality::GOOD;
  p.service = true;
  TEST_ASSERT_FALSE(pr.step(c, p).antifreeze);
  p.service = false;
  p.controller_enable = false;
  TEST_ASSERT_FALSE(pr.step(c, p).antifreeze);
  p.controller_enable = true;
  TEST_ASSERT_TRUE(pr.step(c, p).antifreeze);
  c.antifreeze_enabled = false;
  TEST_ASSERT_FALSE(pr.step(c, p).antifreeze);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_priority_table);
  RUN_TEST(test_A9_sched_night);
  RUN_TEST(test_local_lock_ignores_sched);
  RUN_TEST(test_sched_timeout);
  RUN_TEST(test_boost_timed_no_ramp);
  RUN_TEST(test_ramp);
  RUN_TEST(test_ramp_skips_when_t1_above);
  RUN_TEST(test_antifreeze_enter_exit_all_modes);
  RUN_TEST(test_antifreeze_requires_good_sensor);
  return UNITY_END();
}
