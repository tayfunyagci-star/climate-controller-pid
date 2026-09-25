#include "hal_led.h"
#include <driver/rmt.h>
#include "pins.h"

namespace hal {

static constexpr rmt_channel_t kCh = (rmt_channel_t)hw::RMT_CH_LED_TX;
static bool s_ok = false;
static rmt_item32_t s_items[24];

bool ledBegin() {
  rmt_config_t c = {};
  c.rmt_mode = RMT_MODE_TX;
  c.channel = kCh;
  c.gpio_num = hw::PIN_RGB;
  c.clk_div = 2;  // 40 MHz → 25 ns
  c.mem_block_num = 1;
  c.tx_config.idle_output_en = true;
  c.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  if (rmt_config(&c) != ESP_OK) return false;
  if (rmt_driver_install(kCh, 0, 0) != ESP_OK) return false;
  s_ok = true;
  ledSet(0, 0, 0);
  return true;
}

void ledSet(uint8_t r, uint8_t g, uint8_t b) {
  if (!s_ok) return;
  if (rmt_wait_tx_done(kCh, 0) != ESP_OK) return;  // önceki gönderim sürüyorsa bu güncellemeyi atla
  const uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
  for (int i = 0; i < 24; ++i) {
    const bool one = grb & (1u << (23 - i));
    s_items[i].level0 = 1;
    s_items[i].duration0 = one ? 32 : 16;  // 0.8 / 0.4 µs
    s_items[i].level1 = 0;
    s_items[i].duration1 = one ? 18 : 34;  // 0.45 / 0.85 µs
  }
  rmt_write_items(kCh, s_items, 24, false);
}

}  // namespace hal
