// Yerel program modülü — ADR-009 / PROGRAMS.md: takvim, oluşum, öncelik, atlama, sonraki değişim, doğrulama
// ve ClimateCore entegrasyonu.
#include <unity.h>
#include <cstring>
#include "cc_schedule.h"
#include "support/sim.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static int32_t D(const char* s) { int32_t d = 0; TEST_ASSERT_TRUE_MESSAGE(parseDate(s, d), s); return d; }
static int64_t at(const char* date, int hh, int mm) { return (int64_t)D(date) * 1440 + hh * 60 + mm; }
static Program weekly(const char* nm, uint8_t days, int sh, int sm, int eh, int em, ProgAction a = ProgAction::SETPOINT, float sp = 22) {
  Program p;
  std::strcpy(p.name, nm);
  p.kind = ProgKind::WEEKLY;
  p.days = days;
  p.start_min = (uint16_t)(sh * 60 + sm);
  p.end = ProgEnd::END_TIME;
  p.end_min = (uint16_t)(eh * 60 + em);
  p.action = a;
  p.setpoint = sp;
  return p;
}
constexpr uint8_t MON = 1, TUE = 2, WED = 4, THU = 8, FRI = 16, SAT = 32, SUN = 64, WEEKDAYS = 31;

// ---------------- Takvim ----------------
void test_calendar() {
  TEST_ASSERT_EQUAL_INT32(0, daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_UINT8(3, weekdayMon0(0));                 // 1970-01-01 Perşembe
  TEST_ASSERT_EQUAL_UINT8(4, weekdayMon0(D("2026-09-25")));   // Cuma
  TEST_ASSERT_EQUAL_UINT8(1, weekdayMon0(D("2028-02-29")));   // Salı (artık yıl)
  TEST_ASSERT_EQUAL_INT32(1, D("2100-03-01") - D("2100-02-28"));  // 2100 artık değil
  TEST_ASSERT_EQUAL_INT32(2, D("2000-03-01") - D("2000-02-28"));  // 2000 artık
  int32_t d;
  TEST_ASSERT_FALSE(parseDate("2026-02-29", d));
  TEST_ASSERT_TRUE(parseDate("2024-02-29", d));
  TEST_ASSERT_FALSE(parseDate("2026-13-01", d));
  TEST_ASSERT_FALSE(parseDate("2026-1-01", d));
  TEST_ASSERT_FALSE(parseDate("26-01-01xx", d));
  // gidiş-dönüş: 2000…2199 her gün
  char buf[11];
  for (int32_t z = D("2000-01-01"); z <= D("2199-12-31"); ++z) {
    formatDate(z, buf);
    int32_t back;
    if (!parseDate(buf, back) || back != z) TEST_FAIL_MESSAGE(buf);
  }
}

// ---------------- Oluşumlar ----------------
void test_weekly_window() {
  Program p = weekly("Sabah", WEEKDAYS, 6, 0, 8, 0);
  Occurrence o;
  TEST_ASSERT_TRUE(activeOccurrence(p, at("2026-09-25", 7, 0), o));   // Cuma
  TEST_ASSERT_EQUAL_INT64(at("2026-09-25", 6, 0), o.start);
  TEST_ASSERT_EQUAL_INT64(at("2026-09-25", 8, 0), o.end);
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-25", 8, 0), o));  // bitiş hariç
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-25", 5, 59), o));
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-26", 7, 0), o));  // Cumartesi
  p.enabled = false;
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-25", 7, 0), o));
}

void test_weekly_crosses_midnight() {
  Program p = weekly("Gece", FRI, 22, 0, 6, 0, ProgAction::PROFILE);
  p.profile = ProfileSel::NIGHT;
  Occurrence o;
  TEST_ASSERT_TRUE(activeOccurrence(p, at("2026-09-26", 5, 59), o));  // Cumartesi sabahı, Cuma oluşumu
  TEST_ASSERT_EQUAL_INT64(at("2026-09-25", 22, 0), o.start);
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-26", 6, 0), o));
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-25", 5, 0), o));  // Perşembe gecesi seçili değil
}

void test_duration_and_all_day() {
  Program p = weekly("Kısa", SAT, 14, 0, 0, 0);
  p.end = ProgEnd::DURATION;
  p.duration_min = 90;
  Occurrence o;
  TEST_ASSERT_TRUE(activeOccurrence(p, at("2026-09-26", 15, 29), o));
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-26", 15, 30), o));
  p.end = ProgEnd::ALL_DAY;
  TEST_ASSERT_TRUE(activeOccurrence(p, at("2026-09-26", 0, 0), o));
  TEST_ASSERT_TRUE(activeOccurrence(p, at("2026-09-26", 23, 59), o));
  TEST_ASSERT_FALSE(activeOccurrence(p, at("2026-09-27", 0, 0), o));
}

void test_date_range_and_once() {
  Program r;
  std::strcpy(r.name, "Tatil");
  r.kind = ProgKind::DATE_RANGE;
  r.days = 0;
  r.date_from = D("2026-12-24");
  r.date_to = D("2026-12-31");
  r.end = ProgEnd::ALL_DAY;
  r.action = ProgAction::PROFILE;
  r.profile = ProfileSel::AWAY;
  Occurrence o;
  TEST_ASSERT_TRUE(activeOccurrence(r, at("2026-12-31", 23, 59), o));
  TEST_ASSERT_FALSE(activeOccurrence(r, at("2027-01-01", 0, 0), o));
  TEST_ASSERT_FALSE(activeOccurrence(r, at("2026-12-23", 23, 59), o));
  r.days = SAT | SUN;  // aralıkta yalnız hafta sonu
  TEST_ASSERT_TRUE(activeOccurrence(r, at("2026-12-26", 10, 0), o));   // Cumartesi
  TEST_ASSERT_FALSE(activeOccurrence(r, at("2026-12-28", 10, 0), o));  // Pazartesi
  Program once;
  std::strcpy(once.name, "Misafir");
  once.kind = ProgKind::ONCE;
  once.date_from = D("2026-10-03");
  once.start_min = 14 * 60;
  once.end = ProgEnd::DURATION;
  once.duration_min = 3 * 1440;  // 3 gün
  TEST_ASSERT_TRUE(activeOccurrence(once, at("2026-10-05", 13, 59), o));
  TEST_ASSERT_FALSE(activeOccurrence(once, at("2026-10-06", 14, 0), o));
  TEST_ASSERT_EQUAL_UINT8(3, o.cls);
}

// ---------------- Öncelik ve kanallar ----------------
void test_priority_and_channels() {
  Program list[4];
  list[0] = weekly("Hafta", WEEKDAYS | SAT | SUN, 0, 0, 23, 59, ProgAction::SETPOINT, 20);
  list[1] = weekly("Öğle", WEEKDAYS | SAT | SUN, 12, 0, 14, 0, ProgAction::SETPOINT, 23);
  list[2] = Program();
  std::strcpy(list[2].name, "Etkinlik");
  list[2].kind = ProgKind::ONCE;
  list[2].date_from = D("2026-09-25");
  list[2].start_min = 13 * 60;
  list[2].end = ProgEnd::DURATION;
  list[2].duration_min = 30;
  list[2].setpoint = 25;
  list[3] = weekly("Havalandır", WEEKDAYS, 12, 30, 13, 30, ProgAction::VENTILATE);
  Hold none;
  ScheduleResult r = evaluate(list, 4, at("2026-09-25", 11, 0), none);
  TEST_ASSERT_EQUAL_INT8(0, r.climate.index);
  r = evaluate(list, 4, at("2026-09-25", 12, 10), none);
  TEST_ASSERT_EQUAL_INT8(1, r.climate.index);  // aynı sınıf: daha geç başlayan
  TEST_ASSERT_EQUAL_INT8(-1, r.vent.index);
  r = evaluate(list, 4, at("2026-09-25", 13, 10), none);
  TEST_ASSERT_EQUAL_INT8(2, r.climate.index);  // ONCE › WEEKLY
  TEST_ASSERT_EQUAL_INT8(3, r.vent.index);     // havalandırma bağımsız kanal
  TEST_ASSERT_TRUE(r.valid);
}

void test_hold_skips_current_occurrence_only() {
  Program list[1] = {weekly("Sabah", WEEKDAYS, 6, 0, 8, 0)};
  ScheduleResult r = evaluate(list, 1, at("2026-09-24", 7, 0), Hold());
  Hold h;
  h.index = r.climate.index;
  h.start = r.climate.start;
  r = evaluate(list, 1, at("2026-09-24", 7, 30), h);
  TEST_ASSERT_EQUAL_INT8(-1, r.climate.index);
  TEST_ASSERT_TRUE(r.held);
  r = evaluate(list, 1, at("2026-09-25", 7, 0), h);  // ertesi gün yeniden etkin
  TEST_ASSERT_EQUAL_INT8(0, r.climate.index);
}

void test_next_change() {
  Program list[2] = {weekly("Sabah", WEEKDAYS, 6, 0, 8, 0), weekly("Hep", WEEKDAYS | SAT | SUN, 0, 0, 0, 0)};
  list[1].end = ProgEnd::ALL_DAY;
  list[1].kind = ProgKind::WEEKLY;
  Program one[1] = {list[0]};
  ScheduleResult r = evaluate(one, 1, at("2026-09-25", 5, 0), Hold());
  TEST_ASSERT_EQUAL_INT64(at("2026-09-25", 6, 0), r.next_change);
  r = evaluate(one, 1, at("2026-09-25", 7, 0), Hold());
  TEST_ASSERT_EQUAL_INT64(at("2026-09-25", 8, 0), r.next_change);
  r = evaluate(one, 1, at("2026-09-25", 9, 0), Hold());  // Cuma sonrası: Pazartesi 06:00
  TEST_ASSERT_EQUAL_INT64(at("2026-09-28", 6, 0), r.next_change);
  r = evaluate(list, 2, at("2026-09-25", 9, 0), Hold());  // "Hep" gece yarısı yeni oluşum başlatır
  TEST_ASSERT_EQUAL_INT64(at("2026-09-26", 0, 0), r.next_change);
  Program none[1] = {list[0]};
  none[0].enabled = false;
  TEST_ASSERT_EQUAL_INT64(-1, evaluate(none, 1, at("2026-09-25", 9, 0), Hold()).next_change);
}

// ---------------- Doğrulama ----------------
void test_validation() {
  Program p = weekly("Sabah", WEEKDAYS, 6, 0, 8, 0);
  TEST_ASSERT_TRUE(validatePrograms(&p, 1, 40).ok());
  Program b = p; b.name[0] = 0;                        TEST_ASSERT_EQUAL(ProgErr::NAME, validatePrograms(&b, 1, 40).err);
  b = p; b.days = 0;                                   TEST_ASSERT_EQUAL(ProgErr::DAYS, validatePrograms(&b, 1, 40).err);
  b = p; b.start_min = 1440;                           TEST_ASSERT_EQUAL(ProgErr::START, validatePrograms(&b, 1, 40).err);
  b = p; b.end_min = b.start_min;                      TEST_ASSERT_EQUAL(ProgErr::END, validatePrograms(&b, 1, 40).err);
  b = p; b.end = ProgEnd::DURATION; b.duration_min = 1441; TEST_ASSERT_EQUAL(ProgErr::DURATION, validatePrograms(&b, 1, 40).err);
  b = p; b.setpoint = 22.3f;                           TEST_ASSERT_EQUAL(ProgErr::SETPOINT, validatePrograms(&b, 1, 40).err);
  b = p; b.setpoint = 30.5f;                           TEST_ASSERT_EQUAL(ProgErr::SETPOINT, validatePrograms(&b, 1, 40).err);
  b = p; b.setpoint = 28;                              TEST_ASSERT_EQUAL(ProgErr::SAFETY_MARGIN, validatePrograms(&b, 1, 36).err);
  b = p; b.action = ProgAction::PROFILE; b.profile = ProfileSel::DAY; TEST_ASSERT_EQUAL(ProgErr::PROFILE, validatePrograms(&b, 1, 40).err);
  b = p; b.kind = ProgKind::DATE_RANGE; b.date_from = D("2026-10-10"); b.date_to = D("2026-10-01");
  TEST_ASSERT_EQUAL(ProgErr::DATE_ORDER, validatePrograms(&b, 1, 40).err);
  b.date_to = D("2027-12-01");                         TEST_ASSERT_EQUAL(ProgErr::DATE_SPAN, validatePrograms(&b, 1, 40).err);
  Program many[kMaxPrograms + 1];
  for (auto& m : many) m = p;
  TEST_ASSERT_EQUAL(ProgErr::TOO_MANY, validatePrograms(many, kMaxPrograms + 1, 40).err);
  Program two[2] = {p, p};
  two[1].days = 0;
  ProgValidation v = validatePrograms(two, 2, 40);
  TEST_ASSERT_EQUAL_INT8(1, v.index);
}

// ---------------- ClimateCore entegrasyonu ----------------
static sim::Runner* clockRunner(const char* date, int hh, int mm, float T) {
  auto* r = new sim::Runner();
  r->cabin.T = T;
  r->cabin.T_out = 10;
  r->clock = true;
  // tz_offset_min 180: yerel = UTC + 3 sa
  r->epoch0 = (int64_t)D(date) * 86400 + (hh * 60 + mm - 180) * 60;
  Config c;
  c.setpoint_ramp_c_per_min = 0;
  r->boot(c);
  return r;
}

void test_core_program_setpoint_and_priorities() {
  sim::Runner* r = clockRunner("2026-09-25", 6, 55, 20);
  Program p = weekly("Sabah", WEEKDAYS, 7, 0, 9, 0, ProgAction::SETPOINT, 24);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_POLICY, r->core.setPrograms(&p, 1, CmdSource::MQTT).result);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r->core.setPrograms(&p, 1, CmdSource::LOCAL_WEB).result);
  r->run(3);
  TEST_ASSERT_EQUAL_INT8(-1, r->s().program_index);
  TEST_ASSERT_EQUAL_FLOAT(21.0f, r->s().setpoint_effective);
  r->run(5 * 60);
  TEST_ASSERT_EQUAL_INT8(0, r->s().program_index);
  TEST_ASSERT_EQUAL(ProfileActive::PROGRAM, r->s().profile_active);
  TEST_ASSERT_EQUAL(SetpointSource::PROGRAM, r->s().setpoint_source);
  TEST_ASSERT_EQUAL_FLOAT(24.0f, r->s().setpoint_effective);
  TEST_ASSERT_TRUE(r->core.events().contains(EvCode::PROGRAM_START));
  // Suite sched_night programın altında kalır
  r->core.command("sched_night", "ON", CmdSource::MQTT);
  r->run(3);
  TEST_ASSERT_EQUAL(ProfileActive::PROGRAM, r->s().profile_active);
  // Açık kullanıcı seçimi ve BOOST programın üstündedir
  r->core.command("profile", "AWAY", CmdSource::LOCAL_WEB);
  r->run(3);
  TEST_ASSERT_EQUAL(ProfileActive::AWAY, r->s().profile_active);
  r->core.command("profile", "DAY", CmdSource::LOCAL_WEB);
  r->core.setBoost(true, CmdSource::LOCAL_WEB);
  r->run(3);
  TEST_ASSERT_EQUAL(ProfileActive::BOOST, r->s().profile_active);
  r->core.setBoost(false, CmdSource::LOCAL_WEB);
  // Atla: oluşum bitene kadar program uygulanmaz; sched_night devreye girer
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r->core.command("program_hold", "PRESS", CmdSource::LOCAL_WEB).result);
  r->run(3);
  TEST_ASSERT_TRUE(r->s().program_held);
  TEST_ASSERT_EQUAL(ProfileActive::NIGHT, r->s().profile_active);
  // Modül kapatılınca da program yok
  r->core.clearHold(CmdSource::LOCAL_WEB);
  r->core.command("programs_enabled", "OFF", CmdSource::LOCAL_WEB);
  r->run(3);
  TEST_ASSERT_EQUAL_INT8(-1, r->s().program_index);
  // Bitiş: 09:00'da program sona erer
  r->core.command("programs_enabled", "ON", CmdSource::LOCAL_WEB);
  r->core.command("sched_night", "OFF", CmdSource::MQTT);
  r->run(3);
  TEST_ASSERT_EQUAL_INT8(0, r->s().program_index);
  TEST_ASSERT_TRUE(r->s().program_until > 0);
  r->run(2 * 3600);
  TEST_ASSERT_EQUAL_INT8(-1, r->s().program_index);
  TEST_ASSERT_TRUE(r->core.events().contains(EvCode::PROGRAM_END));
  TEST_ASSERT_TRUE(r->inv.ok());
  delete r;
}

