// Wi-Fi bağlantı yaşam döngüsü (AP kurulum + STA + statik→DHCP düşüşü + AP'de periyodik yeniden deneme).
// SCADA cihaz ailesiyle aynı davranış (4chRelayModule network.h wifiTick, Flowmeter ESP32 startStationMode):
//   - SSID yok            → kurulum AP'si (SCADA_AP_<id>, 192.168.4.1, captive DNS), bekle
//   - bağlanma denemesi   → 20 s; statik başarısızsa bir kez DHCP; o da olmazsa AP açılır (AP+STA)
//   - AP açıkken          → 5 dk'da bir arka planda yeniden dene; bağlanınca AP kapanır
//   - bağlantı koparsa    → servisler durur, 15 s sonra yeniden dene (başarısız deneme AP'yi açar)
//   - ayar değişimi/sıfırlama → deneme baştan (cihaz yeniden başlatılmaz; kontrol etkilenmez)
// Saf mantık: platform çağrıları (WiFi, DNS, mDNS, OTA) NetActions ile HAL'e bırakılır; native testlidir.
#pragma once
#include <cstdint>

namespace cc {

struct NetFsmParams {
  uint32_t connect_timeout_ms = 20000;   // tek deneme
  uint32_t retry_sta_ms = 15000;         // AP kapalıyken kopma sonrası bekleme
  uint32_t retry_ap_ms = 300000;         // AP açıkken arka plan deneme aralığı (5 dk)
};

enum class NetPhase : uint8_t { AP_ONLY, CONNECTING, ONLINE, WAITING };

enum class NetEvent : uint8_t {
  NONE,
  AP_STARTED,              // kurulum ağı açıldı
  AP_STOPPED,              // STA bağlandı, kurulum ağı kapandı
  CONNECTING,
  CONNECTED,
  CONNECTED_DHCP_FALLBACK, // statik başarısız, DHCP ile bağlandı (alarm)
  DISCONNECTED,
  STATIC_TO_DHCP,          // statik deneme zaman aşımı/geçersiz → DHCP denemesi
  TIMEOUT_TO_AP,           // bağlanamadı → AP açıldı
  CONFIG_CHANGED,
};

struct NetInput {
  bool configured = false;      // SSID tanımlı
  bool static_enabled = false;
  bool static_valid = false;    // IP/maske/ağ geçidi tutarlı (sunucu doğrulaması)
  bool sta_connected = false;   // WL_CONNECTED + IP
};

struct NetActions {
  bool start_ap = false;
  bool stop_ap = false;
  bool begin_sta = false;       // WiFi.begin (önce disconnect)
  bool use_static = false;      // begin_sta ile: statik yapılandırma uygula (değilse DHCP)
  bool stop_sta = false;        // denemeyi bırak (AP kanalını sabit tutmak için)
  bool start_services = false;  // mDNS, OTA, NTP
  bool stop_services = false;
  NetEvent ev = NetEvent::NONE;
};

class NetFsm {
 public:
  void setParams(const NetFsmParams& p) { p_ = p; }
  void requestReconnect() { changed_ = true; }   // SSID/parola/statik ayar değişti veya Wi-Fi silindi
  NetActions step(const NetInput& in, uint32_t now_ms);

  NetPhase phase() const { return phase_; }
  bool apActive() const { return ap_; }
  bool staticFailed() const { return static_failed_; }
  bool online() const { return phase_ == NetPhase::ONLINE; }
  // Bir sonraki denemeye kalan süre (ms); WAITING dışında 0
  uint32_t retryInMs(uint32_t now_ms) const;

 private:
  void beginAttempt(const NetInput& in, uint32_t now, NetActions& a);
  NetFsmParams p_;
  NetPhase phase_ = NetPhase::WAITING;
  bool ap_ = false;
  bool static_failed_ = false;
  bool changed_ = true;          // ilk step boot denemesini başlatır
  uint32_t t0_ = 0;
};

// IPv4 yardımcıları (sunucu tarafı ağ doğrulaması; scada-device-baseline §5)
bool parseIpv4(const char* s, uint32_t& out);            // "a.b.c.d" → host sırası; boş/bozuk → false
// Statik yapılandırma tutarlılığı: bitişik maske, IP ve ağ geçidi aynı alt ağda, IP ≠ ağ geçidi,
// IP ağ/yayın adresi değil. err: Türkçe açıklama (UI'da alan mesajı).
bool validStaticIpv4(uint32_t ip, uint32_t mask, uint32_t gw, const char** err);

}  // namespace cc
