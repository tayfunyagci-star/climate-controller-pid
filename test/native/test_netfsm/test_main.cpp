// Wi-Fi bağlantı yaşam döngüsü (cc_netfsm) — SCADA ailesi AP kurulum senaryoları.
#include <unity.h>
#include "cc_netfsm.h"

using namespace cc;

void setUp() {}
void tearDown() {}

struct Sim {
  NetFsm f;
  NetInput in;
  uint32_t t = 0;
  bool ap = false, services = false, sta_try = false, used_static = false;
  NetEvent last = NetEvent::NONE;
  int ap_starts = 0, attempts = 0;
  NetActions step() {
    NetActions a = f.step(in, t);
    if (a.start_ap) { ap = true; ++ap_starts; }
    if (a.stop_ap) ap = false;
    if (a.begin_sta) { sta_try = true; used_static = a.use_static; ++attempts; }
    if (a.stop_sta) sta_try = false;
    if (a.start_services) services = true;
    if (a.stop_services) services = false;
    if (a.ev != NetEvent::NONE) last = a.ev;
    return a;
  }
  void run(uint32_t ms) { for (uint32_t k = 0; k < ms; k += 100) { t += 100; step(); } }
};

// İlk açılış: SSID yok → AP açılır, STA denenmez, AP'de kalınır
void test_first_boot_ap() {
  Sim s;
  s.step();
  TEST_ASSERT_TRUE(s.ap);
  TEST_ASSERT_EQUAL(NetPhase::AP_ONLY, s.f.phase());
  s.run(600000);
  TEST_ASSERT_EQUAL(1, s.ap_starts);
  TEST_ASSERT_EQUAL(0, s.attempts);
}

// AP'de Wi-Fi kaydedilir → AP+STA denemesi; bağlanınca servisler başlar, kurulum ağı devir süresince
// (120 s) açık kalır ki sonuç ve yeni adres kurulum sayfasında görülsün; süre dolunca kapanır
void test_setup_then_connect_handover_then_ap_closes() {
  Sim s;
  s.step();
  const uint32_t seq0 = s.f.trySeq();
  s.in.configured = true;
  s.f.requestReconnect();
  s.step();
  TEST_ASSERT_EQUAL(NetPhase::CONNECTING, s.f.phase());
  TEST_ASSERT_EQUAL(seq0 + 1, s.f.trySeq());
  TEST_ASSERT_EQUAL(NetResult::TRYING, s.f.result());
  TEST_ASSERT_TRUE(s.ap);            // deneme sırasında kurulum ağı açık kalır
  s.run(3000);
  s.in.sta_connected = true;
  s.step();
  TEST_ASSERT_TRUE(s.f.online());
  TEST_ASSERT_EQUAL(NetResult::CONNECTED, s.f.result());
  TEST_ASSERT_EQUAL(NetEvent::AP_HANDOVER, s.last);
  TEST_ASSERT_TRUE(s.ap);
  TEST_ASSERT_TRUE(s.f.handover());
  TEST_ASSERT_TRUE(s.services);
  TEST_ASSERT_UINT32_WITHIN(100, 120000, s.f.apCloseInMs(s.t));
  s.run(119000);
  TEST_ASSERT_TRUE(s.ap);
  s.run(1100);
  TEST_ASSERT_FALSE(s.ap);
  TEST_ASSERT_FALSE(s.f.handover());
  TEST_ASSERT_TRUE(s.f.online());
}

// "Kurulumu bitir": devirdeki kurulum ağı hemen kapanır; devir dışındaki istek birikmez
void test_release_closes_handover_ap() {
  Sim s;
  s.step();
  s.f.releaseAp();                   // devir yokken: etkisiz, sonraya taşınmaz
  s.step();
  s.in.configured = true;
  s.f.requestReconnect();
  s.step();
  s.run(2000);
  s.in.sta_connected = true;
  s.step();
  TEST_ASSERT_TRUE(s.ap);
  s.f.releaseAp();
  s.step();
  TEST_ASSERT_FALSE(s.ap);
  TEST_ASSERT_EQUAL(NetEvent::AP_STOPPED, s.last);
}

