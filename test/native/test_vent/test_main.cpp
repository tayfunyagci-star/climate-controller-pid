// Havalandırma ve koordinasyon — CONTROL_ARCHITECTURE §5.1–§5.3 (öncelik tablosunun 9 satırı).
#include <unity.h>
#include "cc_interlock.h"
#include "cc_vent.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static VentInput base(float t1 = 21, float rh = 50) {
  VentInput v;
  v.dt_ms = 1000;
  v.t1 = t1;
  v.t1_q = Quality::GOOD;
  v.rh = rh;
  v.rh_q = Quality::GOOD;
  v.sp_effective = 21;
  return v;
}

static VentOutput run(Ventilation& v, const Config& c, VentInput in, int s) {
  VentOutput o;
  for (int i = 0; i < s; ++i) o = v.step(c, in);
  return o;
}

// §5.1 TEMP_HIGH histerezisi 26/24
void test_temp_high_source() {
  Config c;
  Ventilation v;
  VentOutput o = run(v, c, base(26.5f), 1);
  TEST_ASSERT_TRUE(o.vf_request);
  TEST_ASSERT_TRUE(o.sources & VS_TEMP_HIGH);
  o = run(v, c, base(24.5f), 1);
  TEST_ASSERT_TRUE(o.vf_request);  // bant içinde sürer
  o = run(v, c, base(23.9f), 1);
  TEST_ASSERT_FALSE(o.vf_request);
}

// §5.1 HUMIDITY_HIGH 75 / 70
void test_humidity_source() {
  Config c;
  Ventilation v;
  VentOutput o = run(v, c, base(21, 76), 1);
  TEST_ASSERT_TRUE(o.sources & VS_HUMIDITY_HIGH);
  o = run(v, c, base(21, 71), 1);
  TEST_ASSERT_TRUE(o.vf_request);
  o = run(v, c, base(21, 69), 1);
  TEST_ASSERT_FALSE(o.vf_request);
  c.humidity_vent_enabled = false;
  o = run(v, c, base(21, 90), 1);
  TEST_ASSERT_FALSE(o.vf_request);
}

// §5.1 SCHEDULED: saatte N dk
void test_periodic_source() {
  Config c;
  c.ventilation_periodic_min = 10;
  Ventilation v;
  VentInput in = base();
  in.uptime_s = 300;
  TEST_ASSERT_TRUE(run(v, c, in, 1).sources & VS_SCHEDULED);
  in.uptime_s = 700;
  TEST_ASSERT_FALSE(run(v, c, in, 1).vf_request);
}

// Satır 1: OVERTEMP → VF zorunlu (interlock I-4)
void test_row1_overtemp_forced() {
  InterlockEngine il;
  InterlockInput ii;
  ii.arm = true;
  ii.vf_force_on = true;
  ii.vf_req = false;
  InterlockOutput o = il.step(ii);
  TEST_ASSERT_TRUE(o.eff[VF]);
  Ventilation v;
  Config c;
  VentInput in = base();
  in.overtemp = true;
  VentOutput vo = run(v, c, in, 1);
  TEST_ASSERT_EQUAL(VentState::FORCED, Ventilation::reportState(vo, true, Reason::OVERTEMPERATURE));
}

// Satır 2: SENSOR/CONFIG/INTERNAL → otomatik kurallar durur, manuel korunur
void test_row2_sensor_fault_manual_kept() {
  Config c;
  Ventilation v;
  VentInput in = base(30, 90);
  in.sensor_fault = true;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_FALSE(o.vf_request);
  v.setManual(true);
  o = run(v, c, in, 1);
  TEST_ASSERT_TRUE(o.vf_request);
  TEST_ASSERT_EQUAL(Reason::NONE, o.inhibit);
}

// Satır 3: antifreeze → havalandırma (manuel dahil) inhibit, ısıtma sınırı yok
void test_row3_antifreeze() {
  Config c;
  Ventilation v;
  v.setManual(true);
  VentInput in = base(3);
  in.antifreeze = true;
  in.heat_request = true;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_FALSE(isValid(o.heat_cap));
  InterlockEngine il;
  InterlockInput ii;
  ii.arm = true;
  ii.vf_req = o.vf_request;
  ii.antifreeze = true;
  InterlockOutput io = il.step(ii);
  TEST_ASSERT_FALSE(io.eff[VF]);
  TEST_ASSERT_EQUAL(Reason::ANTIFREEZE_INHIBIT, io.reason[VF]);
}

// Satır 4: manuel vent + ısıtma: VENT_WINS → talep ≤ vent_heat_cap; HEAT_WINS → vent inhibit
void test_row4_manual_priority() {
  Config c;
  Ventilation v;
  v.setManual(true);
  VentInput in = base(19);
  in.heat_request = true;
  in.heating_active = true;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_TRUE(o.vf_request);
  TEST_ASSERT_EQUAL(Reason::NONE, o.inhibit);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, o.heat_cap);
  c.manual_vent_priority = ManualVentPriority::HEAT_WINS;
  o = run(v, c, in, 1);
  TEST_ASSERT_EQUAL(Reason::HEATING_PRIORITY, o.inhibit);
  TEST_ASSERT_FALSE(isValid(o.heat_cap));
}

