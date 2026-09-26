#include "storage.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>
#include <cstring>
#include <ctime>
#include "net_manager.h"
#include "tasks.h"

namespace storage {

namespace {

// ---------------------------------------------------------------- LittleFS sürücüsü
class LfsBlob : public cc::BlobFs {
 public:
  bool read(const char* name, uint8_t* buf, size_t cap, size_t& len) override {
    char p[40];
    path(name, p);
    if (!LittleFS.exists(p)) return false;
    File f = LittleFS.open(p, "r");
    if (!f) return false;
    const size_t sz = f.size();
    if (sz > cap) { f.close(); return false; }
    len = f.read(buf, sz);
    f.close();
    return len == sz;
  }
  bool write(const char* name, const uint8_t* data, size_t len) override {
    char p[40];
    path(name, p);
    File f = LittleFS.open(p, "w");
    if (!f) return false;
    const size_t w = f.write(data, len);
    f.close();
    return w == len;
  }
  bool remove(const char* name) override {
    char p[40];
    path(name, p);
    return !LittleFS.exists(p) || LittleFS.remove(p);
  }
  bool rename(const char* from, const char* to) override {
    char a[40], b[40];
    path(from, a);
    path(to, b);
    if (!LittleFS.exists(a)) return false;
    if (LittleFS.exists(b)) LittleFS.remove(b);
    return LittleFS.rename(a, b);
  }

