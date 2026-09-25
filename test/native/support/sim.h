// Native test yardımcıları: deterministik PRNG, kulübe termal modeli, ClimateCore koşucusu ve
// çıkış değişmezi denetçisi. Yalnız testlerde kullanılır.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cmath>
#include "cc_core.h"

namespace sim {

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 1) {}
  uint32_t next() {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return (uint32_t)(s >> 11);
  }
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }
  float uniform(float a, float b) { return a + (b - a) * (next() / 4294967296.0f); }
  bool chance(uint32_t per_mille) { return below(1000) < per_mille; }
};

// Birinci derece + ölü zaman kulübe modeli (rezistans ısısı yalnız fan açıkken odaya geçer).
struct Cabin {
  float T = 18.0f;         // °C
  float T_out = 0.0f;      // dış ortam
  float tau_s = 1800.0f;   // zaman sabiti
  float gain = 0.30f;      // °C / % (tam güç kararlı hâl artışı = 30 °C)
  float vent_loss = 0.5f;  // havalandırma açıkken ek kayıp çarpanı
  float dead_s = 60.0f;
  float q_[64] = {0};      // ölü zaman gecikme hattı (5 s kovalar)
  float q_acc = 0; float q_t = 0; int q_i = 0;
  float rh = 50.0f;
  void step(float dt_s, bool r1, bool r2, bool hf, bool vf) {
    float p = ((r1 ? 50.0f : 0.0f) + (r2 ? 50.0f : 0.0f)) * (hf ? 1.0f : 0.7f);
    q_acc += p * dt_s; q_t += dt_s;
    const int nb = (int)(dead_s / 5.0f);
    if (q_t >= 5.0f) {
      q_[q_i % 64] = q_acc / q_t; q_i++; q_acc = 0; q_t = 0;
    }
    const float delayed = (q_i > nb && nb > 0) ? q_[(q_i - nb) % 64] : (nb == 0 ? p : 0.0f);
    const float loss = (T - T_out) * (1.0f + (vf ? vent_loss : 0.0f));
    T += dt_s * (gain * delayed - loss) / tau_s;
  }
};

// Çıkış değişmezleri: (R1∨R2) ⇒ HF, prestart ≥ fan_prestart_s, her kapanıştan sonra post-cool
struct InvariantChecker {
  uint32_t violations_rhf = 0, violations_prestart = 0, violations_postcool = 0;
  uint64_t t_ms = 0;
  uint64_t hf_on_since = 0; bool hf = false;
  uint64_t last_r_on_ms = 0; bool r_seen = false;
  bool prevR = false;
  uint32_t prestart_ms = 3000, postcool_ms = 60000;
  bool boot_postcool_ok = true;
  void check(const bool* o, uint32_t dt_ms) {
    t_ms += dt_ms;
    const bool r = o[cc::R1] || o[cc::R2];
    if (r && !o[cc::HF]) ++violations_rhf;
    if (o[cc::HF] && !hf) hf_on_since = t_ms;
    if (!o[cc::HF] && hf && r_seen) {
      // HF kapanıyor: son R kapanışından bu yana post-cool süresi geçmiş olmalı
      if (t_ms - last_r_on_ms < postcool_ms) ++violations_postcool;
    }
    if (r && !prevR) {
      if (!hf || t_ms - hf_on_since < prestart_ms) ++violations_prestart;
    }
    if (r) { last_r_on_ms = t_ms; r_seen = true; }
    hf = o[cc::HF];
    prevR = r;
  }
  bool ok() const { return !violations_rhf && !violations_prestart && !violations_postcool; }
};

struct Runner {
  cc::ClimateCore core;
  Cabin cabin;
  InvariantChecker inv;
  uint32_t t_ms = 0, feed_acc = 0;
  bool sensor_connected = true;
  bool sensor_frozen = false;       // değer sabit (stale değil; aynı değer)
  float noise = 0.0f;
  float t1_override = NAN;          // NaN değilse sensör bu değeri okur
  Rng rng{1};
  bool clock = false;               // duvar saati geçerli mi
  int64_t epoch0 = 0;               // UTC epoch (t_ms = 0 anı)

  void begin(const cc::Config& c, const cc::BootInfo& b = cc::BootInfo()) {
    core.begin(c, b);
    inv.prestart_ms = (uint32_t)c.fan_prestart_s * 1000u;
    inv.postcool_ms = (uint32_t)c.post_cool_seconds * 1000u;
  }
  float reading() {
    float v = std::isnan(t1_override) ? cabin.T : t1_override;
    if (noise > 0) v += rng.uniform(-noise, noise);
    return v;
  }
  void step() {
    const uint32_t dt = cc::ClimateCore::kBaseTickMs;
    feed_acc += dt;
    if (feed_acc >= 1000) {
      feed_acc -= 1000;
      if (sensor_connected) {
        core.feedT1(cc::DrvStatus::OK, reading());
        core.feedRh(cc::DrvStatus::OK, cabin.rh);
      } else {
        core.feedT1(cc::DrvStatus::TIMEOUT, NAN);
        core.feedRh(cc::DrvStatus::TIMEOUT, NAN);
      }
    }
    if (clock) core.setClock(true, epoch0 + (int64_t)(t_ms / 1000));
    core.tick(dt);
    const bool* o = core.outputs();
    cabin.step(dt / 1000.0f, o[cc::R1], o[cc::R2], o[cc::HF], o[cc::VF]);
    inv.check(o, dt);
    t_ms += dt;
  }
  void run(float seconds) {
    const uint32_t n = (uint32_t)(seconds * 1000.0f / cc::ClimateCore::kBaseTickMs);
    for (uint32_t i = 0; i < n; ++i) step();
  }
  // Koşul sağlanana kadar (en çok max_s) çalıştır; geçen süre (s), sağlanmazsa -1
  template <typename F>
  float runUntil(F cond, float max_s) {
    const uint32_t start = t_ms;
    const uint32_t n = (uint32_t)(max_s * 1000.0f / cc::ClimateCore::kBaseTickMs);
    for (uint32_t i = 0; i < n; ++i) {
      step();
      if (cond()) return (t_ms - start) / 1000.0f;
    }
    return -1.0f;
  }
  // Boot + self-test sonrası RUN
  void boot(const cc::Config& c, const cc::BootInfo& b = cc::BootInfo()) {
    begin(c, b);
    run(4.0f);
  }
  const cc::CoreSnapshot& s() const { return core.snapshot(); }
  bool out(uint8_t k) const { return core.outputs()[k]; }
};

}  // namespace sim
