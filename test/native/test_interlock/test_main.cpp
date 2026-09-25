// InterlockEngine + OutputGuard — OUTPUT_AND_INTERLOCKS §5–§6, SR-01..SR-04.
// Özellik testi: rastgele komut/safety/sensör dizileri × zaman; (R1∨R2)⇒HF, prestart, post-cool.
#include <unity.h>
#include "cc_interlock.h"
#include "support/sim.h"

using namespace cc;

void setUp() {}
void tearDown() {}

struct IL {
  InterlockEngine il;
  InterlockInput in;
  InterlockOutput o;
  IL() { in.arm = true; }
  InterlockOutput run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 100) o = il.step(in);
    return o;
  }
};

static void heat(IL& x, bool r1, bool r2) {
  x.in.heat_chain = r1 || r2;
  x.in.r_req[0] = r1;
  x.in.r_req[1] = r2;
}

// A1 / SR-02: fan ON → 3 s sonra R1; prepurge
void test_prestart() {
  IL x;
  heat(x, true, false);
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_EQUAL(Reason::FAN_PRESTART, o.reason[R1]);
  TEST_ASSERT_TRUE(o.prepurge);
  TEST_ASSERT_EQUAL(Reason::PREPURGE, o.reason[HF]);
  o = x.run(2800);
  TEST_ASSERT_FALSE(o.eff[R1]);
  o = x.run(200);
  TEST_ASSERT_TRUE(o.eff[R1]);
  TEST_ASSERT_EQUAL(Reason::HEATER_INTERLOCK, o.reason[HF]);
  TEST_ASSERT_FALSE(o.prepurge);
}

// A2 / SR-03: kapanışta post-cool 60 s, sonra HF OFF
void test_post_cool_time() {
  IL x;
  heat(x, true, false);
  x.run(10000);
  heat(x, false, false);
  InterlockOutput o = x.run(100);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_TRUE(o.post_cool);
  TEST_ASSERT_EQUAL(Reason::POST_COOL, o.reason[HF]);
  o = x.run(59800);
  TEST_ASSERT_TRUE(o.eff[HF]);
  o = x.run(200);
  TEST_ASSERT_FALSE(o.post_cool);
  TEST_ASSERT_FALSE(o.eff[HF]);
}

// Post-cool sırasında yeni talep: prepurge olmadan aktif
void test_post_cool_reheat_without_prepurge() {
  IL x;
  heat(x, true, false);
  x.run(10000);
  heat(x, false, false);
  x.run(20000);
  heat(x, true, false);
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[R1]);
  TEST_ASSERT_FALSE(o.post_cool);
}

// I-1: Safety kilidi → R anında OFF (min ON beklenmez), post-cool sürer (FAILSAFE dahil)
void test_I1_lockout_immediate_off_with_postcool() {
  IL x;
  x.il.setParams([] { InterlockParams p; p.heater_min_on_ms = 120000; return p; }());
  heat(x, true, true);
  x.run(5000);  // R'ler 2 s açık, min ON 120 s dolmadı
  TEST_ASSERT_TRUE(x.o.eff[R1]);
  x.in.heater_lockout = true;
  x.in.lockout_reason = Reason::SENSOR_FAULT;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_FALSE(o.eff[R2]);
  TEST_ASSERT_EQUAL(Reason::SENSOR_FAULT, o.reason[R1]);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_TRUE(o.post_cool);
  // ARM=0 da aynı etki
  IL y;
  heat(y, true, false);
  y.run(5000);
  y.in.arm = false;
  y.in.lockout_reason = Reason::OTA;
  o = y.run(100);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_EQUAL(Reason::OTA, o.reason[R1]);
}

// I-2 / A3: kullanıcı R1 aktifken HF OFF ister → HF ON, HEATER_INTERLOCK
void test_I2_hf_off_request_overridden() {
  IL x;
  x.in.hf_req = true;
  x.run(70000);
  heat(x, true, false);
  x.run(5000);
  x.in.hf_req = false;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_EQUAL(Reason::HEATER_INTERLOCK, o.reason[HF]);
}

