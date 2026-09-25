// Proses snapshot'ının JSON yazımı — tek kaynak: web /api/data ve MQTT B/state aynı alanları aynı biçimde
// üretir (MQTT_INTEGRATION §4.1: düz, eksiksiz, ikili "ON"/"OFF", geçersiz ölçüm null, yuvarlama cihazda).
#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include "core_api.h"

namespace app {

struct Frame {
  cc::CoreSnapshot s;
  cc::Config c;
  cc::HpmOutput hpm;
  uint32_t sw[4] = {0, 0, 0, 0};
  uint64_t on_ms[4] = {0, 0, 0, 0};
  bool svc[4] = {false, false, false, false};
  uint8_t vsrc = 0;
  float err_rate = 0;
  char prog[24] = "—";
};

bool capture(Frame& f, uint32_t timeout_ms);          // çekirdek kilidi altında kısa kopya
void writeProcess(JsonObject d, const Frame& f);       // B/state (+ ts, seq, uptime) — MQTT_INTEGRATION §4.1
void writeWebExtras(JsonObject d, const Frame& f);     // yalnız /api/data: PID terimleri, eşikler (B/state tamponunu şişirmez)
void writeConfigReported(JsonObject d, const cc::Config& c);   // B/config/reported (sırsız)
void writeDiag(JsonObject d, const Frame& f);          // B/diag/state

const char* onoff(bool b);
void fnum(JsonObject o, const char* k, float v, int dec = 2);
void isoLocal(int64_t local_min, char out[24]);

}  // namespace app
