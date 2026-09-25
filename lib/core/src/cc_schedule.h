// Yerel program modülü — ADR-009, docs/PROGRAMS.md.
// Saf ve durumsuz değerlendirme: etkin program yalnız (yerel zaman, program listesi, atlama) fonksiyonudur;
// kaçırılan kenar yoktur, boot'ta ve saat düzeltmesinde aynı sonuç yeniden hesaplanır.
// Zaman birimi: yerel dakika = (UTC epoch + tz ofseti) / 60. Gün numarası: 1970-01-01'den bu yana yerel gün.
#pragma once
#include "cc_types.h"

namespace cc {

// ---------------- Takvim (Howard Hinnant gün serisi; artık yıl, 2100, yıl sonu doğru) ----------------
int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d);
void civilFromDays(int32_t z, int32_t& y, uint32_t& m, uint32_t& d);
// 0 = Pazartesi … 6 = Pazar
inline uint8_t weekdayMon0(int32_t day) { return (uint8_t)(((day % 7) + 7 + 3) % 7); }  // 1970-01-01 Perşembe
bool parseDate(const char* s, int32_t& day);   // "YYYY-MM-DD"
void formatDate(int32_t day, char out[11]);

constexpr uint8_t kMaxPrograms = 16;
constexpr uint8_t kProgNameMax = 23;   // UTF-8 bayt

enum class ProgKind : uint8_t { WEEKLY, DATE_RANGE, ONCE };
enum class ProgEnd : uint8_t { DURATION, END_TIME, ALL_DAY };
enum class ProgAction : uint8_t { SETPOINT, PROFILE, HEATING_OFF, VENTILATE };
const char* name(ProgKind);
const char* name(ProgEnd);
const char* name(ProgAction);

struct Program {
  bool enabled = true;
  char name[kProgNameMax + 1] = "";
  ProgKind kind = ProgKind::WEEKLY;
  uint8_t days = 0x1F;            // bit0 = Pzt … bit6 = Paz (WEEKLY zorunlu; DATE_RANGE'de 0 = her gün)
  int32_t date_from = 0;          // DATE_RANGE başlangıç / ONCE günü
  int32_t date_to = 0;            // DATE_RANGE bitiş (dahil)
  uint16_t start_min = 0;         // 0…1439
  ProgEnd end = ProgEnd::END_TIME;
  uint16_t end_min = 0;           // END_TIME: ertesi güne taşabilir (end ≤ start → +1 gün)
  uint16_t duration_min = 60;     // DURATION
  ProgAction action = ProgAction::SETPOINT;
  float setpoint = 21.0f;         // SETPOINT
  ProfileSel profile = ProfileSel::NIGHT;  // PROFILE (NIGHT/AWAY/FROST)
};

enum class ProgErr : uint8_t {
  OK, TOO_MANY, NAME, DAYS, START, END, DURATION, DATE_ORDER, DATE_SPAN, SETPOINT, PROFILE, SAFETY_MARGIN, ALL_DAY_KIND
};
const char* name(ProgErr);
struct ProgValidation {
  ProgErr err = ProgErr::OK;
  int8_t index = -1;              // hatalı program
  bool ok() const { return err == ProgErr::OK; }
};
// Liste tamamı doğrulanır; bir hata = hiçbir program uygulanmaz. overtemp_limit: SETPOINT ≤ limit − 10 (V5 eşi).
ProgValidation validatePrograms(const Program* list, uint8_t n, float cabin_overtemp_limit);

// Bir programın yerel dakika cinsinden süresi (ONCE en çok 7 gün, diğerleri en çok 1 gün)
uint32_t occurrenceLength(const Program& p);

struct Occurrence {
  int8_t index = -1;
  int64_t start = 0, end = 0;     // yerel dakika [start, end)
  uint8_t cls = 0;                // öncelik sınıfı: ONCE 3 › DATE_RANGE 2 › WEEKLY 1
};

struct ScheduleResult {
  bool valid = false;             // saat geçerli ve modül etkin
  Occurrence climate;             // SETPOINT / PROFILE / HEATING_OFF kanalı (index −1: yok)
  Occurrence vent;                // VENTILATE kanalı
  bool held = false;              // etkin olacak bir oluşum atlandı
  int64_t next_change = -1;       // sonraki değişim (yerel dakika), 8 gün içinde yoksa −1
};

struct Hold {
  int8_t index = -1;              // atlanan oluşum (program + başlangıç)
  int64_t start = 0;
};

// Verilen yerel dakikada `p` programının etkin oluşumu var mı
bool activeOccurrence(const Program& p, int64_t local_min, Occurrence& out);

// Değerlendirme: öncelik = sınıf › daha geç başlayan › küçük indeks. Atlanan oluşum yok sayılır.
ScheduleResult evaluate(const Program* list, uint8_t n, int64_t local_min, const Hold& hold, bool with_next = true);

}  // namespace cc
