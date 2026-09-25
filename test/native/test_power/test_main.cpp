// PowerManager — OUTPUT_AND_INTERLOCKS §3–§4, ADR-002.
#include <unity.h>
#include "cc_power.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static PowerOutput run(PowerManager& pm, float d, uint32_t ms, bool rotate = false) {
  PowerOutput o;
  for (uint32_t t = 0; t < ms; t += 100) {
    PowerInput in;
    in.demand = d;
    in.enabled = true;
    in.rotate = rotate && t == 0;
    o = pm.step(in);
  }
  return o;
}

// Bir pencere boyunca kanal ON süresi (ms)
static uint32_t onTime(PowerManager& pm, float d, int ch, uint32_t window_ms) {
  uint32_t on = 0;
  for (uint32_t t = 0; t < window_ms; t += 100) {
    PowerInput in;
    in.demand = d;
    in.enabled = true;
    if (pm.step(in).req[ch]) on += 100;
  }
  return on;
}

// §3.1 eşit güç tablosu (kademe 2'ye dwell sonrası)
void test_equal_power_table() {
  struct Row { float d; uint8_t stage; float r1, r2; };
  const Row rows[] = {{0, 0, 0, 0}, {10, 1, 20, 0}, {25, 1, 50, 0}, {50, 1, 100, 0},
                      {75, 2, 100, 50}, {90, 2, 100, 80}, {100, 2, 100, 100}};
  for (const Row& r : rows) {
    PowerManager pm;
    PowerOutput o = run(pm, r.d, 200000);  // 200 s > dwell 120 s
    TEST_ASSERT_EQUAL_UINT8(r.stage, o.stage);
    TEST_ASSERT_EQUAL_FLOAT(r.r1, o.duty[0]);
    TEST_ASSERT_EQUAL_FLOAT(r.r2, o.duty[1]);
  }
}

// §3.2 kademe histerezisi 55/45 + min kalış 120 s
void test_stage_hysteresis_and_dwell() {
  PowerManager pm;
  PowerOutput o = run(pm, 60, 100000);  // 100 s < dwell
  TEST_ASSERT_EQUAL_UINT8(1, o.stage);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, o.duty[0]);  // kademe 1'de R1 %100'e kıstırılır
  o = run(pm, 60, 25000);
  TEST_ASSERT_EQUAL_UINT8(2, o.stage);
  o = run(pm, 47, 300000);  // bantta: kademe korunur
  TEST_ASSERT_EQUAL_UINT8(2, o.stage);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.duty[1]);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 47.0f, o.applied);  // ±5 % sapma
  o = run(pm, 45, 100);
  TEST_ASSERT_EQUAL_UINT8(1, o.stage);
  o = run(pm, 54, 300000);  // 1'de bant içi: 2'ye geçmez
  TEST_ASSERT_EQUAL_UINT8(1, o.stage);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 54.0f, o.applied);
  o = run(pm, 55, 100);
  TEST_ASSERT_EQUAL_UINT8(2, o.stage);
  o = run(pm, 44, 60000);  // dwell dolmadan inemez
  TEST_ASSERT_EQUAL_UINT8(2, o.stage);
  o = run(pm, 0, 100);  // talep 0: anında kademe 0 (güvenli yön)
  TEST_ASSERT_EQUAL_UINT8(0, o.stage);
  TEST_ASSERT_FALSE(o.req[0]);
  TEST_ASSERT_FALSE(o.req[1]);
}

// Zaman-oransal: SSR_ZC 20 s pencere, duty 30 % → 6 s ON
void test_time_proportional_ssr() {
  PowerManager pm;
  run(pm, 15, 20000);  // lider duty 30 %
  const uint32_t on = onTime(pm, 15, 0, 20000);
  TEST_ASSERT_UINT32_WITHIN(100, 6000, on);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 15.0f, pm.last().applied);
}

// Min darbe: 1 s altı ON veya OFF üretilmez (%5 çözünürlük)
void test_min_pulse() {
  PowerManager pm;
  run(pm, 1.5f, 40000);  // lider 3 % → 0.6 s < 1 s
  TEST_ASSERT_EQUAL_UINT32(0, onTime(pm, 1.5f, 0, 20000));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pm.last().applied);
  PowerManager pm2;
  run(pm2, 48.5f, 40000);  // lider 97 % → boşluk 0.6 s < 1 s → tam
  TEST_ASSERT_EQUAL_UINT32(20000, onTime(pm2, 48.5f, 0, 20000));
}

