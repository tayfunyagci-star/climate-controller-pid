// WS2812B durum şeridi: durum seçimi, yanıp sönme, parlaklık, renk anahtarları.
#include <unity.h>
#include "cc_ledstrip.h"

using namespace cc;

void setUp() {}
void tearDown() {}

void test_status_led_alarm_levels() {
  LedInputs in;
  uint8_t st[kLedCount];
  ledStates(in, st);
  TEST_ASSERT_EQUAL(0, st[0]);
  in.alarm = Severity::INFO;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(0, st[0]);   // bilgi seviyesi alarm sayılmaz
  in.alarm = Severity::WARNING;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(1, st[0]);
  in.alarm = Severity::CRITICAL;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(2, st[0]);
  in.alarm = Severity::NONE;
  in.failsafe = true;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(2, st[0]);
}

void test_network_mqtt_mdns() {
  LedInputs in;
  uint8_t st[kLedCount];
  ledStates(in, st);
  TEST_ASSERT_EQUAL(0, st[1]);   // bağlantı yok
  TEST_ASSERT_EQUAL(2, st[2]);   // MQTT tanımsız
  TEST_ASSERT_EQUAL(0, st[3]);   // mDNS yok
  in.ap_mode = true;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(2, st[1]);   // yalnız kurulum ağı
  in.sta_ok = true;              // devir: STA bağlı + AP açık → Wi-Fi
  in.mqtt = MqttLink::UP;
  in.mdns_ok = true;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(1, st[1]);
  TEST_ASSERT_EQUAL(1, st[2]);
  TEST_ASSERT_EQUAL(1, st[3]);
  in.mqtt = MqttLink::DOWN;
  in.mdns_enabled = false;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(0, st[2]);
  TEST_ASSERT_EQUAL(2, st[3]);
}

void test_device_leds() {
  LedInputs in;
  uint8_t st[kLedCount];
  in.heaters_on = 1;
  in.heater_fan = true;
  ledStates(in, st);
  TEST_ASSERT_EQUAL(1, st[4]);
  TEST_ASSERT_EQUAL(1, st[5]);
  in.heaters_on = 2;
  in.vent_fan = true;            // havalandırma ısıtıcı fanına baskın
  ledStates(in, st);
  TEST_ASSERT_EQUAL(2, st[4]);
  TEST_ASSERT_EQUAL(2, st[5]);
}

void test_render_blink_and_brightness() {
  LedConfig c;
  c.brightness = 100;
  uint8_t st[kLedCount] = {2, 1, 0, 0, 0, 0};
  uint8_t grb[kLedCount * 3];
  ledRender(c, st, 0, grb);
  TEST_ASSERT_EQUAL_HEX8(0x00, grb[0]);   // alarm kırmızı: G=0
  TEST_ASSERT_EQUAL_HEX8(0xFF, grb[1]);   // R=255
  TEST_ASSERT_EQUAL_HEX8(0xFF, grb[3]);   // LED2 Wi-Fi yeşil: G=255
  ledRender(c, st, kLedBlinkMs, grb);
  TEST_ASSERT_EQUAL_HEX8(0x00, grb[1]);   // yanıp sönmenin kapalı yarısı
  TEST_ASSERT_EQUAL_HEX8(0xFF, grb[3]);   // LED2 yanıp sönmez
  st[0] = 0;
  ledRender(c, st, kLedBlinkMs, grb);
  TEST_ASSERT_EQUAL_HEX8(0xFF, grb[0]);   // normal yeşil sabit
  c.brightness = 20;
  ledRender(c, st, 0, grb);
  TEST_ASSERT_EQUAL(51, grb[0]);          // 255 × %20
  c.brightness = 0;
  ledRender(c, st, 0, grb);
  TEST_ASSERT_EQUAL(0, grb[0]);
}

void test_color_parse_and_keys() {
  uint32_t v = 0;
  TEST_ASSERT_TRUE(parseHexColor("#FF8000", v));
  TEST_ASSERT_EQUAL_HEX32(0xFF8000, v);
  TEST_ASSERT_TRUE(parseHexColor("#00ff7f", v));
  TEST_ASSERT_EQUAL_HEX32(0x00FF7F, v);
  TEST_ASSERT_FALSE(parseHexColor("FF8000", v));
  TEST_ASSERT_FALSE(parseHexColor("#FF800", v));
  TEST_ASSERT_FALSE(parseHexColor("#GG8000", v));
  TEST_ASSERT_FALSE(parseHexColor(nullptr, v));
  char buf[8];
  formatHexColor(0xFF8000, buf);
  TEST_ASSERT_EQUAL_STRING("#ff8000", buf);
  uint8_t led = 9, s = 9;
  TEST_ASSERT_TRUE(ledColorKey("cls2", led, s));
  TEST_ASSERT_EQUAL(0, led);
  TEST_ASSERT_EQUAL(2, s);
  TEST_ASSERT_TRUE(ledColorKey("clf0", led, s));
  TEST_ASSERT_EQUAL(5, led);
  TEST_ASSERT_FALSE(ledColorKey("cls3", led, s));
  TEST_ASSERT_FALSE(ledColorKey("clh0", led, s));
  TEST_ASSERT_FALSE(ledColorKey("ledB", led, s));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_led_alarm_levels);
  RUN_TEST(test_network_mqtt_mdns);
  RUN_TEST(test_device_leds);
  RUN_TEST(test_render_blink_and_brightness);
  RUN_TEST(test_color_parse_and_keys);
  return UNITY_END();
}
