// Talep koşullandırma — PID_DESIGN §5, §9 (min talep, slew), CONTROL_ARCHITECTURE §3.4 (MANUAL + antifreeze).
#include <unity.h>
#include "cc_demand.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static DemandInput pidIn(float v) {
  DemandInput in;
  in.permitted = true;
  in.source = DemandSource::PID;
  in.pid_output = v;
  in.dt_s = 2;
  return in;
}

// §9-6: talep 3 % → 0; 7 %'ye çıkınca etkin (min 5 % + 2 % histerezis)
void test_min_demand_hysteresis() {
  DemandConditioner d;
  DemandOutput o = d.step(pidIn(3));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.heat_demand);
  o = d.step(pidIn(6));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.target);  // 5 ≤ 6 < 7: hâlâ 0
  o = d.step(pidIn(7));
  TEST_ASSERT_EQUAL_FLOAT(7.0f, o.target);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, o.heat_demand);
  for (int i = 0; i < 60; ++i) o = d.step(pidIn(7));
  TEST_ASSERT_EQUAL_FLOAT(7.0f, o.heat_demand);
  o = d.step(pidIn(5.5f));  // aktifken 5'e kadar sürer
  TEST_ASSERT_EQUAL_FLOAT(5.5f, o.heat_demand);
  o = d.step(pidIn(4.9f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.heat_demand);
}

// §9-7: artış ≤ 10 %/dk, azalış anında
void test_slew_rise_only() {
  DemandConditioner d;
  float prev = 0, maxRate = 0;
  for (int i = 0; i < 600; ++i) {  // 20 dk @ 2 s
    DemandOutput o = d.step(pidIn(100));
    const float rate = (o.heat_demand - prev) / (2.0f / 60.0f);
    if (rate > maxRate) maxRate = rate;
    prev = o.heat_demand;
  }
  TEST_ASSERT_EQUAL_FLOAT(100.0f, prev);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(10.0f + 1e-3f, maxRate);
  DemandOutput o = d.step(pidIn(20));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, o.heat_demand);  // azalış sınırsız
}

void test_max_and_cap() {
  DemandParams p; p.max_demand = 60; p.slew_pct_per_min = 100;
  DemandConditioner d(p);
  DemandOutput o;
  for (int i = 0; i < 60; ++i) o = d.step(pidIn(90));
  TEST_ASSERT_EQUAL_FLOAT(60.0f, o.heat_demand);
  TEST_ASSERT_TRUE(o.limited);
  DemandInput in = pidIn(90);
  in.cap = 0;  // VENT_WINS, vent_heat_cap 0
  o = d.step(in);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.heat_demand);
}

void test_not_permitted_is_zero() {
  DemandConditioner d;
  for (int i = 0; i < 200; ++i) d.step(pidIn(50));
  DemandInput in = pidIn(50);
  in.permitted = false;
  TEST_ASSERT_EQUAL_FLOAT(0.0f, d.step(in).heat_demand);
}

void test_manual_and_antifreeze_max() {
  DemandParams p; p.slew_pct_per_min = 100;
  DemandConditioner d(p);
  DemandInput in;
  in.permitted = true;
  in.source = DemandSource::MANUAL;
  in.manual_demand = 20;
  in.antifreeze_demand = 45;
  in.dt_s = 2;
  DemandOutput o;
  for (int i = 0; i < 60; ++i) o = d.step(in);
  TEST_ASSERT_EQUAL_FLOAT(45.0f, o.heat_demand);
  in.antifreeze_demand = kNaN;
  o = d.step(in);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, o.heat_demand);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_min_demand_hysteresis);
  RUN_TEST(test_slew_rise_only);
  RUN_TEST(test_max_and_cap);
  RUN_TEST(test_not_permitted_is_zero);
  RUN_TEST(test_manual_and_antifreeze_max);
  return UNITY_END();
}
