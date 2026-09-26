#include "cc_store.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cc {

namespace {
constexpr uint32_t kMagic = 0x31534343;   // "CCS1"

void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i)); }
uint32_t get32(const uint8_t* p) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (8 * i); return v; }

// Tek çerçeve arabelleği (yalnız StorageTask / test; eşzamanlı çağrı yok)
constexpr size_t kMaxPayload = 3072;
uint8_t g_frame[kMaxPayload + GenStore::kHeader];
}  // namespace

uint32_t crc32(const void* data, size_t len, uint32_t seed) {
  uint32_t c = ~seed;
  const uint8_t* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; ++i) {
    c ^= p[i];
    for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

const char* name(StoreResult r) {
  switch (r) {
    case StoreResult::OK: return "OK";
    case StoreResult::EMPTY: return "EMPTY";
    case StoreResult::CORRUPT: return "CORRUPT";
    case StoreResult::IO_ERROR: return "IO_ERROR";
    default: return "TOO_LARGE";
  }
}

GenStore::GenStore(BlobFs& fs, const char* base, size_t max_payload) : fs_(fs), max_(max_payload) {
  if (max_ > kMaxPayload) max_ = kMaxPayload;
  snprintf(prim_, sizeof prim_, "%s.bin", base);
  snprintf(back_, sizeof back_, "%s.bak", base);
  snprintf(tmp_, sizeof tmp_, "%s.tmp", base);
}

// Çerçeveyi g_frame'e okuyup doğrular; yük g_frame + kHeader'dadır
bool GenStore::readFrame(const char* file, uint8_t*, size_t cap, size_t& len, uint32_t& gen, bool& present) {
  size_t n = 0;
  present = false;
  if (!fs_.read(file, g_frame, sizeof g_frame, n)) return false;
  present = true;
  if (n < kHeader || get32(g_frame) != kMagic) return false;
  const uint32_t g = get32(g_frame + 4), l = get32(g_frame + 8), c = get32(g_frame + 12);
  if (l > max_ || l > cap || n != kHeader + l) return false;          // kısa/uzun dosya = bozuk
  if (crc32(g_frame + kHeader, l, g) != c) return false;             // CRC nesli de kapsar
  len = l;
  gen = g;
  return true;
}

StoreResult GenStore::load(uint8_t* payload, size_t cap, size_t& len) {
  const char* files[3] = {prim_, back_, tmp_};
  uint32_t best = 0;
  bool found = false, any = false;
  for (const char* f : files) {
    size_t l = 0;
    uint32_t g = 0;
    bool present = false;
    if (readFrame(f, nullptr, cap, l, g, present)) {
      if (!found || g > best) { best = g; memcpy(payload, g_frame + kHeader, l); len = l; found = true; }
    }
    any = any || present;
  }
  if (found) { gen_ = best; return StoreResult::OK; }
  return any ? StoreResult::CORRUPT : StoreResult::EMPTY;
}

StoreResult GenStore::save(const uint8_t* payload, size_t len) {
  if (len > max_) return StoreResult::TOO_LARGE;
  const uint32_t g = gen_ + 1;
  const uint32_t crc = crc32(payload, len, g);
  put32(g_frame, kMagic);
  put32(g_frame + 4, g);
  put32(g_frame + 8, (uint32_t)len);
  put32(g_frame + 12, crc);
  memcpy(g_frame + kHeader, payload, len);
  const size_t total = kHeader + len;
  if (!fs_.write(tmp_, g_frame, total)) return StoreResult::IO_ERROR;
  // Geri okuma: beklenen boyut + başlık + yük CRC'si (yalnız "yazılan == diskteki" yetmez)
  size_t l = 0;
  uint32_t rg = 0;
  bool present = false;
  if (!readFrame(tmp_, nullptr, max_, l, rg, present) || rg != g || l != len || get32(g_frame + 12) != crc)
    return StoreResult::IO_ERROR;
  // Sıra: eski birincil → yedek, tmp → birincil. Herhangi bir adımda kesinti: tmp veya birincil/yedek sağlam.
  fs_.remove(back_);
  fs_.rename(prim_, back_);                        // birincil yoksa başarısızlık önemsiz
  if (!fs_.rename(tmp_, prim_)) return StoreResult::IO_ERROR;   // tmp sağlam kalır, yüklemede seçilir
  gen_ = g;
  return StoreResult::OK;
}

// ---------------------------------------------------------------- konfigürasyon belgesi
size_t configSerialize(const Config& c, uint32_t rev, char* out, size_t cap) {
  size_t n = 0;
  auto app = [&](const char* s) {
    const size_t l = strlen(s);
    if (n + l + 1 > cap) { n = cap + 1; return; }
    memcpy(out + n, s, l + 1);
    n += l;
  };
  char line[96];
  snprintf(line, sizeof line, "schema=%u\nrev=%u\n", (unsigned)kConfigSchema, (unsigned)rev);
  app(line);
  for (size_t i = 0; i < fieldCount() && n <= cap; ++i) {
    const FieldInfo& f = fieldAt(i);
    const float v = fieldValue(c, f);
    switch (f.kind) {
      case FieldKind::FLOAT: snprintf(line, sizeof line, "%s=%.6g\n", f.key, (double)v); break;
      case FieldKind::INT: snprintf(line, sizeof line, "%s=%ld\n", f.key, (long)lroundf(v)); break;
      case FieldKind::BOOL: snprintf(line, sizeof line, "%s=%s\n", f.key, v != 0 ? "ON" : "OFF"); break;
      case FieldKind::ENUM:
        snprintf(line, sizeof line, "%s=%s\n", f.key, (uint8_t)v < f.enumCount ? f.enumNames[(uint8_t)v] : "?");
        break;
    }
    app(line);
  }
  return n <= cap ? n : 0;
}

ConfigParse configParse(const char* text, size_t len, Config& io) {
  ConfigParse r;
  Config c = io;
  bool seen[256] = {};
  size_t pos = 0;
  char line[128];
  while (pos < len) {
    size_t e = pos;
    while (e < len && text[e] != '\n') ++e;
    const size_t l = e - pos;
    if (l > 0 && l < sizeof line) {
      memcpy(line, text + pos, l);
      line[l] = 0;
      char* eq = strchr(line, '=');
      if (eq) {
        *eq = 0;
        const char* k = line;
        const char* v = eq + 1;
        if (!strcmp(k, "schema")) r.schema = (uint32_t)strtoul(v, nullptr, 10);
        else if (!strcmp(k, "rev")) r.rev = (uint32_t)strtoul(v, nullptr, 10);
        else {
          const FieldInfo* f = findField(k);
          if (!f) { ++r.unknown; }
          else {
            char* base = reinterpret_cast<char*>(&c) + f->offset;
            bool ok = true;
            switch (f->kind) {
              case FieldKind::FLOAT:
              case FieldKind::INT: {
                char* end = nullptr;
                const float x = strtof(v, &end);
                ok = end && *end == 0 && end != v && std::isfinite(x) && x >= f->min - 1e-4f && x <= f->max + 1e-4f;
                if (ok) {
                  if (f->kind == FieldKind::INT) *reinterpret_cast<int32_t*>(base) = (int32_t)lroundf(x);
                  else *reinterpret_cast<float*>(base) = x;
                }
                break;
              }
              case FieldKind::BOOL:
                ok = !strcmp(v, "ON") || !strcmp(v, "OFF");
                if (ok) *reinterpret_cast<bool*>(base) = !strcmp(v, "ON");
                break;
              case FieldKind::ENUM: {
                int idx = -1;
                for (uint8_t i = 0; i < f->enumCount; ++i) if (!strcmp(v, f->enumNames[i])) idx = i;
                ok = idx >= 0;
                if (ok) *reinterpret_cast<uint8_t*>(base) = (uint8_t)idx;
                break;
              }
            }
            if (!ok) { snprintf(r.bad_key, sizeof r.bad_key, "%.39s", k); return r; }
            const size_t fi = (size_t)(f - &fieldAt(0));
            if (fi < sizeof seen) seen[fi] = true;
            ++r.applied;
          }
        }
      }
    }
    pos = e + 1;
  }
  if (r.schema == 0 || r.schema > kConfigSchema) { snprintf(r.bad_key, sizeof r.bad_key, "schema"); return r; }
  for (size_t i = 0; i < fieldCount() && i < sizeof seen; ++i) if (!seen[i]) ++r.missing;
  io = c;
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------- sayaçlar / geçmiş
bool historyTick(CounterRec& r, bool clock_valid, int32_t day, uint32_t heating_s) {
  if (!clock_valid || day < 0) { r.today_s += heating_s; return false; }   // saat yokken bugüne birikir
  if (r.since_day < 0) r.since_day = day;
  if (r.today_day < 0) { r.today_day = day; r.today_s += heating_s; return false; }
  if (day < r.today_day) { r.today_s += heating_s; return false; }         // geri saat: kaydırma yok
  bool rolled = false;
  if (day > r.today_day) {
    // Kapanan gün(ler): dün = today; aradaki boş günler 0 (cihaz kapalıydı ≠ tüketim yok, bilinen sınır)
    const int32_t gap = day - r.today_day;
    uint16_t nh[kHistDays] = {};
    const uint32_t closed_min = r.today_s / 60u;
    for (int i = 0; i < kHistDays; ++i) {
      const int32_t d = day - 1 - i;             // nh[i]'nin günü
      if (d == r.today_day) nh[i] = (uint16_t)(closed_min > 1440 ? 1440 : closed_min);
      else if (r.hist_end >= 0) {
        const int32_t j = r.hist_end - d;        // eski pencerede indeks
        if (j >= 0 && j < kHistDays && d < r.today_day) nh[i] = r.hist_min[j];
      }
    }
    (void)gap;
    memcpy(r.hist_min, nh, sizeof nh);
    r.hist_end = day - 1;
    r.today_day = day;
    r.today_s = 0;
    rolled = true;
  }
  r.today_s += heating_s;
  return rolled;
}

void eventStorePush(EventStoreRec& r, const Event& e, int64_t ts) {
  if (r.n >= kPersistEvents) {
    memmove(&r.ev[0], &r.ev[1], sizeof(Event) * (kPersistEvents - 1));
    memmove(&r.ts[0], &r.ts[1], sizeof(int64_t) * (kPersistEvents - 1));
    r.n = kPersistEvents - 1;
  }
  r.ts[r.n] = ts;
  r.ev[r.n++] = e;
}

}  // namespace cc
