#include "hal_led.h"
#include "pins.h"

namespace hal {

void ledBegin() {
  gpio_set_level(hw::PIN_LED, 0);
  gpio_set_direction(hw::PIN_LED, GPIO_MODE_OUTPUT);
}

void ledService(LedPattern p, uint32_t now) {
  bool on = false;
  switch (p) {
    case LedPattern::OFF: on = false; break;
    case LedPattern::ON: on = true; break;
    case LedPattern::SLOW: on = (now / 1000) % 2; break;                  // 0.5 Hz
    case LedPattern::FAST: on = (now / 125) % 2; break;                   // 4 Hz
    case LedPattern::DOUBLE: { const uint32_t t = now % 1200; on = t < 100 || (t >= 200 && t < 300); } break;
    case LedPattern::HEARTBEAT: on = (now % 2000) < 60; break;           // 2 s'de bir kısa çakma
  }
  gpio_set_level(hw::PIN_LED, on ? 1 : 0);
}

}  // namespace hal