 private:
  static void path(const char* n, char out[40]) { snprintf(out, 40, "/%s", n); }
};

LfsBlob g_fs;
cc::GenStore g_cfg_store(g_fs, "config", 3072);
cc::GenStore g_alm_store(g_fs, "alarms", 256);
cc::GenStore g_cnt_store(g_fs, "counters", 256);
cc::GenStore g_ev_store(g_fs, "events", 3072);
cc::GenStore g_prg_store(g_fs, "programs", 1024);

SemaphoreHandle_t g_mtx = nullptr;          // g_status, g_cnt, g_ev (okuyucular: web/MQTT)
Status g_status;
cc::CounterRec g_cnt;
cc::EventStoreRec g_ev;
std::atomic<bool> g_flush_req{false}, g_erase_req{false}, g_erased{false};
SemaphoreHandle_t g_flush_done = nullptr;

struct Lock {
  Lock() { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { if (g_mtx) xSemaphoreGive(g_mtx); }
};

void noteErr(const char* what, cc::StoreResult r) {
  Lock l;
  ++g_status.errors;
  snprintf(g_status.last_error, sizeof g_status.last_error, "%s: %s", what, cc::name(r));
}
void noteSave() { Lock l; ++g_status.saves; g_status.last_save_ms = millis(); }

// Program kaydı: sürüm + yapı boyutu (düzen değişirse eski kayıt okunmaz) + etkinlik + liste
struct ProgHdr {
  uint16_t version = 1, item_size = sizeof(cc::Program);
  uint8_t n = 0, enabled = 1, pad[2] = {0, 0};
};
struct CntHdr { uint16_t version = 1, size = sizeof(cc::CounterRec); };

uint8_t g_buf[3072];                        // yalnız StorageTask / setup

// ---- StorageTask durumu
uint32_t g_cfg_crc = 0, g_cfg_changed_ms = 0, g_cfg_dirty_since = 0;
uint32_t g_alm_crc = 0, g_alm_saved_ms = 0;
bool g_alm_dirty = false;
uint32_t g_cnt_saved_ms = 0;
bool g_cnt_dirty = false;
uint32_t g_ev_last_seq = 0, g_ev_saved_ms = 0;
bool g_ev_dirty = false;
uint32_t g_prg_crc = 0;
uint32_t g_boot_seq = 0;                    // bu açılıştan önceki son olay seq'i

bool saveConfig(const cc::Config& c) {
  char* txt = reinterpret_cast<char*>(g_buf);
  const uint32_t rev = g_cfg_store.generation() + 1;
  const size_t n = cc::configSerialize(c, rev, txt, sizeof g_buf);
  if (!n) { noteErr("config", cc::StoreResult::TOO_LARGE); return false; }
  const cc::StoreResult r = g_cfg_store.save(g_buf, n);
  if (r != cc::StoreResult::OK) { noteErr("config", r); return false; }
  { Lock l; g_status.config_rev = g_cfg_store.generation(); g_status.config_loaded = true; g_status.config_corrupt = false; }
  noteSave();
  return true;
}

bool saveAlarms(const cc::AlarmPersist& a) {
  const cc::StoreResult r = g_alm_store.save(reinterpret_cast<const uint8_t*>(&a), sizeof a);
  if (r != cc::StoreResult::OK) { noteErr("alarms", r); return false; }
  noteSave();
  return true;
}

bool saveCounters() {
  CntHdr h;
  cc::CounterRec c;
  { Lock l; c = g_cnt; }
  memcpy(g_buf, &h, sizeof h);
  memcpy(g_buf + sizeof h, &c, sizeof c);
  const cc::StoreResult r = g_cnt_store.save(g_buf, sizeof h + sizeof c);
  if (r != cc::StoreResult::OK) { noteErr("counters", r); return false; }
  noteSave();
  return true;
}

bool saveEvents() {
  cc::EventStoreRec* e = reinterpret_cast<cc::EventStoreRec*>(g_buf);
  static_assert(sizeof(cc::EventStoreRec) <= sizeof g_buf, "events tamponu");
  { Lock l; memcpy(e, &g_ev, sizeof g_ev); }
  const cc::StoreResult r = g_ev_store.save(g_buf, sizeof(cc::EventStoreRec));
  if (r != cc::StoreResult::OK) { noteErr("events", r); return false; }
  noteSave();
  return true;
}

bool savePrograms(const cc::Program* list, uint8_t n, bool enabled) {
  ProgHdr h;
  h.n = n;
  h.enabled = enabled ? 1 : 0;
  memcpy(g_buf, &h, sizeof h);
  memcpy(g_buf + sizeof h, list, sizeof(cc::Program) * n);
  const cc::StoreResult r = g_prg_store.save(g_buf, sizeof h + sizeof(cc::Program) * n);
  if (r != cc::StoreResult::OK) { noteErr("programs", r); return false; }
  noteSave();
  return true;
}

// Bir tur: kirli olanları politikaya göre yaz. force: reboot/OTA öncesi hepsi
void cycle(uint32_t now, bool force) {
  // Çekirdekten kısa kopya
  cc::Config c;
  cc::AlarmPersist ap;
  cc::Program progs[cc::kMaxPrograms];
  uint8_t np = 0;
  bool pen = true, heating = false, clock = false;
  int64_t lmin = 0;
  uint32_t sw[4];
  uint64_t onms[4];
  cc::Event ev[16];
  uint8_t nev = 0;
  uint32_t up_now = 0;
  if (!app::coreLock(100)) return;
  c = app::core().config();
  app::core().alarms().exportPersist(ap);
  np = app::core().programCount();
  for (uint8_t i = 0; i < np && i < cc::kMaxPrograms; ++i) progs[i] = app::core().programs()[i];
  pen = app::core().programsEnabled();
  heating = app::core().snapshot().heating_active;
  clock = net::clockValid();
  lmin = app::core().localMinutes();
  for (uint8_t k = 0; k < 4; ++k) { sw[k] = app::core().guard().switchCount(k); onms[k] = app::core().guard().onTimeMs(k); }
  const auto& ring = app::core().events();
  for (uint16_t i = 0; i < ring.size() && nev < 16; ++i) {
    const cc::Event& e = ring.at(i);
    if (e.seq > g_ev_last_seq && (uint8_t)e.sev >= (uint8_t)cc::Severity::WARNING) ev[nev++] = e;
  }
  up_now = app::core().uptimeMs() / 1000;
  app::coreUnlock();

  // ---- konfigürasyon: 5 s sessizlik veya 60 s azami
  const size_t tl = cc::configSerialize(c, 0, reinterpret_cast<char*>(g_buf), sizeof g_buf);
  const uint32_t crc = cc::crc32(g_buf, tl);
  if (crc != g_cfg_crc) {
    if (!g_cfg_dirty_since) g_cfg_dirty_since = now ? now : 1;
    g_cfg_changed_ms = now;
    g_cfg_crc = crc;
  }
  if (g_cfg_dirty_since && (force || now - g_cfg_changed_ms >= 5000 || now - g_cfg_dirty_since >= 60000)) {
    if (saveConfig(c)) g_cfg_dirty_since = 0;
  }

  // ---- kilitli alarmlar
  const uint32_t acrc = cc::crc32(&ap, sizeof ap);
  if (acrc != g_alm_crc) { g_alm_crc = acrc; g_alm_dirty = true; }
  if (g_alm_dirty && (force || now - g_alm_saved_ms >= 5000)) {
    if (saveAlarms(ap)) { g_alm_dirty = false; g_alm_saved_ms = now; }
  }

  // ---- sayaçlar + günlük geçmiş (1 s tik)
  bool rolled = false;
  {
    Lock l;
    const int32_t day = clock ? (int32_t)(lmin >= 0 ? lmin / 1440 : (lmin - 1439) / 1440) : -1;
    rolled = cc::historyTick(g_cnt, clock, day, heating ? 1 : 0);
    for (uint8_t k = 0; k < 4; ++k) {
      if (g_cnt.sw[k] != sw[k] || g_cnt.on_ms[k] != onms[k]) g_cnt_dirty = true;
      g_cnt.sw[k] = sw[k];
      g_cnt.on_ms[k] = onms[k];
    }
    if (heating) g_cnt_dirty = true;
  }
  if ((g_cnt_dirty && (force || now - g_cnt_saved_ms >= 15u * 60u * 1000u)) || rolled) {
    if (saveCounters()) { g_cnt_dirty = false; g_cnt_saved_ms = now; }
  }

  // ---- olaylar (WARNING+), en çok 60 s'de bir
  if (nev) {
    const bool cv = net::clockValid();
    const int64_t t = (int64_t)time(nullptr);
    Lock l;
    for (uint8_t i = 0; i < nev; ++i) {
      cc::eventStorePush(g_ev, ev[i], cv ? t - (int64_t)(up_now - ev[i].up_s) : 0);
      g_ev_last_seq = ev[i].seq;
    }
    g_ev_dirty = true;
  }
  if (g_ev_dirty && (force || now - g_ev_saved_ms >= 60000)) {
    if (saveEvents()) { g_ev_dirty = false; g_ev_saved_ms = now; }
  }

  // ---- programlar
  uint32_t pcrc = cc::crc32(progs, sizeof(cc::Program) * np, np);
  pcrc = cc::crc32(&pen, 1, pcrc);
  if (pcrc != g_prg_crc) {
    if (savePrograms(progs, np, pen)) g_prg_crc = pcrc;
  }
}

void task(void*) {
  uint32_t last = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));
    const uint32_t now = millis();
    if (g_erase_req.exchange(false)) {
      // Kilitli alarmlar korunur (fabrika ayarı güvenlik kilidini kaldırmaz; reset ayrı ve koşula bağlıdır)
      static const char* const bases[] = {"config", "counters", "events", "programs"};
      static const char* const ext[] = {"bin", "bak", "tmp"};
      bool ok = true;
      char n[24];
      for (const char* b : bases)
        for (const char* e : ext) { snprintf(n, sizeof n, "%s.%s", b, e); ok = g_fs.remove(n) && ok; }
      g_erased.store(ok);                     // sonrasında yazım yok: yeniden başlatmada varsayılanlar
      xSemaphoreGive(g_flush_done);
      continue;
    }
    const bool force = g_flush_req.exchange(false);
    if (!force && now - last < 1000) continue;
    last = now;
    if (g_status.fs_ok && !g_erased.load()) cycle(now, force);
    if (force) xSemaphoreGive(g_flush_done);
  }
}

}  // namespace

