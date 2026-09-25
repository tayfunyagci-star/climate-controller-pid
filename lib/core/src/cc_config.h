// Konfigürasyon modeli — CONFIGURATION_MODEL.md §2 (çekirdeği ilgilendiren alanlar).
// Ağ/erişim/sır alanları servis katmanındadır (F3–F5); burada yalnız doğrulamaya giren
// zamanlama alanları tutulur.
#pragma once
#include <cstddef>
#include "cc_types.h"

namespace cc {

// Alan bayrakları
enum CfgFlag : uint16_t {
  CF_NONE = 0,
  CF_SC = 1u << 0,      // güvenlik-kritik (değişim olay günlüğüne, onay)
  CF_OPER = 1u << 1,    // operasyonel: MQTT'den yazılabilir (yerel kilit hariç)
  CF_RW = 1u << 2,      // remote_config_enabled açıkken MQTT'den yazılabilir
  CF_RW_PID = 1u << 3,  // + pid_remote_tuning
  CF_SAFETY = 1u << 4,  // güvenlik limiti grubu: yalnız yerel web (yönetici)
  CF_REBOOT = 1u << 5,  // yeniden başlatma gerektirir
};

//      anahtar                         varsayılan  min     max      adım  bayrak
#define CC_CFG_FLOATS(X)                                                                   \
  X(temperature_setpoint,            21.0f,  5.0f,   30.0f,  0.5f, CF_OPER)                \
  X(setpoint_night,                  18.0f,  5.0f,   30.0f,  0.5f, CF_RW)                  \
  X(setpoint_away,                   12.0f,  5.0f,   25.0f,  0.5f, CF_RW)                  \
  X(setpoint_frost,                   5.0f,  4.0f,   12.0f,  0.5f, CF_RW | CF_SC)          \
  X(setpoint_boost,                  23.0f, 10.0f,   30.0f,  0.5f, CF_RW)                  \
  X(setpoint_ramp_c_per_min,          0.2f,  0.0f,    2.0f,  0.0f, CF_RW)                  \
  X(manual_heat_demand,               0.0f,  0.0f,  100.0f,  5.0f, CF_OPER)                \
  X(frost_guard_temperature,          4.0f,  2.0f,   10.0f,  0.0f, CF_SC)                  \
  X(frost_exit_hysteresis,            1.0f,  0.5f,    3.0f,  0.0f, CF_SC)                  \
  X(pid_kp,                          20.0f,  0.5f,  200.0f,  0.5f, CF_RW_PID)              \
  X(pid_ki,                           1.0f,  0.0f,   20.0f, 0.05f, CF_RW_PID)              \
  X(pid_kd,                           0.0f,  0.0f,   60.0f,  0.5f, CF_RW_PID)              \
  X(pid_setpoint_weight,              1.0f,  0.0f,    1.0f,  0.1f, CF_NONE)                \
  X(pid_deadband,                     0.1f,  0.0f,    1.0f, 0.05f, CF_NONE)                \
  X(min_heat_demand,                  5.0f,  0.0f,   20.0f,  0.0f, CF_NONE)                \
  X(max_heat_demand,                100.0f, 20.0f,  100.0f,  0.0f, CF_RW)                  \
  X(demand_slew_pct_per_min,         10.0f,  1.0f,  100.0f,  0.0f, CF_NONE)                \
  X(onoff_hysteresis,                 0.5f,  0.2f,    2.0f,  0.0f, CF_NONE)                \
  X(stage2_on,                       55.0f, 40.0f,   90.0f,  0.0f, CF_NONE)                \
  X(stage2_off,                      45.0f, 10.0f,   85.0f,  0.0f, CF_NONE)                \
  X(post_cool_safe_temp,             40.0f, 25.0f,   70.0f,  0.0f, CF_SC)                  \
  X(ventilation_start_temperature,   26.0f, 15.0f,   40.0f,  0.5f, CF_RW)                  \
  X(ventilation_stop_temperature,    24.0f, 14.0f,   39.0f,  0.5f, CF_RW)                  \
  X(vent_sp_margin,                   2.0f,  1.0f,   10.0f,  0.0f, CF_NONE)                \
  X(humidity_high_limit,             75.0f, 40.0f,   95.0f,  1.0f, CF_RW)                  \
  X(humidity_hysteresis,              5.0f,  2.0f,   20.0f,  1.0f, CF_RW)                  \
  X(humidity_low_limit,              30.0f, 10.0f,   60.0f,  1.0f, CF_RW)                  \
  X(vent_heat_cap,                    0.0f,  0.0f,   50.0f,  0.0f, CF_NONE)                \
  X(cabin_overtemp_limit,            40.0f, 30.0f,   60.0f,  0.0f, CF_SAFETY | CF_SC)      \
  X(overtemp_reset_hysteresis,        3.0f,  1.0f,   10.0f,  0.0f, CF_SAFETY | CF_SC)      \
  X(heater_outlet_limit,             80.0f, 50.0f,  150.0f,  0.0f, CF_SAFETY | CF_SC)      \
  X(unexpected_rise_c_per_10min,      1.5f,  0.5f,   10.0f,  0.0f, CF_SAFETY | CF_SC)      \
  X(max_rise_c_per_10min,             5.0f,  1.0f,   20.0f,  0.0f, CF_SAFETY | CF_SC)      \
  X(t1_offset,                        0.0f, -5.0f,    5.0f,  0.0f, CF_SC)                  \
  X(rh1_offset,                       0.0f,-10.0f,   10.0f,  0.0f, CF_SC)                  \
  X(t2_offset,                        0.0f,-10.0f,   10.0f,  0.0f, CF_SC)

#define CC_CFG_INTS(X)                                                                     \
  X(tz_offset_min,                     180,  -720,    840, CF_NONE)                        \
  X(restart_storm_limit,                 5,     3,     10, CF_SC)                          \
  X(restart_storm_window_min,           30,    10,    120, CF_SC)                          \
  X(boost_minutes,                      30,    10,    240, CF_RW)                          \
  X(sched_timeout_h,                    16,     0,     48, CF_RW)                          \
  X(manual_timeout_h,                    8,     0,     48, CF_RW)                          \
  X(control_interval_s,                  2,     1,     30, CF_NONE)                        \
  X(tp_window_s,                        20,     5,   1800, CF_SC)                          \
  X(heater_min_on_s,                     1,     1,    900, CF_SC)                          \
  X(heater_min_off_s,                    1,     1,    900, CF_SC)                          \
  X(stage_min_dwell_s,                 120,     0,   1800, CF_NONE)                        \
  X(heater_power_w_r1,                   0,     0,   5000, CF_NONE)                        \
  X(heater_power_w_r2,                   0,     0,   5000, CF_NONE)                        \
  X(fan_prestart_s,                      3,     0,     30, CF_SC)                          \
  X(post_cool_seconds,                  60,    30,    600, CF_SC | CF_RW)                  \
  X(post_cool_min_s,                    20,    10,    120, CF_SC)                          \
  X(post_cool_max_s,                   600,    60,   1800, CF_SC)                          \
  X(relay_life_cycles,              100000, 10000, 10000000, CF_NONE)                      \
  X(manual_vent_timeout_min,           120,     0,    720, CF_RW)                          \
  X(ventilation_periodic_min,            0,     0,     60, CF_RW)                          \
  X(vent_min_on_s,                      60,    10,    600, CF_NONE)                        \
  X(vent_min_off_s,                     60,    10,    600, CF_NONE)                        \
  X(heat_vent_changeover_s,            120,     0,    600, CF_NONE)                        \
  X(vent_heat_changeover_s,             60,     0,    600, CF_NONE)                        \
  X(max_continuous_heating_min,        240,    30,   1440, CF_SAFETY | CF_SC)              \
  X(sensor_stale_s,                     10,     5,     60, CF_SAFETY | CF_SC)              \
  X(service_timeout_min,                30,     5,    120, CF_SAFETY | CF_SC)              \
  X(service_test_max_s,                120,    10,    300, CF_SAFETY | CF_SC)              \
  X(sensor_interval_s,                   1,     1,     10, CF_NONE)                        \
  X(sensor_filter_tau_s,                10,     0,    120, CF_NONE)                        \
  X(sensor_stuck_s,                   1800,   300,   7200, CF_NONE)                        \
  X(state_active_s,                      5,     1,     30, CF_NONE)                        \
  X(state_idle_s,                       30,    10,     60, CF_NONE)                        \
  X(diag_interval_s,                    60,    30,    300, CF_NONE)

#define CC_CFG_BOOLS(X)                                                                    \
  X(remote_manual_allowed,     true,  CF_SC)                                               \
  X(antifreeze_enabled,        true,  CF_SC)                                               \
  X(pid_remote_tuning,         false, CF_NONE)                                             \
  X(humidity_vent_enabled,     true,  CF_RW)                                               \
  X(t2_enabled,                false, CF_SC | CF_REBOOT)                                   \
  X(remote_config_enabled,     false, CF_SC)                                               \
  X(service_channel_enabled,   false, CF_SC)                                               \
  X(discovery_enabled,         true,  CF_NONE)                                             \
  X(history_discovery_enabled, false, CF_NONE)

//      anahtar                       tip                   varsayılan                        bayrak
#define CC_CFG_ENUMS(X)                                                                                   \
  X(operating_mode,              OpMode,               OpMode::AUTO,                   CF_OPER)              \
  X(profile,                     ProfileSel,           ProfileSel::DAY,                CF_OPER)              \
  X(pid_mode,                    PidMode,              PidMode::PI,                    CF_RW_PID)            \
  X(output_driver_r,             DriverKind,           DriverKind::SSR_ZC,             CF_SC | CF_REBOOT)    \
  X(output_driver_fan,           DriverKind,           DriverKind::RELAY,              CF_SC | CF_REBOOT)    \
  X(lead_rotation,               LeadRotation,         LeadRotation::DAILY,            CF_NONE)              \
  X(post_cool_mode,              PostCoolMode,         PostCoolMode::TIME,             CF_SC)                \
  X(humidity_vent_while_heating, HumVentWhileHeating,  HumVentWhileHeating::INHIBIT,   CF_RW)                \
  X(manual_vent_priority,        ManualVentPriority,   ManualVentPriority::VENT_WINS,  CF_RW)                \
  X(ota_vent_state,              OtaVentState,         OtaVentState::LAST,             CF_NONE)              \
  X(time_source,                 TimeSource,           TimeSource::NTP,                CF_NONE)

struct Config {
#define X_F(k, d, lo, hi, st, f) float k = d;
#define X_I(k, d, lo, hi, f) int32_t k = d;
#define X_B(k, d, f) bool k = d;
#define X_E(k, T, d, f) T k = d;
  CC_CFG_FLOATS(X_F)
  CC_CFG_INTS(X_I)
  CC_CFG_BOOLS(X_B)
  CC_CFG_ENUMS(X_E)
#undef X_F
#undef X_I
#undef X_B
#undef X_E
};

// Sürücü profiline bağlı varsayılanlar (OUTPUT_AND_INTERLOCKS §4.1)
void applyDriverDefaults(Config& c, DriverKind k);

// Heater Fan min ON/OFF (OUTPUT_AND_INTERLOCKS §4.3) — konfigürasyonda alan yok, sabit.
constexpr uint32_t kHeaterFanMinOnS = 60;
constexpr uint32_t kHeaterFanMinOffS = 30;

// ---------------------------------------------------------------------------
// Alan meta tablosu
// ---------------------------------------------------------------------------
enum class FieldKind : uint8_t { FLOAT, INT, BOOL, ENUM };

struct FieldInfo {
  const char* key;
  FieldKind kind;
  float min, max, step;       // FLOAT/INT
  uint16_t flags;
  size_t offset;
  const char* const* enumNames;  // ENUM
  uint8_t enumCount;
};

size_t fieldCount();
const FieldInfo& fieldAt(size_t i);
const FieldInfo* findField(const char* key);

// Alan değerini sayısal olarak oku (FLOAT/INT/BOOL/ENUM → float); karşılaştırma ve test için.
float fieldValue(const Config& c, const FieldInfo& f);

}  // namespace cc
