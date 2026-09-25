#include "cc_config_validate.h"
#include <cmath>
#include <cstring>

namespace cc {

const char* name(ValCode v) {
  static const char* const t[] = {"OK", "INVALID_FORMAT", "INVALID_RANGE", "INVALID_STEP", "UNKNOWN_FIELD",
                                  "INVALID_RELATION", "INVALID_SAFETY_MARGIN", "INVALID_DRIVER_TIMING",
                                  "REQUIRES_T2", "INVALID_PID", "WARN_PROFILE_ORDER", "WARN_PID_KD"};
  unsigned i = (unsigned)v;
  return i < sizeof(t) / sizeof(t[0]) ? t[i] : "?";
}

bool ValidationResult::hasRule(uint8_t rule) const {
  for (int i = 0; i < nErrors; ++i)
    if (errors[i].rule == rule) return true;
  for (int i = 0; i < nWarnings; ++i)
    if (warnings[i].rule == rule) return true;
  return false;
}
bool ValidationResult::hasErrorOn(const char* field) const {
  for (int i = 0; i < nErrors; ++i)
    if (std::strcmp(errors[i].field, field) == 0) return true;
  return false;
}
void ValidationResult::addError(uint8_t rule, ValCode c, const char* f) {
  if (nErrors < kMax) errors[nErrors++] = {rule, c, f};
}
void ValidationResult::addWarning(uint8_t rule, ValCode c, const char* f) {
  if (nWarnings < 8) warnings[nWarnings++] = {rule, c, f};
}

static bool onStep(float v, float step) {
  if (step <= 0) return true;
  float q = v / step;
  return std::fabs(q - std::round(q)) < 1e-3f;
}

ValidationResult validate(const Config& c) {
  ValidationResult r;
  // --- Alan aralıkları ---
  for (size_t i = 0; i < fieldCount(); ++i) {
    const FieldInfo& f = fieldAt(i);
    float v = fieldValue(c, f);
    if (!std::isfinite(v) || v < f.min - 1e-6f || v > f.max + 1e-6f) {
      r.addError(0, ValCode::INVALID_RANGE, f.key);
    } else if (f.kind == FieldKind::FLOAT && !onStep(v, f.step)) {
      r.addError(0, ValCode::INVALID_STEP, f.key);
    }
  }
  // --- İlişkiler ---
  // V1
  if (!(c.ventilation_stop_temperature <= c.ventilation_start_temperature - 1.0f + 1e-4f))
    r.addError(1, ValCode::INVALID_RELATION, "ventilation_stop_temperature");
  // V2
  if (!(c.humidity_low_limit <= c.humidity_high_limit - c.humidity_hysteresis - 5.0f + 1e-4f))
    r.addError(2, ValCode::INVALID_RELATION, "humidity_low_limit");
  // V3 (uyarı)
  if (!(c.setpoint_frost <= c.setpoint_away && c.setpoint_away <= c.setpoint_night &&
        c.setpoint_night <= c.temperature_setpoint && c.setpoint_boost >= c.temperature_setpoint))
    r.addWarning(3, ValCode::WARN_PROFILE_ORDER, "setpoint_night");
  // V4
  if (!(c.frost_guard_temperature < c.setpoint_frost))
    r.addError(4, ValCode::INVALID_RELATION, "frost_guard_temperature");
  // V5
  {
    float m = c.setpoint_boost > c.temperature_setpoint ? c.setpoint_boost : c.temperature_setpoint;
    if (!(c.cabin_overtemp_limit >= m + 10.0f - 1e-4f))
      r.addError(5, ValCode::INVALID_SAFETY_MARGIN, "cabin_overtemp_limit");
  }
  // V6
  if (!(c.cabin_overtemp_limit >= c.ventilation_start_temperature + 5.0f - 1e-4f))
    r.addError(6, ValCode::INVALID_SAFETY_MARGIN, "cabin_overtemp_limit");
  // V7
  if (!(c.stage2_off <= c.stage2_on - 5.0f + 1e-4f)) r.addError(7, ValCode::INVALID_RELATION, "stage2_off");
  // V8 — sürücü profiline göre zamanlama (OUTPUT_AND_INTERLOCKS §4.2)
  if (c.output_driver_r == DriverKind::RELAY) {
    if (c.tp_window_s < 300 || c.tp_window_s > 1800) r.addError(8, ValCode::INVALID_DRIVER_TIMING, "tp_window_s");
    if (c.heater_min_on_s < 60 || c.heater_min_on_s > 900)
      r.addError(8, ValCode::INVALID_DRIVER_TIMING, "heater_min_on_s");
    if (c.heater_min_off_s < 60 || c.heater_min_off_s > 900)
      r.addError(8, ValCode::INVALID_DRIVER_TIMING, "heater_min_off_s");
  } else {
    if (c.tp_window_s < 5 || c.tp_window_s > 120) r.addError(8, ValCode::INVALID_DRIVER_TIMING, "tp_window_s");
    if (c.heater_min_on_s < 1 || c.heater_min_on_s > 60)
      r.addError(8, ValCode::INVALID_DRIVER_TIMING, "heater_min_on_s");
    if (c.heater_min_off_s < 1 || c.heater_min_off_s > 60)
      r.addError(8, ValCode::INVALID_DRIVER_TIMING, "heater_min_off_s");
  }
  // V9
  if (!(c.heater_min_on_s + c.heater_min_off_s <= c.tp_window_s))
    r.addError(9, ValCode::INVALID_DRIVER_TIMING, "tp_window_s");
  // V10
  if (!(c.post_cool_min_s <= c.post_cool_seconds && c.post_cool_seconds <= c.post_cool_max_s))
    r.addError(10, ValCode::INVALID_RELATION, "post_cool_seconds");
  // V11
  if (c.post_cool_mode != PostCoolMode::TIME && !c.t2_enabled)
    r.addError(11, ValCode::REQUIRES_T2, "post_cool_mode");
  // V12
  if (c.pid_mode == PidMode::PI && !(c.pid_ki > 0)) r.addError(12, ValCode::INVALID_PID, "pid_ki");
  if (c.pid_mode == PidMode::PID && !(c.pid_kd > 0)) r.addWarning(12, ValCode::WARN_PID_KD, "pid_kd");
  // V13 — Kp·1°C ≤ 200 % ve Ti = Kp/Ki ≥ 1 dk
  if (!(c.pid_kp * 1.0f <= 200.0f) || !(c.pid_ki <= c.pid_kp)) r.addError(13, ValCode::INVALID_PID, "pid_ki");
  // V14
  if (!(c.control_interval_s >= c.sensor_interval_s))
    r.addError(14, ValCode::INVALID_RELATION, "control_interval_s");
  // V15
  if (!(c.sensor_stale_s >= 3 * c.sensor_interval_s)) r.addError(15, ValCode::INVALID_RELATION, "sensor_stale_s");
  // V16 — Suite nokta geçerlilik süresinden kısa
  if (!(c.state_idle_s < 60)) r.addError(16, ValCode::INVALID_RANGE, "state_idle_s");
  return r;
}

bool parseNumberStrict(const char* s, float& out) {
  if (!s || !*s) return false;
  const char* p = s;
  bool neg = false;
  if (*p == '-' || *p == '+') { neg = (*p == '-'); ++p; }
  if (!*p) return false;
  double v = 0;
  int digits = 0;
  while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; ++digits; }
  if (*p == '.') {
    ++p;
    double scale = 0.1;
    int fd = 0;
    while (*p >= '0' && *p <= '9') { v += (*p - '0') * scale; scale *= 0.1; ++p; ++fd; }
    if (fd == 0) return false;
    digits += fd;
  }
  if (*p != '\0' || digits == 0 || digits > 12) return false;
  out = (float)(neg ? -v : v);
  return true;
}

