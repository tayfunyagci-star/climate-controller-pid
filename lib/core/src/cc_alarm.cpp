#include "cc_alarm.h"

namespace cc {

namespace {
constexpr uint32_t S = 1000, M = 60000;
// ALARM_AND_EVENTS §2. Safety kaynaklı alarmlarda gecikmeyi Safety uygular (onDelay 0).
const AlarmDef kDefs[kAlarmCount] = {
    {AlarmId::SENSOR_FAULT, Severity::CRITICAL, false, 0, 0},
    {AlarmId::SENSOR_STALE, Severity::CRITICAL, false, 0, 0},
    {AlarmId::OVERTEMPERATURE, Severity::CRITICAL, true, 0, 60 * S},
    {AlarmId::HEATER_FAN_FAULT, Severity::CRITICAL, true, 0, 0},
    {AlarmId::HEATING_TIMEOUT, Severity::CRITICAL, true, 0, 0},
    {AlarmId::HEATING_PERFORMANCE_LOW, Severity::WARNING, false, 0, 10 * M},
    {AlarmId::UNEXPECTED_TEMPERATURE_RISE, Severity::WARNING, false, 0, 10 * M},
    {AlarmId::OUTPUT_FAULT, Severity::CRITICAL, true, 0, 0},
    {AlarmId::MQTT_OFFLINE, Severity::WARNING, false, 60 * S, 0},
    {AlarmId::WIFI_OFFLINE, Severity::WARNING, false, 60 * S, 0},
    {AlarmId::CONFIGURATION_ERROR, Severity::CRITICAL, false, 0, 0},
    {AlarmId::WATCHDOG_RESET, Severity::WARNING, false, 0, 0},
    {AlarmId::RESTART_STORM, Severity::CRITICAL, true, 0, 0},
    {AlarmId::INTERNAL_FAULT, Severity::CRITICAL, true, 0, 0},
    {AlarmId::FROST_RISK_NO_SENSOR, Severity::CRITICAL, false, 0, 0},
    {AlarmId::HUMIDITY_HIGH, Severity::INFO, false, 0, 0},
    {AlarmId::HUMIDITY_LOW, Severity::INFO, false, 30 * M, 0},
    {AlarmId::TIME_INVALID, Severity::INFO, false, 0, 0},
    {AlarmId::RELAY_LIFE_WARNING, Severity::INFO, false, 0, 0},
    {AlarmId::POST_COOL_TIMEOUT, Severity::WARNING, false, 0, 0},
};
}  // namespace

const char* name(AlarmId v) {
  static const char* const t[] = {"SENSOR_FAULT", "SENSOR_STALE", "OVERTEMPERATURE", "HEATER_FAN_FAULT",
                                  "HEATING_TIMEOUT", "HEATING_PERFORMANCE_LOW", "UNEXPECTED_TEMPERATURE_RISE",
                                  "OUTPUT_FAULT", "MQTT_OFFLINE", "WIFI_OFFLINE", "CONFIGURATION_ERROR",
                                  "WATCHDOG_RESET", "RESTART_STORM", "INTERNAL_FAULT", "FROST_RISK_NO_SENSOR",
                                  "HUMIDITY_HIGH", "HUMIDITY_LOW", "TIME_INVALID", "RELAY_LIFE_WARNING",
                                  "POST_COOL_TIMEOUT"};
  unsigned i = (unsigned)v;
  return i < kAlarmCount ? t[i] : "?";
}
const char* name(AlarmState v) {
  static const char* const t[] = {"normal", "pending", "active_unacknowledged", "active_acknowledged",
                                  "cleared_unacknowledged", "latched"};
  unsigned i = (unsigned)v;
  return i < 6 ? t[i] : "?";
}
const AlarmDef& alarmDef(AlarmId id) { return kDefs[(uint8_t)id]; }

void AlarmManager::setCondition(AlarmId id, bool active, Severity sev) {
  const uint8_t i = (uint8_t)id;
  cond_[i] = active;
  cond_sev_[i] = sev == Severity::NONE ? kDefs[i].sev : sev;
}

void AlarmManager::transition(uint8_t i, AlarmState to) {
  AlarmRec& r = r_[i];
  if (r.st == to) return;
  const bool newOcc = (to == AlarmState::ACTIVE_UNACK) &&
                      (r.st == AlarmState::NORMAL || r.st == AlarmState::PENDING ||
                       r.st == AlarmState::CLEARED_UNACK || r.st == AlarmState::LATCHED);
  if (newOcc) {
    r.reset_granted = false;
    ++r.occ;
    r.sev = cond_sev_[i];
  }
  AlarmEvent e{(AlarmId)i, r.st, to, r.sev, r.occ, r.suppressed};
  if (qn_ == 32) { qh_ = (uint8_t)((qh_ + 1) % 32); --qn_; ++dropped_; }
  q_[(qh_ + qn_) % 32] = e;
  ++qn_;
  r.st = to;
  r.on_t.reset();
  r.off_t.reset();
}

void AlarmManager::step(uint32_t dt_ms, bool service_mode) {
  for (uint8_t i = 0; i < kAlarmCount; ++i) {
    AlarmRec& r = r_[i];
    const AlarmDef& d = kDefs[i];
    bool cond = cond_[i];
    // SERVICE: INFO/WARNING bastırılır (olay kaydı suppressed=true), CRITICAL bastırılmaz
    r.suppressed = service_mode && cond && cond_sev_[i] < Severity::CRITICAL;
    if (r.suppressed) cond = false;
    if (cond && r.st != AlarmState::NORMAL && r.st != AlarmState::PENDING && cond_sev_[i] > r.sev)
      r.sev = cond_sev_[i];

    switch (r.st) {
      case AlarmState::NORMAL:
        if (cond) transition(i, d.on_delay_ms == 0 ? AlarmState::ACTIVE_UNACK : AlarmState::PENDING);
        break;
      case AlarmState::PENDING:
        if (!cond) transition(i, AlarmState::NORMAL);
        else {
          r.on_t.add(dt_ms);
          if (r.on_t.atLeast(d.on_delay_ms)) transition(i, AlarmState::ACTIVE_UNACK);
        }
        break;
      case AlarmState::ACTIVE_UNACK:
        if (!cond) {
          r.off_t.add(dt_ms);
          if (r.off_t.atLeast(d.off_delay_ms)) transition(i, AlarmState::CLEARED_UNACK);
        } else r.off_t.reset();
        break;
      case AlarmState::ACTIVE_ACK:
        if (!cond) {
          r.off_t.add(dt_ms);
          if (r.off_t.atLeast(d.off_delay_ms)) transition(i, d.latching ? AlarmState::LATCHED : AlarmState::NORMAL);
        } else r.off_t.reset();
        break;
      case AlarmState::CLEARED_UNACK:
      case AlarmState::LATCHED:
        if (cond) transition(i, AlarmState::ACTIVE_UNACK);
        break;
    }
  }
}

bool AlarmManager::ack(AlarmId id) {
  const uint8_t i = (uint8_t)id;
  AlarmRec& r = r_[i];
  if (r.st == AlarmState::ACTIVE_UNACK) { transition(i, AlarmState::ACTIVE_ACK); return true; }
  if (r.st == AlarmState::CLEARED_UNACK) {
    transition(i, (kDefs[i].latching && !r.reset_granted) ? AlarmState::LATCHED : AlarmState::NORMAL);
    return true;
  }
  return false;
}

uint8_t AlarmManager::ackAll() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < kAlarmCount; ++i) n += ack((AlarmId)i) ? 1 : 0;
  return n;
}