void test_core_program_heating_off_keeps_antifreeze() {
  sim::Runner* r = clockRunner("2026-09-25", 12, 0, 20);
  Program p = weekly("Kapalı", WEEKDAYS | SAT | SUN, 0, 0, 0, 0, ProgAction::HEATING_OFF);
  p.end = ProgEnd::ALL_DAY;
  r->core.setPrograms(&p, 1, CmdSource::LOCAL_WEB);
  r->run(600);
  TEST_ASSERT_TRUE(r->s().heat_suspended);
  TEST_ASSERT_FALSE(r->out(R1) || r->out(R2));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, r->s().heat_demand);
  // Donma riski: program ısıtmayı durdurmuş olsa da antifreeze ısıtır
  r->cabin.T = 3.5f;
  r->cabin.T_out = -15;
  float t = r->runUntil([&] { return r->out(R1); }, 300);
  TEST_ASSERT_TRUE(t > 0);
  TEST_ASSERT_EQUAL(HeatingReason::ANTIFREEZE, r->s().heating_reason);
  delete r;
}

void test_core_program_ventilate_and_invalid_clock() {
  sim::Runner* r = clockRunner("2026-09-25", 10, 0, 23);  // hedefin üstünde: ısıtma yok (§5.2 ısıtırken SCHEDULED engellenir)
  r->cabin.T_out = 23;
  Program p = weekly("Hava", WEEKDAYS, 10, 1, 10, 30, ProgAction::VENTILATE);
  r->core.setPrograms(&p, 1, CmdSource::LOCAL_WEB);
  float t = r->runUntil([&] { return r->out(VF); }, 200);
  TEST_ASSERT_TRUE(t > 0);
  TEST_ASSERT_EQUAL_INT8(0, r->s().program_vent_index);
  TEST_ASSERT_EQUAL(Reason::AUTO_DEMAND, r->s().reason[VF]);
  delete r;
  // Saat geçersiz: program çalışmaz
  sim::Runner q;
  q.cabin.T = 20;
  Config c;
  c.setpoint_ramp_c_per_min = 0;
  q.boot(c);
  Program s = weekly("Her", WEEKDAYS | SAT | SUN, 0, 0, 0, 0, ProgAction::SETPOINT, 25);
  s.end = ProgEnd::ALL_DAY;
  q.core.setPrograms(&s, 1, CmdSource::LOCAL_WEB);
  q.run(10);
  TEST_ASSERT_FALSE(q.s().time_valid);
  TEST_ASSERT_EQUAL_INT8(-1, q.s().program_index);
  TEST_ASSERT_EQUAL_FLOAT(21.0f, q.s().setpoint_effective);
}