void bootLoad(cc::Config& cfg, cc::BootInfo& boot) {
  g_mtx = xSemaphoreCreateMutex();
  g_flush_done = xSemaphoreCreateBinary();
  // Biçimlendirme yalnız hiç biçimlendirilmemiş cihazda (NVS bayrağı yok). Önceden çalışmış FS açılamazsa
  // biçimlendirilmez: kayıtlar korunur, cihaz varsayılan konfigürasyon + CONFIGURATION_ERROR ile çalışır.
  Preferences p;
  p.begin("stor", false);
  const bool inited = p.getBool("fsinit", false);
  bool ok = LittleFS.begin(false, "/littlefs", 5, "littlefs");
  if (!ok && !inited) {
    ok = LittleFS.format() && LittleFS.begin(false, "/littlefs", 5, "littlefs");
    if (ok) g_status.formatted_now = true;
  }
  if (ok && !inited) p.putBool("fsinit", true);
  p.end();
  g_status.fs_ok = ok;
  if (!ok) {
    snprintf(g_status.last_error, sizeof g_status.last_error, "Dosya sistemi açılamadı");
    boot.config_error = true;
    return;
  }
  // ---- konfigürasyon
  size_t n = 0;
  const cc::StoreResult r = g_cfg_store.load(g_buf, sizeof g_buf - 1, n);
  if (r == cc::StoreResult::OK) {
    cc::Config cand = cfg;
    const cc::ConfigParse pr = cc::configParse(reinterpret_cast<const char*>(g_buf), n, cand);
    if (pr.ok && cc::validate(cand).ok()) {
      cfg = cand;
      g_status.config_loaded = true;
      g_status.config_rev = g_cfg_store.generation();
    } else {
      g_status.config_corrupt = true;
      snprintf(g_status.last_error, sizeof g_status.last_error, "config: %s", pr.ok ? "doğrulama" : pr.bad_key);
      boot.config_error = true;              // güvenli varsayılan + CONFIGURATION_ERROR + ısıtma kilidi
    }
  } else if (r == cc::StoreResult::CORRUPT) {
    g_status.config_corrupt = true;
    snprintf(g_status.last_error, sizeof g_status.last_error, "config: CORRUPT");
    boot.config_error = true;
  }
  // ---- kilitli alarmlar
  cc::AlarmPersist ap;
  if (g_alm_store.load(reinterpret_cast<uint8_t*>(&ap), sizeof ap, n) == cc::StoreResult::OK && n == sizeof ap) {
    boot.restore_alarms = true;
    boot.alarms = ap;
  }
}

