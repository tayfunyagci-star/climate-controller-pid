#include "web.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <functional>
#include <cmath>
#include <cstring>
#include "auth.h"
#include "boot_state.h"
#include "core_api.h"
#include "cc_netfsm.h"
#include "mqtt_cfg.h"
#include "mqtt_client.h"
#include "state_json.h"
#include "version.h"
#include "net_manager.h"
#include "status_led.h"
#include "storage.h"
#include "tasks.h"
#include "trend.h"
#include "ui_generated.h"

namespace web {

namespace {

WebServer g_srv(80);
cc::Event g_ev[200];   // olay kopyası (yalnız NetTask)


void headers(bool api) {
  g_srv.sendHeader("X-Content-Type-Options", "nosniff");
  g_srv.sendHeader("X-Frame-Options", "DENY");
  g_srv.sendHeader("Referrer-Policy", "same-origin");
  if (api) g_srv.sendHeader("Cache-Control", "no-store");
}

void replyJson(int code, JsonDocument& d) {
  String out;
  serializeJson(d, out);
  headers(true);
  g_srv.send(code, "application/json", out);
}

void replyMsg(int code, const char* msg, const char* field = nullptr) {
  JsonDocument d;
  d["message"] = msg;
  if (field) d["field"] = field;
  replyJson(code, d);
}

// Yazma istekleri: özel başlık (CSRF) — scada-ui-design §8.5. Oturum F4'te.
bool writeOk() {
  if (g_srv.header("X-SCADA") != "1") { replyMsg(403, "İstek reddedildi (X-SCADA başlığı yok)."); return false; }
  return true;
}

bool body(JsonDocument& d) {
  if (deserializeJson(d, g_srv.arg("plain")) || !d.is<JsonObject>()) { replyMsg(400, "Geçersiz JSON."); return false; }
  return true;
}

void sendAsset(const ui::Asset& a) {
  headers(!a.immutable);
  g_srv.sendHeader("Content-Security-Policy",
                   "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; connect-src 'self'; "
                   "frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
  if (a.immutable) g_srv.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  g_srv.sendHeader("Content-Encoding", "gzip");
  g_srv.send_P(200, a.type, (const char*)a.data, a.len);
}

const char* onoff(bool b) { return b ? "ON" : "OFF"; }

// Ağ yaşam döngüsü adları (UI sözleşmesi; docs/NETWORK.md §4.1)
const char* phaseName(uint8_t p) {
  static const char* const n[] = {"AP_ONLY", "CONNECTING", "ONLINE", "WAITING"};
  return p < 4 ? n[p] : "?";
}
const char* resultName(uint8_t r) {
  static const char* const n[] = {"NONE", "TRYING", "CONNECTED", "FAILED"};
  return r < 4 ? n[r] : "?";
}
const char* failName(uint8_t f) {
  static const char* const n[] = {"NONE", "NOT_FOUND", "AUTH", "ASSOC", "NO_IP", "SIGNAL_LOST", "OTHER"};
  return f < 7 ? n[f] : "OTHER";
}
// Kurulum bağlamı: ilk kurulum (SSID yok) / kurtarma (kayıtlı ağa bağlanılamadı) / devir / yok
const char* setupName(const net::Status& ns) {
  if (!ns.ap_mode) return "NONE";
  if (ns.handover) return "HANDOVER";
  return ns.configured ? "RECOVERY" : "FIRST";
}
template <typename T>
void num(JsonObject o, const char* k, T v) { o[k] = v; }
void fnum(JsonObject o, const char* k, float v, int dec = 2) {
  if (std::isnan(v)) o[k] = nullptr;
  else { const float p = powf(10.f, (float)dec); o[k] = roundf(v * p) / p; }
}


// ---------------------------------------------------------------- /api/data
// Proses alanları MQTT B/state ile aynı yazıcıdan (state_json); burada yalnız web'e özgü kimlik/ağ/tanı eklenir.
void handleData() {
  app::Frame f;
  if (!app::capture(f, 100)) { replyMsg(503, "Çekirdek meşgul; yeniden deneyin."); return; }
  const net::Status ns = net::status();
  const net::NetSettings nc = net::settings();
  const app::TaskStats ts = app::stats();
  const mq::Status ms = mq::status();

  JsonDocument doc;
  JsonObject d = doc.to<JsonObject>();
  app::writeProcess(d, f);
  app::writeWebExtras(d, f);
  if (!ns.clock_valid) d["ts"] = nullptr;
  d["device_name"] = nc.adn;
  d["ip"] = ns.ip;
  d["mdns"] = nc.mdns;
  d["client_ip"] = g_srv.client().remoteIP().toString();
  d["fw_version"] = app::kFwVersion;
  d["fw_build"] = ui::kUiBuild;
  d["ap_mode"] = ns.ap_mode;
  d["ap_name"] = ns.ap_name;
  d["ap_ip"] = net::kApIp;
  d["wifi_ssid"] = ns.ssid;
  d["net_note"] = ns.note;
  d["wifi_ok"] = ns.sta_ok;
  if (ns.sta_ok) d["wifi_rssi"] = ns.rssi; else d["wifi_rssi"] = nullptr;
  d["mqtt_status"] = mq::stateName(ms.state);
  d["mqtt_reconnects"] = ms.reconnects;
  d["mqtt_note"] = ms.note;
  {
    uint8_t ls[cc::kLedCount];
    leds::states(ls);
    JsonArray la = d["led_states"].to<JsonArray>();
    for (uint8_t v : ls) la.add(v);
    d["led_ok"] = leds::driverOk();
  }
  d["time_valid"] = onoff(ns.clock_valid);
  d["password_set"] = auth::passwordSet();
  d["service_remaining_s"] = f.svc_remaining_s;
  d["ota_password_set"] = net::otaPasswordSet();   // false: OTA parolasız açık → UI kalıcı uyarı
  d["ota_ready"] = ns.ota_ready;
  static const char* const outs[4] = {"r1", "r2", "heater_fan", "ventilation_fan"};
  char k[40];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(k, sizeof k, "%s_switch_count", outs[i]); d[k] = f.sw[i];
    snprintf(k, sizeof k, "%s_hours_total", outs[i]); d[k] = (float)(f.on_ms[i] / 36000ULL) / 100.0f;
    snprintf(k, sizeof k, "svc_test_%s", outs[i]); d[k] = onoff(f.svc[i]);
  }
  JsonArray vs = d["vent_sources"].to<JsonArray>();
  static const char* const vn[5] = {"TEMP_HIGH", "HUMIDITY_HIGH", "MANUAL", "SCHEDULED", "OVERTEMP"};
  for (uint8_t i = 0; i < 5; ++i) if (f.vsrc & (1u << i)) vs.add(vn[i]);
  d["free_heap"] = ESP.getFreeHeap();
  d["min_heap"] = ESP.getMinFreeHeap();
  d["control_loop_max_ms"] = (ts.max_us[2] + 999) / 1000;
  d["reset_reason"] = app::resetReasonName();
  d["fault_boot_count"] = app::rtcFaultBoots();
  d["wifi_reconnects"] = ns.reconnects;
  d["net_phase"] = phaseName(ns.phase);
  d["net_setup"] = setupName(ns);
  d["net_try"] = ns.try_seq;
  d["net_result"] = resultName(ns.result);
  d["net_fail"] = failName(ns.fail);
  d["net_fail_code"] = ns.fail_code;
  d["net_retry_s"] = ns.retry_s;
  d["ap_close_s"] = ns.ap_close_s;
  d["ap_clients"] = ns.ap_clients;
  d["sta_ip"] = ns.sta_ip;
  d["static_ip"] = nc.st;
  fnum(d, "sensor_error_rate_10m", f.err_rate, 0);
  d["sensor_model"] = "DHT22";
  {
    const storage::Status st = storage::status();
    d["config_rev"] = st.config_rev;
    d["boot_count"] = st.boots;
    d["storage_ok"] = st.fs_ok && !st.config_corrupt;
    d["storage_saves"] = st.saves;
    d["storage_errors"] = st.errors;
    d["storage_note"] = st.last_error;
  }
  replyJson(200, doc);
}

// ---------------------------------------------------------------- /api/cmd
void handleCmd() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  const char* id = b["id"] | "";
  const char* value = b["value"] | "";
  cc::CmdReply r;
  uint32_t seq = 0;
  if (!app::coreLock(200)) { replyMsg(503, "Komut kuyruğu meşgul."); return; }
  r = app::core().command(id, value, cc::CmdSource::LOCAL_WEB);
  seq = app::core().snapshot().seq + 1;   // bir sonraki yayın komutu yansıtır
  app::coreUnlock();
  JsonDocument d;
  d["id"] = id;
  d["value"] = value;
  d["result"] = cc::name(r.result);
  d["reason"] = cc::name(r.reason);
  d["seq"] = seq;
  const bool ok = r.result == cc::CmdResult::ACCEPTED || r.result == cc::CmdResult::OVERRIDDEN;
  if (!ok) d["message"] = cc::name(r.result);
  replyJson(ok ? 200 : 409, d);
}

// ---------------------------------------------------------------- Wi-Fi tarama (SCADA ailesi sözleşmesi)
void handleScan() {
  JsonDocument d;
  JsonArray a = d["networks"].to<JsonArray>();
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    d["pending"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    d["pending"] = true;
  } else {
    d["pending"] = false;
    for (int i = 0; i < n && i < 30; ++i) {
      JsonObject o = a.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      o["channel"] = WiFi.channel(i);
    }
    WiFi.scanDelete();
  }
  replyJson(200, d);
}

// ---------------------------------------------------------------- /api/settings
void settingsGet() {
  cc::Config c;
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
  c = app::core().config();
  app::coreUnlock();
  JsonDocument doc;
  JsonObject d = doc.to<JsonObject>();
  for (size_t i = 0; i < cc::fieldCount(); ++i) {
    const cc::FieldInfo& f = cc::fieldAt(i);
    const float v = cc::fieldValue(c, f);
    switch (f.kind) {
      case cc::FieldKind::FLOAT: d[f.key] = v; break;
      case cc::FieldKind::INT: d[f.key] = (int32_t)lroundf(v); break;
      case cc::FieldKind::BOOL: d[f.key] = v != 0; break;
      case cc::FieldKind::ENUM: d[f.key] = ((uint8_t)v < f.enumCount) ? f.enumNames[(uint8_t)v] : "?"; break;
    }
  }
  const net::NetSettings n = net::settings();
  const net::Status ns = net::status();
  d["adN"] = n.adn;
  d["mdns"] = n.mdns;
  d["staticEnabled"] = n.st;
  d["staticIP"] = n.ip;
  d["gateway"] = n.gw;
  d["subnet"] = n.sn;
  d["dns1"] = n.d1;
  d["dns2"] = n.d2;
  d["ntp_server"] = n.ntp;
  {
    char slug[24];
    mqttcfg::slug(slug);   // MQTT_INTEGRATION §2: kulube_iklim_ + MAC son 3 bayt
    d["slug"] = slug;
  }
  {
    const cc::LedConfig lc = leds::config();
    d["ledB"] = lc.brightness;
    char key[5], hex[8];
    for (uint8_t i = 0; i < cc::kLedCount; ++i)
      for (uint8_t k = 0; k < cc::kLedStates; ++k) {
        snprintf(key, sizeof key, "%s%u", cc::ledGroup(i).key, (unsigned)k);
        cc::formatHexColor(lc.color[i][k], hex);
        d[key] = hex;
      }
    d["ledOk"] = leds::driverOk();
  }
  {
    const mqttcfg::Settings ms = mqttcfg::settings();
    d["mqtt_host"] = ms.host;
    d["mqtt_port"] = ms.port;
    d["mqtt_user"] = ms.user;
    d["mqtt_base"] = ms.base;
    d["mqPwSet"] = mqttcfg::passSet();
    char sl[24], tb[128];
    mqttcfg::slug(sl);
    snprintf(tb, sizeof tb, "%s/%s", ms.base, sl);
    d["mqtt_topic_base"] = tb;
  }
  {
    const auth::Settings as = auth::settings();
    d["user"] = as.user;
    d["guestRead"] = as.guest_read;
    d["session_hours"] = as.session_h;
    d["passwordSet"] = auth::passwordSet();
    d["servicePinSet"] = auth::pinSet();
  }
  d["ssid"] = ns.ssid;
  d["passSet"] = net::passSet();
  d["otaPasswordSet"] = net::otaPasswordSet();
  d["apName"] = ns.ap_name;
  d["devName"] = n.adn;
  d["ip"] = ns.ip;
  d["fwVersion"] = app::kFwVersion;
  d["fwBuild"] = ui::kUiBuild;
  replyJson(200, doc);
}

// MQTT bölümü: metin alanları mqtt_cfg (NVS "mqtt"), çekirdek alanları (yayın aralıkları, keşif, uzak yetkiler)
// hem çekirdek konfigürasyonuna hem NVS'e. Aday bütünüyle doğrulanır; hata = hiçbir şey değişmez. Yanıt hata
// durumunda gönderilmiş olur (false).
bool mqttSection(JsonDocument& b) {
  mqttcfg::Settings s = mqttcfg::settings();
  auto str = [&](const char* key, char* dst, size_t cap) -> bool {
    if (b[key].isNull()) return true;
    if (!b[key].is<const char*>() || strlen(b[key].as<const char*>()) >= cap) { replyMsg(400, "Metin çok uzun veya geçersiz.", key); return false; }
    strcpy(dst, b[key].as<const char*>());
    return true;
  };
  if (!str("mqtt_host", s.host, sizeof s.host) || !str("mqtt_user", s.user, sizeof s.user) || !str("mqtt_base", s.base, sizeof s.base)) return false;
  if (!b["mqtt_port"].isNull()) {
    const float v = b["mqtt_port"].as<float>();
    if (!b["mqtt_port"].is<float>() || v < 1 || v > 65535 || v != floorf(v)) { replyMsg(400, "Broker portu 1–65535 olmalı.", "mqtt_port"); return false; }
    s.port = (uint16_t)v;
  }
  const char* pass = nullptr;
  if (!b["mqtt_password"].isNull()) {
    if (!b["mqtt_password"].is<const char*>()) { replyMsg(400, "Geçersiz parola.", "mqtt_password"); return false; }
    pass = b["mqtt_password"].as<const char*>();
  }
  const char* err = nullptr;
  const char* field = nullptr;
  if (!mqttcfg::validate(s, pass, &err, &field)) { replyMsg(400, err, field); return false; }
  cc::Config cur, cand;
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return false; }
  cur = app::core().config();
  app::coreUnlock();
  cand = cur;
  for (const char* k : mqttcfg::kCoreKeys) {
    const JsonVariantConst v = b[k];
    if (v.isNull()) continue;
    char payload[24];
    if (v.is<bool>()) strcpy(payload, v.as<bool>() ? "ON" : "OFF");
    else if (v.is<float>()) snprintf(payload, sizeof payload, "%g", v.as<float>());
    else { replyMsg(400, "Geçersiz değer.", k); return false; }
    const cc::SetResult r = cc::setField(cand, k, payload, cc::CmdSource::LOCAL_WEB, cand);
    if (r.result != cc::CmdResult::ACCEPTED) {
      char msg[96];
      snprintf(msg, sizeof msg, "Değer kabul edilmedi (%s).", cc::name(r.code));
      replyMsg(400, msg, k);
      return false;
    }
  }
  // Anonim broker'da MQTT'den konfigürasyon yazımı açılamaz (SECURITY)
  if (cand.remote_config_enabled && !s.user[0]) { replyMsg(400, "Anonim broker’da (kullanıcı adı boş) MQTT’den konfigürasyon yazımı açılamaz.", "remote_config_enabled"); return false; }
  if (!mqttcfg::apply(s, pass, cand, &err, &field)) { replyMsg(field ? 400 : 507, err, field); return false; }
  bool changed = false;
  for (const char* k : mqttcfg::kCoreKeys) {
    const cc::FieldInfo* f = cc::findField(k);
    if (f && cc::fieldValue(cand, *f) != cc::fieldValue(cur, *f)) changed = true;
  }
  if (changed) {
    cc::CmdReply r;
    if (!app::coreLock(200)) { replyMsg(503, "Kaydedildi ancak çekirdek meşgul; yeniden başlatmada uygulanacak."); return false; }
    r = app::core().applyConfig(cand, cc::CmdSource::LOCAL_WEB);
    app::coreUnlock();
    if (r.result != cc::CmdResult::ACCEPTED) { replyMsg(409, "Kaydedildi ancak çalışan konfigürasyona uygulanamadı; yeniden başlatmada uygulanacak."); return false; }
  }
  return true;
}