// Satır 5: nem + ısıtma: INHIBIT / ALLOW / ALLOW_ABOVE_SP
void test_row5_humidity_while_heating() {
  Config c;
  Ventilation v;
  VentInput in = base(20, 85);
  in.heating_active = true;
  in.heat_request = true;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_EQUAL(Reason::HEATING_PRIORITY, o.inhibit);
  TEST_ASSERT_TRUE(o.humidity_high);  // HUMIDITY_HIGH INFO koşulu
  c.humidity_vent_while_heating = HumVentWhileHeating::ALLOW;
  o = run(v, c, in, 1);
  TEST_ASSERT_EQUAL(Reason::NONE, o.inhibit);
  TEST_ASSERT_TRUE(o.auto_request);
  c.humidity_vent_while_heating = HumVentWhileHeating::ALLOW_ABOVE_SP;
  o = run(v, c, in, 1);  // T1 20 < SP − 0.5
  TEST_ASSERT_FALSE(o.auto_request);
  in.t1 = 20.6f;
  o = run(v, c, in, 1);
  TEST_ASSERT_TRUE(o.auto_request);
}

// Satır 6: TEMP_HIGH → ON (ısıtma yok)
void test_row6_temp_high_on() {
  Config c;
  Ventilation v;
  VentOutput o = run(v, c, base(28), 1);
  TEST_ASSERT_TRUE(o.auto_request);
}

// Satır 7: ısıtma aktif, otomatik istek yok → OFF
void test_row7_heating_no_request() {
  Config c;
  Ventilation v;
  VentInput in = base(19, 50);
  in.heating_active = true;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_FALSE(o.vf_request);
}

// Satır 8: ısıtma → vent changeover 120 s
void test_row8_heat_to_vent_changeover() {
  Config c;
  Ventilation v;
  VentInput in = base(21, 85);
  in.heating_active = true;
  in.heat_request = true;
  run(v, c, in, 10);
  in.heating_active = false;
  in.heat_request = false;
  VentOutput o = run(v, c, in, 60);
  TEST_ASSERT_EQUAL(Reason::CHANGEOVER_DELAY, o.inhibit);
  TEST_ASSERT_FALSE(o.auto_request);
  o = run(v, c, in, 61);
  TEST_ASSERT_TRUE(o.auto_request);
}

// Satır 9: vent → ısıtma changeover 60 s
void test_row9_vent_to_heat_changeover() {
  Config c;
  Ventilation v;
  VentInput in = base(21, 85);
  in.vf_effective = true;
  run(v, c, in, 10);  // otomatik havalandırma çalışıyor
  in.rh = 60;         // istek kalktı
  in.vf_effective = false;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_TRUE(o.heat_inhibit);
  o = run(v, c, in, 58);
  TEST_ASSERT_TRUE(o.heat_inhibit);
  o = run(v, c, in, 3);
  TEST_ASSERT_FALSE(o.heat_inhibit);
  // antifreeze beklemeyi kaldırır
  Ventilation v2;
  in.rh = 85; in.vf_effective = true;
  run(v2, c, in, 10);
  in.rh = 60; in.vf_effective = false; in.antifreeze = true;
  TEST_ASSERT_FALSE(run(v2, c, in, 1).heat_inhibit);
}

// §5.3: BOOST gibi yüksek SP'de havalandırma eşiği SP + margin'e çıkar
void test_setpoint_margin() {
  Config c;
  Ventilation v;
  VentInput in = base(26.5f);
  in.sp_effective = 25.0f;  // 25 + 2 = 27 > 26
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_EQUAL_FLOAT(27.0f, o.start_effective);
  TEST_ASSERT_EQUAL_FLOAT(25.0f, o.stop_effective);
  TEST_ASSERT_FALSE(o.vf_request);
}

// Mod OFF: yalnız güvenlik; manuel istek MODE_OFF ile engellenir. Manuel zaman aşımı.
void test_mode_off_and_manual_timeout() {
  Config c;
  c.manual_vent_timeout_min = 2;
  Ventilation v;
  v.setManual(true);
  VentInput in = base(30, 90);
  in.mode = OpMode::OFF;
  VentOutput o = run(v, c, in, 1);
  TEST_ASSERT_EQUAL(Reason::MODE_OFF, o.inhibit);
  TEST_ASSERT_FALSE(o.auto_request);
  in.mode = OpMode::VENT_ONLY;
  in.t1 = 21; in.rh = 50;
  bool ev = false;
  for (int i = 0; i < 121; ++i) ev |= v.step(c, in).ev_manual_timeout;
  TEST_ASSERT_TRUE(ev);
  TEST_ASSERT_FALSE(v.manual());
}

// OTA: ota_vent_state OFF → kapalı; LAST → son durum
void test_ota_vent_state() {
  Config c;
  Ventilation v;
  run(v, c, base(28), 1);
  VentInput in = base(28);
  in.ota = true;
  TEST_ASSERT_TRUE(run(v, c, in, 1).vf_request);
  c.ota_vent_state = OtaVentState::OFF;
  TEST_ASSERT_FALSE(run(v, c, in, 1).vf_request);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_temp_high_source);
  RUN_TEST(test_humidity_source);
  RUN_TEST(test_periodic_source);
  RUN_TEST(test_row1_overtemp_forced);
  RUN_TEST(test_row2_sensor_fault_manual_kept);
  RUN_TEST(test_row3_antifreeze);
  RUN_TEST(test_row4_manual_priority);
  RUN_TEST(test_row5_humidity_while_heating);
  RUN_TEST(test_row6_temp_high_on);
  RUN_TEST(test_row7_heating_no_request);
  RUN_TEST(test_row8_heat_to_vent_changeover);
  RUN_TEST(test_row9_vent_to_heat_changeover);
  RUN_TEST(test_setpoint_margin);
  RUN_TEST(test_mode_off_and_manual_timeout);
  RUN_TEST(test_ota_vent_state);
  return UNITY_END();
}
