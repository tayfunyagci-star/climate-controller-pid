// Alarm durum makinesi — ALARM_AND_EVENTS §2–§3 (tüm geçişler, kilit, onay, SERVICE bastırma, kalıcılık).
#include <unity.h>
#include "cc_alarm.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static AlarmState st(const AlarmManager& m, AlarmId id) { return m.rec(id).st; }

// NORMAL → ACTIVE_UNACK → ACTIVE_ACK → NORMAL (kilitsiz)
void test_non_latching_ack_path() {
  AlarmManager m;
  m.setCondition(AlarmId::SENSOR_FAULT, true);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, AlarmId::SENSOR_FAULT));
  TEST_ASSERT_TRUE(m.ack(AlarmId::SENSOR_FAULT));
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_ACK, st(m, AlarmId::SENSOR_FAULT));
  m.setCondition(AlarmId::SENSOR_FAULT, false);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, AlarmId::SENSOR_FAULT));
}

// onDelay: PENDING → NORMAL (erken kalkış) / → ACTIVE_UNACK
void test_pending_on_delay() {
  AlarmManager m;
  m.setCondition(AlarmId::MQTT_OFFLINE, true);
  m.step(1000, false);
  TEST_ASSERT_EQUAL(AlarmState::PENDING, st(m, AlarmId::MQTT_OFFLINE));
  m.setCondition(AlarmId::MQTT_OFFLINE, false);
  m.step(1000, false);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, AlarmId::MQTT_OFFLINE));
  m.setCondition(AlarmId::MQTT_OFFLINE, true);
  for (int i = 0; i < 61; ++i) m.step(1000, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, AlarmId::MQTT_OFFLINE));
}

// Kilitli: ACTIVE_UNACK → CLEARED_UNACK (offDelay) → onay → LATCHED → reset → NORMAL
void test_latching_path() {
  AlarmManager m;
  const AlarmId id = AlarmId::OVERTEMPERATURE;
  m.setCondition(id, true);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, id));
  TEST_ASSERT_FALSE(m.reset(id));  // koşul sürerken sıfırlanamaz
  m.setCondition(id, false);
  for (int i = 0; i < 59; ++i) m.step(1000, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, id));  // offDelay 60 s
  m.step(1000, false);
  TEST_ASSERT_EQUAL(AlarmState::CLEARED_UNACK, st(m, id));
  m.ack(id);
  TEST_ASSERT_EQUAL(AlarmState::LATCHED, st(m, id));
  AlarmSummary s = m.summary();
  TEST_ASSERT_TRUE(s.alarm);
  TEST_ASSERT_EQUAL(Severity::CRITICAL, s.highest);
  TEST_ASSERT_EQUAL_UINT8(1, s.active_count);
  TEST_ASSERT_TRUE(m.reset(id));
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, id));
  TEST_ASSERT_FALSE(m.summary().alarm);
}

// ACTIVE_ACK → (koşul temiz) LATCHED; LATCHED → koşul yeniden → ACTIVE_UNACK (yeni occurrence)
void test_latched_reoccurs() {
  AlarmManager m;
  const AlarmId id = AlarmId::HEATING_TIMEOUT;
  m.setCondition(id, true);
  m.step(250, false);
  const uint32_t occ1 = m.rec(id).occ;
  m.ack(id);
  m.setCondition(id, false);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::LATCHED, st(m, id));
  m.setCondition(id, true);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, id));
  TEST_ASSERT_EQUAL_UINT32(occ1 + 1, m.rec(id).occ);
  // Sürekli koşul yeni occurrence üretmez
  for (int i = 0; i < 10; ++i) m.step(250, false);
  TEST_ASSERT_EQUAL_UINT32(occ1 + 1, m.rec(id).occ);
}

// CLEARED_UNACK → koşul yeniden → ACTIVE_UNACK
void test_cleared_unack_reactivates() {
  AlarmManager m;
  const AlarmId id = AlarmId::SENSOR_STALE;
  m.setCondition(id, true);
  m.step(250, false);
  m.setCondition(id, false);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::CLEARED_UNACK, st(m, id));
  m.setCondition(id, true);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, id));
}

// Kilit sıfırlandıktan sonra onay NORMAL'e götürür
void test_reset_then_ack() {
  AlarmManager m;
  const AlarmId id = AlarmId::HEATER_FAN_FAULT;
  m.setCondition(id, true);
  m.step(250, false);
  m.setCondition(id, false);  // safety kilidi kalktı
  TEST_ASSERT_TRUE(m.reset(id));
  TEST_ASSERT_EQUAL(AlarmState::CLEARED_UNACK, st(m, id));
  m.ack(id);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, id));
}

