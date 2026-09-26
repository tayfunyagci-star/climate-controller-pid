// Kalıcı depo (F3): nesil kontrollü atomik yazım, güç kesintisi enjeksiyonu, konfigürasyon belgesi,
// günlük ısıtma geçmişi, kalıcı olay halkası.
#include <unity.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "cc_config_validate.h"
#include "cc_store.h"

using namespace cc;

void setUp() {}
void tearDown() {}

// Bellek içi dosya sistemi: `cut` işlemden sonra güç kesilir (o işlem yarım kalabilir, sonrakiler başarısız)
struct MemFs : BlobFs {
  std::map<std::string, std::vector<uint8_t>> files;
  int ops = 0, cut = -1;
  bool partial = true;
  bool dead() { return cut >= 0 && ops > cut; }
  bool read(const char* n, uint8_t* b, size_t cap, size_t& len) override {
    auto it = files.find(n);
    if (it == files.end() || it->second.size() > cap) return false;
    memcpy(b, it->second.data(), it->second.size());
    len = it->second.size();
    return true;
  }
  bool write(const char* n, const uint8_t* d, size_t len) override {
    ++ops;
    if (dead()) return false;
    if (cut >= 0 && ops == cut && partial) { files[n] = std::vector<uint8_t>(d, d + len / 2); ops = cut + 1; return false; }
    files[n] = std::vector<uint8_t>(d, d + len);
    return true;
  }
  bool remove(const char* n) override { ++ops; if (dead()) return false; files.erase(n); return true; }
  bool rename(const char* a, const char* b) override {
    ++ops;
    if (dead()) return false;
    auto it = files.find(a);
    if (it == files.end()) return false;
    files[b] = it->second;
    files.erase(a);
    return true;
  }
};

static std::vector<uint8_t> pay(const char* s) { return std::vector<uint8_t>(s, s + strlen(s)); }

void test_empty_then_roundtrip() {
  MemFs fs;
  GenStore st(fs, "cfg", 1024);
  uint8_t buf[1024];
  size_t n = 0;
  TEST_ASSERT_EQUAL(StoreResult::EMPTY, st.load(buf, sizeof buf, n));
  auto a = pay("birinci");
  TEST_ASSERT_EQUAL(StoreResult::OK, st.save(a.data(), a.size()));
  auto b = pay("ikinci kayit");
  TEST_ASSERT_EQUAL(StoreResult::OK, st.save(b.data(), b.size()));
  GenStore st2(fs, "cfg", 1024);
  TEST_ASSERT_EQUAL(StoreResult::OK, st2.load(buf, sizeof buf, n));
  TEST_ASSERT_EQUAL(b.size(), n);
  TEST_ASSERT_EQUAL_MEMORY(b.data(), buf, n);
  TEST_ASSERT_EQUAL(2, st2.generation());
}

void test_power_cut_at_every_step_keeps_old_or_new() {
  // Kaydın her dosya işleminde kesinti: yüklemede eski ya da yeni kayıt, asla bozuk/boş değil
  for (int partial = 0; partial < 2; ++partial)
    for (int k = 1; k <= 6; ++k) {
      MemFs fs;
      GenStore st(fs, "cfg", 1024);
      auto a = pay("ESKI-DEGER"), b = pay("YENI-DEGER-UZUN");
      TEST_ASSERT_EQUAL(StoreResult::OK, st.save(a.data(), a.size()));
      TEST_ASSERT_EQUAL(StoreResult::OK, st.save(a.data(), a.size()));   // birincil + yedek var
      fs.ops = 0;
      fs.cut = k;
      fs.partial = partial != 0;
      st.save(b.data(), b.size());
      fs.cut = -1;                        // yeniden açılış
      GenStore re(fs, "cfg", 1024);
      uint8_t buf[1024];
      size_t n = 0;
      TEST_ASSERT_EQUAL_MESSAGE(StoreResult::OK, re.load(buf, sizeof buf, n), "kesinti sonrası kayıt kayboldu");
      const bool old_ok = n == a.size() && !memcmp(buf, a.data(), n);
      const bool new_ok = n == b.size() && !memcmp(buf, b.data(), n);
      TEST_ASSERT_TRUE(old_ok || new_ok);
      // Kesintiden sonra yeni kayıt yine yazılabilir ve kazanır
      auto c = pay("SONRAKI");
      TEST_ASSERT_EQUAL(StoreResult::OK, re.save(c.data(), c.size()));
      GenStore re2(fs, "cfg", 1024);
      TEST_ASSERT_EQUAL(StoreResult::OK, re2.load(buf, sizeof buf, n));
      TEST_ASSERT_EQUAL_MEMORY(c.data(), buf, n);
    }
}

void test_corruption_detected_and_fallback() {
  MemFs fs;
  GenStore st(fs, "cfg", 1024);
  auto a = pay("sagalm-A"), b = pay("sagalm-B");
  st.save(a.data(), a.size());
  st.save(b.data(), b.size());
  fs.files["cfg.bin"][GenStore::kHeader + 2] ^= 0x40;   // birincilde bit hatası
  GenStore re(fs, "cfg", 1024);
  uint8_t buf[1024];
  size_t n = 0;
  TEST_ASSERT_EQUAL(StoreResult::OK, re.load(buf, sizeof buf, n));
  TEST_ASSERT_EQUAL_MEMORY(a.data(), buf, n);            // yedeğe düşer
  fs.files["cfg.bak"][0] ^= 1;
  GenStore re2(fs, "cfg", 1024);
  TEST_ASSERT_EQUAL(StoreResult::CORRUPT, re2.load(buf, sizeof buf, n));   // dosya var, geçerli kopya yok
}