void afterCoreBegin(bool fault_reset) {
  if (!g_status.fs_ok) return;
  size_t n = 0;
  // ---- sayaçlar
  if (g_cnt_store.load(g_buf, sizeof g_buf, n) == cc::StoreResult::OK && n == sizeof(CntHdr) + sizeof(cc::CounterRec)) {
    CntHdr h;
    memcpy(&h, g_buf, sizeof h);
    if (h.version == 1 && h.size == sizeof(cc::CounterRec)) memcpy(&g_cnt, g_buf + sizeof h, sizeof g_cnt);
  }
  ++g_cnt.boots;
  if (fault_reset) ++g_cnt.fault_boots_total;
  g_status.boots = g_cnt.boots;
  g_cnt_dirty = true;
  g_cnt_saved_ms = 0;                        // ilk tur boot sayacını yazar
  // ---- olaylar
  if (g_ev_store.load(g_buf, sizeof g_buf, n) == cc::StoreResult::OK && n == sizeof(cc::EventStoreRec)) {
    memcpy(&g_ev, g_buf, sizeof g_ev);
    if (g_ev.version != 1 || g_ev.n > cc::kPersistEvents) g_ev = cc::EventStoreRec();
  }
  uint32_t last_seq = g_ev.n ? g_ev.ev[g_ev.n - 1].seq : 0;
  // ---- programlar
  bool prog_ok = false;
  cc::Program progs[cc::kMaxPrograms];
  ProgHdr h;
  if (g_prg_store.load(g_buf, sizeof g_buf, n) == cc::StoreResult::OK && n >= sizeof h) {
    memcpy(&h, g_buf, sizeof h);
    if (h.version == 1 && h.item_size == sizeof(cc::Program) && h.n <= cc::kMaxPrograms && n == sizeof h + h.n * sizeof(cc::Program)) {
      memcpy(progs, g_buf + sizeof h, sizeof(cc::Program) * h.n);
      prog_ok = true;
    }
  }
  if (app::coreLock(500)) {
    for (uint8_t k = 0; k < 4; ++k) app::core().restoreCounters(k, g_cnt.sw[k], g_cnt.on_ms[k]);
    app::core().restoreEventSeq(last_seq);   // olay seq'i boot'lar arası tekdüze artar
    if (prog_ok) prog_ok = app::core().restorePrograms(progs, h.n, h.enabled != 0);
    app::coreUnlock();
  }
  g_ev_last_seq = last_seq;
  g_boot_seq = last_seq;
  // Açılışta değişmemiş kayıtlar yeniden yazılmaz: başlangıç özetleri çekirdeğin şu anki durumundan
  if (app::coreLock(500)) {
    const cc::Config c = app::core().config();
    cc::AlarmPersist ap;
    app::core().alarms().exportPersist(ap);
    app::coreUnlock();
    if (g_status.config_loaded) g_cfg_crc = cc::crc32(g_buf, cc::configSerialize(c, 0, reinterpret_cast<char*>(g_buf), sizeof g_buf));
    g_alm_crc = cc::crc32(&ap, sizeof ap);
  }
  if (prog_ok) g_prg_crc = cc::crc32(&h.enabled, 1, cc::crc32(progs, sizeof(cc::Program) * h.n, h.n));
}

void begin() {
  xTaskCreatePinnedToCore(task, "storage", 6144, nullptr, 1, nullptr, 0);
}

bool flushNow(uint32_t timeout_ms) {
  if (!g_status.fs_ok || !g_flush_done) return false;
  xSemaphoreTake(g_flush_done, 0);
  g_flush_req.store(true);
  return xSemaphoreTake(g_flush_done, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

bool factoryErase(uint32_t timeout_ms) {
  if (!g_status.fs_ok || !g_flush_done) return false;
  xSemaphoreTake(g_flush_done, 0);
  g_erase_req.store(true);
  return xSemaphoreTake(g_flush_done, pdMS_TO_TICKS(timeout_ms)) == pdTRUE && g_erased.load();
}

Status status() { Lock l; return g_status; }
bool configLoaded() { Lock l; return g_status.config_loaded; }
cc::CounterRec counters() { Lock l; return g_cnt; }

uint8_t persistedEvents(cc::Event* out, int64_t* ts, uint8_t cap) {
  Lock l;
  uint8_t k = 0;
  for (uint8_t i = 0; i < g_ev.n && k < cap; ++i)
    if (g_ev.ev[i].seq <= g_boot_seq) { out[k] = g_ev.ev[i]; ts[k] = g_ev.ts[i]; ++k; }   // yalnız önceki açılışlar
  return k;
}

}  // namespace storage
