// F2 görev modeli — ClimateCore aşamalarının ayrı görevlerden (Safety 250 ms, Output 100 ms,
// Control 2 s, Sensor 2 s / DHT22) tek kilit altında, rastgele faz ve gecikmeyle çağrılması.
// Hedefteki src/app/tasks.cpp ile aynı çağrı sırası: Safety = baseTick + safetyStep + publish.
#include <unity.h>
#include "support/sim.h"

using namespace cc;

void setUp() {}
void tearDown() {}

namespace {

struct Task {
  uint32_t period, next;
  uint32_t last = 0;  // son çalışma zamanı (dt ölçümü)
};

// 10 ms kuantumla ilerleyen zamanlayıcı; aynı anda hazır görevler öncelik sırasıyla çalışır.
struct TaskedRunner {
  ClimateCore core;
  sim::Cabin cabin;
  sim::InvariantChecker inv;
  sim::Rng rng{1};
  uint32_t t = 0;
  Task saf{250, 0}, out{100, 0}, ctl{2000, 0}, sen{2000, 0};
  uint32_t jitter_ms = 0;          // her çevrime eklenebilecek azami gecikme
  bool stall_control = false, sensor_ok = true;
  float t1_override = NAN;
  bool outs[OUT_COUNT] = {false, false, false, false};

  void boot(const Config& c, uint64_t seed) {
    rng = sim::Rng(seed);
    core.begin(c, BootInfo());
    inv.prestart_ms = (uint32_t)c.fan_prestart_s * 1000u;
    inv.postcool_ms = (uint32_t)c.post_cool_seconds * 1000u;
    // İlk çalışma: görevler oluşturulduğu anda (dt = 0 ilk çağrıda atlanır), sonra rastgele faz
    saf.next = 10 + rng.below(250) / 10 * 10;
    out.next = 10 + rng.below(100) / 10 * 10;
    ctl.next = 10 + rng.below(2000) / 10 * 10;
    sen.next = 10 + rng.below(2000) / 10 * 10;
  }
  // Hedefteki görev döngüsü: periyot hedefi + gecikme; geç kalan görev yetişme patlaması yapmaz,
  // bir kez çalışır ve gerçek geçen süreyi (dt) aşamaya verir.
  uint32_t due(Task& k) {
    if (t < k.next) return 0;
    const uint32_t dt = t - k.last;
    k.last = t;
    const uint32_t late = jitter_ms ? rng.below(jitter_ms + 1) / 10 * 10 : 0;
    k.next = t + k.period + late;
    return dt;
  }
  void step10() {
    // Öncelik: Safety > Output > Control > Sensor
    if (uint32_t dt = due(saf)) { core.baseTick(dt); core.safetyStep(dt); core.publish(); }
    if (uint32_t dt = due(out)) {
      core.outputStep(dt);
      for (uint8_t k = 0; k < OUT_COUNT; ++k) outs[k] = core.outputs()[k];  // GPIO yazımı kilit dışında
    }
    if (uint32_t dt = due(ctl)) { if (!stall_control) core.controlStep(dt); }
    if (due(sen)) {
      if (sensor_ok) {
        core.feedT1(DrvStatus::OK, std::isnan(t1_override) ? cabin.T : t1_override);
        core.feedRh(DrvStatus::OK, cabin.rh);
      } else {
        core.feedT1(DrvStatus::TIMEOUT, NAN);
        core.feedRh(DrvStatus::TIMEOUT, NAN);
      }
    }
    for (int i = 0; i < 10; ++i) cabin.step(0.001f, outs[R1], outs[R2], outs[HF], outs[VF]);
    inv.check(outs, 10);
    t += 10;
  }
  void run(float s) { for (uint32_t n = (uint32_t)(s * 100); n > 0; --n) step10(); }
  template <typename F>
  float runUntil(F cond, float max_s) {
    const uint32_t t0 = t;
    for (uint32_t n = (uint32_t)(max_s * 100); n > 0; --n) { step10(); if (cond()) return (t - t0) / 1000.0f; }
    return -1;
  }
};

Config f2cfg() {
  Config c;
  c.sensor_interval_s = 2;          // DHT22: ≥ 2 s
  c.heater_power_w_r1 = 1000;
  c.heater_power_w_r2 = 1000;
  c.temperature_setpoint = 22;
  c.setpoint_ramp_c_per_min = 0;
  return c;
}

}  // namespace

// Varsayılan F2 konfigürasyonu doğrulamadan geçer
void test_f2_config_valid() {
  TEST_ASSERT_TRUE(validate(f2cfg()).ok());
}