// Ayarlar bölüm bölüm kaydedilir (UI her sekmeyi ayrı gönderir): gövde yalnız o bölümün alanlarını taşır,
// gövdede olmayan alan korunur. Ağ + kablosuz kimlik ve LED F2'de kalıcıdır; diğer bölümler kalıcı depo (F3)
// gelene kadar değişen değerde reddedilir. Bir bölümün reddi başka bölümün kaydını engellemez.
void settingsPost() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  static const char* const netKeys[] = {"adN", "mdns", "staticEnabled", "staticIP", "gateway", "subnet", "dns1",
                                        "dns2", "ntp_server", "ssid", "pass", "clearWifiPassword"};
  bool anyNet = false, anyLed = false, anyMqtt = false, anyCore = false, anyAccess = false, needReboot = false;
  cc::Config coreCur, coreCand;
  cc::LedConfig lc = leds::config();
  for (JsonPair kv : b.as<JsonObject>()) {
    bool known = false;
    for (const char* k : netKeys) if (!strcmp(kv.key().c_str(), k)) { known = true; break; }
    if (known) { anyNet = true; continue; }
    if (mqttcfg::isStringKey(kv.key().c_str()) || mqttcfg::isCoreKey(kv.key().c_str())) { anyMqtt = true; continue; }
    if (!strcmp(kv.key().c_str(), "user") || !strcmp(kv.key().c_str(), "guestRead") || !strcmp(kv.key().c_str(), "session_hours")) { anyAccess = true; continue; }
    uint8_t li = 0, ls = 0;
    if (!strcmp(kv.key().c_str(), "ledB")) {
      const float v = kv.value().as<float>();
      if (!kv.value().is<float>() || v < 0 || v > 100 || v != floorf(v)) { replyMsg(400, "LED parlaklığı %0–100 tam sayı olmalı.", "ledB"); return; }
      lc.brightness = (uint8_t)v;
      anyLed = true;
      continue;
    }
    if (cc::ledColorKey(kv.key().c_str(), li, ls)) {
      uint32_t rgb = 0;
      if (!cc::parseHexColor(kv.value().as<const char*>(), rgb)) { replyMsg(400, "LED rengi #rrggbb biçiminde olmalı.", kv.key().c_str()); return; }
      lc.color[li][ls] = rgb;
      anyLed = true;
      continue;
    }
    const cc::FieldInfo* f = cc::findField(kv.key().c_str());
    if (!f) {
      // Bu yazılımda henüz karşılığı olmayan UI alanları (erişim F4 …): GET bunları göndermez,
      // form boş/varsayılan gönderir. Dolu gelen değer sessizce yok sayılmaz, reddedilir.
      const JsonVariantConst v = kv.value();
      const bool empty = v.isNull() || (v.is<const char*>() && !*v.as<const char*>()) || (v.is<bool>() && !v.as<bool>()) ||
                         (v.is<float>() && v.as<float>() == 0.0f);
      if (empty) continue;
      replyMsg(409, "Bu ayar sonraki fazda (erişim F4) etkinleşecek; şimdilik kaydedilemez.", kv.key().c_str());
      return;
    }
    // Çekirdek konfigürasyon alanı (Sensörler/Kontrol/Güvenlik): adaya işlenir; tamamı doğrulanınca uygulanır
    if (!anyCore) {
      if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
      coreCur = app::core().config();
      app::coreUnlock();
      coreCand = coreCur;
      anyCore = true;
    }
    char payload[32];
    const JsonVariantConst v = kv.value();
    if (v.is<bool>()) strcpy(payload, v.as<bool>() ? "ON" : "OFF");
    else if (v.is<float>()) snprintf(payload, sizeof payload, "%.6g", v.as<float>());
    else if (v.is<const char*>()) snprintf(payload, sizeof payload, "%s", v.as<const char*>());
    else { replyMsg(400, "Geçersiz değer.", kv.key().c_str()); return; }
    const cc::SetResult r = cc::setField(coreCand, kv.key().c_str(), payload, cc::CmdSource::LOCAL_WEB, coreCand);
    if (r.result != cc::CmdResult::ACCEPTED) {
      char msg[120];
      if (r.result == cc::CmdResult::REJECTED_RELATION)
        snprintf(msg, sizeof msg, "Alanlar arası kural ihlali (V%u, %s). İlişkili alanları birlikte düzeltin.", (unsigned)r.rule, cc::name(r.code));
      else snprintf(msg, sizeof msg, "Değer kabul edilmedi (%s).", cc::name(r.code));
      replyMsg(400, msg, kv.key().c_str());
      return;
    }
    if (f->flags & cc::CF_REBOOT && cc::fieldValue(coreCand, *f) != cc::fieldValue(coreCur, *f)) needReboot = true;
  }
  if (anyCore) {
    // Adayın tamamı (V1–V17 + programlar) çekirdekte doğrulanır; hata = hiçbir alan uygulanmaz
    cc::CmdReply r;
    if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
    r = app::core().applyConfig(coreCand, cc::CmdSource::LOCAL_WEB);
    app::coreUnlock();
    if (r.result != cc::CmdResult::ACCEPTED) {
      char msg[120];
      snprintf(msg, sizeof msg, "Kaydedilmedi: %s (%s).", cc::name(r.result), cc::name(r.code));
      replyMsg(409, msg);
      return;
    }
  }
  if (anyMqtt && !mqttSection(b)) return;
  if (anyAccess) {
    auth::Settings as = auth::settings();
    if (b["user"].is<const char*>()) { strncpy(as.user, b["user"].as<const char*>(), sizeof as.user - 1); as.user[sizeof as.user - 1] = 0; }
    if (b["guestRead"].is<bool>()) as.guest_read = b["guestRead"].as<bool>();
    if (!b["session_hours"].isNull()) {
      const float h = b["session_hours"].as<float>();
      if (h < 1 || h > 24 || h != floorf(h)) { replyMsg(400, "Oturum süresi 1–24 saat olmalı.", "session_hours"); return; }
      as.session_h = (uint8_t)h;
    }
    const char* err = nullptr;
    const char* field = nullptr;
    if (!auth::applySettings(as, &err, &field)) { replyMsg(field ? 400 : 507, err, field); return; }
  }
  const net::Status ns = net::status();   // net_try tabanı: UI bu değerden büyük denemenin sonucunu bekler
  if (!anyNet) {
    const char* err = nullptr;
    if (anyLed && !leds::apply(lc, &err)) { replyMsg(507, err); return; }
    JsonDocument d;
    d["message"] = anyAccess ? "Erişim ayarları kaydedildi" : anyCore ? (needReboot ? "Kaydedildi. Bu değişiklik yeniden başlatmadan sonra geçerli olur." : "Kaydedildi")
                   : anyLed ? "LED ayarları kaydedildi"
                   : anyMqtt ? "MQTT ayarları kaydedildi. Bağlantı yeni ayarlarla yeniden kuruluyor; sonuç MQTT durumunda görünür."
                             : "Kaydedildi";
    d["reconnect"] = false;
    d["net_try_base"] = ns.try_seq;
    replyJson(200, d);
    return;
  }
  net::NetSettings n = net::settings();
  auto str = [&](const char* key, char* dst, size_t cap) {
    if (!b[key].is<const char*>()) return;
    strncpy(dst, b[key].as<const char*>(), cap - 1);
    dst[cap - 1] = 0;
  };
  str("adN", n.adn, sizeof n.adn);
  str("mdns", n.mdns, sizeof n.mdns);
  if (b["staticEnabled"].is<bool>()) n.st = b["staticEnabled"].as<bool>();
  str("staticIP", n.ip, sizeof n.ip);
  str("gateway", n.gw, sizeof n.gw);
  str("subnet", n.sn, sizeof n.sn);
  str("dns1", n.d1, sizeof n.d1);
  str("dns2", n.d2, sizeof n.d2);
  str("ntp_server", n.ntp, sizeof n.ntp);
  const char* ssid = b["ssid"].is<const char*>() ? b["ssid"].as<const char*>() : nullptr;
  const char* pass = b["pass"].is<const char*>() ? b["pass"].as<const char*>() : nullptr;
  if (b["clearWifiPassword"] | false) pass = "";
  // Kimliği değiştirmeyen tekrar gönderim (ana formdaki mevcut SSID) kimlik yazımı sayılmaz
  if (ssid && !pass && !strcmp(ssid, ns.ssid)) ssid = nullptr;
  const char* err = nullptr;
  const char* field = nullptr;
  bool reconnect = false;
  if (!net::apply(n, ssid, pass, &err, &field, &reconnect)) { replyMsg(field ? 400 : 507, err, field); return; }
  if (anyLed && !leds::apply(lc, &err)) { replyMsg(507, err); return; }
  // Kayıt ≠ bağlantı: yanıt yalnız kalıcı kaydı onaylar; bağlantı sonucu /api/data net_try/net_result ile izlenir
  char msg[160];
  if (ssid) snprintf(msg, sizeof msg, "Ayarlar kaydedildi. Cihaz “%s” ağına bağlanmayı deneyecek.", ssid);
  else if (reconnect) snprintf(msg, sizeof msg, "Ayarlar kaydedildi. Ağ bağlantısı yeni ayarlarla yeniden kurulacak.");
  else snprintf(msg, sizeof msg, "Kaydedildi");
  JsonDocument d;
  d["message"] = msg;
  d["reconnect"] = reconnect;
  d["net_try_base"] = ns.try_seq;
  replyJson(200, d);
}

