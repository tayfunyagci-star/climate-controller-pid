#include "boot_state.h"
#include <esp_attr.h>
#include <esp_system.h>

namespace app {

struct RtcBlock {
  uint32_t magic;
  uint8_t heater_was_on;
  uint8_t fault_boots;
  uint8_t safety_latched;
  uint8_t sw_fault;    // acil yoldan yazılım reseti (esp_restart) — sonraki boot'ta hatalı boot sayılır
  uint32_t check;  // basit bütünlük: magic ^ alanlar (kasıtlı bozulmaya karşı değil)
};
static constexpr uint32_t kMagic = 0x43434632u;  // "CCF2"
RTC_NOINIT_ATTR static RtcBlock s_rtc;

static uint32_t chk(const RtcBlock& b) {
  return b.magic ^ ((uint32_t)b.heater_was_on << 8) ^ ((uint32_t)b.fault_boots << 16) ^ ((uint32_t)b.safety_latched << 24) ^ ((uint32_t)b.sw_fault << 4) ^ 0xA5A5A5A5u;
}
static void seal() { s_rtc.check = chk(s_rtc); }

static const char* reasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWER_ON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

cc::BootInfo readBoot(BootState& bs) {
  const esp_reset_reason_t r = esp_reset_reason();
  bs.reset_reason = reasonName(r);
  bs.power_on = r == ESP_RST_POWERON || r == ESP_RST_UNKNOWN;
  bs.fault_reset = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
                   r == ESP_RST_BROWNOUT;
  const bool valid = s_rtc.magic == kMagic && s_rtc.check == chk(s_rtc);
  if (bs.power_on || !valid) {
    s_rtc = RtcBlock{kMagic, 0, 0, 0, 0, 0};
    // Güç kesintisinde RTC silinir: rezistans açıkken kesinti olduysa boot post-cool bilgisi yoktur.
    // Kulübede bu durumda fan yine SELF_TEST sonrası talep varsa prestart ile başlar (F3: kalıcı bayrak).
  }
  if (s_rtc.sw_fault) { bs.fault_reset = true; s_rtc.sw_fault = 0; }
  if (bs.fault_reset && s_rtc.fault_boots < 255) ++s_rtc.fault_boots;
  seal();
  cc::BootInfo b;
  b.fault_reset = bs.fault_reset;
  b.heater_was_on = s_rtc.heater_was_on != 0;
  b.fault_boots_in_window = s_rtc.fault_boots;
  b.safety_latched = s_rtc.safety_latched;
  return b;
}

void rtcSetHeaterWasOn(bool v) {
  const uint8_t n = v ? 1 : 0;
  if (s_rtc.heater_was_on != n) { s_rtc.heater_was_on = n; seal(); }
}
void rtcSetSafetyLatched(uint8_t mask) {
  if (s_rtc.safety_latched != mask) { s_rtc.safety_latched = mask; seal(); }
}
void rtcClearFaultBoots() {
  if (s_rtc.fault_boots) { s_rtc.fault_boots = 0; seal(); }
}
uint8_t rtcFaultBoots() { return s_rtc.fault_boots; }
const char* resetReasonName() { return reasonName(esp_reset_reason()); }
void rtcMarkSwFault() { s_rtc.sw_fault = 1; s_rtc.heater_was_on = 1; seal(); }

}  // namespace app
