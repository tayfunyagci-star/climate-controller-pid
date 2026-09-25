// SafetyEvaluator — SAFETY_DESIGN §3 (S1–S19), STATE_MACHINE §4, SR-05..SR-08, SR-11.
#include <unity.h>
#include "cc_safety.h"

using namespace cc;

void setUp() {}
void tearDown() {}

struct S {
  SafetyEvaluator sf;
  SafetyInput in;
  SafetyOutput o;
  S() { in.t1 = 20; in.t1_q = Quality::GOOD; in.dt_ms = 250; }
  SafetyOutput run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 250) o = sf.step(in);
    return o;
  }
};

// S1: T1 ≥ 40 °C 10 s → kilit, VF zorunlu; koşul sürerken reset reddedilir (SR-11)
void test_S1_overtemp() {
  S s;
  s.in.t1 = 41;
  SafetyOutput o = s.run(9750);
  TEST_ASSERT_FALSE(o.lockout);
  TEST_ASSERT_TRUE(o.overtemp_condition);
  o = s.run(500);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::OVERTEMP, o.reason);
  TEST_ASSERT_EQUAL(Reason::OVERTEMPERATURE_LOCKOUT, o.lockout_reason);
  TEST_ASSERT_TRUE(o.vf_force);
  s.in.t1 = 38;  // limit − 3 = 37'nin üstü: koşul temizlenmedi
  s.in.reset_request = true;
  o = s.run(250);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_TRUE(o.reset_refused);
  TEST_ASSERT_TRUE(o.vf_force);
  s.in.reset_request = false;
  s.in.t1 = 36.5f;
  o = s.run(5000);
  TEST_ASSERT_TRUE(o.lockout);    // kilit kendiliğinden kalkmaz
  TEST_ASSERT_FALSE(o.vf_force);  // tahliye koşulu temizlendi
  s.in.reset_request = true;
  o = s.run(250);
  TEST_ASSERT_TRUE(o.reset_done);
  TEST_ASSERT_FALSE(o.lockout);
}

// S2: T2 ≥ 80 °C 3 s → kilit, HF ON
void test_S2_outlet_overtemp() {
  SafetyParams p;
  p.t2_enabled = true;
  S s;
  s.sf.setParams(p);
  s.in.t2_q = Quality::GOOD;
  s.in.t2 = 85;
  SafetyOutput o = s.run(2750);
  TEST_ASSERT_FALSE(o.lockout);
  o = s.run(500);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_TRUE(o.hf_force);
  TEST_ASSERT_FALSE(o.vf_force);
}

// S3/S4 + SR-05: sensör BAD/STALE → anında kilit; GOOD ≥ 30 s ile otomatik temizlenir
void test_S3_S4_sensor_fault_auto_clear() {
  S s;
  s.in.t1_q = Quality::BAD;
  SafetyOutput o = s.run(250);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::SENSOR_FAULT, o.reason);
  TEST_ASSERT_EQUAL(Reason::SENSOR_FAULT, o.lockout_reason);
  s.in.t1_q = Quality::GOOD;
  o = s.run(29750);
  TEST_ASSERT_TRUE(o.lockout);
  o = s.run(500);
  TEST_ASSERT_FALSE(o.lockout);
  s.in.t1_q = Quality::STALE;
  o = s.run(250);
  TEST_ASSERT_TRUE(o.sensor_stale);
  TEST_ASSERT_TRUE(o.lockout);
  // UNCERTAIN kilit üretmez
  S u;
  u.in.t1_q = Quality::UNCERTAIN;
  TEST_ASSERT_FALSE(u.run(5000).lockout);
  // SensorTask heartbeat > 5 s → bayat
  S h;
  h.in.sensor_hb_age_ms = 5100;
  TEST_ASSERT_TRUE(h.run(250).sensor_stale);
}

// S6: T2 varken HF ON ∧ R ON ve T2 > 15 °C/dk → HEATER_FAN_FAULT kilidi
void test_S6_heater_fan_fault() {
  SafetyParams p;
  p.t2_enabled = true;
  S s;
  s.sf.setParams(p);
  s.in.t2_q = Quality::GOOD;
  s.in.any_r_on = true;
  s.in.hf_on = true;
  float t2 = 30;
  SafetyOutput o;
  for (int i = 0; i < 4 * 70; ++i) {  // 70 s, 20 °C/dk
    t2 += 20.0f / 60.0f / 4.0f;
    s.in.t2 = t2;
    o = s.sf.step(s.in);
  }
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::HEATER_FAN_FAULT, o.reason);
}

