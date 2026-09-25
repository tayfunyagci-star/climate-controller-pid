#include "cc_config.h"
#include <cstring>

namespace cc {

namespace {
const char* const kOpMode[] = {"OFF", "AUTO", "MANUAL", "VENT_ONLY"};
const char* const kProfileSel[] = {"DAY", "NIGHT", "AWAY", "FROST"};
const char* const kPidMode[] = {"P", "PI", "PID", "ONOFF"};
const char* const kDriverKind[] = {"SSR_ZC", "SSR_RANDOM", "RELAY"};
const char* const kLeadRotation[] = {"OFF", "DAILY"};
const char* const kPostCoolMode[] = {"TIME", "TEMPERATURE", "HYBRID"};
const char* const kHumVent[] = {"INHIBIT", "ALLOW", "ALLOW_ABOVE_SP"};
const char* const kManVentPrio[] = {"VENT_WINS", "HEAT_WINS"};
const char* const kOtaVent[] = {"LAST", "OFF"};
const char* const kTimeSource[] = {"NONE", "NTP", "RTC"};

template <typename T> struct EnumNames;
#define CC_EN(T, arr)                                                   \
  template <> struct EnumNames<T> {                                     \
    static const char* const* names() { return arr; }                   \
    static uint8_t count() { return sizeof(arr) / sizeof(arr[0]); }     \
  };
CC_EN(OpMode, kOpMode)
CC_EN(ProfileSel, kProfileSel)
CC_EN(PidMode, kPidMode)
CC_EN(DriverKind, kDriverKind)
CC_EN(LeadRotation, kLeadRotation)
CC_EN(PostCoolMode, kPostCoolMode)
CC_EN(HumVentWhileHeating, kHumVent)
CC_EN(ManualVentPriority, kManVentPrio)
CC_EN(OtaVentState, kOtaVent)
CC_EN(TimeSource, kTimeSource)
#undef CC_EN

const FieldInfo kFields[] = {
#define X_F(k, d, lo, hi, st, f) {#k, FieldKind::FLOAT, lo, hi, st, (uint16_t)(f), offsetof(Config, k), nullptr, 0},
#define X_I(k, d, lo, hi, f) {#k, FieldKind::INT, (float)(lo), (float)(hi), 1.0f, (uint16_t)(f), offsetof(Config, k), nullptr, 0},
#define X_B(k, d, f) {#k, FieldKind::BOOL, 0, 1, 1, (uint16_t)(f), offsetof(Config, k), nullptr, 0},
#define X_E(k, T, d, f) {#k, FieldKind::ENUM, 0, (float)(EnumNames<T>::count() - 1), 1, (uint16_t)(f), offsetof(Config, k), EnumNames<T>::names(), EnumNames<T>::count()},
    CC_CFG_FLOATS(X_F) CC_CFG_INTS(X_I) CC_CFG_BOOLS(X_B) CC_CFG_ENUMS(X_E)
#undef X_F
#undef X_I
#undef X_B
#undef X_E
};
constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);
}  // namespace

// Enum alanlar Config içinde uint8_t tabanlı enum class olarak saklanır.
static_assert(sizeof(OpMode) == 1 && sizeof(PidMode) == 1 && sizeof(DriverKind) == 1, "enum boyutu");

size_t fieldCount() { return kFieldCount; }
const FieldInfo& fieldAt(size_t i) { return kFields[i]; }

const FieldInfo* findField(const char* key) {
  if (!key) return nullptr;
  for (size_t i = 0; i < kFieldCount; ++i)
    if (std::strcmp(kFields[i].key, key) == 0) return &kFields[i];
  return nullptr;
}

float fieldValue(const Config& c, const FieldInfo& f) {
  const char* base = reinterpret_cast<const char*>(&c) + f.offset;
  switch (f.kind) {
    case FieldKind::FLOAT: return *reinterpret_cast<const float*>(base);
    case FieldKind::INT: return (float)*reinterpret_cast<const int32_t*>(base);
    case FieldKind::BOOL: return *reinterpret_cast<const bool*>(base) ? 1.0f : 0.0f;
    case FieldKind::ENUM: return (float)*reinterpret_cast<const uint8_t*>(base);
  }
  return kNaN;
}

void applyDriverDefaults(Config& c, DriverKind k) {
  c.output_driver_r = k;
  if (k == DriverKind::RELAY) {
    c.tp_window_s = 600;
    c.heater_min_on_s = 120;
    c.heater_min_off_s = 180;
  } else {
    c.tp_window_s = 20;
    c.heater_min_on_s = 1;
    c.heater_min_off_s = 1;
  }
}

}  // namespace cc
