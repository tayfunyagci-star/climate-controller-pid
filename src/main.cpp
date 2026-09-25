// Kulübe İklim Kontrolörü — ESP32-S3
// F1: yalnız hedef derleme doğrulaması. Çekirdek (lib/core) hedefte derlenir; GPIO sürülmez,
// görev oluşturulmaz, çıkışlar donanım pull-down'larıyla güvenli kalır. HAL + FreeRTOS görevleri F2'dedir.
#include <Arduino.h>
#include "cc_core.h"

static cc::ClimateCore g_core;  // F2'de ControlTask/OutputTask/SafetyTask'a dağıtılacak

void setup() {
  Serial.begin(115200);
  Serial.println(F("climate-core F1: derleme dogrulamasi (cikis surulmez)"));
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // bloklamayan RTOS beklemesi; delay() kullanılmaz
}
