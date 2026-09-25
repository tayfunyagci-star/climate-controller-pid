// Rol filtresi ve kalite — SENSOR_ARCHITECTURE §4, PID_DESIGN §8.
#include <unity.h>
#include "cc_sensor.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static RoleFilter t1() { return RoleFilter(RoleParams::t1(0, 1000, 10000, 10, 1800000)); }

static const RoleReading& feed(RoleFilter& f, DrvStatus st, float v) {
  f.tick(1000);
  return f.sample(st, v, 0);
}

void test_good_and_ema() {
  RoleFilter f = t1();
  const RoleReading* r = &feed(f, DrvStatus::OK, 20);
  TEST_ASSERT_EQUAL(Quality::GOOD, r->quality);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, r->value);
  for (int i = 0; i < 10; ++i) r = &feed(f, DrvStatus::OK, 21);
  // EMA τ 10 s: 10 s sonra ~%63 (medyan gecikmesiyle biraz daha az)
  TEST_ASSERT_TRUE(r->value > 20.4f && r->value < 20.8f);
  TEST_ASSERT_EQUAL_FLOAT(21.0f, r->safety);  // medyan-3, EMA'sız
}

// Geçersiz ölçüm 0 değil NaN; 3 ardışık hata → BAD
void test_errors_bad_after_three() {
  RoleFilter f = t1();
  feed(f, DrvStatus::OK, 20);
  const RoleReading* r = &feed(f, DrvStatus::TIMEOUT, NAN);
  TEST_ASSERT_NOT_EQUAL(Quality::BAD, r->quality);
  feed(f, DrvStatus::CRC_ERROR, NAN);
  r = &feed(f, DrvStatus::TIMEOUT, NAN);
  TEST_ASSERT_EQUAL(Quality::BAD, r->quality);
  TEST_ASSERT_TRUE(std::isnan(r->value));
  TEST_ASSERT_EQUAL_UINT32(3, r->errors);
  TEST_ASSERT_EQUAL_UINT32(1, r->crc_errors);
  r = &feed(f, DrvStatus::OK, 20);
  // Geri döndü; son 10 dk hata oranı %60 > %50 → UNCERTAIN, değer kullanılabilir
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, r->value);
}

// Fiziksel aralık dışı → hata; makul aralık dışı → UNCERTAIN (değer tutulur)
void test_ranges() {
  RoleFilter f = t1();
  feed(f, DrvStatus::OK, 20);
  const RoleReading* r = &feed(f, DrvStatus::OK, 70);  // −30…60 dışı, −40…85 içi
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, r->value);
  for (int i = 0; i < 3; ++i) r = &feed(f, DrvStatus::OK, 90);
  TEST_ASSERT_EQUAL(Quality::BAD, r->quality);
}

// Tekil sıçrama medyan-3 ile elenir
void test_spike_rejected() {
  RoleFilter f = t1();
  for (int i = 0; i < 5; ++i) feed(f, DrvStatus::OK, 20);
  const RoleReading* r = &feed(f, DrvStatus::OK, 30);  // 10 °C/s > 2 °C/s
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, r->safety);
  r = &feed(f, DrvStatus::OK, 20);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, r->safety);
}

// Yaş: > 3 × aralık UNCERTAIN, > stale STALE
void test_stale() {
  RoleFilter f = t1();
  feed(f, DrvStatus::OK, 20);
  const RoleReading* r = nullptr;
  for (int i = 0; i < 4; ++i) r = &f.tick(1000);
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
  for (int i = 0; i < 7; ++i) r = &f.tick(1000);
  TEST_ASSERT_EQUAL(Quality::STALE, r->quality);
  TEST_ASSERT_TRUE(std::isnan(r->value));
}

// DS18B20 85.0 / −127 sahte değerleri BAD
void test_ds18b20_sentinels() {
  RoleFilter f(RoleParams::t2(0, 1000, 10000));
  feed(f, DrvStatus::OK, 50);
  const RoleReading* r = nullptr;
  for (int i = 0; i < 3; ++i) r = &feed(f, DrvStatus::OK, 85.0f);
  TEST_ASSERT_EQUAL(Quality::BAD, r->quality);
  r = &feed(f, DrvStatus::OK, 50);
  r = &feed(f, DrvStatus::OK, -127.0f);
  TEST_ASSERT_EQUAL_UINT32(4, r->errors);
}

// Takılı değer (ısıtma aktif) ve RH yoğuşma → UNCERTAIN
void test_stuck_and_condensation() {
  RoleFilter f(RoleParams::t1(0, 1000, 10000, 10, 300000));
  const RoleReading* r = nullptr;
  for (int i = 0; i < 302; ++i) { f.tick(1000); r = &f.sample(DrvStatus::OK, 20.0f, 0, true); }
  TEST_ASSERT_TRUE(r->stuck);
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
  RoleFilter h(RoleParams::rh1(0, 1000, 10000, 10, 1800000));
  for (int i = 0; i < 30 * 60 + 2; ++i) r = &feed(h, DrvStatus::OK, 99.7f);
  TEST_ASSERT_TRUE(r->condensation);
  TEST_ASSERT_EQUAL(Quality::UNCERTAIN, r->quality);
}

// Aralıklı hata: 10 dk'da > 20 % → intermittent
void test_intermittent() {
  RoleFilter f = t1();
  const RoleReading* r = nullptr;
  for (int i = 0; i < 300; ++i) r = &feed(f, (i % 4 == 0) ? DrvStatus::TIMEOUT : DrvStatus::OK, 20);
  TEST_ASSERT_TRUE(r->intermittent);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 25.0f, r->error_rate_10m);
}

// Kalibrasyon ofseti, DISABLED / MISSING
void test_offset_disabled_missing() {
  RoleFilter f(RoleParams::t1(1.5f, 1000, 10000, 0, 1800000));
  TEST_ASSERT_EQUAL_FLOAT(21.5f, feed(f, DrvStatus::OK, 20).value);
  f.setDisabled(true);
  TEST_ASSERT_EQUAL(Quality::DISABLED, feed(f, DrvStatus::OK, 20).quality);
  f.setDisabled(false);
  f.setMissing(true);
  TEST_ASSERT_EQUAL(Quality::MISSING, feed(f, DrvStatus::OK, 20).quality);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_good_and_ema);
  RUN_TEST(test_errors_bad_after_three);
  RUN_TEST(test_ranges);
  RUN_TEST(test_spike_rejected);
  RUN_TEST(test_stale);
  RUN_TEST(test_ds18b20_sentinels);
  RUN_TEST(test_stuck_and_condensation);
  RUN_TEST(test_intermittent);
  RUN_TEST(test_offset_disabled_missing);
  return UNITY_END();
}
