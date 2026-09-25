// Kulübe İklim Kontrolörü — ESP32-S3 (F2: HAL + FreeRTOS görevleri + seri konsol)
// Boot sırası SYSTEM_ARCHITECTURE §5: çıkışlar pasif → reset nedeni → config → çekirdek → görevler → ağ.
// Karta yükleme yalnız kullanıcının açık talimatıyla; ilk HIL'de rezistans yerine SSR girişlerinde LED.
#include <Arduino.h>
#include "app/boot_state.h"
#include "app/console.h"
#include "app/hal_outputs.h"
#include "app/net_clock.h"
#include "app/tasks.h"
#include "app/core_api.h"

static cc::ClimateCore g_core;

// F2 konfigürasyonu: kalıcı depo F3'te. Donanım kararları (CHANGELOG F2) varsayılanların üstüne yazılır.
static cc::Config f2Config() {
  cc::Config c;
  c.sensor_interval_s = 2;            // DHT22 asgari 2 s
  c.heater_power_w_r1 = 1000;         // 2 × 1000 W eşit
  c.heater_power_w_r2 = 1000;
  c.output_driver_r = cc::DriverKind::SSR_ZC;
  c.output_driver_fan = cc::DriverKind::RELAY;
  c.t2_enabled = false;               // T2 yok → post-cool yalnız süre
  c.post_cool_mode = cc::PostCoolMode::TIME;
  c.time_source = cc::TimeSource::NTP;
  return c;
}

void setup() {
  hal::outputsEarlyInit();            // ilk iş: R pasif (LOW), fan röleleri pasif (HIGH)
  Serial.begin(115200);
  app::BootState bs;
  const cc::BootInfo boot = app::readBoot(bs);
  Serial.printf("\nKulube Iklim Kontrolcusu F2 | reset=%s hatali_boot=%u heater_was_on=%d\n", bs.reset_reason,
                (unsigned)boot.fault_boots_in_window, (int)boot.heater_was_on);
  g_core.begin(f2Config(), boot);     // doğrulanmamış config reddedilir → güvenli varsayılan + CONFIG_ERROR
  app::tasksStart(g_core);            // kontrol ağdan bağımsız başlar
  net::begin();                       // Wi-Fi + SNTP paralel (kontrol beklemez)
  app::consoleBegin(bs);
}

void loop() {
  app::consoleService();
  vTaskDelay(pdMS_TO_TICKS(20));      // RTOS uykusu; delay() kullanılmaz
}
