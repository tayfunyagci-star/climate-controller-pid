// ClimateCore uçtan uca senaryolar — REQUIREMENTS §7 (A1–A12, donanımsız olanlar), komut arbitrasyonu,
// servis modu ve bütün çekirdek üzerinde rastgele özellik testi.
#include <unity.h>
#include "support/sim.h"

using namespace cc;
using sim::Runner;

void setUp() {}
void tearDown() {}

static Config cfg() {
  Config c;
  c.temperature_setpoint = 22.0f;
  c.setpoint_ramp_c_per_min = 0;
  return c;
}

// A1: AUTO, T1=18, SP=22 → HF ON → 3 s sonra R1 → talep > 55 % ise R2; HEATING
void test_A1_heating_start_sequence() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
  float tHF = r.runUntil([&] { return r.out(HF); }, 30);
  TEST_ASSERT_TRUE(tHF >= 0);
  TEST_ASSERT_FALSE(r.out(R1));
  TEST_ASSERT_EQUAL(CtrlState::HEATING, r.s().controller_state);
  TEST_ASSERT_EQUAL(HeatPhase::PREPURGE, r.s().heating_phase);
  float tR1 = r.runUntil([&] { return r.out(R1); }, 30);
  TEST_ASSERT_TRUE(tR1 >= 3.0f - 0.1f);
  TEST_ASSERT_EQUAL(HeatPhase::ACTIVE, r.s().heating_phase);
  TEST_ASSERT_EQUAL(HeatingReason::PID, r.s().heating_reason);
  float tR2 = r.runUntil([&] { return r.out(R2); }, 900);
  TEST_ASSERT_TRUE(tR2 > 0);
  TEST_ASSERT_EQUAL_UINT8(2, r.s().power_stage);
  TEST_ASSERT_TRUE(r.core.events().contains(EvCode::STAGE2_ON));
  TEST_ASSERT_TRUE(r.core.events().contains(EvCode::HEATING_START));
  TEST_ASSERT_TRUE(r.inv.ok());
}

// A2: SP'ye ulaşılır → talep düşer, R'ler kapanır, POST_COOL 60 s, HF OFF, IDLE; aşım ≤ 0.5 °C (FR-01)
void test_A2_reach_setpoint_postcool_idle() {
  Runner r;
  r.cabin.T = 18;
  r.cabin.T_out = 21.5f;  // kararlı hâl gücü < min talep: SP'de ısıtma durur
  r.boot(cfg());
  float maxT = 0;
  float t = r.runUntil([&] {
    if (r.cabin.T > maxT) maxT = r.cabin.T;
    return r.s().heating_phase == HeatPhase::POST_COOL;
  }, 4 * 3600);
  TEST_ASSERT_TRUE_MESSAGE(t > 0, "post-cool'a girilmedi");
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_TRUE(r.out(HF));
  TEST_ASSERT_EQUAL(CtrlState::POST_COOL, r.s().controller_state);
  TEST_ASSERT_EQUAL(Reason::POST_COOL, r.s().reason[HF]);
  float tIdle = r.runUntil([&] { return !r.out(HF); }, 120);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, tIdle);
  TEST_ASSERT_EQUAL(CtrlState::IDLE, r.s().controller_state);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(22.5f, maxT);
  TEST_ASSERT_TRUE(r.inv.ok());
}

// A3: R1 aktifken HF OFF isteği → requested OFF, effective ON, HEATER_INTERLOCK; OVERRIDDEN
void test_A3_hf_off_overridden() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  r.core.command("heater_fan_manual", "ON", CmdSource::MQTT);
  r.runUntil([&] { return r.out(R1); }, 60);
  CmdReply rep = r.core.command("heater_fan_manual", "OFF", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::OVERRIDDEN, rep.result);
  TEST_ASSERT_EQUAL(Reason::HEATER_INTERLOCK, rep.reason);
  r.run(1);
  TEST_ASSERT_FALSE(r.s().heater_fan_manual);
  TEST_ASSERT_TRUE(r.s().active[HF]);
  TEST_ASSERT_EQUAL(Reason::HEATER_INTERLOCK, r.s().reason[HF]);
}