// OTA parolası: ayrı form (Ayarlar › Erişim). "" = kaldır → OTA parolasız açık kalır (D-17, F2.5).
// Yeni parola OTA sunucusu yeniden kurulunca (yeniden başlatmadan) geçerli olur.
void handleOtaPassword() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  if (!b["password"].is<const char*>()) { replyMsg(400, "password alanı gerekli (boş = parolayı kaldır).", "otaPw"); return; }
  const char* pw = b["password"].as<const char*>();
  const char* err = nullptr;
  if (!net::setOtaPassword(pw, &err)) { replyMsg(pw[0] ? 400 : 507, err, "otaPw"); return; }
  JsonDocument d;
  d["message"] = pw[0] ? "OTA parolası kaydedildi; yüklemede --auth gerekir." : "OTA parolası kaldırıldı; OTA parolasız açık.";
  d["otaPasswordSet"] = pw[0] != 0;
  replyJson(200, d);
}

void handleNetRetry() {
  if (!writeOk()) return;
  const net::Status ns = net::status();
  if (!net::retryNow()) { replyMsg(409, "Kayıtlı Wi-Fi ağı yok; önce bir ağ seçin."); return; }
  JsonDocument d;
  d["message"] = "Kayıtlı ağ yeniden deneniyor.";
  d["net_try_base"] = ns.try_seq;
  replyJson(200, d);
}