void test_config_roundtrip_and_migration() {
  Config c;
  c.temperature_setpoint = 23.5f;
  c.boost_minutes = 45;
  c.antifreeze_enabled = false;
  c.pid_mode = PidMode::PID;
  c.pid_kd = 2.5f;
  static char txt[4096];
  const size_t n = configSerialize(c, 7, txt, sizeof txt);
  TEST_ASSERT_TRUE(n > 0);
  Config d;
  ConfigParse r = configParse(txt, n, d);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(7, r.rev);
  TEST_ASSERT_EQUAL(0, r.missing);
  TEST_ASSERT_EQUAL_FLOAT(23.5f, d.temperature_setpoint);
  TEST_ASSERT_EQUAL(45, d.boost_minutes);
  TEST_ASSERT_FALSE(d.antifreeze_enabled);
  TEST_ASSERT_TRUE(d.pid_mode == PidMode::PID);
  TEST_ASSERT_TRUE(validate(d).ok());
  // Eski şema: eksik alan varsayılan, bilinmeyen alan yok sayılır
  const char* old = "schema=1\nrev=3\ntemperature_setpoint=19\neski_alan=5\n";
  Config e;
  r = configParse(old, strlen(old), e);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(1, r.unknown);
  TEST_ASSERT_TRUE(r.missing > 50);
  TEST_ASSERT_EQUAL_FLOAT(19.0f, e.temperature_setpoint);
  TEST_ASSERT_EQUAL_FLOAT(Config().setpoint_night, e.setpoint_night);
  // Aralık dışı / biçim hatası belgeyi reddeder, girdi değişmez
  const char* bad = "schema=1\nrev=4\ntemperature_setpoint=99\n";
  Config f;
  r = configParse(bad, strlen(bad), f);
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL_STRING("temperature_setpoint", r.bad_key);
  TEST_ASSERT_EQUAL_FLOAT(Config().temperature_setpoint, f.temperature_setpoint);
  const char* future = "schema=9\nrev=1\n";
  TEST_ASSERT_FALSE(configParse(future, strlen(future), f).ok);   // bilinmeyen gelecek şema okunmaz
}

void test_history_rollover() {
  CounterRec r;
  TEST_ASSERT_FALSE(historyTick(r, false, -1, 60));      // saat yok: bugüne birikir
  TEST_ASSERT_EQUAL(60, r.today_s);
  TEST_ASSERT_FALSE(historyTick(r, true, 100, 60));
  TEST_ASSERT_EQUAL(100, r.since_day);
  TEST_ASSERT_EQUAL(120, r.today_s);
  TEST_ASSERT_TRUE(historyTick(r, true, 101, 0));         // gün devri
  TEST_ASSERT_EQUAL(100, r.hist_end);
  TEST_ASSERT_EQUAL(2, r.hist_min[0]);
  historyTick(r, true, 101, 600);
  TEST_ASSERT_TRUE(historyTick(r, true, 104, 0));         // 2 günlük boşluk (kapalı)
  TEST_ASSERT_EQUAL(103, r.hist_end);
  TEST_ASSERT_EQUAL(0, r.hist_min[0]);                    // 103
  TEST_ASSERT_EQUAL(0, r.hist_min[1]);                    // 102
  TEST_ASSERT_EQUAL(10, r.hist_min[2]);                   // 101
  TEST_ASSERT_EQUAL(2, r.hist_min[3]);                    // 100
  TEST_ASSERT_FALSE(historyTick(r, true, 90, 60));        // geri saat: kaydırma yok
  TEST_ASSERT_EQUAL(103, r.hist_end);
  TEST_ASSERT_TRUE(historyTick(r, true, 120, 0));         // pencereden uzun boşluk
  for (int i = 0; i < kHistDays; ++i) TEST_ASSERT_EQUAL(i == 0 ? 0 : 0, r.hist_min[i]);
  TEST_ASSERT_EQUAL(100, r.since_day);                    // başlangıç günü korunur
}

void test_event_store_ring() {
  static EventStoreRec r;
  for (uint32_t i = 1; i <= kPersistEvents + 5; ++i) { Event e; e.seq = i; eventStorePush(r, e, 1000 + i); }
  TEST_ASSERT_EQUAL(kPersistEvents, r.n);
  TEST_ASSERT_EQUAL(6, r.ev[0].seq);
  TEST_ASSERT_EQUAL(kPersistEvents + 5, r.ev[kPersistEvents - 1].seq);
  TEST_ASSERT_EQUAL(1006, (int)r.ts[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty_then_roundtrip);
  RUN_TEST(test_power_cut_at_every_step_keeps_old_or_new);
  RUN_TEST(test_corruption_detected_and_fallback);
  RUN_TEST(test_config_roundtrip_and_migration);
  RUN_TEST(test_history_rollover);
  RUN_TEST(test_event_store_ring);
  return UNITY_END();
}