// A4 + SR-05: sensör kablosu çekilir → ≤ sensor_stale_s içinde R OFF, POST_COOL, FAILSAFE/SENSOR_FAULT, CRITICAL
void test_A4_sensor_unplugged() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  r.runUntil([&] { return r.out(R1); }, 60);
  r.sensor_connected = false;
  float t = r.runUntil([&] { return r.core.safety().lockout; }, 30);
  TEST_ASSERT_TRUE(t >= 0);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(10.0f, t);  // ≤ sensor_stale_s
  r.run(0.2f);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  r.run(1);
  TEST_ASSERT_EQUAL(CtrlState::FAILSAFE, r.s().controller_state);
  TEST_ASSERT_EQUAL(FailsafeReason::SENSOR_FAULT, r.s().failsafe_reason);
  TEST_ASSERT_TRUE(r.out(HF));  // post-cool
  TEST_ASSERT_EQUAL(HeatPhase::LOCKOUT, r.s().heating_phase);
  TEST_ASSERT_TRUE(std::isnan(r.s().temperature));  // geçersiz ölçüm null
  TEST_ASSERT_TRUE(r.s().alarm);
  TEST_ASSERT_EQUAL(Severity::CRITICAL, r.s().alarm_state);
  r.run(70);
  TEST_ASSERT_FALSE(r.out(HF));
  TEST_ASSERT_TRUE(r.inv.ok());
  // Sensör geri gelir → otomatik dönüş (kilitsiz)
  r.sensor_connected = true;
  float back = r.runUntil([&] { return r.core.sysState() == SysState::RUN; }, 900);
  TEST_ASSERT_TRUE(back > 29);
}

// A5 + SR-06: T1 > limit → R OFF, VF ON (OVERTEMPERATURE), kilit; soğuyunca cleared_unack, reset gerekir
void test_A5_overtemperature_latch() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  r.runUntil([&] { return r.out(R1); }, 60);
  r.t1_override = 41;
  float t = r.runUntil([&] { return r.core.safety().lockout; }, 30);
  TEST_ASSERT_TRUE(t >= 9.5f && t <= 12.0f);  // S1: 10 s gecikme (+ medyan/örnekleme)
  r.run(0.2f);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_TRUE(r.out(VF));
  TEST_ASSERT_EQUAL(Reason::AUTO_DEMAND, r.s().reason[VF]);  // TEMP_HIGH da istiyor
  r.core.command("ventilation_fan_manual", "OFF", CmdSource::LOCAL_WEB);
  TEST_ASSERT_EQUAL(FailsafeReason::OVERTEMP, r.s().failsafe_reason);
  TEST_ASSERT_TRUE(r.s().overtemperature);
  // Koşul sürerken reset reddedilir
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_STATE, r.core.alarmReset(CmdSource::LOCAL_WEB).result);
  r.t1_override = 30;
  r.run(90);
  TEST_ASSERT_EQUAL(AlarmState::CLEARED_UNACK, r.core.alarms().rec(AlarmId::OVERTEMPERATURE).st);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, r.core.sysState());  // kilit sürer
  // MQTT reset politikası: yalnız yerel (servis kanalı F5)
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.alarmReset(CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.alarmReset(CmdSource::LOCAL_WEB).result);
  r.run(1);
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
  r.core.alarmAck(CmdSource::LOCAL_WEB);
  r.run(1);
  TEST_ASSERT_EQUAL(AlarmState::NORMAL, r.core.alarms().rec(AlarmId::OVERTEMPERATURE).st);
  TEST_ASSERT_TRUE(r.inv.ok());
}

// A6/A7: MQTT/Wi-Fi kaybı kontrolü değiştirmez; yalnız alarm (FR, NFR)
void test_A6_A7_network_loss_no_effect() {
  Runner a, b;
  a.cabin.T = b.cabin.T = 18;
  a.boot(cfg());
  b.boot(cfg());
  b.core.setNetStatus(true, false, true, false);
  for (int i = 0; i < 20 * 60 * 20; ++i) {
    a.step();
    b.step();
    for (uint8_t k = 0; k < OUT_COUNT; ++k) TEST_ASSERT_EQUAL(a.out(k), b.out(k));
  }
  TEST_ASSERT_EQUAL(a.s().controller_state, b.s().controller_state);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, a.s().heat_demand, b.s().heat_demand);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, b.core.alarms().rec(AlarmId::MQTT_OFFLINE).st);
  TEST_ASSERT_EQUAL(AlarmState::ACTIVE_UNACK, b.core.alarms().rec(AlarmId::WIFI_OFFLINE).st);
  TEST_ASSERT_EQUAL(Severity::WARNING, b.s().alarm_state);
}