// Kayıtlı ağ + AP (kurtarma): arka plan denemesi başarılı; kurulum ağında istemci yoksa AP hemen kapanır,
// istemci varsa devir uygulanır
void test_background_success_handover_only_with_clients() {
  Sim s;
  s.in.configured = true;
  s.step();
  s.run(20100);                      // 20 s → AP
  TEST_ASSERT_TRUE(s.ap);
  s.run(300100);                     // 5 dk → arka plan denemesi
  s.in.sta_connected = true;
  s.step();
  TEST_ASSERT_FALSE(s.ap);           // kimse yok: eski davranış
  Sim c;
  c.in.configured = true;
  c.step();
  c.run(20100);
  c.run(300100);
  c.in.ap_clients = true;
  c.in.sta_connected = true;
  c.step();
  TEST_ASSERT_TRUE(c.ap);
  TEST_ASSERT_TRUE(c.f.handover());
}

// Başarısız deneme: sonuç FAILED ve cihazın bildirdiği neden sınıfı korunur; yeni kayıt sıfırlar
void test_failed_attempt_reports_reason_class() {
  Sim s;
  s.step();
  s.in.configured = true;
  s.f.requestReconnect();
  s.step();
  s.in.fail = netFailFrom(15, false);   // 4WAY_HANDSHAKE_TIMEOUT
  s.run(20100);
  TEST_ASSERT_EQUAL(NetResult::FAILED, s.f.result());
  TEST_ASSERT_EQUAL(NetFail::AUTH, s.f.lastFail());
  TEST_ASSERT_TRUE(s.ap);
  s.in.fail = NetFail::NONE;
  s.f.requestReconnect();
  s.step();
  TEST_ASSERT_EQUAL(NetResult::TRYING, s.f.result());
  TEST_ASSERT_EQUAL(NetFail::NONE, s.f.lastFail());
}

void test_reason_classes() {
  TEST_ASSERT_EQUAL(NetFail::NONE, classifyWifiReason(0));
  TEST_ASSERT_EQUAL(NetFail::NONE, classifyWifiReason(8));
  TEST_ASSERT_EQUAL(NetFail::NOT_FOUND, classifyWifiReason(201));
  TEST_ASSERT_EQUAL(NetFail::AUTH, classifyWifiReason(202));
  TEST_ASSERT_EQUAL(NetFail::AUTH, classifyWifiReason(204));
  TEST_ASSERT_EQUAL(NetFail::ASSOC, classifyWifiReason(203));
  TEST_ASSERT_EQUAL(NetFail::SIGNAL_LOST, classifyWifiReason(200));
  TEST_ASSERT_EQUAL(NetFail::OTHER, classifyWifiReason(205));
  TEST_ASSERT_EQUAL(NetFail::NO_IP, netFailFrom(201, true));
}

// Kayıtlı ağ, boot'ta ulaşılamaz → 20 s sonra AP; 5 dk'da bir arka plan denemesi
void test_boot_unreachable_opens_ap_and_retries_every_5min() {
  Sim s;
  s.in.configured = true;
  s.step();
  TEST_ASSERT_FALSE(s.ap);
  TEST_ASSERT_EQUAL(1, s.attempts);
  s.run(19900);
  TEST_ASSERT_FALSE(s.ap);
  s.run(200);
  TEST_ASSERT_TRUE(s.ap);
  TEST_ASSERT_EQUAL(NetEvent::TIMEOUT_TO_AP, s.last);
  TEST_ASSERT_FALSE(s.sta_try);      // AP kanalı sabit kalsın
  s.run(299000);
  TEST_ASSERT_EQUAL(1, s.attempts);
  s.run(1100);
  TEST_ASSERT_EQUAL(2, s.attempts);
  TEST_ASSERT_TRUE(s.ap);
  // ağ geri geldi
  s.in.sta_connected = true;
  s.run(200);
  TEST_ASSERT_TRUE(s.f.online());
  TEST_ASSERT_FALSE(s.ap);
  TEST_ASSERT_EQUAL(1, s.ap_starts);
}

// Statik IP başarısız → aynı açılışta DHCP; DHCP ile bağlanınca alarm olayı
void test_static_timeout_falls_back_to_dhcp() {
  Sim s;
  s.in.configured = true;
  s.in.static_enabled = true;
  s.in.static_valid = true;
  s.step();
  TEST_ASSERT_TRUE(s.used_static);
  s.run(20100);
  TEST_ASSERT_EQUAL(2, s.attempts);
  TEST_ASSERT_FALSE(s.used_static);
  TEST_ASSERT_FALSE(s.ap);           // AP ancak DHCP de başarısızsa
  s.in.sta_connected = true;
  s.step();
  TEST_ASSERT_EQUAL(NetEvent::CONNECTED_DHCP_FALLBACK, s.last);
  TEST_ASSERT_TRUE(s.f.staticFailed());
}

