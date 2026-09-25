#include "mqtt_cfg.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cctype>
#include <cstring>

namespace mqttcfg {

namespace {

SemaphoreHandle_t g_mtx = nullptr;
Settings g_s;
bool g_pass = false;
uint32_t g_gen = 1;
bool g_has_core[kCoreKeyCount] = {};
float g_core[kCoreKeyCount] = {};            // kayıtlı çekirdek değerleri (sayısal; bool 0/1)

struct Lock {
  Lock() { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { if (g_mtx) xSemaphoreGive(g_mtx); }
};

// NVS anahtarı en çok 15 karakter: çekirdek alanları sıra numarasıyla saklanır (c0…c8)
void coreNvsKey(size_t i, char out[4]) { snprintf(out, 4, "c%u", (unsigned)i); }

bool putS(Preferences& p, const char* k, const char* v) {
  if (!v[0]) return p.isKey(k) ? p.remove(k) : true;
  return p.putString(k, v) == strlen(v);
}

bool hasSpace(const char* s) {
  for (; *s; ++s) if (isspace((unsigned char)*s)) return true;
  return false;
}

}  // namespace

void load() {
  if (!g_mtx) g_mtx = xSemaphoreCreateMutex();
  Preferences p;
  if (!p.begin("mqtt", true)) return;          // ilk açılış: varsayılanlar
  Lock l;
  if (p.isKey("host")) p.getString("host", g_s.host, sizeof g_s.host);
  if (p.isKey("port")) g_s.port = p.getUShort("port", g_s.port);
  if (p.isKey("user")) p.getString("user", g_s.user, sizeof g_s.user);
  if (p.isKey("base")) p.getString("base", g_s.base, sizeof g_s.base);
  g_pass = p.isKey("pass");
  char k[4];
  for (size_t i = 0; i < kCoreKeyCount; ++i) {
    coreNvsKey(i, k);
    g_has_core[i] = p.isKey(k);
    if (g_has_core[i]) g_core[i] = p.getFloat(k, 0);
  }
  p.end();
}

void overlay(cc::Config& c) {
  Lock l;
  for (size_t i = 0; i < kCoreKeyCount; ++i) {
    if (!g_has_core[i]) continue;
    const cc::FieldInfo* f = cc::findField(kCoreKeys[i]);
    if (!f) continue;
    char* base = reinterpret_cast<char*>(&c) + f->offset;
    switch (f->kind) {
      case cc::FieldKind::BOOL: *reinterpret_cast<bool*>(base) = g_core[i] != 0; break;
      case cc::FieldKind::INT: *reinterpret_cast<int32_t*>(base) = (int32_t)lroundf(g_core[i]); break;
      case cc::FieldKind::FLOAT: *reinterpret_cast<float*>(base) = g_core[i]; break;
      case cc::FieldKind::ENUM: *reinterpret_cast<uint8_t*>(base) = (uint8_t)g_core[i]; break;
    }
  }
}

Settings settings() { Lock l; return g_s; }
bool passSet() { Lock l; return g_pass; }
uint32_t generation() { Lock l; return g_gen; }

bool password(char* out, size_t cap) {
  out[0] = 0;
  Preferences p;
  if (!p.begin("mqtt", true)) return false;
  if (p.isKey("pass")) p.getString("pass", out, cap);
  p.end();
  return out[0] != 0;
}

void slug(char out[24]) {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(out, 24, "kulube_iklim_%02x%02x%02x", mac[3], mac[4], mac[5]);
}

void devName(char out[32]) {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(out, 32, "Kulübe İklim %02x%02x%02x", mac[3], mac[4], mac[5]);
}

bool isStringKey(const char* k) {
  return !strcmp(k, "mqtt_host") || !strcmp(k, "mqtt_port") || !strcmp(k, "mqtt_user") || !strcmp(k, "mqtt_password") ||
         !strcmp(k, "mqtt_base");
}

bool isCoreKey(const char* k) {
  for (const char* c : kCoreKeys) if (!strcmp(k, c)) return true;
  return false;
}

bool validate(const Settings& s, const char* pass, const char** err, const char** field) {
  auto bad = [&](const char* f, const char* e) { *field = f; *err = e; return false; };
  if (strlen(s.host) > 63 || hasSpace(s.host)) return bad("mqtt_host", "Broker adresi en çok 63 karakter olmalı ve boşluk içermemeli.");
  if (s.port < 1) return bad("mqtt_port", "Broker portu 1–65535 olmalı.");
  if (strlen(s.user) > 64) return bad("mqtt_user", "Kullanıcı adı en çok 64 karakter olmalı.");
  if (pass && strlen(pass) > 128) return bad("mqtt_password", "MQTT parolası en çok 128 karakter olmalı.");
  const size_t n = strlen(s.base);
  if (n < 1 || n > 96 || hasSpace(s.base) || strchr(s.base, '+') || strchr(s.base, '#') || s.base[0] == '/' || s.base[n - 1] == '/')
    return bad("mqtt_base", "Kök topic 1–96 karakter olmalı; boşluk, + ve # içeremez, / ile başlayıp bitemez.");
  return true;
}

bool apply(const Settings& s, const char* pass, const cc::Config& core, const char** err, const char** field) {
  *err = nullptr; *field = nullptr;
  if (!validate(s, pass, err, field)) return false;
  float vals[kCoreKeyCount];
  for (size_t i = 0; i < kCoreKeyCount; ++i) {
    const cc::FieldInfo* f = cc::findField(kCoreKeys[i]);
    vals[i] = f ? cc::fieldValue(core, *f) : 0;
  }
  Preferences p;
  if (!p.begin("mqtt", false)) { *err = "Kayıt alanı açılamadı; önceki MQTT ayarları korundu."; return false; }
  bool ok = putS(p, "host", s.host) && p.putUShort("port", s.port) == 2 && putS(p, "user", s.user) && putS(p, "base", s.base);
  if (ok && pass) ok = putS(p, "pass", pass);
  char k[4];
  for (size_t i = 0; ok && i < kCoreKeyCount; ++i) { coreNvsKey(i, k); ok = p.putFloat(k, vals[i]) == 4; }
  p.end();
  if (!ok) { *err = "MQTT ayarları yazılamadı; NVS hatası."; return false; }
  Lock l;
  g_s = s;
  if (pass) g_pass = pass[0] != 0;
  for (size_t i = 0; i < kCoreKeyCount; ++i) { g_core[i] = vals[i]; g_has_core[i] = true; }
  ++g_gen;
  return true;
}

}  // namespace mqttcfg