void handleNetFinish() {
  if (!writeOk()) return;
  if (!net::finishSetup()) { replyMsg(409, "Kurulum ağı devir durumunda değil; kapatılacak bir şey yok."); return; }
  replyMsg(200, "Kurulum ağı kapatılıyor.");
}

void handleResetWifi() {
  if (!writeOk()) return;
  const char* err = nullptr;
  if (!net::resetWifi(&err)) { replyMsg(507, err); return; }
  char msg[160];
  snprintf(msg, sizeof msg, "Wi-Fi bilgileri silindi. Kurulum ağı %s açılıyor; adres http://%s", net::status().ap_name, net::kApIp);
  replyMsg(200, msg);
}

void handleReboot() {
  if (!writeOk()) return;
  bool busy = true;
  if (app::coreLock(200)) {
    const bool* o = app::core().outputs();
    busy = o[cc::R1] || o[cc::R2] || app::core().snapshot().post_cool_remaining_s > 0;
    app::coreUnlock();
  }
  if (busy) { replyMsg(409, "Isıtma veya fan soğutması sürüyor. Modu KAPALI yapın ve soğutmanın bitmesini bekleyin."); return; }
  replyMsg(200, "Cihaz yeniden başlatılıyor");
  net::requestReboot(800);
}

// ---------------------------------------------------------------- olaylar / alarmlar / programlar
void handleEvents() {
  uint16_t n = 0;
  uint32_t overwritten = 0, up_now = 0;
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
  const auto& r = app::core().events();
  n = r.size();
  for (uint16_t i = 0; i < n; ++i) g_ev[i] = r.at(i);
  overwritten = r.overwritten();
  up_now = app::core().uptimeMs() / 1000;
  app::coreUnlock();
  const bool clock = net::clockValid();
  const int64_t now = (int64_t)time(nullptr);
  JsonDocument doc;
  JsonArray a = doc["events"].to<JsonArray>();
  char msg[64];
  // Önceki açılışlardan kalıcı WARNING+ olaylar (F3): epoch kayıt anında damgalandı
  static cc::Event pe[cc::kPersistEvents];
  static int64_t pts[cc::kPersistEvents];
  const uint8_t np = storage::persistedEvents(pe, pts, cc::kPersistEvents);
  for (uint8_t i = 0; i < np; ++i) {
    const cc::Event& e = pe[i];
    JsonObject o = a.add<JsonObject>();
    o["seq"] = e.seq;
    o["up"] = e.up_s;
    if (pts[i]) o["ts"] = pts[i]; else o["ts"] = nullptr;
    o["sev"] = cc::name(e.sev);
    o["src"] = cc::name(e.src);
    if (std::isnan(e.val)) snprintf(msg, sizeof msg, "%s", cc::name(e.code));
    else snprintf(msg, sizeof msg, "%s %.2f", cc::name(e.code), e.val);
    o["msg"] = msg;
    if (e.actor != cc::CmdSource::SYSTEM) o["actor"] = cc::name(e.actor);
    o["prev_boot"] = true;
  }
  for (uint16_t i = 0; i < n; ++i) {
    const cc::Event& e = g_ev[i];
    JsonObject o = a.add<JsonObject>();
    o["seq"] = e.seq;
    o["up"] = e.up_s;
    if (clock) o["ts"] = now - (int64_t)(up_now - e.up_s); else o["ts"] = nullptr;
    o["sev"] = cc::name(e.sev);
    o["src"] = cc::name(e.src);
    if (std::isnan(e.val)) snprintf(msg, sizeof msg, "%s", cc::name(e.code));
    else snprintf(msg, sizeof msg, "%s %.2f", cc::name(e.code), e.val);
    o["msg"] = msg;
    if (e.actor != cc::CmdSource::SYSTEM) o["actor"] = cc::name(e.actor);
  }
  doc["overwritten"] = overwritten;
  replyJson(200, doc);
}