// A8 (çekirdek kısmı): OTA → R OFF, post-cool tamamlanır, sonra OTA; ARM=0
void test_A8_ota_prepare() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  r.runUntil([&] { return r.out(R1); }, 60);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.otaBegin(CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.otaBegin(CmdSource::LOCAL_WEB).result);
  r.run(0.2f);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_FALSE(r.core.heaterArm());
  TEST_ASSERT_EQUAL(CtrlState::OTA, r.s().controller_state);
  TEST_ASSERT_TRUE(r.out(HF));
  float t = r.runUntil([&] { return r.core.otaReady(); }, 120);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, t);
  r.run(1);
  TEST_ASSERT_FALSE(r.out(HF));
  TEST_ASSERT_TRUE(r.inv.ok());
  // Donma riski: T1 < frost_guard + 2 → reddedilir
  Runner f;
  f.cabin.T = 5;
  f.boot(cfg());
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_STATE, f.core.otaBegin(CmdSource::LOCAL_WEB).result);
}

// A9: Programs sched_night ON → NIGHT, 18 °C; OFF → DAY
void test_A9_programs_night() {
  Runner r;
  r.cabin.T = 20;
  Config c = cfg();
  c.temperature_setpoint = 21;
  r.boot(c);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.command("sched_night", "ON", CmdSource::MQTT).result);
  r.run(3);
  TEST_ASSERT_EQUAL(ProfileActive::NIGHT, r.s().profile_active);
  TEST_ASSERT_EQUAL_FLOAT(18.0f, r.s().setpoint_effective);
  r.core.command("sched_night", "OFF", CmdSource::MQTT);
  r.run(3);
  TEST_ASSERT_EQUAL(ProfileActive::DAY, r.s().profile_active);
}

// A10: stop ≥ start → REJECTED (ilişki), state değişmez; remote config kapalıyken POLICY
void test_A10_relation_rejected() {
  Runner r;
  r.boot(cfg());
  CmdReply rep = r.core.command("ventilation_stop_temperature", "26.0", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, rep.result);
  Config c = cfg();
  c.remote_config_enabled = true;
  Runner q;
  q.boot(c);
  rep = q.core.command("ventilation_stop_temperature", "26.0", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_RELATION, rep.result);
  TEST_ASSERT_EQUAL(ValCode::INVALID_RELATION, rep.code);
  TEST_ASSERT_EQUAL_FLOAT(24.0f, q.core.config().ventilation_stop_temperature);
  // Aralık dışı setpoint kıstırılmaz
  rep = q.core.command("temperature_setpoint", "35", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_INVALID, rep.result);
  TEST_ASSERT_EQUAL_FLOAT(22.0f, q.core.config().temperature_setpoint);
  // Safety limiti uzaktan yazılamaz
  rep = q.core.command("cabin_overtemp_limit", "50", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, rep.result);
}

// A11 + SR-08: kontrol görevi dondurulur → heartbeat sınırı (6 s) sonrası ≤ 1 s içinde R OFF, INTERNAL_FAULT
void test_A11_control_task_stall() {
  Runner r;
  r.cabin.T = 18;
  r.boot(cfg());
  r.runUntil([&] { return r.out(R1); }, 60);
  r.run(20);
  r.core.testFreezeControl(true);
  float t = r.runUntil([&] { return r.core.safety().lockout; }, 30);
  TEST_ASSERT_TRUE(t >= 6.0f);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(7.0f, t);  // 6 s heartbeat sınırı + ≤ 1 s
  r.run(0.2f);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_EQUAL(FailsafeReason::INTERNAL_FAULT, r.s().failsafe_reason);
  TEST_ASSERT_TRUE(r.out(HF));  // OutputTask canlı: post-cool
  // reset ile kalkmaz
  r.core.testFreezeControl(false);
  r.core.alarmReset(CmdSource::LOCAL_WEB);
  r.run(5);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, r.core.sysState());
  TEST_ASSERT_TRUE(r.inv.ok());
  // OutputTask dondurulursa ARM düşer (≤ 1 s + safety periyodu)
  Runner o;
  o.cabin.T = 18;
  o.boot(cfg());
  o.runUntil([&] { return o.out(R1); }, 60);
  o.core.testFreezeOutput(true);
  float ta = o.runUntil([&] { return !o.core.heaterArm() || o.core.safety().lockout; }, 5);
  TEST_ASSERT_TRUE(ta >= 0 && ta <= 1.3f);
  TEST_ASSERT_FALSE(o.core.safety().arm_allowed);
}