// Geçersiz statik ayar hiç denenmez
void test_invalid_static_goes_dhcp_directly() {
  Sim s;
  s.in.configured = true;
  s.in.static_enabled = true;
  s.in.static_valid = false;
  NetActions a = s.step();
  TEST_ASSERT_TRUE(a.begin_sta);
  TEST_ASSERT_FALSE(a.use_static);
}

// Kopma: servisler durur, 15 s sonra yeniden deneme; o da başarısızsa AP açılır
void test_disconnect_retry_then_ap() {
  Sim s;
  s.in.configured = true;
  s.step();
  s.in.sta_connected = true;
  s.step();
  TEST_ASSERT_TRUE(s.services);
  s.in.sta_connected = false;
  s.step();
  TEST_ASSERT_FALSE(s.services);
  TEST_ASSERT_EQUAL(NetEvent::DISCONNECTED, s.last);
  s.run(14900);
  TEST_ASSERT_EQUAL(1, s.attempts);
  s.run(200);
  TEST_ASSERT_EQUAL(2, s.attempts);
  TEST_ASSERT_FALSE(s.ap);
  s.run(20100);
  TEST_ASSERT_TRUE(s.ap);
}

// Wi-Fi sıfırlama (kimlik silindi): yeniden başlatmadan AP'ye döner
void test_reset_wifi_returns_to_ap() {
  Sim s;
  s.in.configured = true;
  s.step();
  s.in.sta_connected = true;
  s.step();
  s.in.configured = false;
  s.in.sta_connected = false;
  s.f.requestReconnect();
  s.step();
  TEST_ASSERT_TRUE(s.ap);
  TEST_ASSERT_FALSE(s.services);
  TEST_ASSERT_EQUAL(NetPhase::AP_ONLY, s.f.phase());
}

// millis() taşması
void test_wraparound() {
  Sim s;
  s.t = 0xFFFFF000u;
  s.in.configured = true;
  s.step();
  s.run(20100);
  TEST_ASSERT_TRUE(s.ap);
}

void test_ipv4_parse_and_static_rules() {
  uint32_t ip, mask, gw;
  TEST_ASSERT_TRUE(parseIpv4("192.168.1.57", ip));
  TEST_ASSERT_EQUAL_HEX32(0xC0A80139, ip);
  TEST_ASSERT_FALSE(parseIpv4("192.168.1", ip));
  TEST_ASSERT_FALSE(parseIpv4("192.168.1.256", ip));
  TEST_ASSERT_FALSE(parseIpv4("192.168.01.1x", ip));
  TEST_ASSERT_FALSE(parseIpv4("", ip));
  parseIpv4("255.255.255.0", mask);
  parseIpv4("192.168.1.1", gw);
  TEST_ASSERT_TRUE(validStaticIpv4(ip, mask, gw, nullptr));
  const char* e = nullptr;
  uint32_t bad;
  parseIpv4("255.0.255.0", bad);
  TEST_ASSERT_FALSE(validStaticIpv4(ip, bad, gw, &e));
  parseIpv4("192.168.2.1", bad);
  TEST_ASSERT_FALSE(validStaticIpv4(ip, mask, bad, &e));
  TEST_ASSERT_FALSE(validStaticIpv4(gw, mask, gw, &e));
  parseIpv4("192.168.1.255", bad);
  TEST_ASSERT_FALSE(validStaticIpv4(bad, mask, gw, &e));
  parseIpv4("192.168.1.0", bad);
  TEST_ASSERT_FALSE(validStaticIpv4(bad, mask, gw, &e));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_boot_ap);
  RUN_TEST(test_setup_then_connect_handover_then_ap_closes);
  RUN_TEST(test_release_closes_handover_ap);
  RUN_TEST(test_background_success_handover_only_with_clients);
  RUN_TEST(test_failed_attempt_reports_reason_class);
  RUN_TEST(test_reason_classes);
  RUN_TEST(test_boot_unreachable_opens_ap_and_retries_every_5min);
  RUN_TEST(test_static_timeout_falls_back_to_dhcp);
  RUN_TEST(test_invalid_static_goes_dhcp_directly);
  RUN_TEST(test_disconnect_retry_then_ap);
  RUN_TEST(test_reset_wifi_returns_to_ap);
  RUN_TEST(test_wraparound);
  RUN_TEST(test_ipv4_parse_and_static_rules);
  return UNITY_END();
}
