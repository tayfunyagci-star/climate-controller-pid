// Konfigürasyon doğrulayıcı — CONFIGURATION_MODEL §2 aralıkları, §4 V1–V17, ret ≠ kıstırma.
#include <unity.h>
#include "cc_config_validate.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static void expectRule(const Config& c, uint8_t rule, bool present) {
  ValidationResult r = validate(c);
  if (present) TEST_ASSERT_TRUE_MESSAGE(r.hasRule(rule), "kural bekleniyordu");
  else TEST_ASSERT_FALSE_MESSAGE(r.hasRule(rule), "kural beklenmiyordu");
}

void test_defaults_valid() {
  Config c;
  ValidationResult r = validate(c);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_EQUAL(0, r.nWarnings);
}

void test_relay_defaults_valid() {
  Config c;
  applyDriverDefaults(c, DriverKind::RELAY);
  TEST_ASSERT_TRUE(validate(c).ok());
}

void test_range_every_numeric_field() {
  // Her sayısal alan: min−1 ve max+1 reddedilir, min ve max (tek başına aralık) kabul edilir
  for (size_t i = 0; i < fieldCount(); ++i) {
    const FieldInfo& f = fieldAt(i);
    if (f.kind != FieldKind::FLOAT && f.kind != FieldKind::INT) continue;
    Config c;
    char* base = reinterpret_cast<char*>(&c) + f.offset;
    const float span = (f.max - f.min);
    const float d = f.kind == FieldKind::INT ? 1.0f : (f.step > 0 ? f.step : span * 0.01f + 0.001f);
    if (f.kind == FieldKind::FLOAT) *reinterpret_cast<float*>(base) = f.max + d;
    else *reinterpret_cast<int32_t*>(base) = (int32_t)(f.max + d);
    ValidationResult r = validate(c);
    TEST_ASSERT_TRUE_MESSAGE(r.hasErrorOn(f.key), f.key);
    if (f.kind == FieldKind::FLOAT) *reinterpret_cast<float*>(base) = f.min - d;
    else *reinterpret_cast<int32_t*>(base) = (int32_t)(f.min - d);
    r = validate(c);
    TEST_ASSERT_TRUE_MESSAGE(r.hasErrorOn(f.key), f.key);
  }
}

