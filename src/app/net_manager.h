// Ağ yöneticisi — SCADA ailesi bağlantı yaşam döngüsü (docs/NETWORK.md, lib/core/cc_netfsm):
// ilk açılışta kurulum AP'si (SCADA_AP_<id>, 192.168.4.1, captive DNS), Wi-Fi seçimi web arayüzünden,
// statik IP → DHCP düşüşü, bağlanamayınca AP + 5 dk'da bir arka plan denemesi, mDNS, ArduinoOTA, SNTP.
// NetTask (çekirdek 0) WiFi/DNS/HTTP/OTA nesnelerinin tek sahibidir. Kontrol ağdan bağımsızdır:
// buradaki hiçbir durum proses çıkışlarını değiştirmez (MQTT/Wi-Fi LOST ≠ LOCAL CONTROL LOST).
#pragma once
#include <cstdint>

namespace net {

constexpr const char* kApPass = "12345678";   // SCADA ailesi varsayılanı (etiket); CHANGELOG F2.2 notu
constexpr const char* kApIp = "192.168.4.1";

struct NetSettings {
  char adn[65] = "Kulübe İklim";      // görünen cihaz adı (yalnız UI)
  char mdns[64] = "kulube-iklim";
  bool st = false;                     // statik IP
  char ip[16] = "", gw[16] = "", sn[16] = "", d1[16] = "", d2[16] = "";
  char ntp[64] = "pool.ntp.org";
};

struct Status {
  bool ap_mode = false, sta_ok = false, configured = false, static_failed = false;
  bool services = false, mdns_ok = false, ota_ready = false, clock_valid = false;
  int8_t rssi = 0;
  uint8_t phase = 0;                   // cc::NetPhase
  uint32_t retry_s = 0;
  uint32_t reconnects = 0;
  // Kurulum/kurtarma akışı (docs/NETWORK.md §3): UI sonucu yalnız bu cihaz kanıtlarına dayandırır
  bool handover = false;               // STA bağlı + kurulum ağı devir için açık
  uint32_t ap_close_s = 0;             // devirde kurulum ağının kapanmasına kalan süre
  uint32_t try_seq = 0;                // ayar kaynaklı deneme sayacı
  uint8_t result = 0;                  // cc::NetResult
  uint8_t fail = 0;                    // cc::NetFail (son başarısız deneme)
  uint16_t fail_code = 0;              // ham wifi_err_reason_t (tanı)
  uint8_t ap_clients = 0;              // kurulum ağındaki istemci sayısı
  char ip[16] = "0.0.0.0";
  char sta_ip[16] = "";                // yalnız STA bağlıyken
  char ap_name[24] = "";
  char ssid[33] = "";
  char note[96] = "";                  // ör. "Statik IP başarısız; DHCP ile bağlandı"
};

void begin();                          // NVS'ten yükle, NetTask'ı başlat
Status status();
NetSettings settings();
bool passSet();
bool otaPasswordSet();

// Saat (programlar ve kontrol görevi)
bool clockValid();
int64_t epochUtc();
bool wifiConfigured();
bool wifiOk();

// Yazım (web ve konsol aynı yol): aday bütünüyle doğrulanır; hata = hiçbir şey değişmez.
// ssid == nullptr: kablosuz kimlik değişmez. pass == nullptr: parola korunur; "" = açık ağ.
bool apply(const NetSettings& n, const char* ssid, const char* pass, const char** err, const char** field, bool* reconnect);
bool resetWifi(const char** err);      // kimlik silinir, statik kapanır, yeniden başlatmadan AP
bool retryNow();                       // kayıtlı ağı hemen yeniden dene (SSID yoksa false)
bool finishSetup();                    // devirdeki kurulum ağını kapat (devir yoksa false)
bool setOtaPassword(const char* pw, const char** err);   // "" = kaldır (OTA parolasız açık kalır, D-17)
void requestReboot(uint32_t delay_ms); // yanıt gönderildikten sonra güvenli yeniden başlatma

}  // namespace net
