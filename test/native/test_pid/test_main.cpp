// PID / PI — PID_DESIGN §9 doğrulama kriterleri ve mekanizma testleri.
#include <unity.h>
#include "cc_pid.h"
#include "support/sim.h"

using namespace cc;

void setUp() {}
void tearDown() {}

// §9-1: 100 % doyumda ≥ 60 dk sonra SP'ye ulaşma, aşım ≤ 0.5 °C (anti-windup)
void test_saturation_overshoot() {
  Pid pid;  // PI, Kp 20, Ki 1.0
  // Kulübe: τ = 60 dk, kazanç 0.3 °C/%, ölü zaman 120 s, dış −5 °C
  const float sp = 22, dt = 2, tau = 3600, gain = 0.3f, Tout = -5;
  float T = -5, applied = kNaN, maxT = -100, satMin = 0;
  float delay[60] = {0};
  bool crossed = false;
  for (int k = 0; k < 6 * 3600 / 2; ++k) {
    PidInput in;
    in.sp = sp; in.pv = T; in.dt_s = dt; in.applied = applied;
    const PidOutput o = pid.step(in);
    if (o.output >= 99.99f) satMin += dt / 60.0f;
    applied = o.output;
    const float u = delay[k % 60];
    delay[k % 60] = o.output;
    T += dt * (gain * u - (T - Tout)) / tau;
    if (T >= sp - 0.1f) crossed = true;  // deadband içinde SP
    if (crossed && T > maxT) maxT = T;
  }
  TEST_ASSERT_TRUE(crossed);
  TEST_ASSERT_TRUE_MESSAGE(satMin >= 60.0f, "en az 60 dk doyum beklenir");
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(sp + 0.5f, maxT);
}

// §9-2: MANUAL 40 % → AUTO, talep sıçraması ≤ 2 %
void test_bumpless_manual_to_auto() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 20.5f; in.dt_s = 2;
  in.tracking = true; in.track_value = 40;
  for (int i = 0; i < 30; ++i) pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, pid.last().output);
  pid.bumplessTo(40, in.sp, in.pv);
  in.tracking = false;
  in.applied = 40;
  PidOutput o = pid.step(in);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, 40.0f, o.output);
}

// §9-3: Kp 20 → 30 anlık değişim, çıkış sıçraması ≤ 1 %
void test_bumpless_gain_change() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 21.0f; in.dt_s = 2;
  for (int i = 0; i < 100; ++i) { in.applied = pid.last().output; pid.step(in); }
  const float before = pid.last().output;
  PidParams p = pid.params();
  p.kp = 30;
  pid.setParams(p, in.sp, in.pv);
  in.applied = before;
  PidOutput o = pid.step(in);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, before, o.output);
}

// Mod değişimi PI → PID ve ONOFF → PI de bumpless
void test_bumpless_mode_change() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 21.5f; in.dt_s = 2;
  for (int i = 0; i < 50; ++i) { in.applied = pid.last().output; pid.step(in); }
  float before = pid.last().output;
  PidParams p = pid.params();
  p.mode = PidMode::PID; p.kd = 10;
  pid.setParams(p, in.sp, in.pv);
  PidOutput o = pid.step(in);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, before, o.output);
  // ONOFF → PI
  p.mode = PidMode::ONOFF;
  pid.setParams(p, in.sp, in.pv);
  in.pv = 21.0f;
  o = pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, o.output);
  p.mode = PidMode::PI;
  pid.setParams(p, in.sp, in.pv);
  in.applied = 100;
  o = pid.step(in);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, o.output);
}

// §9-4: deadband içinde 1 sa gürültülü PV, integratör kayması ≤ 1 %
void test_deadband_integrator_drift() {
  Pid pid;
  sim::Rng rng(7);
  PidInput in;
  in.sp = 22; in.dt_s = 2;
  in.pv = 22; in.tracking = true; in.track_value = 30;
  pid.step(in);
  in.tracking = false;
  const float i0 = pid.integrator();
  for (int k = 0; k < 1800; ++k) {
    in.pv = 22.0f + rng.uniform(-0.09f, 0.09f);
    in.applied = pid.last().output;
    pid.step(in);
  }
  TEST_ASSERT_FLOAT_WITHIN(1.0f, i0, pid.integrator());
}

// §9-8: Ts 2 → 5 s, Ki dakika tabanlı: aynı sürede aynı integral
void test_ts_independence() {
  float res[2];
  const float ts[2] = {2.0f, 5.0f};
  for (int j = 0; j < 2; ++j) {
    Pid pid;
    PidInput in;
    in.sp = 22; in.pv = 21; in.dt_s = ts[j];
    const int n = (int)(600 / ts[j]);
    for (int k = 0; k < n; ++k) { in.applied = pid.last().output; pid.step(in); }
    res[j] = pid.integrator();
  }
  TEST_ASSERT_FLOAT_WITHIN(0.2f, res[0], res[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.0f, res[0]);  // Ki 1 %/(°C·dk) × 1 °C × 10 dk
}

// Koşullu entegrasyon: doyumda integratör büyümez
void test_conditional_integration() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 10; in.dt_s = 2;  // P = 240 → doyum
  for (int k = 0; k < 600; ++k) pid.step(in);
  TEST_ASSERT_TRUE(pid.last().anti_windup);
  TEST_ASSERT_EQUAL(Saturation::HIGH, pid.last().sat);
  TEST_ASSERT_LESS_THAN_FLOAT(1.0f, pid.integrator());
}