// 2 s sensör aralığıyla SELF_TEST 10 s içinde geçer, ısıtma sırası (HF → prestart → R1) korunur
void test_boot_and_heat_with_split_tasks() {
  TaskedRunner r;
  r.cabin.T = 17;
  r.boot(f2cfg(), 7);
  TEST_ASSERT_TRUE(r.runUntil([&] { return r.core.sysState() == SysState::RUN; }, 12) >= 0);
  TEST_ASSERT_TRUE(r.runUntil([&] { return r.outs[R1]; }, 30) >= 0);
  TEST_ASSERT_TRUE(r.outs[HF]);
  r.run(3600);
  TEST_ASSERT_FLOAT_WITHIN(0.7f, 22.0f, r.cabin.T);
  TEST_ASSERT_TRUE(r.inv.ok());
}

// Control görevi durursa heartbeat (6 s) → INTERNAL_FAULT → rezistans OFF, HF post-cool
void test_control_stall_detected() {
  TaskedRunner r;
  r.cabin.T = 17;
  r.boot(f2cfg(), 11);
  TEST_ASSERT_TRUE(r.runUntil([&] { return r.outs[R1]; }, 40) >= 0);
  r.stall_control = true;
  const float tf = r.runUntil([&] { return r.core.sysState() == SysState::FAILSAFE; }, 20);
  TEST_ASSERT_TRUE(tf > 0 && tf <= 7.5f);
  TEST_ASSERT_TRUE(r.runUntil([&] { return !r.outs[R1] && !r.outs[R2]; }, 1.0f) >= 0);
  TEST_ASSERT_TRUE(r.outs[HF]);
  TEST_ASSERT_TRUE(r.inv.ok());
}

// DHT22 kopması: 3 hata → BAD → sensör arızası güvenli durumu; R OFF
void test_sensor_loss_with_2s_interval() {
  TaskedRunner r;
  r.cabin.T = 17;
  r.boot(f2cfg(), 13);
  TEST_ASSERT_TRUE(r.runUntil([&] { return r.outs[R1]; }, 40) >= 0);
  r.sensor_ok = false;
  const float tf = r.runUntil([&] { return !r.outs[R1] && !r.outs[R2]; }, 20);
  TEST_ASSERT_TRUE(tf > 0 && tf <= 12.0f);   // sensor_stale_s 10 s içinde
  TEST_ASSERT_TRUE(r.runUntil([&] { return r.core.snapshot().controller_state == CtrlState::FAILSAFE; }, 15) >= 0);
  TEST_ASSERT_EQUAL(FailsafeReason::SENSOR_FAULT, r.core.snapshot().failsafe_reason);
  TEST_ASSERT_TRUE(r.outs[HF]);  // post-cool
  TEST_ASSERT_TRUE(r.inv.ok());
}

// Özellik: rastgele faz + görev gecikmesi (≤ 1 periyot) altında çıkış değişmezleri
void test_property_jitter_invariants() {
  for (uint64_t seed = 1; seed <= 16; ++seed) {
    sim::Rng g(seed);
    TaskedRunner r;
    r.cabin.T = g.uniform(3, 23);
    r.cabin.T_out = g.uniform(-10, 10);
    r.jitter_ms = 50 + g.below(250);
    Config c = f2cfg();
    c.fan_prestart_s = 1 + (int32_t)g.below(6);
    c.post_cool_seconds = 30 + (int32_t)g.below(90);
    r.boot(c, seed + 100);
    for (int k = 0; k < 2 * 3600 * 100; ++k) {
      if (g.chance(1)) r.core.command("temperature_setpoint", g.chance(500) ? "25" : "12", CmdSource::LOCAL_WEB);
      if (g.chance(1)) r.core.command("operating_mode", g.chance(800) ? "AUTO" : "OFF", CmdSource::LOCAL_WEB);
      if (!(k % 30000)) r.sensor_ok = !g.chance(150);
      if (!(k % 45000)) r.t1_override = g.chance(100) ? 45.0f : NAN;
      r.step10();
      if (r.core.guardViolationSeen()) break;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "seed %llu: rhf=%u prestart=%u postcool=%u guard=%d", (unsigned long long)seed,
             r.inv.violations_rhf, r.inv.violations_prestart, r.inv.violations_postcool, r.core.guardViolationSeen());
    TEST_ASSERT_TRUE_MESSAGE(r.inv.ok() && !r.core.guardViolationSeen(), msg);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_f2_config_valid);
  RUN_TEST(test_boot_and_heat_with_split_tasks);
  RUN_TEST(test_control_stall_detected);
  RUN_TEST(test_sensor_loss_with_2s_interval);
  RUN_TEST(test_property_jitter_invariants);
  return UNITY_END();
}
