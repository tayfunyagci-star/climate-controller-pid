// ClimateCore — çekirdek modüllerin tek iş parçacıklı birleşimi.
// Üç aşama ayrıdır (F2'de görevlere dağıtılır): controlStep (ControlTask), outputStep (OutputTask),
// safetyStep (SafetyTask). tick() bunları periyotlarına göre çağırır (native simülasyon / test).
// Hiçbir kaynak çıkışı doğrudan sürmez: komutlar yalnız istek/hedef değerini değiştirir.
#pragma once
#include "cc_alarm.h"
#include "cc_config_validate.h"
#include "cc_demand.h"
#include "cc_events.h"
#include "cc_hpm.h"
#include "cc_interlock.h"
#include "cc_pid.h"
#include "cc_power.h"
#include "cc_profile.h"
#include "cc_safety.h"
#include "cc_sensor.h"
#include "cc_state.h"
#include "cc_vent.h"

namespace cc {

struct BootInfo {
  bool fault_reset = false;           // WDT / PANIC / BROWNOUT
  bool heater_was_on = false;         // RTC belleği bayrağı
  uint32_t fault_boots_in_window = 0; // restart fırtınası
  bool config_error = false;          // config yüklenemedi → güvenli varsayılan
  bool restore_alarms = false;
  AlarmPersist alarms{};
  uint8_t safety_latched = 0;
};

// B/state alanlarının çekirdek karşılığı (MQTT_INTEGRATION §4.1)
struct CoreSnapshot {
  uint32_t seq = 0, uptime_s = 0;
  float temperature = kNaN, humidity = kNaN, t2 = kNaN;
  Quality temperature_quality = Quality::MISSING, humidity_quality = Quality::MISSING, t2_quality = Quality::DISABLED;
  bool sensor_ok = false;
  uint32_t sensor_age_s = 0;
  float temperature_setpoint = 21, setpoint_effective = 21;
  SetpointSource setpoint_source = SetpointSource::DAY;
  ProfileSel profile = ProfileSel::DAY;
  ProfileActive profile_active = ProfileActive::DAY;
  bool sched_night = false, sched_away = false, boost = false;
  uint32_t boost_remaining_min = 0;
  OpMode operating_mode = OpMode::AUTO;
  bool controller_enable = true;
  CtrlState controller_state = CtrlState::BOOT;
  HeatPhase heating_phase = HeatPhase::IDLE;
  VentState ventilation_state = VentState::OFF;
  HeatingReason heating_reason = HeatingReason::NONE;
  FailsafeReason failsafe_reason = FailsafeReason::NONE;
  float pid_output = 0, heat_demand = 0, manual_heat_demand = 0;
  float pid_error = kNaN, pid_p = 0, pid_i = 0, pid_d = 0;
  Saturation pid_saturation = Saturation::NONE;
  bool anti_windup_active = false, pid_tracking = false;
  float r1_duty = 0, r2_duty = 0;
  uint8_t power_stage = 0;
  float temperature_rate = kNaN;
  bool active[OUT_COUNT] = {false, false, false, false};   // r1_active … ventilation_fan_active
  Reason reason[OUT_COUNT] = {Reason::NONE, Reason::NONE, Reason::NONE, Reason::NONE};
  bool heating_active = false, ventilation_active = false;
  bool heater_fan_manual = false, ventilation_fan_manual = false;
  bool overtemperature = false;
  bool alarm = false;
  Severity alarm_state = Severity::NONE;
  uint8_t active_alarm_count = 0, unacked_alarm_count = 0;
  bool local_lock = false;
  CmdSource last_command_source = CmdSource::SYSTEM;
  uint32_t ack_count = 0;
  SysState sys_state = SysState::BOOT;
  bool heater_arm = false;
  float ventilation_start_effective = 26;
  uint32_t post_cool_remaining_s = 0;
};

struct CmdReply {
  CmdResult result = CmdResult::ACCEPTED;
  Reason reason = Reason::NONE;
  ValCode code = ValCode::OK;
};

class ClimateCore {
 public:
  static constexpr uint32_t kBaseTickMs = 50;
  static constexpr uint32_t kOutputPeriodMs = 100;
  static constexpr uint32_t kSafetyPeriodMs = 250;
  static constexpr uint32_t kSelfTestTimeoutMs = 10000;

  ClimateCore();
  // Boot: config yüklendi, çıkışlar güvenli. Doğrulanmamış config reddedilir → güvenli varsayılan.
  void begin(const Config& cfg, const BootInfo& boot = BootInfo());

  // ---- Sensör girişi (SensorTask) ----
  void feedT1(DrvStatus st, float raw);
  void feedRh(DrvStatus st, float raw);
  void feedT2(DrvStatus st, float raw);
  void setT1Missing(bool m) { t1f_.setMissing(m); }

  // ---- Zaman ----
  void tick(uint32_t dt_ms = kBaseTickMs);
  void controlStep(uint32_t dt_ms);
  void outputStep(uint32_t dt_ms);
  void safetyStep(uint32_t dt_ms);
  void setTimeInfo(bool valid, bool day_rollover) { time_valid_ = valid; day_rollover_ |= day_rollover; }
  // Ağ durumu yalnız alarm/olay üretir; kontrol durumunu değiştirmez (MQTT LOST ≠ LOCAL CONTROL LOST)
  void setNetStatus(bool wifi_configured, bool wifi_ok, bool mqtt_configured, bool mqtt_ok) {
    wifi_alarm_ = wifi_configured && !wifi_ok;
    mqtt_alarm_ = mqtt_configured && !mqtt_ok;
  }