const char* alarmStateName(cc::AlarmState s) {
  switch (s) {
    case cc::AlarmState::ACTIVE_UNACK: return "active_unacknowledged";
    case cc::AlarmState::ACTIVE_ACK: return "active_acknowledged";
    case cc::AlarmState::CLEARED_UNACK: return "cleared_unacknowledged";
    case cc::AlarmState::LATCHED: return "latched";
    default: return "normal";
  }
}

void handleAlarms() {
  cc::AlarmRec recs[cc::kAlarmCount];
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
  for (uint8_t i = 0; i < cc::kAlarmCount; ++i) recs[i] = app::core().alarms().rec((cc::AlarmId)i);
  app::coreUnlock();
  JsonDocument doc;
  JsonArray a = doc["active"].to<JsonArray>();
  for (uint8_t i = 0; i < cc::kAlarmCount; ++i) {
    const cc::AlarmRec& r = recs[i];
    if (r.st == cc::AlarmState::NORMAL || r.st == cc::AlarmState::PENDING) continue;
    JsonObject o = a.add<JsonObject>();
    o["code"] = cc::name((cc::AlarmId)i);
    o["sev"] = cc::name(r.sev);
    o["state"] = alarmStateName(r.st);
    o["latched"] = r.st == cc::AlarmState::LATCHED;
    o["occ"] = r.occ;
    o["since"] = nullptr;
  }
  doc["history"].to<JsonArray>();
  replyJson(200, doc);
}

void handleAlarmAck() {
  if (!writeOk()) return;
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().alarmAck(cc::CmdSource::LOCAL_WEB);
  app::coreUnlock();
  replyMsg(r.result == cc::CmdResult::ACCEPTED ? 200 : 409, r.result == cc::CmdResult::ACCEPTED ? "Onaylandı" : cc::name(r.result));
}

void handleAlarmReset() {
  if (!writeOk()) return;
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().alarmReset(cc::CmdSource::LOCAL_WEB);
  app::coreUnlock();
  if (r.result == cc::CmdResult::ACCEPTED) replyMsg(200, "Kilit sıfırlandı");
  else replyMsg(409, r.result == cc::CmdResult::REJECTED_STATE ? "Koşul sürüyor; kilit sıfırlanamaz." : cc::name(r.result));
}

void hhmm(uint16_t m, char out[8]) { m %= 1440; snprintf(out, 8, "%02u:%02u", (unsigned)(m / 60) % 24u, (unsigned)(m % 60)); }

void handlePrograms() {
  static cc::Program list[cc::kMaxPrograms];
  uint8_t n = 0;
  bool en = false;
  cc::ScheduleResult sr;
  int64_t local = 0;
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
  n = app::core().programCount();
  for (uint8_t i = 0; i < n; ++i) list[i] = app::core().programs()[i];
  en = app::core().programsEnabled();
  sr = app::core().schedule();
  local = app::core().localMinutes();
  app::coreUnlock();
  JsonDocument doc;
  doc["enabled"] = en;
  doc["time_valid"] = net::clockValid();
  doc["now"] = local;
  doc["tz_offset_min"] = 180;
  JsonObject act = doc["active"].to<JsonObject>();
  act["climate"] = sr.climate.index;
  act["vent"] = sr.vent.index;
  act["until"] = sr.climate.index >= 0 ? sr.climate.end : -1;
  act["vent_until"] = sr.vent.index >= 0 ? sr.vent.end : -1;
  act["held"] = sr.held;
  doc["next_change"] = sr.next_change;
  JsonArray a = doc["list"].to<JsonArray>();
  char t[11];
  for (uint8_t i = 0; i < n; ++i) {
    const cc::Program& p = list[i];
    JsonObject o = a.add<JsonObject>();
    o["name"] = p.name;
    o["enabled"] = p.enabled;
    o["kind"] = cc::name(p.kind);
    o["days"] = p.days;
    cc::formatDate(p.date_from, t); o["date_from"] = t;
    cc::formatDate(p.date_to, t); o["date_to"] = t;
    char hm[8];
    hhmm(p.start_min, hm); o["start"] = hm;
    o["end"] = cc::name(p.end);
    hhmm(p.end_min, hm); o["end_time"] = hm;
    o["duration"] = p.duration_min;
    o["action"] = cc::name(p.action);
    o["setpoint"] = p.setpoint;
    o["profile"] = cc::name(p.profile);
  }
  replyJson(200, doc);
}

// ================================================================ F4: erişim, oturum, servis, programlar, trend, OTA
enum class Lvl : uint8_t { PUBLIC, VIEW, ADMIN };

uint32_t clientIp() { return (uint32_t)g_srv.client().remoteIP(); }

// Çerezden oturum belirteci ("sid=<32 hex>")
bool cookieToken(char out[33]) {
  const String c = g_srv.header("Cookie");
  const int i = c.indexOf("sid=");
  if (i < 0) return false;
  const String t = c.substring(i + 4, i + 4 + 32);
  if (t.length() != 32) return false;
  strcpy(out, t.c_str());
  return true;
}

// Parola tanımsızsa cihaz açıktır (kalıcı uyarı). Tanımlıysa: oturum; misafir okuma açıksa VIEW uçları serbest.
bool authorize(Lvl l) {
  if (l == Lvl::PUBLIC || !auth::passwordSet()) return true;
  char t[33];
  if (cookieToken(t) && auth::check(t, nullptr)) return true;
  if (l == Lvl::VIEW && auth::settings().guest_read) return true;
  JsonDocument d;
  d["message"] = "Oturum gerekli. Oturum sayfasından giriş yapın.";
  d["login"] = true;
  replyJson(401, d);
  return false;
}

using Fn = void (*)();
std::function<void()> G(Lvl l, Fn fn) {
  return [l, fn]() { if (authorize(l)) fn(); };
}

void note(cc::Severity s, cc::EvSrc src, cc::EvCode c, float v) {
  if (app::coreLock(50)) { app::core().noteEvent(s, src, c, v, cc::CmdSource::LOCAL_WEB); app::coreUnlock(); }
}

