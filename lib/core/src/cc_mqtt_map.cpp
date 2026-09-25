#include "cc_mqtt_map.h"
#include <cstdio>
#include <cstring>
#include "cc_config.h"

namespace cc {

namespace {

using C = MqComp;
using S = MqSrc;

// comp, id, ad (sabit), birim, device_class, kaynak, ikon
const MqEntity kEntities[] = {
    // ---- Proses (B/state) — ENTITY_MODEL §2
    {C::SENSOR, "temperature", "Kulübe sıcaklığı", "°C", "temperature", S::STATE, nullptr},
    {C::SENSOR, "humidity", "Kulübe nemi", "%", "humidity", S::STATE, nullptr},
    {C::NUMBER, "temperature_setpoint", "Sıcaklık hedefi", "°C", "temperature", S::STATE, nullptr},
    {C::SENSOR, "setpoint_effective", "Etkin hedef", "°C", "temperature", S::STATE, nullptr},
    {C::SENSOR, "setpoint_source", "Hedef kaynağı", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "heat_demand", "Isı talebi", "%", nullptr, S::STATE, "mdi:radiator"},
    {C::SENSOR, "pid_output", "PID çıkışı", "%", nullptr, S::STATE, nullptr},
    {C::SENSOR, "r1_duty", "R1 oranı", "%", nullptr, S::STATE, nullptr},
    {C::SENSOR, "r2_duty", "R2 oranı", "%", nullptr, S::STATE, nullptr},
    {C::SENSOR, "power_stage", "Güç kademesi", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "temperature_rate", "Sıcaklık değişim hızı", "°C/h", nullptr, S::STATE, nullptr},
    {C::BINARY, "r1_active", "R1 çalışıyor", nullptr, "heat", S::STATE, nullptr},
    {C::BINARY, "r2_active", "R2 çalışıyor", nullptr, "heat", S::STATE, nullptr},
    {C::BINARY, "heater_fan_active", "Isıtıcı fanı çalışıyor", nullptr, "running", S::STATE, nullptr},
    {C::BINARY, "ventilation_fan_active", "Havalandırma çalışıyor", nullptr, "running", S::STATE, nullptr},
    {C::BINARY, "heating_active", "Isıtma aktif", nullptr, "heat", S::STATE, nullptr},
    {C::BINARY, "ventilation_active", "Havalandırma aktif", nullptr, "running", S::STATE, nullptr},
    {C::SWITCH, "heater_fan_manual", "Isıtıcı fanı isteği", nullptr, nullptr, S::STATE, "mdi:fan"},
    {C::SWITCH, "ventilation_fan_manual", "Havalandırma isteği", nullptr, nullptr, S::STATE, "mdi:fan"},
    {C::SENSOR, "r1_reason", "R1 nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "r2_reason", "R2 nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "heater_fan_reason", "Isıtıcı fanı nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "ventilation_fan_reason", "Havalandırma nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::SELECT, "operating_mode", "Çalışma modu", nullptr, nullptr, S::STATE, nullptr},
    {C::SWITCH, "controller_enable", "Kontrolör etkin", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "controller_state", "Kontrolör durumu", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "heating_phase", "Isıtma fazı", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "ventilation_state", "Havalandırma durumu", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "heating_reason", "Isıtma nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "failsafe_reason", "Güvenli durum nedeni", nullptr, nullptr, S::STATE, nullptr},
    {C::NUMBER, "manual_heat_demand", "Manuel ısı talebi", "%", nullptr, S::STATE, nullptr},
    {C::SELECT, "profile", "Profil", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "profile_active", "Etkin profil", nullptr, nullptr, S::STATE, nullptr},
    {C::SWITCH, "boost", "Boost", nullptr, nullptr, S::STATE, "mdi:rocket-launch"},
    {C::SENSOR, "boost_remaining_min", "Boost kalan", "min", "duration", S::STATE, nullptr},
    {C::SWITCH, "sched_night", "Gece programı isteği", nullptr, nullptr, S::STATE, "mdi:weather-night"},
    {C::SWITCH, "sched_away", "Uzakta programı isteği", nullptr, nullptr, S::STATE, "mdi:home-export-outline"},
    {C::SWITCH, "programs_enabled", "Yerel programlar", nullptr, nullptr, S::STATE, "mdi:calendar-clock"},
    {C::SENSOR, "program_active", "Etkin yerel program", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "program_until", "Program bitişi", nullptr, nullptr, S::STATE, nullptr},
    {C::BUTTON, "program_hold", "Etkin programı atla", nullptr, nullptr, S::STATE, nullptr},
    {C::BINARY, "sensor_ok", "Sensör sağlıklı", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "temperature_quality", "Sıcaklık kalitesi", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "humidity_quality", "Nem kalitesi", nullptr, nullptr, S::STATE, nullptr},
    {C::BINARY, "overtemperature", "Aşırı sıcaklık", nullptr, "problem", S::STATE, nullptr},
    {C::BINARY, "alarm", "Alarm var", nullptr, "problem", S::STATE, nullptr},
    {C::SENSOR, "alarm_state", "Alarm seviyesi", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "active_alarm_count", "Etkin alarm sayısı", nullptr, nullptr, S::STATE, nullptr},
    {C::SENSOR, "unacked_alarm_count", "Onaysız alarm sayısı", nullptr, nullptr, S::STATE, nullptr},
    {C::BUTTON, "alarm_ack", "Alarmları onayla", nullptr, nullptr, S::STATE, "mdi:bell-check"},
    {C::BINARY, "local_lock", "Yerel kilit", nullptr, "lock", S::STATE, nullptr},
    {C::SENSOR, "last_command_source", "Son komut kaynağı", nullptr, nullptr, S::STATE, nullptr},
    // ---- Konfigürasyon (B/config/reported) — ENTITY_MODEL §3; yazım remote_config_enabled'a bağlı
    {C::NUMBER, "setpoint_night", "Gece hedefi", "°C", "temperature", S::CONFIG, nullptr},
    {C::NUMBER, "setpoint_away", "Uzakta hedefi", "°C", "temperature", S::CONFIG, nullptr},
    {C::NUMBER, "setpoint_frost", "Donma hedefi", "°C", "temperature", S::CONFIG, nullptr},
    {C::NUMBER, "setpoint_boost", "Boost hedefi", "°C", "temperature", S::CONFIG, nullptr},
    {C::NUMBER, "boost_minutes", "Boost süresi", "min", nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "humidity_high_limit", "Nem üst sınırı", "%", nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "humidity_low_limit", "Nem alt sınırı", "%", nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "ventilation_start_temperature", "Havalandırma başlama sıcaklığı", "°C", "temperature", S::CONFIG, nullptr},
    {C::NUMBER, "ventilation_stop_temperature", "Havalandırma durma sıcaklığı", "°C", "temperature", S::CONFIG, nullptr},
    {C::SELECT, "pid_mode", "PID modu", nullptr, nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "pid_kp", "PID Kp", nullptr, nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "pid_ki", "PID Ki", nullptr, nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "pid_kd", "PID Kd", nullptr, nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "post_cool_seconds", "Soğutma süresi", "s", nullptr, S::CONFIG, nullptr},
    {C::NUMBER, "max_heat_demand", "Azami ısı talebi", "%", nullptr, S::CONFIG, nullptr},
    {C::SELECT, "humidity_vent_while_heating", "Isıtırken nem havalandırması", nullptr, nullptr, S::CONFIG, nullptr},
    // antifreeze_enabled uzaktan yazılamaz (güvenlik etkili) → yalnız okunur
    {C::BINARY, "antifreeze_enabled", "Donma koruması etkin", nullptr, nullptr, S::CONFIG, "mdi:snowflake-alert"},
    {C::BINARY, "remote_config_enabled", "MQTT konfigürasyon izni", nullptr, nullptr, S::CONFIG, nullptr},
    // ---- Tanı (B/diag/state) — ENTITY_MODEL §4
    {C::SENSOR, "uptime", "Çalışma süresi", "s", "duration", S::DIAG, nullptr},
    {C::SENSOR, "wifi_rssi", "Wi-Fi sinyali", "dBm", "signal_strength", S::DIAG, nullptr},
    {C::SENSOR, "free_heap", "Boş bellek", "B", nullptr, S::DIAG, nullptr},
    {C::SENSOR, "min_heap", "En düşük boş bellek", "B", nullptr, S::DIAG, nullptr},
    {C::SENSOR, "reset_reason", "Reset nedeni", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "mqtt_reconnects", "MQTT yeniden bağlanma", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "sensor_error_count", "Sensör hata sayısı", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "control_loop_max_ms", "En uzun kontrol döngüsü", "ms", nullptr, S::DIAG, nullptr},
    {C::SENSOR, "r1_hours", "R1 çalışma saati", "h", "duration", S::DIAG, nullptr},
    {C::SENSOR, "r2_hours", "R2 çalışma saati", "h", "duration", S::DIAG, nullptr},
    {C::SENSOR, "heater_fan_hours", "Isıtıcı fanı çalışma saati", "h", "duration", S::DIAG, nullptr},
    {C::SENSOR, "ventilation_fan_hours", "Havalandırma çalışma saati", "h", "duration", S::DIAG, nullptr},
    {C::SENSOR, "r1_switch_count", "R1 anahtarlama sayısı", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "r2_switch_count", "R2 anahtarlama sayısı", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "heater_fan_switch_count", "Isıtıcı fanı anahtarlama sayısı", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "ventilation_fan_switch_count", "Havalandırma anahtarlama sayısı", nullptr, nullptr, S::DIAG, nullptr},
    {C::SENSOR, "heating_minutes_today", "Bugün ısıtma", "min", "duration", S::DIAG, nullptr},
    {C::SENSOR, "duty_cycle_24h", "24 saat doluluk", "%", nullptr, S::DIAG, nullptr},
    {C::SENSOR, "fw_version", "Firmware sürümü", nullptr, nullptr, S::DIAG, nullptr},
};
constexpr size_t kCount = sizeof kEntities / sizeof kEntities[0];

const char* srcTopic(MqSrc s) {
  switch (s) {
    case MqSrc::CONFIG: return "~/config/reported";
    case MqSrc::DIAG: return "~/diag/state";
    default: return "~/state";
  }
}

// Sınırlı JSON yazıcı: taşarsa ok=false, çıktı kullanılmaz
struct W {
  char* p;
  size_t cap, n = 0;
  bool ok = true;
  bool first = true;
  W(char* out, size_t c) : p(out), cap(c) { if (cap) p[0] = 0; }
  void raw(const char* s) {
    const size_t l = strlen(s);
    if (!ok || n + l + 1 > cap) { ok = false; return; }
    memcpy(p + n, s, l + 1);
    n += l;
  }
  void ch(char c) { char b[2] = {c, 0}; raw(b); }
  void str(const char* s) {
    ch('"');
    for (; s && *s; ++s) {
      const unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\') { ch('\\'); ch((char)c); }
      else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); raw(b); }
      else ch((char)c);
    }
    ch('"');
  }
  void key(const char* k) { if (!first) ch(','); first = false; str(k); ch(':'); }
  void kv(const char* k, const char* v) { if (!v) return; key(k); str(v); }
  void kn(const char* k, float v) { char b[24]; snprintf(b, sizeof b, "%g", (double)v); key(k); raw(b); }
  void kraw(const char* k, const char* v) { key(k); raw(v); }
};

}  // namespace