// A12: güç kesilip gelir → boot'ta çıkışlar OFF, ayarlar korunur; POWER_ON'da boot post-cool yok
void test_A12_power_cycle() {
  Config c = cfg();
  c.temperature_setpoint = 19.5f;
  Runner r;
  r.begin(c);
  for (uint8_t k = 0; k < OUT_COUNT; ++k) TEST_ASSERT_FALSE(r.out(k));
  TEST_ASSERT_EQUAL(SysState::SELF_TEST, r.core.sysState());
  TEST_ASSERT_FALSE(r.core.heaterArm());
  r.run(0.5f);
  for (uint8_t k = 0; k < OUT_COUNT; ++k) TEST_ASSERT_FALSE(r.out(k));
  r.run(3);
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
  TEST_ASSERT_EQUAL_FLOAT(19.5f, r.core.config().temperature_setpoint);
  TEST_ASSERT_EQUAL(OpMode::AUTO, r.core.config().operating_mode);
  TEST_ASSERT_FALSE(r.core.events().contains(EvCode::BOOT_POST_COOL));
}

// D-20: WDT/PANIC resetinde heater_was_on → Heater Fan post-cool (BOOT_POST_COOL)
void test_boot_post_cool_after_fault_reset() {
  BootInfo b;
  b.fault_reset = true;
  b.heater_was_on = true;
  Runner r;
  r.cabin.T = 25;
  r.begin(cfg(), b);
  r.run(0.2f);
  TEST_ASSERT_TRUE(r.out(HF));
  TEST_ASSERT_FALSE(r.out(R1));
  TEST_ASSERT_EQUAL(Reason::BOOT_POST_COOL, r.s().reason[HF]);
  float t = r.runUntil([&] { return !r.out(HF); }, 120);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, t);
  TEST_ASSERT_TRUE(r.core.alarms().rec(AlarmId::WATCHDOG_RESET).st != AlarmState::NORMAL);
}

// S14: restart fırtınası → RECOVERY, ısıtma kilitli, yerel onay
void test_restart_storm_recovery() {
  BootInfo b;
  b.fault_boots_in_window = 5;
  Runner r;
  r.cabin.T = 15;
  r.begin(cfg(), b);
  r.run(20);
  TEST_ASSERT_EQUAL(CtrlState::RECOVERY, r.s().controller_state);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.recoveryAck(CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.recoveryAck(CmdSource::LOCAL_WEB).result);
  r.run(5);
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
}

// S13: geçersiz config → güvenli varsayılan + CONFIG_ERROR + ısıtma kilitli
void test_invalid_config_boot() {
  Config c = cfg();
  c.ventilation_stop_temperature = 30;  // V1 ihlali
  Runner r;
  r.cabin.T = 15;
  r.begin(c);
  r.run(20);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, r.core.sysState());
  TEST_ASSERT_EQUAL(FailsafeReason::CONFIG_ERROR, r.s().failsafe_reason);
  TEST_ASSERT_FALSE(r.out(R1));
  TEST_ASSERT_EQUAL_FLOAT(24.0f, r.core.config().ventilation_stop_temperature);  // varsayılan
  // Geçerli config kaydedilince otomatik dönüş
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.applyConfig(cfg(), CmdSource::LOCAL_WEB).result);
  r.run(2);
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
}

// §9-2 çekirdek: MANUAL 40 % → AUTO, talep sıçraması ≤ 2 %; MANUAL zaman aşımı → AUTO
void test_manual_mode_and_bumpless() {
  Config c = cfg();
  c.manual_heat_demand = 40;
  c.operating_mode = OpMode::MANUAL;
  c.temperature_setpoint = 21;
  Runner r;
  r.cabin.T = 20.5f;
  r.boot(c);
  r.run(600);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, r.s().heat_demand);
  TEST_ASSERT_EQUAL(HeatingReason::MANUAL, r.s().heating_reason);
  const float before = r.s().heat_demand;
  r.core.command("operating_mode", "AUTO", CmdSource::LOCAL_WEB);
  r.run(2.1f);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, before, r.s().heat_demand);
  // Zaman aşımı
  Config m = c;
  m.manual_timeout_h = 1;
  Runner q;
  q.cabin.T = 20.5f;
  q.boot(m);
  q.run(3600 + 5);
  TEST_ASSERT_EQUAL(OpMode::AUTO, q.core.config().operating_mode);
  TEST_ASSERT_TRUE(q.core.events().contains(EvCode::MANUAL_TIMEOUT));
  // Uzaktan MANUAL izni kapalıysa reddedilir; FAILSAFE'te REJECTED_STATE
  Config n = cfg();
  n.remote_manual_allowed = false;
  Runner p;
  p.boot(n);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, p.core.command("operating_mode", "MANUAL", CmdSource::MQTT).result);
  p.sensor_connected = false;
  p.run(15);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_STATE, p.core.command("operating_mode", "MANUAL", CmdSource::LOCAL_WEB).result);
}

