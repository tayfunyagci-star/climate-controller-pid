#include "hal_ws2812.h"
#include <driver/rmt.h>
#include <esp_attr.h>
#include <cstring>
#include "pins.h"

namespace hal {

namespace {

constexpr rmt_channel_t kCh = (rmt_channel_t)hw::RMT_CH_LED_TX;
constexpr size_t kMaxLeds = 8;                 // 8 × 24 = 192 girdi = 3 bellek bloğu
// clk_div 2 → 40 MHz, 25 ns tık. WS2812B: 0 = 0.40/0.85 µs, 1 = 0.80/0.45 µs (±150 ns)
constexpr uint32_t kT0H = 16, kT0L = 34, kT1H = 32, kT1L = 18;

bool s_ok = false;
size_t s_leds = 0;
uint8_t s_buf[kMaxLeds * 3];                   // RMT gönderim süresince geçerli kalmalı (rmt_write_sample kopyalamaz)

void IRAM_ATTR toRmt(const void* src, rmt_item32_t* dest, size_t src_size, size_t wanted_num,
                     size_t* translated_size, size_t* item_num) {
  if (!src || !dest) { *translated_size = 0; *item_num = 0; return; }
  const rmt_item32_t bit0 = {{{kT0H, 1, kT0L, 0}}};
  const rmt_item32_t bit1 = {{{kT1H, 1, kT1L, 0}}};
  const uint8_t* p = (const uint8_t*)src;
  size_t size = 0, num = 0;
  while (size < src_size && num + 8 <= wanted_num) {
    for (int i = 7; i >= 0; --i) { dest->val = ((*p >> i) & 1) ? bit1.val : bit0.val; ++dest; ++num; }
    ++size; ++p;
  }
  *translated_size = size;
  *item_num = num;
}

}  // namespace

bool stripBegin(size_t leds) {
  if (leds > kMaxLeds) leds = kMaxLeds;
  rmt_config_t c = RMT_DEFAULT_CONFIG_TX(hw::PIN_LED_STRIP, kCh);
  c.clk_div = 2;
  c.mem_block_num = hw::RMT_LED_MEM_BLOCKS;
  c.tx_config.idle_output_en = true;
  c.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;  // çerçeveler arası ≥ 50 µs LOW = latch
  if (rmt_config(&c) != ESP_OK) return false;
  if (rmt_driver_install(kCh, 0, 0) != ESP_OK) return false;
  if (rmt_translator_init(kCh, toRmt) != ESP_OK) return false;
  s_leds = leds;
  s_ok = true;
  memset(s_buf, 0, sizeof s_buf);
  rmt_write_sample(kCh, s_buf, s_leds * 3, false);   // boot: bütün LED'ler sönük
  return true;
}

bool stripShow(const uint8_t* grb, size_t leds) {
  if (!s_ok) return false;
  if (rmt_wait_tx_done(kCh, 0) != ESP_OK) return false;   // önceki çerçeve sürüyor → bu tur atlanır
  if (leds > s_leds) leds = s_leds;
  memcpy(s_buf, grb, leds * 3);
  return rmt_write_sample(kCh, s_buf, leds * 3, false) == ESP_OK;
}

bool stripOk() { return s_ok; }

}  // namespace hal
