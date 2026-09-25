#include "web.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cmath>
#include <cstring>
#include "boot_state.h"
#include "core_api.h"
#include "cc_netfsm.h"
#include "net_manager.h"
#include "status_led.h"
#include "tasks.h"
#include "ui_generated.h"

namespace web {

namespace {

WebServer g_srv(80);
cc::Event g_ev[200];   // olay kopyası (yalnız NetTask)

const char* kFwVersion = "0.2.4";

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

void isoLocal(int64_t local_min, char out[24]) {
  const int32_t day = (int32_t)(local_min >= 0 ? local_min / 1440 : (local_min - 1439) / 1440);
  char d[11];
  cc::formatDate(day, d);
  const int32_t m = (int32_t)(local_min - (int64_t)day * 1440);
  snprintf(out, 24, "%sT%02d:%02d", d, (int)((m / 60) % 24), (int)(m % 60));
}

// ---------------------------------------------------------------- /api/data
void handleData() {
  cc::CoreSnapshot s;
  cc::Config c;
  uint32_t sw[4] = {0, 0, 0, 0};
  uint64_t on_ms[4] = {0, 0, 0, 0};
  uint8_t vsrc = 0;
  bool svc[4] = {false, false, false, false};
  float err_rate = 0;
  char prog[24] = "—";
  if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul; yeniden deneyin."); return; }
  s = app::core().snapshot();
  c = app::core().config();
  for (uint8_t k = 0; k < 4; ++k) {
    sw[k] = app::core().guard().switchCount(k);
    on_ms[k] = app::core().guard().onTimeMs(k);
    svc[k] = app::core().serviceTestOn(k);
  }
  vsrc = app::core().vent().sources;
  err_rate = app::core().t1().error_rate_10m;
  if (s.program_index >= 0 && s.program_index < app::core().programCount())
    strncpy(prog, app::core().programs()[s.program_index].name, sizeof prog - 1);
  app::coreUnlock();
  const net::Status ns = net::status();
  const net::NetSettings nc = net::settings();
  const app::TaskStats ts = app::stats();

  JsonDocument doc;
  JsonObject d = doc.to<JsonObject>();
  d["v"] = 1;
  d["seq"] = s.seq;
  if (ns.clock_valid) d["ts"] = (int64_t)time(nullptr); else d["ts"] = nullptr;
  d["uptime"] = s.uptime_s;
  d["device_name"] = nc.adn;
  d["ip"] = ns.ip;
  d["mdns"] = nc.mdns;
  d["client_ip"] = g_srv.client().remoteIP().toString();
  d["fw_version"] = kFwVersion;
  d["fw_build"] = ui::kUiBuild;
  d["ap_mode"] = ns.ap_mode;
  d["ap_name"] = ns.ap_name;
  d["ap_ip"] = net::kApIp;
  d["wifi_ssid"] = ns.ssid;
  d["net_note"] = ns.note;
  d["wifi_ok"] = ns.sta_ok;
  if (ns.sta_ok) d["wifi_rssi"] = ns.rssi; else d["wifi_rssi"] = nullptr;
  d["mqtt_status"] = "DISABLED";   // F5
  {
    uint8_t ls[cc::kLedCount];
    leds::states(ls);
    JsonArray la = d["led_states"].to<JsonArray>();
    for (uint8_t v : ls) la.add(v);
    d["led_ok"] = leds::driverOk();
  }
  d["time_valid"] = onoff(ns.clock_valid);
  d["password_set"] = false;       // web parolası F4
  fnum(d, "temperature", s.temperature, 1);
  fnum(d, "humidity", s.humidity, 1);
  d["temperature_quality"] = cc::name(s.temperature_quality);
  d["humidity_quality"] = cc::name(s.humidity_quality);
  d["t2"] = nullptr;
  d["t2_quality"] = cc::name(s.t2_quality);
  d["sensor_ok"] = onoff(s.sensor_ok);
  d["sensor_age_s"] = s.sensor_age_s;
  fnum(d, "temperature_setpoint", s.temperature_setpoint, 1);
  fnum(d, "setpoint_effective", s.setpoint_effective);
  d["setpoint_source"] = cc::name(s.setpoint_source);
  fnum(d, "setpoint_night", c.setpoint_night, 1);
  fnum(d, "setpoint_away", c.setpoint_away, 1);
  fnum(d, "setpoint_frost", c.setpoint_frost, 1);
  fnum(d, "setpoint_boost", c.setpoint_boost, 1);
  d["boost_minutes"] = c.boost_minutes;
  fnum(d, "frost_guard_temperature", c.frost_guard_temperature, 1);
  d["profile"] = cc::name(s.profile);
  d["profile_active"] = cc::name(s.profile_active);
  d["sched_night"] = onoff(s.sched_night);
  d["sched_away"] = onoff(s.sched_away);
  d["boost"] = onoff(s.boost);
  d["boost_remaining_min"] = s.boost_remaining_min;
  d["operating_mode"] = cc::name(s.operating_mode);
  d["controller_enable"] = onoff(s.controller_enable);
  d["controller_state"] = cc::name(s.controller_state);
  d["heating_phase"] = cc::name(s.heating_phase);
  d["ventilation_state"] = cc::name(s.ventilation_state);
  d["heating_reason"] = cc::name(s.heating_reason);
  d["failsafe_reason"] = cc::name(s.failsafe_reason);
  fnum(d, "pid_output", s.pid_output, 1);
  fnum(d, "heat_demand", s.heat_demand, 1);
  fnum(d, "manual_heat_demand", s.manual_heat_demand, 0);
  fnum(d, "pid_error", s.pid_error);
  fnum(d, "pid_p", s.pid_p, 1);
  fnum(d, "pid_i", s.pid_i, 1);
  fnum(d, "pid_d", s.pid_d, 1);
  d["pid_saturation"] = cc::name(s.pid_saturation);
  d["anti_windup_active"] = onoff(s.anti_windup_active);
  d["pid_tracking"] = onoff(s.pid_tracking);
  fnum(d, "r1_duty", s.r1_duty, 1);
  fnum(d, "r2_duty", s.r2_duty, 1);
  d["power_stage"] = s.power_stage;
  fnum(d, "stage2_on", c.stage2_on, 0);
  fnum(d, "stage2_off", c.stage2_off, 0);
  fnum(d, "temperature_rate", s.temperature_rate, 1);
  static const char* const outs[4] = {"r1", "r2", "heater_fan", "ventilation_fan"};
  char k[40];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(k, sizeof k, "%s_active", outs[i]); d[k] = onoff(s.active[i]);
    snprintf(k, sizeof k, "%s_reason", outs[i]); d[k] = cc::name(s.reason[i]);
    snprintf(k, sizeof k, "%s_switch_count", outs[i]); d[k] = sw[i];
    snprintf(k, sizeof k, "%s_hours_total", outs[i]); d[k] = (float)(on_ms[i] / 36000ULL) / 100.0f;
    snprintf(k, sizeof k, "svc_test_%s", outs[i]); d[k] = onoff(svc[i]);
  }
  d["heating_active"] = onoff(s.heating_active);
  d["ventilation_active"] = onoff(s.ventilation_active);
  d["heater_fan_manual"] = onoff(s.heater_fan_manual);
  d["ventilation_fan_manual"] = onoff(s.ventilation_fan_manual);
  d["post_cool_remaining_s"] = s.post_cool_remaining_s;
  JsonArray vs = d["vent_sources"].to<JsonArray>();
  static const char* const vn[5] = {"TEMP_HIGH", "HUMIDITY_HIGH", "MANUAL", "SCHEDULED", "OVERTEMP"};
  for (uint8_t i = 0; i < 5; ++i) if (vsrc & (1u << i)) vs.add(vn[i]);
  fnum(d, "ventilation_start_effective", s.ventilation_start_effective, 1);
  fnum(d, "ventilation_start_temperature", c.ventilation_start_temperature, 1);
  fnum(d, "ventilation_stop_temperature", c.ventilation_stop_temperature, 1);
  fnum(d, "humidity_high_limit", c.humidity_high_limit, 0);
  fnum(d, "humidity_hysteresis", c.humidity_hysteresis, 0);
  d["humidity_vent_while_heating"] = c.humidity_vent_while_heating == cc::HumVentWhileHeating::INHIBIT ? "INHIBIT" : "ALLOW";
  d["manual_vent_priority"] = c.manual_vent_priority == cc::ManualVentPriority::VENT_WINS ? "VENT_WINS" : "HEAT_WINS";
  d["overtemperature"] = onoff(s.overtemperature);
  d["alarm"] = onoff(s.alarm);
  d["alarm_state"] = cc::name(s.alarm_state);
  d["active_alarm_count"] = s.active_alarm_count;
  d["unacked_alarm_count"] = s.unacked_alarm_count;
  d["local_lock"] = onoff(s.local_lock);
  d["last_command_source"] = cc::name(s.last_command_source);
  d["ack_count"] = s.ack_count;
  d["programs_enabled"] = onoff(s.programs_enabled);
  d["program_active"] = prog;
  if (s.program_index >= 0 && s.program_until >= 0) { char iso[24]; isoLocal(s.program_until, iso); d["program_until"] = iso; }
  else d["program_until"] = "";
  d["program_held"] = onoff(s.program_held);
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
  fnum(d, "sensor_error_rate_10m", err_rate, 0);
  d["sensor_model"] = "DHT22";
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
    // MQTT SLUG varsayılanı (MQTT_INTEGRATION §2): kulube_iklim_ + MAC son 3 bayt; taşınma F5'te
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char slug[24];
    snprintf(slug, sizeof slug, "kulube_iklim_%02x%02x%02x", mac[3], mac[4], mac[5]);
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
  d["ssid"] = ns.ssid;
  d["passSet"] = net::passSet();
  d["otaPasswordSet"] = net::otaPasswordSet();
  d["apName"] = ns.ap_name;
  d["devName"] = n.adn;
  d["ip"] = ns.ip;
  d["fwVersion"] = kFwVersion;
  d["fwBuild"] = ui::kUiBuild;
  replyJson(200, doc);
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
  bool anyNet = false, anyLed = false;
  cc::LedConfig lc = leds::config();
  for (JsonPair kv : b.as<JsonObject>()) {
    bool known = false;
    for (const char* k : netKeys) if (!strcmp(kv.key().c_str(), k)) { known = true; break; }
    if (known) { anyNet = true; continue; }
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
      // Bu yazılımda henüz karşılığı olmayan UI alanları (MQTT F5, erişim F4 …): GET bunları göndermez,
      // form boş/varsayılan gönderir. Dolu gelen değer sessizce yok sayılmaz, reddedilir.
      const JsonVariantConst v = kv.value();
      const bool empty = v.isNull() || (v.is<const char*>() && !*v.as<const char*>()) || (v.is<bool>() && !v.as<bool>()) ||
                         (v.is<float>() && v.as<float>() == 0.0f);
      if (empty) continue;
      replyMsg(409, "Bu ayar sonraki fazda (MQTT F5, erişim F4) etkinleşecek; şimdilik kaydedilemez.", kv.key().c_str());
      return;
    }
    // Değişmeyen değerler (form tüm alanları gönderir) kabul; değişen değer F3'e kadar kaydedilemez
    cc::Config c;
    if (!app::coreLock(100)) { replyMsg(503, "Çekirdek meşgul."); return; }
    c = app::core().config();
    app::coreUnlock();
    const float cur = cc::fieldValue(c, *f);
    bool same = false;
    if (f->kind == cc::FieldKind::ENUM) same = kv.value().is<const char*>() && (uint8_t)cur < f->enumCount && !strcmp(kv.value().as<const char*>(), f->enumNames[(uint8_t)cur]);
    else if (f->kind == cc::FieldKind::BOOL) same = kv.value().as<bool>() == (cur != 0);
    else same = fabsf(kv.value().as<float>() - cur) < 1e-4f;
    if (!same) { replyMsg(409, "Bu ayar kalıcı ayar deposu (F3) eklenince kaydedilebilecek. Şimdilik Ağ, LED ve Wi-Fi ayarları kaydedilir.", kv.key().c_str()); return; }
  }
  const net::Status ns = net::status();   // net_try tabanı: UI bu değerden büyük denemenin sonucunu bekler
  if (!anyNet) {
    const char* err = nullptr;
    if (anyLed && !leds::apply(lc, &err)) { replyMsg(507, err); return; }
    JsonDocument d;
    d["message"] = anyLed ? "LED ayarları kaydedildi" : "Kaydedildi";
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

void notYet() { replyMsg(501, "Bu işlev F4'te etkinleşecek (oturum, parola, trend, servis, web OTA)."); }

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

void begin() {
  for (size_t i = 0; i < ui::kAssetCount; ++i) {
    const ui::Asset* a = &ui::kAssets[i];
    g_srv.on(a->path, HTTP_GET, [a]() { sendAsset(*a); });
  }
  for (const char* p : {"/control", "/programs", "/trends", "/outputs", "/alarms", "/events", "/settings", "/login"})
    g_srv.on(p, HTTP_GET, sendIndex);
  g_srv.on("/api/data", HTTP_GET, handleData);
  g_srv.on("/api/cmd", HTTP_POST, handleCmd);
  g_srv.on("/scan", HTTP_GET, handleScan);
  g_srv.on("/api/settings", HTTP_GET, settingsGet);
  g_srv.on("/api/settings", HTTP_POST, settingsPost);
  g_srv.on("/api/reset-wifi", HTTP_POST, handleResetWifi);
  g_srv.on("/api/net/retry", HTTP_POST, handleNetRetry);
  g_srv.on("/api/net/finish", HTTP_POST, handleNetFinish);
  g_srv.on("/api/reboot", HTTP_POST, handleReboot);
  g_srv.on("/api/events", HTTP_GET, handleEvents);
  g_srv.on("/api/alarms", HTTP_GET, handleAlarms);
  g_srv.on("/api/alarms/ack", HTTP_POST, handleAlarmAck);
  g_srv.on("/api/alarms/reset", HTTP_POST, handleAlarmReset);
  g_srv.on("/api/programs", HTTP_GET, handlePrograms);
  g_srv.on("/api/session", HTTP_GET, []() { JsonDocument d; d["user"] = nullptr; d["auth"] = false; replyJson(200, d); });
  for (const char* p : {"/api/programs", "/api/login", "/api/logout", "/api/password", "/api/service/enter",
                        "/api/service/exit", "/api/service/test", "/api/service/pin", "/api/service/reset-counters",
                        "/api/ota/begin", "/api/factory-reset"})
    g_srv.on(p, HTTP_POST, notYet);
  g_srv.on("/api/trend", HTTP_GET, notYet);
  g_srv.onNotFound(handleNotFound);
  static const char* hdrs[] = {"X-SCADA"};
  g_srv.collectHeaders(hdrs, 1);
}

void start() { g_srv.begin(); }
void handle() { g_srv.handleClient(); }

}  // namespace web