// §9-7: safety kilidi 10 dk sonra açılır → integratör 0'dan, slew ≤ 10 %/dk
void test_fault_recovery_integrator_and_slew() {
  Runner r;
  r.cabin.T = 17;
  r.boot(cfg());
  r.run(900);
  r.t1_override = 41;
  r.run(15);
  r.t1_override = 17;
  r.run(600);
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, r.core.sysState());
  r.core.alarmReset(CmdSource::LOCAL_WEB);
  r.run(0.1f);
  TEST_ASSERT_EQUAL(SysState::RUN, r.core.sysState());
  r.run(2.1f);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(0.5f, r.core.pid().i);  // I = 0'dan
  float prev = r.s().heat_demand, maxRate = 0;
  for (int i = 0; i < 300; ++i) {
    r.run(2);
    const float d = r.s().heat_demand;
    const float rate = (d - prev) / (2.0f / 60.0f);
    if (rate > maxRate) maxRate = rate;
    prev = d;
  }
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(10.0f + 0.01f, maxRate);
}

// Antifreeze: OFF modunda T1 < 4 °C → FROST setpoint'iyle ısıtma; havalandırma inhibit
void test_antifreeze_in_off_mode() {
  Config c = cfg();
  c.operating_mode = OpMode::OFF;
  Runner r;
  r.cabin.T = 3.5f;
  r.cabin.T_out = -10;
  r.boot(c);
  r.core.setVentFanManual(true, CmdSource::LOCAL_WEB);
  float t = r.runUntil([&] { return r.out(R1); }, 120);
  TEST_ASSERT_TRUE(t > 0);
  TEST_ASSERT_EQUAL(HeatingReason::ANTIFREEZE, r.s().heating_reason);
  TEST_ASSERT_EQUAL(SetpointSource::ANTIFREEZE, r.s().setpoint_source);
  TEST_ASSERT_EQUAL_FLOAT(5.0f, r.s().setpoint_effective);
  TEST_ASSERT_FALSE(r.out(VF));
  TEST_ASSERT_EQUAL(Reason::ANTIFREEZE_INHIBIT, r.s().reason[VF]);
  TEST_ASSERT_EQUAL(CtrlState::HEATING, r.s().controller_state);
  // controller_enable=OFF: MQTT reddedilir, yerel kabul → antifreeze dahil otomatik kontrol durur
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.command("controller_enable", "OFF", CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.setControllerEnable(false, CmdSource::LOCAL_WEB).result);
  r.run(10);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_TRUE(r.inv.ok());
}

// Ölçümsüz ısıtma yok: donma riski + sensör arızası → FROST_RISK_NO_SENSOR, R OFF
void test_frost_risk_no_sensor() {
  Config c = cfg();
  c.operating_mode = OpMode::OFF;
  Runner r;
  r.cabin.T = 3.5f;
  r.boot(c);
  r.run(30);
  r.sensor_connected = false;
  r.run(30);
  TEST_ASSERT_FALSE(r.out(R1) || r.out(R2));
  TEST_ASSERT_NOT_EQUAL(AlarmState::NORMAL, r.core.alarms().rec(AlarmId::FROST_RISK_NO_SENSOR).st);
}