bool AlarmManager::reset(AlarmId id) {
  const uint8_t i = (uint8_t)id;
  AlarmRec& r = r_[i];
  if (cond_[i]) return false;  // koşul sürerken sıfırlanamaz (SR-11)
  if (r.st == AlarmState::LATCHED || r.st == AlarmState::ACTIVE_ACK) {
    transition(i, AlarmState::NORMAL);
    return true;
  }
  if (r.st == AlarmState::CLEARED_UNACK) {  // kilit kalktı; onay NORMAL'e götürür
    r.reset_granted = true;
    return true;
  }
  if (r.st == AlarmState::ACTIVE_UNACK) {  // koşul kalktı ama onay hâlâ gerekli
    transition(i, AlarmState::CLEARED_UNACK);
    r.reset_granted = true;  // kilit kalktı: onayda NORMAL
    return true;
  }
  return false;
}

AlarmSummary AlarmManager::summary() const {
  AlarmSummary s;
  for (uint8_t i = 0; i < kAlarmCount; ++i) {
    const AlarmRec& r = r_[i];
    const bool active = r.st == AlarmState::ACTIVE_UNACK || r.st == AlarmState::ACTIVE_ACK;
    const bool on = active || r.st == AlarmState::CLEARED_UNACK || r.st == AlarmState::LATCHED;
    if (on) {
      s.alarm = true;
      if (r.sev > s.highest) s.highest = r.sev;
    }
    if (active || r.st == AlarmState::LATCHED) ++s.active_count;
    if (r.st == AlarmState::ACTIVE_UNACK || r.st == AlarmState::CLEARED_UNACK) ++s.unacked_count;
  }
  return s;
}

bool AlarmManager::popEvent(AlarmEvent& e) {
  if (!qn_) return false;
  e = q_[qh_];
  qh_ = (uint8_t)((qh_ + 1) % 32);
  --qn_;
  return true;
}

void AlarmManager::exportPersist(AlarmPersist& p) const {
  for (uint8_t i = 0; i < kAlarmCount; ++i) {
    p.st[i] = (uint8_t)r_[i].st;
    p.sev[i] = (uint8_t)r_[i].sev;
    p.occ[i] = r_[i].occ;
  }
}

void AlarmManager::importPersist(const AlarmPersist& p, bool self_test_passed) {
  for (uint8_t i = 0; i < kAlarmCount; ++i) {
    AlarmState st = p.st[i] <= (uint8_t)AlarmState::LATCHED ? (AlarmState)p.st[i] : AlarmState::NORMAL;
    if (st == AlarmState::PENDING) st = AlarmState::NORMAL;
    if ((AlarmId)i == AlarmId::INTERNAL_FAULT && self_test_passed) st = AlarmState::NORMAL;
    // Kilitsiz alarmlar koşuldan yeniden türetilir; kilitli ve onaysız durumlar korunur
    if (!kDefs[i].latching && (st == AlarmState::ACTIVE_ACK || st == AlarmState::ACTIVE_UNACK)) st = AlarmState::NORMAL;
    r_[i].st = st;
    r_[i].sev = (Severity)p.sev[i];
    r_[i].occ = p.occ[i];
  }
}

}  // namespace cc
