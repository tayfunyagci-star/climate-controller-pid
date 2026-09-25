#include "mqtt_client.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <ctime>
#include "mqtt_cfg.h"
#include "net_manager.h"
#include "state_json.h"
#include "tasks.h"
#include "version.h"

namespace mq {

namespace {

// ---------------------------------------------------------------- paylaşılan durum
SemaphoreHandle_t g_mtx = nullptr;   // g_status
Status g_status;

struct Lock {
  Lock() { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { if (g_mtx) xSemaphoreGive(g_mtx); }
};

void setState(State s, const char* note = nullptr) {
  Lock l;
  g_status.state = s;
  if (note) { strncpy(g_status.note, note, sizeof g_status.note - 1); g_status.note[sizeof g_status.note - 1] = 0; }
}
template <typename F>
void bump(F fn) { Lock l; fn(g_status); }

// ---------------------------------------------------------------- esp-mqtt olayları (esp-mqtt görevi)
struct Rx {
  char topic[128];
  char data[96];
  bool retain;
};
QueueHandle_t g_rx = nullptr;
std::atomic<bool> g_ev_conn{false}, g_ev_disc{false}, g_ev_auth{false};

esp_err_t onEvent(esp_mqtt_event_handle_t e) {
  switch (e->event_id) {
    case MQTT_EVENT_CONNECTED: g_ev_conn.store(true); break;
    case MQTT_EVENT_DISCONNECTED: g_ev_disc.store(true); break;
    case MQTT_EVENT_ERROR:
      if (e->error_handle && e->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED &&
          (e->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME ||
           e->error_handle->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED))
        g_ev_auth.store(true);
      break;
    case MQTT_EVENT_DATA: {
      // Yalnız tek parçalı, küçük yükler (komutlar ≤ 64 B); büyük/parçalı mesaj komut olamaz
      if (e->current_data_offset != 0 || e->data_len != e->total_data_len) break;
      if (e->topic_len <= 0 || e->topic_len >= (int)sizeof(Rx::topic) || e->data_len >= (int)sizeof(Rx::data)) break;
      Rx r;
      memcpy(r.topic, e->topic, e->topic_len);
      r.topic[e->topic_len] = 0;
      memcpy(r.data, e->data, e->data_len);
      r.data[e->data_len] = 0;
      r.retain = e->retain;
      if (g_rx) xQueueSend(g_rx, &r, 0);   // dolu kuyruk: komut düşer (Suite 8 s'de "doğrulanmadı" görür)
      break;
    }
    default: break;
  }
  return ESP_OK;
}

// ---------------------------------------------------------------- görev durumu (yalnız mqtt görevi)
esp_mqtt_client_handle_t g_cli = nullptr;
mqttcfg::Settings g_cfg;           // istemcinin kurulduğu ayarlar
uint32_t g_gen = 0;
char g_slug[24] = "", g_dev[32] = "", g_base[128] = "", g_pass[129] = "";
bool g_connected = false;
uint32_t g_backoff_s = 1, g_next_try_ms = 0, g_sub_ms = 0, g_attempt_ms = 0;
// Bağlantı sonrası sıra (sözleşme §4, MQTT_INTEGRATION §7): keşif → config → alarm → diag → state → online
int g_disc_i = -1;                 // ≥0: keşif sürüyor (20 ms aralık)
bool g_disc_clear = false;         // keşif kapalı: kayıtları boş retained ile sil
bool g_online_pending = false;
uint32_t g_last_disc_ms = 0, g_disc_tick_ms = 0;
uint32_t g_state_hash = 0, g_state_ms = 0, g_cfg_hash = 0, g_alarm_hash = 0, g_diag_ms = 0, g_avail_ms = 0;
bool g_force_state = false;
uint32_t g_ev_seq = 0;
bool g_disc_enabled_seen = true;

void topic(char* out, size_t cap, const char* suffix) { snprintf(out, cap, "%s/%s", g_base, suffix); }

bool pub(const char* t, const char* data, size_t len, int qos, bool retain) {
  if (!g_cli || !g_connected) return false;
  return esp_mqtt_client_publish(g_cli, t, data, (int)len, qos, retain ? 1 : 0) >= 0;
}
bool pubStr(const char* suffix, const char* data, int qos, bool retain) {
  char t[160];
  topic(t, sizeof t, suffix);
  return pub(t, data, strlen(data), qos, retain);
}
bool pubJson(const char* suffix, JsonDocument& d, int qos, bool retain) {
  static char buf[cc::kMqBuffer];
  const size_t n = serializeJson(d, buf, sizeof buf);
  if (n == 0 || n >= sizeof buf - 1) return false;   // tampon 2048 B; taşan yük yayınlanmaz
  char t[160];
  topic(t, sizeof t, suffix);
  return pub(t, buf, n, qos, retain);
}

uint32_t fnv(const char* s, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i) { h ^= (uint8_t)s[i]; h *= 16777619u; }
  return h;
}

cc::MqIdentity identity() { return {g_base, g_slug, g_dev, app::kFwVersion}; }

void teardown(bool announce_offline) {
  if (!g_cli) return;
  if (announce_offline && g_connected) {
    // Temiz kapanışta LWT düşmez: offline elle yazılır (hayalet cihaz önlenir, sözleşme §4)
    pubStr("avail", "offline", 1, true);
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  esp_mqtt_client_stop(g_cli);
  esp_mqtt_client_destroy(g_cli);
  g_cli = nullptr;
  g_connected = false;
  g_disc_i = -1;
  g_online_pending = false;
}

// Adres (kök topic) taşınması: eski adresin retained kayıtları temizlenir, eski avail offline
void migrateOld() {
  if (!g_connected) return;
  static const char* const retained[] = {"state", "diag/state", "config/reported", "alarm/state", "history/heat_minutes_daily"};
  for (const char* r : retained) pubStr(r, "", 1, true);
  const cc::MqIdentity idn = identity();
  char t[160];
  for (size_t i = 0; i < cc::mqEntityCount(); ++i)
    if (cc::mqDiscoveryTopic(idn, cc::mqEntity(i), t, sizeof t)) pub(t, "", 0, 1, true);
}

void create() {
  g_cfg = mqttcfg::settings();
  g_gen = mqttcfg::generation();
  mqttcfg::slug(g_slug);
  mqttcfg::devName(g_dev);
  mqttcfg::password(g_pass, sizeof g_pass);
  snprintf(g_base, sizeof g_base, "%s/%s", g_cfg.base, g_slug);
  static char lwt[160];
  topic(lwt, sizeof lwt, "avail");
  esp_mqtt_client_config_t c = {};
  c.event_handle = onEvent;
  c.host = g_cfg.host;
  c.port = g_cfg.port;
  c.transport = MQTT_TRANSPORT_OVER_TCP;
  c.client_id = g_slug;                         // SABİT (sözleşme §4.1)
  c.username = g_cfg.user[0] ? g_cfg.user : nullptr;
  c.password = g_pass[0] ? g_pass : nullptr;
  c.lwt_topic = lwt;
  c.lwt_msg = "offline";
  c.lwt_qos = 1;
  c.lwt_retain = 1;
  c.keepalive = 30;
  c.disable_auto_reconnect = true;              // üstel geri çekilme bu görevde (§7)
  c.buffer_size = cc::kMqBuffer;
  c.out_buffer_size = cc::kMqBuffer;
  c.network_timeout_ms = 5000;
  c.task_stack = 6144;
  c.task_prio = 3;
  g_cli = esp_mqtt_client_init(&c);
  if (!g_cli) { setState(State::Backoff, "İstemci oluşturulamadı"); return; }
  g_ev_conn.store(false); g_ev_disc.store(false); g_ev_auth.store(false);
  esp_mqtt_client_start(g_cli);
  g_attempt_ms = millis();
  char n[72];
  snprintf(n, sizeof n, "%.40s:%u bağlanılıyor", g_cfg.host, (unsigned)g_cfg.port);
  setState(State::Connecting, n);
}

void onConnected(uint32_t now) {
  g_connected = true;
  g_backoff_s = 1;
  char t[160];
  topic(t, sizeof t, "+/set");
  esp_mqtt_client_subscribe(g_cli, t, 1);
  esp_mqtt_client_subscribe(g_cli, "homeassistant/status", 1);
  g_sub_ms = now;                               // sonraki 2 s: retained komut koruması (§6.5)
  app::Frame f;
  const bool disc = app::capture(f, 100) ? f.c.discovery_enabled : g_disc_enabled_seen;
  g_disc_enabled_seen = disc;
  g_disc_clear = !disc;
  g_disc_i = 0;
  g_disc_tick_ms = now;
  g_last_disc_ms = now;
  g_online_pending = true;
  g_cfg_hash = g_alarm_hash = g_state_hash = 0;   // bağlantıda hepsi tam yayınlanır
  if (!g_ev_seq) { if (app::coreLock(50)) { g_ev_seq = app::core().events().lastSeq(); app::coreUnlock(); } }   // kopukluk olayları replay edilmez
  char n[72];
  snprintf(n, sizeof n, "%.24s:%u · %.36s", g_cfg.host, (unsigned)g_cfg.port, g_base);
  setState(State::Connected, n);
}

void onDisconnected(uint32_t now, bool auth) {
  if (g_connected) bump([](Status& s) { ++s.reconnects; });
  g_connected = false;
  g_disc_i = -1;
  g_online_pending = false;
  const uint32_t jitter = esp_random() % (g_backoff_s * 200 + 1);   // + %20'ye kadar
  g_next_try_ms = now + g_backoff_s * 1000 + jitter;
  char n[72];
  snprintf(n, sizeof n, "%s · %u s sonra yeniden denenecek", auth ? "Kimlik reddedildi" : "Broker'a ulaşılamadı", (unsigned)g_backoff_s);
  setState(auth ? State::AuthFail : State::Backoff, n);
  g_backoff_s = g_backoff_s >= 32 ? 60 : g_backoff_s * 2;          // 1 → 2 → 4 … 60 s
}

// ---------------------------------------------------------------- yayınlar
void publishDiscoveryStep(uint32_t now) {
  if (g_disc_i < 0 || now - g_disc_tick_ms < 20) return;
  g_disc_tick_ms = now;
  const cc::MqIdentity idn = identity();
  static char buf[cc::kMqBuffer];
  char t[160];
  for (int k = 0; k < 2 && g_disc_i < (int)cc::mqEntityCount(); ++k, ++g_disc_i) {
    const cc::MqEntity& e = cc::mqEntity((size_t)g_disc_i);
    if (!cc::mqDiscoveryTopic(idn, e, t, sizeof t)) continue;
    if (g_disc_clear) { pub(t, "", 0, 1, true); continue; }
    const size_t n = cc::mqDiscoveryPayload(idn, e, buf, sizeof buf);
    if (n) pub(t, buf, n, 1, true);
  }
  if (g_disc_i >= (int)cc::mqEntityCount()) g_disc_i = -1;
}

void publishAlarms() {
  cc::AlarmRec recs[cc::kAlarmCount];
  if (!app::coreLock(50)) return;
  for (uint8_t i = 0; i < cc::kAlarmCount; ++i) recs[i] = app::core().alarms().rec((cc::AlarmId)i);
  app::coreUnlock();
  JsonDocument d;
  d["v"] = 1;
  JsonArray a = d["active"].to<JsonArray>();
  uint8_t cnt = 0, unack = 0;
  cc::Severity hi = cc::Severity::NONE;
  uint32_t h = 2166136261u;
  for (uint8_t i = 0; i < cc::kAlarmCount; ++i) {
    const cc::AlarmRec& r = recs[i];
    if (r.st == cc::AlarmState::NORMAL || r.st == cc::AlarmState::PENDING) continue;
    JsonObject o = a.add<JsonObject>();
    o["code"] = cc::name((cc::AlarmId)i);
    o["sev"] = cc::name(r.sev);
    o["state"] = cc::name(r.st);
    o["latched"] = r.st == cc::AlarmState::LATCHED;
    ++cnt;
    if (r.st == cc::AlarmState::ACTIVE_UNACK || r.st == cc::AlarmState::CLEARED_UNACK) ++unack;
    if ((uint8_t)r.sev > (uint8_t)hi) hi = r.sev;
    h = (h ^ (i * 8u + (uint8_t)r.st + ((uint8_t)r.sev << 4))) * 16777619u;
  }
  d["highest"] = hi == cc::Severity::NONE ? "NORMAL" : cc::name(hi);
  d["count"] = cnt;
  d["unacked"] = unack;
  if (h == g_alarm_hash) return;
  if (pubJson("alarm/state", d, 1, true)) g_alarm_hash = h;
}

void publishState(uint32_t now, bool force) {
  app::Frame f;
  if (!app::capture(f, 50)) return;
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  app::writeProcess(o, f);
  // İçerik özeti seq/ts/uptime olmadan: yalnız gerçek değişim erken yayın tetikler
  const uint32_t seq = f.s.seq, up = f.s.uptime_s;
  o.remove("seq"); o.remove("ts"); o.remove("uptime");
  static char buf[cc::kMqBuffer];
  const size_t n0 = serializeJson(d, buf, sizeof buf);
  const uint32_t h = fnv(buf, n0);
  const uint32_t period_ms = 1000u * (uint32_t)(f.s.heating_active ? f.c.state_active_s : f.c.state_idle_s);
  const bool due = now - g_state_ms >= period_ms;
  const bool changed = h != g_state_hash && now - g_state_ms >= 1000;   // değişimde ≤ 1 s
  if (!(force || due || changed)) {
    // config/reported ve alarm listesi kendi özetleriyle
  } else {
    o["seq"] = seq;
    if (net::clockValid()) o["ts"] = (int64_t)time(nullptr);
    o["uptime"] = up;
    if (pubJson("state", d, 1, true)) { g_state_hash = h; g_state_ms = now; g_force_state = false; }
  }
  // Konfigürasyon yansıması (özet değişince)
  JsonDocument c;
  app::writeConfigReported(c.to<JsonObject>(), f.c);
  const uint32_t ch = fnv(c["config_hash"] | "", 8);
  if (ch != g_cfg_hash && pubJson("config/reported", c, 1, true)) g_cfg_hash = ch;
  // Keşif aç/kapa değişimi: yeniden yayın veya temizlik
  if (f.c.discovery_enabled != g_disc_enabled_seen && g_disc_i < 0) {
    g_disc_enabled_seen = f.c.discovery_enabled;
    g_disc_clear = !f.c.discovery_enabled;
    g_disc_i = 0;
  }
  if (now - g_diag_ms >= 1000u * (uint32_t)f.c.diag_interval_s) {
    JsonDocument g;
    app::writeDiag(g.to<JsonObject>(), f);
    if (pubJson("diag/state", g, 0, true)) g_diag_ms = now;
  }
}

void publishEvents() {
  cc::Event ev[8];
  uint16_t n = 0;
  uint32_t up_now = 0;
  if (!app::coreLock(20)) return;
  const auto& r = app::core().events();
  uint32_t dropped = 0;
  for (uint16_t i = 0; i < r.size() && n < 8; ++i) {
    const cc::Event& e = r.at(i);
    if (e.seq > g_ev_seq) ev[n++] = e;
  }
  if (r.size() && r.at(0).seq > g_ev_seq + 1 && g_ev_seq) dropped = r.at(0).seq - g_ev_seq - 1;   // halka taştı
  up_now = app::core().uptimeMs() / 1000;
  app::coreUnlock();
  if (dropped) bump([dropped](Status& s) { s.events_dropped += dropped; });
  const bool clock = net::clockValid();
  const int64_t now = (int64_t)time(nullptr);
  char t[160];
  topic(t, sizeof t, "event");
  for (uint16_t i = 0; i < n; ++i) {
    const cc::Event& e = ev[i];
    JsonDocument d;
    d["v"] = 1;
    d["seq"] = e.seq;
    if (clock) d["ts"] = now - (int64_t)(up_now - e.up_s);
    d["up"] = e.up_s;
    d["sev"] = cc::name(e.sev);
    d["src"] = cc::name(e.src);
    d["code"] = cc::name(e.code);
    if (e.actor != cc::CmdSource::SYSTEM) d["actor"] = cc::name(e.actor);
    if (!std::isnan(e.val)) d["val"] = roundf(e.val * 100.f) / 100.f;
    char buf[256];
    const size_t len = serializeJson(d, buf, sizeof buf);
    if (!pub(t, buf, len, 0, false)) break;
    g_ev_seq = e.seq;
  }
}

// ---------------------------------------------------------------- komutlar
void handleRx(const Rx& r, uint32_t now) {
  if (!strcmp(r.topic, "homeassistant/status")) {
    if (!strcmp(r.data, "online") && g_disc_i < 0 && now - g_last_disc_ms >= 10000) {   // ≥ 10 s aralık
      g_disc_i = 0; g_disc_clear = !g_disc_enabled_seen; g_last_disc_ms = now; g_online_pending = true;
    }
    return;
  }
  char id[48];
  if (!cc::mqParseSetTopic(g_base, r.topic, id, sizeof id)) return;
  // Retained komut uygulanmaz: retain bayrağı veya abonelik sonrası ilk 2 s (§6.5)
  if (r.retain || now - g_sub_ms < 2000) { bump([](Status& s) { ++s.ignored_retained; }); return; }
  cc::CmdReply rep;
  uint32_t seq = 0;
  if (!app::coreLock(200)) {
    rep.result = cc::CmdResult::REJECTED_BUSY;
  } else {
    rep = app::core().command(id, r.data, cc::CmdSource::MQTT);
    seq = app::core().snapshot().seq + 1;
    app::coreUnlock();
  }
  const bool ok = rep.result == cc::CmdResult::ACCEPTED || rep.result == cc::CmdResult::OVERRIDDEN;
  bump([ok](Status& s) { ++s.commands; if (!ok) ++s.rejected; });
  JsonDocument d;
  d["v"] = 1;
  d["id"] = id;
  d["value"] = r.data;
  d["result"] = cc::name(rep.result);
  d["reason"] = cc::name(rep.reason);
  d["src"] = "MQTT";
  d["seq"] = seq;
  if (net::clockValid()) d["ts"] = (int64_t)time(nullptr);
  pubJson("ack", d, 1, false);
  g_force_state = true;   // kabul ya da ret: güncel state ≤ 1 s (Suite 8 s doğrulaması)
}

// ---------------------------------------------------------------- görev
void task(void*) {
  uint32_t last_hash_check = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(20));
    const uint32_t now = millis();
    const mqttcfg::Settings s = mqttcfg::settings();
    const uint32_t gen = mqttcfg::generation();
    if (!s.host[0]) {
      if (g_cli) teardown(true);
      if (status().state != State::Disabled) setState(State::Disabled, "Broker adresi boş: MQTT kapalı");
      continue;
    }
    if (g_cli && gen != g_gen) {
      // Ayar değişti: kök topic değiştiyse taşınma (eski retained temizliği), sonra yeniden bağlan
      if (strcmp(s.base, g_cfg.base)) migrateOld();
      teardown(true);
      g_next_try_ms = 0;
      g_backoff_s = 1;
    }
    if (!net::wifiOk()) {
      if (g_cli && g_connected) onDisconnected(now, false);
      if (!g_cli) setState(State::Connecting, "Wi-Fi bağlantısı bekleniyor");
      while (g_rx && uxQueueMessagesWaiting(g_rx)) { Rx r; xQueueReceive(g_rx, &r, 0); }
      continue;
    }
    if (!g_cli) { if ((int32_t)(now - g_next_try_ms) >= 0) create(); continue; }
    if (g_ev_conn.exchange(false)) onConnected(now);
    if (g_ev_disc.exchange(false)) onDisconnected(now, g_ev_auth.exchange(false));
    if (!g_connected) {
      const State st = status().state;
      // Olay gelmeyen asılı deneme (DNS/TCP) 15 s'de başarısız sayılır
      if (st == State::Connecting && now - g_attempt_ms > 15000) onDisconnected(now, false);
      // Yeniden deneme istemci yeniden kurularak yapılır (otomatik yeniden bağlanma kapalı; IDF sürümünden bağımsız)
      else if ((st == State::Backoff || st == State::AuthFail) && (int32_t)(now - g_next_try_ms) >= 0) {
        teardown(false);
        create();
      }
      continue;
    }
    // ---- bağlı
    Rx r;
    while (xQueueReceive(g_rx, &r, 0) == pdTRUE) handleRx(r, now);
    if (g_disc_i >= 0) { publishDiscoveryStep(now); continue; }   // keşif bitmeden başka yayın yok
    if (g_online_pending) {
      publishAlarms();
      publishState(now, true);
      JsonDocument g;
      app::Frame f;
      if (app::capture(f, 50)) { app::writeDiag(g.to<JsonObject>(), f); pubJson("diag/state", g, 0, true); g_diag_ms = now; }
      if (pubStr("avail", "online", 1, true)) { g_online_pending = false; g_avail_ms = now; }   // önce keşif, sonra online
      continue;
    }
    if (now - last_hash_check >= 200 || g_force_state) {
      last_hash_check = now;
      publishState(now, g_force_state);
      publishAlarms();
      publishEvents();
    }
    if (now - g_avail_ms >= 30000 && pubStr("avail", "online", 1, true)) g_avail_ms = now;   // 30 s tazeleme
  }
}

}  // namespace

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  g_rx = xQueueCreate(8, sizeof(Rx));
  xTaskCreatePinnedToCore(task, "mqtt", 8192, nullptr, 2, nullptr, 0);
}

Status status() { Lock l; return g_status; }

const char* stateName(State s) {
  switch (s) {
    case State::Connecting: return "CONNECTING";
    case State::Connected: return "CONNECTED";
    case State::Backoff: return "BACKOFF";
    case State::AuthFail: return "AUTH_FAIL";
    default: return "DISABLED";
  }
}

}  // namespace mq