// Yerel kilit: MQTT operasyonel komutları REJECTED_POLICY; sched_* kaydedilir (OVERRIDDEN, LOCAL_LOCK)
void test_local_lock() {
  Runner r;
  r.boot(cfg());
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.setLocalLock(60, CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.setLocalLock(15, CmdSource::LOCAL_WEB).result);
  CmdReply rep = r.core.command("temperature_setpoint", "20.0", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, rep.result);
  TEST_ASSERT_EQUAL(Reason::LOCAL_LOCK, rep.reason);
  rep = r.core.command("sched_away", "ON", CmdSource::MQTT);
  TEST_ASSERT_EQUAL(CmdResult::OVERRIDDEN, rep.result);
  r.run(3);
  TEST_ASSERT_TRUE(r.s().sched_away);
  TEST_ASSERT_EQUAL(ProfileActive::DAY, r.s().profile_active);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.command("temperature_setpoint", "20.0", CmdSource::LOCAL_WEB).result);
  r.run(15 * 60 + 3);
  TEST_ASSERT_FALSE(r.s().local_lock);
  TEST_ASSERT_EQUAL(ProfileActive::AWAY, r.s().profile_active);
}

// Servis modu: yalnız yerel servis, ısıtma boştayken; tek rezistans; interlock ve safety altında; süre sınırı
void test_service_mode() {
  Runner r;
  r.cabin.T = 25;
  r.boot(cfg());
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.serviceEnter(CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r.core.serviceEnter(CmdSource::LOCAL_WEB).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_STATE, r.core.serviceTest(R1, true, CmdSource::LOCAL_SERVICE).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.serviceEnter(CmdSource::LOCAL_SERVICE).result);
  r.run(1);
  TEST_ASSERT_EQUAL(CtrlState::SERVICE, r.s().controller_state);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.serviceTest(R1, true, CmdSource::LOCAL_SERVICE).result);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_STATE, r.core.serviceTest(R2, true, CmdSource::LOCAL_SERVICE).result);
  r.run(0.2f);
  TEST_ASSERT_TRUE(r.out(HF));
  TEST_ASSERT_FALSE(r.out(R1));  // prestart
  r.run(3.5f);
  TEST_ASSERT_TRUE(r.out(R1));
  TEST_ASSERT_EQUAL(Reason::SERVICE_TEST, r.s().reason[R1]);
  // §5.3: servis testi R1=ON, sensör arızası → R1 OFF, SENSOR_FAULT
  r.sensor_connected = false;
  r.run(12);
  TEST_ASSERT_FALSE(r.out(R1));
  TEST_ASSERT_EQUAL(SysState::FAILSAFE, r.core.sysState());
  // Süre sınırı (service_test_max_s)
  Runner q;
  q.cabin.T = 25;
  q.boot(cfg());
  q.core.serviceEnter(CmdSource::LOCAL_SERVICE);
  q.core.serviceTest(VF, true, CmdSource::LOCAL_SERVICE);
  q.run(61);
  TEST_ASSERT_TRUE(q.out(VF));
  q.run(60);
  TEST_ASSERT_FALSE(q.out(VF));
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, q.core.serviceExit(CmdSource::LOCAL_SERVICE).result);
  q.run(1);
  TEST_ASSERT_EQUAL(SysState::RUN, q.core.sysState());
  TEST_ASSERT_TRUE(q.inv.ok());
}

// Havalandırma: nem yüksek, ısıtma yok → otomatik; manuel OFF ama etkin ON → AUTO_DEMAND
void test_vent_auto_humidity() {
  Runner r;
  r.cabin.T = 23;
  r.cabin.T_out = 23;
  r.cabin.rh = 85;
  r.boot(cfg());
  float t = r.runUntil([&] { return r.out(VF); }, 200);
  TEST_ASSERT_TRUE(t >= 0);
  TEST_ASSERT_EQUAL(VentState::AUTO, r.s().ventilation_state);
  TEST_ASSERT_EQUAL(Reason::AUTO_DEMAND, r.s().reason[VF]);
  TEST_ASSERT_EQUAL(CtrlState::VENTILATING, r.s().controller_state);
}

