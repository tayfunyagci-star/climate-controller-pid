#include "trend.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cmath>
#include <cstdlib>
#include "tasks.h"

namespace trend {

namespace {

constexpr size_t kFast = 720, kSlow = 1440;   // 1 sa / 5 s, 24 sa / 60 s
Sample* g_fast = nullptr;
Sample* g_slow = nullptr;
size_t g_fn = 0, g_fh = 0, g_sn = 0, g_sh = 0;  // adet, en eskinin indeksi
uint32_t g_last_fast = 0, g_last_slow = 0, g_fast_ms = 0, g_slow_ms = 0;
uint8_t g_min_bits = 0;
SemaphoreHandle_t g_mtx = nullptr;

void push(Sample* ring, size_t cap, size_t& n, size_t& head, const Sample& s) {
  if (n < cap) ring[(head + n++) % cap] = s;
  else { ring[head] = s; head = (head + 1) % cap; }
}

int16_t q10(float v) { return std::isnan(v) ? kNoT : (int16_t)lroundf(v * 10.f); }

}  // namespace

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  g_fast = static_cast<Sample*>(malloc(sizeof(Sample) * kFast));
  g_slow = static_cast<Sample*>(malloc(sizeof(Sample) * kSlow));
  if (!g_fast || !g_slow) Serial.println("[TREND] bellek ayrilamadi; trend kapali");
}

void tick(uint32_t now) {
  if (!g_fast || !g_slow || now - g_last_fast < 5000) return;
  g_last_fast = now;
  Sample s;
  if (!app::coreLock(5)) return;                 // kilit meşgulse bu örnek atlanır (proses önceliklidir)
  const cc::CoreSnapshot& c = app::core().snapshot();
  s.t10 = q10(c.temperature);
  s.sp10 = q10(c.setpoint_effective);
  s.rh10 = std::isnan(c.humidity) ? kNoRh : (uint16_t)lroundf(c.humidity * 10.f);
  s.d2 = (uint8_t)lroundf((c.heat_demand > 100 ? 100 : (c.heat_demand < 0 ? 0 : c.heat_demand)) * 2.f);
  s.b = (c.active[0] ? 1 : 0) | (c.active[1] ? 2 : 0) | (c.active[2] ? 4 : 0) | (c.active[3] ? 8 : 0);
  app::coreUnlock();
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  push(g_fast, kFast, g_fn, g_fh, s);
  g_fast_ms = now;
  g_min_bits |= s.b;
  if (now - g_last_slow >= 60000 || !g_sn) {
    Sample m = s;
    m.b = g_min_bits;
    push(g_slow, kSlow, g_sn, g_sh, m);
    g_last_slow = now;
    g_slow_ms = now;
    g_min_bits = 0;
  }
  xSemaphoreGive(g_mtx);
}

size_t maxSamples() { return kSlow; }

size_t read(uint32_t win_s, Sample* out, size_t cap, uint32_t& res_s, uint32_t& age_last_s) {
  if (!g_fast || !g_slow) { res_s = 5; age_last_s = 0; return 0; }
  const bool slow = win_s > 3600;
  res_s = slow ? 60 : 5;
  size_t want = win_s / res_s;
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  const Sample* ring = slow ? g_slow : g_fast;
  const size_t rc = slow ? kSlow : kFast, n = slow ? g_sn : g_fn, head = slow ? g_sh : g_fh;
  age_last_s = (millis() - (slow ? g_slow_ms : g_fast_ms)) / 1000u;
  if (want > n) want = n;
  if (want > cap) want = cap;
  for (size_t i = 0; i < want; ++i) out[i] = ring[(head + n - want + i) % rc];
  xSemaphoreGive(g_mtx);
  return want;
}

}  // namespace trend