  // ---- Komutlar (ACCEPTED/OVERRIDDEN/REJECTED_*) ----
  // Genel dağıtıcı: MQTT /set ve web tek alan yazımı (id = entity/config anahtarı)
  CmdReply command(const char* id, const char* payload, CmdSource src);
  CmdReply setMode(OpMode m, CmdSource src);
  CmdReply setControllerEnable(bool on, CmdSource src);
  CmdReply setHeaterFanManual(bool on, CmdSource src);
  CmdReply setVentFanManual(bool on, CmdSource src);
  CmdReply setBoost(bool on, CmdSource src);
  CmdReply setSchedNight(bool on, CmdSource src);
  CmdReply setSchedAway(bool on, CmdSource src);
  CmdReply alarmAck(CmdSource src);
  CmdReply alarmReset(CmdSource src);
  CmdReply setLocalLock(uint32_t minutes, CmdSource src);  // 0 = kaldır; yalnız yerel web
  CmdReply applyConfig(const Config& cand, CmdSource src); // web formu: atomik çoklu alan
  CmdReply serviceEnter(CmdSource src);
  CmdReply serviceExit(CmdSource src);
  CmdReply serviceTest(uint8_t out, bool on, CmdSource src);
  CmdReply otaBegin(CmdSource src);
  void otaAbort() { sys_.otaAbort(); }
  bool otaReady() const { return sys_.state() == SysState::OTA; }
  CmdReply recoveryAck(CmdSource src);

  // ---- Test kancaları (yalnız test build; üretimde derlenmez — SECURITY §5) ----
  void testFreezeControl(bool f) { freeze_control_ = f; }
  void testFreezeOutput(bool f) { freeze_output_ = f; }

  // ---- Durum ----
  const CoreSnapshot& snapshot() const { return snap_; }
  const Config& config() const { return cfg_; }
  const bool* outputs() const { return guard_.applied(); }  // OutputGuard sonrası fiziksel istek
  bool heaterArm() const { return arm_; }
  SysState sysState() const { return sys_.state(); }
  HeatPhase heatPhase() const { return chain_.phase(); }
  const InterlockOutput& interlock() const { return il_.last(); }
  const SafetyOutput& safety() const { return saf_.last(); }
  const PowerOutput& power() const { return pm_.last(); }
  const PidOutput& pid() const { return pid_.last(); }
  const VentOutput& vent() const { return vent_.last(); }
  const HpmOutput& hpm() const { return hpm_.last(); }
  const RoleReading& t1() const { return t1f_.reading(); }
  const AlarmManager& alarms() const { return alm_; }
  const EventRing<200>& events() const { return ev_; }
  uint32_t uptimeMs() const { return up_ms_; }
  bool guardViolationSeen() const { return guard_violation_; }

 private:
  void commitConfig(const Config& c, bool bumpless);
  void log(Severity s, EvSrc src, EvCode code, float val = kNaN, CmdSource actor = CmdSource::SYSTEM, uint16_t aux = 0);
  CmdReply reply(CmdResult r, Reason rs = Reason::NONE, ValCode v = ValCode::OK);
  CmdReply cmdLog(const CmdReply& r, CmdSource src, float val);
  bool mqttLocked(CmdSource src) const { return src == CmdSource::MQTT && local_lock_; }
  bool heatingPermittedBase() const;
  void updateSnapshot();
  void runSelfTest();

  Config cfg_;
  bool config_error_ = false;
  RoleFilter t1f_, rhf_, t2f_;
  Timer t1_since_, rh_since_, t2_since_;
  ProfileResolver prof_;
  Pid pid_;
  DemandConditioner dem_;
  PowerManager pm_;
  InterlockEngine il_;
  OutputGuard guard_;
  Ventilation vent_;
  SafetyEvaluator saf_;
  AlarmManager alm_;
  HeatingPerformanceMonitor hpm_;
  SystemSM sys_;
  HeatingChain chain_;
  EventRing<200> ev_;

  bool controller_enable_ = true;
  bool heater_fan_manual_ = false;
  bool local_lock_ = false;
  Timer local_lock_t_;
  uint32_t local_lock_ms_ = 0;
  Timer manual_t_;
  CmdSource last_src_ = CmdSource::SYSTEM;
  uint32_t ack_count_ = 0;

  // Servis testleri
  bool svc_test_[OUT_COUNT] = {false, false, false, false};
  Timer svc_test_t_[OUT_COUNT];

  // Kontrol çıktıları (aşamalar arası)
  bool chain_request_ = false;
  bool antifreeze_ = false;
  HeatingReason heating_reason_ = HeatingReason::NONE;
  float heat_demand_ = 0;
  bool was_locked_ = false;
  bool arm_ = false;
  bool guard_violation_ = false;
  bool reset_request_ = false;
  bool rotate_request_ = false;
  bool time_valid_ = false, day_rollover_ = false;
  bool freeze_control_ = false, freeze_output_ = false;
  bool wifi_alarm_ = false, mqtt_alarm_ = false;

  // Zamanlama
  uint32_t up_ms_ = 0;
  uint32_t acc_ctrl_ = 0, acc_out_ = 0, acc_saf_ = 0;
  Timer ctrl_hb_, out_hb_, sens_hb_;
  Timer selftest_t_, rot_t_;
  SysState last_sys_ = SysState::BOOT;
  HeatPhase last_phase_ = HeatPhase::IDLE;
  FailsafeReason last_fs_ = FailsafeReason::NONE;
  uint32_t seq_ = 0;
  CoreSnapshot snap_;
};

}  // namespace cc