// Bütün çekirdek üzerinde rastgele özellik testi: komutlar × arızalar × zaman
void test_property_core_random() {
  static const char* const modes[] = {"OFF", "AUTO", "MANUAL", "VENT_ONLY"};
  const CmdSource srcs[] = {CmdSource::MQTT, CmdSource::LOCAL_WEB, CmdSource::LOCAL_SERVICE};
  uint32_t rOnTicks = 0, ticks = 0;
  for (uint64_t seed = 1; seed <= 24; ++seed) {
    sim::Rng g(seed);
    Config c = cfg();
    if (g.chance(300)) applyDriverDefaults(c, DriverKind::RELAY);
    c.fan_prestart_s = (int32_t)g.below(8);
    c.post_cool_seconds = 30 + (int32_t)g.below(120);
    Runner r;
    r.rng = sim::Rng(seed + 1000);
    r.noise = 0.05f;
    r.cabin.T = g.uniform(2, 24);
    r.cabin.T_out = g.uniform(-10, 15);
    r.cabin.rh = g.uniform(40, 90);
    BootInfo b;
    b.fault_reset = g.chance(200);
    b.heater_was_on = g.chance(500);
    r.boot(c, b);
    for (int k = 0; k < 3 * 3600 * 20; ++k) {  // 3 sa @ 50 ms
      if (g.chance(2)) {
        const CmdSource s = srcs[g.below(3)];
        switch (g.below(14)) {
          case 0: r.core.command("operating_mode", modes[g.below(4)], s); break;
          case 1: { char v[8]; snprintf(v, sizeof v, "%.1f", 5 + 0.5f * g.below(51)); r.core.command("temperature_setpoint", v, s); } break;
          case 2: r.core.command("heater_fan_manual", g.chance(500) ? "ON" : "OFF", s); break;
          case 3: r.core.command("ventilation_fan_manual", g.chance(500) ? "ON" : "OFF", s); break;
          case 4: r.core.command("boost", g.chance(500) ? "ON" : "OFF", s); break;
          case 5: r.core.command("sched_night", g.chance(500) ? "ON" : "OFF", s); break;
          case 6: { char v[8]; snprintf(v, sizeof v, "%d", 5 * (int)g.below(21)); r.core.command("manual_heat_demand", v, s); } break;
          case 7: r.core.alarmReset(s); break;
          case 8: r.core.alarmAck(s); break;
          case 9: r.core.serviceEnter(s); break;
          case 10: r.core.serviceTest((uint8_t)g.below(4), g.chance(700), s); break;
          case 11: r.core.serviceExit(s); break;
          case 12: if (g.chance(100)) { r.core.otaBegin(s); } else { r.core.otaAbort(); } break;
          case 13: r.core.setControllerEnable(g.chance(800), s); break;
        }
      }
      if (g.chance(1)) r.sensor_connected = !g.chance(300) ? true : false;
      if (g.chance(1)) r.t1_override = g.chance(100) ? g.uniform(30, 45) : NAN;
      r.step();
      ++ticks;
      if (r.out(R1) || r.out(R2)) ++rOnTicks;
      if (r.core.guardViolationSeen()) break;
    }
    char msg[128];
    snprintf(msg, sizeof msg, "seed %llu: rhf=%u prestart=%u postcool=%u guard=%d", (unsigned long long)seed,
             r.inv.violations_rhf, r.inv.violations_prestart, r.inv.violations_postcool, r.core.guardViolationSeen());
    TEST_ASSERT_TRUE_MESSAGE(r.inv.ok() && !r.core.guardViolationSeen(), msg);
  }
  TEST_ASSERT_GREATER_THAN_UINT32(ticks / 50, rOnTicks);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_A1_heating_start_sequence);
  RUN_TEST(test_A2_reach_setpoint_postcool_idle);
  RUN_TEST(test_A3_hf_off_overridden);
  RUN_TEST(test_A4_sensor_unplugged);
  RUN_TEST(test_A5_overtemperature_latch);
  RUN_TEST(test_A6_A7_network_loss_no_effect);
  RUN_TEST(test_A8_ota_prepare);
  RUN_TEST(test_A9_programs_night);
  RUN_TEST(test_A10_relation_rejected);
  RUN_TEST(test_A11_control_task_stall);
  RUN_TEST(test_A12_power_cycle);
  RUN_TEST(test_boot_post_cool_after_fault_reset);
  RUN_TEST(test_restart_storm_recovery);
  RUN_TEST(test_invalid_config_boot);
  RUN_TEST(test_manual_mode_and_bumpless);
  RUN_TEST(test_fault_recovery_integrator_and_slew);
  RUN_TEST(test_antifreeze_in_off_mode);
  RUN_TEST(test_frost_risk_no_sensor);
  RUN_TEST(test_local_lock);
  RUN_TEST(test_service_mode);
  RUN_TEST(test_vent_auto_humidity);
  RUN_TEST(test_property_core_random);
  return UNITY_END();
}
