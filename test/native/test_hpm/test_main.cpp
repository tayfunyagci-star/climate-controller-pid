// Heating Performance Monitor — SAFETY_DESIGN §6 (metrikler ve 6 kural; S9 Safety'de).
#include <unity.h>
#include "cc_hpm.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static HpmOutput run(HeatingPerformanceMonitor& h, HpmInput in, uint32_t s, float t1_rate_per_min = 0) {
  HpmOutput o;
  for (uint32_t i = 0; i < s; ++i) {
    o = h.step(in);
    in.t1 += t1_rate_per_min / 60.0f;
  }
  return o;
}

static HpmInput heating(float t1, float demand) {
  HpmInput in;
  in.dt_ms = 1000;
  in.t1 = t1;
  in.sp = 22;
  in.demand = demand;
  in.heating = demand > 0;
  in.stage = demand > 50 ? 2 : (demand > 0 ? 1 : 0);
  return in;
}

void test_temperature_rate_regression() {
  HeatingPerformanceMonitor h;
  HpmOutput o = run(h, heating(18, 50), 600, 0.05f);  // 3 °C/sa
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.0f, o.temperature_rate);
}

// HEATING_PERFORMANCE_LOW: talep ≥ 80 % ≥ 10 dk, artış < 0.2 °C, T1 < SP − 1
void test_performance_low() {
  HeatingPerformanceMonitor h;
  HpmOutput o = run(h, heating(18, 90), 590);
  TEST_ASSERT_FALSE(o.performance_low);
  o = run(h, heating(18, 90), 20);
  TEST_ASSERT_TRUE(o.performance_low);
  HeatingPerformanceMonitor ok;
  o = run(ok, heating(18, 90), 700, 0.05f);  // 0.5 °C/10 dk artıyor
  TEST_ASSERT_FALSE(o.performance_low);
}

// LONG_HEATING_DURATION: tek dönem > 120 dk
void test_long_heating() {
  HeatingPerformanceMonitor h;
  HpmOutput o = run(h, heating(20, 30), 120 * 60 - 1);
  TEST_ASSERT_FALSE(o.long_heating);
  o = run(h, heating(20, 30), 2);
  TEST_ASSERT_TRUE(o.long_heating);
}

// FREQUENT_CYCLING: > 6 döngü/sa (röle profilinde > 3)
void test_frequent_cycling() {
  HeatingPerformanceMonitor h;
  HpmOutput o;
  for (int c = 0; c < 7; ++c) {
    o = run(h, heating(20, 30), 60);
    o = run(h, heating(20, 0), 240);
  }
  TEST_ASSERT_EQUAL_UINT16(7, o.cycles_1h);
  TEST_ASSERT_TRUE(o.frequent_cycling);
  HeatingPerformanceMonitor r(HpmParams::fromConfig(true, 240));
  for (int c = 0; c < 4; ++c) { run(r, heating(20, 30), 60); o = run(r, heating(20, 0), 240); }
  TEST_ASSERT_TRUE(o.frequent_cycling);
}

// Duty 1 sa / 24 sa ve EXCESSIVE_DUTY (> 85 %, 24 sa veri)
void test_duty_and_excessive() {
  HeatingPerformanceMonitor h;
  HpmOutput o = run(h, heating(20, 90), 3600);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, o.duty_1h);
  TEST_ASSERT_FALSE(o.excessive_duty);
  o = run(h, heating(20, 90), 23 * 3600);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, o.duty_24h);
  TEST_ASSERT_TRUE(o.excessive_duty);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, o.stage2_ratio_24h);
}

// Günlük ısıtma dakikası ve gün devri
void test_heating_minutes_today() {
  HeatingPerformanceMonitor h;
  HpmOutput o = run(h, heating(20, 30), 30 * 60);
  TEST_ASSERT_EQUAL_UINT32(30, o.heating_minutes_today);
  HpmInput in = heating(20, 0);
  in.day_rollover = true;
  o = h.step(in);
  TEST_ASSERT_EQUAL_UINT32(0, o.heating_minutes_today);
}

// Verim indeksi ve CAPACITY_DEGRADATION (14 gün referans, son 7 gün %30 düşük)
void test_efficiency_and_capacity() {
  HeatingPerformanceMonitor h;
  HpmOutput o;
  for (int day = 0; day < 21; ++day) {
    const float rate = day < 14 ? 0.10f : 0.05f;  // °C / dk @ 50 % talep
    o = run(h, heating(15, 50), 20 * 60, rate);  // 20 dk ısıtma
    HpmInput idle = heating(15 + rate * 20.0f, 0);
    o = h.step(idle);
    idle.day_rollover = true;
    idle.time_valid = true;
    o = h.step(idle);
  }
  // 0.10 °C/dk / 0.5 tam güç = 0.2 °C / tam güç dk
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.2f, o.efficiency_ref);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.1f, o.efficiency_recent);
  HpmInput last = heating(15, 0);
  last.time_valid = true;
  o = h.step(last);
  TEST_ASSERT_TRUE(o.capacity_degradation);
  last.time_valid = false;
  TEST_ASSERT_FALSE(h.step(last).capacity_degradation);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_temperature_rate_regression);
  RUN_TEST(test_performance_low);
  RUN_TEST(test_long_heating);
  RUN_TEST(test_frequent_cycling);
  RUN_TEST(test_duty_and_excessive);
  RUN_TEST(test_heating_minutes_today);
  RUN_TEST(test_efficiency_and_capacity);
  return UNITY_END();
}