// ---------------------------------------------------------------- oturum
void handleLogin() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  if (!auth::passwordSet()) { replyMsg(409, "Web parolası tanımlı değil; giriş gerekmiyor. Ayarlar › Erişim'den parola belirleyin."); return; }
  char tok[33];
  uint32_t ttl = 0, lock = 0;
  const auth::Login r = auth::login(b["user"] | "", b["password"] | "", clientIp(), b["remember"] | false, tok, ttl, lock);
  if (r != auth::Login::OK) {
    note(cc::Severity::WARNING, cc::EvSrc::SYSTEM, cc::EvCode::AUTH_FAIL, (float)(clientIp() >> 24));
    char m[96];
    if (r == auth::Login::LOCKED) snprintf(m, sizeof m, "Çok fazla hatalı deneme. %u sn sonra yeniden deneyin.", (unsigned)lock);
    else snprintf(m, sizeof m, "Kullanıcı adı veya parola yanlış.");
    replyMsg(r == auth::Login::LOCKED ? 429 : 401, m);
    return;
  }
  char ck[120];
  snprintf(ck, sizeof ck, "sid=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=%u", tok, (unsigned)ttl);
  g_srv.sendHeader("Set-Cookie", ck);
  replyMsg(200, "Giriş yapıldı");
}

void handleLogout() {
  if (!writeOk()) return;
  char t[33];
  if (cookieToken(t)) auth::logout(t);
  g_srv.sendHeader("Set-Cookie", "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  replyMsg(200, "Çıkış yapıldı");
}

void handleSession() {
  JsonDocument d;
  const bool pw = auth::passwordSet();
  char t[33];
  uint32_t exp = 0;
  const bool in = pw && cookieToken(t) && auth::check(t, &exp);
  const auth::Settings s = auth::settings();
  d["password_set"] = pw;
  d["guest_read"] = s.guest_read;
  d["auth"] = in || !pw;
  if (in) {
    d["user"] = s.user;
    d["role"] = "admin";
    char e[40];
    if (exp >= 86400) snprintf(e, sizeof e, "%u gün kaldı", (unsigned)(exp / 86400));
    else snprintf(e, sizeof e, "%u sa %u dk kaldı", (unsigned)(exp / 3600), (unsigned)(exp % 3600 / 60));
    d["expires"] = e;
  } else d["user"] = nullptr;
  replyJson(200, d);
}

void handlePassword() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  const char* pw = b["password"] | "";
  const char* err = nullptr;
  uint32_t lock = 0;
  if (!auth::setPassword(b["oldPassword"] | "", pw, clientIp(), &err, &lock)) { replyMsg(lock ? 429 : 400, err); return; }
  note(cc::Severity::WARNING, cc::EvSrc::SYSTEM, cc::EvCode::PASSWORD_CHANGED, pw[0] ? 1.f : 0.f);
  g_srv.sendHeader("Set-Cookie", "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  replyMsg(200, pw[0] ? "Parola kaydedildi; bütün oturumlar kapatıldı. Yeniden giriş yapın." : "Web parola koruması kaldırıldı.");
}

// ---------------------------------------------------------------- servis
void handleServicePin() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  const char* err = nullptr;
  if (!auth::setPin(b["pin"] | "", &err)) { replyMsg(400, err); return; }
  replyMsg(200, "Servis PIN'i kaydedildi");
}

void handleServiceEnter() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  if (!auth::pinSet()) { replyMsg(409, "Servis PIN'i tanımlı değil. Ayarlar › Erişim'den PIN belirleyin."); return; }
  uint32_t lock = 0;
  if (!auth::verifyPin(b["pin"] | "", clientIp(), &lock)) {
    char m[80];
    if (lock) snprintf(m, sizeof m, "Çok fazla hatalı deneme. %u sn sonra yeniden deneyin.", (unsigned)lock);
    else snprintf(m, sizeof m, "Servis PIN'i yanlış.");
    replyMsg(lock ? 429 : 403, m);
    return;
  }
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().serviceEnter(cc::CmdSource::LOCAL_SERVICE);
  app::coreUnlock();
  if (r.result != cc::CmdResult::ACCEPTED) { replyMsg(409, "Servis moduna girilemedi: ısıtma veya soğutma sürüyor. Modu KAPALI yapıp soğutmanın bitmesini bekleyin."); return; }
  replyMsg(200, "Servis modu etkin");
}

void handleServiceExit() {
  if (!writeOk()) return;
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().serviceExit(cc::CmdSource::LOCAL_SERVICE);
  app::coreUnlock();
  replyMsg(r.result == cc::CmdResult::ACCEPTED ? 200 : 409, r.result == cc::CmdResult::ACCEPTED ? "Servis modundan çıkıldı" : "Servis modunda değil");
}

void handleServiceTest() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  const int out = b["out"] | -1;
  if (out < 0 || out > 3) { replyMsg(400, "Geçersiz çıkış."); return; }
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().serviceTest((uint8_t)out, b["on"] | false, cc::CmdSource::LOCAL_SERVICE);
  app::coreUnlock();
  if (r.result != cc::CmdResult::ACCEPTED && r.result != cc::CmdResult::OVERRIDDEN) {
    char m[96];
    snprintf(m, sizeof m, "Çıkış testi reddedildi (%s%s%s).", cc::name(r.result), r.reason != cc::Reason::NONE ? ", " : "",
             r.reason != cc::Reason::NONE ? cc::name(r.reason) : "");
    replyMsg(409, m);
    return;
  }
  replyMsg(200, "Test komutu uygulandı");
}

void handleResetCounters() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  const char* o = b["out"] | "";
  static const char* const names[4] = {"r1", "r2", "heater_fan", "ventilation_fan"};
  uint8_t mask = !strcmp(o, "all") ? 0x0F : 0;
  for (uint8_t i = 0; i < 4; ++i) if (!strcmp(o, names[i])) mask = (uint8_t)(1u << i);
  if (!mask) { replyMsg(400, "Geçersiz çıkış seçimi."); return; }
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().resetCounters(mask, cc::CmdSource::LOCAL_WEB);
  app::coreUnlock();
  if (r.result != cc::CmdResult::ACCEPTED) { replyMsg(409, cc::name(r.result)); return; }
  storage::flushNow(1500);   // sıfırlama güç kesintisinde geri dönmesin
  replyMsg(200, "Sayaçlar sıfırlandı; önceki değer olay günlüğüne yazıldı");
}

