#include "net_clock.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <atomic>
#include <cstring>

namespace net {

static constexpr int64_t kMinValidEpoch = 1735689600;  // 2025-01-01 — öncesi geçersiz sayılır
static char s_ssid[33] = "";
static char s_ntp[64] = "pool.ntp.org";
static char s_gw[16] = "";
static char s_ip[16] = "0.0.0.0";
static std::atomic<bool> s_synced{false};
static std::atomic<uint32_t> s_sync_ms{0};
static bool s_started = false;

static void onSync(struct timeval*) {
  s_synced.store(true);
  s_sync_ms.store(millis());
}

static void startSntp() {
  // Sunucu işaretçileri statik tamponlardadır (lwIP kopyalamaz). 2. sunucu: ağ geçidi (modem NTP'si).
  if (sntp_enabled()) sntp_stop();
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_set_time_sync_notification_cb(onSync);
  sntp_setservername(0, s_ntp);
  if (s_gw[0]) sntp_setservername(1, s_gw);
  sntp_init();
}

static void connect() {
  if (!s_ssid[0]) return;
  Preferences p;
  char pass[65] = "";
  if (p.begin("net", true)) {
    p.getString("pass", pass, sizeof pass);
    p.end();
  }
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("kulube-iklim");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);  // kimlik yalnız bizim NVS alanımızda
  WiFi.begin(s_ssid, pass[0] ? pass : nullptr);
  memset(pass, 0, sizeof pass);
}

void begin() {
  Preferences p;
  if (p.begin("net", true)) {
    p.getString("ssid", s_ssid, sizeof s_ssid);
    String ntp = p.getString("ntp", "");
    if (ntp.length() > 0 && ntp.length() < sizeof s_ntp) strcpy(s_ntp, ntp.c_str());
    p.end();
  }
  WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t) {
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      strncpy(s_gw, WiFi.gatewayIP().toString().c_str(), sizeof s_gw - 1);
      strncpy(s_ip, WiFi.localIP().toString().c_str(), sizeof s_ip - 1);
      startSntp();  // her bağlantıda idempotent yeniden yapılandırma
    } else if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      strcpy(s_ip, "0.0.0.0");
    }
  });
  connect();
  s_started = true;
}

void service() {}

bool wifiConfigured() { return s_ssid[0] != 0; }
bool wifiOk() { return s_started && WiFi.status() == WL_CONNECTED; }
int8_t rssi() { return wifiOk() ? (int8_t)WiFi.RSSI() : 0; }
const char* ipString() { return s_ip; }
const char* ssid() { return s_ssid; }
const char* ntpServer() { return s_ntp; }

bool clockValid() {
  if (!s_synced.load()) return false;
  return (int64_t)time(nullptr) >= kMinValidEpoch;
}
int64_t epochUtc() { return (int64_t)time(nullptr); }
uint32_t lastSyncAgeS() { return s_synced.load() ? (millis() - s_sync_ms.load()) / 1000u : UINT32_MAX; }

bool setCredentials(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0] || strlen(ssid) > 32 || (pass && strlen(pass) > 64)) return false;
  if (pass && pass[0] && strlen(pass) < 8) return false;  // WPA2 asgari 8 karakter
  Preferences p;
  if (!p.begin("net", false)) return false;
  const bool ok = p.putString("ssid", ssid) > 0 && (pass && pass[0] ? p.putString("pass", pass) > 0 : p.remove("pass") || true);
  p.end();
  if (!ok) return false;
  strncpy(s_ssid, ssid, sizeof s_ssid - 1);
  WiFi.disconnect(false);
  connect();
  return true;
}

bool clearCredentials() {
  Preferences p;
  if (!p.begin("net", false)) return false;
  p.remove("ssid");
  p.remove("pass");
  p.end();
  s_ssid[0] = 0;
  WiFi.disconnect(true);
  return true;
}

bool setNtpServer(const char* host) {
  if (!host || !host[0] || strlen(host) >= sizeof s_ntp) return false;
  Preferences p;
  if (!p.begin("net", false)) return false;
  const bool ok = p.putString("ntp", host) > 0;
  p.end();
  if (!ok) return false;
  strcpy(s_ntp, host);
  if (wifiOk()) startSntp();
  return true;
}

}  // namespace net