// Dinamik üst sınır (max_heat_demand / vent cap) clamping
void test_dynamic_out_max() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 19; in.dt_s = 2; in.out_max = 40;
  for (int k = 0; k < 300; ++k) { in.applied = pid.last().output; pid.step(in); }
  TEST_ASSERT_EQUAL_FLOAT(40.0f, pid.last().output);
  TEST_ASSERT_TRUE(pid.last().anti_windup);
}

// Geri hesaplama: aşağı akış daha az uygularsa integratör düşer
void test_back_calculation() {
  Pid a, b;
  PidInput in;
  in.sp = 22; in.pv = 21.5f; in.dt_s = 2;
  for (int k = 0; k < 300; ++k) {
    in.applied = a.last().output; a.step(in);
    in.applied = b.last().output * 0.5f; b.step(in);
  }
  TEST_ASSERT_LESS_THAN_FLOAT(a.integrator(), b.integrator());
}

// Geçici hold (prestart/post-cool): 5 dk'dan kısa kesintide integratör korunur
void test_hold_preserves_integrator() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 21; in.dt_s = 2;
  for (int k = 0; k < 100; ++k) { in.applied = pid.last().output; pid.step(in); }
  const float i0 = pid.integrator();
  in.hold = true; in.applied = 0;
  for (int k = 0; k < 60; ++k) pid.step(in);  // 2 dk
  TEST_ASSERT_EQUAL_FLOAT(i0, pid.integrator());
  for (int k = 0; k < 150; ++k) pid.step(in);  // toplam 7 dk: artık korunmaz
  TEST_ASSERT_NOT_EQUAL(i0, pid.integrator());
}

// Tracking ve arıza dönüşü
void test_tracking_and_fault_reset() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 20; in.dt_s = 2;
  for (int k = 0; k < 100; ++k) { in.applied = pid.last().output; pid.step(in); }
  in.tracking = true; in.track_value = 0;
  PidOutput o = pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.output);
  TEST_ASSERT_TRUE(o.tracking);
  pid.resetAfterFault();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integrator());
}

// Türev ölçüm üzerinden: setpoint basamağında D sıçraması yok
void test_derivative_on_measurement() {
  PidParams p; p.mode = PidMode::PID; p.kd = 10;
  Pid pid(p);
  PidInput in;
  in.sp = 20; in.pv = 20; in.dt_s = 2;
  for (int k = 0; k < 10; ++k) pid.step(in);
  in.sp = 23;
  PidOutput o = pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.d);
  in.pv = 20.5f;  // ölçüm artışı → negatif D
  o = pid.step(in);
  TEST_ASSERT_LESS_THAN_FLOAT(0.0f, o.d);
}

// Setpoint ağırlığı b < 1: SP basamağında P sıçraması sınırlı
void test_setpoint_weight() {
  PidParams p; p.b = 0.5f;
  Pid pid(p);
  PidInput in;
  in.sp = 20; in.pv = 20; in.dt_s = 2;
  PidOutput o1 = pid.step(in);
  in.sp = 22;
  PidOutput o2 = pid.step(in);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f * 0.5f * 2.0f, o2.p - o1.p);
}

void test_onoff_hysteresis() {
  PidParams p; p.mode = PidMode::ONOFF; p.onoff_hyst = 0.5f;
  Pid pid(p);
  PidInput in;
  in.sp = 22; in.dt_s = 2;
  in.pv = 21.7f; TEST_ASSERT_EQUAL_FLOAT(100.0f, pid.step(in).output);
  in.pv = 22.1f; TEST_ASSERT_EQUAL_FLOAT(100.0f, pid.step(in).output);  // bant içinde korunur
  in.pv = 22.3f; TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.step(in).output);
  in.pv = 21.9f; TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.step(in).output);
}

void test_invalid_pv_gives_zero() {
  Pid pid;
  PidInput in;
  in.sp = 22; in.pv = 20; in.dt_s = 2;
  for (int k = 0; k < 50; ++k) pid.step(in);
  const float i0 = pid.integrator();
  in.pv = kNaN;
  PidOutput o = pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.output);
  TEST_ASSERT_EQUAL_FLOAT(i0, pid.integrator());
}

void test_p_mode_no_integration() {
  PidParams p; p.mode = PidMode::P;
  Pid pid(p);
  PidInput in;
  in.sp = 22; in.pv = 21; in.dt_s = 2;
  for (int k = 0; k < 100; ++k) pid.step(in);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pid.integrator());
  TEST_ASSERT_EQUAL_FLOAT(20.0f, pid.last().output);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_saturation_overshoot);
  RUN_TEST(test_bumpless_manual_to_auto);
  RUN_TEST(test_bumpless_gain_change);
  RUN_TEST(test_bumpless_mode_change);
  RUN_TEST(test_deadband_integrator_drift);
  RUN_TEST(test_ts_independence);
  RUN_TEST(test_conditional_integration);
  RUN_TEST(test_dynamic_out_max);
  RUN_TEST(test_back_calculation);
  RUN_TEST(test_hold_preserves_integrator);
  RUN_TEST(test_tracking_and_fault_reset);
  RUN_TEST(test_derivative_on_measurement);
  RUN_TEST(test_setpoint_weight);
  RUN_TEST(test_onoff_hysteresis);
  RUN_TEST(test_invalid_pv_gives_zero);
  RUN_TEST(test_p_mode_no_integration);
  return UNITY_END();
}
