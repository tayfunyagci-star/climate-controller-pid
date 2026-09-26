// Kalıcı depo (F3) — ADR-007. Flash'ın tek sahibi StorageTask'tır (LittleFS, "littlefs" bölümü).
//   config   : çekirdek konfigürasyonu (metin, nesil kontrollü); değişimden 5 s sonra, en geç 60 s'de yazılır
//   alarms   : kilitli alarmlar + onay durumu (AlarmPersist); geçişte (≥ 5 s aralık)
//   counters : çalışma saati, anahtarlama, boot sayıları, günlük ısıtma geçmişi; 15 dk'da bir + gün devri
//   events   : WARNING+ olayların son 64'ü (epoch damgalı); en çok 60 s'de bir
//   programs : yerel programlar + etkinlik; değişimde
// Güç kesintisinde en çok: sayaçlarda 15 dk, olaylarda 60 s, konfigürasyonda 60 s kayıp.
// Açılamayan dosya sistemi otomatik biçimlendirilmez (yalnız hiç biçimlendirilmemiş yeni cihazda).
#pragma once
#include <cstdint>
#include "core_api.h"
#include "cc_store.h"

namespace storage {

struct Status {
  bool fs_ok = false, formatted_now = false;
  bool config_loaded = false, config_corrupt = false;
  uint32_t config_rev = 0;
  uint32_t saves = 0, errors = 0;
  uint32_t last_save_ms = 0;
  char last_error[48] = "";
  uint32_t boots = 0;
};

// setup(): çekirdekten önce. cfg: taban (f2Config) → kayıtlı konfigürasyon üzerine; boot: kilitli alarmlar
void bootLoad(cc::Config& cfg, cc::BootInfo& boot);
// setup(): core.begin'den sonra, görevlerden önce — sayaçlar, olay sırası, programlar
void afterCoreBegin(bool fault_reset);
void begin();                                // StorageTask
bool flushNow(uint32_t timeout_ms);          // reboot/OTA öncesi: kirli her şeyi yaz ve bekle
Status status();
bool configLoaded();
cc::CounterRec counters();                   // günlük geçmiş (MQTT history) ve tanı
// Önceki boot'lardan WARNING+ olaylar (web /api/events). n: çıktı adedi
uint8_t persistedEvents(cc::Event* out, int64_t* ts, uint8_t cap);

}  // namespace storage