// S7: talep doyumda kesintisiz 240 dk → HEATING_TIMEOUT (F1 yorumu: doyum şartı)
void test_S7_heating_timeout() {
  S s;
  s.in.heating_active = true;
  s.in.heat_demand = 100;
  SafetyOutput o = s.run(239u * 60000u);
  TEST_ASSERT_FALSE(o.lockout);
  o = s.run(61000);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::HEATING_TIMEOUT, o.reason);
  // Kararlı rejimde (talep doyumda değil) süre birikmez
  S k;
  k.in.heating_active = true;
  k.in.heat_demand = 40;
  TEST_ASSERT_FALSE(k.run(300u * 60000u).lockout);
  // Reset: talep düşmüşse kalkar
  s.in.heating_active = false;
  s.in.heat_demand = 0;
  s.in.reset_request = true;
  o = s.run(250);
  TEST_ASSERT_FALSE(o.lockout);
}

// S9: R OFF iken 10 dk'da > 1.5 °C artış → uyarı; R ON iken > 5 °C → kritik kilit
void test_S9_unexpected_rise() {
  S s;
  float t = 20;
  SafetyOutput o;
  for (int i = 0; i < 4 * 660; ++i) {  // 11 dk, 2 °C/10 dk, R OFF
    t += 2.0f / 600.0f / 4.0f;
    s.in.t1 = t;
    o = s.sf.step(s.in);
  }
  TEST_ASSERT_TRUE(o.rise_warning);
  TEST_ASSERT_FALSE(o.lockout);
  S r;
  r.in.any_r_on = true;
  t = 20;
  for (int i = 0; i < 4 * 660; ++i) {  // 6 °C/10 dk, R ON
    t += 6.0f / 600.0f / 4.0f;
    r.in.t1 = t;
    o = r.sf.step(r.in);
  }
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::TEMP_RISE, o.reason);
}

// S13: config hatası → CONFIG_ERROR kilidi (kilitsiz, geçerli config ile kalkar)
void test_S13_config_error() {
  S s;
  s.in.config_error = true;
  SafetyOutput o = s.run(250);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::CONFIG_ERROR, o.reason);
  s.in.config_error = false;
  TEST_ASSERT_FALSE(s.run(250).lockout);
}

// S17 + SR-08: ControlTask heartbeat 6 s, OutputTask 1 s, guard ihlali → INTERNAL_FAULT (reset ile kalkmaz)
void test_S17_internal_fault() {
  S s;
  s.in.control_hb_age_ms = 6000;
  TEST_ASSERT_FALSE(s.run(250).lockout);
  s.in.control_hb_age_ms = 6250;
  SafetyOutput o = s.run(250);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::INTERNAL_FAULT, o.reason);
  s.in.control_hb_age_ms = 0;
  s.in.reset_request = true;
  o = s.run(250);
  TEST_ASSERT_TRUE(o.lockout);  // yalnız reboot + self-test
  S out;
  out.in.output_hb_age_ms = 1100;
  o = out.run(250);
  TEST_ASSERT_FALSE(o.arm_allowed);
  TEST_ASSERT_TRUE(o.lockout);
  S g;
  g.in.guard_violation = true;
  TEST_ASSERT_EQUAL(FailsafeReason::INTERNAL_FAULT, g.run(250).reason);
}

// S19: donma riski + sensör arızası → alarm; ısıtma yok (kilit sensör nedeniyle)
void test_S19_frost_risk_no_sensor() {
  S s;
  s.in.t1 = 5;
  s.run(1000);
  s.in.t1_q = Quality::STALE;
  s.in.t1 = kNaN;
  SafetyOutput o = s.run(250);
  TEST_ASSERT_TRUE(o.frost_risk);
  TEST_ASSERT_TRUE(o.lockout);
}

// Kilitler reboot'ta korunur (INTERNAL hariç)
void test_restore_latched() {
  SafetyEvaluator sf;
  sf.restoreLatched(SL_OVERTEMP | SL_INTERNAL);
  SafetyInput in;
  in.t1 = 20; in.t1_q = Quality::GOOD;
  SafetyOutput o = sf.step(in);
  TEST_ASSERT_TRUE(o.lockout);
  TEST_ASSERT_EQUAL(FailsafeReason::OVERTEMP, o.reason);
  TEST_ASSERT_FALSE(o.latched & SL_INTERNAL);
}

// Neden önceliği: INTERNAL › OVERTEMP › … › SENSOR
void test_reason_priority() {
  SafetyEvaluator sf;
  sf.restoreLatched(SL_HEATING_TIMEOUT | SL_OVERTEMP);
  SafetyInput in;
  in.t1_q = Quality::BAD;
  in.config_error = true;
  TEST_ASSERT_EQUAL(FailsafeReason::OVERTEMP, sf.step(in).reason);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_S1_overtemp);
  RUN_TEST(test_S2_outlet_overtemp);
  RUN_TEST(test_S3_S4_sensor_fault_auto_clear);
  RUN_TEST(test_S6_heater_fan_fault);
  RUN_TEST(test_S7_heating_timeout);
  RUN_TEST(test_S9_unexpected_rise);
  RUN_TEST(test_S13_config_error);
  RUN_TEST(test_S17_internal_fault);
  RUN_TEST(test_S19_frost_risk_no_sensor);
  RUN_TEST(test_restore_latched);
  RUN_TEST(test_reason_priority);
  return UNITY_END();
}
