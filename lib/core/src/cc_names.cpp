// Sabit ad tabloları (MQTT/UI tel değerleri). Sözlük kapalıdır; değişiklik sürüm notuna yazılır.
#include "cc_types.h"
#include "cc_events.h"
#include "cc_sensor.h"

namespace cc {

#define CC_NAME_FN(T, ...)                                              \
  const char* name(T v) {                                               \
    static const char* const t[] = {__VA_ARGS__};                       \
    unsigned i = (unsigned)v;                                           \
    return i < sizeof(t) / sizeof(t[0]) ? t[i] : "?";                   \
  }

CC_NAME_FN(Quality, "GOOD", "UNCERTAIN", "STALE", "BAD", "MISSING", "DISABLED")
CC_NAME_FN(OpMode, "OFF", "AUTO", "MANUAL", "VENT_ONLY")
CC_NAME_FN(ProfileSel, "DAY", "NIGHT", "AWAY", "FROST")
CC_NAME_FN(ProfileActive, "DAY", "NIGHT", "AWAY", "FROST", "BOOST", "PROGRAM")
CC_NAME_FN(SetpointSource, "DAY", "NIGHT", "AWAY", "FROST", "BOOST", "ANTIFREEZE", "MANUAL", "PROGRAM")
CC_NAME_FN(PidMode, "P", "PI", "PID", "ONOFF")
CC_NAME_FN(DriverKind, "SSR_ZC", "SSR_RANDOM", "RELAY")
CC_NAME_FN(PostCoolMode, "TIME", "TEMPERATURE", "HYBRID")
CC_NAME_FN(Reason, "NONE", "HEATER_INTERLOCK", "POST_COOL", "PREPURGE", "FAN_PRESTART", "BOOT_POST_COOL",
           "SAFETY_LOCKOUT", "OVERTEMPERATURE", "OVERTEMPERATURE_LOCKOUT", "SENSOR_FAULT",
           "ANTIFREEZE_INHIBIT", "HEATING_PRIORITY", "VENT_PRIORITY", "MIN_ON_TIME", "MIN_OFF_TIME",
           "CHANGEOVER_DELAY", "LOCAL_LOCK", "SERVICE_ONLY", "SERVICE_TEST", "OTA", "CONTROLLER_DISABLED",
           "AUTO_DEMAND", "MODE_OFF")
CC_NAME_FN(SysState, "BOOT", "SELF_TEST", "RECOVERY", "RUN", "SERVICE", "FAILSAFE", "OTA_PREP", "OTA")
CC_NAME_FN(HeatPhase, "IDLE", "PREPURGE", "ACTIVE", "POST_COOL", "LOCKOUT")
CC_NAME_FN(VentState, "OFF", "AUTO", "MANUAL", "FORCED", "INHIBITED")
CC_NAME_FN(CtrlState, "BOOT", "SELF_TEST", "RECOVERY", "OTA", "FAILSAFE", "SERVICE", "HEATING",
           "POST_COOL", "VENTILATING", "MANUAL", "OFF", "IDLE")
CC_NAME_FN(HeatingReason, "NONE", "PID", "MANUAL", "ANTIFREEZE", "BOOST", "SERVICE_TEST")
CC_NAME_FN(FailsafeReason, "NONE", "SENSOR_FAULT", "OVERTEMP", "HEATING_TIMEOUT", "CONFIG_ERROR",
           "INTERNAL_FAULT", "HEATER_FAN_FAULT", "OUTPUT_FAULT", "TEMP_RISE")
CC_NAME_FN(Severity, "NORMAL", "INFO", "WARNING", "CRITICAL")
CC_NAME_FN(CmdSource, "SAFETY", "INTERLOCK", "LOCAL_SERVICE", "LOCAL_WEB", "MQTT", "CONTROLLER", "BUTTON",
           "SYSTEM")
CC_NAME_FN(CmdResult, "ACCEPTED", "OVERRIDDEN", "REJECTED_INVALID", "REJECTED_RELATION", "REJECTED_POLICY",
           "REJECTED_BUSY", "REJECTED_STATE")
CC_NAME_FN(Saturation, "NONE", "HIGH", "LOW")
CC_NAME_FN(DrvStatus, "OK", "MISSING", "TIMEOUT", "CRC_ERROR", "BUS_ERROR", "NOT_READY")

CC_NAME_FN(EvSrc, "STATE", "SAFETY", "CONTROLLER", "OUTPUT", "ALARM", "COMMAND", "CONFIG", "NET", "SYSTEM", "SERVICE")
CC_NAME_FN(EvCode, "NONE", "STATE_CHANGE", "HEATING_START", "POST_COOL_START", "POST_COOL_END", "STAGE2_ON",
           "STAGE2_OFF", "LEAD_ROTATED", "OUTPUT_ON", "OUTPUT_OFF", "SAFETY_TRIP", "SAFETY_RESET",
           "SAFETY_RESET_REFUSED", "ALARM_TRANSITION", "COMMAND", "COMMAND_REJECTED", "CONFIG_CHANGE",
           "SCHEDULE_REQUEST_EXPIRED", "BOOST_END", "MANUAL_TIMEOUT", "MANUAL_VENT_TIMEOUT", "ANTIFREEZE_ON",
           "ANTIFREEZE_OFF", "SERVICE_ENTER", "SERVICE_EXIT", "SERVICE_TIMEOUT", "SERVICE_TEST", "OTA_PREP",
           "OTA_TIMEOUT", "DEVICE_BOOT", "BOOT_POST_COOL", "LOCAL_LOCK", "PROGRAM_START", "PROGRAM_END",
           "PROGRAM_HOLD", "PROGRAMS_CHANGED")

#undef CC_NAME_FN
}  // namespace cc