// §5.3: R'ler kapandı, 40 s geçti → HF ON, POST_COOL
void test_example_postcool_40s() {
  IL x;
  heat(x, true, false);
  x.run(10000);
  heat(x, false, false);
  InterlockOutput o = x.run(40000);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_EQUAL(Reason::POST_COOL, o.reason[HF]);
}

// I-4: aşırı sıcaklık, kullanıcı vent OFF → VF ON, OVERTEMPERATURE (min OFF beklenmez)
void test_I4_overtemp_forces_vent() {
  IL x;
  x.in.vf_force_on = true;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[VF]);
  TEST_ASSERT_EQUAL(Reason::OVERTEMPERATURE, o.reason[VF]);
}

// I-5: donma koruması, kullanıcı vent ON → VF OFF, ANTIFREEZE_INHIBIT (min ON beklenmez)
void test_I5_antifreeze_inhibits_vent() {
  IL x;
  x.in.vf_req = true;
  x.run(120000);
  TEST_ASSERT_TRUE(x.o.eff[VF]);
  x.in.antifreeze = true;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_FALSE(o.eff[VF]);
  TEST_ASSERT_EQUAL(Reason::ANTIFREEZE_INHIBIT, o.reason[VF]);
  // I-4 önceliklidir
  x.in.vf_force_on = true;
  o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[VF]);
}

// I-6: koordinasyon inhibit
void test_I6_coordination() {
  IL x;
  x.in.vf_req = true;
  x.in.vf_inhibit = Reason::HEATING_PRIORITY;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_FALSE(o.eff[VF]);
  TEST_ASSERT_EQUAL(Reason::HEATING_PRIORITY, o.reason[VF]);
}

// I-7 / §5.3: talep düştü, R1 90 s önce açıldı (röle, min ON 120 s) → R1 ON, MIN_ON_TIME
void test_I7_min_on_relay() {
  IL x;
  InterlockParams p;
  p.heater_min_on_ms = 120000;
  p.heater_min_off_ms = 180000;
  x.il.setParams(p);
  heat(x, true, false);
  x.run(3100 + 90000);
  heat(x, false, false);
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[R1]);
  TEST_ASSERT_EQUAL(Reason::MIN_ON_TIME, o.reason[R1]);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_FALSE(o.post_cool);  // R açıkken post-cool başlamaz
  o = x.run(30000);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_TRUE(o.post_cool);
  // I-8: min OFF dolmadan yeniden açılmaz
  heat(x, true, false);
  o = x.run(100);
  TEST_ASSERT_FALSE(o.eff[R1]);
  TEST_ASSERT_EQUAL(Reason::MIN_OFF_TIME, o.reason[R1]);
  o = x.run(180000);
  TEST_ASSERT_TRUE(o.eff[R1]);
}

// HF güvenlik açılışı min OFF'u beklemez; manuel HF min ON/OFF uygulanır
void test_hf_min_times() {
  IL x;
  x.in.hf_req = true;
  x.run(1000);
  x.in.hf_req = false;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_EQUAL(Reason::MIN_ON_TIME, o.reason[HF]);
  o = x.run(60000);
  TEST_ASSERT_FALSE(o.eff[HF]);
  heat(x, true, false);  // min OFF 30 s dolmadı ama I-2 zorlar
  o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
}

