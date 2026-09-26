// Kalıcı depo mantığı (F3) — ADR-007, CONFIGURATION_MODEL §1, scada-cihaz-standardi cihaz-temeli §4.
// Saf C++: dosya sistemi soyut arayüzdür (cihazda LittleFS, testte bellek içi + güç kesintisi enjeksiyonu).
//
// GenStore: her kayıt nesil numaralı, CRC32'li çerçevedir. Yazım: <ad>.tmp yaz → geri oku ve doğrula →
// eski birincili yedeğe taşı → tmp'yi birincil yap. Yükleme: birincil / yedek / tmp içinden geçerli ve en
// yüksek nesilli çerçeve (deterministik). Hangi adımda kesinti olursa olsun en az bir sağlam kopya kalır;
// sağlam tek kopya olan .tmp asla silinmez. Bozuk kayıt fabrika sıfırlaması tetiklemez.
#pragma once
#include <cstddef>
#include <cstdint>
#include "cc_alarm.h"
#include "cc_config.h"
#include "cc_events.h"

namespace cc {

uint32_t crc32(const void* data, size_t len, uint32_t seed = 0);

class BlobFs {
 public:
  virtual ~BlobFs() = default;
  virtual bool read(const char* name, uint8_t* buf, size_t cap, size_t& len) = 0;   // false: yok/okunamadı
  virtual bool write(const char* name, const uint8_t* data, size_t len) = 0;        // tüm dosya
  virtual bool remove(const char* name) = 0;                                          // yoksa true
  virtual bool rename(const char* from, const char* to) = 0;                          // hedef varsa üzerine
};

enum class StoreResult : uint8_t { OK, EMPTY, CORRUPT, IO_ERROR, TOO_LARGE };
const char* name(StoreResult);

class GenStore {
 public:
  static constexpr size_t kHeader = 16;          // magic, nesil, uzunluk, crc
  GenStore(BlobFs& fs, const char* base, size_t max_payload);
  // Yükle: en yüksek nesilli geçerli kopya. EMPTY: hiç dosya yok; CORRUPT: dosya var ama geçerli kopya yok.
  StoreResult load(uint8_t* payload, size_t cap, size_t& len);
  StoreResult save(const uint8_t* payload, size_t len);
  uint32_t generation() const { return gen_; }

 private:
  bool readFrame(const char* file, uint8_t* payload, size_t cap, size_t& len, uint32_t& gen, bool& present);
  BlobFs& fs_;
  char prim_[32], back_[32], tmp_[32];
  size_t max_;
  uint32_t gen_ = 0;
};

// ---------------------------------------------------------------- konfigürasyon belgesi
// Metin: "schema=1\nrev=<n>\n<anahtar>=<değer>\n…". Eksik anahtar varsayılan kalır (şema göçü), bilinmeyen
// anahtar yok sayılır (sayılır), geçersiz değer belgeyi reddeder (diğer nesle düşülür). Doğrulama (V1–V17)
// çağıranın işidir: validate() aynı fonksiyonla web/MQTT/boot'ta.
constexpr uint32_t kConfigSchema = 1;
size_t configSerialize(const Config& c, uint32_t rev, char* out, size_t cap);
struct ConfigParse {
  bool ok = false;
  uint32_t rev = 0, schema = 0;
  uint16_t applied = 0, unknown = 0, missing = 0;
  char bad_key[40] = "";
};
ConfigParse configParse(const char* text, size_t len, Config& io);   // io: taban (varsayılan) → üzerine

// ---------------------------------------------------------------- sayaçlar + günlük ısıtma geçmişi
constexpr uint8_t kHistDays = 7;
struct CounterRec {
  uint32_t version = 1;
  uint32_t sw[4] = {0, 0, 0, 0};
  uint64_t on_ms[4] = {0, 0, 0, 0};
  uint32_t boots = 0, fault_boots_total = 0;
  int32_t today_day = -1;            // yerel gün numarası (saat geçersizken -1)
  uint32_t today_s = 0;              // bugünkü ısıtma saniyesi
  int32_t hist_end = -1;             // hist[0]'ın günü (dün)
  uint16_t hist_min[kHistDays] = {};
  int32_t since_day = -1;            // kayıt başlangıç günü (bilinmiyor = -1)
};

// Isıtma süresi birikimi ve gün devri (takvim gün serisi; geri saatte geçmiş kaydırılmaz).
// Dönüş: gün devrinde true (geçmiş penceresi değişti → yayın).
bool historyTick(CounterRec& r, bool clock_valid, int32_t day, uint32_t heating_s);

// ---------------------------------------------------------------- kalıcı olay halkası (WARNING+)
constexpr uint8_t kPersistEvents = 64;
struct EventStoreRec {
  uint32_t version = 1;
  uint8_t n = 0;
  Event ev[kPersistEvents];
  int64_t ts[kPersistEvents] = {};   // epoch s (kayıt anında saat geçerliyse; 0 = bilinmiyor)
};
void eventStorePush(EventStoreRec& r, const Event& e, int64_t ts);   // doluysa en eskiyi düşürür (eski → yeni)

}  // namespace cc