// ---------------------------------------------------------------- programlar (liste atomik, PROGRAMS §5.1)
template <typename E, uint8_t N>
bool parseEnum(const char* s, E& out) {
  for (uint8_t i = 0; i < N; ++i) if (s && !strcmp(s, cc::name((E)i))) { out = (E)i; return true; }
  return false;
}
int16_t parseHm(const char* s) {
  int h = 0, m = 0;
  if (!s || sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return (int16_t)(h * 60 + m);
}

void handleProgramsPost() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  JsonArrayConst a = b["list"].as<JsonArrayConst>();
  if (a.isNull() || a.size() > cc::kMaxPrograms) { JsonDocument d; d["code"] = "TOO_MANY"; d["index"] = -1; d["message"] = "Program listesi reddedildi: TOO_MANY"; replyJson(400, d); return; }
  static cc::Program list[cc::kMaxPrograms];
  uint8_t n = 0;
  const char* code = nullptr;
  int idx = -1;
  for (JsonObjectConst o : a) {
    cc::Program p;
    const char* nm = o["name"] | "";
    while (*nm == ' ') ++nm;
    strncpy(p.name, nm, sizeof p.name - 1);
    p.name[sizeof p.name - 1] = 0;
    for (int k = (int)strlen(p.name) - 1; k >= 0 && p.name[k] == ' '; --k) p.name[k] = 0;
    p.enabled = o["enabled"] | true;
    if (strlen(nm) > cc::kProgNameMax) code = "NAME";
    if (!code && !parseEnum<cc::ProgKind, 3>(o["kind"] | "", p.kind)) code = "DAYS";
    p.days = (uint8_t)(o["days"] | 0);
    if (!code && p.kind != cc::ProgKind::WEEKLY) {
      if (!cc::parseDate(o["date_from"] | "", p.date_from)) code = "DATE_ORDER";
      p.date_to = p.date_from;
      if (!code && p.kind == cc::ProgKind::DATE_RANGE && !cc::parseDate(o["date_to"] | "", p.date_to)) code = "DATE_ORDER";
    }
    if (!code && !parseEnum<cc::ProgEnd, 3>(o["end"] | "", p.end)) code = "END";
    if (!code && p.end != cc::ProgEnd::ALL_DAY) { const int16_t s = parseHm(o["start"] | ""); if (s < 0) code = "START"; else p.start_min = (uint16_t)s; }
    if (!code && p.end == cc::ProgEnd::END_TIME) { const int16_t e = parseHm(o["end_time"] | ""); if (e < 0) code = "END"; else p.end_min = (uint16_t)e; }
    p.duration_min = (uint16_t)(o["duration"] | 60);
    if (!code && !parseEnum<cc::ProgAction, 4>(o["action"] | "", p.action)) code = "PROFILE";
    p.setpoint = o["setpoint"] | 21.0f;
    if (!code && p.action == cc::ProgAction::PROFILE && !parseEnum<cc::ProfileSel, 4>(o["profile"] | "", p.profile)) code = "PROFILE";
    if (code) { idx = n; break; }
    list[n++] = p;
  }
  if (!code) {
    float lim = 40;
    if (app::coreLock(100)) { lim = app::core().config().cabin_overtemp_limit; app::coreUnlock(); }
    const cc::ProgValidation v = cc::validatePrograms(list, n, lim);
    if (!v.ok()) { code = cc::name(v.err); idx = v.index; }
  }
  if (code) {
    JsonDocument d;
    d["code"] = code;
    d["index"] = idx;
    char m[64];
    snprintf(m, sizeof m, "Program listesi reddedildi: %s", code);
    d["message"] = m;
    replyJson(400, d);
    return;
  }
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  r = app::core().setPrograms(list, n, cc::CmdSource::LOCAL_WEB);
  app::coreUnlock();
  if (r.result != cc::CmdResult::ACCEPTED) { replyMsg(409, cc::name(r.result)); return; }
  replyMsg(200, "Kaydedildi");
}

// ---------------------------------------------------------------- trend (akış: büyük dizi RAM'de JSON belgesi olmaz)
void handleTrend() {
  uint32_t win = (uint32_t)g_srv.arg("win").toInt();
  if (win < 60) win = 900;
  if (win > 86400) win = 86400;
  const size_t cap = trend::maxSamples();
  trend::Sample* s = static_cast<trend::Sample*>(malloc(sizeof(trend::Sample) * cap));
  if (!s) { replyMsg(503, "Bellek yetersiz."); return; }
  uint32_t res = 5, age = 0;
  const size_t n = trend::read(win, s, cap, res, age);
  const bool clock = net::clockValid();
  const int64_t tlast = clock ? (int64_t)time(nullptr) - age : (int64_t)(millis() / 1000) - age;
  headers(true);
  g_srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_srv.send(200, "application/json", "");
  char buf[1100];
  size_t bl = 0;
  auto out = [&](const char* x) {
    const size_t l = strlen(x);
    if (bl + l >= sizeof buf) { g_srv.sendContent(buf, bl); bl = 0; }
    memcpy(buf + bl, x, l);
    bl += l;
  };
  char v[24];
  snprintf(v, sizeof v, "{\"res\":%u,\"clock\":%s", (unsigned)res, clock ? "true" : "false");
  out(v);
  const char* keys[6] = {"t", "T", "SP", "RH", "D", "B"};
  for (int k = 0; k < 6; ++k) {
    snprintf(v, sizeof v, ",\"%s\":[", keys[k]);
    out(v);
    for (size_t i = 0; i < n; ++i) {
      const trend::Sample& x = s[i];
      switch (k) {
        case 0: snprintf(v, sizeof v, "%lld", (long long)(tlast - (int64_t)(n - 1 - i) * res)); break;
        case 1: if (x.t10 == trend::kNoT) strcpy(v, "null"); else snprintf(v, sizeof v, "%.1f", x.t10 / 10.0); break;
        case 2: if (x.sp10 == trend::kNoT) strcpy(v, "null"); else snprintf(v, sizeof v, "%.1f", x.sp10 / 10.0); break;
        case 3: if (x.rh10 == trend::kNoRh) strcpy(v, "null"); else snprintf(v, sizeof v, "%.1f", x.rh10 / 10.0); break;
        case 4: snprintf(v, sizeof v, "%.1f", x.d2 / 2.0); break;
        default: snprintf(v, sizeof v, "%u", (unsigned)x.b); break;
      }
      if (i) out(",");
      out(v);
    }
    out("]");
  }
  out(clock ? ",\"boot_note\":\"\"}" : ",\"boot_note\":\"saat bekleniyor: zaman ekseni çalışma süresidir\"}");
  if (bl) g_srv.sendContent(buf, bl);
  g_srv.sendContent("");
  free(s);
}

// ---------------------------------------------------------------- web OTA (hazırlık + ham gövde)
bool otaPasswordOk(const char* pw) {
  if (!net::otaPasswordSet()) return true;             // parolasız OTA (uyarılı, D-17)
  return net::otaPasswordCheck(pw ? pw : "");
}

// 1. adım: parola + boyut denetimi, güvenli duruş (OTA_PREP). ready=false → UI 2 s aralıkla yineler
void handleOtaBegin() {
  if (!writeOk()) return;
  JsonDocument b;
  if (!body(b)) return;
  if (!otaPasswordOk(b["password"] | "")) { replyMsg(403, "OTA parolası yanlış."); return; }
  const uint32_t size = b["size"] | 0;
  const esp_partition_t* part = esp_ota_get_next_update_partition(nullptr);
  if (!part || size < 100000 || size > part->size) { replyMsg(400, "İmaj boyutu geçersiz veya OTA bölümüne sığmıyor."); return; }
  bool ready = false;
  cc::CmdReply r;
  if (!app::coreLock(200)) { replyMsg(503, "Çekirdek meşgul."); return; }
  if (!app::core().otaReady()) r = app::core().otaBegin(cc::CmdSource::LOCAL_WEB);   // hazırlıkta yinelenirse zararsız ret
  ready = app::core().otaReady();
  app::coreUnlock();
  if (!ready && r.reason == cc::Reason::ANTIFREEZE_INHIBIT) { replyMsg(409, "Donma riski: kulübe sıcaklığı donma korumasına yakın, güncelleme şimdi yapılamaz."); return; }
  JsonDocument d;
  d["ready"] = ready;
  d["message"] = ready ? "Cihaz güncellemeye hazır; imaj yükleniyor." : "Hazırlanıyor: ısıtma durduruldu, fan soğutması bitince yükleme başlayacak.";
  replyJson(200, d);
}

struct OtaUp { bool active = false, ok = false; const char* err = nullptr; size_t written = 0; };
OtaUp g_up;