// §6 TEMPERATURE / HYBRID post-cool (T2) ve zaman aşımı uyarısı
void test_post_cool_temperature_modes() {
  IL x;
  InterlockParams p;
  p.pc_mode = PostCoolMode::TEMPERATURE;
  x.il.setParams(p);
  x.in.t2_q = Quality::GOOD;
  x.in.t2 = 60;
  heat(x, true, false);
  x.run(10000);
  heat(x, false, false);
  InterlockOutput o = x.run(30000);
  TEST_ASSERT_TRUE(o.post_cool);
  x.in.t2 = 35;  // < 40 ve ≥ 20 s
  o = x.run(100);
  TEST_ASSERT_FALSE(o.post_cool);
  // T2 düşmezse: max sonrası timeout uyarısı, fan açık kalır
  IL y;
  y.il.setParams(p);
  y.in.t2_q = Quality::GOOD;
  y.in.t2 = 60;
  heat(y, true, false);
  y.run(10000);
  heat(y, false, false);
  o = y.run(601000);
  TEST_ASSERT_TRUE(o.post_cool);
  TEST_ASSERT_TRUE(o.post_cool_timeout);
  TEST_ASSERT_TRUE(o.eff[HF]);
  // HYBRID: zaman + T2
  IL z;
  p.pc_mode = PostCoolMode::HYBRID;
  z.il.setParams(p);
  z.in.t2_q = Quality::GOOD;
  z.in.t2 = 30;
  heat(z, true, false);
  z.run(10000);
  heat(z, false, false);
  o = z.run(30000);
  TEST_ASSERT_TRUE(o.post_cool);  // T2 iyi ama 60 s dolmadı
  o = z.run(31000);
  TEST_ASSERT_FALSE(o.post_cool);
}

// D-20: boot post-cool
void test_boot_post_cool() {
  IL x;
  x.il.startBootPostCool();
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_EQUAL(Reason::BOOT_POST_COOL, o.reason[HF]);
  o = x.run(60000);
  TEST_ASSERT_FALSE(o.eff[HF]);
}

// S2: T2 aşırı sıcaklık HF'yi açık tutar
void test_hf_force_safety() {
  IL x;
  x.in.hf_force_on = true;
  InterlockOutput o = x.run(100);
  TEST_ASSERT_TRUE(o.eff[HF]);
  TEST_ASSERT_EQUAL(Reason::OVERTEMPERATURE, o.reason[HF]);
}

// R yalnız ısıtma zinciri talebiyle (heat_chain) açılabilir
void test_r_requires_chain() {
  IL x;
  x.in.r_req[0] = true;
  x.in.heat_chain = false;
  InterlockOutput o = x.run(10000);
  TEST_ASSERT_FALSE(o.eff[R1]);
}

// OutputGuard: ikinci kontrol ve sıralı uygulama
void test_output_guard() {
  OutputGuard g;
  bool d[4] = {true, false, true, false};  // R1 + HF aynı tikte
  OutputGuard::Result r = g.apply(d, 100);
  TEST_ASSERT_FALSE(r.out[R1]);  // HF önceki tikte kapalıydı
  TEST_ASSERT_TRUE(r.out[HF]);
  r = g.apply(d, 100);
  TEST_ASSERT_TRUE(r.out[R1]);
  bool bad[4] = {true, false, false, false};  // ihlal: R1 ∧ ¬HF
  r = g.apply(bad, 100);
  TEST_ASSERT_TRUE(r.violation);
  TEST_ASSERT_FALSE(r.out[R1]);
  TEST_ASSERT_TRUE(r.out[HF]);  // güvenli yön
  // kapanışta önce R: R ve HF aynı tikte kapanırsa HF bir tik daha açık
  OutputGuard g2;
  bool on[4] = {true, false, true, false};
  g2.apply(on, 100);
  g2.apply(on, 100);
  bool off[4] = {false, false, false, false};
  r = g2.apply(off, 100);
  TEST_ASSERT_FALSE(r.out[R1]);
  TEST_ASSERT_TRUE(r.out[HF]);
  r = g2.apply(off, 100);
  TEST_ASSERT_FALSE(r.out[HF]);
  TEST_ASSERT_EQUAL_UINT32(1, g2.switchCount(R1));
}

