#include "cc_ledstrip.h"
#include <cstdio>
#include <cstring>

namespace cc {

namespace {
const LedGroup kGroups[kLedCount] = {
    {"cls", "Durum", {"Normal", "Uyarı", "Alarm"}},
    {"clw", "Ağ", {"Bağlantı yok", "Wi-Fi bağlı", "AP kurulum"}},
    {"clq", "MQTT", {"Kesik", "Bağlı", "Tanımsız"}},
    {"clm", "mDNS", {"Yok", "Hazır", "Devre dışı"}},
    {"clr", "Isıtma", {"Kapalı", "1 kademe", "2 kademe"}},
    {"clf", "Fan", {"Kapalı", "Isıtıcı fanı", "Havalandırma"}},
};

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

const LedGroup& ledGroup(uint8_t i) { return kGroups[i < kLedCount ? i : 0]; }

void ledStates(const LedInputs& in, uint8_t out[kLedCount]) {
  out[0] = (in.failsafe || in.alarm == Severity::CRITICAL) ? 2 : (in.alarm == Severity::WARNING ? 1 : 0);
  // STA bağlıyken (devirde AP de açık olabilir) Wi-Fi; yalnız kurulum ağı açıksa AP
  out[1] = in.sta_ok ? 1 : (in.ap_mode ? 2 : 0);
  out[2] = in.mqtt == MqttLink::UP ? 1 : (in.mqtt == MqttLink::DOWN ? 0 : 2);
  out[3] = !in.mdns_enabled ? 2 : (in.mdns_ok ? 1 : 0);
  out[4] = in.heaters_on >= 2 ? 2 : in.heaters_on;
  out[5] = in.vent_fan ? 2 : (in.heater_fan ? 1 : 0);
}

bool ledBlinks(uint8_t led, uint8_t state) { return led == 0 && state > 0; }

void ledRender(const LedConfig& c, const uint8_t st[kLedCount], uint32_t now_ms, uint8_t grb[kLedCount * 3]) {
  const uint32_t b = c.brightness > 100 ? 100 : c.brightness;
  const bool off_half = (now_ms / kLedBlinkMs) % 2 == 1;
  for (uint8_t i = 0; i < kLedCount; ++i) {
    const uint8_t s = st[i] < kLedStates ? st[i] : 0;
    uint32_t rgb = c.color[i][s];
    if (ledBlinks(i, s) && off_half) rgb = 0;
    const uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, bl = rgb & 0xFF;
    // Yuvarlamalı ölçek: %100 → 255 aynen, sıfırdan büyük parlaklıkta tam renk sönmez
    grb[i * 3 + 0] = (uint8_t)((g * b + 50) / 100);
    grb[i * 3 + 1] = (uint8_t)((r * b + 50) / 100);
    grb[i * 3 + 2] = (uint8_t)((bl * b + 50) / 100);
  }
}

bool parseHexColor(const char* s, uint32_t& out) {
  if (!s || s[0] != '#' || strlen(s) != 7) return false;
  uint32_t v = 0;
  for (int i = 1; i < 7; ++i) {
    const int n = hexNibble(s[i]);
    if (n < 0) return false;
    v = (v << 4) | (uint32_t)n;
  }
  out = v;
  return true;
}

void formatHexColor(uint32_t c, char out[8]) { snprintf(out, 8, "#%06x", (unsigned)(c & 0xFFFFFF)); }

bool ledColorKey(const char* key, uint8_t& led, uint8_t& state) {
  if (!key || strlen(key) != 4 || key[3] < '0' || key[3] >= '0' + kLedStates) return false;
  for (uint8_t i = 0; i < kLedCount; ++i)
    if (!strncmp(key, kGroups[i].key, 3)) { led = i; state = (uint8_t)(key[3] - '0'); return true; }
  return false;
}

}  // namespace cc
