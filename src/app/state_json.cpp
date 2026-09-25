#include "state_json.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include "boot_state.h"
#include "mqtt_client.h"
#include "net_manager.h"
#include "tasks.h"
#include "web.h"
#include "version.h"

namespace app {

const char* onoff(bool b) { return b ? "ON" : "OFF"; }

void fnum(JsonObject o, const char* k, float v, int dec) {
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

bool capture(Frame& f, uint32_t timeout_ms) {
  if (!coreLock(timeout_ms)) return false;
  f.s = core().snapshot();
  f.c = core().config();
  f.hpm = core().hpm();
  for (uint8_t k = 0; k < 4; ++k) {
    f.sw[k] = core().guard().switchCount(k);
    f.on_ms[k] = core().guard().onTimeMs(k);
    f.svc[k] = core().serviceTestOn(k);
  }
  f.vsrc = core().vent().sources;
  f.err_rate = core().t1().error_rate_10m;
  strcpy(f.prog, "—");
  if (f.s.program_index >= 0 && f.s.program_index < core().programCount()) {
    strncpy(f.prog, core().programs()[f.s.program_index].name, sizeof f.prog - 1);
    f.prog[sizeof f.prog - 1] = 0;
  }
  coreUnlock();
  return true;
}

namespace {
const char* enumName(const cc::Config& c, const char* key) {
  const cc::FieldInfo* f = cc::findField(key);
  if (!f || f->kind != cc::FieldKind::ENUM) return "?";
  const uint8_t v = (uint8_t)cc::fieldValue(c, *f);
  return v < f->enumCount ? f->enumNames[v] : "?";
}
const char* const kOuts[4] = {"r1", "r2", "heater_fan", "ventilation_fan"};
}  // namespace

void writeProcess(JsonObject d, const Frame& f) {
  const cc::CoreSnapshot& s = f.s;
  d["v"] = 1;
  d["seq"] = s.seq;
  if (net::clockValid()) d["ts"] = (int64_t)time(nullptr);   // saat geçersizse alan yazılmaz
  d["uptime"] = s.uptime_s;
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
  fnum(d, "r1_duty", s.r1_duty, 1);
  fnum(d, "r2_duty", s.r2_duty, 1);
  d["power_stage"] = s.power_stage;
  fnum(d, "temperature_rate", s.temperature_rate, 1);
  char k[40];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(k, sizeof k, "%s_active", kOuts[i]); d[k] = onoff(s.active[i]);
    snprintf(k, sizeof k, "%s_reason", kOuts[i]); d[k] = cc::name(s.reason[i]);
  }
  d["heating_active"] = onoff(s.heating_active);
  d["ventilation_active"] = onoff(s.ventilation_active);
  d["heater_fan_manual"] = onoff(s.heater_fan_manual);
  d["ventilation_fan_manual"] = onoff(s.ventilation_fan_manual);
  d["overtemperature"] = onoff(s.overtemperature);
  d["alarm"] = onoff(s.alarm);
  d["alarm_state"] = cc::name(s.alarm_state);
  d["active_alarm_count"] = s.active_alarm_count;
  d["unacked_alarm_count"] = s.unacked_alarm_count;
  d["local_lock"] = onoff(s.local_lock);
  d["last_command_source"] = cc::name(s.last_command_source);
  d["ack_count"] = s.ack_count;
  d["programs_enabled"] = onoff(s.programs_enabled);
  d["program_active"] = f.prog;
  if (s.program_index >= 0 && s.program_until >= 0) { char iso[24]; isoLocal(s.program_until, iso); d["program_until"] = iso; }
  else d["program_until"] = "";
}

void writeWebExtras(JsonObject d, const Frame& f) {
  const cc::CoreSnapshot& s = f.s;
  const cc::Config& c = f.c;
  fnum(d, "setpoint_night", c.setpoint_night, 1);
  fnum(d, "setpoint_away", c.setpoint_away, 1);
  fnum(d, "setpoint_frost", c.setpoint_frost, 1);
  fnum(d, "setpoint_boost", c.setpoint_boost, 1);
  d["boost_minutes"] = c.boost_minutes;
  fnum(d, "frost_guard_temperature", c.frost_guard_temperature, 1);
  fnum(d, "pid_error", s.pid_error);
  fnum(d, "pid_p", s.pid_p, 1);
  fnum(d, "pid_i", s.pid_i, 1);
  fnum(d, "pid_d", s.pid_d, 1);
  d["pid_saturation"] = cc::name(s.pid_saturation);
  d["anti_windup_active"] = onoff(s.anti_windup_active);
  d["pid_tracking"] = onoff(s.pid_tracking);
  fnum(d, "stage2_on", c.stage2_on, 0);
  fnum(d, "stage2_off", c.stage2_off, 0);
  d["post_cool_remaining_s"] = s.post_cool_remaining_s;
  fnum(d, "ventilation_start_effective", s.ventilation_start_effective, 1);
  fnum(d, "ventilation_start_temperature", c.ventilation_start_temperature, 1);
  fnum(d, "ventilation_stop_temperature", c.ventilation_stop_temperature, 1);
  fnum(d, "humidity_high_limit", c.humidity_high_limit, 0);
  fnum(d, "humidity_hysteresis", c.humidity_hysteresis, 0);
  d["humidity_vent_while_heating"] = enumName(c, "humidity_vent_while_heating");
  d["manual_vent_priority"] = enumName(c, "manual_vent_priority");
  d["program_held"] = onoff(s.program_held);
}

void writeConfigReported(JsonObject d, const cc::Config& c) {
  // Konfigürasyon entity'lerinin state kaynağı + web dışı tüketiciler için sırsız özet (≤ 2048 B tampon)
  static const char* const keys[] = {
      "setpoint_night", "setpoint_away", "setpoint_frost", "setpoint_boost", "boost_minutes", "setpoint_ramp_c_per_min",
      "pid_mode", "pid_kp", "pid_ki", "pid_kd", "humidity_high_limit", "humidity_low_limit", "humidity_hysteresis",
      "ventilation_start_temperature", "ventilation_stop_temperature", "post_cool_seconds", "max_heat_demand",
      "antifreeze_enabled", "frost_guard_temperature", "humidity_vent_while_heating", "manual_vent_priority",
      "remote_config_enabled", "pid_remote_tuning", "remote_manual_allowed", "output_driver_r"};
  d["v"] = 1;
  uint32_t h = 2166136261u;   // FNV-1a: değişim algılama ve tüketici için özet
  for (const char* key : keys) {
    const cc::FieldInfo* f = cc::findField(key);
    if (!f) continue;
    const float v = cc::fieldValue(c, *f);
    switch (f->kind) {
      case cc::FieldKind::FLOAT: fnum(d, key, v, 2); break;
      case cc::FieldKind::INT: d[key] = (int32_t)lroundf(v); break;
      case cc::FieldKind::BOOL: d[key] = onoff(v != 0); break;
      case cc::FieldKind::ENUM: d[key] = ((uint8_t)v < f->enumCount) ? f->enumNames[(uint8_t)v] : "?"; break;
    }
    const uint32_t bits = (uint32_t)lroundf(v * 1000.f);
    for (int i = 0; i < 4; ++i) { h ^= (bits >> (8 * i)) & 0xFF; h *= 16777619u; }
  }
  char hx[9];
  snprintf(hx, sizeof hx, "%08x", (unsigned)h);
  d["config_hash"] = hx;
}

void writeDiag(JsonObject d, const Frame& f) {
  const net::Status ns = net::status();
  const TaskStats ts = stats();
  const mq::Status ms = mq::status();
  d["v"] = 1;
  d["uptime"] = f.s.uptime_s;
  d["reset_reason"] = resetReasonName();
  d["fault_boot_count"] = rtcFaultBoots();
  d["free_heap"] = ESP.getFreeHeap();
  d["min_heap"] = ESP.getMinFreeHeap();
  if (ns.sta_ok) d["wifi_rssi"] = ns.rssi; else d["wifi_rssi"] = nullptr;
  d["ip"] = ns.ip;
  d["wifi_reconnects"] = ns.reconnects;
  d["mqtt_reconnects"] = ms.reconnects;
  d["control_loop_max_ms"] = (ts.max_us[2] + 999) / 1000;
  d["safety_loop_max_ms"] = (ts.max_us[0] + 999) / 1000;
  d["sensor_error_count"] = ts.dht_err + ts.dht_timeout;
  d["sensor_crc_errors"] = ts.dht_crc;
  fnum(d, "sensor_error_rate_10m", f.err_rate, 1);
  d["sensor_model"] = "DHT22";
  char k[40];
  for (uint8_t i = 0; i < 4; ++i) {
    snprintf(k, sizeof k, "%s_hours", kOuts[i]); d[k] = (float)(f.on_ms[i] / 36000ULL) / 100.0f;
    snprintf(k, sizeof k, "%s_switch_count", kOuts[i]); d[k] = f.sw[i];
  }
  d["heating_minutes_today"] = f.hpm.heating_minutes_today;
  fnum(d, "duty_cycle_24h", f.hpm.duty_24h, 1);
  d["heating_cycles_1h"] = f.hpm.cycles_1h;
  fnum(d, "heating_efficiency_index", f.hpm.efficiency_index, 2);
  d["time_valid"] = onoff(ns.clock_valid);
  d["fw_version"] = kFwVersion;
  d["fw_build"] = web::uiBuild();
  d["hw_rev"] = "A";
}

}  // namespace app
