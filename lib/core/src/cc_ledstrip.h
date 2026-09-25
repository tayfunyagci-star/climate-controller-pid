// WS2812B durum LED şeridi — SCADA ailesi ortak düzeni (scada-cihaz-standardi §LED):
//   LED1 Durum (alarm yoksa yeşil, uyarı/alarm yanıp söner) · LED2 Ağ (yok / Wi-Fi / AP kurulum)
//   LED3 MQTT (kesik / bağlı / tanımsız) · LED4 mDNS (yok / hazır / devre dışı) · LED5+ cihaza özgü.
// Bu cihazda LED5 Isıtma (kapalı / 1 kademe / 2 kademe), LED6 Fan (kapalı / ısıtıcı fanı / havalandırma).
// Saf mantık: durum seçimi + renk/parlaklık/yanıp sönme. Donanım sürücüsü src/app/hal_ws2812.
#pragma once
#include <cstdint>
#include "cc_types.h"

namespace cc {

constexpr uint8_t kLedCount = 6;
constexpr uint8_t kLedStates = 3;
constexpr uint32_t kLedBlinkMs = 500;          // uyarı/alarm: 1 Hz (500 ms açık / 500 ms kapalı)

enum class MqttLink : uint8_t { DOWN, UP, UNDEFINED };   // UNDEFINED: broker tanımsız veya MQTT kapalı

struct LedGroup {
  const char* key;                              // ayar anahtarı öneki: <key><0..2> = "#rrggbb"
  const char* label;
  const char* states[kLedStates];
};
const LedGroup& ledGroup(uint8_t i);

struct LedConfig {
  uint8_t brightness = 20;                      // %0–100 (5 V şerit akımı ve göz yorgunluğu için düşük varsayılan)
  uint32_t color[kLedCount][kLedStates] = {
      {0x00FF00, 0xFF8000, 0xFF0000},           // Durum: normal yeşil, uyarı turuncu, alarm kırmızı
      {0xFF0000, 0x00FF00, 0x0000FF},           // Ağ: yok kırmızı, Wi-Fi yeşil, AP mavi
      {0xFF0000, 0x00FF00, 0x000000},           // MQTT: kesik kırmızı, bağlı yeşil, tanımsız sönük
      {0xFF0000, 0x00FF00, 0x000000},           // mDNS: yok kırmızı, hazır yeşil, devre dışı sönük
      {0x000000, 0xFF8000, 0xFF0000},           // Isıtma: kapalı sönük, 1 kademe turuncu, 2 kademe kırmızı
      {0x000000, 0x00FFFF, 0x0000FF},           // Fan: kapalı sönük, ısıtıcı fanı turkuaz, havalandırma mavi
  };
};

struct LedInputs {
  Severity alarm = Severity::NONE;              // en yüksek etkin alarm önemi
  bool failsafe = false;                        // güvenli durum → alarm gibi
  bool ap_mode = false, sta_ok = false;
  MqttLink mqtt = MqttLink::UNDEFINED;
  bool mdns_enabled = true, mdns_ok = false;
  uint8_t heaters_on = 0;                       // 0..2 açık rezistans
  bool heater_fan = false, vent_fan = false;
};

// Her LED için durum indeksi 0..2
void ledStates(const LedInputs& in, uint8_t out[kLedCount]);
// Yanıp sönen durum (yalnız LED1 uyarı/alarm)
bool ledBlinks(uint8_t led, uint8_t state);
// Şerit verisi (WS2812B sırası G,R,B), parlaklık ölçekli; yanıp sönmenin kapalı yarısında 0
void ledRender(const LedConfig& c, const uint8_t st[kLedCount], uint32_t now_ms, uint8_t grb[kLedCount * 3]);

bool parseHexColor(const char* s, uint32_t& out);   // "#rrggbb" (büyük/küçük harf)
void formatHexColor(uint32_t c, char out[8]);
// "<key><n>" → (led, state); false: LED renk anahtarı değil
bool ledColorKey(const char* key, uint8_t& led, uint8_t& state);

}  // namespace cc
