#include "hal_outputs.h"
#include "pins.h"

namespace hal {

static bool s_last[4] = {false, false, false, false};

static inline uint32_t levelFor(const hw::OutPin& p, bool on) { return (on == p.active_high) ? 1u : 0u; }

void outputsEarlyInit() {
  uint64_t mask = 0;
  for (const auto& p : hw::kOutPins) {
    gpio_set_level(p.pin, levelFor(p, false));  // çıkış yazmacı önce pasif
    mask |= 1ULL << p.pin;
  }
  gpio_config_t io = {};
  io.pin_bit_mask = mask;
  io.mode = GPIO_MODE_OUTPUT;
  io.pull_up_en = GPIO_PULLUP_DISABLE;     // polarite harici dirençlerle tanımlı
  io.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&io);
  for (const auto& p : hw::kOutPins) gpio_set_level(p.pin, levelFor(p, false));
  for (bool& b : s_last) b = false;
}

void outputsWrite(const bool on[4]) {
  for (uint8_t k = 0; k < 4; ++k) {
    gpio_set_level(hw::kOutPins[k].pin, levelFor(hw::kOutPins[k], on[k]));
    s_last[k] = on[k];
  }
}

const bool* outputsLast() { return s_last; }

void emergencyHeatersOff() {
  gpio_set_level(hw::kOutPins[0].pin, levelFor(hw::kOutPins[0], false));
  gpio_set_level(hw::kOutPins[1].pin, levelFor(hw::kOutPins[1], false));
  s_last[0] = s_last[1] = false;
}

}  // namespace hal