void test_V1() {
  Config c;
  c.ventilation_stop_temperature = 25.5f;  // start 26 → stop ≤ 25
  expectRule(c, 1, true);
  c.ventilation_stop_temperature = 25.0f;
  expectRule(c, 1, false);
}
void test_V2() {
  Config c;
  c.humidity_low_limit = 66;  // 75 − 5 − 5 = 65
  expectRule(c, 2, true);
  c.humidity_low_limit = 65;
  expectRule(c, 2, false);
}
void test_V3_warning_only() {
  Config c;
  c.setpoint_night = 22.0f;  // gece > gündüz
  ValidationResult r = validate(c);
  TEST_ASSERT_TRUE(r.ok());
  TEST_ASSERT_TRUE(r.hasRule(3));
  TEST_ASSERT_EQUAL(ValCode::WARN_PROFILE_ORDER, r.warnings[0].code);
}
void test_V4() {
  Config c;
  c.frost_guard_temperature = 5.0f;  // setpoint_frost 5
  expectRule(c, 4, true);
  c.frost_guard_temperature = 4.5f;
  expectRule(c, 4, false);
}
void test_V5() {
  Config c;
  c.setpoint_boost = 30.5f - 0.5f;  // 30 → limit ≥ 40
  c.cabin_overtemp_limit = 39.5f;
  expectRule(c, 5, true);
  c.cabin_overtemp_limit = 40.0f;
  expectRule(c, 5, false);
}
void test_V6() {
  Config c;
  c.cabin_overtemp_limit = 40;
  c.ventilation_start_temperature = 36.0f;  // 36 + 5 = 41 > 40
  c.ventilation_stop_temperature = 30.0f;
  expectRule(c, 6, true);
  c.ventilation_start_temperature = 35.0f;
  expectRule(c, 6, false);
}
void test_V7() {
  Config c;
  c.stage2_off = 51;  // 55 − 5 = 50
  expectRule(c, 7, true);
  c.stage2_off = 50;
  expectRule(c, 7, false);
}
void test_V8_relay_short_window_rejected() {
  Config c;
  applyDriverDefaults(c, DriverKind::RELAY);
  c.tp_window_s = 299;
  expectRule(c, 8, true);
  c.tp_window_s = 600;
  c.heater_min_on_s = 59;
  expectRule(c, 8, true);
  c.heater_min_on_s = 60;
  expectRule(c, 8, false);
}
void test_V8_ssr_window() {
  Config c;
  c.tp_window_s = 4;  // aralık dışı da (5–1800) — V8 SSR 5–120
  expectRule(c, 8, true);
  c.tp_window_s = 121;
  expectRule(c, 8, true);
  c.tp_window_s = 120;
  expectRule(c, 8, false);
}
void test_V9() {
  Config c;
  c.tp_window_s = 20;
  c.heater_min_on_s = 15;
  c.heater_min_off_s = 10;
  expectRule(c, 9, true);
  c.heater_min_off_s = 5;
  expectRule(c, 9, false);
}
void test_V10() {
  Config c;
  c.post_cool_min_s = 90;  // > post_cool_seconds 60
  expectRule(c, 10, true);
  c.post_cool_min_s = 60;
  expectRule(c, 10, false);
}
void test_V11() {
  Config c;
  c.post_cool_mode = PostCoolMode::HYBRID;
  expectRule(c, 11, true);
  c.t2_enabled = true;
  expectRule(c, 11, false);
}
void test_V12() {
  Config c;
  c.pid_mode = PidMode::PI;
  c.pid_ki = 0;
  expectRule(c, 12, true);
  Config d;
  d.pid_mode = PidMode::PID;
  d.pid_kd = 0;
  ValidationResult r = validate(d);
  TEST_ASSERT_TRUE(r.ok());  // yalnız uyarı
  TEST_ASSERT_TRUE(r.hasRule(12));
}
void test_V13() {
  Config c;
  c.pid_kp = 1.0f;
  c.pid_ki = 1.5f;  // Ti < 1 dk
  expectRule(c, 13, true);
  c.pid_ki = 1.0f;
  expectRule(c, 13, false);
}
void test_V14() {
  Config c;
  c.sensor_interval_s = 3;
  c.control_interval_s = 2;
  c.sensor_stale_s = 10;
  expectRule(c, 14, true);
  c.control_interval_s = 3;
  expectRule(c, 14, false);
}
void test_V15() {
  Config c;
  c.sensor_interval_s = 4;
  c.control_interval_s = 4;
  c.sensor_stale_s = 11;
  expectRule(c, 15, true);
  c.sensor_stale_s = 12;
  expectRule(c, 15, false);
}
void test_V16() {
  Config c;
  c.state_idle_s = 60;
  expectRule(c, 16, true);
  c.state_idle_s = 59;
  expectRule(c, 16, false);
}

// V17 + D-13: tek komut ilişki ihlali → REJECTED_RELATION, mevcut değişmez, kıstırma yok
void test_V17_single_command_relation_rejected() {
  Config cur;
  cur.remote_config_enabled = true;
  Config out;
  SetResult r = setField(cur, "ventilation_stop_temperature", "26.0", CmdSource::MQTT, out);  // A10
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_RELATION, r.result);
  TEST_ASSERT_EQUAL(1, r.rule);
  TEST_ASSERT_EQUAL_FLOAT(24.0f, cur.ventilation_stop_temperature);
  // Doğru sırayla iki komut
  r = setField(cur, "ventilation_start_temperature", "30.0", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.result);
  Config c2 = out;
  r = setField(c2, "ventilation_stop_temperature", "28.0", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.result);
  TEST_ASSERT_EQUAL_FLOAT(28.0f, out.ventilation_stop_temperature);
}

