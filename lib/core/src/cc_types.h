// Kulübe İklim Kontrolörü — çekirdek ortak tipleri.
// Saf C++17: Arduino / ESP-IDF başlığı içermez (native test edilebilir).
#pragma once
#include <cstdint>
#include <cmath>
#include <limits>

namespace cc {

// ---------------------------------------------------------------------------
// Zaman: modüller mutlak saat görmez; her adımda geçen süre (dt_ms) verilir.
// Zamanlayıcılar doygun (saturating) biriktiricidir → uint32 taşmasına dayanıklı.
// ---------------------------------------------------------------------------
struct Timer {
  uint32_t ms = 0;
  void reset() { ms = 0; }
  void add(uint32_t dt) { ms = (UINT32_MAX - ms < dt) ? UINT32_MAX : ms + dt; }
  void saturate() { ms = UINT32_MAX; }
  bool atLeast(uint32_t limit_ms) const { return ms >= limit_ms; }
  float seconds() const { return ms / 1000.0f; }
};

inline uint32_t sToMs(float s) { return s <= 0 ? 0u : (uint32_t)(s * 1000.0f + 0.5f); }
inline uint32_t minToMs(float m) { return sToMs(m * 60.0f); }

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
inline bool isValid(float v) { return !std::isnan(v); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Enum'lar (sabit ad tabloları cc_reason.cpp'de)
// ---------------------------------------------------------------------------
enum class Quality : uint8_t { GOOD, UNCERTAIN, STALE, BAD, MISSING, DISABLED };
enum class OpMode : uint8_t { OFF, AUTO, MANUAL, VENT_ONLY };
enum class ProfileSel : uint8_t { DAY, NIGHT, AWAY, FROST };                    // kullanıcı seçimi
enum class ProfileActive : uint8_t { DAY, NIGHT, AWAY, FROST, BOOST, PROGRAM };  // çözülen profil (PROGRAM: ADR-009)
enum class SetpointSource : uint8_t { DAY, NIGHT, AWAY, FROST, BOOST, ANTIFREEZE, MANUAL, PROGRAM };
enum class PidMode : uint8_t { P, PI, PID, ONOFF };
enum class DriverKind : uint8_t { SSR_ZC, SSR_RANDOM, RELAY };
enum class PostCoolMode : uint8_t { TIME, TEMPERATURE, HYBRID };
enum class LeadRotation : uint8_t { OFF, DAILY };
enum class HumVentWhileHeating : uint8_t { INHIBIT, ALLOW, ALLOW_ABOVE_SP };
enum class ManualVentPriority : uint8_t { VENT_WINS, HEAT_WINS };
enum class OtaVentState : uint8_t { LAST, OFF };
enum class TimeSource : uint8_t { NONE, NTP, RTC };
enum class Saturation : uint8_t { NONE, HIGH, LOW };

// Çıkış indeksleri
enum Out : uint8_t { R1 = 0, R2 = 1, HF = 2, VF = 3, OUT_COUNT = 4 };

// Kapalı reason sözlüğü (ENTITY_MODEL §5). MODE_OFF F1'de eklendi (CHANGELOG).
enum class Reason : uint8_t {
  NONE, HEATER_INTERLOCK, POST_COOL, PREPURGE, FAN_PRESTART, BOOT_POST_COOL,
  SAFETY_LOCKOUT, OVERTEMPERATURE, OVERTEMPERATURE_LOCKOUT, SENSOR_FAULT,
  ANTIFREEZE_INHIBIT, HEATING_PRIORITY, VENT_PRIORITY, MIN_ON_TIME, MIN_OFF_TIME,
  CHANGEOVER_DELAY, LOCAL_LOCK, SERVICE_ONLY, SERVICE_TEST, OTA, CONTROLLER_DISABLED,
  AUTO_DEMAND, MODE_OFF,
  COUNT_
};

// Sistem (ana) durum makinesi — STATE_MACHINE §1
enum class SysState : uint8_t { BOOT, SELF_TEST, RECOVERY, RUN, SERVICE, FAILSAFE, OTA_PREP, OTA };
// Isıtma zinciri — STATE_MACHINE §2 (rapor adları IDLE/PREPURGE/ACTIVE/POST_COOL/LOCKOUT)
enum class HeatPhase : uint8_t { IDLE, PREPURGE, ACTIVE, POST_COOL, LOCKOUT };
// Havalandırma — STATE_MACHINE §3 (rapor adları)
enum class VentState : uint8_t { OFF, AUTO, MANUAL, FORCED, INHIBITED };
// controller_state — STATE_MACHINE §5
enum class CtrlState : uint8_t { BOOT, SELF_TEST, RECOVERY, OTA, FAILSAFE, SERVICE, HEATING,
                                 POST_COOL, VENTILATING, MANUAL, OFF, IDLE };
enum class HeatingReason : uint8_t { NONE, PID, MANUAL, ANTIFREEZE, BOOST, SERVICE_TEST };
// STATE_MACHINE §4 (+ TEMP_RISE: S9 kritik; CHANGELOG)
enum class FailsafeReason : uint8_t { NONE, SENSOR_FAULT, OVERTEMP, HEATING_TIMEOUT, CONFIG_ERROR,
                                      INTERNAL_FAULT, HEATER_FAN_FAULT, OUTPUT_FAULT, TEMP_RISE };

enum class Severity : uint8_t { NONE, INFO, WARNING, CRITICAL };

// Komut kaynakları — CONTROL_ARCHITECTURE §6.1
enum class CmdSource : uint8_t { SAFETY, INTERLOCK, LOCAL_SERVICE, LOCAL_WEB, MQTT, CONTROLLER, BUTTON, SYSTEM };
// Komut sonucu — MQTT_INTEGRATION §6.3
enum class CmdResult : uint8_t { ACCEPTED, OVERRIDDEN, REJECTED_INVALID, REJECTED_RELATION,
                                 REJECTED_POLICY, REJECTED_BUSY, REJECTED_STATE };

// Ad tabloları
const char* name(Quality);
const char* name(OpMode);
const char* name(ProfileSel);
const char* name(ProfileActive);
const char* name(SetpointSource);
const char* name(PidMode);
const char* name(DriverKind);
const char* name(PostCoolMode);
const char* name(Reason);
const char* name(SysState);
const char* name(HeatPhase);
const char* name(VentState);
const char* name(CtrlState);
const char* name(HeatingReason);
const char* name(FailsafeReason);
const char* name(Severity);
const char* name(CmdSource);
const char* name(CmdResult);
const char* name(Saturation);

}  // namespace cc
