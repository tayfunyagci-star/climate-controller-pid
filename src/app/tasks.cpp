#include "tasks.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include "boot_state.h"
#include "hal_dht22.h"
#include "hal_outputs.h"
#include "net_manager.h"

namespace app {

namespace {

enum { T_SAF = 0, T_OUT = 1, T_CTL = 2, T_SEN = 3 };

cc::ClimateCore* g_core = nullptr;
SemaphoreHandle_t g_mtx = nullptr;
std::atomic<uint32_t> g_hb[4];
std::atomic<uint32_t> g_max_us[4];
std::atomic<uint32_t> g_lock_timeouts{0};
TaskHandle_t g_task[4] = {nullptr, nullptr, nullptr, nullptr};
hal::Dht22 g_dht;

// Sensör istatistiği yalnız SensorTask yazar; okuyucular kilit altında kopyalar
TaskStats g_sen;

#ifdef CC_HIL
std::atomic<bool> g_sim{false}, g_sim_fail{false};
std::atomic<float> g_sim_t{20.0f}, g_sim_rh{50.0f};
std::atomic<int> g_hang{-1};
inline void hilMaybeHang(uint8_t me) {
  if (g_hang.load() != me) return;
  // Safety: meşgul döngü (çekirdek 1 aç kalır, TWDT ≤ 5 s panik → reset; ARM'sız artık risk).
  // Diğerleri: görev kendini askıya alır (takılı bekleme) → heartbeat / acil yol sınanır.
  if (me == 0) { for (;;) { } }
  vTaskSuspend(nullptr);
}
#else
inline void hilMaybeHang(uint8_t) {}
#endif

constexpr uint32_t kOutputStallMs = 1000;       // SYSTEM_ARCHITECTURE §4.1
constexpr uint32_t kSafetyLockMissLimit = 4;    // 4 × 250 ms kilit alınamazsa acil yol
constexpr uint32_t kFaultWindowMs = 30u * 60u * 1000u;

[[noreturn]] void emergency(const char* why) {
  hal::emergencyHeatersOff();   // ARM hattı yok (CHANGELOG F2 sapma 1): tek yol R hatlarını pasife çekmek
  rtcMarkSwFault();             // sonraki boot: hatalı boot + boot post-cool
  ets_printf("\n!!! ACIL: %s -> rezistans OFF, yeniden baslatma\n", why);
  esp_restart();
  for (;;) {}
}

inline void noteTime(uint8_t k, int64_t t0) {
  const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);
  if (us > g_max_us[k].load()) g_max_us[k].store(us);
}

// Periyodik bekleme: kaçırılan periyotlar biriktirilmez (yetişme patlaması yok), dt gerçek süredir
inline uint32_t waitPeriod(TickType_t& wake, uint32_t period_ms, uint32_t& last_ms) {
  const TickType_t p = pdMS_TO_TICKS(period_ms);
  const TickType_t now = xTaskGetTickCount();
  if ((TickType_t)(now - wake) >= p) wake = now;  // geride kaldık: hedefi şimdiye çek
  vTaskDelayUntil(&wake, p);
  const uint32_t m = millis();
  const uint32_t dt = m - last_ms;
  last_ms = m;
  return dt;
}

void safetyTask(void*) {
  esp_task_wdt_add(nullptr);
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis(), miss = 0;
  for (;;) {
    const uint32_t dt = waitPeriod(wake, cc::ClimateCore::kSafetyPeriodMs, last);
    hilMaybeHang(T_SAF);
    const uint32_t now = millis();
    // OutputTask canlılığı kilitten bağımsız izlenir: takıldıysa GPIO eski seviyede kalmış olabilir
    if (now - g_hb[T_OUT].load() > kOutputStallMs) emergency("OUTPUT_TASK_STALL");
    if (xSemaphoreTake(g_mtx, pdMS_TO_TICKS(100)) == pdTRUE) {
      miss = 0;
      const int64_t t0 = esp_timer_get_time();
      g_core->baseTick(dt);
      g_core->safetyStep(dt);
      g_core->publish();
      const uint8_t latched = g_core->safety().latched;
      const bool healthy = g_core->sysState() == cc::SysState::RUN && g_core->uptimeMs() > kFaultWindowMs;
      noteTime(T_SAF, t0);
      xSemaphoreGive(g_mtx);
      rtcSetSafetyLatched(latched);
      if (healthy) rtcClearFaultBoots();
    } else {
      g_lock_timeouts.fetch_add(1);
      if (++miss >= kSafetyLockMissLimit) emergency("CORE_LOCK_TIMEOUT");
    }
    g_hb[T_SAF].store(millis());
    esp_task_wdt_reset();
  }
}

void outputTask(void*) {
  esp_task_wdt_add(nullptr);
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis();
  bool outs[cc::OUT_COUNT] = {false, false, false, false};
  for (;;) {
    const uint32_t dt = waitPeriod(wake, cc::ClimateCore::kOutputPeriodMs, last);
    hilMaybeHang(T_OUT);
    if (xSemaphoreTake(g_mtx, pdMS_TO_TICKS(50)) == pdTRUE) {
      const int64_t t0 = esp_timer_get_time();
      g_core->outputStep(dt);
      for (uint8_t k = 0; k < cc::OUT_COUNT; ++k) outs[k] = g_core->outputs()[k];
      const bool heat_trace = outs[cc::R1] || outs[cc::R2] || g_core->snapshot().post_cool_remaining_s > 0;
      noteTime(T_OUT, t0);
      xSemaphoreGive(g_mtx);
      // GPIO'nun tek sahibi: kilit dışında, her çevrimde dört pin yeniden yazılır (OutputGuard sıralı)
      hal::outputsWrite(outs);
      rtcSetHeaterWasOn(heat_trace);
      g_hb[T_OUT].store(millis());
    } else {
      g_lock_timeouts.fetch_add(1);  // heartbeat güncellenmez → Safety 1 s sonra acil yol
    }
    esp_task_wdt_reset();
  }
}

void controlTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis(), period = 2000;
  for (;;) {
    const uint32_t dt = waitPeriod(wake, period, last);
    hilMaybeHang(T_CTL);
    // Ağ/saat durumu kilit dışında okunur (ağ çağrısı sırasında kilit tutulmaz)
    const bool cv = net::clockValid();
    const int64_t ep = net::epochUtc();
    const bool wcfg = net::wifiConfigured(), wok = net::wifiOk();
    if (xSemaphoreTake(g_mtx, pdMS_TO_TICKS(500)) == pdTRUE) {
      const int64_t t0 = esp_timer_get_time();
      g_core->setClock(cv, ep);
      g_core->setNetStatus(wcfg, wok, false, false);  // MQTT F5
      g_core->controlStep(dt);
      period = g_core->controlPeriodMs();
      noteTime(T_CTL, t0);
      xSemaphoreGive(g_mtx);
      g_hb[T_CTL].store(millis());
    } else {
      g_lock_timeouts.fetch_add(1);
    }
  }
}