// Uzak (MQTT) yazım politikası
static bool remoteAllowed(const Config& cur, uint16_t flags) {
  if (flags & CF_SAFETY) return false;
  if (flags & CF_OPER) return true;
  if (flags & CF_RW_PID) return cur.remote_config_enabled && cur.pid_remote_tuning;
  if (flags & CF_RW) return cur.remote_config_enabled;
  return false;
}

SetResult setField(const Config& current, const char* key, const char* payload, CmdSource src, Config& out) {
  const FieldInfo* f = findField(key);
  if (!f) return {CmdResult::REJECTED_INVALID, ValCode::UNKNOWN_FIELD, 0};
  if (src == CmdSource::MQTT && !remoteAllowed(current, f->flags))
    return {CmdResult::REJECTED_POLICY, ValCode::OK, 0};

  Config cand = current;
  char* base = reinterpret_cast<char*>(&cand) + f->offset;
  switch (f->kind) {
    case FieldKind::FLOAT:
    case FieldKind::INT: {
      float v;
      if (!parseNumberStrict(payload, v)) return {CmdResult::REJECTED_INVALID, ValCode::INVALID_FORMAT, 0};
      if (v < f->min - 1e-6f || v > f->max + 1e-6f) return {CmdResult::REJECTED_INVALID, ValCode::INVALID_RANGE, 0};
      if (f->kind == FieldKind::INT) {
        if (std::fabs(v - std::round(v)) > 1e-6f) return {CmdResult::REJECTED_INVALID, ValCode::INVALID_STEP, 0};
        *reinterpret_cast<int32_t*>(base) = (int32_t)std::lround(v);
      } else {
        if (!onStep(v, f->step)) return {CmdResult::REJECTED_INVALID, ValCode::INVALID_STEP, 0};
        *reinterpret_cast<float*>(base) = v;
      }
      break;
    }
    case FieldKind::BOOL:
      if (payload && std::strcmp(payload, "ON") == 0) *reinterpret_cast<bool*>(base) = true;
      else if (payload && std::strcmp(payload, "OFF") == 0) *reinterpret_cast<bool*>(base) = false;
      else return {CmdResult::REJECTED_INVALID, ValCode::INVALID_FORMAT, 0};
      break;
    case FieldKind::ENUM: {
      int idx = -1;
      for (uint8_t i = 0; payload && i < f->enumCount; ++i)
        if (std::strcmp(payload, f->enumNames[i]) == 0) idx = i;
      if (idx < 0) return {CmdResult::REJECTED_INVALID, ValCode::INVALID_FORMAT, 0};
      *reinterpret_cast<uint8_t*>(base) = (uint8_t)idx;
      break;
    }
  }
  ValidationResult vr = validate(cand);
  if (!vr.ok()) {
    // V17: tek komut ilişki ihlali oluşturuyorsa ret (kıstırma yok)
    return {CmdResult::REJECTED_RELATION, vr.errors[0].code, vr.errors[0].rule};
  }
  out = cand;
  return {CmdResult::ACCEPTED, ValCode::OK, 0};
}

}  // namespace cc