// Özellik testi: rastgele girdi dizileri × zaman
void test_property_random_sequences() {
  uint32_t totalSteps = 0, rOnSteps = 0;
  for (uint64_t seed = 1; seed <= 200; ++seed) {
    sim::Rng rng(seed);
    InterlockEngine il;
    InterlockParams p;
    const bool relay = rng.chance(300);
    if (relay) { p.heater_min_on_ms = 120000; p.heater_min_off_ms = 180000; }
    p.prestart_ms = rng.below(6) * 1000u;
    p.pc_ms = (30 + rng.below(90)) * 1000u;
    il.setParams(p);
    if (rng.chance(200)) il.startBootPostCool();
    OutputGuard g;
    sim::InvariantChecker inv;
    inv.prestart_ms = p.prestart_ms;
    inv.postcool_ms = p.pc_ms;
    InterlockInput in;
    in.arm = true;
    for (int k = 0; k < 20000; ++k) {  // 2000 s @ 100 ms
      if (rng.chance(20)) in.heat_chain = rng.chance(600);
      if (rng.chance(30)) in.r_req[0] = rng.chance(600);
      if (rng.chance(30)) in.r_req[1] = rng.chance(500);
      if (rng.chance(10)) in.hf_req = rng.chance(300);
      if (rng.chance(10)) in.vf_req = rng.chance(400);
      if (rng.chance(5)) in.heater_lockout = rng.chance(200);
      if (rng.chance(5)) in.arm = !rng.chance(150);
      if (rng.chance(5)) in.vf_force_on = rng.chance(100);
      if (rng.chance(5)) in.hf_force_on = rng.chance(50);
      if (rng.chance(5)) in.antifreeze = rng.chance(200);
      if (rng.chance(5)) in.vf_inhibit = rng.chance(300) ? Reason::HEATING_PRIORITY : Reason::NONE;
      const InterlockOutput o = il.step(in);
      // Motor çıktısı değişmezi (guard öncesi)
      TEST_ASSERT_FALSE_MESSAGE((o.eff[R1] || o.eff[R2]) && !o.eff[HF], "interlock: R ∧ ¬HF");
      TEST_ASSERT_FALSE_MESSAGE((o.eff[R1] || o.eff[R2]) && (in.heater_lockout || !in.arm), "kilitliyken R");
      const OutputGuard::Result r = g.apply(o.eff, 100);
      TEST_ASSERT_FALSE_MESSAGE(r.violation, "guard ihlali");
      inv.check(r.out, 100);
      ++totalSteps;
      if (r.out[R1] || r.out[R2]) ++rOnSteps;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "seed %llu: rhf=%u prestart=%u postcool=%u", (unsigned long long)seed,
             inv.violations_rhf, inv.violations_prestart, inv.violations_postcool);
    TEST_ASSERT_TRUE_MESSAGE(inv.ok(), msg);
  }
  TEST_ASSERT_GREATER_THAN_UINT32(totalSteps / 50, rOnSteps);  // test gerçekten ısıtma içerdi
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_prestart);
  RUN_TEST(test_post_cool_time);
  RUN_TEST(test_post_cool_reheat_without_prepurge);
  RUN_TEST(test_I1_lockout_immediate_off_with_postcool);
  RUN_TEST(test_I2_hf_off_request_overridden);
  RUN_TEST(test_example_postcool_40s);
  RUN_TEST(test_I4_overtemp_forces_vent);
  RUN_TEST(test_I5_antifreeze_inhibits_vent);
  RUN_TEST(test_I6_coordination);
  RUN_TEST(test_I7_min_on_relay);
  RUN_TEST(test_hf_min_times);
  RUN_TEST(test_post_cool_temperature_modes);
  RUN_TEST(test_boot_post_cool);
  RUN_TEST(test_hf_force_safety);
  RUN_TEST(test_r_requires_chain);
  RUN_TEST(test_output_guard);
  RUN_TEST(test_property_random_sequences);
  return UNITY_END();
}