void sensorTask(void*) {
  TickType_t wake = xTaskGetTickCount();
  uint32_t last = millis(), period = 2000;
  for (;;) {
    waitPeriod(wake, period, last);
    hilMaybeHang(T_SEN);
    cc::DhtFrame f = g_dht.read();  // kilit dışında (≈ 5–20 ms)
#ifdef CC_HIL
    if (g_sim_fail.load()) { f = cc::DhtFrame(); f.status = cc::DrvStatus::TIMEOUT; }
    else if (g_sim.load()) { f.status = cc::DrvStatus::OK; f.temperature = g_sim_t.load(); f.humidity = g_sim_rh.load(); }
#endif
    if (xSemaphoreTake(g_mtx, pdMS_TO_TICKS(500)) == pdTRUE) {
      const int64_t t0 = esp_timer_get_time();
      g_core->feedT1(f.status, f.temperature);
      g_core->feedRh(f.status, f.humidity);
      // DHT22 asgari 2 s örnekleme; konfigürasyon daha kısa isterse 2 s uygulanır
      const int32_t iv = g_core->config().sensor_interval_s;
      period = (uint32_t)(iv < 2 ? 2 : iv) * 1000u;
      g_sen.dht_last = f.status;
      g_sen.dht_pulses = g_dht.lastPulseCount();
      if (f.status == cc::DrvStatus::OK) { ++g_sen.dht_ok; g_sen.dht_t = f.temperature; g_sen.dht_rh = f.humidity; }
      else {
        ++g_sen.dht_err;
        if (f.status == cc::DrvStatus::TIMEOUT) ++g_sen.dht_timeout;
        if (f.status == cc::DrvStatus::CRC_ERROR) ++g_sen.dht_crc;
      }
      noteTime(T_SEN, t0);
      xSemaphoreGive(g_mtx);
      g_hb[T_SEN].store(millis());
    } else {
      g_lock_timeouts.fetch_add(1);
    }
  }
}

}  // namespace

void tasksStart(cc::ClimateCore& core) {
  g_core = &core;
  g_mtx = xSemaphoreCreateMutex();  // öncelik mirası
  const uint32_t now = millis();
  for (auto& h : g_hb) h.store(now);
  for (auto& m : g_max_us) m.store(0);
  g_dht.begin();
  // TWDT 5 s, panik → reset (SYSTEM_ARCHITECTURE §4.1). Idle görevleri izlenmeye devam eder.
  esp_task_wdt_init(5, true);
  enableCore1WDT();
  // Yığın boyutları bayttır (IDF). Öncelikler çekirdek 1 üzerinde göreli; ağ yığını çekirdek 0'dadır.
  xTaskCreatePinnedToCore(safetyTask, "safety", 4096, nullptr, 10, &g_task[T_SAF], 1);
  xTaskCreatePinnedToCore(outputTask, "output", 4096, nullptr, 9, &g_task[T_OUT], 1);
  xTaskCreatePinnedToCore(controlTask, "control", 8192, nullptr, 7, &g_task[T_CTL], 1);
  xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, nullptr, 6, &g_task[T_SEN], 1);
}

bool coreLock(uint32_t timeout_ms) { return xSemaphoreTake(g_mtx, pdMS_TO_TICKS(timeout_ms)) == pdTRUE; }
void coreUnlock() { xSemaphoreGive(g_mtx); }
cc::ClimateCore& core() { return *g_core; }

TaskStats stats() {
  TaskStats s;
  if (coreLock(100)) {
    s = g_sen;
    coreUnlock();
  }
  const uint32_t now = millis();
  for (uint8_t k = 0; k < 4; ++k) {
    s.age_ms[k] = now - g_hb[k].load();
    s.max_us[k] = g_max_us[k].load();
    s.stack_free[k] = g_task[k] ? uxTaskGetStackHighWaterMark(g_task[k]) : 0;
  }
  s.lock_timeouts = g_lock_timeouts.load();
  return s;
}

#ifdef CC_HIL
void hilSim(bool on, float t, float rh) { g_sim_t.store(t); g_sim_rh.store(rh); g_sim.store(on); }
void hilSensorFail(bool on) { g_sim_fail.store(on); }
void hilHang(uint8_t which) { g_hang.store(which); }
#endif

}  // namespace app