size_t mqEntityCount() { return kCount; }
const MqEntity& mqEntity(size_t i) { return kEntities[i < kCount ? i : 0]; }
const MqEntity* mqFind(const char* id) {
  for (const MqEntity& e : kEntities) if (id && !strcmp(e.id, id)) return &e;
  return nullptr;
}

const char* mqCompName(MqComp c) {
  switch (c) {
    case MqComp::BINARY: return "binary_sensor";
    case MqComp::SWITCH: return "switch";
    case MqComp::NUMBER: return "number";
    case MqComp::SELECT: return "select";
    case MqComp::BUTTON: return "button";
    default: return "sensor";
  }
}

bool mqWritable(const MqEntity& e) {
  return e.comp == MqComp::SWITCH || e.comp == MqComp::NUMBER || e.comp == MqComp::SELECT || e.comp == MqComp::BUTTON;
}

bool mqDiscoveryTopic(const MqIdentity& idn, const MqEntity& e, char* out, size_t cap) {
  const int n = snprintf(out, cap, "homeassistant/%s/%s_%s/config", mqCompName(e.comp), idn.slug, e.id);
  return n > 0 && (size_t)n < cap;
}

size_t mqDiscoveryPayload(const MqIdentity& idn, const MqEntity& e, char* out, size_t cap) {
  W w(out, cap);
  char buf[160];
  w.ch('{');
  w.kv("~", idn.base);
  w.kv("name", e.name);
  snprintf(buf, sizeof buf, "%s_%s", idn.slug, e.id);
  w.kv("uniq_id", buf);
  snprintf(buf, sizeof buf, "{{ value_json.%s }}", e.id);
  if (e.comp != MqComp::BUTTON) {
    w.kv("stat_t", srcTopic(e.src));
    w.kv("val_tpl", buf);
  }
  if (mqWritable(e)) {
    snprintf(buf, sizeof buf, "~/%s/set", e.id);
    w.kv("cmd_t", buf);
  }
  w.kv("avty_t", "~/avail");
  w.kv("pl_avail", "online");
  w.kv("pl_not_avail", "offline");
  w.kv("unit_of_meas", e.unit);
  w.kv("dev_cla", e.dev_cla);
  w.kv("ic", e.icon);
  const FieldInfo* f = findField(e.id);
  switch (e.comp) {
    case MqComp::BINARY:
    case MqComp::SWITCH:
      w.kv("pl_on", "ON");
      w.kv("pl_off", "OFF");
      break;
    case MqComp::NUMBER:
      if (f) {
        w.kn("min", f->min);
        w.kn("max", f->max);
        w.kn("step", f->step > 0 ? f->step : 1.0f);
      }
      w.kv("mode", "box");
      break;
    case MqComp::SELECT:
      if (f && f->kind == FieldKind::ENUM) {
        w.key("options");
        w.ch('[');
        for (uint8_t i = 0; i < f->enumCount; ++i) { if (i) w.ch(','); w.str(f->enumNames[i]); }
        w.ch(']');
      }
      break;
    case MqComp::BUTTON:
      w.kv("pl_prs", "PRESS");
      if (!strcmp(e.id, "alarm_ack")) {
        // Buton geri bildirimi: her geçerli PRESS sayacı artırır (uzun anahtar zorunlu, M10)
        w.kv("json_attr_t", "~/state");
        w.kv("json_attributes_template", "{{ value_json.ack_count }}");
      }
      break;
    case MqComp::SENSOR:
      if (e.unit && strcmp(e.unit, "s") && strcmp(e.unit, "h")) w.kv("stat_cla", "measurement");
      break;
  }
  if (e.src == MqSrc::CONFIG) w.kv("ent_cat", "config");
  if (e.src == MqSrc::DIAG) w.kv("ent_cat", "diagnostic");
  w.key("dev");
  w.ch('{');
  w.first = true;
  w.key("ids");
  w.ch('[');
  w.str(idn.slug);
  w.ch(']');
  w.kv("name", idn.dev_name);
  w.kv("mf", "Kendi Yapımı");
  w.kv("mdl", "ESP32 Climate PID");
  w.kv("sw", idn.sw);
  w.ch('}');
  w.first = false;
  w.key("o");
  w.ch('{');
  w.first = true;
  w.kv("name", "esp-climate-node");   // "bridge" geçmez: kaynak `ha` (köprü kipi tuzağı)
  w.kv("sw", idn.sw);
  w.ch('}');
  w.ch('}');
  return w.ok ? w.n : 0;
}

bool mqParseSetTopic(const char* base, const char* topic, char* id, size_t cap) {
  if (!base || !topic) return false;
  const size_t bl = strlen(base);
  if (strncmp(topic, base, bl) || topic[bl] != '/') return false;
  const char* s = topic + bl + 1;
  const char* slash = strchr(s, '/');
  if (!slash || strcmp(slash, "/set")) return false;
  const size_t n = (size_t)(slash - s);
  if (!n || n >= cap) return false;
  memcpy(id, s, n);
  id[n] = 0;
  const MqEntity* e = mqFind(id);
  return e && mqWritable(*e);
}

}  // namespace cc
