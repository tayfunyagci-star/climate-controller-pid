#include "hal_dht22.h"
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "pins.h"

namespace hal {

static constexpr rmt_channel_t kCh = (rmt_channel_t)hw::RMT_CH_DHT_RX;
static cc::DhtPulse s_pulses[128];

bool Dht22::begin() {
  rmt_config_t c = {};
  c.rmt_mode = RMT_MODE_RX;
  c.channel = kCh;
  c.gpio_num = hw::PIN_DHT;
  c.clk_div = 80;                         // 1 µs tık (APB 80 MHz)
  c.mem_block_num = 2;                    // 96 girdi; çerçeve ≈ 84 darbe
  c.rx_config.filter_en = true;
  c.rx_config.filter_ticks_thresh = 100;  // < 1.25 µs parazit
  c.rx_config.idle_threshold = 300;       // 300 µs sabit seviye = çerçeve sonu
  if (rmt_config(&c) != ESP_OK) return false;
  if (rmt_driver_install(kCh, 1024, 0) != ESP_OK) return false;
  // RMT girişi GPIO matrisinden bağlı kalır; pin ayrıca open-drain çıkış (başlatma darbesi için)
  gpio_set_direction(hw::PIN_DHT, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_pullup_en(hw::PIN_DHT);            // harici 4.7 kΩ'a ek
  gpio_set_level(hw::PIN_DHT, 1);
  ok_ = true;
  return true;
}

cc::DhtFrame Dht22::read() {
  cc::DhtFrame f;
  pulses_ = 0;
  if (!ok_) { f.status = cc::DrvStatus::MISSING; return f; }
  RingbufHandle_t rb = nullptr;
  rmt_get_ringbuf_handle(kCh, &rb);
  size_t sz = 0;
  // Önceki kayıtları at
  while (void* old = xRingbufferReceive(rb, &sz, 0)) vRingbufferReturnItem(rb, old);

  // Başlatma: hattı ≥ 1 ms (burada 2–3 ms) LOW tut. RTOS uykusu; AM2302 0.8–20 ms kabul eder.
  gpio_set_level(hw::PIN_DHT, 0);
  vTaskDelay(pdMS_TO_TICKS(3) > 0 ? pdMS_TO_TICKS(3) : 1);
  // Yakalamayı başlat ve hattı bırak: arada görev geçişi olmasın (kesmeler çalışır)
  vTaskSuspendAll();
  rmt_rx_start(kCh, true);
  gpio_set_level(hw::PIN_DHT, 1);
  xTaskResumeAll();

  // Yanıt 80+80 µs + 40 bit ≈ 5 ms; boşta eşiği sonrası kayıt halkaya düşer
  rmt_item32_t* items = (rmt_item32_t*)xRingbufferReceive(rb, &sz, pdMS_TO_TICKS(20));
  rmt_rx_stop(kCh);
  if (!items) { f.status = cc::DrvStatus::TIMEOUT; return f; }
  uint16_t n = 0;
  const size_t cnt = sz / sizeof(rmt_item32_t);
  for (size_t i = 0; i < cnt && n + 2 <= (uint16_t)(sizeof s_pulses / sizeof s_pulses[0]); ++i) {
    s_pulses[n++] = cc::DhtPulse{(uint8_t)items[i].level0, (uint16_t)items[i].duration0};
    s_pulses[n++] = cc::DhtPulse{(uint8_t)items[i].level1, (uint16_t)items[i].duration1};
  }
  vRingbufferReturnItem(rb, items);
  pulses_ = n;
  return cc::decodeDht22(s_pulses, n);
}

}  // namespace hal