// Güvenlik payı: program hedefi varken aşırı sıcaklık limiti düşürülemez (V5 eşi)
void test_core_overtemp_margin_protects_programs() {
  sim::Runner r;
  r.boot(Config());
  Program p = weekly("Sıcak", WEEKDAYS, 7, 0, 9, 0, ProgAction::SETPOINT, 29);
  TEST_ASSERT_EQUAL(CmdResult::ACCEPTED, r.core.setPrograms(&p, 1, CmdSource::LOCAL_WEB).result);
  Config c = r.core.config();
  c.cabin_overtemp_limit = 38;  // V5: 23+10=33 ≤ 38 geçerli, ama program 29+10=39 > 38
  CmdReply rep = r.core.applyConfig(c, CmdSource::LOCAL_WEB);
  TEST_ASSERT_EQUAL(CmdResult::REJECTED_RELATION, rep.result);
  TEST_ASSERT_EQUAL_FLOAT(40.0f, r.core.config().cabin_overtemp_limit);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calendar);
  RUN_TEST(test_weekly_window);
  RUN_TEST(test_weekly_crosses_midnight);
  RUN_TEST(test_duration_and_all_day);
  RUN_TEST(test_date_range_and_once);
  RUN_TEST(test_priority_and_channels);
  RUN_TEST(test_hold_skips_current_occurrence_only);
  RUN_TEST(test_next_change);
  RUN_TEST(test_validation);
  RUN_TEST(test_core_program_setpoint_and_priorities);
  RUN_TEST(test_core_program_heating_off_keeps_antifreeze);
  RUN_TEST(test_core_program_ventilate_and_invalid_clock);
  RUN_TEST(test_core_overtemp_margin_protects_programs);
  return UNITY_END();
}
