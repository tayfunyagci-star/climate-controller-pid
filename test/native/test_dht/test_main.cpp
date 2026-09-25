// DHT22 çerçeve çözücü — F2 HAL'in platformdan bağımsız kısmı (cc_dht).
#include <unity.h>
#include "cc_dht.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static DhtPulse buf[128];

static uint16_t frame(uint8_t h1, uint8_t h0, uint8_t t1, uint8_t t0, int chkDelta = 0) {
  uint8_t b[5] = {h1, h0, t1, t0, 0};
  b[4] = (uint8_t)(b[0] + b[1] + b[2] + b[3] + chkDelta);
  return encodeDht22(b, buf, 128);
}

// Veri sayfası örneği: RH 65.2 %, T 35.1 °C
void test_decode_datasheet_example() {
  const uint16_t n = frame(0x02, 0x8C, 0x01, 0x5F);
  DhtFrame f = decodeDht22(buf, n);
  TEST_ASSERT_EQUAL(DrvStatus::OK, f.status);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 65.2f, f.humidity);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 35.1f, f.temperature);
}

// Negatif sıcaklık: işaret biti (T_H bit7) — −10.1 °C
void test_decode_negative_temperature() {
  const uint16_t n = frame(0x01, 0xF4, 0x80, 0x65);
  DhtFrame f = decodeDht22(buf, n);
  TEST_ASSERT_EQUAL(DrvStatus::OK, f.status);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -10.1f, f.temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, f.humidity);
}

// Checksum hatası → CRC_ERROR, değer NaN (geçersiz ölçüm null)
void test_checksum_error() {
  const uint16_t n = frame(0x02, 0x8C, 0x01, 0x5F, 1);
  DhtFrame f = decodeDht22(buf, n);
  TEST_ASSERT_EQUAL(DrvStatus::CRC_ERROR, f.status);
  TEST_ASSERT_TRUE(std::isnan(f.temperature));
  TEST_ASSERT_TRUE(std::isnan(f.humidity));
}

// Yanıt yok (yalnız LOW / boş kayıt) → TIMEOUT
void test_no_response_timeout() {
  DhtPulse p[2] = {{0, 2000}, {0, 0}};
  TEST_ASSERT_EQUAL(DrvStatus::TIMEOUT, decodeDht22(p, 2).status);
  TEST_ASSERT_EQUAL(DrvStatus::TIMEOUT, decodeDht22(p, 0).status);
}

// Eksik bit (kopuk kablo, yarım çerçeve) → BUS_ERROR
void test_truncated_frame() {
  const uint16_t n = frame(0x02, 0x8C, 0x01, 0x5F);
  DhtFrame f = decodeDht22(buf, (uint16_t)(n - 30));
  TEST_ASSERT_EQUAL(DrvStatus::BUS_ERROR, f.status);
}

// Belirsiz bit süresi (50 µs) → BUS_ERROR
void test_ambiguous_bit_width() {
  const uint16_t n = frame(0x02, 0x8C, 0x01, 0x5F);
  buf[11].us = 50;  // bir veri biti HIGH
  TEST_ASSERT_EQUAL(1, buf[11].level);
  TEST_ASSERT_EQUAL(DrvStatus::BUS_ERROR, decodeDht22(buf, n).status);
}

// Sensör yanıtından önce ana birimin LOW darbesi ve fazladan önek yakalansa da son 40 bit çözülür
void test_leading_host_pulse_ignored() {
  uint8_t b[5] = {0x02, 0x8C, 0x01, 0x5F, 0};
  b[4] = (uint8_t)(b[0] + b[1] + b[2] + b[3]);
  DhtPulse p[130];
  p[0] = {1, 30};    // hat bırakıldı (pull-up)
  const uint16_t n = encodeDht22(b, p + 1, 129);
  DhtFrame f = decodeDht22(p, (uint16_t)(n + 1));
  TEST_ASSERT_EQUAL(DrvStatus::OK, f.status);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 35.1f, f.temperature);
}

// Fiziksel aralık dışı (RH 120 %, checksum doğru) → BUS_ERROR
void test_out_of_range_rejected() {
  const uint16_t n = frame(0x04, 0xB0, 0x00, 0xC8);  // 120.0 %, 20.0 °C
  TEST_ASSERT_EQUAL(DrvStatus::BUS_ERROR, decodeDht22(buf, n).status);
}

// Gidiş-dönüş: aralık içindeki bütün tam derece/nem değerleri
void test_roundtrip_sweep() {
  for (int t = -400; t <= 800; t += 37) {
    for (int h = 0; h <= 1000; h += 97) {
      const uint16_t at = (uint16_t)(t < 0 ? -t : t);
      const uint16_t n = frame((uint8_t)(h >> 8), (uint8_t)h, (uint8_t)((at >> 8) | (t < 0 ? 0x80 : 0)), (uint8_t)at);
      DhtFrame f = decodeDht22(buf, n);
      TEST_ASSERT_EQUAL(DrvStatus::OK, f.status);
      TEST_ASSERT_FLOAT_WITHIN(0.001f, t / 10.0f, f.temperature);
      TEST_ASSERT_FLOAT_WITHIN(0.001f, h / 10.0f, f.humidity);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_datasheet_example);
  RUN_TEST(test_decode_negative_temperature);
  RUN_TEST(test_checksum_error);
  RUN_TEST(test_no_response_timeout);
  RUN_TEST(test_truncated_frame);
  RUN_TEST(test_ambiguous_bit_width);
  RUN_TEST(test_leading_host_pulse_ignored);
  RUN_TEST(test_out_of_range_rejected);
  RUN_TEST(test_roundtrip_sweep);
  return UNITY_END();
}