void test_out_of_range_rejected_not_clamped() {
  Config cur, out;
  out.temperature_setpoint = 99;
  SetResult r = setField(cur, "temperature_setpoint", "35", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_INVALID, r.result);
  TEST_ASSERT_EQUAL(ValCode::INVALID_RANGE, r.code);
  TEST_ASSERT_EQUAL_FLOAT(21.0f, cur.temperature_setpoint);
  TEST_ASSERT_EQUAL_FLOAT(99.0f, out.temperature_setpoint);  // out dokunulmadı
  r = setField(cur, "temperature_setpoint", "22.3", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(ValCode::INVALID_STEP, r.code);
  r = setField(cur, "temperature_setpoint", "22,5", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(ValCode::INVALID_FORMAT, r.code);
  r = setField(cur, "temperature_setpoint", "22.5", CmdSource::MQTT, out);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.result);
  TEST_ASSERT_EQUAL_FLOAT(22.5f, out.temperature_setpoint);
}

void test_parse_strict() {
  float v;
  TEST_ASSERT_TRUE(parseNumberStrict("22.5", v));
  TEST_ASSERT_EQUAL_FLOAT(22.5f, v);
  TEST_ASSERT_TRUE(parseNumberStrict("-3", v));
  TEST_ASSERT_FALSE(parseNumberStrict("22,5", v));
  TEST_ASSERT_FALSE(parseNumberStrict(" 22", v));
  TEST_ASSERT_FALSE(parseNumberStrict("2e1", v));
  TEST_ASSERT_FALSE(parseNumberStrict("22.", v));
  TEST_ASSERT_FALSE(parseNumberStrict("", v));
  TEST_ASSERT_FALSE(parseNumberStrict("nan", v));
}

void test_remote_policy() {
  Config cur, out;
  // remote_config_enabled kapalı: konfigürasyon reddedilir, operasyonel kabul
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "setpoint_night", "17.0", CmdSource::MQTT, out).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, setField(cur, "setpoint_night", "17.0", CmdSource::LOCAL_WEB, out).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, setField(cur, "operating_mode", "OFF", CmdSource::MQTT, out).result);
  // Safety limitleri uzaktan hiçbir koşulda yazılamaz
  cur.remote_config_enabled = true;
  cur.pid_remote_tuning = true;
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "cabin_overtemp_limit", "45", CmdSource::MQTT, out).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "max_continuous_heating_min", "300", CmdSource::MQTT, out).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "sensor_stale_s", "20", CmdSource::MQTT, out).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "antifreeze_enabled", "OFF", CmdSource::MQTT, out).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "remote_config_enabled", "OFF", CmdSource::MQTT, out).result);
  // PID uzaktan: remote_config + pid_remote_tuning
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, setField(cur, "pid_kp", "25", CmdSource::MQTT, out).result);
  cur.pid_remote_tuning = false;
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, setField(cur, "pid_kp", "25", CmdSource::MQTT, out).result);
  // Bilinmeyen alan
  TEST_ASSERT_EQUAL(ValCode::UNKNOWN_FIELD, setField(cur, "climate", "ON", CmdSource::LOCAL_WEB, out).code);
}

void test_enum_and_bool_payloads() {
  Config cur, out;
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_INVALID, setField(cur, "operating_mode", "auto", CmdSource::LOCAL_WEB, out).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, setField(cur, "humidity_vent_enabled", "OFF", CmdSource::LOCAL_WEB, out).result);
  TEST_ASSERT_FALSE(out.humidity_vent_enabled);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_INVALID, setField(cur, "humidity_vent_enabled", "true", CmdSource::LOCAL_WEB, out).result);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_valid);
  RUN_TEST(test_relay_defaults_valid);
  RUN_TEST(test_range_every_numeric_field);
  RUN_TEST(test_V1);
  RUN_TEST(test_V2);
  RUN_TEST(test_V3_warning_only);
  RUN_TEST(test_V4);
  RUN_TEST(test_V5);
  RUN_TEST(test_V6);
  RUN_TEST(test_V7);
  RUN_TEST(test_V8_relay_short_window_rejected);
  RUN_TEST(test_V8_ssr_window);
  RUN_TEST(test_V9);
  RUN_TEST(test_V10);
  RUN_TEST(test_V11);
  RUN_TEST(test_V12);
  RUN_TEST(test_V13);
  RUN_TEST(test_V14);
  RUN_TEST(test_V15);
  RUN_TEST(test_V16);
  RUN_TEST(test_V17_single_command_relation_rejected);
  RUN_TEST(test_out_of_range_rejected_not_clamped);
  RUN_TEST(test_parse_strict);
  RUN_TEST(test_remote_policy);
  RUN_TEST(test_enum_and_bool_payloads);
  return UNITY_END();
}