// RELAY profili: 600 s pencere, 120 s min darbe
void test_relay_profile() {
  PowerParams p;
  p.driver = DriverKind::RELAY;
  p.window_ms = 600000;
  p.min_on_ms = 120000;
  p.min_off_ms = 180000;
  PowerManager pm(p);
  run(pm, 5, 1000);  // lider 10 % → 60 s < 120 s → 0
  TEST_ASSERT_EQUAL_FLOAT(0.0f, pm.last().applied);
  PowerManager pm2(p);
  run(pm2, 15, 100);  // lider 30 % → 180 s ON
  TEST_ASSERT_UINT32_WITHIN(100, 180000, onTime(pm2, 15, 0, 600000) + 100);
  // Pencere başına anahtarlama ≤ 1 → ≤ 6/sa
  PowerManager pm3(p);
  uint32_t sw = 0;
  bool prev = false;
  for (uint32_t t = 0; t < 3600000; t += 100) {
    PowerInput in; in.demand = 20; in.enabled = true;
    const bool r = pm3.step(in).req[0];
    if (r && !prev) ++sw;
    prev = r;
  }
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(6, sw);
}

// §3.3 farklı güç: küçük rezistans lider, kapasiteye göre eşleme
void test_unequal_power() {
  PowerParams p;
  p.power_w[0] = 2000;
  p.power_w[1] = 1000;
  PowerManager pm(p);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f / 3.0f, pm.stage1Capacity());
  PowerOutput o = run(pm, 20, 1000);
  TEST_ASSERT_EQUAL_UINT8(1, o.lead);  // R2 (1000 W) lider
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, o.duty[1]);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.duty[0]);
  o = run(pm, 80, 200000);
  TEST_ASSERT_EQUAL_UINT8(2, o.stage);
  TEST_ASSERT_EQUAL_FLOAT(100.0f, o.duty[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 70.0f, o.duty[0]);
}

// §3.4 lider rotasyonu yalnız talep 0 anında
void test_lead_rotation() {
  PowerManager pm;
  PowerOutput o = run(pm, 20, 1000, true);
  TEST_ASSERT_EQUAL_UINT8(0, o.lead);  // kademe 1 aktif: bekler
  o = run(pm, 0, 100);
  TEST_ASSERT_EQUAL_UINT8(1, o.lead);
  TEST_ASSERT_TRUE(o.ev_rotated);
  o = run(pm, 20, 1000);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, o.duty[1]);
  PowerParams off; off.rotation = LeadRotation::OFF;
  PowerManager pm2(off);
  run(pm2, 0, 100, true);
  TEST_ASSERT_EQUAL_UINT8(0, pm2.last().lead);
}

// Disabled → her şey kapalı
void test_disabled() {
  PowerManager pm;
  run(pm, 80, 200000);
  PowerInput in;
  in.demand = 80;
  in.enabled = false;
  PowerOutput o = pm.step(in);
  TEST_ASSERT_EQUAL_UINT8(0, o.stage);
  TEST_ASSERT_FALSE(o.req[0] || o.req[1]);
}

// §3.5 pencere fazı: iki kanalın açılışı aynı tike düşmez (rastgele talep dizisi)
void test_no_simultaneous_rising_edges() {
  PowerManager pm;
  uint32_t seed = 12345, coincident = 0, rises = 0;
  bool p0 = false, p1 = false;
  float d = 50;
  for (uint32_t t = 0; t < 6u * 3600u * 1000u; t += 100) {
    if (t % 30000 == 0) { seed = seed * 1103515245u + 12345u; d = (float)((seed >> 16) % 101); }
    PowerInput in; in.demand = d; in.enabled = true;
    const PowerOutput o = pm.step(in);
    const bool r0 = o.req[0] && !p0, r1 = o.req[1] && !p1;
    if (r0 && r1) ++coincident;
    rises += r0 + r1;
    p0 = o.req[0]; p1 = o.req[1];
  }
  TEST_ASSERT_GREATER_THAN_UINT32(100, rises);
  TEST_ASSERT_EQUAL_UINT32(0, coincident);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_equal_power_table);
  RUN_TEST(test_stage_hysteresis_and_dwell);
  RUN_TEST(test_time_proportional_ssr);
  RUN_TEST(test_min_pulse);
  RUN_TEST(test_relay_profile);
  RUN_TEST(test_unequal_power);
  RUN_TEST(test_lead_rotation);
  RUN_TEST(test_disabled);
  RUN_TEST(test_no_simultaneous_rising_edges);
  return UNITY_END();
}
