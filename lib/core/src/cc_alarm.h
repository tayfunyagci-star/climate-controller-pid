// Alarm kataloğu ve durum makinesi — ALARM_AND_EVENTS §2–§3.
// Onay koşulu/kilidi değiştirmez; reset yalnız koşul temizken; kilit reboot'ta korunur.
#pragma once
#include "cc_types.h"

namespace cc {

enum class AlarmId : uint8_t {
  SENSOR_FAULT, SENSOR_STALE, OVERTEMPERATURE, HEATER_FAN_FAULT, HEATING_TIMEOUT,
  HEATING_PERFORMANCE_LOW, UNEXPECTED_TEMPERATURE_RISE, OUTPUT_FAULT, MQTT_OFFLINE, WIFI_OFFLINE,
  CONFIGURATION_ERROR, WATCHDOG_RESET, RESTART_STORM, INTERNAL_FAULT, FROST_RISK_NO_SENSOR,
  HUMIDITY_HIGH, HUMIDITY_LOW, TIME_INVALID, RELAY_LIFE_WARNING, POST_COOL_TIMEOUT,
  COUNT_
};
constexpr uint8_t kAlarmCount = (uint8_t)AlarmId::COUNT_;
const char* name(AlarmId);

enum class AlarmState : uint8_t { NORMAL, PENDING, ACTIVE_UNACK, ACTIVE_ACK, CLEARED_UNACK, LATCHED };
const char* name(AlarmState);

struct AlarmDef {
  AlarmId id;
  Severity sev;          // varsayılan önem (koşul değişken önem verebilir)
  bool latching;
  uint32_t on_delay_ms;
  uint32_t off_delay_ms;
};
const AlarmDef& alarmDef(AlarmId id);

struct AlarmRec {
  AlarmState st = AlarmState::NORMAL;
  Severity sev = Severity::NONE;
  uint32_t occ = 0;           // occurrence
  Timer on_t, off_t;
  bool suppressed = false;    // SERVICE bastırması
  bool reset_granted = false; // kilit sıfırlandı; onayda LATCHED yerine NORMAL
};

// Geçiş olayları (olay günlüğü için)
struct AlarmEvent {
  AlarmId id;
  AlarmState from, to;
  Severity sev;
  uint32_t occ;
  bool suppressed;
};

struct AlarmSummary {
  bool alarm = false;
  Severity highest = Severity::NONE;   // alarm_state (NONE → "NORMAL")
  uint8_t active_count = 0;            // ACTIVE_* + LATCHED
  uint8_t unacked_count = 0;           // ACTIVE_UNACK + CLEARED_UNACK
};

// Kalıcılık kaydı (kilitli alarmlar ve onay durumu)
struct AlarmPersist {
  uint8_t st[kAlarmCount];
  uint8_t sev[kAlarmCount];
  uint32_t occ[kAlarmCount];
};

class AlarmManager {
 public:
  // Koşul girdisi (her değerlendirme periyodunda). sev=NONE → tanımdaki önem.
  void setCondition(AlarmId id, bool active, Severity sev = Severity::NONE);
  void step(uint32_t dt_ms, bool service_mode);
  bool ack(AlarmId id);
  uint8_t ackAll();
  // Reset: LATCHED → NORMAL, yalnız koşul temizken (safety kilidinin kalktığı çağıran tarafça doğrulanır)
  bool reset(AlarmId id);
  const AlarmRec& rec(AlarmId id) const { return r_[(uint8_t)id]; }
  AlarmSummary summary() const;
  // Olay kuyruğu (FIFO, taşmada en eski düşer)
  bool popEvent(AlarmEvent& e);
  uint32_t droppedEvents() const { return dropped_; }
  void exportPersist(AlarmPersist& p) const;
  // INTERNAL_FAULT kilidi başarılı self-test ile kalkar (ALARM §3)
  void importPersist(const AlarmPersist& p, bool self_test_passed);

 private:
  void transition(uint8_t i, AlarmState to);
  AlarmRec r_[kAlarmCount];
  bool cond_[kAlarmCount] = {};
  Severity cond_sev_[kAlarmCount] = {};
  AlarmEvent q_[32];
  uint8_t qh_ = 0, qn_ = 0;
  uint32_t dropped_ = 0;
};

}  // namespace cc
