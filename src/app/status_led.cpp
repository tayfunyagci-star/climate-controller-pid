#include "status_led.h"
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>
#include "hal_ws2812.h"
#include "net_manager.h"
#include "tasks.h"

namespace leds {

namespace {

SemaphoreHandle_t g_mtx = nullptr;               // g_cfg, g_st
cc::LedConfig g_cfg;
uint8_t g_st[cc::kLedCount] = {};
cc::LedInputs g_in;                              // yalnız loop görevi
uint32_t g_tick_ms = 0;

// Web görevi (NetTask) begin()'den önce okuyabilir: mutex yoksa yazıcı da yoktur, kilitsiz okunur
struct Lock {
  Lock() { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { if (g_mtx) xSemaphoreGive(g_mtx); }
};

void load() {
  Preferences p;
  if (!p.begin("led", true)) return;             // ilk açılış: varsayılanlar
  if (p.isKey("b")) g_cfg.brightness = p.getUChar("b", g_cfg.brightness);
  if (p.isKey("c") && p.getBytesLength("c") == sizeof g_cfg.color) p.getBytes("c", g_cfg.color, sizeof g_cfg.color);
  p.end();
  if (g_cfg.brightness > 100) g_cfg.brightness = 100;
}

void gather() {
  if (app::coreLock(5)) {
    const cc::CoreSnapshot& s = app::core().snapshot();
    g_in.alarm = s.alarm_state;
    g_in.failsafe = s.controller_state == cc::CtrlState::FAILSAFE;
    const bool* o = app::core().outputs();
    g_in.heaters_on = (uint8_t)(o[cc::R1] ? 1 : 0) + (uint8_t)(o[cc::R2] ? 1 : 0);
    g_in.heater_fan = o[cc::HF];
    g_in.vent_fan = o[cc::VF];
    app::coreUnlock();
  }
  const net::Status ns = net::status();
  g_in.ap_mode = ns.ap_mode;
  g_in.sta_ok = ns.sta_ok;
  g_in.mdns_enabled = true;                      // mDNS adı zorunlu; ayrı kapatma yok
  g_in.mdns_ok = ns.mdns_ok;
  g_in.mqtt = cc::MqttLink::UNDEFINED;           // MQTT istemcisi F5'te
}

}  // namespace

void begin() {
  load();
  g_mtx = xSemaphoreCreateMutex();
  if (!hal::stripBegin(cc::kLedCount)) Serial.println("[LED] WS2812B surucusu baslatilamadi");
}

void service(uint32_t now) {
  if (now - g_tick_ms < 50) return;
  g_tick_ms = now;
  gather();
  uint8_t st[cc::kLedCount];
  cc::ledStates(g_in, st);
  cc::LedConfig c;
  {
    Lock l;
    memcpy(g_st, st, sizeof g_st);
    c = g_cfg;
  }
  uint8_t grb[cc::kLedCount * 3];
  cc::ledRender(c, st, now, grb);
  hal::stripShow(grb, cc::kLedCount);
}

cc::LedConfig config() { Lock l; return g_cfg; }

void states(uint8_t out[cc::kLedCount]) { Lock l; memcpy(out, g_st, sizeof g_st); }

bool driverOk() { return hal::stripOk(); }

bool apply(const cc::LedConfig& c, const char** err) {
  *err = nullptr;
  if (c.brightness > 100) { *err = "LED parlaklığı %0–100 olmalı."; return false; }
  for (auto& row : c.color)
    for (uint32_t v : row)
      if (v > 0xFFFFFF) { *err = "LED rengi #rrggbb olmalı."; return false; }
  Preferences p;
  if (!p.begin("led", false)) { *err = "Kayıt alanı açılamadı; önceki LED ayarları korundu."; return false; }
  const bool ok = p.putUChar("b", c.brightness) == 1 && p.putBytes("c", c.color, sizeof c.color) == sizeof c.color;
  p.end();
  if (!ok) { *err = "LED ayarları yazılamadı; NVS hatası."; return false; }
  Lock l;
  g_cfg = c;
  return true;
}

}  // namespace leds
