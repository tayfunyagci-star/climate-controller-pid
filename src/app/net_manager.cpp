#include "net_manager.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <MD5Builder.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <cstring>
#include "core_api.h"
#include "cc_netfsm.h"
#include "tasks.h"
#include "web.h"

namespace net {

namespace {

constexpr int64_t kMinValidEpoch = 1735689600;   // 2025-01-01 öncesi geçersiz

SemaphoreHandle_t g_mtx = nullptr;               // g_cfg, g_status, bayraklar
NetSettings g_cfg;
char g_ssid[33] = "", g_pass[65] = "", g_ota[33] = "";   // g_ota: MD5 hex (yalnız özet)
Status g_status;
bool g_reconnect = false;
uint32_t g_reboot_at = 0;
bool g_reboot = false;
bool g_ota_reload = false;                     // OTA parolası değişti → ArduinoOTA yeniden kur

// NetTask'a ait (tek sahip)
cc::NetFsm g_fsm;
DNSServer g_dns;
bool g_ap = false, g_services = false, g_ota_started = false;
char g_gw[16] = "";
char g_ntp_buf[64] = "pool.ntp.org";
std::atomic<bool> g_synced{false};

struct Lock {
  Lock() { xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(g_mtx); }
};

void note(cc::Severity s, cc::EvCode c, cc::CmdSource actor = cc::CmdSource::SYSTEM, float v = cc::kNaN) {
  if (app::coreLock(50)) {
    app::core().noteEvent(s, cc::EvSrc::NET, c, v, actor);
    app::coreUnlock();
  }
}

void onSync(struct timeval*) { g_synced.store(true); }

bool staticValid(const NetSettings& n) {
  uint32_t ip, sn, gw, d;
  if (!cc::parseIpv4(n.ip, ip) || !cc::parseIpv4(n.sn, sn) || !cc::parseIpv4(n.gw, gw)) return false;
  if (!cc::validStaticIpv4(ip, sn, gw, nullptr)) return false;
  if (n.d1[0] && !cc::parseIpv4(n.d1, d)) return false;
  if (n.d2[0] && !cc::parseIpv4(n.d2, d)) return false;
  return true;
}

IPAddress toIp(const char* s) { IPAddress a; a.fromString(s); return a; }

void getS(Preferences& p, const char* k, char* buf, size_t n) {
  if (p.isKey(k)) p.getString(k, buf, n);
}
// Boş değer anahtarın silinmesidir (Preferences boş dizgide 0 döndürür; başarısızlıkla karışmasın)
bool putS(Preferences& p, const char* k, const char* v) {
  if (!v[0]) return p.isKey(k) ? p.remove(k) : true;
  return p.putString(k, v) == strlen(v);
}

void load() {
  Preferences p;
  if (!p.begin("net", true)) return;
  getS(p, "ssid", g_ssid, sizeof g_ssid);
  getS(p, "pass", g_pass, sizeof g_pass);
  getS(p, "ota", g_ota, sizeof g_ota);
  getS(p, "adn", g_cfg.adn, sizeof g_cfg.adn);
  getS(p, "mdns", g_cfg.mdns, sizeof g_cfg.mdns);
  g_cfg.st = p.isKey("st") ? p.getBool("st", false) : false;
  getS(p, "ip", g_cfg.ip, sizeof g_cfg.ip);
  getS(p, "gw", g_cfg.gw, sizeof g_cfg.gw);
  getS(p, "sn", g_cfg.sn, sizeof g_cfg.sn);
  getS(p, "d1", g_cfg.d1, sizeof g_cfg.d1);
  getS(p, "d2", g_cfg.d2, sizeof g_cfg.d2);
  getS(p, "ntp", g_cfg.ntp, sizeof g_cfg.ntp);
  p.end();
}

// ---------------------------------------------------------------- platform eylemleri (yalnız NetTask)
void startSntp() {
  if (sntp_enabled()) sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_set_time_sync_notification_cb(onSync);
  sntp_setservername(0, g_ntp_buf);
  if (g_gw[0]) sntp_setservername(1, g_gw);   // modem/ağ geçidi NTP'si
  sntp_init();
}

void startOta(const char* host, const char* hash) {
  if (g_ota_started || !hash[0]) return;       // D-17: parolasız OTA yok
  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPasswordHash(hash);
  ArduinoOTA.setMdnsEnabled(false);             // mDNS'i biz yönetiyoruz
  ArduinoOTA.onStart([]() {
    bool ready = false;
    if (app::coreLock(200)) {
      app::core().otaBegin(cc::CmdSource::LOCAL_SERVICE);   // güvenli duruş: OTA_PREP → ısıtma/post-cool biter → OTA
      ready = app::core().otaReady();
      app::coreUnlock();
    }
    if (!ready) {
      Serial.println("[OTA] Hazirlik basladi: isitma/post-cool bitince yuklemeyi tekrarlayin. Bu yukleme iptal.");
      Update.abort();
      return;
    }
    note(cc::Severity::WARNING, cc::EvCode::OTA_START, cc::CmdSource::LOCAL_SERVICE);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    if (app::coreLock(200)) { app::core().otaAbort(); app::coreUnlock(); }
    note(cc::Severity::WARNING, cc::EvCode::OTA_FAIL, cc::CmdSource::LOCAL_SERVICE, (float)e);
  });
  ArduinoOTA.begin();
  g_ota_started = true;
}

void apply(const cc::NetActions& a, const NetSettings& n, const char* ssid, const char* pass, const char* ota,
           const char* ap_name) {
  if (a.stop_services && g_services) {
    if (g_ota_started) { ArduinoOTA.end(); g_ota_started = false; }
    MDNS.end();
    g_services = false;
  }
  if (a.start_ap) {
    WiFi.mode(WIFI_AP_STA);
    const IPAddress ip = toIp(kApIp);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    bool ok = WiFi.softAP(ap_name, kApPass);
    if (!ok) { vTaskDelay(pdMS_TO_TICKS(500)); ok = WiFi.softAP(ap_name, kApPass); }
    g_dns.setErrorReplyCode(DNSReplyCode::NoError);
    g_dns.start(53, "*", ip);                   // captive: her ad 192.168.4.1
    g_ap = true;
    Serial.printf("[NET] Kurulum AP'si %s: %s  sifre: %s  adres: %s\n", ok ? "acik" : "ACILAMADI", ap_name, kApPass, kApIp);
    note(ok ? cc::Severity::WARNING : cc::Severity::CRITICAL, cc::EvCode::NET_AP_ON);
  }
  if (a.stop_sta) WiFi.disconnect(false, false);
  if (a.begin_sta) {
    WiFi.setHostname(n.mdns);
    WiFi.mode(g_ap ? WIFI_AP_STA : WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);               // yeniden deneme FSM'de (tek yol)
    WiFi.disconnect(false, false);
    if (a.use_static) {
      const IPAddress gw = toIp(n.gw);
      WiFi.config(toIp(n.ip), gw, toIp(n.sn), n.d1[0] ? toIp(n.d1) : gw, n.d2[0] ? toIp(n.d2) : IPAddress(0, 0, 0, 0));
    } else {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    WiFi.begin(ssid, pass[0] ? pass : nullptr);
    Serial.printf("[NET] '%s' baglaniliyor (%s)\n", ssid, a.use_static ? "statik" : "DHCP");
  }
  if (a.stop_ap) {
    g_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    g_ap = false;
    Serial.println("[NET] Kurulum AP'si kapatildi");
    note(cc::Severity::INFO, cc::EvCode::NET_AP_OFF);
  }
  if (a.start_services) {
    strncpy(g_gw, WiFi.gatewayIP().toString().c_str(), sizeof g_gw - 1);
    const bool md = MDNS.begin(n.mdns);
    if (md) MDNS.addService("http", "tcp", 80);
    startOta(n.mdns, ota);
    startSntp();
    g_services = true;
    Serial.printf("[NET] Baglandi: %s  mDNS: %s.local%s  OTA: %s\n", WiFi.localIP().toString().c_str(), n.mdns,
                  md ? "" : " (HATA)", ota[0] ? "hazir" : "kapali (parola yok)");
    Lock l;
    g_status.mdns_ok = md;
  }
  switch (a.ev) {
    case cc::NetEvent::CONNECTED: note(cc::Severity::INFO, cc::EvCode::NET_CONNECTED); break;
    case cc::NetEvent::CONNECTED_DHCP_FALLBACK: note(cc::Severity::WARNING, cc::EvCode::NET_DHCP_FALLBACK); break;
    case cc::NetEvent::DISCONNECTED:
      note(cc::Severity::WARNING, cc::EvCode::NET_DISCONNECTED);
      Serial.println("[NET] Wi-Fi baglantisi koptu");
      break;
    case cc::NetEvent::TIMEOUT_TO_AP: Serial.println("[NET] Baglanilamadi; kurulum AP'si acik, 5 dk'da bir yeniden denenecek"); break;
    case cc::NetEvent::STATIC_TO_DHCP: Serial.println("[NET] Statik IP basarisiz/gecersiz; DHCP deneniyor"); break;
    default: break;
  }
}

void netTask(void*) {
  char ap_name[24];
  {
    const uint32_t id = (uint32_t)(ESP.getEfuseMac() >> 16);   // SCADA ailesi: SCADA_AP_<chipId32>
    snprintf(ap_name, sizeof ap_name, "SCADA_AP_%08lX", (unsigned long)id);
    Lock l;
    strncpy(g_status.ap_name, ap_name, sizeof g_status.ap_name - 1);
  }
  bool web_started = false;
  uint32_t last_fsm = 0;
  for (;;) {
    const uint32_t now = millis();
    if (now - last_fsm >= 100 || !web_started) {
      last_fsm = now;
      NetSettings n;
      char ssid[33], pass[65], ota[33];
      bool reconnect, ota_reload;
      {
        Lock l;
        n = g_cfg;
        memcpy(ssid, g_ssid, sizeof ssid);
        memcpy(pass, g_pass, sizeof pass);
        memcpy(ota, g_ota, sizeof ota);
        reconnect = g_reconnect;
        g_reconnect = false;
        ota_reload = g_ota_reload;
        g_ota_reload = false;
      }
      if (ota_reload && g_ota_started) { ArduinoOTA.end(); g_ota_started = false; }
      if (ota_reload && g_services) startOta(n.mdns, ota);
      strncpy(g_ntp_buf, n.ntp, sizeof g_ntp_buf - 1);
      if (reconnect) g_fsm.requestReconnect();
      cc::NetInput in;
      in.configured = ssid[0] != 0;
      in.static_enabled = n.st;
      in.static_valid = n.st && staticValid(n);
      in.sta_connected = WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
      const cc::NetActions a = g_fsm.step(in, now);
      apply(a, n, ssid, pass, ota, ap_name);
      memset(pass, 0, sizeof pass);
      if (!web_started) { web::start(); web_started = true; }   // soket, WiFi.mode sonrası açılır
      {
        Lock l;
        Status& s = g_status;
        s.ap_mode = g_ap;
        s.sta_ok = g_fsm.online();
        s.configured = in.configured;
        s.static_failed = g_fsm.staticFailed() && n.st;
        s.services = g_services;
        s.ota_ready = g_ota_started;
        s.phase = (uint8_t)g_fsm.phase();
        s.retry_s = g_fsm.retryInMs(now) / 1000;
        if (a.ev == cc::NetEvent::DISCONNECTED) ++s.reconnects;
        s.rssi = s.sta_ok ? (int8_t)WiFi.RSSI() : 0;
        strncpy(s.ip, (s.sta_ok ? WiFi.localIP() : (g_ap ? toIp(kApIp) : IPAddress(0, 0, 0, 0))).toString().c_str(), sizeof s.ip - 1);
        strncpy(s.ssid, ssid, sizeof s.ssid - 1);
        if (s.static_failed && s.sta_ok) snprintf(s.note, sizeof s.note, "Statik IP ile bağlanılamadı; DHCP ile alınan adres: %s", s.ip);
        else s.note[0] = 0;
        s.clock_valid = g_synced.load() && (int64_t)time(nullptr) >= kMinValidEpoch;
      }
    }
    if (g_ap) g_dns.processNextRequest();
    web::handle();
    if (g_ota_started) ArduinoOTA.handle();
    bool reboot;
    uint32_t at;
    { Lock l; reboot = g_reboot; at = g_reboot_at; }
    if (reboot && (int32_t)(millis() - at) >= 0) {
      Serial.println("[NET] Yeniden baslatiliyor");
      Serial.flush();
      ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

}  // namespace

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  load();
  web::begin();   // rotalar (soket henüz açılmaz)
  xTaskCreatePinnedToCore(netTask, "net", 8192, nullptr, 3, nullptr, 0);
}

Status status() { Lock l; return g_status; }
NetSettings settings() { Lock l; return g_cfg; }
bool passSet() { Lock l; return g_pass[0] != 0; }
bool otaPasswordSet() { Lock l; return g_ota[0] != 0; }
bool clockValid() { Lock l; return g_status.clock_valid; }
int64_t epochUtc() { return (int64_t)time(nullptr); }
bool wifiConfigured() { Lock l; return g_ssid[0] != 0; }
bool wifiOk() { Lock l; return g_status.sta_ok; }

static bool mdnsOk(const char* s) {
  const size_t n = strlen(s);
  if (n < 1 || n > 63 || s[0] == '-' || s[n - 1] == '-') return false;
  for (size_t i = 0; i < n; ++i) if (!isalnum((unsigned char)s[i]) && s[i] != '-') return false;
  return true;
}

bool apply(const NetSettings& c, const char* ssid, const char* pass, const char** err, const char** field, bool* reconnect) {
  *err = nullptr; *field = nullptr;
  auto bad = [&](const char* f, const char* e) { *field = f; *err = e; return false; };
  if (!c.adn[0] || strlen(c.adn) > 64) return bad("adN", "Cihaz adı 1–64 karakter olmalı.");
  if (!mdnsOk(c.mdns)) return bad("mdns", "mDNS adı harf, rakam ve tire içerebilir (1–63, tireyle başlayıp bitemez).");
  if (!c.ntp[0] || strlen(c.ntp) > 63) return bad("ntp", "NTP sunucusu 1–63 karakter olmalı.");
  if (c.st) {
    uint32_t ip, sn, gw, d;
    if (!cc::parseIpv4(c.ip, ip)) return bad("staticIP", "IP adresi geçersiz.");
    if (!cc::parseIpv4(c.gw, gw)) return bad("gateway", "Ağ geçidi geçersiz.");
    if (!cc::parseIpv4(c.sn, sn)) return bad("subnet", "Alt ağ maskesi geçersiz.");
    const char* e = nullptr;
    if (!cc::validStaticIpv4(ip, sn, gw, &e)) return bad("staticIP", e);
    if (c.d1[0] && !cc::parseIpv4(c.d1, d)) return bad("dns1", "Birincil DNS geçersiz.");
    if (c.d2[0] && !cc::parseIpv4(c.d2, d)) return bad("dns2", "İkincil DNS geçersiz.");
  }
  if (ssid && (!ssid[0] || strlen(ssid) > 32)) return bad("ssid", "Wi-Fi adı 1–32 karakter olmalı.");
  if (pass && pass[0] && (strlen(pass) < 8 || strlen(pass) > 64)) return bad("pass", "Wi-Fi parolası 8–64 karakter olmalı (boş = açık ağ).");
  NetSettings old;
  char old_ssid[33], old_pass[65];
  { Lock l; old = g_cfg; memcpy(old_ssid, g_ssid, sizeof old_ssid); memcpy(old_pass, g_pass, sizeof old_pass); }
  const bool wifi_changed = (ssid && strcmp(ssid, old_ssid)) || (pass && strcmp(pass, old_pass));
  const bool net_changed = wifi_changed || c.st != old.st || strcmp(c.ip, old.ip) || strcmp(c.gw, old.gw) ||
                           strcmp(c.sn, old.sn) || strcmp(c.d1, old.d1) || strcmp(c.d2, old.d2) || strcmp(c.mdns, old.mdns);
  Preferences p;
  if (!p.begin("net", false)) return bad(nullptr, "Kayıt alanı açılamadı; önceki ayarlar korundu.");
  bool ok = putS(p, "adn", c.adn) && putS(p, "mdns", c.mdns) && p.putBool("st", c.st) == 1 && putS(p, "ip", c.ip) &&
            putS(p, "gw", c.gw) && putS(p, "sn", c.sn) && putS(p, "d1", c.d1) && putS(p, "d2", c.d2) && putS(p, "ntp", c.ntp);
  if (ok && ssid) ok = putS(p, "ssid", ssid);
  if (ok && pass) ok = putS(p, "pass", pass);
  p.end();
  if (!ok) return bad(nullptr, "Kayıt yazılamadı; NVS hatası.");
  {
    Lock l;
    g_cfg = c;
    if (ssid) strncpy(g_ssid, ssid, sizeof g_ssid - 1), g_ssid[sizeof g_ssid - 1] = 0;
    if (pass) strncpy(g_pass, pass, sizeof g_pass - 1), g_pass[sizeof g_pass - 1] = 0;
    if (net_changed) g_reconnect = true;
  }
  if (reconnect) *reconnect = net_changed;
  if (wifi_changed) note(cc::Severity::WARNING, cc::EvCode::NET_WIFI_CHANGED, cc::CmdSource::LOCAL_WEB);
  return true;
}

bool resetWifi(const char** err) {
  Preferences p;
  if (!p.begin("net", false)) { *err = "Kayıt alanı açılamadı; Wi-Fi silinmedi."; return false; }
  const bool ok = putS(p, "ssid", "") && putS(p, "pass", "") && p.putBool("st", false) == 1;
  p.end();
  if (!ok) { *err = "Wi-Fi bilgileri silinemedi."; return false; }
  {
    Lock l;
    g_ssid[0] = 0; g_pass[0] = 0; g_cfg.st = false;
    g_reconnect = true;
  }
  note(cc::Severity::WARNING, cc::EvCode::NET_WIFI_CLEARED, cc::CmdSource::LOCAL_WEB);
  return true;
}

bool setOtaPassword(const char* pw, const char** err) {
  char hash[33] = "";
  if (pw[0]) {
    if (strlen(pw) < 8 || strlen(pw) > 64) { *err = "OTA parolası 8–64 karakter olmalı."; return false; }
    MD5Builder md;
    md.begin();
    md.add(String(pw));
    md.calculate();
    strncpy(hash, md.toString().c_str(), sizeof hash - 1);
  }
  Preferences p;
  if (!p.begin("net", false)) { *err = "Kayıt alanı açılamadı."; return false; }
  const bool ok = putS(p, "ota", hash);
  p.end();
  if (!ok) { *err = "OTA parolası yazılamadı."; return false; }
  { Lock l; memcpy(g_ota, hash, sizeof g_ota); g_ota_reload = true; }
  return true;
}

void requestReboot(uint32_t delay_ms) {
  Lock l;
  g_reboot = true;
  g_reboot_at = millis() + delay_ms;
}

}  // namespace net
