#include "cc_schedule.h"
#include <cstring>
#include <initializer_list>

namespace cc {

int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

void civilFromDays(int32_t z, int32_t& y, uint32_t& m, uint32_t& d) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = (uint32_t)(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t yy = (int32_t)yoe + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = yy + (m <= 2);
}

bool parseDate(const char* s, int32_t& day) {
  if (!s || std::strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
  for (int i : {0, 1, 2, 3, 5, 6, 8, 9})
    if (s[i] < '0' || s[i] > '9') return false;
  const int32_t y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  const uint32_t m = (uint32_t)((s[5] - '0') * 10 + (s[6] - '0'));
  const uint32_t d = (uint32_t)((s[8] - '0') * 10 + (s[9] - '0'));
  if (y < 2000 || y > 2199 || m < 1 || m > 12 || d < 1) return false;
  static const uint8_t mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  if (d > (uint32_t)(mdays[m - 1] + (m == 2 && leap ? 1 : 0))) return false;
  day = daysFromCivil(y, m, d);
  return true;
}

void formatDate(int32_t day, char out[11]) {
  int32_t y;
  uint32_t m, d;
  civilFromDays(day, y, m, d);
  out[0] = (char)('0' + y / 1000 % 10); out[1] = (char)('0' + y / 100 % 10);
  out[2] = (char)('0' + y / 10 % 10); out[3] = (char)('0' + y % 10); out[4] = '-';
  out[5] = (char)('0' + m / 10); out[6] = (char)('0' + m % 10); out[7] = '-';
  out[8] = (char)('0' + d / 10); out[9] = (char)('0' + d % 10); out[10] = 0;
}

#define CC_NAME_FN(T, ...)                                    \
  const char* name(T v) {                                     \
    static const char* const t[] = {__VA_ARGS__};             \
    unsigned i = (unsigned)v;                                 \
    return i < sizeof(t) / sizeof(t[0]) ? t[i] : "?";         \
  }
CC_NAME_FN(ProgKind, "WEEKLY", "DATE_RANGE", "ONCE")
CC_NAME_FN(ProgEnd, "DURATION", "END_TIME", "ALL_DAY")
CC_NAME_FN(ProgAction, "SETPOINT", "PROFILE", "HEATING_OFF", "VENTILATE")
CC_NAME_FN(ProgErr, "OK", "TOO_MANY", "NAME", "DAYS", "START", "END", "DURATION", "DATE_ORDER", "DATE_SPAN", "SETPOINT",
           "PROFILE", "SAFETY_MARGIN", "ALL_DAY_KIND")
#undef CC_NAME_FN

uint32_t occurrenceLength(const Program& p) {
  switch (p.end) {
    case ProgEnd::ALL_DAY: return 1440;
    case ProgEnd::END_TIME: return (uint32_t)((p.end_min + 1440 - p.start_min) % 1440);
    case ProgEnd::DURATION: return p.duration_min;
  }
  return 0;
}

ProgValidation validatePrograms(const Program* list, uint8_t n, float limit) {
  ProgValidation v;
  if (n > kMaxPrograms) { v.err = ProgErr::TOO_MANY; return v; }
  for (uint8_t i = 0; i < n; ++i) {
    const Program& p = list[i];
    auto fail = [&](ProgErr e) { v.err = e; v.index = (int8_t)i; return v; };
    const size_t nl = strnlen(p.name, sizeof p.name);
    if (nl == 0 || nl > kProgNameMax) return fail(ProgErr::NAME);
    if (p.kind == ProgKind::WEEKLY && (p.days & 0x7F) == 0) return fail(ProgErr::DAYS);
    if (p.days & 0x80) return fail(ProgErr::DAYS);
    if (p.end != ProgEnd::ALL_DAY && p.start_min >= 1440) return fail(ProgErr::START);
    if (p.end == ProgEnd::END_TIME && (p.end_min >= 1440 || p.end_min == p.start_min)) return fail(ProgErr::END);
    if (p.end == ProgEnd::DURATION) {
      const uint16_t mx = p.kind == ProgKind::ONCE ? 7 * 1440 : 1440;
      if (p.duration_min < 1 || p.duration_min > mx) return fail(ProgErr::DURATION);
    }
    if (p.kind == ProgKind::DATE_RANGE) {
      if (p.date_to < p.date_from) return fail(ProgErr::DATE_ORDER);
      if (p.date_to - p.date_from > 366) return fail(ProgErr::DATE_SPAN);
    }
    if (p.kind != ProgKind::WEEKLY && p.date_from <= 0) return fail(ProgErr::DATE_ORDER);
    if (p.action == ProgAction::SETPOINT) {
      const float q = p.setpoint * 2.0f;
      if (!(p.setpoint >= 5.0f && p.setpoint <= 30.0f) || q - (float)(int)(q + 0.5f) > 1e-3f ||
          (float)(int)(q + 0.5f) - q > 1e-3f)
        return fail(ProgErr::SETPOINT);
      if (p.setpoint > limit - 10.0f + 1e-4f) return fail(ProgErr::SAFETY_MARGIN);
    }
    if (p.action == ProgAction::PROFILE && p.profile == ProfileSel::DAY) return fail(ProgErr::PROFILE);
  }
  return v;
}

static bool occursOn(const Program& p, int32_t day) {
  switch (p.kind) {
    case ProgKind::WEEKLY: return (p.days >> weekdayMon0(day)) & 1u;
    case ProgKind::DATE_RANGE:
      return day >= p.date_from && day <= p.date_to && (p.days == 0 || ((p.days >> weekdayMon0(day)) & 1u));
    case ProgKind::ONCE: return day == p.date_from;
  }
  return false;
}

static uint8_t classOf(ProgKind k) { return k == ProgKind::ONCE ? 3 : (k == ProgKind::DATE_RANGE ? 2 : 1); }

static int32_t floorDay(int64_t local_min) { return (int32_t)(local_min >= 0 ? local_min / 1440 : (local_min - 1439) / 1440); }

bool activeOccurrence(const Program& p, int64_t t, Occurrence& out) {
  if (!p.enabled) return false;
  const uint32_t len = occurrenceLength(p);
  if (!len) return false;
  const int32_t day = floorDay(t);
  const int32_t back = (int32_t)((len + 1439) / 1440);
  for (int32_t D = day; D >= day - back; --D) {  // en geç başlayan oluşum
    if (!occursOn(p, D)) continue;
    const int64_t s = (int64_t)D * 1440 + (p.end == ProgEnd::ALL_DAY ? 0 : p.start_min);
    if (s <= t && t < s + (int64_t)len) {
      out.start = s;
      out.end = s + len;
      out.cls = classOf(p.kind);
      return true;
    }
  }
  return false;
}

static bool better(const Occurrence& a, const Occurrence& b) {  // a, b'den öncelikli mi
  if (b.index < 0) return true;
  if (a.cls != b.cls) return a.cls > b.cls;
  if (a.start != b.start) return a.start > b.start;
  return a.index < b.index;
}

static ScheduleResult evalAt(const Program* list, uint8_t n, int64_t t, const Hold& hold) {
  ScheduleResult r;
  r.valid = true;
  for (uint8_t i = 0; i < n; ++i) {
    Occurrence o;
    if (!activeOccurrence(list[i], t, o)) continue;
    o.index = (int8_t)i;
    if (hold.index == (int8_t)i && hold.start == o.start) { r.held = true; continue; }
    Occurrence& slot = list[i].action == ProgAction::VENTILATE ? r.vent : r.climate;
    if (better(o, slot)) slot = o;
  }
  return r;
}

static bool sameResult(const ScheduleResult& a, const ScheduleResult& b) {
  return a.climate.index == b.climate.index && a.climate.start == b.climate.start && a.vent.index == b.vent.index &&
         a.vent.start == b.vent.start;
}

ScheduleResult evaluate(const Program* list, uint8_t n, int64_t t, const Hold& hold, bool with_next) {
  ScheduleResult r = evalAt(list, n, t, hold);
  if (!with_next) return r;
  // Aday sınırlar: 8 gün içindeki bütün oluşum başlangıç ve bitişleri
  int64_t cand[kMaxPrograms * 20 * 2];
  int nc = 0;
  const int32_t day = floorDay(t);
  const int64_t horizon = t + 8 * 1440;
  for (uint8_t i = 0; i < n; ++i) {
    const Program& p = list[i];
    if (!p.enabled) continue;
    const uint32_t len = occurrenceLength(p);
    if (!len) continue;
    for (int32_t D = day - 7; D <= day + 8; ++D) {
      if (!occursOn(p, D)) continue;
      const int64_t s = (int64_t)D * 1440 + (p.end == ProgEnd::ALL_DAY ? 0 : p.start_min), e = s + len;
      if (s > t && s <= horizon && nc < (int)(sizeof cand / sizeof cand[0])) cand[nc++] = s;
      if (e > t && e <= horizon && nc < (int)(sizeof cand / sizeof cand[0])) cand[nc++] = e;
    }
  }
  // küçükten büyüğe sırala (ekleme sıralaması; en çok 640 aday)
  for (int i = 1; i < nc; ++i)
    for (int j = i; j > 0 && cand[j - 1] > cand[j]; --j) { const int64_t x = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = x; }
  for (int i = 0; i < nc; ++i) {
    if (i > 0 && cand[i] == cand[i - 1]) continue;
    if (!sameResult(evalAt(list, n, cand[i], hold), r)) { r.next_change = cand[i]; break; }
  }
  return r;
}

}  // namespace cc