void otaRaw() {
  HTTPRaw& r = g_srv.raw();
  if (r.status == RAW_START) {
    g_up = OtaUp();
    if (!authorize(Lvl::ADMIN)) { g_up.err = "Oturum gerekli."; return; }
    if (g_srv.header("X-SCADA") != "1") { g_up.err = "İstek reddedildi (X-SCADA başlığı yok)."; return; }
    if (!otaPasswordOk(g_srv.header("X-OTA-Password").c_str())) { g_up.err = "OTA parolası yanlış."; return; }
    bool ready = false;
    if (app::coreLock(200)) { ready = app::core().otaReady(); app::coreUnlock(); }
    if (!ready) { g_up.err = "Cihaz güncellemeye hazır değil (soğutma sürüyor). Önce hazırlık adımı."; return; }
    const int len = g_srv.clientContentLength();
    storage::flushNow(1500);
    if (len <= 0 || !Update.begin((size_t)len, U_FLASH)) { g_up.err = "Güncelleme başlatılamadı (boyut/bölüm)."; return; }
    note(cc::Severity::WARNING, cc::EvSrc::SYSTEM, cc::EvCode::OTA_WEB, (float)(len / 1024));
    g_up.active = true;
  } else if (r.status == RAW_WRITE && g_up.active) {
    if (Update.write(r.buf, r.currentSize) != r.currentSize) { g_up.err = "Flash yazımı başarısız."; Update.abort(); g_up.active = false; }
    else g_up.written += r.currentSize;
  } else if (r.status == RAW_END && g_up.active) {
    g_up.active = false;
    if (Update.end(true)) g_up.ok = true;
    else g_up.err = "İmaj doğrulanamadı (bozuk veya uyumsuz).";
  } else if (r.status == RAW_ABORTED && g_up.active) {
    Update.abort();
    g_up.active = false;
    g_up.err = "Yükleme kesildi.";
  }
}

void otaDone() {
  if (g_up.ok) {
    replyMsg(200, "Güncelleme tamamlandı; cihaz yeniden başlıyor. Sayfa 20–40 sn sonra yeniden bağlanır.");
    net::requestReboot(1500);
    return;
  }
  if (app::coreLock(200)) { app::core().otaAbort(); app::coreUnlock(); }   // hazırlık geri alınır: kontrol sürer
  replyMsg(g_up.err && !strncmp(g_up.err, "Oturum", 6) ? 401 : 409, g_up.err ? g_up.err : "Güncelleme başarısız.");
}

// ---------------------------------------------------------------- fabrika ayarı
void handleFactoryReset() {
  if (!writeOk()) return;
  bool busy = true;
  if (app::coreLock(200)) {
    const bool* o = app::core().outputs();
    busy = o[cc::R1] || o[cc::R2] || app::core().snapshot().post_cool_remaining_s > 0;
    app::coreUnlock();
  }
  if (busy) { replyMsg(409, "Isıtma veya fan soğutması sürüyor. Modu KAPALI yapın ve soğutmanın bitmesini bekleyin."); return; }
  note(cc::Severity::WARNING, cc::EvSrc::SYSTEM, cc::EvCode::FACTORY_RESET, 0);
  const bool files = storage::factoryErase(3000);
  static const char* const ns[] = {"net", "mqtt", "led", "auth"};
  for (const char* n : ns) { Preferences p; if (p.begin(n, false)) { p.clear(); p.end(); } }
  auth::factoryErase();
  replyMsg(200, files ? "Fabrika ayarlarına dönüldü; cihaz kurulum modunda yeniden başlıyor."
                      : "Ayarlar silindi ancak bazı kayıt dosyaları silinemedi; cihaz yeniden başlıyor.");
  net::requestReboot(1500);
}


void sendIndex() { sendAsset(ui::kAssets[0]); }

// Kurulum AP'sinde her bilinmeyen istek (telefon captive-portal denetimleri dahil) kurulum sayfasına yönlenir
void handleNotFound() {
  if (net::status().ap_mode && !g_srv.uri().startsWith("/api/")) {
    headers(true);
    g_srv.sendHeader("Location", String("http://") + net::kApIp + "/", true);
    g_srv.send(302, "text/plain", "");
    return;
  }
  if (g_srv.uri().startsWith("/api/")) { replyMsg(404, "Bilinmeyen uç."); return; }
  headers(true);
  g_srv.send(404, "text/plain", "404");
}

}  // namespace

const char* uiBuild() { return ui::kUiBuild; }

void begin() {
  for (size_t i = 0; i < ui::kAssetCount; ++i) {
    const ui::Asset* a = &ui::kAssets[i];
    g_srv.on(a->path, HTTP_GET, [a]() { sendAsset(*a); });
  }
  for (const char* p : {"/control", "/programs", "/trends", "/outputs", "/alarms", "/events", "/settings", "/login"})
    g_srv.on(p, HTTP_GET, sendIndex);
  // Erişim: PUBLIC (oturum/giriş), VIEW (misafir okuma açıksa serbest), ADMIN (oturum). Parola tanımsızsa hepsi açık.
  g_srv.on("/api/data", HTTP_GET, G(Lvl::VIEW, handleData));
  g_srv.on("/api/cmd", HTTP_POST, G(Lvl::ADMIN, handleCmd));
  g_srv.on("/scan", HTTP_GET, G(Lvl::ADMIN, handleScan));
  g_srv.on("/api/settings", HTTP_GET, G(Lvl::ADMIN, settingsGet));
  g_srv.on("/api/settings", HTTP_POST, G(Lvl::ADMIN, settingsPost));
  g_srv.on("/api/reset-wifi", HTTP_POST, G(Lvl::ADMIN, handleResetWifi));
  g_srv.on("/api/net/retry", HTTP_POST, G(Lvl::ADMIN, handleNetRetry));
  g_srv.on("/api/net/finish", HTTP_POST, G(Lvl::ADMIN, handleNetFinish));
  g_srv.on("/api/reboot", HTTP_POST, G(Lvl::ADMIN, handleReboot));
  g_srv.on("/api/ota/password", HTTP_POST, G(Lvl::ADMIN, handleOtaPassword));
  g_srv.on("/api/events", HTTP_GET, G(Lvl::VIEW, handleEvents));
  g_srv.on("/api/alarms", HTTP_GET, G(Lvl::VIEW, handleAlarms));
  g_srv.on("/api/alarms/ack", HTTP_POST, G(Lvl::ADMIN, handleAlarmAck));
  g_srv.on("/api/alarms/reset", HTTP_POST, G(Lvl::ADMIN, handleAlarmReset));
  g_srv.on("/api/programs", HTTP_GET, G(Lvl::VIEW, handlePrograms));
  g_srv.on("/api/programs", HTTP_POST, G(Lvl::ADMIN, handleProgramsPost));
  g_srv.on("/api/trend", HTTP_GET, G(Lvl::VIEW, handleTrend));
  g_srv.on("/api/session", HTTP_GET, handleSession);
  g_srv.on("/api/login", HTTP_POST, handleLogin);
  g_srv.on("/api/logout", HTTP_POST, handleLogout);
  g_srv.on("/api/password", HTTP_POST, G(Lvl::ADMIN, handlePassword));
  g_srv.on("/api/service/pin", HTTP_POST, G(Lvl::ADMIN, handleServicePin));
  g_srv.on("/api/service/enter", HTTP_POST, G(Lvl::ADMIN, handleServiceEnter));
  g_srv.on("/api/service/exit", HTTP_POST, G(Lvl::ADMIN, handleServiceExit));
  g_srv.on("/api/service/test", HTTP_POST, G(Lvl::ADMIN, handleServiceTest));
  g_srv.on("/api/service/reset-counters", HTTP_POST, G(Lvl::ADMIN, handleResetCounters));
  g_srv.on("/api/ota/begin", HTTP_POST, G(Lvl::ADMIN, handleOtaBegin));
  g_srv.on("/api/ota", HTTP_POST, otaDone, otaRaw);   // ham gövde parça parça flash'a (yetki RAW_START'ta)
  g_srv.on("/api/factory-reset", HTTP_POST, G(Lvl::ADMIN, handleFactoryReset));
  g_srv.onNotFound(handleNotFound);
  static const char* hdrs[] = {"X-SCADA", "Cookie", "X-OTA-Password"};
  g_srv.collectHeaders(hdrs, 3);
}

void start() { g_srv.begin(); }
void handle() { g_srv.handleClient(); }

}  // namespace web
