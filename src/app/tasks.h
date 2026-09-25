// FreeRTOS görevleri ve çekirdek kilidi — SYSTEM_ARCHITECTURE §4.
// Safety (250 ms) > Output (100 ms) > Control (control_interval_s) > Sensor (≥ 2 s, DHT22), hepsi çekirdek 1.
// Her görev aşamasını tek çekirdek kilidi (öncelik mirasçı mutex) altında, ölçülen gerçek dt ile çağırır;
// geç kalan görev yetişme patlaması yapmaz (native test_tasking bu modeli doğrular).
#pragma once
#include <cstdint>
#include "core_api.h"

namespace app {

struct TaskStats {
  uint32_t age_ms[4];          // 0 safety, 1 output, 2 control, 3 sensor: son çalışmadan bu yana
  uint32_t max_us[4];          // aşama süresi (kilit içi) azami
  uint32_t lock_timeouts = 0;
  uint32_t dht_ok = 0, dht_err = 0, dht_timeout = 0, dht_crc = 0;
  cc::DrvStatus dht_last = cc::DrvStatus::NOT_READY;
  float dht_t = 0, dht_rh = 0;
  uint32_t dht_pulses = 0;
  uint32_t stack_free[4];      // bayt, en düşük
};

void tasksStart(cc::ClimateCore& core);
bool coreLock(uint32_t timeout_ms);
void coreUnlock();
cc::ClimateCore& core();
TaskStats stats();

#ifdef CC_HIL
// HIL (yalnız hil build): sensör benzetimi ve görev dondurma — üretim imajında derlenmez (SECURITY §5)
void hilSim(bool on, float t, float rh);
void hilSensorFail(bool on);
void hilHang(uint8_t which);  // 0 safety (TWDT), 1 output (acil yol), 2 control (heartbeat), 3 sensor
#endif

}  // namespace app
