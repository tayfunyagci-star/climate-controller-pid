// F2 ağ ve saat — Wi-Fi istasyonu + SNTP (checklist 16: NTP). Kontrol ağdan bağımsızdır:
// buradaki durumlar yalnız alarm/olay (setNetStatus) ve program saati (setClock) üretir.
// Wi-Fi kimliği F2'de seri konsoldan NVS'e yazılır; F3/F4'te konfigürasyon deposu ve web kurulumu devralır.
#pragma once
#include <cstdint>

namespace net {

void begin();                 // NVS'ten kimlik; varsa bağlan; SNTP yapılandır
void service();               // ~1 s: durum izleme (bloklamaz)
bool wifiConfigured();
bool wifiOk();
int8_t rssi();
const char* ipString();
bool clockValid();            // en az bir SNTP eşitlemesi + makul epoch
int64_t epochUtc();
uint32_t lastSyncAgeS();
bool setCredentials(const char* ssid, const char* pass);  // NVS + yeniden bağlan; parola loglanmaz
bool clearCredentials();
bool setNtpServer(const char* host);
const char* ntpServer();
const char* ssid();

}  // namespace net