// SERVICE: INFO/WARNING bastırılır, CRITICAL bastırılmaz
void test_service_suppression() {
  AlarmManager m;
  m.setCondition(AlarmId::HUMIDITY_HIGH, true);
  m.setCondition(AlarmId::SENSOR_FAULT, true, Severity::CRITICAL);
  m.step(250, true);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, AlarmId::HUMIDITY_HIGH));
  TEST_ASSERT_TRUE(m.rec(AlarmId::HUMIDITY_HIGH).suppressed);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, st(m, AlarmId::SENSOR_FAULT));
}

// Değişken önem ve özet alanlar
void test_summary_counts() {
  AlarmManager m;
  m.setCondition(AlarmId::SENSOR_FAULT, true, Severity::WARNING);
  m.setCondition(AlarmId::HUMIDITY_HIGH, true);
  m.step(250, false);
  AlarmSummary s = m.summary();
  TEST_ASSERT_EQUAL(Severity::WARNING, s.highest);
  TEST_ASSERT_EQUAL_UINT8(2, s.active_count);
  TEST_ASSERT_EQUAL_UINT8(2, s.unacked_count);
  m.ackAll();
  s = m.summary();
  TEST_ASSERT_EQUAL_UINT8(0, s.unacked_count);
  TEST_ASSERT_EQUAL_UINT8(2, s.active_count);
}

// WATCHDOG_RESET: anlık koşul, onayla kapanır
void test_watchdog_pulse() {
  AlarmManager m;
  m.setCondition(AlarmId::WATCHDOG_RESET, true);
  m.step(250, false);
  m.setCondition(AlarmId::WATCHDOG_RESET, false);
  m.step(250, false);
  TEST_ASSERT_EQUAL(AlarmState::CLEARED_UNACK, st(m, AlarmId::WATCHDOG_RESET));
  m.ack(AlarmId::WATCHDOG_RESET);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(m, AlarmId::WATCHDOG_RESET));
}

// D-16: kilit ve onay reboot'ta korunur; INTERNAL_FAULT başarılı self-test ile kalkar
void test_persistence() {
  AlarmManager a;
  a.setCondition(AlarmId::OVERTEMPERATURE, true);
  a.setCondition(AlarmId::INTERNAL_FAULT, true);
  a.setCondition(AlarmId::SENSOR_FAULT, true);
  a.step(250, false);
  a.setCondition(AlarmId::OVERTEMPERATURE, false);
  for (int i = 0; i < 61; ++i) a.step(1000, false);
  a.ack(AlarmId::OVERTEMPERATURE);
  AlarmPersist p;
  a.exportPersist(p);
  AlarmManager b;
  b.importPersist(p, true);
  TEST_ASSERT_EQUAL(AlarmState::LATCHED, st(b, AlarmId::OVERTEMPERATURE));
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(b, AlarmId::INTERNAL_FAULT));
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, st(b, AlarmId::SENSOR_FAULT));  // kilitsiz: koşuldan türetilir
  AlarmManager c;
  c.importPersist(p, false);
  TEST_ASSERT_NOT_EQUAL(AlarmState::NORMAL, st(c, AlarmId::INTERNAL_FAULT));
}

// Olay kuyruğu
void test_events() {
  AlarmManager m;
  m.setCondition(AlarmId::SENSOR_FAULT, true);
  m.step(250, false);
  AlarmEvent e;
  TEST_ASSERT_TRUE(m.popEvent(e));
  TEST_ASSERT_EQUAL(AlarmId::SENSOR_FAULT, e.id);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, e.from);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, e.to);
  TEST_ASSERT_FALSE(m.popEvent(e));
  TEST_ASSERT_EQUAL_STRING("cleared_unacknowledged", name(AlarmState::CLEARED_UNACK));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_non_latching_ack_path);
  RUN_TEST(test_pending_on_delay);
  RUN_TEST(test_latching_path);
  RUN_TEST(test_latched_reoccurs);
  RUN_TEST(test_cleared_unack_reactivates);
  RUN_TEST(test_reset_then_ack);
  RUN_TEST(test_service_suppression);
  RUN_TEST(test_summary_counts);
  RUN_TEST(test_watchdog_pulse);
  RUN_TEST(test_persistence);
  RUN_TEST(test_events);
  return UNITY_END();
}
